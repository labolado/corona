//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Renderer/Rtt_BgfxExports.h"
#include "Renderer/Rtt_BgfxRenderer.h"
#include "Core/Rtt_Assert.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

Renderer*
BgfxExports::CreateBgfxRenderer(
    Rtt_Allocator* allocator,
    void* nativeWindowHandle,
    U32 width,
    U32 height
)
{
    Rtt_ASSERT(allocator != NULL);
    Rtt_ASSERT(nativeWindowHandle != NULL);
    Rtt_ASSERT(width > 0);
    Rtt_ASSERT(height > 0);

    BgfxRenderer* renderer = Rtt_NEW(allocator, BgfxRenderer(allocator));

    if (renderer)
    {
        bool initialized = renderer->InitializeBgfx(nativeWindowHandle, width, height);

        if (!initialized)
        {
            Rtt_DELETE(renderer);
            renderer = NULL;
            Rtt_LogException("BgfxExports: Failed to initialize bgfx renderer");
        }
    }

    return renderer;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
