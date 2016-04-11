#include "../../base.inl"

bool clear_high = false;
bool test_high = true;

bool depth_test = true;
bool depth_mask = true;
bool depth_read_allow = true;
bool depth_write_allow = true;

bool stencil_test = true;
bool stencil_mask = true;
bool stencil_read_allow = true;
bool stencil_write_allow = true;

bool color_blend = false;
Mask color_mask = 0xF;
Mask color_read_allow = 0xF;
Mask color_write_allow = 0xF;

Selection selections[] = {
  { "Clear high", boolModify, boolValue, &clear_high },
  { "Test higher", boolModify, boolValue, &test_high },
  {},
  { "Stencil test", boolModify, boolValue, &stencil_test },
  { "Stencil mask (INV 0x2A)", boolModify, boolValue, &stencil_mask },
  { "Stencil read allow", boolModify, boolValue, &stencil_read_allow },
  { "Stencil write allow", boolModify, boolValue, &stencil_write_allow },
  {},
  { "Depth test", boolModify, boolValue, &depth_test },
  { "Depth mask", boolModify, boolValue, &depth_mask },
  { "Depth read allow", boolModify, boolValue, &depth_read_allow },
  { "Depth write allow", boolModify, boolValue, &depth_write_allow },
  {},
  { "Color blend", boolModify, boolValue, &color_blend },
  { "Color mask", maskModify, maskValueRGBA, &color_mask },
  { "Color read allow", maskModify, maskValueRGBA, &color_read_allow },
  { "Color write allow", maskModify, maskValueRGBA, &color_write_allow },
};
const unsigned int selection_count = ARRAY_SIZE(selections);

static DVLB_s* vshader_dvlb;

static shaderProgram_s program;

static int uLoc_projection;
static C3D_Mtx projection;

struct {
  float x, y;
  float r, g, b;
} vertices[] = {
  {    0.0f,   80.0f, 1.0f, 0.0f, 0.0f },
  { -100.0f,  -80.0f, 0.0f, 1.0f, 0.0f },
  { +100.0f,  -80.0f, 0.0f, 0.0f, 1.0f }
};

static void* vbo_data;

void initialize(void) {

  // Load the vertex shader, create a shader program and bind it
  vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
  shaderProgramInit(&program);
  shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
  C3D_BindProgram(&program);
  
  // Get the location of the uniforms
  uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

  // Compute the projection matrix
  Mtx_OrthoTilt(&projection, -200.0, 200.0, -120.0, 120.0, 0.0, 1.0);

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

void update(void) {

  // Coordinate system has been corrected to reflect actual display orientation
  uint32_t raw_zeta = *(uint32_t*)get_depth(200, 120);
  uint32_t raw_depth = raw_zeta & 0xFFFFFF;
  float f_depth = raw_depth / (float)0xFFFFFF;
  uint8_t raw_stencil = raw_zeta >> 24;
  float f_stencil = raw_stencil / (float)0xFF;

  uint32_t raw_color = *(uint32_t*)get_color(200, 120);

  printf("Last frame stencil:       0x%02" PRIX8 " = %.2f\n", raw_stencil, f_stencil);
  printf("Last frame depth:     0x%06" PRIX32 " = %.2f\n", raw_depth, f_depth);
  float f_color = 0.0f;
  f_color += (raw_color & 0xFF) / 255.0f;
  f_color += ((raw_color >> 8) & 0xFF) / 255.0f;
  f_color += ((raw_color >> 16) & 0xFF) / 255.0f;
  f_color += (raw_color >> 24) / 255.0f;
  f_color /= 4.0f;
  printf("Last frame color:   0x%08" PRIX32 " = %.2f\n", raw_color, f_color);

  //FIXME: Configure clear for next frame
  uint32_t clear_value = 0x55555555;
  if (clear_high) { clear_value ^= 0xFFFFFFFF; }
  rb.clearColor = clear_value;
  rb.clearDepth = clear_value;

}

void draw(void) {

  // WARNING: DO **NOT** USE ANY C3D STUFF, ESPECIALLY NOTHING EFFECT RELATED!

  // FIXME: Enable alpha blend for 50% mixing with BG
  GPUCMD_AddWrite(GPUREG_BLEND_COLOR, color_blend ? 0x7F7F7F7F : 0x00000000);
  GPUCMD_AddWrite(GPUREG_BLEND_FUNC, (GPU_BLEND_ADD << 0) |
                                     (GPU_BLEND_ADD << 8) |
                                     (GPU_ONE_MINUS_CONSTANT_COLOR << 16) |
                                     (GPU_CONSTANT_COLOR << 20) |
                                     (GPU_ONE_MINUS_CONSTANT_ALPHA << 24) |
                                     (GPU_CONSTANT_ALPHA << 28));
  GPUCMD_AddWrite(GPUREG_COLOR_OPERATION, (0x0E4 << 24) | (1 << 8));
  

  uint32_t compare_mode = test_high ? GPU_GREATER : GPU_LESS;

  // Set color and depth mask + depth test
  {
    uint32_t mask_reg = (depth_mask ? 1 << 12 : 0) |
                        ((color_mask & 0xF) << 8);
    GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK, mask_reg | ((compare_mode & 7) << 4) | (!!depth_test));
  }

  // Set stencil mask + stencil test
  {
    uint32_t buffer_mask = stencil_mask ? 0x2A : 0x00; // Mask used for buffer write
    uint32_t mask = 0xFF; // Mask for comparison
    uint32_t ref = 0x80;
    GPUCMD_AddWrite(GPUREG_STENCIL_TEST, ((mask & 0xFF) << 24) | ((ref & 0xFF) << 16) | ((buffer_mask & 0xFF) << 8) | ((compare_mode & 7) << 4) | (!!stencil_test));
    //0-2	unsigned, Fail operation
    //4-6	unsigned, Z-fail operation
    //8-10	unsigned, Z-pass operation
    GPUCMD_AddWrite(GPUREG_STENCIL_OP, (GPU_STENCIL_INVERT << 8) | (GPU_STENCIL_INVERT << 4) | (GPU_STENCIL_KEEP << 0));
  }

  // Allow functions..
  GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_READ, (depth_read_allow ? 2 : 0) | (stencil_read_allow ? 1 : 0));
  GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_WRITE, (depth_write_allow ? 2 : 0) | (stencil_write_allow ? 1 : 0));
  GPUCMD_AddWrite(GPUREG_COLORBUFFER_READ, color_read_allow);
  GPUCMD_AddWrite(GPUREG_COLORBUFFER_WRITE, color_write_allow);

  // Configure depth values (= will always return 0.5)
  bool w_buffer = true;
  float depth_scale = -1.0f;
  float depth_offset = 0.0f;

#if 1
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_ENABLE, w_buffer ? 0x00000000 : 0x00000001);
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_SCALE, f32tof24(depth_scale));
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_OFFSET, f32tof24(depth_offset));
#endif

  // Draw the VBO
  C3D_DrawArrays(GPU_TRIANGLES, 0, 3);

}

#if 0
static void sceneExit(void) {
  // Free the VBO
  linearFree(vbo_data);

  // Free the shader program
  shaderProgramFree(&program);
  DVLB_Free(vshader_dvlb);
}
#endif
