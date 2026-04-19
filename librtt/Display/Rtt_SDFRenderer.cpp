#include "Core/Rtt_Config.h"
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )

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
#if defined(Rtt_ANDROID_ENV)
    #include "Renderer/Rtt_BgfxShaderData_sdf_essl.h"
    #include "Renderer/Rtt_BgfxShaderData_sdf_spirv.h"
#else
    #include "Renderer/Rtt_BgfxShaderData_sdf_metal.h"
#endif

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
	fLineParamsUniform = BGFX_INVALID_HANDLE;
	fPolyParamsUniform = BGFX_INVALID_HANDLE;
	fPolyVertsUniform = BGFX_INVALID_HANDLE;
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
	fLineParamsUniform = bgfx::createUniform( "u_lineParams", bgfx::UniformType::Vec4 );
	fPolyParamsUniform = bgfx::createUniform( "u_polyParams", bgfx::UniformType::Vec4 );
	fPolyVertsUniform = bgfx::createUniform( "u_polyVerts", bgfx::UniformType::Vec4, 8 );

	// Create SDF shader programs from platform-specific embedded binaries
	// Vertex shader is shared across all shape types
#if defined(Rtt_ANDROID_ENV)
	const bool useVulkanShaders = ( bgfx::getRendererType() == bgfx::RendererType::Vulkan );
	#define SDF_VS_DATA   ( useVulkanShaders ? s_vs_sdf_spirv : s_vs_sdf_essl )
	#define SDF_VS_SIZE   ( useVulkanShaders ? s_vs_sdf_spirv_size : s_vs_sdf_essl_size )
	#define SDF_FS_CIRCLE ( useVulkanShaders ? s_fs_sdf_circle_spirv : s_fs_sdf_circle_essl )
	#define SDF_FS_CIRCLE_SIZE ( useVulkanShaders ? s_fs_sdf_circle_spirv_size : s_fs_sdf_circle_essl_size )
	#define SDF_FS_RECT   ( useVulkanShaders ? s_fs_sdf_rect_spirv : s_fs_sdf_rect_essl )
	#define SDF_FS_RECT_SIZE ( useVulkanShaders ? s_fs_sdf_rect_spirv_size : s_fs_sdf_rect_essl_size )
	#define SDF_FS_LINE   ( useVulkanShaders ? s_fs_sdf_line_spirv : s_fs_sdf_line_essl )
	#define SDF_FS_LINE_SIZE ( useVulkanShaders ? s_fs_sdf_line_spirv_size : s_fs_sdf_line_essl_size )
	#define SDF_FS_POLY   ( useVulkanShaders ? s_fs_sdf_polygon_spirv : s_fs_sdf_polygon_essl )
	#define SDF_FS_POLY_SIZE ( useVulkanShaders ? s_fs_sdf_polygon_spirv_size : s_fs_sdf_polygon_essl_size )
#else
	#define SDF_VS_DATA   s_vs_sdf_metal
	#define SDF_VS_SIZE   s_vs_sdf_metal_size
	#define SDF_FS_CIRCLE s_fs_sdf_circle_metal
	#define SDF_FS_CIRCLE_SIZE s_fs_sdf_circle_metal_size
	#define SDF_FS_RECT   s_fs_sdf_rect_metal
	#define SDF_FS_RECT_SIZE s_fs_sdf_rect_metal_size
	#define SDF_FS_LINE   s_fs_sdf_line_metal
	#define SDF_FS_LINE_SIZE s_fs_sdf_line_metal_size
	#define SDF_FS_POLY   s_fs_sdf_polygon_metal
	#define SDF_FS_POLY_SIZE s_fs_sdf_polygon_metal_size
