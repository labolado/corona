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

	private:
		bgfx::FrameBufferHandle fHandle;
		bgfx::TextureHandle fTextureHandle;
		bgfx::ViewId fViewId;
		static bgfx::ViewId sNextViewId;
		static bgfx::ViewId AllocateViewId();
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxFrameBufferObject_H__
