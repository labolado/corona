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
#include "Renderer/Rtt_BgfxContext.h"

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

	// bgfx renders to its own CAMetalLayer; the GL path repaints via NSView.
	static int sBgfxMode = -1;
	if (sBgfxMode < 0)
	{
		const char* backend = getenv("SOLAR2D_BACKEND");
		sBgfxMode = (backend && strcmp(backend, "bgfx") == 0) ? 1 : 0;
	}

	if (sBgfxMode)
	{
		// Issue #027 (A part): when a second Solar2D Runtime is attached
		// (Welcome + Game), bgfx requires every attached swap chain to be
		// submitted each frame — its triple-buffered drawable rotation
		// otherwise clears the unsubmitted chain and the user sees white.
		// Static UI (Welcome's Projects browser) keeps Scene::IsValid()
		// returning true forever, so the original gate would skip Render()
		// and starve that swap chain.
		//
		// Force every runtime to invalidate + render each tick while the
		// process holds two or more bgfx attachments. The companion B''
		// change in BgfxCommandBuffer::Execute keeps frame() per-tick to
		// exactly one (only primary calls bgfx::frame), so we don't
		// regress to the v2-dryrun "both windows white" failure caused by
		// double frame() per tick fighting over Metal drawables.
		bool multiRuntime = ( BgfxContext::Instance().RefCount() >= 2 );
		if ( multiRuntime )
		{
			fRuntime->GetDisplay().Invalidate();
			fRuntime->Render();
		}
		else if ( ! fRuntime->GetDisplay().GetScene().IsValid() )
		{
			fRuntime->Render();
		}
	}
	else
	{
		if ( ! fRuntime->GetDisplay().GetScene().IsValid() )
		{
			[fView setNeedsDisplay:YES];
		}
	}
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

