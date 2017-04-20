#include "../../base.inl"

const char* combiner_list[] = {
  "Replace",
  "Modulate",
  "Add",
  "Add signed",
  "Interpolate",
  "Subtract",
  "Dot3 RGB",
  "Dot3 RGBA",
  "Multiply then add",
  "Add then multiply",
};

ListSelector rgb_combiner = { 0, ARRAY_SIZE(combiner_list), combiner_list };
ListSelector a_combiner = { 0, ARRAY_SIZE(combiner_list), combiner_list };

Selection selections[] = {
  { "RGB Combiner", listSelectorModify, listSelectorValue, &rgb_combiner },
  { " A  Combiner", listSelectorModify, listSelectorValue, &a_combiner },
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
  Mtx_OrthoTilt(&projection, -200.0, 200.0, -120.0, 120.0, 0.0, 1.0, true);

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

}

// random colors
u32 color_a = 0xEE4D5CBB, color_b = 0x09423D8C, color_c = 0x438619F7;

inline u32 swap32(u32 _data) {
    __asm__("rev %0, %1\n" : "=l"(_data) : "l"(_data));
    return _data;
}

void print_color(u32 color) {
    printf("r=%02lX, g=%02lX, b=%02lX, a=%02lX\n", 
        color & 0xFF,
        (color >> 8) & 0xFF,
        (color >> 16) & 0xFF,
        color >> 24);
}

void update(void) { 

  // Coordinate system has been corrected to reflect actual display orientation
  uint32_t raw_color = *(uint32_t*)get_color(200, 120);
  
  printf("Color A: ");
  print_color(color_a);
  printf("Color B: ");
  print_color(color_b);
  printf("Color C: ");
  print_color(color_c);
  printf("Output:  ");
  print_color(swap32(raw_color));

  // Configure clear for next frame
  uint32_t clear_value = 0x00000000;
  rb.clearColor = clear_value;
  rb.clearDepth = clear_value;

}

void draw(void) {

  // WARNING: DO **NOT** USE ANY C3D STUFF, ESPECIALLY NOTHING EFFECT RELATED!

  
  GPUCMD_AddWrite(GPUREG_COLOR_OPERATION, 0x00E40000); // logic op
  GPUCMD_AddWrite(GPUREG_LOGIC_OP, 3); // copy
  
  GPUCMD_AddMaskedWrite(GPUREG_TEXENV_UPDATE_BUFFER, 2, 0x00001100); // TEXENV0 updates buffer
  
  GPUCMD_AddWrite(GPUREG_TEXENV0_SOURCE, 0x000E000E); // Constant, *, *
  GPUCMD_AddWrite(GPUREG_TEXENV0_COLOR, color_a);
  GPUCMD_AddWrite(GPUREG_TEXENV0_OPERAND, 0x00000000); // Source
  GPUCMD_AddWrite(GPUREG_TEXENV0_COMBINER, 0x00000000); // Replace
  
  GPUCMD_AddWrite(GPUREG_TEXENV1_SOURCE, 0x000E000E); // Constant, *, *
  GPUCMD_AddWrite(GPUREG_TEXENV1_COLOR, color_b);
  GPUCMD_AddWrite(GPUREG_TEXENV1_OPERAND, 0x00000000); // Source
  GPUCMD_AddWrite(GPUREG_TEXENV1_COMBINER, 0x00000000); // Replace
  
  
  GPUCMD_AddWrite(GPUREG_TEXENV2_SOURCE, 0x0EFD0EFD); // PreviousBuffer(color_a), Previous(color_b), Constant(color_c)
  GPUCMD_AddWrite(GPUREG_TEXENV2_COLOR, color_c);
  GPUCMD_AddWrite(GPUREG_TEXENV2_OPERAND, 0x00000000); // Source
  GPUCMD_AddWrite(GPUREG_TEXENV2_COMBINER,rgb_combiner.index | (a_combiner.index << 16)); // The combiner we are testing
  

  // Draw the VBO
  C3D_DrawArrays(GPU_TRIANGLES, 0, 3);

}
