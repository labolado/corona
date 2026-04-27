#include "Core/Rtt_Config.h"
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Renderer/Rtt_BgfxRenderer.h"

#include "Core/Rtt_CrashReporter.h"
#include "Display/Rtt_SDFRenderer.h"
#include "Display/Rtt_InstancedBatchRenderer.h"
#include "Renderer/Rtt_BgfxCommandBuffer.h"
#include "Renderer/Rtt_BgfxFrameBufferObject.h"
#include "Renderer/Rtt_BgfxGeometry.h"
#include "Renderer/Rtt_BgfxProgram.h"
#include "Renderer/Rtt_BgfxTexture.h"
#include "Renderer/Rtt_CPUResource.h"
#include "Display/Rtt_BufferBitmap.h"
#include "Rtt_GPUStream.h"
#include "Core/Rtt_Assert.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#if defined(Rtt_ANDROID_ENV)
#include <dlfcn.h>

// Minimal Vulkan types for GPU probe (avoid vulkan.h dependency)
// We dlopen libvulkan.so and query device properties before bgfx::init()
namespace {
    typedef uint32_t VkFlags;
    typedef VkFlags VkInstanceCreateFlags;
    typedef enum { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1, VK_STRUCTURE_TYPE_APPLICATION_INFO = 0 } VkStructureType_Probe;
    typedef enum { VK_SUCCESS_PROBE = 0 } VkResult_Probe;
    typedef enum { VK_PHYSICAL_DEVICE_TYPE_OTHER = 0 } VkPhysicalDeviceType_Probe;

    struct VkApplicationInfo_Probe {
        uint32_t sType; const void* pNext; const char* pApplicationName;
        uint32_t applicationVersion; const char* pEngineName; uint32_t engineVersion; uint32_t apiVersion;
    };
    struct VkInstanceCreateInfo_Probe {
        uint32_t sType; const void* pNext; VkInstanceCreateFlags flags;
        const VkApplicationInfo_Probe* pApplicationInfo;
        uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
        uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
    };
    struct VkPhysicalDeviceLimits_Probe { uint32_t pad[128]; }; // opaque, we don't read it
    struct VkPhysicalDeviceSparseProperties_Probe { uint32_t pad[5]; };
    struct VkPhysicalDeviceProperties_Probe {
        uint32_t apiVersion; uint32_t driverVersion; uint32_t vendorID; uint32_t deviceID;
        uint32_t deviceType; char deviceName[256];
        uint8_t pipelineCacheUUID[16];
        VkPhysicalDeviceLimits_Probe limits;
        VkPhysicalDeviceSparseProperties_Probe sparseProperties;
    };

    typedef void* VkInstance_Probe;
    typedef void* VkPhysicalDevice_Probe;

    typedef int (*PFN_vkCreateInstance)(const VkInstanceCreateInfo_Probe*, const void*, VkInstance_Probe*);
    typedef void (*PFN_vkDestroyInstance)(VkInstance_Probe, const void*);
    typedef int (*PFN_vkEnumeratePhysicalDevices)(VkInstance_Probe, uint32_t*, VkPhysicalDevice_Probe*);
    typedef void (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice_Probe, VkPhysicalDeviceProperties_Probe*);

    // Unity-derived vendor thresholds for Vulkan stability
    // Source: Unity Engine Vulkan allow/deny list (2025)
    enum VkVendorID {
        kVendorARM         = 0x13B5,  // Mali GPUs
        kVendorQualcomm    = 0x5143,  // Adreno GPUs
        kVendorImagination = 0x1010,  // PowerVR GPUs
        kVendorSamsung     = 0x144D,  // Xclipse GPUs
    };

    #define VK_MAKE_API_VERSION(major, minor, patch) \
        (((uint32_t)(major) << 22) | ((uint32_t)(minor) << 12) | (uint32_t)(patch))

    // Returns true if this GPU should use Vulkan based on Unity's vendor thresholds
    static bool isVulkanSafeForDevice()
    {
        void* lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        if (!lib) {
            Rtt_LogException("VulkanProbe: cannot dlopen libvulkan.so");
            return false;
        }

        auto fnCreateInstance = (PFN_vkCreateInstance)dlsym(lib, "vkCreateInstance");
        auto fnDestroyInstance = (PFN_vkDestroyInstance)dlsym(lib, "vkDestroyInstance");
        auto fnEnumDevices = (PFN_vkEnumeratePhysicalDevices)dlsym(lib, "vkEnumeratePhysicalDevices");
        auto fnGetProps = (PFN_vkGetPhysicalDeviceProperties)dlsym(lib, "vkGetPhysicalDeviceProperties");

        if (!fnCreateInstance || !fnDestroyInstance || !fnEnumDevices || !fnGetProps) {
            Rtt_LogException("VulkanProbe: missing Vulkan symbols");
            dlclose(lib);
            return false;
        }

        VkInstanceCreateInfo_Probe ici = {};
        ici.sType = 1; // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
        VkInstance_Probe instance = nullptr;

        int result = fnCreateInstance(&ici, nullptr, &instance);
        if (result != 0 || !instance) {
            Rtt_LogException("VulkanProbe: vkCreateInstance failed (%d)", result);
            dlclose(lib);
            return false;
        }

        uint32_t deviceCount = 0;
        fnEnumDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            Rtt_LogException("VulkanProbe: no physical devices");
            fnDestroyInstance(instance, nullptr);
            dlclose(lib);
            return false;
        }