#endif

	const bgfx::Memory* vsMemory = bgfx::copy(SDF_VS_DATA, SDF_VS_SIZE);
	bgfx::ShaderHandle vsHandle = bgfx::createShader(vsMemory);

	// Circle program
	const bgfx::Memory* fsCircleMemory = bgfx::copy(SDF_FS_CIRCLE, SDF_FS_CIRCLE_SIZE);
	bgfx::ShaderHandle fsCircleHandle = bgfx::createShader(fsCircleMemory);
	fPrograms[kCircle] = bgfx::createProgram(vsHandle, fsCircleHandle, true);

	// Rect program (re-create vs since previous createProgram destroyed it)
	vsMemory = bgfx::copy(SDF_VS_DATA, SDF_VS_SIZE);
	vsHandle = bgfx::createShader(vsMemory);
	const bgfx::Memory* fsRectMemory = bgfx::copy(SDF_FS_RECT, SDF_FS_RECT_SIZE);
	bgfx::ShaderHandle fsRectHandle = bgfx::createShader(fsRectMemory);
	fPrograms[kRect] = bgfx::createProgram(vsHandle, fsRectHandle, true);

	// Rounded rect uses same shader as rect
	fPrograms[kRoundedRect] = fPrograms[kRect];

	// Line program
	vsMemory = bgfx::copy(SDF_VS_DATA, SDF_VS_SIZE);
	vsHandle = bgfx::createShader(vsMemory);
	const bgfx::Memory* fsLineMemory = bgfx::copy(SDF_FS_LINE, SDF_FS_LINE_SIZE);
	bgfx::ShaderHandle fsLineHandle = bgfx::createShader(fsLineMemory);
	fPrograms[kLine] = bgfx::createProgram(vsHandle, fsLineHandle, true);

	// Polygon program
	vsMemory = bgfx::copy(SDF_VS_DATA, SDF_VS_SIZE);
	vsHandle = bgfx::createShader(vsMemory);
	const bgfx::Memory* fsPolyMemory = bgfx::copy(SDF_FS_POLY, SDF_FS_POLY_SIZE);
	bgfx::ShaderHandle fsPolyHandle = bgfx::createShader(fsPolyMemory);
	fPrograms[kPolygon] = bgfx::createProgram(vsHandle, fsPolyHandle, true);

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
	fPrograms[kRoundedRect] = BGFX_INVALID_HANDLE;

	if ( bgfx::isValid( fPrograms[kLine] ) )
	{
		bgfx::destroy( fPrograms[kLine] );
		fPrograms[kLine] = BGFX_INVALID_HANDLE;
	}
	if ( bgfx::isValid( fPrograms[kPolygon] ) )
	{
		bgfx::destroy( fPrograms[kPolygon] );
		fPrograms[kPolygon] = BGFX_INVALID_HANDLE;
	}

	bgfx::UniformHandle uniforms[] = {
		fParamsUniform, fFillColorUniform, fStrokeColorUniform,
		fLineParamsUniform, fPolyParamsUniform, fPolyVertsUniform
	};
	for ( int i = 0; i < 6; ++i )
	{
		if ( bgfx::isValid( uniforms[i] ) )
		{
			bgfx::destroy( uniforms[i] );
		}
	}
	fParamsUniform = BGFX_INVALID_HANDLE;
	fFillColorUniform = BGFX_INVALID_HANDLE;
	fStrokeColorUniform = BGFX_INVALID_HANDLE;
	fLineParamsUniform = BGFX_INVALID_HANDLE;
	fPolyParamsUniform = BGFX_INVALID_HANDLE;
	fPolyVertsUniform = BGFX_INVALID_HANDLE;

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

void
SDFRenderer::SetLineUniforms( Real x0, Real y0, Real x1, Real y1 )
{
	if ( !fInitialized )
	{
		return;
	}

	float params[4] = {
		(float)x0, (float)y0, (float)x1, (float)y1
	};

	if ( bgfx::isValid( fLineParamsUniform ) )
	{
		bgfx::setUniform( fLineParamsUniform, params );
	}
}

bool
SDFRenderer::SetPolygonUniforms( const Real* verts, int numVerts )
{
	if ( !fInitialized || numVerts < 3 || numVerts > kMaxPolygonVerts )
	{
		return false;
	}

	// Set vertex count
	float polyParams[4] = { (float)numVerts, 0.0f, 0.0f, 0.0f };
	if ( bgfx::isValid( fPolyParamsUniform ) )
	{
		bgfx::setUniform( fPolyParamsUniform, polyParams );
	}

	// Pack vertices into vec4 array: each vec4 = (x0,y0, x1,y1)
	float packedVerts[32]; // 8 vec4s * 4 floats
	memset( packedVerts, 0, sizeof(packedVerts) );
	for ( int i = 0; i < numVerts; ++i )
	{
		int vecIdx = i / 2;
		int comp = (i % 2) * 2;
		packedVerts[vecIdx * 4 + comp] = (float)verts[i * 2];
		packedVerts[vecIdx * 4 + comp + 1] = (float)verts[i * 2 + 1];
	}

	if ( bgfx::isValid( fPolyVertsUniform ) )
	{
		bgfx::setUniform( fPolyVertsUniform, packedVerts, 8 );
	}

	return true;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------


#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
