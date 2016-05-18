// Contains code from ffmpeg


/*
 * Copyright (C) 2010 David Conrad
 * Copyright (C) 2010 Ronald S. Bultje
 * Copyright (C) 2014 Peter Ross
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


/*
 * VP8 ARMv6 optimisations
 *
 * Copyright (c) 2010 Google Inc.
 * Copyright (c) 2010 Rob Clark <rob@ti.com>
 * Copyright (c) 2011 Mans Rullgard <mans@mansr.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * This code was partially ported from libvpx, which uses this license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


  asm(R"(


  fourtap_filters_1324:
          .short     -6,  12, 123, -1
          .short     -9,  50,  93, -6
          .short     -6,  93,  50, -9
          .short     -1, 123,  12, -6

.macro  sat4            r0,  r1,  r2,  r3
        asr             \r0, \r0, #7
        asr             \r1, \r1, #7
        pkhbt           \r0, \r0, \r2, lsl #9
        pkhbt           \r1, \r1, \r3, lsl #9
        usat16          \r0, #8,  \r0
        usat16          \r1, #8,  \r1
        orr             \r0, \r0, \r1, lsl #8
.endm

.macro  movrel rd, val
        ldr             \rd, =\val
.endm

.func vp8_put_epel_h4_armv6
        push            {r1, r4-r11, lr}
bl_put_epel_h4_armv6:
        subs            r2,  r2,  #1
        movrel          lr,  fourtap_filters_1324 - 4
        add             lr,  lr,  r12, lsl #2
        sub             r3,  r3,  r4
        ldm             lr,  {r5, r6}
        ldr             lr,  [sp, #44]
1:
        ldr             r9,  [r2, #3]
        ldr             r8,  [r2, #2]
        ldr             r7,  [r2], #4

        uxtb16          r9,  r9,  ror #8        @ src[6] | src[4]
        uxtb16          r10, r8,  ror #8        @ src[5] | src[3]
        uxtb16          r8,  r8                 @ src[4] | src[2]
        uxtb16          r11, r7,  ror #8        @ src[3] | src[1]
        uxtb16          r7,  r7                 @ src[2] | src[0]

        mov             r12, #0x40
        smlad           r9,  r9,  r6,  r12      @ filter[3][1]
        smlad           r7,  r7,  r5,  r12      @ filter[0][0]
        smlad           r9,  r10, r5,  r9       @ filter[3][0]
        smlad           r10, r10, r6,  r12      @ filter[2][1]
        smlad           r12, r11, r5,  r12      @ filter[1][0]
        smlad           r7,  r11, r6,  r7       @ filter[0][1]
        smlad           r10, r8,  r5,  r10      @ filter[2][0]
        smlad           r12, r8,  r6,  r12      @ filter[1][1]

        subs            r4,  r4,  #4

        sat4            r7,  r12, r10, r9
        str             r7,  [r0], #4

        bne             1b

        subs            lr,  lr,  #1
        ldr             r4,  [sp, #40]
        add             r2,  r2,  r3
        add             r0,  r0,  r1

        bne             1b

        pop             {r1, r4-r11, pc}
.endfunc


.macro  vp8_mc_1        name, size, hv
ff_put_vp8_\name\size\()_\hv\()_armv6:
.func ff_put_vp8_\name\size\()_\hv\()_armv6 @ export=1
        sub             r1,  r1,  #\size
        mov             r12, sp
        push            {r1, r4-r11, lr}
        ldm             r12, {r5-r7}
        mov             r4,  #\size
        stm             r12, {r4, r5}
        orr             r12, r6,  r7
        b               bl_put_\name\()_\hv\()_armv6
.endfunc
.endm

vp8_mc_1                epel,   8, h4
vp8_mc_1                epel,   4, h4

  )");

void ff_put_vp8_epel8_h4_armv6(uint8_t *dst,
                               ptrdiff_t dststride,
                               uint8_t *src,
                               ptrdiff_t srcstride,
                               int h, int mx, int my);
void ff_put_vp8_epel4_h4_armv6(uint8_t *dst,
                               ptrdiff_t dststride,
                               uint8_t *src,
                               ptrdiff_t srcstride,
                               int h, int mx, int my);

void asm_vp8_put_epel_h4_armv6(uint8_t *dst,
                               ptrdiff_t dststride,
                               uint8_t *src,
                               ptrdiff_t srcstride,
                               int h, int mx, int my
) {
  ff_put_vp8_epel8_h4_armv6(dst, dststride, src, srcstride, h, mx, my);
}

#define FILTER_6TAP(src, F, stride)                                           \
    cm[(F[2] * src[x + 0 * stride] - F[1] * src[x - 1 * stride] +             \
        F[0] * src[x - 2 * stride] + F[3] * src[x + 1 * stride] -             \
        F[4] * src[x + 2 * stride] + F[5] * src[x + 3 * stride] + 64) >> 7]

#define FILTER_4TAP(src, F, stride)                                           \
    cm[(F[2] * src[x + 0 * stride] - F[1] * src[x - 1 * stride] +             \
        F[3] * src[x + 1 * stride] - F[4] * src[x + 2 * stride] + 64) >> 7]

#define VP8_EPEL_H(SIZE, TAPS)                                                \
static void put_vp8_epel ## SIZE ## _h ## TAPS ## _c(uint8_t *dst,            \
                                                     ptrdiff_t dststride,     \
                                                     uint8_t *src,            \
                                                     ptrdiff_t srcstride,     \
                                                     int h, int mx, int my)   \
{                                                                             \
    const uint8_t *filter = subpel_filters[mx - 1];                           \
    const uint8_t *cm     = ff_crop_tab + MAX_NEG_CROP;                       \
    int x, y;                                                                 \
    for (y = 0; y < h; y++) {                                                 \
        for (x = 0; x < SIZE; x++)                                            \
            dst[x] = FILTER_ ## TAPS ## TAP(src, filter, 1);                  \
        dst += dststride;                                                     \
        src += srcstride;                                                     \
    }                                                                         \
}

static const uint8_t subpel_filters[7][6] = {
    { 0,  6, 123,  12,  1, 0 },
    { 2, 11, 108,  36,  8, 1 },
    { 0,  9,  93,  50,  6, 0 },
    { 3, 16,  77,  77, 16, 3 },
    { 0,  6,  50,  93,  9, 0 },
    { 1,  8,  36, 108, 11, 2 },
    { 0,  1,  12, 123,  6, 0 },
};


#define MAX_NEG_CROP 1024

#define times4(x) x, x, x, x
#define times256(x) times4(times4(times4(times4(times4(x)))))

const uint8_t ff_crop_tab[256 + 2 * MAX_NEG_CROP] = {
times256(0x00),
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF,
times256(0xFF)
};

VP8_EPEL_H(8, 4)
VP8_EPEL_H(4, 4)

void c_vp8_put_epel_h4_armv6(uint8_t *dst,
                             ptrdiff_t dststride,
                             uint8_t *src,
                             ptrdiff_t srcstride,
                             int h, int mx, int my) {
  put_vp8_epel8_h4_c(dst, dststride, src, srcstride, h, mx, my);
}

void cmp_vp8_put_epel_h4_armv6(const char* path,
                               void(*callback)(uint8_t *dst,
                                   ptrdiff_t dststride,
                                   uint8_t *src,
                                   ptrdiff_t srcstride,
                                   int h, int mx, int my)
) {

  static uint8_t dst[1024];
  static uint8_t src[1024];

  FILE* f = fopen(path, "w");
  memset(dst, 0xAA, sizeof(dst));
  memset(src, 0xAA, sizeof(src));
  callback(dst, 8, src, 8, 8, 8, 8);
  fwrite(dst, sizeof(dst), 1, f);
  fclose(f);

}

int main() {
  cmp_vp8_put_epel_h4_armv6("out-ffmpeg-asm.bin", asm_vp8_put_epel_h4_armv6);
//  cmp_vp8_put_epel_h4_armv6("out-ffmpeg-c.bin", c_vp8_put_epel_h4_armv6);
  return 0;
}
