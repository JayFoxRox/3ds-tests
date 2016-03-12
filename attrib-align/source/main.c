#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
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

static void* vbo_data;
size_t vbo_size = 1 * 1024; // 1 KiB should be plenty..

static void sceneInit(void)
{
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	// Get the location of the uniforms
	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

	// Compute the projection matrix
	Mtx_OrthoTilt(&projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0);

	// Create the VBO (vertex buffer object)
	vbo_data = linearAlloc(vbo_size);

	// Configure the first fragment shading substage to just pass through the vertex color
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}


void draw(size_t stride, unsigned int attrib_count, unsigned int permutation, int pad_type, int pad_count, int type) {

    // Configure buffers
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, vbo_data, stride, attrib_count, permutation);

	  // Configure attributes for use with the vertex shader
	  C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	  AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 2); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, pad_type, pad_count); // v1=pad
    AttrInfo_AddLoader(attrInfo, 2, type, 3); // v2=color
//	  AttrInfo_AddFixed(attrInfo, 1); // v1=color
	// Set the fixed attribute (color) to solid white
//	C3D_FixedAttribSet(1, 1.0, 1.0, 1.0, 1.0);

	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

	// Draw the VBO
	C3D_DrawArrays(GPU_TRIANGLES, 0, 3);
}

static int sceneRender(unsigned int mode)
{

#define DRAW(stride, attrib_count, permutation, pad_gpu_type, pad_count, pad_skip, gpu_type, type, one, zero) \
  { \
    float x[] = {200.0f, 100.0f, 300.0f}; \
    float y[] = {200.0f,  40.0f,  40.0f}; \
    memset(vbo_data, 0x00, vbo_size); \
    for(unsigned int i = 0; i < 3; i++) { \
      uintptr_t base_buffer = (uintptr_t)vbo_data + (stride) * i; \
      uintptr_t position_buffer = base_buffer; \
      ((float*)position_buffer)[0] = x[i]; \
      ((float*)position_buffer)[1] = y[i]; \
      base_buffer += 8; \
      base_buffer += (pad_skip); \
      uintptr_t color_buffer = base_buffer; \
      for(unsigned int j = 0; j < 3; j++) { \
        ((type*)color_buffer)[j] = (j == i) ? (one) : (zero); \
      } \
      base_buffer += sizeof(type) * 3; \
    } \
    draw(stride, attrib_count, permutation, pad_gpu_type, pad_count, gpu_type); \
  }

  if (mode == 0) {
    DRAW(64,
         3, 0x210,
         GPU_UNSIGNED_BYTE, 4, 4,
         GPU_FLOAT, float, 1.0f, 0.0f);
  } else if (mode == 1) {
    DRAW(64,
         3, 0x210,
         GPU_UNSIGNED_BYTE, 3, 4,
         GPU_FLOAT, float, 1.0f, 0.0f);
  } else if (mode == 2) {
    DRAW(64,
         4, 0x21C0, // a0, 4 byte pad, a1, a2
         GPU_UNSIGNED_BYTE, 4, 8,
         GPU_FLOAT, float, 1.0f, 0.0f);  
  } else if (mode == 3) {
    DRAW(64,
         4, 0x2C10, // a0, a1, 4 byte pad, a2

         //unaligned:
         // 0-7:         a0
         // 8,9,10:      a1 pad
         // 11,12,13,14: 4 byte pad
         // 15+: a2.x
         // = 7 byte padding

         //aligned:
         // 0-7:         a0
         // 8,9,10:      a1 pad
         // 11,12,13,14: 4 byte pad
         // *align*
         // 16+: a2.x
         // = 8 byte padding

         GPU_UNSIGNED_BYTE, 3, 8,
         GPU_FLOAT, float, 1.0f, 0.0f);      
  } else if (mode == 4) {
    DRAW(64,
         5, 0x21C10, // a0, a1, 4 byte pad, a1, a2

         //unaligned:
         // 0-7:         a0
         // 8,9,10:      a1 pad
         // 11,12,13,14: 4 byte pad
         // 15,16,17:    a1 pad
         // 18+: a2.x
         // = 10 byte padding

         //aligned:
         // 0-7:         a0
         // 8,9,10:      a1 pad
         // 11,12,13,14: 4 byte pad
         // 15,16,17:    a1 pad
         // *align*
         // 20+: a2.x
         // = 12 byte padding

         GPU_UNSIGNED_BYTE, 3, 12,
         GPU_FLOAT, float, 1.0f, 0.0f);
  } else if (mode == 5) {
    DRAW(64,
         4, 0x2C10, // a0, a1, 4 byte pad, a1, a2

         //unaligned explicit-pad:
         // 0-7:         a0
         // 8,9,10:      a1 pad
         // 11,12,13,14: 4 byte pad
         // 15+: a2.x
         // = 7 byte padding

         //aligned explicit-pad:
         // 0-7:         a0
         // 8,9,10:      a1 pad
         // *align*
         // 12,13,14,15: 4 byte pad
         // 16+: a2.x
         // = 8 byte padding

         GPU_UNSIGNED_BYTE, 3, 7,
         GPU_UNSIGNED_BYTE, uint8_t, 1, 0);
  } else if (mode == 6) {
    DRAW(62, // !!! Still expected to work because float pos will realign to multiple of 4
         3, 0x210,
         GPU_UNSIGNED_BYTE, 3, 4,
         GPU_FLOAT, float, 1.0f, 0.0f);
  } else if (mode == 7) {
    DRAW(63, // !!! Still expected to work because float pos will realign to multiple of 4
         3, 0x210,
         GPU_UNSIGNED_BYTE, 3, 4,
         GPU_FLOAT, float, 1.0f, 0.0f);
  } else {
    /* Reached the end.. */
    printf("Mode %d not found!\nUsing mode 0\n", mode);
    return sceneRender(0);
  }

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
