#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#include "../../rand.h"

#define EXP_SIZE 11ULL
#define EXP_SHIFT 52ULL
#define EXP_MASK (((1ULL << EXP_SIZE) - 1ULL) << EXP_SHIFT)


static void run_test(uint32_t fpscr) {

  // 10k rounds should be enough
  unsigned int rounds = 10000;

  char path[64];
  sprintf(path, "out-ftoi-0x%08" PRIX32 ".txt", fpscr);

  // Initialize the seed and open a log file
  FILE* f = fopen(path, "w");
  if (f == NULL) {
     return;
  }

  uint32_t old_fpscr = 0xFFFFFFFF;
  asm volatile("vmrs %0, fpscr\n"
               : "=r"(old_fpscr));
  fprintf(f, "Setting fpscr 0x%08" PRIX32 ", keeping 0x%08" PRIX32 "\n", fpscr, old_fpscr);
  fprintf(f, "Starting\n");

  srand(555);
  for(unsigned int i = 0; i < rounds; i++) {

    // We'll do 5 different cases for simplicity

    // 0: vdm_exponent < 1022 (neither)
    // 1: vdm_exponent = 1022 (vdm_exponent >= 1023 - 1)
    // 2: vdm_exponent = 1023 (vdm_exponent >= 1023 but also >= 1023 - 1)
    // 3: vdm_exponent = rand()
    // 4: vdm_exponent = rand() in [-5;+34]

    unsigned int type = i % 5;
    int exp = 0;
    switch(type) {
      case 0:
        exp = rand32() % 1022;
        break;
      case 1:
        exp = 1022;
        break;
      case 2:
        exp = 1023;
        break;
      case 3:
        exp = rand32();
        break;
      case 4:
        exp = (rand32() % 40) - 5;
        break;
      default:
        fprintf(f, "Unsupported mode %d!\n", type);
        break;
    }

    // Generate random mantissa + sign and replace the exp by what we chose
    uint64_t in = rand64();
    in &= ~EXP_MASK;
    in |= ((uint64_t)(exp + 1023) << EXP_SHIFT) & EXP_MASK;

    uint32_t out_u;
    uint32_t out_s;

    // Have to reset fpscr before doing any calls to nintendo code,
    // as there seems to be a bug with some rounding modes
    asm volatile("vmsr fpscr, %3\n"
                 "vldr d0, %2\n"
                 "ftouid s0, d0\n"
                 "ftosid s1, d0\n"
                 "vstr s0, %0\n"
                 "vstr s1, %1\n"
                 "vmsr fpscr, %4\n"
                 : "=m"(out_u), "=m"(out_s)
                 : "m"(in), "r"(fpscr), "r"(old_fpscr));


    fprintf(f, "in: 0x%016" PRIX64 "\ttype: %d\tfptoui: 0x%08" PRIX32 "\tfptosi: 0x%08" PRIX32 "\n", in, type, out_u, out_s);

  }

  // Close the log file
  fclose(f);

}

int main() {

  run_test(0x01000000);
  run_test(0x01400000);
  run_test(0x01800000);
  run_test(0x01C00000);

  run_test(0x03000000);
  run_test(0x03400000);
  run_test(0x03800000);
  run_test(0x03C00000);

  return 0;

}
