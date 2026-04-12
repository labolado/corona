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

#include "Display/Rtt_BatchObject.h"
#include "Display/Rtt_TextureAtlas.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_Shader.h"
#include "Display/Rtt_ShaderFactory.h"
#include "Display/Rtt_TextureResource.h"
#include "Display/Rtt_InstancedBatchRenderer.h"
#include "Renderer/Rtt_Geometry_Renderer.h"
#include "Renderer/Rtt_Renderer.h"
#include "Rtt_LuaProxyVTable.h"
#include "Rtt_Matrix.h"
#include "Core/Rtt_Geometry.h"

#include <cmath>
#include <cstring>

// ----------------------------------------------------------------------------

#define VERTICES_PER_QUAD 6

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

BatchObject::BatchObject( Rtt_Allocator* allocator, Display& display )
:	Super(),
	fAtlas( NULL ),
	fSlots( allocator ),
	fActiveCount( 0 ),
	fGeometry( NULL ),
	fData(),
	fShader( NULL ),
	fVerticesDirty( true ),
	fRemoved( false ),
	fUseInstancing( false )
{
	SetObjectDesc( "BatchObject" );
	memset( &fInstanceDrawData, 0, sizeof(fInstanceDrawData) );
}

BatchObject::~BatchObject()
{
	Rtt_DELETE( fGeometry );
}

BatchObject*
BatchObject::New(
	Rtt_Allocator* allocator,
	Display& display,
	TextureAtlas* atlas,
	int initialCapacity )
{
	if ( !atlas || initialCapacity <= 0 )
	{
		return NULL;
	}

	BatchObject* batch = Rtt_NEW( allocator, BatchObject( allocator, display ) );
	batch->fAtlas = atlas;

	// Pre-allocate slot storage
	for ( int i = 0; i < initialCapacity; i++ )
	{
		Slot slot;
		slot.frameIndex = 0;
		slot.x = Rtt_REAL_0;
		slot.y = Rtt_REAL_0;
		slot.scaleX = Rtt_REAL_1;
		slot.scaleY = Rtt_REAL_1;
		slot.rotation = Rtt_REAL_0;
		slot.alpha = Rtt_REAL_1;
		slot.isVisible = false;
		slot.isDirty = false;
		slot.isActive = false;
		batch->fSlots.Append( slot );
	}

	// Create geometry buffer (triangles, not stored on GPU = transient)
	batch->fGeometry = Rtt_NEW( allocator,
		Geometry( allocator, Geometry::kTriangles, 0, 0, false ) );

	batch->fGeometry->Resize( initialCapacity * VERTICES_PER_QUAD, false );
	batch->fGeometry->SetVerticesUsed( 0 );

	// Set up render data
	ShaderFactory& factory = display.GetShaderFactory();
	batch->fShader = &factory.GetDefault();

	const SharedPtr< TextureResource >& texRes = atlas->GetTextureResource();
	batch->fData.fFillTexture0 = &texRes->GetTexture();
	batch->fData.fFillTexture1 = NULL;
	batch->fData.fMaskTexture = NULL;
	batch->fData.fMaskUniform = NULL;
	batch->fData.fUserUniform0 = NULL;
	batch->fData.fUserUniform1 = NULL;
	batch->fData.fUserUniform2 = NULL;
	batch->fData.fUserUniform3 = NULL;
	batch->fData.fGeometry = batch->fGeometry;

	return batch;
}

