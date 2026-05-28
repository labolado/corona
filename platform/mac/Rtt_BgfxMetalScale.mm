//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#import <AppKit/AppKit.h>

namespace Rtt {

double
Rtt_GetBgfxMetalLayerScale(void* nsViewHandle)
{
    NSView* view = (__bridge NSView*)nsViewHandle;
    if (view && view.window)
        return (double)view.window.backingScaleFactor;
    return 1.0;
}

void
Rtt_SetBgfxMetalLayerScale(void* nsViewHandle, double scale)
{
    NSView* view = (__bridge NSView*)nsViewHandle;
    if (view && view.layer)
        view.layer.contentsScale = (CGFloat)scale;
}

} // namespace Rtt
