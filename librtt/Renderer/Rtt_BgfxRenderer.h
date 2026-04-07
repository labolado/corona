//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxRenderer_H__
#define _Rtt_BgfxRenderer_H__

#include "Renderer/Rtt_Renderer.h"
#include "Renderer/Rtt_FrameBufferObject.h"
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

struct Rtt_Allocator;

namespace Rtt
{

// ----------------------------------------------------------------------------

class GPUResource;
class CPUResource;
class RenderingStream;
class BufferBitmap;

// ----------------------------------------------------------------------------

class BgfxRenderer : public Renderer
{
public:
    typedef Renderer Super;
    typedef BgfxRenderer Self;

public:
    BgfxRenderer(Rtt_Allocator* allocator);
    virtual ~BgfxRenderer();

    // Initialize bgfx with the given native window handle and dimensions
    bool InitializeBgfx(void* nativeWindowHandle, U32 width, U32 height);

    // Shutdown bgfx
    void ShutdownBgfx();

    // Get renderer capabilities
    virtual const RendererCaps& GetCaps() const;
    virtual bool IsBgfx() const override { return true; }
    virtual void SetSkipPresent( bool skip ) override;

    // Read back the current FBO's content to CPU bitmap (for display.capture save-to-file)
    virtual void CaptureFrameBuffer( RenderingStream & stream, BufferBitmap & bitmap, S32 x_in_pixels, S32 y_in_pixels, S32 w_in_pixels, S32 h_in_pixels );
    virtual void EndCapture();

protected:
    // Create a bgfx GPU resource appropriate for the given CPUResource
    virtual GPUResource* Create(const CPUResource* resource);

private:
    void InitCaps();

private:
    mutable RendererCaps fCaps;
    mutable bool fCapsInitialized;
    bool fBgfxInitialized;

    // Staging texture for CaptureFrameBuffer readback
    bgfx::TextureHandle fStagingTexture;
    U32 fStagingW;
    U32 fStagingH;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxRenderer_H__
