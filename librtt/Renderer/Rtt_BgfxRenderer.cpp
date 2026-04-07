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

// ----------------------------------------------------------------------------

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
    init.type = bgfx::RendererType::Count;  // Auto-select best backend
    init.resolution.width = width;
    init.resolution.height = height;
    init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4 | BGFX_RESET_FLIP_AFTER_RENDER;
    init.platformData.nwh = nativeWindowHandle;

    fBgfxInitialized = bgfx::init(init);

    if (fBgfxInitialized)
    {
        fprintf(stderr, "BGFX_INIT: renderer=%s nwh=%p w=%u h=%u\n",
                bgfx::getRendererName(bgfx::getRendererType()), nativeWindowHandle, width, height);
        bgfx::setDebug(BGFX_DEBUG_NONE);
        // Set default view clear state (view 200 = screen, FBO views use 1-199)
        bgfx::setViewClear(200, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::setViewRect(200, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
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

            while( currentFrame < readyFrame )
            {
                currentFrame = bgfx::frame();
            }
            bgfx::setSkipPresent( false );
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
        // Mac desktop bitmap (kBGRA) uses kCGImageAlphaPremultipliedFirst with
        // Big-endian byte order → memory is [A, R, G, B].
        // GL reads with GL_BGRA + GL_UNSIGNED_INT_8_8_8_8 to match this layout.
        //
        // TODO(cross-platform): Windows kBGRA is standard LE [B,G,R,A] — needs
        // R↔B swap instead of byte rotation. iOS/Android use kRGBA (GLES) and
        // don't need conversion. Add #if platform guards when porting bgfx to
        // other desktop platforms.
        //
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
