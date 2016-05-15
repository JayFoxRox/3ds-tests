// Includes nops for safety, remove them after checking the datasheets..

#include <stdint.h>

static uint32_t get_fpscr(void) {
  uint32_t fpscr = 0xFFFFFFFF;
  asm volatile("nop\n"
               "vmrs %0, fpscr\n"
               "nop\n"
               : "=r"(fpscr));
  return fpscr;
}

static void set_fpscr(uint32_t fpscr) {
  asm volatile("nop\n"
               "vmsr fpscr, %0\n"
               "nop\n"
               : : "r"(fpscr));
  return;
}

static uint32_t ftoui32(uint32_t in) {
    uint32_t out;
    asm volatile("nop\n"
                 "vldr s0, %1\n"
                 "nop\n"
                 "ftouis s2, s0\n"
                 "nop\n"
                 "vstr s2, %0\n"
                 "nop\n"
                 : "=m"(out)
                 : "m"(in));
  return out;
}

static uint32_t ftosi32(uint32_t in) {
    uint32_t out;
    asm volatile("nop\n"
                 "vldr s0, %1\n"
                 "nop\n"
                 "ftosis s2, s0\n"
                 "nop\n"
                 "vstr s2, %0\n"
                 "nop\n"
                 : "=m"(out)
                 : "m"(in));
  return out;
}

static uint32_t ftoui64(uint64_t in) {
    uint32_t out;
    asm volatile("nop\n"
                 "vldr d0, %1\n"
                 "nop\n"
                 "ftouid s2, d0\n"
                 "nop\n"
                 "vstr s2, %0\n"
                 "nop\n"
                 : "=m"(out)
                 : "m"(in));
  return out;
}

static uint32_t ftosi64(uint64_t in) {
    uint32_t out;
    asm volatile("nop\n"
                 "vldr d0, %1\n"
                 "nop\n"
                 "ftouid s2, d0\n"
                 "nop\n"
                 "vstr s2, %0\n"
                 "nop\n"
                 : "=m"(out)
                 : "m"(in));
  return out;
}
