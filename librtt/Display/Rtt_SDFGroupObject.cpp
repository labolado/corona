#include "Core/Rtt_Config.h"
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )

////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// Author: Labo Lado, laboladoapp@gmail.com
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Display/Rtt_SDFGroupObject.h"

#include "Core/Rtt_Geometry.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_LuaLibSDFInstance.h"
#include "Display/Rtt_SDFInstanceRenderer.h"
#include "Display/Rtt_Shader.h"
#include "Display/Rtt_ShaderFactory.h"
#include "Renderer/Rtt_Renderer.h"
#include "Rtt_Matrix.h"

#include <cmath>
#include <cstring>

namespace Rtt
{

SDFGroupObject::SDFGroupObject( Rtt_Allocator* allocator, Display& display )
:	Super(),
	fSlots( allocator ),
	fActiveCount( 0 ),
	fGeometry( NULL ),
	fData(),
	fShader( NULL ),
	fUseInstancing( false )
{
	SetObjectDesc( "SDFGroupObject" );
	memset( &fDrawData, 0, sizeof( fDrawData ) );

	ShaderFactory& factory = display.GetShaderFactory();
	fShader = &factory.GetDefault();

	fGeometry = Rtt_NEW( allocator, Geometry( allocator, Geometry::kTriangles, 0, 0, false ) );
	fGeometry->Resize( 6, false );
	fGeometry->SetVerticesUsed( 6 );

	fData.fGeometry = fGeometry;
	fData.fFillTexture0 = NULL;
	fData.fFillTexture1 = NULL;
	fData.fMaskTexture = NULL;
	fData.fMaskUniform = NULL;
	fData.fUserUniform0 = NULL;
	fData.fUserUniform1 = NULL;
	fData.fUserUniform2 = NULL;
	fData.fUserUniform3 = NULL;
}

SDFGroupObject::~SDFGroupObject()
{
	Rtt_DELETE( fGeometry );
}

SDFGroupObject*
SDFGroupObject::New( Rtt_Allocator* allocator, Display& display, int capacity )
{
	if ( capacity < 1 )
	{
		capacity = 1;
	}

	SDFGroupObject* group = Rtt_NEW( allocator, SDFGroupObject( allocator, display ) );
	for ( int i = 0; i < capacity; i++ )
	{
		SDFInstanceSlot slot;
		slot.x = 0.0f;
		slot.y = 0.0f;
		slot.w = 1.0f;
		slot.h = 1.0f;
		slot.rotation = 0.0f;
		slot.shapeId = 0;
		slot.nVerts = ShapeVertexCount( 0 );
		slot.r = 1.0f;
		slot.g = 1.0f;
		slot.b = 1.0f;
		slot.a = 1.0f;
		slot.active = false;
		group->fSlots.Append( slot );
	}

	return group;
}

U8
SDFGroupObject::ShapeVertexCount( U8 shapeId )
{
	static const U8 kCounts[8] = { 5, 10, 7, 6, 8, 6, 3, 12 };
	return kCounts[ shapeId & 7 ];
}

int
SDFGroupObject::AddShape( const SDFInstanceSlot& params )
{
	for ( S32 i = 0, iMax = fSlots.Length(); i < iMax; i++ )
	{
		if ( !fSlots[i].active )
		{
			SDFInstanceSlot& slot = fSlots[i];
			slot = params;
			slot.shapeId &= 7;
			slot.nVerts = ShapeVertexCount( slot.shapeId );
			slot.active = true;
			fActiveCount++;
			Invalidate( kGeometryFlag | kStageBoundsFlag );
			return i;
		}
	}

	return -1;
}

bool
SDFGroupObject::UpdateShape( int slotId, const SDFInstanceSlot& params, U32 fieldMask )
{
	if ( slotId < 0 || slotId >= fSlots.Length() || !fSlots[slotId].active )
	{
		return false;
	}

	enum
	{
		kX = 0x001,
		kY = 0x002,
		kW = 0x004,
		kH = 0x008,
		kRotation = 0x010,
		kShapeId = 0x020,
		kR = 0x040,
		kG = 0x080,
		kB = 0x100,
		kA = 0x200
	};

	SDFInstanceSlot& slot = fSlots[slotId];
	if ( fieldMask & kX ) slot.x = params.x;
	if ( fieldMask & kY ) slot.y = params.y;
	if ( fieldMask & kW ) slot.w = params.w;
	if ( fieldMask & kH ) slot.h = params.h;
	if ( fieldMask & kRotation ) slot.rotation = params.rotation;
	if ( fieldMask & kShapeId )
	{
		slot.shapeId = params.shapeId & 7;
		slot.nVerts = ShapeVertexCount( slot.shapeId );
	}
	if ( fieldMask & kR ) slot.r = params.r;
	if ( fieldMask & kG ) slot.g = params.g;
	if ( fieldMask & kB ) slot.b = params.b;
	if ( fieldMask & kA ) slot.a = params.a;

	Invalidate( kGeometryFlag | kStageBoundsFlag );
	return true;
}

bool
SDFGroupObject::RemoveShape( int slotId )
{
	if ( slotId < 0 || slotId >= fSlots.Length() || !fSlots[slotId].active )
	{
		return false;
	}

	fSlots[slotId].active = false;
	fActiveCount--;
	Invalidate( kGeometryFlag | kStageBoundsFlag );
	return true;
}

void
SDFGroupObject::ClearShapes()
{
	for ( S32 i = 0, iMax = fSlots.Length(); i < iMax; i++ )
	{
		fSlots[i].active = false;
	}
	fActiveCount = 0;
	Invalidate( kGeometryFlag | kStageBoundsFlag );
}

void
SDFGroupObject::EnsurePlaceholderGeometry() const
{
	if ( fGeometry->GetVerticesAllocated() < 6 )
	{
		fGeometry->Resize( 6, false );
	}
	fGeometry->SetVerticesUsed( 6 );
	fGeometry->Invalidate();
}

void
SDFGroupObject::FillInstanceData() const
{
	SDFInstanceRenderer& inst = SDFInstanceRenderer::Instance();

	bgfx::allocInstanceDataBuffer( &fDrawData.instanceBuffer, fActiveCount, kSDFInstanceStride );
	float* data = (float*)fDrawData.instanceBuffer.data;

	const Matrix& parentXform = GetSrcToDstMatrix();
	float parentMat[16];
	if ( parentXform.IsIdentity() )
	{
		parentMat[0] = 1; parentMat[1] = 0; parentMat[2] = 0; parentMat[3] = 0;
		parentMat[4] = 0; parentMat[5] = 1; parentMat[6] = 0; parentMat[7] = 0;
		parentMat[8] = 0; parentMat[9] = 0; parentMat[10] = 1; parentMat[11] = 0;
		parentMat[12] = 0; parentMat[13] = 0; parentMat[14] = 0; parentMat[15] = 1;
	}
	else
	{
		const Real* r0 = parentXform.Row0();
		const Real* r1 = parentXform.Row1();
		parentMat[0]  = (float)r0[0]; parentMat[1]  = (float)r1[0]; parentMat[2]  = 0.0f; parentMat[3]  = 0.0f;
		parentMat[4]  = (float)r0[1]; parentMat[5]  = (float)r1[1]; parentMat[6]  = 0.0f; parentMat[7]  = 0.0f;
		parentMat[8]  = 0.0f;         parentMat[9]  = 0.0f;         parentMat[10] = 1.0f; parentMat[11] = 0.0f;
		parentMat[12] = (float)r0[2]; parentMat[13] = (float)r1[2]; parentMat[14] = 0.0f; parentMat[15] = 1.0f;
	}

	int written = 0;
	for ( S32 i = 0, iMax = fSlots.Length(); i < iMax && written < fActiveCount; i++ )
	{
		const SDFInstanceSlot& slot = fSlots[i];
		if ( !slot.active )
		{
			continue;
		}

		const float radians = slot.rotation;
		const float cosR = cosf( radians );
		const float sinR = sinf( radians );
		const float scaleW = slot.w * 0.5f;
		const float scaleH = slot.h * 0.5f;

		float localMat[16] = {
			 scaleW * cosR, scaleW * sinR, 0.0f, 0.0f,
			-scaleH * sinR, scaleH * cosR, 0.0f, 0.0f,
			 0.0f, 0.0f, 1.0f, 0.0f,
			 slot.x, slot.y, 0.0f, 1.0f
		};

		float worldMat[16];
		for ( int r = 0; r < 4; r++ )
		{
			for ( int c = 0; c < 4; c++ )
			{
				worldMat[c * 4 + r] =
					parentMat[0 * 4 + r] * localMat[c * 4 + 0] +
					parentMat[1 * 4 + r] * localMat[c * 4 + 1] +
					parentMat[2 * 4 + r] * localMat[c * 4 + 2] +
					parentMat[3 * 4 + r] * localMat[c * 4 + 3];
			}
		}

		data[0] = worldMat[0];
		data[1] = worldMat[1];
		data[2] = worldMat[2];
		data[3] = worldMat[3];

		data[4] = worldMat[4];
		data[5] = worldMat[5];
		data[6] = worldMat[6];
		data[7] = worldMat[7];

		data[8] = worldMat[12];
		data[9] = worldMat[13];
		data[10] = worldMat[14];
		data[11] = worldMat[15];

		data[12] = cosR;
		data[13] = sinR;
		data[14] = (float)slot.shapeId;
		data[15] = (float)slot.nVerts;

		data[16] = slot.r;
		data[17] = slot.g;
		data[18] = slot.b;
		data[19] = slot.a * ( (float)AlphaCumulative() / 255.0f );

		data += kSDFInstanceStride / sizeof( float );
		written++;
	}

	fDrawData.instanceCount = written;
	fDrawData.programHandle = inst.GetProgram();
	fDrawData.baseQuadVB = inst.GetBaseQuadVB();
	fDrawData.baseQuadIB = inst.GetBaseQuadIB();
}

void
SDFGroupObject::Prepare( const Display& display )
{
	Super::Prepare( display );

	if ( ShouldPrepare() )
	{
		SDFInstanceRenderer& inst = SDFInstanceRenderer::Instance();
		if ( !inst.IsAvailable() )
		{
			inst.Initialize();
		}

		fUseInstancing = inst.IsAvailable() && fActiveCount > 0;
		if ( fUseInstancing )
		{
			FillInstanceData();
			fData.fInstanceDraw = &fDrawData;
			EnsurePlaceholderGeometry();
		}
		else
		{
			fData.fInstanceDraw = NULL;
			fGeometry->SetVerticesUsed( 0 );
		}

		fShader->Prepare( fData, 0, 0, ShaderResource::kDefault );
		SetValid( kGeometryFlag | kPaintFlag | kColorFlag | kProgramFlag | kProgramDataFlag );
	}
}

void
SDFGroupObject::Draw( Renderer& renderer ) const
{
	if ( !ShouldDraw() || fActiveCount == 0 )
	{
		return;
	}

	if ( fUseInstancing )
	{
		EnsurePlaceholderGeometry();
		fShader->Draw( renderer, fData );
	}
}

void
SDFGroupObject::GetSelfBounds( Rect& rect ) const
{
	if ( fActiveCount == 0 )
	{
		rect.SetEmpty();
		return;
	}

	Real xMin = Rtt_IntToReal( 0x7FFFFFFF );
	Real yMin = xMin;
	Real xMax = -xMin;
	Real yMax = -yMin;

	for ( S32 i = 0, iMax = fSlots.Length(); i < iMax; i++ )
	{
		const SDFInstanceSlot& slot = fSlots[i];
		if ( !slot.active )
		{
			continue;
		}

		const Real halfW = Rtt_RealDiv2( Rtt_FloatToReal( slot.w ) );
		const Real halfH = Rtt_RealDiv2( Rtt_FloatToReal( slot.h ) );
		const Real radius = Rtt_RealSqrt( Rtt_RealMul( halfW, halfW ) + Rtt_RealMul( halfH, halfH ) );
		const Real x = Rtt_FloatToReal( slot.x );
		const Real y = Rtt_FloatToReal( slot.y );

		if ( x - radius < xMin ) xMin = x - radius;
		if ( x + radius > xMax ) xMax = x + radius;
		if ( y - radius < yMin ) yMin = y - radius;
		if ( y + radius > yMax ) yMax = y + radius;
	}

	rect.Initialize( xMin, yMin, xMax, yMax );
}

bool
SDFGroupObject::HitTest( Real, Real )
{
	return false;
}

const LuaProxyVTable&
SDFGroupObject::ProxyVTable() const
{
	return LuaSDFGroupObjectProxyVTable::Constant();
}

} // namespace Rtt

#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
