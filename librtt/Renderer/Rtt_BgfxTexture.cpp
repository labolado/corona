//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Config.h"

#include "Renderer/Rtt_BgfxTexture.h"

#include "Renderer/Rtt_Texture.h"
#include "Core/Rtt_Assert.h"

#include "Rtt_Profiling.h"

// ----------------------------------------------------------------------------

#define ENABLE_DEBUG_PRINT    0

#if ENABLE_DEBUG_PRINT
    #define DEBUG_PRINT( ... ) Rtt_LogException( __VA_ARGS__ );
#else
    #define DEBUG_PRINT( ... )
#endif

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

BgfxTexture::BgfxTexture()
:	fHandle( BGFX_INVALID_HANDLE ),
	fBoundUnit( 0 ),
	fCachedFormat( -1 ),
	fCachedWidth( 0 ),
	fCachedHeight( 0 )
{
}

BgfxTexture::~BgfxTexture()
{
	Destroy();
}

bgfx::TextureFormat::Enum
BgfxTexture::ConvertFormat( Texture::Format format )
{
	switch( format )
	{
		case Texture::kAlpha:			return bgfx::TextureFormat::A8;
		case Texture::kLuminance:		return bgfx::TextureFormat::R8;
		case Texture::kRGB:				return bgfx::TextureFormat::RGB8;
		case Texture::kRGBA:			return bgfx::TextureFormat::RGBA8;
		case Texture::kBGRA:			return bgfx::TextureFormat::BGRA8;
		case Texture::kARGB:			return bgfx::TextureFormat::BGRA8; // Note: may need swizzling in shader
		case Texture::kABGR:			return bgfx::TextureFormat::RGBA8; // Note: may need swizzling in shader
		case Texture::kLuminanceAlpha:	return bgfx::TextureFormat::RG8;
		default:
			Rtt_ASSERT_NOT_REACHED();
			return bgfx::TextureFormat::RGBA8;
	}
}

uint64_t
BgfxTexture::ConvertFilter( Texture::Filter filter )
{
	switch( filter )
	{
		case Texture::kNearest:			return BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT;
		case Texture::kLinear:			return 0; // Default is linear
		default:
			Rtt_ASSERT_NOT_REACHED();
			return 0;
	}
}

uint64_t
BgfxTexture::ConvertWrap( Texture::Wrap wrap )
{
	switch( wrap )
	{
		case Texture::kClampToEdge:		return BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		case Texture::kRepeat:			return 0; // Default is repeat
		case Texture::kMirroredRepeat:	return BGFX_SAMPLER_U_MIRROR | BGFX_SAMPLER_V_MIRROR;
		default:
			Rtt_ASSERT_NOT_REACHED();
			return 0;
	}
}

void
BgfxTexture::Create( CPUResource* resource )
{
	Rtt_ASSERT( CPUResource::kTexture == resource->GetType() || CPUResource::kVideoTexture == resource->GetType() );
	Texture* texture = static_cast<Texture*>( resource );

	SUMMED_TIMING( bgfxtc, "Bgfx Texture GPU Resource: Create" );

	// Destroy existing handle if any
	if( bgfx::isValid( fHandle ) )
	{
		bgfx::destroy( fHandle );
	}

	// Build sampler flags from filter and wrap modes
	uint64_t flags = BGFX_TEXTURE_NONE;
	flags |= ConvertFilter( texture->GetFilter() );
	flags |= ConvertWrap( texture->GetWrapX() );
	flags |= ConvertWrap( texture->GetWrapY() );

	// Convert texture format
	bgfx::TextureFormat::Enum format = ConvertFormat( texture->GetFormat() );

	const U32 w = texture->GetWidth();
	const U32 h = texture->GetHeight();
	const U8* data = texture->GetData();

	// Calculate data size for copy
	uint32_t dataSize = 0;
	switch( texture->GetFormat() )
	{
		case Texture::kAlpha:			dataSize = w * h * 1; break;
		case Texture::kLuminance:		dataSize = w * h * 1; break;
		case Texture::kRGB:				dataSize = w * h * 3; break;
		case Texture::kRGBA:			dataSize = w * h * 4; break;
		case Texture::kBGRA:			dataSize = w * h * 4; break;
		case Texture::kARGB:			dataSize = w * h * 4; break;
		case Texture::kABGR:			dataSize = w * h * 4; break;
		case Texture::kLuminanceAlpha:	dataSize = w * h * 2; break;
		default:						dataSize = w * h * 4; break;
	}

	// Create texture
	// Note: bgfx::copy() copies data to internal memory, so we can safely ReleaseData() after
	const bgfx::Memory* mem = data ? bgfx::copy( data, dataSize ) : NULL;
	fHandle = bgfx::createTexture2D( 
		static_cast<uint16_t>( w ), 
		static_cast<uint16_t>( h ), 
		false, // no mips
		1,     // num layers
		format,
		flags,
		mem
	);

	fCachedFormat = static_cast<S32>( texture->GetFormat() );
	fCachedWidth = w;
	fCachedHeight = h;

	texture->ReleaseData();

	DEBUG_PRINT( "%s : bgfx handle: %d\n",
					Rtt_FUNCTION,
					fHandle.idx );
}

