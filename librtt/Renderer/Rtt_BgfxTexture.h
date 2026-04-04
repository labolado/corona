//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxTexture_H__
#define _Rtt_BgfxTexture_H__

#include "Renderer/Rtt_GPUResource.h"
#include "Renderer/Rtt_Texture.h"
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

class BgfxTexture : public GPUResource
{
	public:
		typedef GPUResource Super;
		typedef BgfxTexture Self;

	public:
		BgfxTexture();
		virtual ~BgfxTexture();

		virtual void Create( CPUResource* resource );
		virtual void Update( CPUResource* resource );
		virtual void Destroy();
		virtual void Bind( U32 unit );

		bgfx::TextureHandle GetHandle() const { return fHandle; }
		U32 GetBoundUnit() const { return fBoundUnit; }
		S32 GetCachedFormat() const { return fCachedFormat; }

	private:
		static bgfx::TextureFormat::Enum ConvertFormat( Texture::Format format );
		static uint64_t ConvertFilter( Texture::Filter filter );
		static uint64_t ConvertWrap( Texture::Wrap wrap );

	private:
		bgfx::TextureHandle fHandle;
		U32 fBoundUnit;
		S32 fCachedFormat;
		U32 fCachedWidth;
		U32 fCachedHeight;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxTexture_H__
