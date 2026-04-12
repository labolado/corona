//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_SDFRenderer_H__
#define _Rtt_SDFRenderer_H__

#include "Core/Rtt_Real.h"
#include "Core/Rtt_Types.h"

// SDF rendering requires bgfx — exclude platforms without bgfx support
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )
#define Rtt_SDF_AVAILABLE 1
#include <bgfx/bgfx.h>
#endif

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

#if defined( Rtt_SDF_AVAILABLE )

// SDFRenderer manages SDF (Signed Distance Field) shader programs and uniforms
// for rendering basic shapes (circle, rect, roundedRect) with pixel-perfect
// anti-aliasing using a single quad per shape instead of tessellated meshes.
class SDFRenderer
{
	public:
		enum ShapeType
		{
			kCircle = 0,
			kRect,
			kRoundedRect,
			kLine,
			kPolygon,
			kNumShapeTypes
		};

		// Maximum polygon vertices for SDF rendering (16 verts packed in 8 vec4s)
		static const int kMaxPolygonVerts = 16;

	public:
		static SDFRenderer& Instance();

		// Lifecycle
		void Initialize();
		void Finalize();

		// Runtime SDF toggle
		static bool IsEnabled() { return sEnabled; }
		static void SetEnabled( bool value ) { sEnabled = value; }

		// Returns true if SDF shaders are loaded and ready
		bool IsAvailable() const;

		// Get bgfx program handle for given shape type
		bgfx::ProgramHandle GetProgram( ShapeType type ) const;

		// Set SDF shape uniforms before draw submission.
		// width/height: shape dimensions in pixels
		// cornerRadius: for roundedRect (0 for circle/rect)
		// strokeWidth: stroke width in pixels (0 for no stroke)
		void SetShapeUniforms(
			ShapeType type,
			Real width, Real height,
			Real cornerRadius,
			Real strokeWidth );

		// Set fill and stroke colors as vec4 (r, g, b, a) in [0..1]
		void SetColorUniforms(
			Real fillR, Real fillG, Real fillB, Real fillA,
			Real strokeR, Real strokeG, Real strokeB, Real strokeA );

		// Set line endpoints in normalized [-1,1] space for line SDF
		void SetLineUniforms( Real x0, Real y0, Real x1, Real y1 );

		// Set polygon vertices for polygon SDF. Returns false if numVerts > kMaxPolygonVerts.
		bool SetPolygonUniforms( const Real* verts, int numVerts );

	private:
		SDFRenderer();
		~SDFRenderer();

		// Non-copyable
		SDFRenderer( const SDFRenderer& );
		SDFRenderer& operator=( const SDFRenderer& );

	private:
		bgfx::ProgramHandle fPrograms[kNumShapeTypes];
		bgfx::UniformHandle fParamsUniform;       // u_sdfParams: vec4(width, height, cornerRadius, strokeWidth)
		bgfx::UniformHandle fFillColorUniform;    // u_sdfFillColor: vec4(r, g, b, a)
		bgfx::UniformHandle fStrokeColorUniform;  // u_sdfStrokeColor: vec4(r, g, b, a)
		bgfx::UniformHandle fLineParamsUniform;   // u_lineParams: vec4(x0, y0, x1, y1)
		bgfx::UniformHandle fPolyParamsUniform;   // u_polyParams: vec4(numVerts, 0, 0, 0)
		bgfx::UniformHandle fPolyVertsUniform;    // u_polyVerts: vec4[8]
		bool fInitialized;

		static bool sEnabled;
};

#else // !Rtt_SDF_AVAILABLE — stub for platforms without bgfx (emscripten, tvOS)

class SDFRenderer
{
	public:
		enum ShapeType { kCircle = 0, kRect, kRoundedRect, kLine, kPolygon, kNumShapeTypes };
		static const int kMaxPolygonVerts = 16;

		static SDFRenderer& Instance() { static SDFRenderer s; return s; }
		static bool IsEnabled() { return false; }
		static void SetEnabled( bool ) {}
		bool IsAvailable() const { return false; }

		// Stubs for compilation — never called (IsAvailable returns false)
		void Initialize() {}
		void Finalize() {}
		void SetShapeUniforms( ShapeType, Real, Real, Real, Real ) {}
		void SetColorUniforms( Real, Real, Real, Real, Real, Real, Real, Real ) {}
		void SetLineUniforms( Real, Real, Real, Real ) {}
		bool SetPolygonUniforms( const Real*, int ) { return false; }
};

#endif // Rtt_SDF_AVAILABLE

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_SDFRenderer_H__
