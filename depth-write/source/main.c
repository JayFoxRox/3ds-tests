#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include "vshader_shbin.h"

#include "../../morton.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
  (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
  GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static C3D_RenderTarget* target;

static DVLB_s* vshader_dvlb;

static shaderProgram_s program;

static int uLoc_depth;
static int uLoc_projection;
static C3D_Mtx projection;

struct {
  float x, y;
  float r, g, b;
} vertices[] = {
  { 200.0f, 200.0f, 1.0f, 0.0f, 0.0f },
  { 100.0f,  40.0f, 0.0f, 1.0f, 0.0f },
  { 300.0f,  40.0f, 0.0f, 0.0f, 1.0f }
};

static void* vbo_data;

static void sceneInit(void)
{

  // Load the vertex shader, create a shader program and bind it
  vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
  shaderProgramInit(&program);
  shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
  C3D_BindProgram(&program);
  
  // Get the location of the uniforms
  uLoc_depth = shaderInstanceGetUniformLocation(program.vertexShader, "depth");
  uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

  // Compute the projection matrix
  Mtx_OrthoTilt(&projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0);

  // Update the uniforms
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

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
  AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 2); // v0=x,y
  AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // v1=r,g,b

  // Configure the first fragment shading substage to just pass through the vertex color
  // See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
  C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void draw(void) {
  // Draw the VBO
  C3D_DrawArrays(GPU_TRIANGLES, 0, 3);
}

static int sceneRender(unsigned int mode)
{

  C3D_RenderBuf* rb = &target->renderBuf;

  // a0 = c50.x
  // a1 = v0.z

//GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_WRITE, 0x00000003);
//GPUCMD_Run();

  C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);

  float expected = 0.666f;
//  C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, CLEAR_COLOR, 0); //FIXME: depth = expected?!

//Just to make sure we are drawing something..
//vertices[0].x = (mode==2)*100.0f+200.0f;
//memcpy(vbo_data, vertices, sizeof(vertices));

  if (mode == 0) {

    expected = 0.0f; // Expecting depth clear value

  } else if (mode == 1) {

  	C3D_DepthMap(-1.0f, 0.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_depth, 0.001f, 0.0f, 0.0f, 0.0f); // x = depth
    draw();
    expected = 0.001f;

  } else if (mode == 2) {

    C3D_DepthMap(-1.0f, 0.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_depth, 0.999f, 0.0f, 0.0f, 0.0f); // x = depth
    draw();
    expected = 0.999f;

  } else if (mode == 3) {

    C3D_DepthMap(-1.0f, 0.3f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_depth, 0.2f, 0.0f, 0.0f, 0.0f); // x = depth
    draw();
    expected = 0.5f;

  } else if (mode == 4) {

    C3D_DepthMap(-0.5f, 0.3f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_depth, 0.2f, 0.0f, 0.0f, 0.0f); // x = depth
    draw();
    expected = 0.4f;

  } else {
    /* Reached the end.. */
    printf("Mode %d not found!\nUsing mode 0\n", mode);
    return sceneRender(0);
  }

  C3D_FrameEnd(0);

  // This is necessary (citra only maybe?), probably because C3D waits for the new frame too.. or something
	gspWaitForVBlank();

  // Get depth in center of screen (240x400, 32bpp FB)
  uint8_t* buffer = (u8*)rb->depthBuf.data;
  int pixel_offset = GetPixelOffset(120, 200, 240, 400, 4);
  uint32_t raw_buffer = *(u32*)&buffer[pixel_offset];

  uint32_t raw_result = raw_buffer & 0xFFFFFF;
  float f_result = raw_result / (float)0xFFFFFF;

  uint32_t raw_expected = expected * 0xFFFFFF;
  float f_expected = raw_expected / (float)0xFFFFFF;

  static int prev_mode = -1;
  if (prev_mode != mode) {
    printf("  Result: 0x%08" PRIX32 " = %f\n"
           "             (Expected %f)\n"
           "             (Diff    %+f)\n", raw_result, f_result, f_expected, f_expected - f_result);
    prev_mode = mode;
  }

//  C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, CLEAR_COLOR, raw); //FIXME: depth = expected?!

  C3D_FrameBegin(C3D_FRAME_NONBLOCK);
#if 0
  C3D_FrameDrawOn(target);

  if (raw_expected == raw_result) {
    draw();
  }

  // Test if the depth == expected, otherwise don't draw the final triangle
//  C3D_RenderTargetSetClear(target, C3D_CLEAR_COLOR, CLEAR_COLOR, 0)
  C3D_DepthTest(true, GPU_EQUAL, GPU_WRITE_ALL);
  C3D_DepthMap(0.0f, 1.0f);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_depth, expected, 0.0f, 0.0f, 0.0f); // x = depth
  draw();
#endif

  return mode;
}

static void sceneExit(void)
{
  // Free the VBO
  linearFree(vbo_data);

  // Free the shader program
  shaderProgramFree(&program);
  DVLB_Free(vshader_dvlb);
}

int main()
{
  // Initialize graphics
  gfxInitDefault();

  consoleInit(GFX_BOTTOM, NULL);

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

  // Initialize the render target
  target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
  C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

  // Initialize the scene
  sceneInit();

  // Main loop
  while (aptMainLoop())
  {
    hidScanInput();

    // Respond to user input
    u32 kDown = hidKeysDown();
    if (kDown & KEY_SELECT) break;
    bool is_pressed = kDown & KEY_START;
    static bool was_pressed = true;
    static unsigned int mode = 0;
    if (was_pressed != is_pressed && is_pressed) {
      mode++;
      printf("Switched to mode %d!\n", mode);
    }
    was_pressed = is_pressed;

    // Render the scene
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
      C3D_FrameDrawOn(target);
      mode = sceneRender(mode);
    C3D_FrameEnd(0);
  }

  // Deinitialize the scene
  sceneExit();

  // Deinitialize graphics
  C3D_Fini();
  gfxExit();
  return 0;
}