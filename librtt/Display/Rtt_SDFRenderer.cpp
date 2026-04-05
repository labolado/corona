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

	// SDF shader programs are stubs for now (BGFX_INVALID_HANDLE).
	// They will be populated when SDF shader binaries are compiled
	// and embedded via Rtt_BgfxShaderData_effects_metal.h.
	// Until then, IsAvailable() returns false and the SDF code path
	// is never taken.
	for ( int i = 0; i < kNumShapeTypes; ++i )
	{
		fPrograms[i] = BGFX_INVALID_HANDLE;
	}

	fInitialized = true;
}

void
SDFRenderer::Finalize()
{
	if ( !fInitialized )
	{
		return;
	}

	for ( int i = 0; i < kNumShapeTypes; ++i )
	{
		if ( bgfx::isValid( fPrograms[i] ) )
		{
			bgfx::destroy( fPrograms[i] );
			fPrograms[i] = BGFX_INVALID_HANDLE;
		}
	}

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
