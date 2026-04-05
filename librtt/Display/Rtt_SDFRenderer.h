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
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

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
			kNumShapeTypes
		};

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
		bool fInitialized;

		static bool sEnabled;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_SDFRenderer_H__
