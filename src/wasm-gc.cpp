#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wasm.h>
#include <wasmtime.h>
// cpp
#include <iostream>
#include <list>
#include <unordered_map>
#include <set>

static void exit_with_error(const char *message,
                            wasmtime_error_t *error, wasm_trap_t *trap);

wasmtime_memory_t memory;
wasmtime_context_t *context;
int memory_bumper_offset = 0; // integer offset for end of allocated memory
long __heap_base;
long __data_end;

struct Chunk
{
  uint8_t *address;
  int offset;
  bool mark = false;
  int size = 4;
  int memclass_index;
};
std::list<Chunk *> used_list; // store used chunks

enum memory_class
{
  B4,
  B8,
  B16,
  B32,
  B64,
  B128,
  B256,
  B512,
  B1024,
  B2048,
  B4096,
  BIG
};
struct FreeList
{
  std::list<Chunk *> free_chunks[12];
};
FreeList *free_list = new FreeList();

void print_stack(void *stack_ptr, void *base_ptr, int interval, bool as_address)
{
  int cnt = 1;
  for (int i = 0; stack_ptr + i <= base_ptr; i += interval)
  {
    if (!as_address)
    {
      printf("%d. %p --> %d\n", cnt, (void *)(stack_ptr + i), *(uint8_t *)(stack_ptr + i));
    }
    else
      printf("%d. %p --> %p\n", cnt, (void *)(stack_ptr + i), *(uint8_t *)(stack_ptr + i));
    ++cnt;
  }
}

void print_memory(int start, int end, int step)
{
  printf("wasm memory (GC):\n");
  uint8_t *mem = wasmtime_memory_data(context, &memory);
  for (int i = start; i < end; i += step)
  {
    uint32_t data = (mem[i] | (mem[i + 1]) << 8 | (mem[i + 2]) << 16 | (mem[i + 3]) << 24);
    printf("%p -> %d\n", mem + i, data);
  }
}

void print_registers()
{
  void *rsp_ptr;
  void *rbp_ptr;
  void *rax_ptr;
  void *rcx_ptr;
  void *rdx_ptr;
  void *rbx_ptr;
  // void *r6_ptr; //Doesn't exist
  void *r8_ptr;
  void *r9_ptr;
  void *r14_ptr;
  void *r15_ptr;

  // For some reason asm doesn't let you input string so copied register pointers manually
  asm("movq %%rsp, %0"
      : "=r"(rsp_ptr));
  asm("movq %%rbp, %0"
      : "=r"(rbp_ptr));
  asm("movq %%rax, %0"
      : "=r"(rax_ptr));
  asm("movq %%rcx, %0"
      : "=r"(rcx_ptr));
  asm("movq %%rdx, %0"
      : "=r"(rdx_ptr));
  asm("movq %%rbx, %0"
      : "=r"(rbx_ptr));
  // asm("movq %%r6, %0"
  //     : "=r"(r6_ptr));
  asm("movq %%r8, %0"
      : "=r"(r8_ptr));
  asm("movq %%r9, %0"
      : "=r"(r9_ptr));
  asm("movq %%r14, %0"
      : "=r"(r14_ptr));
  asm("movq %%r15, %0"
      : "=r"(r15_ptr));

  printf("rsp: %p -> %d\n", rsp_ptr, *(uint8_t *)rsp_ptr);
  printf("rbp: %p -> %d\n", rbp_ptr, *(uint8_t *)rsp_ptr);
  printf("rax: %p -> %d\n", rbp_ptr, *(uint8_t *)rax_ptr);
  // printf("rcx: %p -> %d\n", rcx_ptr, *(uint8_t *)rcx_ptr); // Seg fault
  // printf("rdx: %p -> %d\n", rdx_ptr, *(uint8_t *)rdx_ptr); // Seg fault
  printf("rbx: %p -> %d\n", rbx_ptr, *(uint8_t *)rbx_ptr);
  // printf("r6: %p -> %d\n", r6_ptr, *(uint8_t *)r6_ptr);
  // printf("r8: %p -> %d\n", r8_ptr, *(uint8_t *)r8_ptr); //Seg fault
  // printf("r9: %p -> %d\n", r9_ptr, *(uint8_t *)r9_ptr);
  // printf("r14: %p -> %d\n", r14_ptr, *(uint8_t *)r14_ptr);
  // printf("r15: %p -> %d\n", r15_ptr, *(uint8_t *)r15_ptr); //Seg fault
}

