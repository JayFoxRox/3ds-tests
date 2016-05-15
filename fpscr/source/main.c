#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <malloc.h>

#include "thread.h"
#include "../../asm.h"

#include <3ds.h>

FILE* f;

static void thread_func(void* arg) {
  uint32_t old_fpscr = *(uint32_t*)arg;
  uint32_t new_fpscr = get_fpscr();
  fprintf(f, "old_fpscr: 0x%08" PRIX32 "\n", old_fpscr);
  fprintf(f, "new_fpscr: 0x%08" PRIX32 "\n", new_fpscr);
  svcExitThread();
}

static void run_thread(void) {

  // Get the current (old) fpscr
  static uint32_t old_fpscr;
  old_fpscr = get_fpscr();

  size_t stack_size = 0x10000;
#if 1
  Thread thread = threadCreate(thread_func, &old_fpscr, stack_size, 0x3F, -2, false);
  threadJoin(thread, U64_MAX);
  threadFree(thread);
#else
  // Didn't work in citra for me..?
  Handle thread;
  uint8_t* stack = memalign(32, stack_size);
  Result res = svcCreateThread(&thread, thread_func, (u32)&old_fpscr, (u32*)&stack[stack_size - 8], 0x3F, -2);
	if (R_FAILED(res)) {
    fprintf(f, "Failed to create thread\n");
    return 1;
  }
  svcWaitSynchronization(thread, U64_MAX);
#endif
  return;
}

int main() {

  // Open log
  f = fopen("out-fpscr.txt", "w");

  // default fpscr
  run_thread();
  
  // force fpscr
  set_fpscr(0x00000000);
  run_thread();
  set_fpscr(0x03C00000);
  run_thread();
  set_fpscr(0x03C00010);
  run_thread();

  // Close log
  fclose(f);

  return 0;
}
