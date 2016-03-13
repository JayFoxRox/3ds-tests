#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "vshader_shbin.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
  (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
  GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))


static DVLB_s* vshader_dvlb;

static shaderProgram_s program;

static int uLoc_projection;
static C3D_Mtx projection;

struct {
  float x, y, index;
  float r, g, b;
} __attribute__((packed)) vertices[] = {
  { 200.0f, 200.0f, 1.0f, 1.0f, 0.0f, 0.0f },
  { 100.0f,  40.0f, 2.0f, 0.0f, 1.0f, 0.0f },
  { 300.0f,  40.0f, 3.0f, 0.0f, 0.0f, 1.0f }
};

static void* vbo_data;

static u32* mad = NULL;
static u32* opdesc = NULL;
static u32 opdesc_idx;


#define ENC_MASK(x,y,z,w) (((x)?8:0) | ((y)?4:0) | ((z)?2:0) | ((w)?1:0))
#define ENC_SW(x,y,z,w) (((x) << 6) | ((y) << 4) | ((z) << 2) | (w))

#define ENC_OPDESC(mask, src1_neg, src1_sw, src2_neg, src2_sw, src3_neg, src3_sw) \
  (((src3_sw) << 23) | ((src3_neg)?1<<22:0) | ((src2_sw) << 14) | ((src2_neg)?1<<13:0) | ((src1_sw) << 5) | ((src1_neg)?1<<4:0) | (mask))

#define ENC_SRC_V(i) (0x00+i)
#define ENC_SRC_R(i) (0x10+i)
#define ENC_SRC_C(i) (0x20+i)

#define ENC_DST_O(i) (0x00+i)
#define ENC_DST_R(i) (0x10+i)

#define ENC_IDX_NONE 0
#define ENC_IDX_A(i) ((i)+1)

#define ENC_MAD(dst, src1, src2, idx, src3, opdesc_idx) \
  ((0x7 << 29) | ((dst) << 24) | ((idx) << 22) | ((src1) << 17) | ((src2) << 10) | ((src3) << 5) | (opdesc_idx))
#define ENC_MADI(dst, src1, src2, src3, idx, opdesc_idx) \
  ((0x6 << 29) | ((dst) << 24) | ((idx) << 22) | ((src1) << 17) | ((src2) << 12) | ((src3) << 5) | (opdesc_idx))