// functions to impliment stubs
// reutnr number of bytes allocaed in the wasm memory
size_t getBytesAllocated()
{
  printf("stub\n");
}
// return size of wasm memory
size_t wasmMemorySize()
{
  printf("stub\n");
}
// return number of bytes allocated in size class
size_t bytesAllocatedInSizeClass()
{
  printf("stub\n");
}

/*--GC functions--*/

// scan memory and mark accessable chunks

// Initialize C and Wasm stack pointers.  Mark functions will use these to scan though data
void *stack_ptr;
void *base_ptr;
int *wasm_stack_ptr;
int *wasm_base_ptr;

// std::unordered_map<int *, int> used_map; // TF: for debugging
void __mark_stack(void *stack_ptr, void *base_ptr, std::set<uint8_t *> used_set)
{
  for (int i = 0; stack_ptr + i <= base_ptr; i += sizeof(uint8_t *)) // Scan through C stack
  {
    try
    {
      // If what appears to be pointer to used_list found on C stack, mark that chunk
      if (used_set.count(*(uint8_t **)(stack_ptr + i)) > 0)
      {
        for (Chunk *c : used_list)
        {
          if (c->address == *(uint8_t **)(stack_ptr + i))
          {
            c->mark = 1;
            break;
          }
        }
      }
    }
    catch (const std::exception &e)
    {
      std::cout << e.what() << "\n";
    }
  }
  // for debugging purposes: finds pointers to ints put on stack in __malloc_callback()
  // printf("Scanning C stack:\n");
  // for (int i = 0; stack_ptr + i <= base_ptr; i += sizeof(void *))
  // {
  //   printf("address: %p -> %p\n", (stack_ptr + i), *(int **)(stack_ptr + i));
  //   try
  //   {
  //     if (used_map.count(*(int **)(stack_ptr + i)) > 0)
  //     {
  //       printf("matched int : %d\n", used_map.at(*(int **)(stack_ptr + i)));
  //     }
  //   }
  //   catch (const std::exception &e)
  //   {
  //     std::cout << e.what() << "\n";
  //   }
  // }
}

void __mark_memory()
{
  std::set<uint8_t *> used_set; // For efficient lookup of addresses when scanning memory
  for (Chunk *c : used_list)
  {
    used_set.insert(c->address);
  }
  __mark_stack(stack_ptr, base_ptr, used_set);           // Mark C stack
  __mark_stack(wasm_stack_ptr, wasm_base_ptr, used_set); // Mark Wasm stack
  // TODO: scan registers and global exports
}

// collect unmarked memory and move it to the appropriate free list (aka sweep)
void __collect_memory()
{
  used_list.remove_if([](Chunk *c) {
            bool flag=c->mark;
            c->mark=false;
            if (flag==false) {
                free_list->free_chunks[c->memclass_index].push_front(c);
            }
            return flag==false;});
}

int __allocate_memory(int bytes_requested)
{
  // get end of alloc memory and save its address
  int offset = memory_bumper_offset;
  uint8_t *mem = wasmtime_memory_data(context, &memory) +
                 memory_bumper_offset;

  // calculate chunk size and class index (for free list)
  int memsize = 4;
  int memclass_index = 0;
  while (memsize < bytes_requested)
  {
    memsize *= 2; // allocations in powers of 2
    if (memclass_index < BIG)
      memclass_index += 1; // memory class
  }

  // check for remaining memory
  size_t memory_length = wasmtime_memory_data_size(context, &memory);
  int memory_remaining = memory_length - offset;
  bool oom_flag = (memory_remaining >= memsize) ? false : true;

  if (oom_flag == true)
  {
    __mark_memory();
    __collect_memory();
  }

  bool BIG_bigenough = false;
  free_list->free_chunks[BIG].remove_if([&BIG_bigenough,bytes_requested](Chunk* c) {
          if (c->size >= bytes_requested) {
              BIG_bigenough = true;
              free_list->free_chunks[BIG].push_front(c);
          }
          return c->size >= bytes_requested;
      });
  
  Chunk *chunk;

  // use freelist memory if free list is non empty (or, for BIG class, chunk is big enough)
  if ((!free_list->free_chunks[memclass_index].empty() && memclass_index != BIG) ||
      (memclass_index == BIG && BIG_bigenough == true))
  {
    chunk = free_list->free_chunks[memclass_index].front();
    free_list->free_chunks[memclass_index].pop_front();
    used_list.push_front(chunk);
    return chunk->offset;
    // make a new chunk of memory and give it data
  }
  else if (oom_flag == false)
  {
    chunk = new Chunk();
    chunk->address = mem;
    chunk->offset = offset;
    chunk->size = memsize;
    chunk->memclass_index = memclass_index;
    used_list.push_front(chunk);
    memory_bumper_offset += chunk->size; // bump the end of alloc memory forward
    return chunk->offset;
  }
  else
  {
    printf("error: out of memory\n");
    exit(-1);
  }
}

