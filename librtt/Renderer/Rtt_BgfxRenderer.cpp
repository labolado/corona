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
#include "Core/Rtt_Assert.h"
#include <stdio.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

BgfxRenderer::BgfxRenderer(Rtt_Allocator* allocator)
:   Super(allocator),
    fCaps(),
    fCapsInitialized(false),
    fBgfxInitialized(false)
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
