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
#include "Renderer/Rtt_Geometry_Renderer.h"
#include "Renderer/Rtt_Renderer.h"
#include "Rtt_LuaProxyVTable.h"
#include "Rtt_Matrix.h"
#include "Core/Rtt_Geometry.h"

#include <cmath>

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
	fRemoved( false )
{
	SetObjectDesc( "BatchObject" );
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
BatchObject::Prepare( const Display& display )
{
	Super::Prepare( display );

	if ( ShouldPrepare() )
	{
		fVerticesDirty = true; // Transform may have changed
		RebuildVertices();

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
