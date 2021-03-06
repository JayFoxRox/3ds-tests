#ifndef __MORTON_H__
#define __MORTON_H__


// Stolen from citra at https://github.com/citra-emu/citra/blob/master/src/video_core/utils.h

// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version

/**
 * Interleave the lower 3 bits of each coordinate to get the intra-block offsets, which are
 * arranged in a Z-order curve. More details on the bit manipulation at:
 * https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
 */
static inline u32 MortonInterleave(u32 x, u32 y) {
    u32 i = (x & 7) | ((y & 7) << 8); // ---- -210
    i = (i ^ (i << 2)) & 0x1313;      // ---2 --10
    i = (i ^ (i << 1)) & 0x1515;      // ---2 -1-0
    i = (i | (i >> 7)) & 0x3F;
    return i;
}

/**
 * Calculates the offset of the position of the pixel in Morton order
 */
static inline u32 GetMortonOffset(u32 x, u32 y, u32 bytes_per_pixel) {
    // Images are split into 8x8 tiles. Each tile is composed of four 4x4 subtiles each
    // of which is composed of four 2x2 subtiles each of which is composed of four texels.
    // Each structure is embedded into the next-bigger one in a diagonal pattern, e.g.
    // texels are laid out in a 2x2 subtile like this:
    // 2 3
    // 0 1
    //
    // The full 8x8 tile has the texels arranged like this:
    //
    // 42 43 46 47 58 59 62 63
    // 40 41 44 45 56 57 60 61
    // 34 35 38 39 50 51 54 55
    // 32 33 36 37 48 49 52 53
    // 10 11 14 15 26 27 30 31
    // 08 09 12 13 24 25 28 29
    // 02 03 06 07 18 19 22 23
    // 00 01 04 05 16 17 20 21
    //
    // This pattern is what's called Z-order curve, or Morton order.

    const unsigned int block_height = 8;
    const unsigned int coarse_x = x & ~7;

    u32 i = MortonInterleave(x, y);

    const unsigned int offset = coarse_x * block_height;

    return (i + offset) * bytes_per_pixel;
}


static u32 GetPixelOffset(u32 x, u32 y, u32 width, u32 height, u32 bytes_per_pixel) {
  int new_y = height - y;
  const u32 coarse_y = new_y & ~7;
  u32 stride = width * bytes_per_pixel;
  return GetMortonOffset(x, new_y, bytes_per_pixel) + coarse_y * stride;
}

#endif
