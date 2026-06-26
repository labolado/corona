#include "Core/Rtt_Config.h"
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )

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

#include "Display/Rtt_SDFInstanceRenderer.h"
#if defined(Rtt_ANDROID_ENV)
	#include "Renderer/Rtt_BgfxShaderData_sdf_instanced_essl.h"
	#include "Renderer/Rtt_BgfxShaderData_sdf_instanced_spirv.h"
#else
	#include "Renderer/Rtt_BgfxShaderData_sdf_instanced_metal.h"
#endif

namespace Rtt
{

#if defined( Rtt_SDF_INSTANCING_AVAILABLE )

SDFInstanceRenderer::SDFInstanceRenderer()
:	fInitialized( false )
{
	fProgram = BGFX_INVALID_HANDLE;
	fBaseQuadVB = BGFX_INVALID_HANDLE;
	fBaseQuadIB = BGFX_INVALID_HANDLE;
	fShapeTableUniform = BGFX_INVALID_HANDLE;
}

SDFInstanceRenderer::~SDFInstanceRenderer()
{
	Finalize();
}

SDFInstanceRenderer&
SDFInstanceRenderer::Instance()
{
	static SDFInstanceRenderer sInstance;
	return sInstance;
}

bool
SDFInstanceRenderer::IsSupported() const
{
	return ( bgfx::getCaps()->supported & BGFX_CAPS_INSTANCING ) != 0;
}

bool
SDFInstanceRenderer::IsAvailable() const
{
	return fInitialized && bgfx::isValid( fProgram ) && IsSupported();
}

void
SDFInstanceRenderer::Initialize()
{
	if ( fInitialized )
	{
		return;
	}

	if ( !IsSupported() )
	{
		Rtt_LogException( "SDFInstanceRenderer: GPU instancing not supported\n" );
		fInitialized = true;
		return;
	}

	CreateProgram();
	CreateBaseQuad();

	fInitialized = true;
}

void
SDFInstanceRenderer::Finalize()
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
	if ( bgfx::isValid( fShapeTableUniform ) )
	{
		bgfx::destroy( fShapeTableUniform );
		fShapeTableUniform = BGFX_INVALID_HANDLE;
	}

	fInitialized = false;
}

void
SDFInstanceRenderer::CreateProgram()
{
#if defined(Rtt_ANDROID_ENV)
	const bool useVulkanShaders = ( bgfx::getRendererType() == bgfx::RendererType::Vulkan );
	const bgfx::Memory* vsMemory = bgfx::copy(
		useVulkanShaders ? s_vs_sdf_instanced_spirv : s_vs_sdf_instanced_essl,
		useVulkanShaders ? s_vs_sdf_instanced_spirv_size : s_vs_sdf_instanced_essl_size );
	const bgfx::Memory* fsMemory = bgfx::copy(
		useVulkanShaders ? s_fs_sdf_instanced_spirv : s_fs_sdf_instanced_essl,
		useVulkanShaders ? s_fs_sdf_instanced_spirv_size : s_fs_sdf_instanced_essl_size );
#else
	const bgfx::Memory* vsMemory = bgfx::copy( s_vs_sdf_instanced_metal, s_vs_sdf_instanced_metal_size );
	const bgfx::Memory* fsMemory = bgfx::copy( s_fs_sdf_instanced_metal, s_fs_sdf_instanced_metal_size );
#endif

	bgfx::ShaderHandle vsHandle = bgfx::createShader( vsMemory );
	bgfx::ShaderHandle fsHandle = bgfx::createShader( fsMemory );
	fProgram = bgfx::createProgram( vsHandle, fsHandle, true );

	if ( !bgfx::isValid( fProgram ) )
	{
		Rtt_LogException( "SDFInstanceRenderer: failed to create instanced SDF program\n" );
	}
}

void
SDFInstanceRenderer::CreateBaseQuad()
{
	bgfx::VertexLayout layout;
	layout.begin()
		.add( bgfx::Attrib::Position, 2, bgfx::AttribType::Float )
	.end();

	struct QuadVertex
	{
		float x, y;
	};

	static const QuadVertex vertices[4] =
	{
		{ -1.0f, -1.0f },
		{  1.0f, -1.0f },
		{ -1.0f,  1.0f },
		{  1.0f,  1.0f },
	};

	fBaseQuadVB = bgfx::createVertexBuffer(
		bgfx::makeRef( vertices, sizeof( vertices ) ),
		layout );

	static const uint16_t indices[6] = { 0, 1, 2, 1, 3, 2 };

	fBaseQuadIB = bgfx::createIndexBuffer(
		bgfx::makeRef( indices, sizeof( indices ) ) );
}

#endif // Rtt_SDF_INSTANCING_AVAILABLE

} // namespace Rtt

#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
