//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxContext_H__
#define _Rtt_BgfxContext_H__

#include "Core/Rtt_Build.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace Rtt
{

// ----------------------------------------------------------------------------

// Process-wide bgfx session manager.
//
// WHY this exists
//   bgfx::init() is a process-level singleton; a second call fails while a
//   session is active. Solar2D's macOS Simulator runs multiple Runtimes in
//   one process (Welcome + Game + extensions), each owning a CAMetalLayer
//   and each previously calling bgfx::init() through its own BgfxRenderer.
//   That broke Issue #027 — the second init forced bgfx::shutdown() which
//   orphaned the first window's swap chain.
//
// WHAT it does
//   - Refcounts the one bgfx::init()/bgfx::shutdown() pair for the whole
//     process.
//   - First AttachWindow() call becomes the "primary": bgfx::init() binds
//     the main swap chain to that window's nwh. screenViewId = 200.
//   - Subsequent AttachWindow() calls become "secondary": they request a
//     dedicated swap chain via bgfx::createFrameBuffer(nwh, w, h). Each
//     gets a unique screenViewId (201, 202, ...). bgfx's Metal / D3D11 /
//     D3D12 / Vulkan / WebGPU backends all advertise BGFX_CAPS_SWAP_CHAIN;
//     GLES on some mobile drivers may not, in which case secondary attach
//     fails and the caller must fall back.
//   - DetachWindow() tears down correctly: secondaries destroy their FB
//     and flush two frames (per bgfx example 22-windows); primaries drop
//     the refcount and shutdown when it hits zero.
//
// WHAT it does NOT do (scope)
//   - Does not call bgfx submit/touch/frame itself; the caller (BgfxRenderer)
//     continues to drive per-Runtime frame() on its own view range.
//   - Does not support re-binding primary to another nwh mid-session; if
//     primary detaches while secondaries are still alive, the main swap chain
//     is orphaned (log warning — practical UX never triggers this because
//     Welcome outlives the extension/game Runtimes).
class BgfxContext
{
	public:
		struct AttachResult
		{
			bool                    primary;      // true for the first attach
			bgfx::FrameBufferHandle fbHandle;     // valid only when !primary
			bgfx::ViewId            screenViewId; // unique per attach (200, 201, ...)
			bool                    success;      // false on failure
		};

		static BgfxContext& Instance()
		{
			static BgfxContext sInstance;
			return sInstance;
		}

		// First call: performs bgfx::init(initCfg) on this window's nwh.
		// Subsequent: performs bgfx::createFrameBuffer(nwh, w, h, BGRA8, D24S8).
		// Returns {primary, fbHandle, screenViewId, success}.
		AttachResult AttachWindow( void* nwh, uint32_t width, uint32_t height,
		                           const bgfx::Init& initCfg )
		{
			AttachResult r;
			r.primary = false;
			r.fbHandle = BGFX_INVALID_HANDLE;
			r.screenViewId = 0;
			r.success = false;

			if ( NULL == nwh || 0 == width || 0 == height )
			{
				Rtt_LogException( "BgfxContext::AttachWindow: invalid args (nwh=%p w=%u h=%u)",
				                  nwh, width, height );
				return r;
			}

			if ( ! fInitialized )
			{
				Rtt_LogException( "BgfxContext: first attach, calling bgfx::init (primary nwh=%p)",
				                  nwh );

				bgfx::Init init = initCfg;
				init.platformData.nwh = nwh;
				init.resolution.width = width;
				init.resolution.height = height;

				bool ok = bgfx::init( init );
				if ( ! ok )
				{
					Rtt_LogException( "BgfxContext: bgfx::init FAILED on primary attach" );
					return r;
				}

				fInitialized = true;
				fPrimaryNwh = nwh;
				fCaps = bgfx::getCaps();
				r.primary = true;
				r.screenViewId = fNextScreenViewId++;  // primary = 200
				r.success = true;
				++fRefCount;

				Rtt_LogException( "BgfxContext: primary attached, screenViewId=%d refCount=%d renderer=%s",
				                  (int) r.screenViewId,
				                  fRefCount,
				                  fCaps ? bgfx::getRendererName( fCaps->rendererType ) : "null" );
				return r;
			}

			// Subsequent attach — secondary swap chain via createFrameBuffer(nwh).
			if ( NULL == fCaps ||
			     0 == ( fCaps->supported & BGFX_CAPS_SWAP_CHAIN ) )
			{
				Rtt_LogException( "BgfxContext: secondary attach FAILED — BGFX_CAPS_SWAP_CHAIN not supported on renderer=%s",
				                  fCaps ? bgfx::getRendererName( fCaps->rendererType ) : "null" );
				return r;
			}

			Rtt_LogException( "BgfxContext: secondary attach — createFrameBuffer(nwh=%p w=%u h=%u)",
			                  nwh, width, height );

			bgfx::FrameBufferHandle fbh = bgfx::createFrameBuffer(
				nwh,
				static_cast< uint16_t >( width ),
				static_cast< uint16_t >( height ),
				bgfx::TextureFormat::BGRA8,
				bgfx::TextureFormat::D24S8 );

			if ( ! bgfx::isValid( fbh ) )
			{
				Rtt_LogException( "BgfxContext: createFrameBuffer returned invalid handle" );
				return r;
			}

			r.primary = false;
			r.fbHandle = fbh;
			r.screenViewId = fNextScreenViewId++;  // 201, 202, ...
			r.success = true;
			++fRefCount;

			Rtt_LogException( "BgfxContext: secondary attached, fb=%d screenViewId=%d refCount=%d",
			                  (int) fbh.idx, (int) r.screenViewId, fRefCount );
			return r;
		}

		// Releases resources held by an attach:
		//   - secondary: destroy fbHandle with two frame() flushes.
		//   - primary: decrement refCount; if it hits 0, bgfx::shutdown().
		void DetachWindow( bool primary,
		                   bgfx::FrameBufferHandle fbh,
		                   bgfx::ViewId screenViewId )
		{
			if ( ! fInitialized )
			{
				return;
			}

			if ( primary )
			{
				int before = fRefCount;
				--fRefCount;
				Rtt_LogException( "BgfxContext: primary detach, refCount %d -> %d",
				                  before, fRefCount );
				if ( fRefCount <= 0 )
				{
					Rtt_LogException( "BgfxContext: refCount reached 0, calling bgfx::shutdown" );
					bgfx::shutdown();
					fInitialized = false;
					fPrimaryNwh = NULL;
					fCaps = NULL;
					fRefCount = 0;
					fNextScreenViewId = 200;
				}
				else
				{
					// Primary detached while secondaries are still alive. The
					// bgfx session is bound to the original primary nwh, so the
					// main swap chain is now orphaned — but the secondaries can
					// keep rendering via their createFrameBuffer swap chains.
					// This is an edge case that Solar2D's UX does not hit in
					// practice (Welcome always outlives extensions).
					Rtt_LogException( "BgfxContext: WARNING primary detached with %d secondaries still attached; main swap chain is now orphaned",
					                  fRefCount );
				}
				return;
			}

			// Secondary detach
			if ( bgfx::isValid( fbh ) )
			{
				// Unbind the screen view first so no stale binding survives destroy.
				bgfx::setViewFrameBuffer( screenViewId, BGFX_INVALID_HANDLE );
				bgfx::destroy( fbh );
				// Per bgfx example 22-windows: flush two frames so destroy
				// reaches the render thread before the native window goes away.
				bgfx::frame();
				bgfx::frame();
			}
			--fRefCount;
			Rtt_LogException( "BgfxContext: secondary detach, fb=%d refCount=%d",
			                  bgfx::isValid( fbh ) ? (int) fbh.idx : -1,
			                  fRefCount );
		}

		// Recreate a secondary swap chain after window resize.
		// Destroys oldFbh, flushes one frame, creates at the new size.
		bgfx::FrameBufferHandle ResizeSecondary( bgfx::FrameBufferHandle oldFbh,
		                                        void* nwh,
		                                        uint32_t width,
		                                        uint32_t height )
		{
			if ( bgfx::isValid( oldFbh ) )
			{
				bgfx::destroy( oldFbh );
				bgfx::frame();  // flush destroy before recreate
			}
			bgfx::FrameBufferHandle invalid = BGFX_INVALID_HANDLE;
			if ( NULL == nwh || 0 == width || 0 == height )
			{
				return invalid;
			}
			return bgfx::createFrameBuffer(
				nwh,
				static_cast< uint16_t >( width ),
				static_cast< uint16_t >( height ),
				bgfx::TextureFormat::BGRA8,
				bgfx::TextureFormat::D24S8 );
		}

		bool              IsInitialized() const { return fInitialized; }
		int               RefCount()      const { return fRefCount; }
		const bgfx::Caps* GetCaps()       const { return fCaps; }

	private:
		BgfxContext()
		:   fInitialized( false ),
		    fRefCount( 0 ),
		    fPrimaryNwh( NULL ),
		    fNextScreenViewId( 200 ),
		    fCaps( NULL )
		{
		}
		~BgfxContext() {}
		BgfxContext( const BgfxContext& );
		BgfxContext& operator=( const BgfxContext& );

		bool              fInitialized;
		int               fRefCount;
		void*             fPrimaryNwh;
		bgfx::ViewId      fNextScreenViewId;
		const bgfx::Caps* fCaps;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

#endif // _Rtt_BgfxContext_H__
