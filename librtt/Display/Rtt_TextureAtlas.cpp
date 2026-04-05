////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Display/Rtt_TextureAtlas.h"

#include "Display/Rtt_BufferBitmap.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_PlatformBitmap.h"
#include "Display/Rtt_TextureFactory.h"
#include "Display/Rtt_TextureResource.h"
#include "Rtt_MPlatform.h"
#include "Rtt_Runtime.h"
#include "Core/Rtt_String.h"

#include <algorithm>
#include <cstring>
#include <vector>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

TextureAtlas::TextureAtlas( Rtt_Allocator* allocator )
:	fAllocator( allocator ),
	fTexture(),
	fFrames( allocator ),
	fAtlasWidth( 0 ),
	fAtlasHeight( 0 )
{
}

TextureAtlas::~TextureAtlas()
{
}

// Shelf packing algorithm: simple row-based bin packing.
// Sorts rects by height descending, places them left-to-right in shelves.
bool
TextureAtlas::ShelfPack( PackRect* rects, int count, S32 maxSize, int padding, S32& outW, S32& outH )
{
	if ( count <= 0 ) return false;

	// Sort by height descending (stable sort preserving order for equal heights)
	std::vector<int> order( count );
	for ( int i = 0; i < count; i++ ) order[i] = i;
	std::sort( order.begin(), order.end(), [&]( int a, int b ) {
		return rects[a].h > rects[b].h;
	});

	S32 shelfX = padding;
	S32 shelfY = padding;
	S32 shelfH = 0;
	S32 usedW = 0;

	for ( int i = 0; i < count; i++ )
	{
		PackRect& r = rects[order[i]];
		S32 pw = r.w + padding;

		// Check if rect fits in current shelf
		if ( shelfX + r.w + padding > maxSize )
		{
			// Start new shelf
			shelfY += shelfH + padding;
			shelfX = padding;
			shelfH = 0;
		}

		// Check if fits vertically
		if ( shelfY + r.h + padding > maxSize )
		{
			return false; // doesn't fit
		}

		r.x = shelfX;
		r.y = shelfY;

		shelfX += pw;
		if ( r.h > shelfH ) shelfH = r.h;
		if ( shelfX > usedW ) usedW = shelfX;
	}

	outW = usedW + padding;
	outH = shelfY + shelfH + padding;

	// Round up to power of two for GPU compatibility
	auto nextPow2 = []( S32 v ) -> S32 {
		v--;
		v |= v >> 1; v |= v >> 2; v |= v >> 4;
		v |= v >> 8; v |= v >> 16;
		v++;
		return v < 1 ? 1 : v;
	};
	outW = nextPow2( outW );
	outH = nextPow2( outH );

	if ( outW > maxSize || outH > maxSize )
	{
		return false;
	}

	return true;
}

static size_t
BytesPerPixel( PlatformBitmap::Format format )
{
	return PlatformBitmap::BytesPerPixel( format );
}

