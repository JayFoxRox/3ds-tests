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


// Add some compatibility macros

#ifndef CONSOLE_RED
#define CONSOLE_RED ""
#endif

#ifndef CONSOLE_RESET
#define CONSOLE_RESET ""
#endif

// 32kB buffer..
#define BUFFER_SIZE 32*1024

static char shown_buffer[BUFFER_SIZE]; // Currently displayed buffer (to avoid weird effects)
static char out_buffer[BUFFER_SIZE]; // Buffer for printf
#define printf(...) sprintf(&out_buffer[strlen(out_buffer)], __VA_ARGS__);

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

typedef struct {
  unsigned int index;
  unsigned int item_count;
  const char** items;
} ListSelector;
char* listSelectorValue(const void* data) {
  static char buffer[128];
  const ListSelector* value = (const ListSelector*)data;
  snprintf(buffer, sizeof(buffer), "%s=%d", value->items[value->index], value->index);
  return buffer;
}
void listSelectorModify(void* data, int step) {
  ListSelector* value = (ListSelector*)data;
  value->index = (unsigned int)(value->index + step) % value->item_count;
  return;
}

typedef uint8_t Mask;

static char* _maskValue(const void* data, char* pattern) {
  static char buffer[32];
  Mask value = *(const Mask*)data;
  value &= 0xF;
  bool x = value & (1 << 0);
  bool y = value & (1 << 1);
  bool z = value & (1 << 2);
  bool w = value & (1 << 3);
  snprintf(buffer, sizeof(buffer), "0x%X (%c%c%c%c)",
           value,
           x ? pattern[0] : ' ',
           y ? pattern[1] : ' ',
           z ? pattern[2] : ' ',
           w ? pattern[3] : ' ');
  return buffer;
}

char* maskValueXYZW(const void* data) { return _maskValue(data, "XYZW"); }
char* maskValueRGBA(const void* data) { return _maskValue(data, "RGBA"); }

void maskModify(void* data, int step) {
  Mask* value = (Mask*)data;
  *value = (*value + step) & 0xF;
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


static C3D_RenderBuf rb;

static void sceneExit(void) {
#if 0
  // Free the VBO
  linearFree(vbo_data);

  // Free the shader program
  shaderProgramFree(&program);
  DVLB_Free(vshader_dvlb);
#endif
}

void* get_depth(unsigned int x, unsigned int y) {
  // Get depth in center of screen (240x400, 32bpp FB)
  uint8_t* buffer = (u8*)rb.depthBuf.data;
  int pixel_offset = GetPixelOffset(y, x, 240, 400, 4);
  return &buffer[pixel_offset];
}

void* get_color(unsigned int x, unsigned int y) {
  // Get depth in center of screen (240x400, 32bpp FB)
  uint8_t* buffer = (u8*)rb.colorBuf.data;
  int pixel_offset = GetPixelOffset(y, x, 240, 400, 4);
  return &buffer[pixel_offset];
}

void clear(u32 mask, u32 color, u32 zeta) {
#if 0
    C3D_Flush();
    C3D_RenderBufTransfer(&rb, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), DISPLAY_TRANSFER_FLAGS);
    C3D_RenderBufClear(&rb);
#endif
}

int main() {
  // Initialize graphics
  gfxInitDefault();

  consoleInit(GFX_BOTTOM, NULL);

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

  C3D_RenderBufInit(&rb, 240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  rb.clearColor = CLEAR_COLOR;
  rb.clearDepth = 0x000000;
  C3D_RenderBufClear(&rb);
  C3D_RenderBufBind(&rb);

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
      int scroll = 0;
      if (kDown & KEY_UP) { scroll--; }
      if (kDown & KEY_DOWN) { scroll++; }
      if (scroll != 0) {
        do {
          selection_index += selection_count * 256 + scroll; // Make sure we can modify downwards
          selection_index %= selection_count;
        } while(selections[selection_index].name == NULL);
      }
    }

    // Clear console and its buffer
    sprintf(out_buffer, "\x1b[2J" CONSOLE_RESET "\nBuilt on %s / %s\n\n\n", __DATE__, __TIME__);

    // Next we run the update handler which might fiddle with the values some more
    update();

    // Now start clearing the buffer
    C3D_RenderBufClear(&rb);
 
    // Draw the menu
    selection_index %= selection_count;
    printf("\n\nStep size: x%d\n\n", step_size);
    for (unsigned int i = 0; i < selection_count; i++) {
        Selection* selection = &selections[i];
        if (selection->name == NULL) {
          printf("\n");
          continue;
        }
        printf("%s%s: %*s%s\n",
               i == selection_index ? CONSOLE_RED " > " : CONSOLE_RESET "   ",
               selection->name,
               32 - strlen(selection->name), selection->valueCb(selection->data),
               i == selection_index ? " <" : "  ");
    }

    // Now we wait for the clear to complete and run the draw function
    C3D_VideoSync();

    draw();

    // Supply new data for bottom screen
    if (memcmp(shown_buffer, out_buffer, sizeof(shown_buffer))) {
        fprintf(stdout, out_buffer);
        memcpy(shown_buffer, out_buffer, sizeof(shown_buffer));
    }

    // Submit GPU commands and transfer bottom screen
    C3D_Flush();
    C3D_RenderBufTransfer(&rb, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), DISPLAY_TRANSFER_FLAGS);

  }

  // Deinitialize the scene
  sceneExit();

  // Deinitialize graphics
  C3D_Fini();
  gfxExit();
  return 0;
}
