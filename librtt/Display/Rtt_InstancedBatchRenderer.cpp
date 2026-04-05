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

#include "Display/Rtt_InstancedBatchRenderer.h"
#include "Renderer/Rtt_BgfxShaderData_instanced_metal.h"

#include <string.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

bool InstancedBatchRenderer::sEnabled = true;

InstancedBatchRenderer::InstancedBatchRenderer()
:	fInitialized( false )
{
	fProgram = BGFX_INVALID_HANDLE;
	fBaseQuadVB = BGFX_INVALID_HANDLE;
	fBaseQuadIB = BGFX_INVALID_HANDLE;
	fSamplerUniform = BGFX_INVALID_HANDLE;
}

InstancedBatchRenderer::~InstancedBatchRenderer()
{
	Finalize();
}

InstancedBatchRenderer&
InstancedBatchRenderer::Instance()
{
	static InstancedBatchRenderer sInstance;
	return sInstance;
}

bool
InstancedBatchRenderer::IsSupported() const
{
	return ( bgfx::getCaps()->supported & BGFX_CAPS_INSTANCING ) != 0;
}

bool
InstancedBatchRenderer::IsAvailable() const
{
	return fInitialized && sEnabled && bgfx::isValid( fProgram ) && IsSupported();
}

void
InstancedBatchRenderer::Initialize()
{
	if ( fInitialized )
	{
		return;
	}

	if ( !IsSupported() )
	{
		Rtt_LogException( "InstancedBatchRenderer: GPU instancing not supported, falling back to CPU batching\n" );
		fInitialized = true; // mark initialized so we don't retry
		return;
	}

	CreateProgram();
	CreateBaseQuad();

	fSamplerUniform = bgfx::createUniform( "s_texColor", bgfx::UniformType::Sampler );

	fInitialized = true;

	Rtt_LogException( "InstancedBatchRenderer: initialized (instancing supported)\n" );
}

void
InstancedBatchRenderer::Finalize()
{
	if ( !fInitialized )
	{
		return;
	}

	if ( bgfx::isValid( fProgram ) )
	{
		bgfx::destroy( fProgram );
		fProgram = BGFX_INVALID_HANDLE;
	}
	if ( bgfx::isValid( fBaseQuadVB ) )
	{
		bgfx::destroy( fBaseQuadVB );
		fBaseQuadVB = BGFX_INVALID_HANDLE;
	}
	if ( bgfx::isValid( fBaseQuadIB ) )
	{
		bgfx::destroy( fBaseQuadIB );
		fBaseQuadIB = BGFX_INVALID_HANDLE;
	}
	if ( bgfx::isValid( fSamplerUniform ) )
	{
		bgfx::destroy( fSamplerUniform );
		fSamplerUniform = BGFX_INVALID_HANDLE;
	}

	fInitialized = false;
}

void
InstancedBatchRenderer::CreateProgram()
{
	// Instanced vertex shader from embedded binary
	const bgfx::Memory* vsMemory = bgfx::copy( s_vs_batch_instanced_metal, s_vs_batch_instanced_metal_size );
	bgfx::ShaderHandle vsHandle = bgfx::createShader( vsMemory );

	// Custom fragment shader for instanced rendering
	const bgfx::Memory* fsMemory = bgfx::copy( s_fs_batch_instanced_metal, s_fs_batch_instanced_metal_size );
	bgfx::ShaderHandle fsHandle = bgfx::createShader( fsMemory );

	fProgram = bgfx::createProgram( vsHandle, fsHandle, true );

	if ( !bgfx::isValid( fProgram ) )
	{
		Rtt_LogException( "InstancedBatchRenderer: failed to create instanced program\n" );
	}
}

void
InstancedBatchRenderer::CreateBaseQuad()
{
	// Unit quad: position (-0.5..0.5), texcoord (0..1)
	// Vertex layout: position (3 float) + texcoord0 (2 float)
	bgfx::VertexLayout layout;
	layout.begin()
		.add( bgfx::Attrib::Position, 3, bgfx::AttribType::Float )
		.add( bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float )
	.end();

	struct QuadVertex
	{
		float x, y, z;
		float u, v;
	};

	static const QuadVertex vertices[4] =
	{
		{ -0.5f, -0.5f, 0.0f,   0.0f, 0.0f }, // TL
		{  0.5f, -0.5f, 0.0f,   1.0f, 0.0f }, // TR
		{ -0.5f,  0.5f, 0.0f,   0.0f, 1.0f }, // BL
		{  0.5f,  0.5f, 0.0f,   1.0f, 1.0f }, // BR
	};

	fBaseQuadVB = bgfx::createVertexBuffer(
		bgfx::makeRef( vertices, sizeof(vertices) ),
		layout
	);

	// Index buffer: two triangles (TL, TR, BL, TR, BR, BL)
	static const uint16_t indices[6] = { 0, 1, 2, 1, 3, 2 };

	fBaseQuadIB = bgfx::createIndexBuffer(
		bgfx::makeRef( indices, sizeof(indices) )
	);
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
