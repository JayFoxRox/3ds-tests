#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "../../rand.h"
#include "../../asm.h"

unsigned int rounds = 50000;

int main() {
  FILE* f = fopen("out-ffmpeg.txt", "w");

  srand(555);
  for (unsigned int i = 0; i < rounds; i++) {
    uint32_t in_a = rand32();
    uint32_t in_b = rand32();
    uint32_t in_c = rand32();

    int x = rand();
    int x_0_3 = x % 4; // 0, 8, 16, 24
    int x_0_15 = x % 16;
    int x_0_31 = x % 32;

    uint32_t(*uxtb16[])(uint32_t) = {
      uxtb16_0, uxtb16_8, uxtb16_16, uxtb16_24,
    };

    uint32_t(*usat16[])(uint32_t) = {
      usat16_0,  usat16_1,  usat16_2,  usat16_3,
      usat16_4,  usat16_5,  usat16_6,  usat16_7,
      usat16_8,  usat16_9,  usat16_10, usat16_11,
      usat16_12, usat16_13, usat16_14, usat16_15,
    };

    uint32_t(*pkhbt[])(uint32_t, uint32_t) = {
      pkhbt_0,  pkhbt_1,  pkhbt_2,  pkhbt_3,
      pkhbt_4,  pkhbt_5,  pkhbt_6,  pkhbt_7,
      pkhbt_8,  pkhbt_9,  pkhbt_10, pkhbt_11,
      pkhbt_12, pkhbt_13, pkhbt_14, pkhbt_15,
      pkhbt_16, pkhbt_17, pkhbt_18, pkhbt_19,
      pkhbt_20, pkhbt_21, pkhbt_22, pkhbt_23,
      pkhbt_24, pkhbt_25, pkhbt_26, pkhbt_27,
      pkhbt_28, pkhbt_29, pkhbt_30, pkhbt_31,
    };

    uint32_t(*pkhtb[])(uint32_t, uint32_t) = {
      pkhtb_1,  pkhtb_2,  pkhtb_3,  pkhtb_4,
      pkhtb_5,  pkhtb_6,  pkhtb_7,  pkhtb_8,
      pkhtb_9,  pkhtb_10, pkhtb_11, pkhtb_12,
      pkhtb_13, pkhtb_14, pkhtb_15, pkhtb_16,
      pkhtb_17, pkhtb_18, pkhtb_19, pkhtb_20,
      pkhtb_21, pkhtb_22, pkhtb_23, pkhtb_24,
      pkhtb_25, pkhtb_26, pkhtb_27, pkhtb_28,
      pkhtb_29, pkhtb_30, pkhtb_31, pkhtb_32,
    };

    fprintf(f, "Input: 0x%08" PRIX32 " 0x%08" PRIX32 " 0x%08" PRIX32 ": ", in_a, in_b, in_c);

    uint32_t out_uxtb16 = uxtb16[x_0_3](in_a);
    uint32_t out_usat16 = usat16[x_0_15](in_a);
    uint32_t out_pkhbt = pkhbt[x_0_31](in_a, in_b);
    uint32_t out_pkhtb = pkhtb[x_0_31](in_a, in_b);
    uint32_t out_smlad = smlad(in_a, in_b, in_c);
    fprintf(f, "uxtb16 %2d: 0x%08" PRIX32 "\t", x_0_3 * 8, out_uxtb16);
    fprintf(f, "usat16 %2d: 0x%08" PRIX32 "\t", x_0_15, out_usat16);
    fprintf(f, "pkhbt %2d: 0x%08" PRIX32 "\t", x_0_31, out_pkhbt);
    fprintf(f, "pkhtb %2d: 0x%08" PRIX32 "\t", x_0_31 + 1, out_pkhtb);
    fprintf(f, "smlad: 0x%08" PRIX32 "\t", out_smlad);

    fprintf(f, "\n");
  }

  fclose(f);
  return 0;
}
