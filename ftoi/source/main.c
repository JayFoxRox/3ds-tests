#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#include "../../asm.h"
#include "../../rand.h"

// 20k rounds should be enough
static unsigned int rounds = 20000;

#define EXP64_SIZE 11ULL
#define EXP64_SHIFT 52ULL
#define EXP64_MASK (((1ULL << EXP64_SIZE) - 1ULL) << EXP64_SHIFT)

#define EXP32_SIZE 8UL
#define EXP32_SHIFT 23UL
#define EXP32_MASK (((1UL << EXP32_SIZE) - 1UL) << EXP32_SHIFT)

// We'll do 5 different cases for simplicity

// 0: vdm_exponent < 1022 (neither)
// 1: vdm_exponent = 1022 (vdm_exponent >= 1023 - 1)
// 2: vdm_exponent = 1023 (vdm_exponent >= 1023 but also >= 1023 - 1)
// 3: vdm_exponent = rand()
// 4: vdm_exponent = rand() in [-5;+34]

static int select_exp64(unsigned int i) {
    unsigned int type = i % 5;
    switch(type) {
      case 0:
        return rand32() % 1022;
      case 1:
        return 1022;
      case 2:
        return 1023;
      case 3:
        return rand32();
      case 4:
        return (rand32() % 40) - 5;
      default:
        break;
    }
    return 0;
}

static uint64_t select_f64(unsigned int i) {
  // Generate random mantissa + sign and replace the exp by what we chose
  int exp = select_exp64(i);
  uint64_t in = rand64();
  in &= ~EXP64_MASK;
  in |= ((uint64_t)(exp + 1023) << EXP64_SHIFT) & EXP64_MASK;
  return in;
}

static int select_exp32(unsigned int i) {
    unsigned int type = i % 5;
    switch(type) {
      case 0:
        return rand32() % 126;
      case 1:
        return 126;
      case 2:
        return 127;
      case 3:
        return rand32();
      case 4:
        return (rand32() % 40) - 5;
      default:
        break;
    }
    return 0;
}

static uint32_t select_f32(unsigned int i) {
  // Generate random mantissa + sign and replace the exp by what we chose
  int exp = select_exp32(i);
  uint32_t in = rand32();
  in &= ~EXP32_MASK;
  in |= ((uint32_t)(exp + 127) << EXP32_SHIFT) & EXP32_MASK;
  return in;
}

static void run_test_ftoi32(FILE* f, unsigned int i, uint32_t fpscr) {
  uint32_t in = select_f32(i);
  uint32_t old_fpscr = get_fpscr();

  set_fpscr(fpscr);
  uint32_t out_u = ftoui32(in);
  uint32_t new_fpscr_u = get_fpscr();

  set_fpscr(fpscr);
  uint32_t out_s = ftosi32(in);
  uint32_t new_fpscr_s = get_fpscr();

  set_fpscr(old_fpscr);
  fprintf(f, "0x%08" PRIX32 " => u:0x%08" PRIX32 " fpscr: 0x%08" PRIX32 " | s:0x%08" PRIX32 " fpscr: 0x%08" PRIX32 "\n",
          in, out_u, new_fpscr_u, out_s, new_fpscr_s);
  return;
}

static void run_test_ftoi64(FILE* f, unsigned int i, uint32_t fpscr) {
  uint64_t in = select_f64(i);
  uint64_t old_fpscr = get_fpscr();

  set_fpscr(fpscr);
  uint32_t out_u = ftoui64(in);
  uint32_t new_fpscr_u = get_fpscr();

  set_fpscr(fpscr);
  uint32_t out_s = ftosi64(in);
  uint32_t new_fpscr_s = get_fpscr();

  set_fpscr(old_fpscr);
  fprintf(f, "0x%016" PRIX64 " => u:0x%08" PRIX32 " fpscr: 0x%08" PRIX32 " | s:0x%08" PRIX32 " fpscr: 0x%08" PRIX32 "\n",
          in, out_u, new_fpscr_u, out_s, new_fpscr_s);
  return;
}

static void run_test_to_file(const char* path, void(*callback)(FILE* f, unsigned int i, uint32_t fpscr), uint32_t fpscr) {
  // Initialize the seed and open a log file
  FILE* f = fopen(path, "w");
  if (f == NULL) {
     return;
  }

  fprintf(f, "Setting fpscr 0x%08" PRIX32 "\n", fpscr);
  fprintf(f, "Starting\n");

  srand(555);
  for(unsigned int i = 0; i < rounds; i++) {
    callback(f, i, fpscr);
  }
  
  fclose(f);
  return;
}

static void run_test(uint32_t fpscr) {
  char path[64];

  sprintf(path, "out-ftoi32-0x%08" PRIX32 ".txt", fpscr);
  run_test_to_file(path, run_test_ftoi32, fpscr);

  sprintf(path, "out-ftoi64-0x%08" PRIX32 ".txt", fpscr);
  run_test_to_file(path, run_test_ftoi64, fpscr);
}

int main() {

  // We must have default NaN enabled or the 3DS will crash on denormals?!

  run_test(0x03000000); // works!
  run_test(0x03400000); // works!
  run_test(0x03800000); // works!
  run_test(0x03C00000); // works!

  // These shouldn't influence the result
  run_test(0x00000000); // crashes?!
  run_test(0x01400000);
  run_test(0x02800000);


  return 0;

}
