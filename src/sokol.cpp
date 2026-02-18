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

#include <SDL3/SDL_main.h>
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
    sg_swapchain swapchain;
    memset(&swapchain, 0, sizeof(swapchain));

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

