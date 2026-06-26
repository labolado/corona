////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_SDFInstanceRenderer_H__
#define _Rtt_SDFInstanceRenderer_H__

#include "Core/Rtt_Types.h"

#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV ) && !defined( Rtt_WIN_DESKTOP_ENV )
#include <bgfx/bgfx.h>
#define Rtt_SDF_INSTANCING_AVAILABLE 1
#endif

namespace Rtt
{

#if defined( Rtt_SDF_INSTANCING_AVAILABLE )

class SDFInstanceRenderer
{
	public:
		static SDFInstanceRenderer& Instance();

		void Initialize();
		void Finalize();

		bool IsAvailable() const;
		bool IsSupported() const;

		bgfx::ProgramHandle GetProgram() const { return fProgram; }
		bgfx::VertexBufferHandle GetBaseQuadVB() const { return fBaseQuadVB; }
		bgfx::IndexBufferHandle GetBaseQuadIB() const { return fBaseQuadIB; }

		static const U32 kInstanceStride = 80;

	private:
		SDFInstanceRenderer();
		~SDFInstanceRenderer();

		SDFInstanceRenderer( const SDFInstanceRenderer& );
		SDFInstanceRenderer& operator=( const SDFInstanceRenderer& );

		void CreateBaseQuad();
		void CreateProgram();

	private:
		bgfx::ProgramHandle fProgram;
		bgfx::VertexBufferHandle fBaseQuadVB;
		bgfx::IndexBufferHandle fBaseQuadIB;
		bgfx::UniformHandle fShapeTableUniform;
		bool fInitialized;
};

#else

class SDFInstanceRenderer
{
	public:
		static SDFInstanceRenderer& Instance() { static SDFInstanceRenderer s; return s; }
		void Initialize() {}
		void Finalize() {}
		bool IsAvailable() const { return false; }
		bool IsSupported() const { return false; }
};

#endif

} // namespace Rtt

#endif // _Rtt_SDFInstanceRenderer_H__
