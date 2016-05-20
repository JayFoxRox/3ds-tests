#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include "vshader_shbin.h"

#define WIDTH 128
#define HEIGHT 960

#define xstr(s) str(s)
#define str(s) #s

#include "../../morton.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define CLEAR_COLOR 0x68B0D8FF

static C3D_RenderBuf rb;

static DVLB_s* vshader_dvlb;

static shaderProgram_s program;

static int uLoc_vertex;

struct {
  float x, y, weight; // weight = 0.0:bottom / 1.0:top
  float r, g, b;
} vertices[] = {
  {  0.0f,   1.0f, 1.0f, 1.0f, 0.0f, 0.0f },
  { -1.0f,  -1.0f, 0.0f, 0.0f, 1.0f, 0.0f },
  { +1.0f,  -1.0f, 0.0f, 0.0f, 0.0f, 1.0f },
};

static void* vbo_data;

void initialize(void) {

  // Load the vertex shader, create a shader program and bind it
  vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
  shaderProgramInit(&program);
  shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
  C3D_BindProgram(&program);
  
  // Get the location of the uniforms
  uLoc_vertex = shaderInstanceGetUniformLocation(program.vertexShader, "vertex");

  // Create the VBO (vertex buffer object)
  vbo_data = linearAlloc(sizeof(vertices));
  memcpy(vbo_data, vertices, sizeof(vertices));

  // Configure buffers
  C3D_BufInfo* bufInfo = C3D_GetBufInfo();
  BufInfo_Init(bufInfo);
  BufInfo_Add(bufInfo, vbo_data, sizeof(vertices[0]), 2, 0x10);

  // Configure attributes for use with the vertex shader
  C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
  AttrInfo_Init(attrInfo);
  AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=x,y,weight
  AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // v1=r,g,b

  // Configure the first fragment shading substage to just pass through the vertex color
  // See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
  C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

}

void set_fog_index(unsigned int index) {
  GPUCMD_AddWrite(GPUREG_FOG_LUT_INDEX, index);
  return;
}

void upload_fog_value(float value, float difference) {
  unsigned int fog_value = (1.0f - value) * ((1 << 11) - 1);
  int fog_difference = -difference * ((1 << 11) - 1);
  uint32_t fog_lut_entry = ((fog_value & ((1 << 11) - 1)) << 13) | (fog_difference & ((1 << 13) - 1));
  GPUCMD_AddWrite(GPUREG_FOG_LUT_DATA7, fog_lut_entry);
  return;
}

void set_linear_fog(float start, float end, unsigned int count) {

/*
  GPUREG_FOG_LUT_DATA0
  ..
  GPUREG_FOG_LUT_DATA7
*/

  float delta = (end - start) / (float)count;
  float value = start;
  for(unsigned int i = 0; i < count; i++) {
    upload_fog_value(value, delta);
    value += delta;
  }
  return;

}

void set_select_fog(float x, unsigned int count) {
  for(unsigned int i = 0; i < count; i++) {
    upload_fog_value(x, 0.0f);
  }
  return;
}

void set_alpha_blend_mode() {
}

void set_logic_op_mode() {
}

void set_fog_mode(bool z_flip, uint32_t color) {
  GPUCMD_AddWrite(GPUREG_FOG_COLOR, color);
  unsigned int fog_mode = 5;
	GPUCMD_AddMaskedWrite(GPUREG_TEXENV_UPDATE_BUFFER, 0b0101, (z_flip ? (1<<16) : 0) | fog_mode);
  return;
}

void set_alpha_test(bool enabled) {
}

void set_alpha_ref(bool enabled) {
  //FIXME: GPUREG_FRAGOP_ALPHA_TEST
}

void set_triangle(bool w_buffer, float top_depth, float bottom_depth) {


  float vertex_bottom_z = -top_depth;
  float vertex_bottom_w = 1.0f;
  float vertex_top_z = -bottom_depth;
  float vertex_top_w = 1.0f;
  float depth_scale = -1.0f;
  float depth_offset = 0.0f;
  if (w_buffer) {
    //FIXME: Do something to depth_offset but make sure we still reach the top_depth and bottom_depth!
  }

  // Configure depth mapping
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_ENABLE, w_buffer ? 0x00000000 : 0x00000001);
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_SCALE, f32tof24(depth_scale));
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_OFFSET, f32tof24(depth_offset));

  C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_vertex, vertex_bottom_z, vertex_bottom_w, vertex_top_z, vertex_top_w); // x = depth

  return;
}

void draw(void) {

  // WARNING: DO **NOT** USE ANY C3D STUFF, ESPECIALLY NOTHING EFFECT RELATED!

  // Now start clearing the buffer
  C3D_RenderBufClear(&rb);

  // Now we wait for the clear to complete and run the draw function
  C3D_VideoSync();

  // Disable or enable depth test..
  bool depth_test = false;
  u32 compare_mode = GPU_ALWAYS;
  GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK, (!!depth_test) | ((compare_mode & 7) << 4) | (GPU_WRITE_ALL << 8));

  // Draw the VBO
  C3D_DrawArrays(GPU_TRIANGLES, 0, 3);

  C3D_Flush();
  C3D_VideoSync();
//    C3D_RenderBufTransfer(&rb, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), DISPLAY_TRANSFER_FLAGS);

}

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

static void dump_frame(const char* title, bool depth) {
  char path_buffer[1024];
  printf("Generating '%s'\n", title);
  sprintf(path_buffer, "%s-color.tga", title);
  dump_buffer32(path_buffer, (u8*)rb.colorBuf.data, WIDTH, HEIGHT, true);
  if (depth) {
    sprintf(path_buffer, "%s-depth.tga", title);
    dump_buffer32(path_buffer, (u8*)rb.depthBuf.data, WIDTH, HEIGHT, false);
  }
}

void fog_test_wbuffer(const char* title, bool w_buffer) {
  char base_path_buffer[1024];
  sprintf(base_path_buffer, "%s-%cbuffer", title, w_buffer ? 'w' : 'z');

  for(unsigned int i = 0; i < 128; i += 4) {
    char path_buffer[1024];
    sprintf(path_buffer, "%s-select-%u", base_path_buffer, i);

    set_fog_index(0);
    set_linear_fog(0.0f, 0.0f, 128);
    set_fog_index(i);
    set_linear_fog(1.0f, 1.0f, 1);

    set_alpha_test(false);
    set_fog_mode(false, 0xFFFFFF00);
    set_triangle(w_buffer, 0.0f, 1.0f);

    draw();
    dump_frame(path_buffer, i == 0);
  }
  return;
}

int main() {
  // Initialize graphics
  gfxInitDefault();

  consoleInit(GFX_BOTTOM, NULL);

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

  C3D_RenderBufInit(&rb, WIDTH, HEIGHT, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  rb.clearColor = CLEAR_COLOR;
  rb.clearDepth = 0x000000;
  C3D_RenderBufClear(&rb);
  C3D_RenderBufBind(&rb);

  // Initialize the scene
//  sceneInit();

  initialize();

  fog_test_wbuffer("fog", true);
  fog_test_wbuffer("fog", false);

  //FIXME: Test fog underflow and overflow
  //FIXME: Alpha fog
  //FIXME: Fog and alpha blending etc?
  //FIXME: Fog on LOP?

  // Deinitialize the scene
//  sceneExit();

  // Deinitialize graphics
  C3D_Fini();
  gfxExit();
  return 0;
}
