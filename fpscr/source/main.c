#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <3ds.h>

uint32_t get_fpscr(void) {
  uint32_t fpscr;
  asm volatile("vmrs %0, fpscr\n" : "=r"(fpscr));
  return fpscr;
}

void thread_func(void* arg) {
  uint32_t old_fpscr = *(uint32_t*)arg;
  uint32_t new_fpscr = get_fpscr();
  printf("old_fpscr: 0x%08" PRIX32 "\n", old_fpscr);
  printf("new_fpscr: 0x%08" PRIX32 "\n", new_fpscr);
  svcExitProcess();
}

int main() {
  static uint32_t old_fpscr;
  old_fpscr = get_fpscr();
  Handle thread;
  Result res = svcCreateThread(&thread, thread_func, &old_fpscr, ..., 0x20, 0);
  while(1);
}