int
BatchObject::AddSlot( int frameIndex, Real x, Real y )
{
	// Look for a reusable inactive slot
	for ( S32 i = 0, iMax = fSlots.Length(); i < iMax; i++ )
	{
		if ( !fSlots[i].isActive )
		{
			Slot& slot = fSlots[i];
			slot.frameIndex = frameIndex;
			slot.x = x;
			slot.y = y;
			slot.scaleX = Rtt_REAL_1;
			slot.scaleY = Rtt_REAL_1;
			slot.rotation = Rtt_REAL_0;
			slot.alpha = Rtt_REAL_1;
			slot.isVisible = true;
			slot.isDirty = true;
			slot.isActive = true;
			fActiveCount++;
			fVerticesDirty = true;
			Invalidate( kGeometryFlag );
			return i;
		}
	}

	// No reusable slot — append
	Slot slot;
	slot.frameIndex = frameIndex;
	slot.x = x;
	slot.y = y;
	slot.scaleX = Rtt_REAL_1;
	slot.scaleY = Rtt_REAL_1;
	slot.rotation = Rtt_REAL_0;
	slot.alpha = Rtt_REAL_1;
	slot.isVisible = true;
	slot.isDirty = true;
	slot.isActive = true;

	int id = fSlots.Length();
	fSlots.Append( slot );
	fActiveCount++;
	fVerticesDirty = true;
	Invalidate( kGeometryFlag );
	return id;
}

void
BatchObject::RemoveSlot( int slotId )
{
	if ( slotId >= 0 && slotId < fSlots.Length() && fSlots[slotId].isActive )
	{
		fSlots[slotId].isActive = false;
		fSlots[slotId].isVisible = false;
		fActiveCount--;
		fVerticesDirty = true;
		Invalidate( kGeometryFlag );
	}
}

BatchObject::Slot*
BatchObject::GetSlot( int slotId )
{
	if ( slotId >= 0 && slotId < fSlots.Length() && fSlots[slotId].isActive )
	{
		return &fSlots[slotId];
	}
	return NULL;
}

const BatchObject::Slot*
BatchObject::GetSlot( int slotId ) const
{
	if ( slotId >= 0 && slotId < fSlots.Length() && fSlots[slotId].isActive )
	{
		return &fSlots[slotId];
	}
	return NULL;
}

void
BatchObject::Clear()
{
	for ( S32 i = 0, iMax = fSlots.Length(); i < iMax; i++ )
	{
		fSlots[i].isActive = false;
		fSlots[i].isVisible = false;
	}
	fActiveCount = 0;
	fVerticesDirty = true;
	Invalidate( kGeometryFlag );
}

