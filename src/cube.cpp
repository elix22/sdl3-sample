
#define VECMATH_GENERICS
#include "vecmath.h"

#include "sokol_gfx.h"
using namespace vecmath;
//  <Exec Command="&quot;$(SokolShdcPath)&quot; --input &quot;%(ShaderFiles.Identity)&quot; --module &quot;%(Filename)&quot;  --output shaders/compiled/c/%(Filename)-shader.h --slang hlsl5:glsl430:glsl300es:metal_macos:metal_ios" />
#include "cube-app-shader.h"
#include "camera-texture-shader.h"

static struct {
    float rx, ry;
    sg_pipeline pip;
    sg_bindings bind;
} state;

cube_app_vs_params_t compute_vsparams(float w , float h,float rx, float ry);
sg_swapchain sglue_swapchain(void);

void cube_init(void) {
    

    // cube vertex buffer
    float vertices[] = {
        -1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
        -1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,

        -1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0,  1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.0, 1.0, 1.0,

        1.0, -1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
        1.0,  1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
        1.0,  1.0,  1.0,    1.0, 0.5, 0.0, 1.0,
        1.0, -1.0,  1.0,    1.0, 0.5, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,

        -1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0,
        -1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0
    };
    
    sg_buffer_desc vbuf_desc{};
    vbuf_desc.data = SG_RANGE(vertices);
    vbuf_desc.label = "cube-vertices";
    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);

    // create an index buffer for the cube
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    
    sg_buffer_desc ibuf_buffer_desc = {0};
    ibuf_buffer_desc.usage.index_buffer = true;
    ibuf_buffer_desc.data = SG_RANGE(indices);
    ibuf_buffer_desc.label = "cube-indices";
    sg_buffer ibuf = sg_make_buffer(ibuf_buffer_desc);
    // create shader
    sg_shader shd = sg_make_shader(cube_shader_desc(sg_query_backend()));

    // create pipeline object
    sg_pipeline_desc pip_desc{};
    pip_desc.layout.buffers[0].stride = 28;
    pip_desc.layout.attrs[ATTR_cube_app_cube_position].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_cube_app_cube_color0].format   = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.cull_mode = SG_CULLMODE_BACK;
    pip_desc.depth.write_enabled = true;
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.label = "cube-pipeline";
    state.pip = sg_make_pipeline(&pip_desc);

    // setup resource bindings
    state.bind = sg_bindings{};
    state.bind.vertex_buffers[0] = vbuf;
    state.bind.index_buffer = ibuf;
    
}

void cube_frame(float w , float h,float t)
{
    if(w<= 0 || h <= 0)return;
    t*=60;
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const cube_app_vs_params_t vs_params = compute_vsparams(w, h,state.rx, state.ry);


    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_range vs_range = SG_RANGE(vs_params);
    sg_apply_uniforms(UB_cube_app_vs_params, &vs_range);
    sg_draw(0, 36, 1);
    sg_end_pass();
  
}



 cube_app_vs_params_t compute_vsparams(float w , float h,float rx, float ry) {
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    cube_app_vs_params_t vs_params{};
    vs_params.mvp = vm_mul(model, view_proj);
    return vs_params;
}