        VkPhysicalDevice_Probe physDevice = nullptr;
        uint32_t one = 1;
        fnEnumDevices(instance, &one, &physDevice);

        VkPhysicalDeviceProperties_Probe props = {};
        fnGetProps(physDevice, &props);

        uint32_t apiMajor = (props.apiVersion >> 22) & 0x3FF;
        uint32_t apiMinor = (props.apiVersion >> 12) & 0x3FF;
        uint32_t apiPatch = props.apiVersion & 0xFFF;

        Rtt_LogException("VulkanProbe: GPU=\"%s\" vendorID=0x%04X deviceID=0x%04X apiVersion=%u.%u.%u driverVersion=0x%08X",
            props.deviceName, props.vendorID, props.deviceID, apiMajor, apiMinor, apiPatch, props.driverVersion);

        bool safe = false;
        switch (props.vendorID) {
            case kVendorARM:         // Mali: stable from Vulkan 1.0.61
                safe = props.apiVersion >= VK_MAKE_API_VERSION(1, 0, 61);
                Rtt_LogException("VulkanProbe: ARM Mali, threshold=1.0.61, safe=%s", safe ? "true" : "false");
                break;
            case kVendorQualcomm:    // Adreno: stable from Vulkan 1.0.49
                safe = props.apiVersion >= VK_MAKE_API_VERSION(1, 0, 49);
                Rtt_LogException("VulkanProbe: Qualcomm Adreno, threshold=1.0.49, safe=%s", safe ? "true" : "false");
                break;
            case kVendorImagination: // PowerVR: highest bar — Vulkan 1.1.170 + driver 1.473
                safe = props.apiVersion >= VK_MAKE_API_VERSION(1, 1, 170);
                Rtt_LogException("VulkanProbe: Imagination PowerVR, threshold=1.1.170, safe=%s", safe ? "true" : "false");
                break;
            case kVendorSamsung:     // Xclipse (Exynos): treat like Mali
                safe = props.apiVersion >= VK_MAKE_API_VERSION(1, 0, 61);
                Rtt_LogException("VulkanProbe: Samsung Xclipse, threshold=1.0.61, safe=%s", safe ? "true" : "false");
                break;
            default:
                Rtt_LogException("VulkanProbe: unknown vendor 0x%04X, defaulting to GLES", props.vendorID);
                safe = false;
                break;
        }

        fnDestroyInstance(instance, nullptr);
        dlclose(lib);
        return safe;
    }
} // anonymous namespace
#endif

#if defined( Rtt_ANDROID_ENV )
#include <android/log.h>
#endif

#include <bgfx/platform.h>
#include <bgfx/bgfx.h>

#include "Rtt_BgfxContext.h"

// ----------------------------------------------------------------------------

// Custom bgfx callback that catches shader compile failures gracefully.
// Default CallbackStub calls abort() on ALL fatal errors including shader
// compile failures, which crashes the app. This callback logs the error
// and only aborts on truly unrecoverable errors.
// Pipeline cache directory (set by BgfxRenderer::Initialize via SetPipelineCacheDir)
static std::string s_pipelineCacheDir;

static std::string PipelineCachePath(uint64_t _id)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/pipeline_%016llx.bin",
             s_pipelineCacheDir.c_str(), (unsigned long long)_id);
    return std::string(buf);
}