void
BatchObject::RebuildVertices() const
{
	if ( !fVerticesDirty ) return;

	int slotCount = fSlots.Length();
	int neededVertices = fActiveCount * VERTICES_PER_QUAD;

	// Ensure buffer is large enough
	if ( fGeometry->GetVerticesAllocated() < (U32)neededVertices )
	{
		U32 newCapacity = fGeometry->GetVerticesAllocated();
		if ( newCapacity == 0 ) newCapacity = VERTICES_PER_QUAD;
		while ( (int)newCapacity < neededVertices )
		{
			newCapacity *= 2;
		}
		fGeometry->Resize( newCapacity, false );
	}

	Geometry::Vertex* verts = fGeometry->GetVertexData();
	int vertIndex = 0;

	// Get the object's src-to-dst transform for world positioning
	const Matrix& xform = GetSrcToDstMatrix();

	for ( int i = 0; i < slotCount; i++ )
	{
		const Slot& slot = fSlots[i];
		if ( !slot.isActive || !slot.isVisible ) continue;

		if ( slot.frameIndex < 0 || slot.frameIndex >= fAtlas->GetFrameCount() ) continue;

		const TextureAtlas::Frame& frame = fAtlas->GetFrameByIndex( slot.frameIndex );

		Real halfW = Rtt_RealDiv2( Rtt_IntToReal( frame.w ) ) * slot.scaleX;
		Real halfH = Rtt_RealDiv2( Rtt_IntToReal( frame.h ) ) * slot.scaleY;

		// Compute rotated quad corners in local space
		Real cosR = Rtt_REAL_1;
		Real sinR = Rtt_REAL_0;
		if ( slot.rotation != Rtt_REAL_0 )
		{
			Real radians = slot.rotation * (Real)M_PI / Rtt_IntToReal( 180 );
			cosR = (Real)cos( (double)radians );
			sinR = (Real)sin( (double)radians );
		}

		// Quad corners relative to slot center (before rotation)
		struct Corner { Real lx, ly; };
		Corner corners[4] = {
			{ -halfW, -halfH }, // TL
			{  halfW, -halfH }, // TR
			{ -halfW,  halfH }, // BL
			{  halfW,  halfH }, // BR
		};

		// Apply rotation and translate to slot position in local coords,
		// then apply the parent transform via Matrix::Apply
		Vertex2 worldCorners[4];
		for ( int c = 0; c < 4; c++ )
		{
			Real rx = corners[c].lx * cosR - corners[c].ly * sinR + slot.x;
			Real ry = corners[c].lx * sinR + corners[c].ly * cosR + slot.y;
			worldCorners[c].x = rx;
			worldCorners[c].y = ry;
		}
		xform.Apply( worldCorners, 4 );

		// Color: white with slot alpha
		U8 alpha8 = (U8)( Rtt_RealToFloat( slot.alpha ) * 255.0f );

		// Triangle 1: TL, TR, BL  (corners 0, 1, 2)
		// Triangle 2: TR, BR, BL  (corners 1, 3, 2)
		int triOrder[6] = { 0, 1, 2, 1, 3, 2 };
		Real uvs[4][2] = {
			{ frame.u0, frame.v0 }, // TL
			{ frame.u1, frame.v0 }, // TR
			{ frame.u0, frame.v1 }, // BL
			{ frame.u1, frame.v1 }, // BR
		};

		for ( int t = 0; t < 6; t++ )
		{
			int ci = triOrder[t];
			Geometry::Vertex& vert = verts[vertIndex++];
			vert.Zero();
			vert.x = worldCorners[ci].x;
			vert.y = worldCorners[ci].y;
			vert.z = Rtt_REAL_0;
			vert.u = uvs[ci][0];
			vert.v = uvs[ci][1];
			vert.q = Rtt_REAL_0;
			vert.rs = 0xFF;
			vert.gs = 0xFF;
			vert.bs = 0xFF;
			vert.as = alpha8;
			vert.ux = Rtt_REAL_0;
			vert.uy = Rtt_REAL_0;
			vert.uz = Rtt_REAL_0;
			vert.uw = Rtt_REAL_0;
		}
	}

	fGeometry->SetVerticesUsed( vertIndex );
	fGeometry->Invalidate();
	fVerticesDirty = false;
}

