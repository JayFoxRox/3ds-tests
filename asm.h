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
                 "ftosid s2, d0\n"
                 "nop\n"
                 "vstr s2, %0\n"
                 "nop\n"
                 : "=m"(out)
                 : "m"(in));
  return out;
}

#define UXTB16(rot) \
  static uint32_t uxtb16_##rot(uint32_t m) { \
    uint32_t d; \
    asm volatile("uxtb16 %0, %1, ror #" #rot "\n" : "=r"(d) : "r"(m)); \
    return d; \
  }
UXTB16(0) UXTB16(8) UXTB16(16) UXTB16(24)


#define USAT16(sat) \
  static uint32_t usat16_##sat(uint32_t n) { \
    uint32_t d; \
    asm volatile("usat16 %0, #" #sat ", %1\n" : "=r"(d) : "r"(n)); \
    return d; \
  }
USAT16( 0) USAT16( 1) USAT16( 2) USAT16( 3)
USAT16( 4) USAT16( 5) USAT16( 6) USAT16( 7)
USAT16( 8) USAT16( 9) USAT16(10) USAT16(11)
USAT16(12) USAT16(13) USAT16(14) USAT16(15)

#define PKHBT(lsl) \
  static uint32_t pkhbt_##lsl(uint32_t n, uint32_t m) { \
    uint32_t d; \
    asm volatile("pkhbt %0, %1, %2, lsl #" #lsl "\n" : "=r"(d) : "r"(n), "r"(m)); \
    return d; \
  }
PKHBT( 0) PKHBT( 1) PKHBT( 2) PKHBT( 3)
PKHBT( 4) PKHBT( 5) PKHBT( 6) PKHBT( 7)
PKHBT( 8) PKHBT( 9) PKHBT(10) PKHBT(11)
PKHBT(12) PKHBT(13) PKHBT(14) PKHBT(15)
PKHBT(16) PKHBT(17) PKHBT(18) PKHBT(19)
PKHBT(20) PKHBT(21) PKHBT(22) PKHBT(23)
PKHBT(24) PKHBT(25) PKHBT(26) PKHBT(27)
PKHBT(28) PKHBT(29) PKHBT(30) PKHBT(31)

#define PKHTB(asr) \
  static uint32_t pkhtb_##asr(uint32_t n, uint32_t m) { \
    uint32_t d; \
    asm volatile("pkhtb %0, %1, %2, asr #" #asr "\n" : "=r"(d) : "r"(n), "r"(m)); \
    return d; \
  }
PKHTB( 1) PKHTB( 2) PKHTB( 3) PKHTB( 4)
PKHTB( 5) PKHTB( 6) PKHTB( 7) PKHTB( 8)
PKHTB( 9) PKHTB(10) PKHTB(11) PKHTB(12)
PKHTB(13) PKHTB(14) PKHTB(15) PKHTB(16)
PKHTB(17) PKHTB(18) PKHTB(19) PKHTB(20)
PKHTB(21) PKHTB(22) PKHTB(23) PKHTB(24)
PKHTB(25) PKHTB(26) PKHTB(27) PKHTB(28)
PKHTB(29) PKHTB(30) PKHTB(31) PKHTB(32)

static uint32_t smlad(uint32_t n, uint32_t m, uint32_t a) {
  uint32_t d;
  asm volatile("smlad %0, %1, %2, %3\n" : "=r"(d) : "r"(n), "r"(m), "r"(a));
  return d;
}
