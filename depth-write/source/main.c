#include "../../base.inl"

bool w_buffer = false;
float depth_scale = -1.0f;
float depth_offset = 0.0f;
float vertex_top_z = -0.5f;
float vertex_top_w = 1.0f;
float vertex_bottom_z = -0.5f;
float vertex_bottom_w = 1.0f;

Selection selections[] = {
  { "W-Buffer", boolModify, boolValue, &w_buffer },
  { "Depth Scale", floatModify, floatValue, &depth_scale },
  { "Depth Offset", floatModify, floatValue, &depth_offset },
  {},
  { "Top-Vertex Z", floatModify, floatValue, &vertex_top_z },
  { "Top-Vertex W", floatModify, floatValue, &vertex_top_w },
  {},
  { "Bottom-Vertex Z", floatModify, floatValue, &vertex_bottom_z },
  { "Bottom-Vertex W", floatModify, floatValue, &vertex_bottom_w }
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

#if 0
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
#endif
