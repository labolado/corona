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

        // Shader compile/link failures: log but don't abort.
        // bgfx will use an invalid handle and rendering may glitch, but won't crash.
        if (_code == bgfx::Fatal::InvalidShader)
        {
            Rtt_LogException("BGFX: Shader compilation failed — skipping (app will not crash)\n");
            return; // Don't abort
        }

        // Other fatal errors: still abort (truly unrecoverable)
        abort();
    }
    virtual void traceVargs(const char* _filePath, uint16_t _line,
                            const char* _format, va_list _argList) override
    {
        // Only log on debug builds
        char buf[2048];
        vsnprintf(buf, sizeof(buf), _format, _argList);
        fprintf(stderr, "BGFX TRACE [%s:%d]: %s", _filePath, _line, buf);
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

BgfxRenderer::BgfxRenderer(Rtt_Allocator* allocator)
:   Super(allocator),
    fCaps(),
    fCapsInitialized(false),
    fBgfxInitialized(false),
    fStagingTexture( BGFX_INVALID_HANDLE ),
    fStagingW( 0 ),
    fStagingH( 0 )
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
#if defined(Rtt_ANDROID_ENV)
    init.type = bgfx::RendererType::OpenGLES;  // Android: force GLES (Vulkan not stable on old devices)
#else
    init.type = bgfx::RendererType::Count;  // Auto-select best backend
#endif
    init.resolution.width = width;
    init.resolution.height = height;
    init.platformData.nwh = nativeWindowHandle;

    // Platform-specific reset flags
    // SOLAR2D_MSAA env var overrides MSAA level for testing degradation paths
    const char* msaaEnv = getenv("SOLAR2D_MSAA");
    int msaaLevel = msaaEnv ? atoi(msaaEnv) : -1;

#if defined(Rtt_IPHONE_ENV) || defined(Rtt_ANDROID_ENV)
    // Mobile: MSAA X4 same as desktop, no FLIP_AFTER_RENDER
    // Use SOLAR2D_MSAA=0 or =2 to test lower quality on low-end devices
    init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4;
#else
    // Desktop
    init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4 | BGFX_RESET_FLIP_AFTER_RENDER;
#endif

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

    init.callback = &s_bgfxCallback;
    fBgfxInitialized = bgfx::init(init);

    if (fBgfxInitialized)
    {
        fprintf(stderr, "BGFX_INIT: renderer=%s nwh=%p w=%u h=%u\n",
                bgfx::getRendererName(bgfx::getRendererType()), nativeWindowHandle, width, height);
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
        // CRITICAL: Flush all pending rendering to ensure the FBO texture
        // is fully written before we blit from it. On iOS Metal, blitting
        // from a texture that's still an active render target crashes.
        // Submit a frame (skip present) to complete any pending FBO renders.
        bgfx::ViewId fboViewId = bgfxFbo->GetViewId();
        bgfx::setViewFrameBuffer( fboViewId, BGFX_INVALID_HANDLE );
        bgfx::touch( fboViewId );
        bgfx::setSkipPresent( true );
        bgfx::frame();
        bgfx::setSkipPresent( false );

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
            bgfx::setSkipPresent( true );
            uint32_t currentFrame = bgfx::frame();

            // Wait for readback with timeout to prevent infinite loop
            uint32_t maxAttempts = 100;
            uint32_t attempts = 0;
            while( currentFrame < readyFrame && attempts < maxAttempts )
            {
                currentFrame = bgfx::frame();
                attempts++;
            }
            bgfx::setSkipPresent( false );

            if( attempts >= maxAttempts )
            {
                fprintf( stderr, "BGFX CaptureFrameBuffer: readback timeout after %u frames\n", attempts );
            }
        }
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

GPUResource*
BgfxRenderer::Create(const CPUResource* resource)
{
    switch (resource->GetType())
    {
        case CPUResource::kFrameBufferObject:
            return new BgfxFrameBufferObject;

        case CPUResource::kGeometry:
            return new BgfxGeometry;

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
