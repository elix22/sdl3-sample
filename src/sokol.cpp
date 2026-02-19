// CODE HERE
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#define SOKOL_IMGUI_NO_SOKOL_APP

#include <stdio.h>
#include <string.h>



//#error "Please define one of SOKOL_GLCORE, SOKOL_GLES3, SOKOL_D3D11, SOKOL_METAL, SOKOL_WGPU or SOKOL_DUMMY_BACKEND!"
#if defined(__APPLE__)
    #define SOKOL_METAL
#elif defined(__ANDROID__)
    #define SOKOL_GLES3
#elif defined(__EMSCRIPTEN__)
    #define SOKOL_GLES3
#elif defined(__WIN32__)
    #define SOKOL_D3D11
#else
    #define SOKOL_GLCORE
#endif


#include "imgui.h"
#define SOKOL_GFX_IMPL
#include "sokol_gfx.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"

#define VECMATH_GENERICS
#include "vecmath.h"

#define SOKOL_LOG_IMPL
#include "sokol_log.h"

#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>
#if defined(__APPLE__)
#include <SDL3/SDL_metal.h>
#endif

using namespace vecmath;
//  <Exec Command="&quot;$(SokolShdcPath)&quot; --input &quot;%(ShaderFiles.Identity)&quot; --module &quot;%(Filename)&quot;  --output shaders/compiled/c/%(Filename)-shader.h --slang hlsl5:glsl430:glsl300es:metal_macos:metal_ios" />
#include "cube-app-shader.h"
#include "camera-texture-shader.h"

// Platform-specific Metal state (Apple only)
#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>

static SDL_MetalView  s_metal_view          = nullptr;
static void*          s_metal_device        = nullptr;  // id<MTLDevice>
static void*          s_depth_stencil_tex   = nullptr;  // id<MTLTexture>
static int            s_ds_tex_width        = 0;
static int            s_ds_tex_height       = 0;

static void* sdl_get_metal_device() {
    if (s_metal_device) return s_metal_device;
    // MTLCreateSystemDefaultDevice()
    SDL_SharedObject* metalLib = SDL_LoadObject("/System/Library/Frameworks/Metal.framework/Metal");
    if (metalLib) {
        typedef void* (*MTLCreateFn)();
        MTLCreateFn fn = (MTLCreateFn)(void*)SDL_LoadFunction(metalLib, "MTLCreateSystemDefaultDevice");
        if (fn) s_metal_device = fn();
    }
    return s_metal_device;
}

static void sdl_ensure_metal_view(SDL_Window* window) {
    if (!s_metal_view) {
        s_metal_view = SDL_Metal_CreateView(window);
    }
}

// Creates (or re-creates on resize) the Metal depth-stencil texture.
static void* sdl_get_depth_stencil_texture(int w, int h) {
    if (s_depth_stencil_tex && s_ds_tex_width == w && s_ds_tex_height == h)
        return s_depth_stencil_tex;

    // Release the old texture if size changed
    if (s_depth_stencil_tex) {
        ((void(*)(void*, SEL))objc_msgSend)(s_depth_stencil_tex, sel_registerName("release"));
        s_depth_stencil_tex = nullptr;
    }

    void* device = sdl_get_metal_device();
    if (!device || w <= 0 || h <= 0) return nullptr;

    // Build MTLTextureDescriptor
    Class DescClass = objc_getClass("MTLTextureDescriptor");
    void* desc = ((void*(*)(id, SEL))objc_msgSend)((id)DescClass, sel_registerName("alloc"));
    desc       = ((void*(*)(void*, SEL))objc_msgSend)(desc, sel_registerName("init"));

    // MTLPixelFormatDepth32Float_Stencil8 = 260
    ((void(*)(void*, SEL, unsigned long))objc_msgSend)(desc, sel_registerName("setPixelFormat:"), 260UL);
    // MTLTextureType2D = 2
    ((void(*)(void*, SEL, unsigned long))objc_msgSend)(desc, sel_registerName("setTextureType:"), 2UL);
    ((void(*)(void*, SEL, unsigned long))objc_msgSend)(desc, sel_registerName("setWidth:"),  (unsigned long)w);
    ((void(*)(void*, SEL, unsigned long))objc_msgSend)(desc, sel_registerName("setHeight:"), (unsigned long)h);
    // MTLStorageModePrivate = 2
    ((void(*)(void*, SEL, unsigned long))objc_msgSend)(desc, sel_registerName("setStorageMode:"), 2UL);
    // MTLTextureUsageRenderTarget = 4
    ((void(*)(void*, SEL, unsigned long))objc_msgSend)(desc, sel_registerName("setUsage:"), 4UL);

    s_depth_stencil_tex = ((void*(*)(void*, SEL, void*))objc_msgSend)(device, sel_registerName("newTextureWithDescriptor:"), desc);
    ((void(*)(void*, SEL))objc_msgSend)(desc, sel_registerName("release"));

    s_ds_tex_width  = w;
    s_ds_tex_height = h;
    return s_depth_stencil_tex;
}

