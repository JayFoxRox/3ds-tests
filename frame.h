#include "morton.h"

static void dump_buffer32(const char* path, uint8_t* buffer, unsigned int width, unsigned int height, bool rgbflip) {

  struct TGAHeader {
      char  idlength;
      char  colormaptype;
      char  datatypecode;
      short int colormaporigin;
      short int colormaplength;
      short int x_origin;
      short int y_origin;
      short width;
      short height;
      char  bitsperpixel;
      char  imagedescriptor;
  };

  struct TGAHeader hdr = {0, 0, 2, 0, 0, 0, 0, width, height, 32, 0};
  FILE* f = fopen(path, "wb");

  fwrite(&hdr, sizeof(struct TGAHeader), 1, f);

  uint8_t* row_buffer = malloc(width * 4);
  for (unsigned int y = 0; y < height; y++) {
    uint8_t* out_pixel = row_buffer;
    for (unsigned int x = 0; x < width; x++) {
      uint8_t* pixel = &buffer[GetPixelOffset(x, y, width, height, 4)];

      //FIXME: Buffer to row. this is slow ..

      *out_pixel++ = pixel[rgbflip ? 1 : 0];
      *out_pixel++ = pixel[rgbflip ? 2 : 1];
      *out_pixel++ = pixel[rgbflip ? 3 : 2];
      *out_pixel++ = rgbflip ? pixel[0] : 255; // Alpha

    }
    fwrite(row_buffer, 1, width * 4, f);
  }
  free(row_buffer);

  fclose(f);
}

static void dump_frame(const C3D_RenderBuf* rb, const char* title, bool depth, unsigned int width, unsigned int height) {
  char path_buffer[1024];
  printf("> %s\n", title);
  sprintf(path_buffer, "%s-color.tga", title);
  dump_buffer32(path_buffer, (u8*)rb->colorBuf.data, width, height, true);
  if (depth) {
    sprintf(path_buffer, "%s-depth.tga", title);
    dump_buffer32(path_buffer, (u8*)rb->depthBuf.data, width, height, false);
  }
}
