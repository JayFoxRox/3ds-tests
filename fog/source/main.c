#include "../../base.inl"

bool w_buffer = false;
float fog_x = 0.0f;

Selection selections[] = {
  { "W-Buffer", boolModify, boolValue, &w_buffer },
  { "Fog enabled", boolModify, boolValue, &depth_offset },
  { "Fog curve", boolModify, boolValue, &depth_offset, "Sin(x)", "x", "x*x", "if(x).+0", "0.+if(x)", "1.-if(x)", "1.+if(x)", "0.-if(x)" },
  { "Fog x", floatModify, floatValue, &depth_value },
  { "Fog R", floatModify, floatValue, &depth_value },
  { "Fog G", floatModify, floatValue, &depth_value },
  { "Fog B", floatModify, floatValue, &depth_value },
  { "Fog A", floatModify, floatValue, &depth_value },
  { "Object alpha", floatModify, floatValue, &depth_value },
  { "Mode", floatModify, floatValue, &mode, "Logic", "Blend" },
};
const unsigned int selection_count = ARRAY_SIZE(selections);

static DVLB_s* vshader_dvlb;

static shaderProgram_s program;

static int uLoc_vertex;
static int uLoc_projection;
static C3D_Mtx projection;

struct {
  float x, y, weight; // weight = 0.0:bottom / 1.0:top
  float r, g, b;
} vertices[] = {
  {    0.0f,   80.0f, 1.0f, 1.0f, 0.0f, 0.0f },
  { -100.0f,  -80.0f, 0.0f, 0.0f, 1.0f, 0.0f },
  { +100.0f,  -80.0f, 0.0f, 0.0f, 0.0f, 1.0f }
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
  AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=x,y,weight
  AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // v1=r,g,b

  // Configure the first fragment shading substage to just pass through the vertex color
  // See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
  C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

}

static float print_depth(const char* title, float z, float w) {
  float depth = z * depth_scale / w + depth_offset;
  if (w_buffer) {
    depth *= w;
    printf("%s W: z*scale+offset*w:  %+.3f\n", title, depth);
  } else {
    printf("%s Z: z/w*scale+offset:  %+.3f\n", title, depth);
  }
  return depth;
}

void update(void) {

  // Coordinate system has been corrected to reflect actual display orientation
  u32 raw_buffer = *(u32*)get_depth(200, 120);
  uint32_t raw_result = raw_buffer & 0xFFFFFF;
  float f_result = raw_result / (float)0xFFFFFF;

  float depth_top = print_depth("Top   ", vertex_top_z, vertex_top_w);
  float depth_bottom = print_depth("Bottom", vertex_bottom_z, vertex_bottom_w);

  float vertex_z = (vertex_bottom_z + vertex_top_z) * 0.5f;
  float vertex_w = (vertex_bottom_w + vertex_top_w) * 0.5f;

  float depth = print_depth("Center", vertex_z, vertex_w);
  printf("Center Depth (Vertex):       %+.3f\n", (depth_top + depth_bottom)*0.5f);

  bool z_clip = (vertex_z < -vertex_w) || (vertex_z > 0.0f);
  bool w_clip = (vertex_w <= 0.0f);

  printf("\n");
  printf("Center Z Clip:                  %3s\n", z_clip ? "Yes" : "No");
  printf("Center W Clip:                  %3s\n", w_clip ? "Yes" : "No");

  // Clamp depth value
  if (depth < 0.0f) { depth = 0.0f; }
  if (depth > 1.0f) { depth = 1.0f; }

  bool shown = !z_clip && !w_clip;

  printf("\n");
  printf("Guessing triangle shown:        %3s\n", shown ? "Yes" : "No");
  printf("Guessed depth:               %+.3f\n", shown ? depth : 0.0f );
  printf("\n");
  printf("Last frame depth: 0x%06" PRIX32 " = %+.3f\n", raw_result, f_result);

}

void draw(void) {

  // WARNING: DO **NOT** USE ANY C3D STUFF, ESPECIALLY NOTHING EFFECT RELATED!

  // Disable or enable depth test..
  bool depth_test = false;
  u32 compare_mode = GPU_ALWAYS;
  GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK, (!!depth_test) | ((compare_mode & 7) << 4) | (GPU_WRITE_ALL << 8));

#if 1
  // Configure depth mapping
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_ENABLE, w_buffer ? 0x00000000 : 0x00000001);
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_SCALE, f32tof24(depth_scale));
  GPUCMD_AddWrite(GPUREG_DEPTHMAP_OFFSET, f32tof24(depth_offset));
#endif

  C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_vertex, vertex_bottom_z, vertex_bottom_w, vertex_top_z, vertex_top_w); // x = depth

  // Draw the VBO
  C3D_DrawArrays(GPU_TRIANGLES, 0, 3);

}