void
BgfxTexture::Update( CPUResource* resource )
{
	Rtt_ASSERT( CPUResource::kTexture == resource->GetType() );
	Texture* texture = static_cast<Texture*>( resource );

	SUMMED_TIMING( bgfxtu, "Bgfx Texture GPU Resource: Update" );

	const U8* data = texture->GetData();
	if( data && bgfx::isValid( fHandle ) )
	{
		const U32 w = texture->GetWidth();
		const U32 h = texture->GetHeight();
		Texture::Format format = texture->GetFormat();

		// Calculate data size
		uint32_t dataSize = 0;
		switch( format )
		{
			case Texture::kAlpha:			dataSize = w * h * 1; break;
			case Texture::kLuminance:		dataSize = w * h * 1; break;
			case Texture::kRGB:				dataSize = w * h * 3; break;
			case Texture::kRGBA:			dataSize = w * h * 4; break;
			case Texture::kBGRA:			dataSize = w * h * 4; break;
			case Texture::kARGB:			dataSize = w * h * 4; break;
			case Texture::kABGR:			dataSize = w * h * 4; break;
			case Texture::kLuminanceAlpha:	dataSize = w * h * 2; break;
			default:						dataSize = w * h * 4; break;
		}

		// If dimensions or format changed, we need to recreate the texture
		if( format != fCachedFormat || w != fCachedWidth || h != fCachedHeight )
		{
			// Destroy and recreate
			bgfx::destroy( fHandle );
			
			uint64_t flags = BGFX_TEXTURE_NONE;
			flags |= ConvertFilter( texture->GetFilter() );
			flags |= ConvertWrap( texture->GetWrapX() );
			flags |= ConvertWrap( texture->GetWrapY() );
			
			bgfx::TextureFormat::Enum bgfxFormat = ConvertFormat( format );
			const bgfx::Memory* mem = bgfx::copy( data, dataSize );
			fHandle = bgfx::createTexture2D( 
				static_cast<uint16_t>( w ), 
				static_cast<uint16_t>( h ), 
				false, 
				1, 
				bgfxFormat,
				flags,
				mem
			);

			fCachedFormat = static_cast<S32>( format );
			fCachedWidth = w;
			fCachedHeight = h;
		}
		else
		{
			// Update existing texture
			const bgfx::Memory* mem = bgfx::copy( data, dataSize );
			bgfx::updateTexture2D( 
				fHandle, 
				0, // layer
				0, // mip
				0, // x
				0, // y
				static_cast<uint16_t>( w ), 
				static_cast<uint16_t>( h ), 
				mem
			);
		}
	}
	texture->ReleaseData();
}

void
BgfxTexture::Destroy()
{
	if( bgfx::isValid( fHandle ) )
	{
		bgfx::destroy( fHandle );
		fHandle = BGFX_INVALID_HANDLE;
	}

	DEBUG_PRINT( "%s : bgfx handle destroyed\n",
					Rtt_FUNCTION );
}

void
BgfxTexture::Bind( U32 unit )
{
	// bgfx does not use global texture binding like GL.
	// Instead, we record the unit here and the CommandBuffer will call
	// bgfx::setTexture() during Draw() using this handle.
	fBoundUnit = unit;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
