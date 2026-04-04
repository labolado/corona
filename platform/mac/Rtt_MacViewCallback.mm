//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Rtt_MacViewCallback.h"

#include "Rtt_Runtime.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_Scene.h"

#import <AppKit/NSView.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

MacViewCallback::MacViewCallback( NSView *view )
:	fView( view ),
	fRuntime( NULL )
{
}

void
MacViewCallback::operator()()
{
	Rtt_ASSERT( fRuntime );
	(*fRuntime)();
	if ( ! fRuntime->GetDisplay().GetScene().IsValid() )
	{
		// bgfx renders to its own CAMetalLayer, bypass NSView drawing system
		static int sBgfxMode = -1;
		if (sBgfxMode < 0)
		{
			const char* backend = getenv("SOLAR2D_BACKEND");
			sBgfxMode = (backend && strcmp(backend, "bgfx") == 0) ? 1 : 0;
		}
		if (sBgfxMode)
		{
			fRuntime->Render();
		}
		else
		{
			[fView setNeedsDisplay:YES];
		}
	}
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

