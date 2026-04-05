////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_TextureAtlas_H__
#define _Rtt_TextureAtlas_H__

#include "Core/Rtt_Macros.h"
#include "Core/Rtt_Types.h"
#include "Core/Rtt_Real.h"
#include "Core/Rtt_Array.h"
#include "Core/Rtt_SharedPtr.h"

#include "Display/Rtt_BitmapPaint.h"

#include <string>

// ----------------------------------------------------------------------------

namespace Rtt
{

class Display;
class ImageFrame;
class ImageSheet;
class Runtime;
class TextureResource;

// ----------------------------------------------------------------------------

// TextureAtlas: packs multiple images into a single texture at runtime.
// Uses shelf-based packing for simplicity.
class TextureAtlas
{
	Rtt_CLASS_NO_COPIES( TextureAtlas )

	public:
		struct Frame
		{
			std::string name;   // e.g. "hero.png"
			S32 x, y, w, h;    // position in atlas texture (pixels)
			Real u0, v0, u1, v1; // UV coordinates
			S32 srcW, srcH;     // original image dimensions
		};

	public:
		// Factory: loads images, packs them, creates combined texture
		static TextureAtlas* Create(
			Rtt_Allocator* allocator,
			Display& display,
			const char** imageNames,
			int numImages,
			MPlatform::Directory baseDir,
			int maxSize,
			int padding
		);

		~TextureAtlas();

	public:
		const Frame* GetFrame( const char* name ) const;
		bool HasFrame( const char* name ) const;
		int GetFrameCount() const { return fFrames.Length(); }
		const Frame& GetFrameByIndex( int index ) const { return fFrames[index]; }
		const SharedPtr< TextureResource >& GetTextureResource() const { return fTexture; }
		S32 GetWidth() const { return fAtlasWidth; }
		S32 GetHeight() const { return fAtlasHeight; }

	private:
		TextureAtlas( Rtt_Allocator* allocator );

		// Shelf packing: assigns x,y to each rect. Returns false if doesn't fit.
		struct PackRect {
			S32 w, h;       // input: size
			S32 x, y;       // output: position
			int origIndex;  // to map back
		};
		static bool ShelfPack( PackRect* rects, int count, S32 maxSize, int padding, S32& outW, S32& outH );

	private:
		Rtt_Allocator* fAllocator;
		SharedPtr< TextureResource > fTexture;
		Array< Frame > fFrames;
		S32 fAtlasWidth;
		S32 fAtlasHeight;
};

// ----------------------------------------------------------------------------

// AtlasBitmapPaint: a BitmapPaint that maps UVs to a sub-region of the atlas texture
class AtlasBitmapPaint : public BitmapPaint
{
	Rtt_CLASS_NO_COPIES( AtlasBitmapPaint )

	public:
		AtlasBitmapPaint( const SharedPtr< TextureResource >& resource,
						  Real u0, Real v0, Real u1, Real v1 );

		virtual void ApplyPaintUVTransformations( ArrayVertex2& vertices ) const override;

	private:
		Real fU0, fV0, fU1, fV1;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_TextureAtlas_H__