static void* sdl_get_current_drawable(SDL_Window* window) {
    sdl_ensure_metal_view(window);
    void* layer = SDL_Metal_GetLayer(s_metal_view);
    if (!layer) return nullptr;
    // Set device on layer: [layer setDevice: device]
    void* device = sdl_get_metal_device();
    if (device) {
        SEL setDevice = sel_registerName("setDevice:");
        ((void(*)(void*, SEL, void*))objc_msgSend)(layer, setDevice, device);
    }
    // [layer nextDrawable]
    SEL nextDrawable = sel_registerName("nextDrawable");
    return ((void*(*)(void*, SEL))objc_msgSend)(layer, nextDrawable);
}
#endif // __APPLE__

// The SDL window used for environment/swapchain queries — set this before calling cube_init()
static SDL_Window* s_sdl_window = nullptr;

void sokol_set_window(SDL_Window* window) {
    s_sdl_window = window;
}

sg_environment sglue_environment(void)
{
    sg_environment env;
    memset(&env, 0, sizeof(env));

#if defined(__APPLE__)
    env.defaults.color_format = SG_PIXELFORMAT_BGRA8;
    env.defaults.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    env.defaults.sample_count = 1;
    env.metal.device          = sdl_get_metal_device();
#else
    // OpenGL ES (Android, Emscripten) or OpenGL Core (Linux/Windows)
    env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
    env.defaults.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    env.defaults.sample_count = 1;
#endif
    return env;
}

sg_swapchain sglue_swapchain(void) {
    sg_swapchain swapchain = {0};
    // memset(&swapchain, 0, sizeof(swapchain));

    int w = 0, h = 0;
    if (s_sdl_window) SDL_GetWindowSizeInPixels(s_sdl_window, &w, &h);
    swapchain.width        = w;
    swapchain.height       = h;
    swapchain.sample_count = 1;

#if defined(__APPLE__)
    swapchain.color_format = SG_PIXELFORMAT_BGRA8;
    swapchain.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    swapchain.metal.current_drawable      = sdl_get_current_drawable(s_sdl_window);
    swapchain.metal.depth_stencil_texture = sdl_get_depth_stencil_texture(w, h);
    swapchain.metal.msaa_color_texture    = nullptr;
#else
    swapchain.color_format     = SG_PIXELFORMAT_RGBA8;
    swapchain.depth_format     = SG_PIXELFORMAT_DEPTH_STENCIL;
    swapchain.gl.framebuffer   = 0;  // default framebuffer
#endif
    return swapchain;
}




void sokol_init(void)
{
    
    sg_desc desc = {0};
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);

}

void sokol_begin_pass(void)
{
    sg_pass pass = {0};
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = { 0.25f, 0.5f, 0.75f, 1.0f };
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
}