void
BatchObject::FillInstanceData() const
{
	InstancedBatchRenderer& inst = InstancedBatchRenderer::Instance();

	// Allocate transient instance buffer (valid for this frame only)
	bgfx::allocInstanceDataBuffer(
		&fInstanceDrawData.instanceBuffer,
		fActiveCount,
		InstancedBatchRenderer::kInstanceStride );

	float* data = (float*)fInstanceDrawData.instanceBuffer.data;
	const Matrix& parentXform = GetSrcToDstMatrix();

	int slotCount = fSlots.Length();
	int written = 0;

	for ( int i = 0; i < slotCount && written < fActiveCount; i++ )
	{
		const Slot& slot = fSlots[i];
		if ( !slot.isActive || !slot.isVisible ) continue;
		if ( slot.frameIndex < 0 || slot.frameIndex >= fAtlas->GetFrameCount() ) continue;

		const TextureAtlas::Frame& frame = fAtlas->GetFrameByIndex( slot.frameIndex );

		Real halfW = Rtt_RealDiv2( Rtt_IntToReal( frame.w ) ) * slot.scaleX;
		Real halfH = Rtt_RealDiv2( Rtt_IntToReal( frame.h ) ) * slot.scaleY;

		// Build local 2D transform: scale by frame size, rotate, translate
		Real cosR = Rtt_REAL_1;
		Real sinR = Rtt_REAL_0;
		if ( slot.rotation != Rtt_REAL_0 )
		{
			Real radians = slot.rotation * (Real)M_PI / Rtt_IntToReal( 180 );
			cosR = (Real)cos( (double)radians );
			sinR = (Real)sin( (double)radians );
		}

		// Local model matrix = Translate(slot.x, slot.y) * Rotate(rotation) * Scale(halfW*2, halfH*2)
		// The base quad is -0.5..0.5, so scaling by (frameW, frameH) makes it pixel-sized.
		Real scaleW = halfW * Rtt_IntToReal( 2 );
		Real scaleH = halfH * Rtt_IntToReal( 2 );

		// Combined: T * R * S in column-major form
		// col0 = ( scaleW*cosR, scaleW*sinR, 0 )
		// col1 = (-scaleH*sinR, scaleH*cosR, 0 )
		// col2 = ( 0, 0, 1 )
		// col3 = ( slot.x, slot.y, 0 )
		float localMat[16] = {
			(float)( scaleW * cosR ), (float)( scaleW * sinR ), 0.0f, 0.0f,  // col 0
			(float)(-scaleH * sinR ), (float)( scaleH * cosR ), 0.0f, 0.0f,  // col 1
			0.0f, 0.0f, 1.0f, 0.0f,                                           // col 2
			(float)slot.x, (float)slot.y, 0.0f, 1.0f                          // col 3
		};

		// Multiply by parent world transform: worldMat = parentXform * localMat
		// Matrix stores row-major: Row0=[a, b, tx], Row1=[c, d, ty]
		// As column-major 4x4:
		// col0=(a,c,0,0), col1=(b,d,0,0), col2=(0,0,1,0), col3=(tx,ty,0,1)
		float parentMat[16];
		if ( parentXform.IsIdentity() )
		{
			parentMat[0]=1; parentMat[1]=0; parentMat[2]=0; parentMat[3]=0;
			parentMat[4]=0; parentMat[5]=1; parentMat[6]=0; parentMat[7]=0;
			parentMat[8]=0; parentMat[9]=0; parentMat[10]=1; parentMat[11]=0;
			parentMat[12]=0; parentMat[13]=0; parentMat[14]=0; parentMat[15]=1;
		}
		else
		{
			const Real* r0 = parentXform.Row0(); // [a, b, tx]
			const Real* r1 = parentXform.Row1(); // [c, d, ty]
			parentMat[0]  = (float)r0[0]; parentMat[1]  = (float)r1[0]; parentMat[2]  = 0.0f; parentMat[3]  = 0.0f;
			parentMat[4]  = (float)r0[1]; parentMat[5]  = (float)r1[1]; parentMat[6]  = 0.0f; parentMat[7]  = 0.0f;
			parentMat[8]  = 0.0f;         parentMat[9]  = 0.0f;         parentMat[10] = 1.0f; parentMat[11] = 0.0f;
			parentMat[12] = (float)r0[2]; parentMat[13] = (float)r1[2]; parentMat[14] = 0.0f; parentMat[15] = 1.0f;
		}

		// worldMat = parentMat * localMat (column-major matrix multiply)
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

		// Pack into 5 vec4s (80 bytes): cols 0,1,3 of worldMat + UV + color
		// Col 2 is always (0,0,1,0) for 2D affine — reconstructed in shader
		// Shader: mat4 model = mtxFromCols(i_data0, i_data1, vec4(0,0,1,0), i_data2);

		// i_data0 = col 0 of worldMat
		data[0] = worldMat[0];
		data[1] = worldMat[1];
		data[2] = worldMat[2];
		data[3] = worldMat[3];

		// i_data1 = col 1 of worldMat
		data[4] = worldMat[4];
		data[5] = worldMat[5];
		data[6] = worldMat[6];
		data[7] = worldMat[7];

		// i_data2 = col 3 of worldMat (translation)
		data[8]  = worldMat[12];
		data[9]  = worldMat[13];
		data[10] = worldMat[14];
		data[11] = worldMat[15];

		// i_data3 = UV rect (u0, v0, u1, v1)
		data[12] = (float)frame.u0;
		data[13] = (float)frame.v0;
		data[14] = (float)frame.u1;
		data[15] = (float)frame.v1;

		// i_data4 = color (r, g, b, a)
		data[16] = 1.0f;
		data[17] = 1.0f;
		data[18] = 1.0f;
		data[19] = (float)Rtt_RealToFloat( slot.alpha );

		data += InstancedBatchRenderer::kInstanceStride / sizeof(float);
		written++;
	}

	fInstanceDrawData.instanceCount = written;
	fInstanceDrawData.programHandle = inst.GetProgram();
	fInstanceDrawData.baseQuadVB = inst.GetBaseQuadVB();
	fInstanceDrawData.baseQuadIB = inst.GetBaseQuadIB();
}