static __attribute__((noinline))
wasm_trap_t *
__malloc_callback(void *env, wasmtime_caller_t *caller,
                  const wasmtime_val_t *args, size_t nargs,
                  wasmtime_val_t *results, size_t nresults)
{
  asm("movq %%rsp, %0"
      : "=r"(stack_ptr));
  asm("movq %%rbp, %0"
      : "=r"(base_ptr));
  wasm_stack_ptr = (int *)args[2].of.i32;
  wasm_base_ptr = (int *)args[1].of.i32;
  int bytes_requested = args->of.i32;
  int offset = __allocate_memory(bytes_requested);
  results->kind = WASMTIME_I32;
  results->of.i32 = offset; // return alloc pointer as int

  // // TF: for debugging (scanning memory offset with __heap_base)
  // long stack_sz = (long)wasm_base_ptr - (long)wasm_stack_ptr;
  // printf("stack size: %ld\n", stack_sz);
  // uint8_t *memstart = wasmtime_memory_data(context, &memory);
  // uint8_t *mem_heap_base = (memstart + __heap_base - stack_sz);
  // for (int i = 0; i < stack_sz; i++)
  // {
  //   printf("addr: %p -> %d\n", mem_heap_base + i, *(mem_heap_base + i));
  // }

  // // TF: for debugging (for testing and printing C stack)
  // int a = 27;
  // int b = 29;
  // int c = 31;
  // int *a_ptr = &a;
  // int *b_ptr = &b;
  // int *c_ptr = &c;
  // used_map[a_ptr] = a;
  // used_map[b_ptr] = b;
  // used_map[c_ptr] = c;
  // __mark_C_stack(stack_ptr, base_ptr);
  return NULL;
}