void sokol_commit(void)
{
    sg_commit();
}

void sokol_shutdown(void)
{
    sg_shutdown();
}

// ---------------------------------------------------------------------------
// NV12 camera background quad
// ---------------------------------------------------------------------------

static struct {
    sg_image    y_img;
    sg_image    uv_img;
    sg_view y_view;
    sg_view uv_view;
    sg_sampler  smp;
    sg_pipeline pip;
    sg_bindings bind;
    int         width;
    int         height;
    float       rotation;  // degrees from SDL_PROP_SURFACE_ROTATION_FLOAT
} s_cam = {};

void nv12_camera_update(SDL_Surface* surface) {
    if (!surface) return;

    int w = surface->w;
    int h = surface->h;
    float rotation = SDL_GetFloatProperty(SDL_GetSurfaceProperties(surface), SDL_PROP_SURFACE_ROTATION_FLOAT, 0.0f);
    const uint8_t* yPlane  = (const uint8_t*)surface->pixels;
    const uint8_t* uvPlane = yPlane + surface->pitch * h;

    if (s_cam.width != w || s_cam.height != h || s_cam.rotation != rotation) {
        if (s_cam.width > 0) {
            sg_destroy_image(s_cam.y_img);
            sg_destroy_image(s_cam.uv_img);
            sg_destroy_sampler(s_cam.smp);
            sg_destroy_buffer(s_cam.bind.vertex_buffers[0]);
            sg_destroy_buffer(s_cam.bind.index_buffer);
            sg_destroy_pipeline(s_cam.pip);
        }
        s_cam.width    = w;
        s_cam.height   = h;
        s_cam.rotation = rotation;

        // Y plane: R8, full resolution, streaming
        sg_image_desc y_desc{};
        y_desc.width              = w;
        y_desc.height             = h;
        y_desc.pixel_format       = SG_PIXELFORMAT_R8;
        y_desc.usage.stream_update = true;
        y_desc.label              = "camera-y";
        s_cam.y_img = sg_make_image(&y_desc);
        
        sg_view_desc y_view_desc = {0};
        sg_texture_view_desc _sg_texture_view_desc={0};
        _sg_texture_view_desc.image = s_cam.y_img;
        y_view_desc.texture = _sg_texture_view_desc;
        y_view_desc.label = "view-camera-y";
        s_cam.y_view = sg_make_view(&y_view_desc);

        // UV plane: RG8, half resolution, streaming
        // NV12 interleaved UV = w/2 pixels * 2 bytes = w bytes per row (same pitch as Y)
        sg_image_desc uv_desc{};
        uv_desc.width              = w / 2;
        uv_desc.height             = h / 2;
        uv_desc.pixel_format       = SG_PIXELFORMAT_RG8;
        uv_desc.usage.stream_update = true;
        uv_desc.label              = "camera-uv";
        s_cam.uv_img = sg_make_image(&uv_desc);
        
        sg_view_desc uv_view_desc = {0};
        sg_texture_view_desc _sg_uv_texture_view_desc={0};
        _sg_uv_texture_view_desc.image = s_cam.uv_img;
        uv_view_desc.texture = _sg_uv_texture_view_desc;
        uv_view_desc.label = "view-camera-uv";
        s_cam.uv_view = sg_make_view(&uv_view_desc);

        // Sampler: linear, clamp
        sg_sampler_desc smp_desc{};
        smp_desc.min_filter = SG_FILTER_LINEAR;
        smp_desc.mag_filter = SG_FILTER_LINEAR;
        smp_desc.wrap_u     = SG_WRAP_CLAMP_TO_EDGE;
        smp_desc.wrap_v     = SG_WRAP_CLAMP_TO_EDGE;
        s_cam.smp = sg_make_sampler(&smp_desc);

        // Fullscreen quad UVs baked with surface rotation.
        // SDL_PROP_SURFACE_ROTATION_FLOAT = degrees to rotate CW to get image right-side-up.
        // We rotate the UV assignment around the 4 corners accordingly.
        // NDC vertex order: BL, BR, TR, TL
        // UV corners at rot=0: BL=(0,1), BR=(1,1), TR=(1,0), TL=(0,0)
        static const float uv_table[4][4][2] = {
            {{0,1},{1,1},{1,0},{0,0}},  // rot=0
            {{1,1},{1,0},{0,0},{0,1}},  // rot=90
            {{1,0},{0,0},{0,1},{1,1}},  // rot=180
            {{0,0},{0,1},{1,1},{1,0}},  // rot=270
        };
        int rot_idx = ((int)(rotation / 90.0f + 0.5f)) % 4;
        const float (*uv)[2] = uv_table[rot_idx];
        float verts[] = {
            -1.0f, -1.0f,  uv[0][0], uv[0][1],
             1.0f, -1.0f,  uv[1][0], uv[1][1],
             1.0f,  1.0f,  uv[2][0], uv[2][1],
            -1.0f,  1.0f,  uv[3][0], uv[3][1],
        };
        sg_buffer_desc vbuf_desc{};
        vbuf_desc.data  = SG_RANGE(verts);
        vbuf_desc.label = "camera-quad-vb";
        sg_buffer vbuf = sg_make_buffer(&vbuf_desc);

        uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
        sg_buffer_desc ibuf_desc{};
        ibuf_desc.usage.index_buffer = true;
        ibuf_desc.data  = SG_RANGE(indices);
        ibuf_desc.label = "camera-quad-ib";
        sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

        // Pipeline — no depth test (background quad)
        sg_pipeline_desc pip_desc{};
        pip_desc.shader = sg_make_shader(camera_texture_shader_desc(sg_query_backend()));
        pip_desc.layout.attrs[ATTR_camera_texture_camera_texture_position].format  = SG_VERTEXFORMAT_FLOAT2;
        pip_desc.layout.attrs[ATTR_camera_texture_camera_texture_texcoord0].format = SG_VERTEXFORMAT_FLOAT2;
        pip_desc.index_type          = SG_INDEXTYPE_UINT16;
        pip_desc.depth.write_enabled = false;
        pip_desc.depth.compare       = SG_COMPAREFUNC_ALWAYS;
        pip_desc.label = "camera-texture-pip";
        s_cam.pip = sg_make_pipeline(&pip_desc);

        // Bindings
        s_cam.bind = sg_bindings{};
        s_cam.bind.vertex_buffers[0]                   = vbuf;
        s_cam.bind.index_buffer                        = ibuf;
        
        s_cam.bind.views[VIEW_camera_texture_tex_y]   = s_cam.y_view;
        s_cam.bind.views[VIEW_camera_texture_tex_uv]  = s_cam.uv_view;
        s_cam.bind.samplers[SMP_camera_texture_smp_y]  = s_cam.smp;
        s_cam.bind.samplers[SMP_camera_texture_smp_uv] = s_cam.smp;
    }

    // Upload Y plane
    sg_image_data y_data{};
    sg_range range = {0};
    range.ptr = yPlane;
    range.size = (size_t)(surface->pitch * h);
    y_data.mip_levels[0] = range;
    sg_update_image(s_cam.y_img, y_data);

    // Upload UV plane (same pitch as Y, h/2 rows)
    sg_image_data uv_data{};
    range.ptr = uvPlane;
    range.size = (size_t)(surface->pitch * h / 2);
    uv_data.mip_levels[0] = range;
    sg_update_image(s_cam.uv_img, uv_data);
}

void nv12_camera_draw(void) {
    if (s_cam.width == 0) return;
    sg_apply_pipeline(s_cam.pip);
    sg_apply_bindings(&s_cam.bind);
    sg_draw(0, 6, 1);
}