struct Solar2dBgfxCallback : public bgfx::CallbackI
{
    virtual void fatal(
        const char* _filePath, uint16_t _line,
        bgfx::Fatal::Enum _code, const char* _str) override
    {
        fprintf(stderr, "BGFX ERROR [%s:%d] code=%d: %s\n", _filePath, _line, _code, _str);
        Rtt_LogException("BGFX ERROR [%s:%d] code=%d: %s\n", _filePath, _line, _code, _str);

        // Recoverable errors: log but don't abort.
        // InvalidShader: bgfx uses invalid handle, rendering may glitch but won't crash.
        // DeviceLost: common on mobile (lock screen, app suspend). bgfx may recover.
        // UnableToCreateTexture: memory pressure, can degrade gracefully.
        if (_code == bgfx::Fatal::InvalidShader
            || _code == bgfx::Fatal::DeviceLost
            || _code == bgfx::Fatal::UnableToCreateTexture)
        {
            const char* names[] = { "DebugCheck", "InvalidShader", "UnableToInitialize",
                                    "UnableToCreateTexture", "DeviceLost" };
            const char* name = (_code < 5) ? names[_code] : "Unknown";
            Rtt_LogException("BGFX: %s — continuing (app will not crash)\n", name);
            return; // Don't abort
        }

        // Truly unrecoverable: DebugCheck, UnableToInitialize
        abort();
    }
    virtual void traceVargs(const char* _filePath, uint16_t _line,
                            const char* _format, va_list _argList) override
    {
        char buf[2048];
        vsnprintf(buf, sizeof(buf), _format, _argList);
        fprintf(stderr, "BGFX TRACE [%s:%d]: %s", _filePath, _line, buf);
#if defined(Rtt_ANDROID_ENV)
        __android_log_print(ANDROID_LOG_INFO, "bgfx", "[%s:%d] %s", _filePath, _line, buf);
#endif
    }
    virtual void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    virtual void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    virtual void profilerEnd() override {}
    virtual uint32_t cacheReadSize(uint64_t _id) override
    {
        if (s_pipelineCacheDir.empty()) return 0;
        std::string path = PipelineCachePath(_id);
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return 0;
        fseek(f, 0, SEEK_END);
        uint32_t size = (uint32_t)ftell(f);
        fclose(f);
        return size;
    }
    virtual bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override
    {
        if (s_pipelineCacheDir.empty()) return false;
        std::string path = PipelineCachePath(_id);
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;
        size_t read = fread(_data, 1, _size, f);
        fclose(f);
        return read == _size;
    }
    virtual void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override
    {
        if (s_pipelineCacheDir.empty()) return;
        std::string path = PipelineCachePath(_id);
        FILE* f = fopen(path.c_str(), "wb");
        if (!f) return;
        fwrite(_data, 1, _size, f);
        fclose(f);
    }
    virtual void screenShot(const char*, uint32_t, uint32_t, uint32_t,
                            bgfx::TextureFormat::Enum, const void*, uint32_t, bool) override {}
    virtual void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    virtual void captureEnd() override {}
    virtual void captureFrame(const void*, uint32_t) override {}
};

static Solar2dBgfxCallback s_bgfxCallback;

