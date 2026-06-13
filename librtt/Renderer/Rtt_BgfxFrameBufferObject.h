//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxFrameBufferObject_H__
#define _Rtt_BgfxFrameBufferObject_H__

#include "Renderer/Rtt_GPUResource.h"
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

class BgfxFrameBufferObject : public GPUResource
{
	public:
		typedef GPUResource Super;
		typedef BgfxFrameBufferObject Self;

	public:
		BgfxFrameBufferObject();
		virtual ~BgfxFrameBufferObject();

		virtual void Create( CPUResource* resource );
		virtual void Update( CPUResource* resource );
		virtual void Destroy();
		virtual void Bind( bool asDrawBuffer );

		bgfx::FrameBufferHandle GetHandle() const { return fHandle; }
		bgfx::TextureHandle GetTextureHandle() const { return fTextureHandle; }
		bgfx::ViewId GetViewId() const { return fViewId; }
		bool IsActive() const { return fViewId != 0 && bgfx::isValid( fHandle ); }

		static bool HasFramebufferBlit( bool* canScale );
		static void Blit( 
			bgfx::ViewId dstView,
			bgfx::TextureHandle dstTexture,
			U16 dstX,
			U16 dstY,
			bgfx::TextureHandle srcTexture,
			U16 srcX,
			U16 srcY,
			U16 width,
			U16 height
		);
		static void ResetViewIdAllocator();

	private:
		bgfx::FrameBufferHandle fHandle;
		bgfx::TextureHandle fTextureHandle;
		bgfx::ViewId fViewId;
		static bgfx::ViewId sNextViewId;
		static bgfx::ViewId AllocateViewId();
		static void ReleaseViewId( bgfx::ViewId viewId );
		enum { kFirstViewId = 1, kDefaultViewId = 200, kMaxViewId = 255, kMaxFreeViewIds = kMaxViewId - kFirstViewId + 1 };
		static bgfx::ViewId sFreeViewIds[kMaxFreeViewIds];
		static unsigned int sFreeViewIdCount;
		static unsigned int sLiveCount;
		static unsigned int sPeakLiveCount;
		static unsigned int sCreateCount;
		static unsigned int sDestroyCount;
		static unsigned int sCreateFailCount;
		static unsigned int sExhaustCount;
		static unsigned int sDegradedCount;
		static bool sLoggedDegraded;
		static bool IsDiagEnabled();
		static void RecordDegradedFbo( const char* reason );
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxFrameBufferObject_H__
