//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Display/Rtt_SDFRenderer.h"
#include "Core/Rtt_Assert.h"
#include "Renderer/Rtt_BgfxShaderData_sdf_metal.h"

#include <string.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

bool SDFRenderer::sEnabled = true;

SDFRenderer::SDFRenderer()
:	fInitialized( false )
{
	for ( int i = 0; i < kNumShapeTypes; ++i )
	{
		fPrograms[i] = BGFX_INVALID_HANDLE;
	}
	fParamsUniform = BGFX_INVALID_HANDLE;
	fFillColorUniform = BGFX_INVALID_HANDLE;
	fStrokeColorUniform = BGFX_INVALID_HANDLE;
}

SDFRenderer::~SDFRenderer()
{
	Finalize();
}

SDFRenderer&
SDFRenderer::Instance()
{
	static SDFRenderer sInstance;
	return sInstance;
}

void
SDFRenderer::Initialize()
{
	if ( fInitialized )
	{
		return;
	}

	// Create uniform handles for SDF parameters
	fParamsUniform = bgfx::createUniform( "u_sdfParams", bgfx::UniformType::Vec4 );
	fFillColorUniform = bgfx::createUniform( "u_sdfFillColor", bgfx::UniformType::Vec4 );
	fStrokeColorUniform = bgfx::createUniform( "u_sdfStrokeColor", bgfx::UniformType::Vec4 );

	// Create SDF shader programs from embedded shader binaries
	// Vertex shader is shared across all shape types
	const bgfx::Memory* vsMemory = bgfx::copy(s_vs_sdf_metal, s_vs_sdf_metal_size);
	bgfx::ShaderHandle vsHandle = bgfx::createShader(vsMemory);
	
	// Circle program
	const bgfx::Memory* fsCircleMemory = bgfx::copy(s_fs_sdf_circle_metal, s_fs_sdf_circle_metal_size);
	bgfx::ShaderHandle fsCircleHandle = bgfx::createShader(fsCircleMemory);
	fPrograms[kCircle] = bgfx::createProgram(vsHandle, fsCircleHandle, true);
	
	// Rect program (re-create vs since previous createProgram destroyed it)
	vsMemory = bgfx::copy(s_vs_sdf_metal, s_vs_sdf_metal_size);
	vsHandle = bgfx::createShader(vsMemory);
	const bgfx::Memory* fsRectMemory = bgfx::copy(s_fs_sdf_rect_metal, s_fs_sdf_rect_metal_size);
	bgfx::ShaderHandle fsRectHandle = bgfx::createShader(fsRectMemory);
	fPrograms[kRect] = bgfx::createProgram(vsHandle, fsRectHandle, true);
	
	// Rounded rect uses same shader as rect
	fPrograms[kRoundedRect] = fPrograms[kRect];

	fInitialized = true;
}

void
SDFRenderer::Finalize()
{
	if ( !fInitialized )
	{
		return;
	}

	// Destroy programs (kRoundedRect shares program with kRect, so skip it)
	if ( bgfx::isValid( fPrograms[kCircle] ) )
	{
		bgfx::destroy( fPrograms[kCircle] );
		fPrograms[kCircle] = BGFX_INVALID_HANDLE;
	}
	if ( bgfx::isValid( fPrograms[kRect] ) )
	{
		bgfx::destroy( fPrograms[kRect] );
		fPrograms[kRect] = BGFX_INVALID_HANDLE;
	}
	// kRoundedRect uses same program as kRect, already destroyed above
	fPrograms[kRoundedRect] = BGFX_INVALID_HANDLE;

	if ( bgfx::isValid( fParamsUniform ) )
	{
		bgfx::destroy( fParamsUniform );
		fParamsUniform = BGFX_INVALID_HANDLE;
	}

	if ( bgfx::isValid( fFillColorUniform ) )
	{
		bgfx::destroy( fFillColorUniform );
		fFillColorUniform = BGFX_INVALID_HANDLE;
	}

	if ( bgfx::isValid( fStrokeColorUniform ) )
	{
		bgfx::destroy( fStrokeColorUniform );
		fStrokeColorUniform = BGFX_INVALID_HANDLE;
	}

	fInitialized = false;
}

bool
SDFRenderer::IsAvailable() const
{
	if ( !fInitialized || !sEnabled )
	{
		return false;
	}

	// Check that at least one program is valid
	for ( int i = 0; i < kNumShapeTypes; ++i )
	{
		if ( bgfx::isValid( fPrograms[i] ) )
		{
			return true;
		}
	}

	return false;
}

bgfx::ProgramHandle
SDFRenderer::GetProgram( ShapeType type ) const
{
	Rtt_ASSERT( type >= 0 && type < kNumShapeTypes );
	return fPrograms[type];
}

void
SDFRenderer::SetShapeUniforms(
	ShapeType type,
	Real width, Real height,
	Real cornerRadius,
	Real strokeWidth )
{
	if ( !fInitialized )
	{
		return;
	}

	float params[4] = {
		(float)width,
		(float)height,
		(float)cornerRadius,
		(float)strokeWidth
	};

	if ( bgfx::isValid( fParamsUniform ) )
	{
		bgfx::setUniform( fParamsUniform, params );
	}
}

void
SDFRenderer::SetColorUniforms(
	Real fillR, Real fillG, Real fillB, Real fillA,
	Real strokeR, Real strokeG, Real strokeB, Real strokeA )
{
	if ( !fInitialized )
	{
		return;
	}

	float fillColor[4] = {
		(float)fillR, (float)fillG, (float)fillB, (float)fillA
	};
	float strokeColor[4] = {
		(float)strokeR, (float)strokeG, (float)strokeB, (float)strokeA
	};

	if ( bgfx::isValid( fFillColorUniform ) )
	{
		bgfx::setUniform( fFillColorUniform, fillColor );
	}

	if ( bgfx::isValid( fStrokeColorUniform ) )
	{
		bgfx::setUniform( fStrokeColorUniform, strokeColor );
	}
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