int main()
{
  // Set up engine and config.
  wasm_config_t *config = wasm_config_new();
  wasmtime_config_static_memory_maximum_size_set(config, 1 << 30);
  assert(config != NULL);
  wasm_engine_t *engine = wasm_engine_new_with_config(config);
  assert(engine != NULL);

  // With an engine we can create a *store* which is a long-lived group of wasm
  // modules. Note that we allocate some custom data here to live in the store,
  // but here we skip that and specify NULL.
  wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
  assert(store != NULL);
  context = wasmtime_store_context(store);
  assert(context != NULL);

  wasmtime_error_t *error;

  // Create Linker
  wasmtime_linker_t *linker = wasmtime_linker_new(engine);
  error = wasmtime_linker_define_wasi(linker);
  if (error != NULL)
    exit_with_error("failed to link wasi", error, NULL);

  // Instantiate wasi
  wasi_config_t *wasi_config = wasi_config_new();
  assert(wasi_config);
  wasi_config_inherit_argv(wasi_config);
  wasi_config_inherit_env(wasi_config);
  wasi_config_inherit_stdin(wasi_config);
  wasi_config_inherit_stdout(wasi_config);
  wasi_config_inherit_stderr(wasi_config);
  wasm_trap_t *trap = NULL;
  error = wasmtime_context_set_wasi(context, wasi_config);
  if (error != NULL)
    exit_with_error("failed to instantiate WASI", error, NULL);

  // Read WASM binary
  wasm_byte_vec_t wasm;

  FILE *file = fopen("./test.wasm", "rb");
  assert(file != NULL);
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_new_uninitialized(&wasm, file_size);
  size_t objs_read = fread(wasm.data, file_size, 1, file);
  assert(objs_read == 1);
  fclose(file);

  // Compile WASM module
  wasmtime_module_t *module = NULL;
  error = wasmtime_module_new(engine, (uint8_t *)wasm.data, wasm.size, &module);
  wasm_byte_vec_delete(&wasm);
  if (error != NULL)
    exit_with_error("failed to compile module", error, NULL);

  // Create the __malloc callback that will be called from the WASM module
  wasm_functype_t *__malloc_ty = wasm_functype_new_3_1(wasm_valtype_new(WASM_I32), wasm_valtype_new(WASM_I32), wasm_valtype_new(WASM_I32), wasm_valtype_new(WASM_I32));
  wasmtime_func_t __malloc;
  wasmtime_func_new(context, __malloc_ty, __malloc_callback, NULL, NULL, &__malloc);
  // Add the callback in linker
  error = wasmtime_linker_define_func(linker, "env", 3, "__malloc", strlen("__malloc"), __malloc_ty, __malloc_callback, NULL, NULL);

  // Instantiate the module using linker
  wasmtime_instance_t instance;
  error = wasmtime_linker_instantiate(linker, context, module, &instance, &trap);
  if (error != NULL)
    exit_with_error("failed to instantiate module", error, NULL);

  // Get _start and memory, which are exports in the WASM module
  wasmtime_extern_t run;
  bool ok = wasmtime_instance_export_get(context, &instance, "_start", strlen("_start"), &run);
  assert(ok);
  assert(run.kind == WASMTIME_EXTERN_FUNC);

  wasmtime_extern_t item;
  ok = wasmtime_instance_export_get(context, &instance, "memory", strlen("memory"), &item);
  assert(ok && item.kind == WASMTIME_EXTERN_MEMORY);
  memory = item.of.memory;

  // Get all other exports
  wasm_exporttype_vec_t exports;
  wasmtime_instancetype_t *instancety = wasmtime_instance_type(context, &instance);
  wasmtime_instancetype_exports(instancety, &exports);

  for (int i = 0; i < exports.size; i++)
  {
    wasm_exporttype_t *exportWasm = exports.data[i];
    const wasm_name_t *exportname = wasm_exporttype_name(exportWasm);
    if (strcmp(exportname->data, "memory") == 0 || strcmp(exportname->data, "_start") == 0)
      continue;

    ok = wasmtime_instance_export_get(context, &instance, exportname->data, exportname->size, &item);
    assert(ok && item.kind == WASMTIME_EXTERN_GLOBAL);

    wasmtime_global_t global = item.of.global;
    wasmtime_val_t globalval;
    wasmtime_global_get(context, &global, &globalval);
    std::cout << "exportname: " << exportname->data << "\n";

    if (strcmp(exportname->data, "__heap_base") == 0)
    {
      __heap_base = globalval.of.i64;
      printf("heapbase: %ld\n", __heap_base);
    }
    if (strcmp(exportname->data, "__data_end") == 0)
    {
      __data_end = globalval.of.i64;
      printf("__data_end: %ld\n", __data_end);
    }
  }
  // what is this for?
  // printf("144: %p\n", wasmtime_memory_data(context, &memory));
  // uint8_t *hp_bs = (uint8_t *)70288;

  // Call _start
  wasmtime_val_t results;
  error = wasmtime_func_call(context, &run.of.func, NULL, 0, NULL, 0, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to call function", error, trap);

  // print_memory(0, 40, 4);

  // print_registers();

  // If you want to grow and read memory you can use below functions
  //  returns memory size in wasm pages
  //  size_t memory_size = wasmtime_memory_size(context, &memory);
  //  printf("%lu",memory_size);

  // returns memory size in bytes (returns inconsistent numbers when called from here. okay inside malloc callback)
  // size_t data_size = wasmtime_memory_data_size(context, &memory);
  // printf("data size: %lu",data_size);

  // wasm_memorytype_t* memty;
  // memty = wasmtime_memory_type(context, &memory);
  // size_t max;
  // wasmtime_memorytype_maximum(memty, &max);
  // size_t prev_size;
  // error = wasmtime_memory_grow(context, &memory, max - memory_size, &prev_size);
  // if (error != NULL)
  //   exit_with_error("failed to instantiate module", error, NULL);
  // MJ: guess: this part is to show you the memory grew?
  // memory_size = wasmtime_memory_size(context, &memory);
  // data_size = wasmtime_memory_data_size(context, &memory);

  // read memory in integers (copied into "print memory")
  //  for (int i = 0; i < 40; i+=4) {
  //      uint32_t j = (wasmtime_memory_data(context, &memory)[i]) |
  //          (wasmtime_memory_data(context, &memory)[i+1] << 8) |
  //          (wasmtime_memory_data(context, &memory)[i+2] << 16) |
  //          ((wasmtime_memory_data(context, &memory)[i+3]) << 24);
  //      printf("%d\n", j);
  //   }

  //-- end --
  // make sure you do this last!
  // Clean up
  wasmtime_module_delete(module);
  wasmtime_store_delete(store);
  wasm_engine_delete(engine);
  return 0;
}

static void exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap)
{
  fprintf(stderr, "error: %s\n", message);
  wasm_byte_vec_t error_message;
  if (error != NULL)
  {
    wasmtime_error_message(error, &error_message);
    wasmtime_error_delete(error);
  }
  else
  {
    wasm_trap_message(trap, &error_message);
    wasm_trap_delete(trap);
  }
  fprintf(stderr, "%.*s\n", (int)error_message.size, error_message.data);
  wasm_byte_vec_delete(&error_message);
  exit(1);
}
