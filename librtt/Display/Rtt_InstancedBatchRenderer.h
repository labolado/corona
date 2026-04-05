////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_InstancedBatchRenderer_H__
#define _Rtt_InstancedBatchRenderer_H__

#include "Core/Rtt_Types.h"
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// Backend-specific instanced draw data, passed via RenderData::fInstanceDraw
struct InstanceDrawData
{
	bgfx::InstanceDataBuffer instanceBuffer;
	bgfx::ProgramHandle programHandle;
	bgfx::VertexBufferHandle baseQuadVB;
	bgfx::IndexBufferHandle baseQuadIB;
	U32 instanceCount;
};

// InstancedBatchRenderer manages GPU instancing resources for BatchObject.
// Singleton pattern following SDFRenderer.
class InstancedBatchRenderer
{
	public:
		static InstancedBatchRenderer& Instance();

		static bool IsEnabled() { return sEnabled; }
		static void SetEnabled( bool value ) { sEnabled = value; }

		void Initialize();
		void Finalize();

		bool IsAvailable() const;
		bool IsSupported() const; // checks BGFX_CAPS_INSTANCING

		bgfx::ProgramHandle GetProgram() const { return fProgram; }
		bgfx::VertexBufferHandle GetBaseQuadVB() const { return fBaseQuadVB; }
		bgfx::IndexBufferHandle GetBaseQuadIB() const { return fBaseQuadIB; }

		// Instance data stride: 5 x vec4 = 80 bytes
		// Layout: mat4x3 (3 cols) + uvRect + color
		static const U32 kInstanceStride = 80;

	private:
		InstancedBatchRenderer();
		~InstancedBatchRenderer();

		InstancedBatchRenderer( const InstancedBatchRenderer& );
		InstancedBatchRenderer& operator=( const InstancedBatchRenderer& );

		void CreateBaseQuad();
		void CreateProgram();

	private:
		static bool sEnabled;

		bgfx::ProgramHandle fProgram;
		bgfx::VertexBufferHandle fBaseQuadVB;
		bgfx::IndexBufferHandle fBaseQuadIB;
		bgfx::UniformHandle fSamplerUniform;
		bool fInitialized;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_InstancedBatchRenderer_H__
