#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wasm.h>
#include <wasmtime.h>
// cpp
#include <iostream>
#include <list>
#include <set>
#include <cstdlib>

static void exit_with_error(const char *message,
                            wasmtime_error_t *error, wasm_trap_t *trap);

wasmtime_memory_t memory;
wasmtime_context_t *context;
long __heap_base;             // exported globals pointing to bounds of wasm stack
long __data_end;              // exported globals pointing to bounds of wasm stack
int memory_bumper_offset = 0; // integer offset for end of allocated memory
int heap_offset = 0;          // start of heap (init in first __malloc call for both of these )
std::list<long> export_list;  // List of all exports
double WASM_GC_THRESH = 0.8;  // Environmental variable inititialized in main(). If not found set default=0.8

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
  uint8_t *mem = wasmtime_memory_data(context, &memory) + heap_offset;
  for (int i = start; i < end; i += step)
  {
    uint32_t data = (mem[i] | (mem[i + 1]) << 8 | (mem[i + 2]) << 16 | (mem[i + 3]) << 24);
    printf("%p -> %d\n", mem + i, data);
  }
}

// return size of wasm memory
size_t wasmMemorySize()
{
  return (size_t)wasmtime_memory_data_size(context, &memory);
}
// return number of bytes allocated in size class
size_t bytesAllocatedInSizeClass(int szClass)
{
  int classes[11] = {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
  int mc = -1;
  if (szClass > 4096)
    mc = BIG;
  for (int i = 10; i-- > 0;)
  {
    if (szClass == classes[i])
    {
      mc = i;
      break;
    }
  }
  if (mc == -1)
    return 0;
  int count = 0;
  for (Chunk *c : used_list)
  {
    if (c->memclass_index == mc)
      count += c->size;
  }
  return count;
}

// return number of bytes allocaed in the wasm memory
size_t getBytesAllocated()
{
  int classes[12] = {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 4097};
  int count = 0;
  for (int i : classes)
    count += bytesAllocatedInSizeClass(i);
  return count;
}

/*--GC functions--*/

// scan memory and mark accessable chunks

// Initialize C and Wasm stack pointers.  Mark functions will use these to scan though data
void *stack_ptr;
void *base_ptr;
void *wasm_stack_ptr;
void *wasm_base_ptr;

void __mark_stack(void *stack_ptr, void *base_ptr, std::set<int> used_set)
{
  for (int i = 0; stack_ptr + i <= base_ptr; i += sizeof(int)) // Scan through C stack
  {
    try
    {
      // If what appears to be pointer to used_list found on stack, mark that chunk
      if (used_set.count(*(int *)(stack_ptr + i)) > 0)
      {
        for (Chunk *c : used_list)
        {
          if (c->offset == *(int *)(stack_ptr + i))
          {
            if (!c->mark)
              printf("Object at offset %d marked\n", c->offset);
            c->mark = true;
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
}

void __mark_registers(std::set<int> used_set)
{
  void *rsp_ptr;
  void *rbp_ptr;
  void *rax_ptr;
  void *rcx_ptr;
  void *rdx_ptr;
  void *rbx_ptr;
  void *r8_ptr;
  void *r9_ptr;
  void *r10_ptr;
  void *r11_ptr;
  void *r12_ptr;
  void *r13_ptr;
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
  asm("movq %%r8, %0"
      : "=r"(r8_ptr));
  asm("movq %%r9, %0"
      : "=r"(r9_ptr));
  asm("movq %%r10, %0"
      : "=r"(r10_ptr));
  asm("movq %%r11, %0"
      : "=r"(r11_ptr));
  asm("movq %%r12, %0"
      : "=r"(r12_ptr));
  asm("movq %%r13, %0"
      : "=r"(r13_ptr));
  asm("movq %%r14, %0"
      : "=r"(r14_ptr));
  asm("movq %%r15, %0"
      : "=r"(r15_ptr));

  uint8_t *memstart = wasmtime_memory_data(context, &memory);
  std::list<void *> register_list({rsp_ptr, rbp_ptr, rax_ptr, rcx_ptr,
                                   rdx_ptr, rbx_ptr, r8_ptr, r9_ptr, r10_ptr, r11_ptr, r12_ptr, r13_ptr, r14_ptr, r15_ptr});
  std::list<void *> accessible_reg_list;
  // Only access registers with valid addresses to avoid segmentation fault
  for (void *reg : register_list)
  {
    if ((long)reg >= (long)memstart)
    {
      accessible_reg_list.push_back(reg);
    }
  }
  for (void *reg : accessible_reg_list)
  {
    try
    {
      // If what appears to be pointer to used_list found, mark that chunk
      if (used_set.count(*(uint32_t *)reg) > 0)
      {
        for (Chunk *c : used_list)
        {
          if (c->offset == *(uint32_t *)reg)
          {
            if (!c->mark)
              printf("Object at offset %d marked\n", c->offset);
            c->mark = true;
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
}

void __mark_exports(std::set<int> used_set)
{
  for (long elt : export_list)
  {
    try
    {
      // If what appears to be pointer to used_list found, mark that chunk
      if (used_set.count(elt) > 0)
      {
        for (Chunk *c : used_list)
        {
          if (c->offset == elt)
          {
            if (!c->mark)
              printf("Object at offset %d marked\n", c->offset);
            c->mark = true;
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
}

void __mark_memory()
{
  std::set<int> used_set; // For efficient lookup of addresses when scanning memory
  uint8_t *memstart = wasmtime_memory_data(context, &memory);
  for (Chunk *c : used_list)
  {
    used_set.insert(c->offset);
  }
  __mark_stack(stack_ptr, base_ptr, used_set);           // C Stack
  __mark_stack(wasm_stack_ptr, wasm_base_ptr, used_set); // wasm stack
  __mark_registers(used_set);                            // registers
  __mark_exports(used_set);                              // exports
}

// collect unmarked memory and move it to the appropriate free list (aka sweep)
void __collect_memory()
{
  used_list.remove_if([](Chunk *c)
                      {
            bool flag=c->mark;
            c->mark=false;
            if (flag==false) {
                free_list->free_chunks[c->memclass_index].push_front(c);
                printf("Object at offset %d collected\n", c->offset);
            }
            return flag == false; });
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
  bool oom_flag = (getBytesAllocated() + memsize <= WASM_GC_THRESH * wasmMemorySize()) ? false : true;

  if (oom_flag == true)
  {
    __mark_memory();
    __collect_memory();
  }

  bool BIG_bigenough = false;
  free_list->free_chunks[BIG].remove_if([&BIG_bigenough, bytes_requested](Chunk *c)
                                        {
          if (c->size >= bytes_requested) {
              BIG_bigenough = true;
              free_list->free_chunks[BIG].push_front(c);
          }
          return c->size >= bytes_requested; });

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
  uint8_t *memstart = wasmtime_memory_data(context, &memory);
  wasm_stack_ptr = memstart + (long)args[2].of.i32;
  wasm_base_ptr = memstart + (long)args[1].of.i32;
  // init offset
  if (heap_offset == 0)
    heap_offset += __heap_base; // stack/heap base
  if (memory_bumper_offset == 0)
    memory_bumper_offset += heap_offset;
  int bytes_requested = args->of.i32;
  int offset = __allocate_memory(bytes_requested);
  results->kind = WASMTIME_I32;
  results->of.i32 = offset; // return alloc pointer as int

  // // For testing
  // __mark_memory();
  // __collect_memory();
  // printf("-------------------------------------------\n");
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

  // Grow memory to 1GB
  size_t memory_size = wasmtime_memory_size(context, &memory);
  wasm_memorytype_t *memty;
  memty = wasmtime_memory_type(context, &memory);
  size_t max;
  wasmtime_memorytype_maximum(memty, &max);
  size_t prev_size;
  error = wasmtime_memory_grow(context, &memory, 16384 - memory_size, &prev_size);
  if (error != NULL)
    exit_with_error("failed to instantiate module", error, NULL);
  memory_size = wasmtime_memory_size(context, &memory);

  // Get all other exports
  wasm_exporttype_vec_t exports;
  wasmtime_instancetype_t *instancety = wasmtime_instance_type(context, &instance);
  wasmtime_instancetype_exports(instancety, &exports);

  printf("--processing exports--\n");
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
    printf("export name=%s, value=%ld\n", exportname->data, globalval.of.i64);
    export_list.push_back(globalval.of.i64);

    if (strcmp(exportname->data, "__heap_base") == 0) // global variable pointing to start of heap; also marking wasm stack base
    {
      __heap_base = globalval.of.i64;
    }
    if (strcmp(exportname->data, "__data_end") == 0) // global variable pointing to bound for wasm stack top
    {
      __data_end = globalval.of.i64;
    }
  }
  printf("\n");

  // WASM_GC_THRESH is environment variable.  If not found we set to 0.8
  char *wasm_gc_thresh_str = getenv("WASM_GC_THRESH");
  if (wasm_gc_thresh_str != nullptr)
  {
    try
    {
      WASM_GC_THRESH = std::atof(wasm_gc_thresh_str);
    }
    catch (const std::exception &e)
    {
      std::cout << e.what();
    }
  }
  printf("WASM_GC_THRESH: %.2f\n\n", WASM_GC_THRESH);

  // Call _start
  wasmtime_val_t results;
  error = wasmtime_func_call(context, &run.of.func, NULL, 0, NULL, 0, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to call function", error, trap);

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