namespace Rtt
{

// Shared cache dir for shader disk caching (extern'd by Rtt_BgfxShaderCompiler.cpp)
std::string s_shaderCacheDir;

void
BgfxRenderer::SetCacheDir(const char* path)
{
    if (!path || !path[0]) return;

    // Pipeline cache subdir
    s_pipelineCacheDir = std::string(path) + "/bgfx_pipeline";
    mkdir(s_pipelineCacheDir.c_str(), 0755);

    // Shader cache subdir (used by BgfxShaderCompiler)
    s_shaderCacheDir = std::string(path) + "/bgfx_shaders";
    mkdir(s_shaderCacheDir.c_str(), 0755);

    Rtt_LogException("BgfxRenderer: cache dirs set — pipeline: %s, shaders: %s\n",
                     s_pipelineCacheDir.c_str(), s_shaderCacheDir.c_str());
}

// ----------------------------------------------------------------------------

#if defined( Rtt_ANDROID_ENV )
static int sZeroViewFrames = 0;
static bool sBugDumped = false;

static void LogBgfxFrameStats( const char* phase, bool isCapture )
{
    const bgfx::Stats* stats = bgfx::getStats();
    if( ! stats )
    {
        __android_log_print( ANDROID_LOG_INFO, "Corona", "BGFX_FRAME: %s capture=%d stats=null", phase, isCapture ? 1 : 0 );
        return;
    }

    // Auto-detect black screen: viewCount=0 but draws > 0
    if( phase[0] == 'e' ) // "end" phase
    {
        if( stats->numViews == 0 && stats->numDraw > 0 )
        {
            sZeroViewFrames++;
            if( sZeroViewFrames == 3 && !sBugDumped )
            {
                sBugDumped = true;
                __android_log_print( ANDROID_LOG_ERROR, "Corona",
                    "BLACK_SCREEN_DETECTED: %d consecutive frames with viewCount=0 draw=%u. "
                    "Views not being touched - check setViewRect/touch/submit calls.",
                    sZeroViewFrames, stats->numDraw );
                // Dump breadcrumbs if crash reporter is available
                Rtt_BreadcrumbDump( 2 ); // stderr → logcat
            }
        }
        else
        {
            if( sZeroViewFrames > 0 && sBugDumped )
            {
                __android_log_print( ANDROID_LOG_INFO, "Corona",
                    "BLACK_SCREEN_RECOVERED: rendering resumed after %d zero-view frames",
                    sZeroViewFrames );
            }
            sZeroViewFrames = 0;
            sBugDumped = false;
        }
    }

    // Suppress per-frame logging to avoid flooding logcat
    // __android_log_print( ANDROID_LOG_INFO, "Corona",
    //     "BGFX_FRAME: %s gpuFrame=%u viewCount=%u draw=%u capture=%d",
    //     phase, stats->gpuFrameNum, stats->numViews, stats->numDraw, isCapture ? 1 : 0 );
}
#endif

BgfxRenderer::BgfxRenderer(Rtt_Allocator* allocator)
:   Super(allocator),
    fCaps(),
    fCapsInitialized(false),
    fBgfxInitialized(false),
    fStagingTexture( BGFX_INVALID_HANDLE ),
    fStagingW( 0 ),
    fStagingH( 0 ),
    fGeometryPool( allocator )
{
    memset(&fCaps, 0, sizeof(fCaps));

    // Create double-buffered command buffers
    fFrontCommandBuffer = Rtt_NEW(allocator, BgfxCommandBuffer(allocator));
    fBackCommandBuffer = Rtt_NEW(allocator, BgfxCommandBuffer(allocator));
}

BgfxRenderer::~BgfxRenderer()
{
    // Shutdown bgfx if it was initialized
    ShutdownBgfx();
}

bool
BgfxRenderer::InitializeBgfx(void* nativeWindowHandle, U32 width, U32 height)
{
    Rtt_ASSERT(!fBgfxInitialized);
    Rtt_ASSERT(nativeWindowHandle != NULL);

    bgfx::Init init;
    // Android: prefer Vulkan if available, otherwise GLES
    // Other platforms: auto-detect (Metal on macOS/iOS)
#if defined(Rtt_ANDROID_ENV)
    // Check if Vulkan is supported before requesting it (avoid SIGSEGV on devices without Vulkan driver)
    bool vulkanAvailable = false;
    {
        bgfx::RendererType::Enum supportedTypes[bgfx::RendererType::Count];
        uint8_t numTypes = bgfx::getSupportedRenderers(bgfx::RendererType::Count, supportedTypes);
        Rtt_LogException("BgfxRenderer: getSupportedRenderers returned %d renderers", numTypes);
        for (uint8_t i = 0; i < numTypes; ++i)
        {
            Rtt_LogException("BgfxRenderer: supported[%d] = %s", i, bgfx::getRendererName(supportedTypes[i]));
            if (supportedTypes[i] == bgfx::RendererType::Vulkan) { vulkanAvailable = true; }
        }
    }
    // Vulkan selection strategy:
    // 1. SOLAR2D_VULKAN=1 → force Vulkan
    // 2. SOLAR2D_VULKAN=0 → force GLES
    // 3. No env var → auto-detect if Vulkan is reported by bgfx and the device probe is safe
    bool useVulkan = false;
    const char* vkEnv = getenv("SOLAR2D_VULKAN");
    if (vkEnv) {
        useVulkan = (atoi(vkEnv) == 1);
        Rtt_LogException("BgfxRenderer: SOLAR2D_VULKAN=%s, manual override → %s",
            vkEnv, useVulkan ? "Vulkan" : "GLES");
    } else if (vulkanAvailable) {
        useVulkan = isVulkanSafeForDevice();
        Rtt_LogException("BgfxRenderer: Vulkan available, auto-detect → %s",
            useVulkan ? "Vulkan" : "GLES");
    } else {
        Rtt_LogException("BgfxRenderer: Vulkan not in supported renderers → GLES");
    }
    init.type = (vulkanAvailable && useVulkan) ? bgfx::RendererType::Vulkan : bgfx::RendererType::OpenGLES;
#else
    init.type = bgfx::RendererType::Count;
#endif
    init.resolution.width = width;
    init.resolution.height = height;
    init.platformData.nwh = nativeWindowHandle;

    // Platform-specific reset flags
    // SOLAR2D_MSAA env var overrides MSAA level for testing degradation paths
    const char* msaaEnv = getenv("SOLAR2D_MSAA");
    int msaaLevel = msaaEnv ? atoi(msaaEnv) : -1;

    // All platforms: FLIP_AFTER_RENDER ensures present happens AFTER GPU render,
    // reducing display latency from N-2 to N-1 frames and preventing 1-frame flash artifacts.
    init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4 | BGFX_RESET_FLIP_AFTER_RENDER;

    // SOLAR2D_MSAA override (0=off, 2=X2, 4=X4)
    if (msaaLevel >= 0)
    {
        init.resolution.reset &= ~BGFX_RESET_MSAA_MASK;
        if (msaaLevel >= 4) init.resolution.reset |= BGFX_RESET_MSAA_X4;
        else if (msaaLevel >= 2) init.resolution.reset |= BGFX_RESET_MSAA_X2;
    }

    // SOLAR2D_RENDERER override (metal/gles/vulkan) for testing
    const char* rendererEnv = getenv("SOLAR2D_RENDERER");
    if (rendererEnv)
    {
        if (strcmp(rendererEnv, "metal") == 0) init.type = bgfx::RendererType::Metal;
        else if (strcmp(rendererEnv, "gles") == 0) init.type = bgfx::RendererType::OpenGLES;
        else if (strcmp(rendererEnv, "vulkan") == 0) init.type = bgfx::RendererType::Vulkan;
        else if (strcmp(rendererEnv, "gl") == 0) init.type = bgfx::RendererType::OpenGL;
    }

    // Cache reset flags for SetViewport to reuse (prevents losing FLIP_AFTER_RENDER on resize)
    BgfxCommandBuffer::SetCachedResetFlags(init.resolution.reset);

    init.callback = &s_bgfxCallback;
    init.fallback = false; // P5 diag: force Vulkan to fail loudly instead of silently falling back to GLES
    Rtt_LogException("BgfxRenderer: attempting init with type=%s fallback=false", bgfx::getRendererName(init.type));

    // Step 0 harness (Issue #027): probe whether bgfx::createFrameBuffer(nwh) on an
    // already-initialized session returns a valid secondary swap chain.
    // Activated by SOLAR2D_DEBUG_SECONDARY_FB=1. Removed in Step 1.
    static int s_bgfxInitCount = 0;
    const char* dbgSecondary = getenv("SOLAR2D_DEBUG_SECONDARY_FB");
    bool harnessActive = (dbgSecondary && atoi(dbgSecondary) == 1);
    if (harnessActive && s_bgfxInitCount > 0)
    {
        Rtt_LogException("STEP0_HARNESS: attempting bgfx::createFrameBuffer(nwh=%p, w=%u, h=%u) on existing session",
                         nativeWindowHandle, width, height);
        const bgfx::Caps* capsNow = bgfx::getCaps();
        bool capSwap = capsNow && (capsNow->supported & BGFX_CAPS_SWAP_CHAIN);
        Rtt_LogException("STEP0_HARNESS: BGFX_CAPS_SWAP_CHAIN supported=%d renderer=%s",
                         (int)capSwap, capsNow ? bgfx::getRendererName(capsNow->rendererType) : "null");

        bgfx::FrameBufferHandle probeFb = bgfx::createFrameBuffer(
            nativeWindowHandle,
            static_cast<uint16_t>(width),
            static_cast<uint16_t>(height),
            bgfx::TextureFormat::BGRA8,
            bgfx::TextureFormat::D24S8);
        bool probeValid = bgfx::isValid(probeFb);
        Rtt_LogException("STEP0_HARNESS: createFrameBuffer returned idx=%d isValid=%d",
                         (int)probeFb.idx, (int)probeValid);

        if (probeValid)
        {
            // Try to submit one cleared frame to the secondary FB on a dedicated view.
            // Magenta fill so we can visually tell if the secondary window picks it up.
            const bgfx::ViewId kProbeView = 250;
            bgfx::setViewFrameBuffer(kProbeView, probeFb);
            bgfx::setViewRect(kProbeView, 0, 0,
                              static_cast<uint16_t>(width),
                              static_cast<uint16_t>(height));
            bgfx::setViewClear(kProbeView, BGFX_CLEAR_COLOR, 0xff00ffff, 1.0f, 0); // magenta
            bgfx::touch(kProbeView);
            bgfx::frame();
            Rtt_LogException("STEP0_HARNESS: frame() after submit to secondary FB OK");

            // Unbind view and destroy probe FB cleanly (2x frame flush per bgfx docs).
            bgfx::setViewFrameBuffer(kProbeView, BGFX_INVALID_HANDLE);
            bgfx::destroy(probeFb);
            bgfx::frame();
            bgfx::frame();
            Rtt_LogException("STEP0_HARNESS: probe FB destroyed cleanly");
        }
        else
        {
            Rtt_LogException("STEP0_HARNESS: FAILED to create secondary FB; Step 0 regarded as FAIL");
        }
        // Fall through to existing path so the app continues (this harness only probes the API).
    }

    fBgfxInitialized = bgfx::init(init);
    if (!fBgfxInitialized)
    {
        // bgfx is a singleton: init() fails if s_ctx != NULL (previous session
        // not fully shut down, e.g. welcome screen → project window transition).
        // Force shutdown the stale instance, drain render thread, and retry.
        Rtt_LogException("BgfxRenderer: init failed (stale session?), forcing shutdown and retrying");
        bgfx::shutdown();
        // Wait for renderer thread to fully exit after shutdown.
        // bgfx::shutdown() may return before the Metal/Vulkan backend thread
        // has finished its last submit(), causing UAF on reinit.
        usleep(500000); // 500ms — generous wait for GPU resources to release
        fBgfxInitialized = bgfx::init(init);
    }
    if (fBgfxInitialized)
    {
        ++s_bgfxInitCount;
    }
#if defined(Rtt_ANDROID_ENV)
    // Vulkan fallback: if Vulkan init failed, try GLES
    if (!fBgfxInitialized && init.type == bgfx::RendererType::Vulkan)
    {
        Rtt_LogException("BgfxRenderer: Vulkan init failed, falling back to OpenGLES");
        bgfx::shutdown();
        init.type = bgfx::RendererType::OpenGLES;
        fBgfxInitialized = bgfx::init(init);
    }
#endif

    if (fBgfxInitialized)
    {
        const char* rendererName = bgfx::getRendererName(bgfx::getRendererType());
        fprintf(stderr, "BGFX_INIT: renderer=%s nwh=%p w=%u h=%u\n",
                rendererName, nativeWindowHandle, width, height);
        Rtt_LogException("BGFX_INIT: renderer=%s w=%u h=%u", rendererName, width, height);
        bgfx::setDebug(BGFX_DEBUG_NONE);
        // Set default view clear state (view 200 = screen, FBO views use 1-199)
        bgfx::setViewClear(200, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::setViewRect(200, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));

        // Initialize SDF renderer for bgfx
        SDFRenderer::Instance().Initialize();
    }

    return fBgfxInitialized;
}

void
BgfxRenderer::ShutdownBgfx()
{
    if( bgfx::isValid( fStagingTexture ) )
    {
        bgfx::destroy( fStagingTexture );
        fStagingTexture = BGFX_INVALID_HANDLE;
    }

    // Drain the geometry pool before bgfx shutdown
    fGeometryPool.Shutdown();

    if (fBgfxInitialized)
    {
        // Finalize singleton renderers BEFORE bgfx::shutdown() to ensure
        // bgfx handles are destroyed while bgfx is still alive.
        // Static singleton destructors have undefined order relative to
        // bgfx::shutdown(), so explicit cleanup here is required.
        SDFRenderer::Instance().Finalize();
        InstancedBatchRenderer::Instance().Finalize();

        bgfx::shutdown();
        fBgfxInitialized = false;
    }
}

void
BgfxRenderer::BeginFrame( Real totalTime, Real deltaTime, const TimeTransform *defTimeTransform, Real contentScaleX, Real contentScaleY, bool isCapture )
{
#if defined( Rtt_ANDROID_ENV )
    LogBgfxFrameStats( "begin", isCapture );
#endif

    Super::BeginFrame( totalTime, deltaTime, defTimeTransform, contentScaleX, contentScaleY, isCapture );
}

void
BgfxRenderer::EndFrame()
{
#if defined( Rtt_ANDROID_ENV )
    LogBgfxFrameStats( "end", false );
#endif

    Super::EndFrame();
}

void
BgfxRenderer::InitCaps()
{
    const bgfx::Caps* caps = bgfx::getCaps();

    fCaps.maxTextureSize = caps->limits.maxTextureSize;
    fCaps.maxUniformVectors = 256;  // bgfx doesn't expose this directly, use safe default
    fCaps.maxVertexTextureUnits = 0;  // Not queried directly, set to 0 for now
    fCaps.supportsHighPrecisionFragmentShaders = true;  // bgfx backends generally support this
    fCaps.originBottomLeft = caps->originBottomLeft;
    fCaps.vendorString = "bgfx";
    fCaps.rendererString = bgfx::getRendererName(bgfx::getRendererType());
    fCaps.versionString = "";

    fCapsInitialized = true;
}

const RendererCaps&
BgfxRenderer::GetCaps() const
{
    if (!fCapsInitialized)
    {
        const_cast<BgfxRenderer*>(this)->InitCaps();
    }
    return fCaps;
}

void
BgfxRenderer::CaptureFrameBuffer( RenderingStream & stream, BufferBitmap & bitmap, S32 x_in_pixels, S32 y_in_pixels, S32 w_in_pixels, S32 h_in_pixels )
{
#if defined( Rtt_ANDROID_ENV )
    static uint32_t sCaptureSeq = 0;
    const uint32_t captureSeq = __atomic_add_fetch( &sCaptureSeq, 1u, __ATOMIC_RELAXED );
    __android_log_print( ANDROID_LOG_INFO, "BGFX_CAPTURE",
        "CAPTURE begin seq=%u ts=%lld rect=%d,%d %dx%d",
        captureSeq,
        static_cast<long long>( bgfx::getStats()->cpuTimeFrame ),
        static_cast<int>( x_in_pixels ),
        static_cast<int>( y_in_pixels ),
        static_cast<int>( w_in_pixels ),
        static_cast<int>( h_in_pixels ) );
#endif

    // Get the current FBO - Display::Capture() renders to an FBO and we need to read it back
    FrameBufferObject* fbo = GetFrameBufferObject();
    if( !fbo )
    {
        // No FBO bound - fall back to base (which uses glReadPixels, may not work)
        Super::CaptureFrameBuffer( stream, bitmap, x_in_pixels, y_in_pixels, w_in_pixels, h_in_pixels );
        return;
    }

    // Get the FBO's texture handle
    BgfxFrameBufferObject* bgfxFbo = static_cast<BgfxFrameBufferObject*>( fbo->GetGPUResource() );
    if( !bgfxFbo )
    {
        return;
    }

    bgfx::TextureHandle srcTexture = bgfxFbo->GetTextureHandle();
    if( !bgfx::isValid( srcTexture ) )
    {
        return;
    }

    U32 readW = static_cast<U32>( w_in_pixels );
    U32 readH = static_cast<U32>( h_in_pixels );
    U32 bufferSize = readW * readH * 4; // RGBA8
    U8* readbackBuffer = static_cast<U8*>( malloc( bufferSize ) );
    if( !readbackBuffer )
    {
        return;
    }
    memset( readbackBuffer, 0, bufferSize );

    // Use bgfx generic path (blit + readTexture) with skipPresent protection.
    // Metal readback via getInternalTexturePtr was removed because calling
    // getInternal() from the API thread races with the render thread.
    {
        // Flush pending rendering while the FBO is still bound so the scene
        // is actually rendered into the source texture before we read it back.
        bgfx::ViewId fboViewId = bgfxFbo->GetViewId();
#if defined( Rtt_ANDROID_ENV )
        __android_log_print( ANDROID_LOG_INFO, "BGFX_CAPTURE",
            "CAPTURE setSkipPresent seq=%u value=1 view=%u",
            captureSeq,
            static_cast<unsigned int>( fboViewId ) );
#endif
        bgfx::setSkipPresent( true );
        bgfx::frame();
        bgfx::setViewFrameBuffer( fboViewId, BGFX_INVALID_HANDLE );

        if( !bgfx::isValid( fStagingTexture ) || fStagingW != readW || fStagingH != readH )
        {
            if( bgfx::isValid( fStagingTexture ) )
            {
                bgfx::destroy( fStagingTexture );
            }

            fStagingTexture = bgfx::createTexture2D(
                static_cast<uint16_t>( readW ),
                static_cast<uint16_t>( readH ),
                false, 1,
                bgfx::TextureFormat::RGBA8,
                BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK
            );
            fStagingW = readW;
            fStagingH = readH;

            if( !bgfx::isValid( fStagingTexture ) )
            {
                Rtt_LogException( "ERROR: BgfxRenderer staging texture createTexture2D FAILED (w=%u h=%u). Screen capture will be skipped.\n",
                    readW, readH );
            }
        }

        if( bgfx::isValid( fStagingTexture ) )
        {
            const bgfx::ViewId kBlitView = 255;
            bgfx::setViewRect( kBlitView, 0, 0, static_cast<uint16_t>( readW ), static_cast<uint16_t>( readH ) );

            // Touch view BEFORE blit to ensure bgfx processes this view
            bgfx::touch( kBlitView );

            bgfx::blit(
                kBlitView,
                fStagingTexture,
                0, 0,
                srcTexture,
                static_cast<uint16_t>( x_in_pixels ),
                static_cast<uint16_t>( y_in_pixels ),
                static_cast<uint16_t>( readW ),
                static_cast<uint16_t>( readH )
            );

            uint32_t readyFrame = bgfx::readTexture( fStagingTexture, readbackBuffer );

            bgfx::touch( kBlitView );
            uint32_t currentFrame = bgfx::frame();
#if defined( Rtt_ANDROID_ENV )
            __android_log_print( ANDROID_LOG_INFO, "BGFX_CAPTURE",
                "CAPTURE readback seq=%u readyFrame=%u currentFrame=%u attempts=0",
                captureSeq,
                readyFrame,
                currentFrame );
#endif

            // Wait for readback with timeout to prevent infinite loop
            uint32_t maxAttempts = 100;
            uint32_t attempts = 0;
            while( currentFrame < readyFrame && attempts < maxAttempts )
            {
                currentFrame = bgfx::frame();
                attempts++;
            }

            if( attempts >= maxAttempts )
            {
                fprintf( stderr, "BGFX CaptureFrameBuffer: readback timeout after %u frames\n", attempts );
            }

#if defined( Rtt_ANDROID_ENV )
            __android_log_print( ANDROID_LOG_INFO, "BGFX_CAPTURE",
                "CAPTURE ready seq=%u readyFrame=%u currentFrame=%u attempts=%u",
                captureSeq,
                readyFrame,
                currentFrame,
                attempts );
#endif
        }
        else
        {
            static int sStagingWarnCount = 0;
            if( sStagingWarnCount < 3 )
            {
                Rtt_LogException( "WARNING: BgfxRenderer staging texture invalid; screen capture will be skipped.\n" );
                if( ++sStagingWarnCount == 3 )
                    Rtt_LogException( "(further staging-texture warnings suppressed)\n" );
            }
        }

#if defined( Rtt_ANDROID_ENV )
        __android_log_print( ANDROID_LOG_INFO, "BGFX_CAPTURE",
            "CAPTURE setSkipPresent seq=%u value=0",
            captureSeq );
#endif
        bgfx::setSkipPresent( false );
        bgfx::setViewFrameBuffer( fboViewId, BGFX_INVALID_HANDLE );
    }

    // Copy readback data to the bitmap
    U8* dstData = static_cast<U8*>( bitmap.WriteAccess() );
    if( dstData )
    {
        U32 bitmapW = bitmap.Width();
        U32 bitmapH = bitmap.Height();
        U32 copyW = ( readW < bitmapW ) ? readW : bitmapW;
        U32 copyH = ( readH < bitmapH ) ? readH : bitmapH;

        // bgfx readback is RGBA8: bytes [R, G, B, A].
        //
        // PLATFORM-SPECIFIC byte order conversion:
        // - GLES (iOS/Android): bitmap is kRGBA [R,G,B,A] — no conversion needed
        // - Mac desktop: bitmap is kBGRA with kCGImageAlphaPremultipliedFirst
        //   Big-endian byte order → memory is [A, R, G, B] — rotate RGBA→ARGB
#if defined( Rtt_OPENGLES )
        // GLES readback is RGBA, bitmap is kRGBA — direct copy
        for( U32 row = 0; row < copyH; ++row )
        {
            const U8* src = readbackBuffer + row * readW * 4;
            U8* dst = dstData + row * bitmapW * 4;
            memcpy( dst, src, copyW * 4 );
        }
#else
        // Convert RGBA → ARGB(BE): rotate bytes right by 1.
        for( U32 row = 0; row < copyH; ++row )
        {
            const U8* src = readbackBuffer + row * readW * 4;
            U8* dst = dstData + row * bitmapW * 4;
            for( U32 col = 0; col < copyW; ++col )
            {
                dst[col * 4 + 0] = src[col * 4 + 3]; // A
                dst[col * 4 + 1] = src[col * 4 + 0]; // R
                dst[col * 4 + 2] = src[col * 4 + 1]; // G
                dst[col * 4 + 3] = src[col * 4 + 2]; // B
            }
        }
#endif
    }

    free( readbackBuffer );
}

void
BgfxRenderer::EndCapture()
{
    // Clean up staging texture after capture is complete
    if( bgfx::isValid( fStagingTexture ) )
    {
        bgfx::destroy( fStagingTexture );
        fStagingTexture = BGFX_INVALID_HANDLE;
        fStagingW = 0;
        fStagingH = 0;
    }
}

void
BgfxRenderer::SetSkipPresent( bool skip )
{
    bgfx::setSkipPresent( skip );
}

void
BgfxRenderer::ReleaseGPUResource( GPUResource* resource )
{
    if( resource->IsPoolable() )
    {
        fGeometryPool.Recycle( static_cast<BgfxGeometry*>( resource ) );
    }
    else
    {
        delete resource;
    }
}

GPUResource*
BgfxRenderer::Create(const CPUResource* resource)
{
    switch (resource->GetType())
    {
        case CPUResource::kFrameBufferObject:
            return new BgfxFrameBufferObject;

        case CPUResource::kGeometry:
            return fGeometryPool.Acquire();

        case CPUResource::kProgram:
            return new BgfxProgram;

        case CPUResource::kTexture:
            return new BgfxTexture;

        case CPUResource::kUniform:
            // Uniforms are handled differently in bgfx (global uniforms)
            return NULL;

#ifdef Rtt_IPHONE_ENV
        case CPUResource::kVideoTexture:
            // TODO: Implement BgfxVideoTexture if needed
            Rtt_LogException("BgfxRenderer: VideoTexture not yet implemented for bgfx backend");
            return NULL;
#endif

        default:
            Rtt_ASSERT_NOT_REACHED();
            return NULL;
    }
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------


#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