void
BatchObject::Prepare( const Display& display )
{
	Super::Prepare( display );

	if ( ShouldPrepare() )
	{
		InstancedBatchRenderer& inst = InstancedBatchRenderer::Instance();
		if ( !inst.IsAvailable() )
		{
			inst.Initialize();
		}

		fUseInstancing = inst.IsAvailable() && fActiveCount > 0;

		if ( fUseInstancing )
		{
			FillInstanceData();
			fData.fInstanceDraw = &fInstanceDrawData;
			// Still need geometry for the pipeline (even though we override it)
			fGeometry->SetVerticesUsed( 0 );
		}
		else
		{
			fData.fInstanceDraw = NULL;
			fVerticesDirty = true;
			RebuildVertices();
		}

		fShader->Prepare( fData, 0, 0, ShaderResource::kDefault );

		SetValid( kGeometryFlag |
					kPaintFlag |
					kColorFlag |
					kProgramFlag |
					kProgramDataFlag );
	}
}

void
BatchObject::Draw( Renderer& renderer ) const
{
	if ( !ShouldDraw() || fActiveCount == 0 )
	{
		return;
	}

	if ( fUseInstancing )
	{
		// Need some vertices for the pipeline to accept this geometry
		// Set a minimal 6-vertex placeholder so FlushBatch processes it
		if ( fGeometry->GetVerticesUsed() == 0 )
		{
			// Ensure at least 6 vertices allocated for pipeline to accept
			if ( fGeometry->GetVerticesAllocated() < 6 )
			{
				fGeometry->Resize( 6, false );
			}
			fGeometry->SetVerticesUsed( 6 );
			fGeometry->Invalidate();
		}
	}

	fShader->Draw( renderer, fData );
}

void
BatchObject::GetSelfBounds( Rect& rect ) const
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
		const Slot& slot = fSlots[i];
		if ( !slot.isActive || !slot.isVisible ) continue;

		if ( slot.frameIndex < 0 || slot.frameIndex >= fAtlas->GetFrameCount() ) continue;

		const TextureAtlas::Frame& frame = fAtlas->GetFrameByIndex( slot.frameIndex );
		Real halfW = Rtt_RealDiv2( Rtt_IntToReal( frame.w ) ) * slot.scaleX;
		Real halfH = Rtt_RealDiv2( Rtt_IntToReal( frame.h ) ) * slot.scaleY;

		// Account for rotation: use bounding radius as half-extent
		Real boundRadius = Rtt_RealSqrt( Rtt_RealMul( halfW, halfW ) + Rtt_RealMul( halfH, halfH ) );

		Real left = slot.x - boundRadius;
		Real right = slot.x + boundRadius;
		Real top = slot.y - boundRadius;
		Real bottom = slot.y + boundRadius;

		if ( left < xMin ) xMin = left;
		if ( right > xMax ) xMax = right;
		if ( top < yMin ) yMin = top;
		if ( bottom > yMax ) yMax = bottom;
	}

	rect.Initialize( xMin, yMin, xMax, yMax );
}

bool
BatchObject::HitTest( Real contentX, Real contentY )
{
	return false;
}

const LuaProxyVTable&
BatchObject::ProxyVTable() const
{
	return LuaBatchObjectProxyVTable::Constant();
}

void
BatchObject::RemovedFromParent( lua_State * L, GroupObject * parent )
{
	fRemoved = true;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------


#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
