//FIXME: Needs w-buffering (0x6D activates it)

#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include "vshader_shbin.h"

#include "../../morton.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
  (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
  GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct {
  const char* name;
  void(*modifyCb)(void*, int);
  char*(*valueCb)(const void*);
  void* data;
} Selection;

extern Selection selections[];
extern const unsigned int selection_count;

void draw(void);
void initialize(void);
void update(void);

char* boolValue(const void* data) {
  const bool* value = (const bool*)data;
  return *value ? "Yes" : "No";
}
void boolModify(void* data, int step) {
  bool* value = (bool*)data;
  *value = step > 0;
  return;
}

char* floatValue(const void* data) {
  static char buffer[32];
  const float* value = (const float*)data;
  snprintf(buffer, sizeof(buffer), "%+.2f", *value);
  return buffer;
}
void floatModify(void* data, int step) {
  float* value = (float*)data;
  float f = *value;
  f *= 100.0f;
  f = roundf(f) + step;
  f /= 100.0f;
  *value = f;
  return;
}


static C3D_RenderTarget* target;

static void sceneExit(void)
{
#if 0
  // Free the VBO
  linearFree(vbo_data);

  // Free the shader program
  shaderProgramFree(&program);
  DVLB_Free(vshader_dvlb);
#endif
}

void* get_depth(unsigned int x, unsigned int y) {

  C3D_RenderBuf* rb = &target->renderBuf;

  // Get depth in center of screen (240x400, 32bpp FB)
  uint8_t* buffer = (u8*)rb->depthBuf.data;
  int pixel_offset = GetPixelOffset(y, x, 240, 400, 4);
  return &buffer[pixel_offset];
}

void clear(u32 mask, u32 color, u32 zeta) {
  C3D_FrameEnd(0);
  target->drawOk = true; // Some hack..
  C3D_RenderTargetSetClear(target, mask, color, zeta);
  C3D_FrameBegin(C3D_FRAME_NONBLOCK);
  C3D_FrameDrawOn(target);
}

int main() {
  // Initialize graphics
  gfxInitDefault();

  consoleInit(GFX_BOTTOM, NULL);

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

  // Initialize the render target
  target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  C3D_RenderTargetSetClear(target, 0, 0, 0);
  C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

  // Initialize the scene
//  sceneInit();

  initialize();

  // Main loop
  while (aptMainLoop()) {
    hidScanInput();

    // Respond to user input
    u32 kHeld = hidKeysHeld();
    u32 kDown = hidKeysDown();
    if (kDown & KEY_SELECT) {
      break;
    }

    static unsigned int selection_index = 0;

    Selection* selection = &selections[selection_index];
    selection_index += selection_count * 256; // Make sure we can modify downwards

    int step_size = 1;
    if (kHeld & KEY_Y) { step_size *= 2; }
    if (kHeld & KEY_X) { step_size *= 5; }
    if (kHeld & KEY_B) { step_size *= 20; }
    if (kHeld & KEY_A) { step_size *= 50; }

    int step = 0;
    if (kDown & KEY_LEFT) { step--; }
    if (kDown & KEY_RIGHT) { step++; }

    if (step != 0) {
      selection->modifyCb(selection->data, step * step_size);
    } else {
      if (kDown & KEY_UP) { selection_index--; }
      if (kDown & KEY_DOWN) { selection_index++; }
    }

    // Next we run the update handler which might fiddle with the values some more
    update();

    // Now we run the draw function    
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
      C3D_FrameDrawOn(target);
 
    // And finally we draw the menu with the values we are left with
    selection_index %= selection_count;

    consoleClear();
    printf("Step size: x%d\n\n", step_size);
    for (unsigned int i = 0; i < selection_count; i++) {
        Selection* selection = &selections[i];
        printf("%s%s: %s\n",
#ifdef CONSOLE_RED
               i == selection_index ? CONSOLE_RED "*" : CONSOLE_RESET " ",
#else
               i == selection_index ? " -> " : "    ",
#endif
               selection->name, selection->valueCb(selection->data));
    }
    printf("\n");

    draw();
    C3D_FrameEnd(0);

  }

  // Deinitialize the scene
  sceneExit();

  // Deinitialize graphics
  C3D_Fini();
  gfxExit();
  return 0;
}
