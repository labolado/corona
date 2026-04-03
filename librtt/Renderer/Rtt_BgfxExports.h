//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxExports_H__
#define _Rtt_BgfxExports_H__

#include "Core/Rtt_Types.h"

// ----------------------------------------------------------------------------

struct Rtt_Allocator;

namespace Rtt
{

class Renderer;

// ----------------------------------------------------------------------------

class BgfxExports
{
public:
    // Create a bgfx renderer with the given native window handle and dimensions
    // This also initializes bgfx with the provided parameters
    static Renderer* CreateBgfxRenderer(
        Rtt_Allocator* allocator,
        void* nativeWindowHandle,
        U32 width,
        U32 height
    );
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxExports_H__
