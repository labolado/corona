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
		case Texture::kAlpha:			return bgfx::TextureFormat::RGBA8; // Expanded to RGBA(255,255,255,a) on CPU
		case Texture::kLuminance:		return bgfx::TextureFormat::RGBA8; // Expanded to RGBA(v,v,v,255) on CPU
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

	// Debug: log texture creation
	{
		static int sTexDbg = 0;
		if (sTexDbg < 20)
		{
			Rtt_LogException("BGFX_TEX_CREATE[%d]: fmt=%d w=%u h=%u data=%p\n",
				sTexDbg, (int)texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->GetData());
			sTexDbg++;
		}
	}

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
	const bgfx::Memory* mem = NULL;
	if( data && dataSize > 0 )
	{
		// Alpha textures (font glyphs): expand to RGBA(255,255,255,alpha)
		// Luminance textures: expand to RGBA(R,R,R,255) to match GL_LUMINANCE behavior
		if( texture->GetFormat() == Texture::kAlpha )
		{
			uint32_t pixelCount = w * h;
			const bgfx::Memory* rgbaMem = bgfx::alloc( pixelCount * 4 );
			U8* dst = rgbaMem->data;
			for( uint32_t i = 0; i < pixelCount; ++i )
			{
				dst[i * 4 + 0] = 255;
				dst[i * 4 + 1] = 255;
				dst[i * 4 + 2] = 255;
				dst[i * 4 + 3] = data[i];
			}
			mem = rgbaMem;
			format = bgfx::TextureFormat::RGBA8;
		}
		else if( texture->GetFormat() == Texture::kLuminance )
		{
			// GL_LUMINANCE maps value to (L,L,L,1) per OpenGL spec
			// Alpha must be 255 (opaque) so that fill alpha = 1.0 and only the
			// mask multiply controls text transparency (matching GL backend behavior)
			uint32_t pixelCount = w * h;
			const bgfx::Memory* rgbaMem = bgfx::alloc( pixelCount * 4 );
			U8* dst = rgbaMem->data;
			for( uint32_t i = 0; i < pixelCount; ++i )
			{
				U8 v = data[i];
				dst[i * 4 + 0] = v;
				dst[i * 4 + 1] = v;
				dst[i * 4 + 2] = v;
				dst[i * 4 + 3] = 255; // alpha = 1.0, matching GL_LUMINANCE (L,L,L,1)
			}
			mem = rgbaMem;
			format = bgfx::TextureFormat::RGBA8;
		}
		else
		{
			mem = bgfx::copy( data, dataSize );
		}
	}
	else
	{
		// No data provided - create a white 1x1 fallback texture
		// Solar2D color shapes rely on vertex color * white texture
		uint32_t white = 0xFFFFFFFF;
		mem = bgfx::copy( &white, 4 );
		format = bgfx::TextureFormat::RGBA8;
		fHandle = bgfx::createTexture2D( 1, 1, false, 1, format, flags, mem );
		fCachedFormat = static_cast<S32>( Texture::kRGBA );
		fCachedWidth = 1;
		fCachedHeight = 1;
		return;
	}
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

		// Alpha textures (font glyphs): expand to RGBA(255,255,255,alpha)
		// Luminance textures: expand to RGBA(R,R,R,255) to match GL_LUMINANCE behavior
		const bgfx::Memory* expandedMem = NULL;
		bgfx::TextureFormat::Enum actualFormat;
		if( format == Texture::kAlpha )
		{
			uint32_t pixelCount = w * h;
			expandedMem = bgfx::alloc( pixelCount * 4 );
			U8* dst = expandedMem->data;
			for( uint32_t i = 0; i < pixelCount; ++i )
			{
				dst[i * 4 + 0] = 255;
				dst[i * 4 + 1] = 255;
				dst[i * 4 + 2] = 255;
				dst[i * 4 + 3] = data[i];
			}
			actualFormat = bgfx::TextureFormat::RGBA8;
		}
		else if( format == Texture::kLuminance )
		{
			// GL_LUMINANCE maps value to (L,L,L,1) per OpenGL spec
			uint32_t pixelCount = w * h;
			expandedMem = bgfx::alloc( pixelCount * 4 );
			U8* dst = expandedMem->data;
			for( uint32_t i = 0; i < pixelCount; ++i )
			{
				U8 v = data[i];
				dst[i * 4 + 0] = v;
				dst[i * 4 + 1] = v;
				dst[i * 4 + 2] = v;
				dst[i * 4 + 3] = 255; // alpha = 1.0, matching GL_LUMINANCE (L,L,L,1)
			}
			actualFormat = bgfx::TextureFormat::RGBA8;
		}
		else
		{
			actualFormat = ConvertFormat( format );
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

			const bgfx::Memory* mem = expandedMem ? expandedMem : bgfx::copy( data, dataSize );
			fHandle = bgfx::createTexture2D(
				static_cast<uint16_t>( w ),
				static_cast<uint16_t>( h ),
				false,
				1,
				actualFormat,
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
			const bgfx::Memory* mem = expandedMem ? expandedMem : bgfx::copy( data, dataSize );
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