static void sceneInit(void)
{

  // Load the vertex shader, create a shader program and bind it
  vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
  shaderProgramInit(&program);
  
  // Find the placeholder MAD
  DVLE_s* dvle = &vshader_dvlb->DVLE[0];
  DVLP_s* dvlp = dvle->dvlp;
  for(unsigned int i = 0; i < dvlp->codeSize; i++) {
    u32* inst = &dvlp->codeData[i];
    printf("inst: 0x%" PRIX32 "\n", *inst);
    if (*inst == 0x84000000) {
      mad = &inst[1];
      printf("Detected mad: ");
    }
  }
  opdesc_idx = *mad & 0x1F;
  opdesc = &dvlp->opcdescData[opdesc_idx];
  printf("Opdesc %" PRId32 " used for mad\n", opdesc_idx);

  // Compute the projection matrix
  Mtx_OrthoTilt(&projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0);

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
  AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=x,y,index
  AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // v1=r,g,b

  // Configure the first fragment shading substage to just pass through the vertex color
  // See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
  C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

static int sceneRender(unsigned int mode)
{

  // a0 = c50.x
  // a1 = v0.z

  // r1 = c10
  // r2 = c20
  // r3 = c30

  C3D_FVUnifSet(GPU_VERTEX_SHADER, 11, 1.0f, 0.0f, 0.0f, 23.1f); // R
  C3D_FVUnifSet(GPU_VERTEX_SHADER, 12, 0.0f, 1.0f, 0.0f, 23.2f); // G
  C3D_FVUnifSet(GPU_VERTEX_SHADER, 13, 0.0f, 0.0f, 1.0f, 23.4f); // B

  C3D_FVUnifSet(GPU_VERTEX_SHADER, 21, 1.0f, 0.0f, 0.0f, 23.1f); // R
  C3D_FVUnifSet(GPU_VERTEX_SHADER, 22, 0.0f, 1.0f, 0.0f, 23.2f); // G
  C3D_FVUnifSet(GPU_VERTEX_SHADER, 23, 0.0f, 0.0f, 1.0f, 23.4f); // B

  C3D_FVUnifSet(GPU_VERTEX_SHADER, 31, 1.0f, 0.0f, 0.0f, 23.1f); // R
  C3D_FVUnifSet(GPU_VERTEX_SHADER, 32, 0.0f, 1.0f, 0.0f, 23.2f); // G
  C3D_FVUnifSet(GPU_VERTEX_SHADER, 33, 0.0f, 0.0f, 1.0f, 23.4f); // B

  if (mode == 0) {

    /* Direct lookup from c21-c23 using relative addressed mov */

    *mad = 0x4c334000 | opdesc_idx; // mov o1, c20[a1]
    *opdesc = ENC_OPDESC(ENC_MASK(true, true, true, false),
                         false, ENC_SW(0,1,2,3),
                         false, 0,
                         false, 0);

  } else if (mode == 1) {

    /*
     * mad o1.xyz, r1, v1, r3
     * mad o1.xyz = 1 * color + 0 = color
     */

    C3D_FVUnifSet(GPU_VERTEX_SHADER, 10, 1.0f, 1.0f, 1.0f, 1.0f); // Multiplier for V1
    C3D_FVUnifSet(GPU_VERTEX_SHADER, 30, 0.0f, 0.0f, 0.0f, 0.0f); // Addition to V1

    *mad = ENC_MAD(ENC_DST_O(1), ENC_SRC_R(1), ENC_SRC_V(1), ENC_IDX_NONE, ENC_SRC_R(3), opdesc_idx);
    *opdesc = ENC_OPDESC(ENC_MASK(true, true, true, false),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3));

  } else if (mode == 2) {

    /*
     * mad o1.xyz, r1, c20[a1], r3
     * mad o1.xyz = 1 * color + 0 = color
     */

    C3D_FVUnifSet(GPU_VERTEX_SHADER, 10, 1.0f, 1.0f, 1.0f, 1.0f); // Multiplier for C20
    C3D_FVUnifSet(GPU_VERTEX_SHADER, 30, 0.0f, 0.0f, 0.0f, 0.0f); // Addition to C20

    *mad = ENC_MAD(ENC_DST_O(1), ENC_SRC_R(1), ENC_SRC_C(20), ENC_IDX_A(1), ENC_SRC_R(3), opdesc_idx);
    *opdesc = ENC_OPDESC(ENC_MASK(true, true, true, false),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3));

  } else if (mode == 3) {

    /*
     * madi o1.xyz, r1, r2, c30[a1]
     * madi o1.xyz = 0 * 0.2 + color = color
     */

    C3D_FVUnifSet(GPU_VERTEX_SHADER, 10, 0.0f, 0.0f, 0.0f, 0.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, 20, 0.2f, 0.2f, 0.2f, 0.2f);

    *mad = ENC_MADI(ENC_DST_O(1), ENC_SRC_R(1), ENC_SRC_R(2), ENC_SRC_C(30), ENC_IDX_A(1), opdesc_idx);
    *opdesc = ENC_OPDESC(ENC_MASK(true, true, true, false),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3));

  } else if (mode == 4) {

    /*
     * madi o1.xyz, r1, v1, r3
     * madi o1.xyz = 1 * color + 0 = color
     */

    C3D_FVUnifSet(GPU_VERTEX_SHADER, 10, 1.0f, 1.0f, 1.0f, 1.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, 30, 0.0f, 0.0f, 0.0f, 0.0f);

    *mad = ENC_MADI(ENC_DST_O(1), ENC_SRC_R(1), ENC_SRC_V(1), ENC_SRC_R(3), ENC_IDX_NONE, opdesc_idx);
    *opdesc = ENC_OPDESC(ENC_MASK(true, true, true, false),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3));

  } else if (mode == 5) {

    /*
     * madi o1.xyz, v1, r2, r3
     * madi o1.xyz = color * 1 + 0 = color
     */

    C3D_FVUnifSet(GPU_VERTEX_SHADER, 20, 1.0f, 1.0f, 1.0f, 1.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, 30, 0.0f, 0.0f, 0.0f, 0.0f);

    *mad = ENC_MADI(ENC_DST_O(1), ENC_SRC_V(1), ENC_SRC_R(2), ENC_SRC_R(3), ENC_IDX_NONE, opdesc_idx);
    *opdesc = ENC_OPDESC(ENC_MASK(true, true, true, false),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3));

  } else if (mode == 6) {

    /*
     * madi o1.xyz, r1, r2, v1
     * madi o1.xyz = 0.2 * 0 + color = color
     */

    C3D_FVUnifSet(GPU_VERTEX_SHADER, 10, 0.2f, 0.2f, 0.2f, 0.2f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, 20, 0.0f, 0.0f, 0.0f, 0.0f);

    *mad = ENC_MADI(ENC_DST_O(1), ENC_SRC_R(1), ENC_SRC_R(2), ENC_SRC_V(1), ENC_IDX_NONE, opdesc_idx);
    *opdesc = ENC_OPDESC(ENC_MASK(true, true, true, false),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3),
                         false, ENC_SW(0,1,2,3));

  } else {
    /* Reached the end.. */
    printf("Mode %d not found!\nUsing mode 0\n", mode);
    return sceneRender(0);
  }

  shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);

  // Update the uniforms
  uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

  // Upload the uniforms which were previously set
  C3D_UpdateUniforms(GPU_VERTEX_SHADER);

  // This will also upload the constants defined in the shader code
  shaderProgramUse(&program);

  // Draw the VBO
  C3D_DrawArrays(GPU_TRIANGLES, 0, 3);

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
  C3D_RenderTarget* target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
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
