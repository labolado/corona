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

#if defined( Rtt_ANDROID_ENV )
#include <android/log.h>
#endif

#include <bgfx/platform.h>
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

// Custom bgfx callback that catches shader compile failures gracefully.
// Default CallbackStub calls abort() on ALL fatal errors including shader
// compile failures, which crashes the app. This callback logs the error
// and only aborts on truly unrecoverable errors.
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
    virtual uint32_t cacheReadSize(uint64_t) override { return 0; }
    virtual bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    virtual void cacheWrite(uint64_t, const void*, uint32_t) override {}
    virtual void screenShot(const char*, uint32_t, uint32_t, uint32_t,
                            bgfx::TextureFormat::Enum, const void*, uint32_t, bool) override {}
    virtual void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    virtual void captureEnd() override {}
    virtual void captureFrame(const void*, uint32_t) override {}
};

static Solar2dBgfxCallback s_bgfxCallback;

namespace Rtt
{

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
    // P5 fix: descriptor pool limit raised from 1024 to 4096 via config.h
    // Default to GLES for safety — some devices (e.g. PowerVR/MediaTek) crash in Vulkan driver init.
    // Vulkan can be enabled via SOLAR2D_VULKAN=1 env var or build-time flag.
    bool forceVulkan = false;
    {
        const char* vkEnv = getenv("SOLAR2D_VULKAN");
        if (vkEnv && atoi(vkEnv) == 1) { forceVulkan = true; }
    }
    init.type = (vulkanAvailable && forceVulkan) ? bgfx::RendererType::Vulkan : bgfx::RendererType::OpenGLES;
    Rtt_LogException("BgfxRenderer: vulkanAvailable=%s forceVulkan=%s, init.type=%s",
        vulkanAvailable ? "true" : "false", forceVulkan ? "true" : "false", bgfx::getRendererName(init.type));
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
    fBgfxInitialized = bgfx::init(init);
    if (!fBgfxInitialized)
    {
        // bgfx is a singleton: init() fails if s_ctx != NULL (previous session
        // not fully shut down, e.g. welcome screen extension closing async).
        // Force shutdown the stale instance and retry once.
        Rtt_LogException("BgfxRenderer: init failed (stale session?), forcing shutdown and retrying");
        bgfx::shutdown();
        // Wait for renderer thread to fully exit after shutdown.
        // bgfx::shutdown() may return before the Metal/Vulkan backend thread
        // has finished its last submit(), causing UAF on reinit.
        usleep(200000); // 200ms
        fBgfxInitialized = bgfx::init(init);
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
