#include "../../base.inl"

const char* blend_names[] = {
  "ZERO",
  "ONE",
  "SRC_COLOR",
  "ONE_MINUS_SRC_COLOR",
  "DST_COLOR",
  "ONE_MINUS_DST_COLOR",
  "SRC_ALPHA",
  "ONE_MINUS_SRC_ALPHA", 
  "DST_ALPHA",
  "ONE_MINUS_DST_ALPHA",
  "CONSTANT_COLOR",
  "ONE_MINUS_CONST_COL.",
  "CONSTANT_ALPHA",
  "ONE_MINUS_CONST_ALP.",
  "SRC_ALPHA_SATURATE",
  "unknown15"
};

ListSelector color_src_blend = { 0, ARRAY_SIZE(blend_names), blend_names };
ListSelector color_dst_blend = { 0, ARRAY_SIZE(blend_names), blend_names };
ListSelector alpha_src_blend = { 0, ARRAY_SIZE(blend_names), blend_names };
ListSelector alpha_dst_blend = { 0, ARRAY_SIZE(blend_names), blend_names };

Selection selections[] = {
  { "RGB Src", listSelectorModify, listSelectorValue, &color_src_blend },
  { "RGB Dst", listSelectorModify, listSelectorValue, &color_dst_blend },
  { " A  Src", listSelectorModify, listSelectorValue, &alpha_src_blend },
  { " A  Dst", listSelectorModify, listSelectorValue, &alpha_dst_blend },
};
const unsigned int selection_count = ARRAY_SIZE(selections);

static DVLB_s* vshader_dvlb;

static shaderProgram_s program;

static int uLoc_projection;
static C3D_Mtx projection;

struct {
  float x, y, z, w;
  float r, g, b;
} vertices[] = {
  {    0.0f,   80.0f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f },
  { -100.0f,  -80.0f, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f },
  { +100.0f,  -80.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f }
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
  AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 4); // v0=x,y
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
  uint32_t raw_color = *(uint32_t*)get_color(200, 120);

  printf("Last frame color:   0x%08" PRIX32 "\n", raw_color);

  // Configure clear for next frame
  uint32_t clear_value = 0x55555555;
  rb.clearColor = clear_value;
  rb.clearDepth = clear_value;

}

void draw(void) {

  // WARNING: DO **NOT** USE ANY C3D STUFF, ESPECIALLY NOTHING EFFECT RELATED!

  GPUCMD_AddWrite(GPUREG_BLEND_COLOR, 0x33333333);
  GPUCMD_AddWrite(GPUREG_BLEND_FUNC, (GPU_BLEND_ADD << 0) |
                                     (GPU_BLEND_ADD << 8) |
                                     (color_src_blend.index << 16) |
                                     (color_dst_blend.index << 20) |
                                     (alpha_src_blend.index << 24) |
                                     (alpha_dst_blend.index << 28));
  GPUCMD_AddWrite(GPUREG_COLOR_OPERATION, (0x0E4 << 24) | (1 << 8));
  

  // Draw the VBO
  C3D_DrawArrays(GPU_TRIANGLES, 0, 3);

}