TextureAtlas*
TextureAtlas::Create(
	Rtt_Allocator* allocator,
	Display& display,
	const char** imageNames,
	int numImages,
	MPlatform::Directory baseDir,
	int maxSize,
	int padding )
{
	if ( numImages <= 0 || !imageNames ) return NULL;

	Runtime& runtime = display.GetRuntime();
	const MPlatform& platform = runtime.Platform();
	TextureFactory& factory = display.GetTextureFactory();

	// Load all bitmaps
	struct BitmapEntry {
		PlatformBitmap* bitmap;
		const char* name;
		U32 w, h;
	};
	std::vector<BitmapEntry> bitmaps;
	bitmaps.reserve( numImages );

	for ( int i = 0; i < numImages; i++ )
	{
		String filePath( allocator );
		platform.PathForFile( imageNames[i], baseDir, MPlatform::kTestFileExists, filePath );

		if ( filePath.IsEmpty() )
		{
			Rtt_TRACE_SIM( ( "WARNING: TextureAtlas: could not find '%s'\n", imageNames[i] ) );
			continue;
		}

		PlatformBitmap* bmp = platform.CreateBitmap( filePath.GetString(), false );
		if ( !bmp )
		{
			Rtt_TRACE_SIM( ( "WARNING: TextureAtlas: failed to load '%s'\n", imageNames[i] ) );
			continue;
		}

		BitmapEntry entry;
		entry.bitmap = bmp;
		entry.name = imageNames[i];
		entry.w = bmp->Width();
		entry.h = bmp->Height();
		bitmaps.push_back( entry );
	}

	if ( bitmaps.empty() )
	{
		return NULL;
	}

	// Set up packing rects
	int count = (int)bitmaps.size();
	std::vector<PackRect> packRects( count );
	for ( int i = 0; i < count; i++ )
	{
		packRects[i].w = bitmaps[i].w;
		packRects[i].h = bitmaps[i].h;
		packRects[i].origIndex = i;
	}

	S32 atlasW = 0, atlasH = 0;
	if ( !ShelfPack( packRects.data(), count, maxSize, padding, atlasW, atlasH ) )
	{
		Rtt_TRACE_SIM( ( "ERROR: TextureAtlas: images don't fit in %dx%d\n", maxSize, maxSize ) );
		for ( auto& e : bitmaps ) Rtt_DELETE( e.bitmap );
		return NULL;
	}

	// Create the combined bitmap (RGBA)
	BufferBitmap* atlasBitmap = Rtt_NEW( allocator,
		BufferBitmap( allocator, atlasW, atlasH, PlatformBitmap::kRGBA, PlatformBitmap::kUp ) );

	// Clear to transparent black
	U8* atlasPixels = (U8*)atlasBitmap->WriteAccess();
	memset( atlasPixels, 0, atlasW * atlasH * 4 );

	// Blit each image into the atlas
	for ( int i = 0; i < count; i++ )
	{
		PackRect& pr = packRects[i];
		BitmapEntry& entry = bitmaps[pr.origIndex];
		PlatformBitmap* srcBmp = entry.bitmap;

		const void* srcBits = srcBmp->GetBits( allocator );
		if ( !srcBits ) continue;

		U32 srcW = srcBmp->Width();
		U32 srcH = srcBmp->Height();
		size_t srcBpp = PlatformBitmap::BytesPerPixel( srcBmp->GetFormat() );
		size_t dstBpp = 4; // RGBA

		for ( U32 row = 0; row < srcH; row++ )
		{
			const U8* srcRow = (const U8*)srcBits + row * srcW * srcBpp;
			U8* dstRow = atlasPixels + ( (pr.y + row) * atlasW + pr.x ) * dstBpp;

			if ( srcBmp->GetFormat() == PlatformBitmap::kRGBA )
			{
				memcpy( dstRow, srcRow, srcW * 4 );
			}
			else if ( srcBmp->GetFormat() == PlatformBitmap::kRGB )
			{
				for ( U32 col = 0; col < srcW; col++ )
				{
					dstRow[col*4+0] = srcRow[col*3+0];
					dstRow[col*4+1] = srcRow[col*3+1];
					dstRow[col*4+2] = srcRow[col*3+2];
					dstRow[col*4+3] = 0xFF;
				}
			}
			else if ( srcBmp->GetFormat() == PlatformBitmap::kBGRA )
			{
				for ( U32 col = 0; col < srcW; col++ )
				{
					dstRow[col*4+0] = srcRow[col*4+2]; // R
					dstRow[col*4+1] = srcRow[col*4+1]; // G
					dstRow[col*4+2] = srcRow[col*4+0]; // B
					dstRow[col*4+3] = srcRow[col*4+3]; // A
				}
			}
			else if ( srcBmp->GetFormat() == PlatformBitmap::kMask )
			{
				for ( U32 col = 0; col < srcW; col++ )
				{
					U8 v = srcRow[col];
					dstRow[col*4+0] = v;
					dstRow[col*4+1] = v;
					dstRow[col*4+2] = v;
					dstRow[col*4+3] = 0xFF;
				}
			}
			else
			{
				// Fallback: copy as many bytes as possible
				size_t copyBytes = srcW * (srcBpp < dstBpp ? srcBpp : dstBpp);
				memcpy( dstRow, srcRow, copyBytes );
			}
		}

		srcBmp->FreeBits();
	}

	// Create texture resource from the combined bitmap
	SharedPtr< TextureResource > texture = factory.FindOrCreate( atlasBitmap, false );
	if ( texture.IsNull() )
	{
		for ( auto& e : bitmaps ) Rtt_DELETE( e.bitmap );
		return NULL;
	}

	// Build the atlas object
	TextureAtlas* atlas = Rtt_NEW( allocator, TextureAtlas( allocator ) );
	atlas->fTexture = texture;
	atlas->fAtlasWidth = atlasW;
	atlas->fAtlasHeight = atlasH;

	Real invW = Rtt_RealDiv( Rtt_REAL_1, Rtt_IntToReal( atlasW ) );
	Real invH = Rtt_RealDiv( Rtt_REAL_1, Rtt_IntToReal( atlasH ) );

	// Build frames
	for ( int i = 0; i < count; i++ )
	{
		PackRect& pr = packRects[i];
		BitmapEntry& entry = bitmaps[pr.origIndex];

		Frame frame;
		frame.name = entry.name;
		frame.x = pr.x;
		frame.y = pr.y;
		frame.w = entry.w;
		frame.h = entry.h;
		frame.srcW = entry.w;
		frame.srcH = entry.h;
		frame.u0 = Rtt_RealMul( Rtt_IntToReal( pr.x ), invW );
		frame.v0 = Rtt_RealMul( Rtt_IntToReal( pr.y ), invH );
		frame.u1 = Rtt_RealMul( Rtt_IntToReal( pr.x + (S32)entry.w ), invW );
		frame.v1 = Rtt_RealMul( Rtt_IntToReal( pr.y + (S32)entry.h ), invH );

		atlas->fFrames.Append( frame );
	}

	// Clean up source bitmaps (the atlas bitmap is now owned by the texture resource)
	for ( auto& e : bitmaps ) Rtt_DELETE( e.bitmap );

	return atlas;
}

const TextureAtlas::Frame*
TextureAtlas::GetFrame( const char* name ) const
{
	if ( !name ) return NULL;

	for ( S32 i = 0, iMax = fFrames.Length(); i < iMax; i++ )
	{
		if ( fFrames[i].name == name )
		{
			return &fFrames[i];
		}
	}

	return NULL;
}

bool
TextureAtlas::HasFrame( const char* name ) const
{
	return GetFrame( name ) != NULL;
}

// ----------------------------------------------------------------------------
// AtlasBitmapPaint
// ----------------------------------------------------------------------------

AtlasBitmapPaint::AtlasBitmapPaint(
	const SharedPtr< TextureResource >& resource,
	Real u0, Real v0, Real u1, Real v1 )
:	BitmapPaint( resource ),
	fU0( u0 ), fV0( v0 ), fU1( u1 ), fV1( v1 )
{
}

void
AtlasBitmapPaint::ApplyPaintUVTransformations( ArrayVertex2& vertices ) const
{
	// Remap UVs from [0,1] to the atlas sub-region [u0,v0]-[u1,v1]
	Vertex2* verts = vertices.WriteAccess();
	Real du = fU1 - fU0;
	Real dv = fV1 - fV0;
	for ( int i = 0, iMax = vertices.Length(); i < iMax; i++ )
	{
		verts[i].x = fU0 + du * verts[i].x;
		verts[i].y = fV0 + dv * verts[i].y;
	}
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
