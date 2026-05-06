#include "Core/Rtt_Config.h"
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Renderer/Rtt_BgfxGeometry.h"

#include "Renderer/Rtt_Geometry_Renderer.h"
#include <stdio.h>
#include "Core/Rtt_Assert.h"
#include "Rtt_Profiling.h"

#include <string.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// Static members
bgfx::VertexLayout BgfxGeometry::sVertexLayout;
bool BgfxGeometry::sLayoutInitialized = false;
U32 BgfxGeometry::sFrameCount = 0;

void
BgfxGeometry::InitializeVertexLayout()
{
	if( !sLayoutInitialized )
	{
		// 68 bytes stride layout (008 mask-PV):
		// Position(3f) + TexCoord0(3f) + Color0(4ub normalized) + TexCoord1(4f)
		// + TexCoord2/3/4(2f each, mask UV slots consumed by vs_default_mN binaries)
		sVertexLayout
			.begin()
			.add( bgfx::Attrib::Position,  3, bgfx::AttribType::Float )           // offset 0,  12 bytes
			.add( bgfx::Attrib::TexCoord0, 3, bgfx::AttribType::Float )           // offset 12, 12 bytes
			.add( bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true )     // offset 24, 4 bytes (normalized)
			.add( bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float )           // offset 28, 16 bytes
			.add( bgfx::Attrib::TexCoord2, 2, bgfx::AttribType::Float )           // offset 44, 8 bytes (mask UV 0)
			.add( bgfx::Attrib::TexCoord3, 2, bgfx::AttribType::Float )           // offset 52, 8 bytes (mask UV 1)
			.add( bgfx::Attrib::TexCoord4, 2, bgfx::AttribType::Float )           // offset 60, 8 bytes (mask UV 2)
			.end();

		sLayoutInitialized = true;

		// Verify stride matches expected 68 bytes (must stay in lockstep with
		// Geometry::Vertex layout — see Rtt_Geometry_Renderer.h).
		Rtt_ASSERT( sVertexLayout.getStride() == 68 );
	}
}

BgfxGeometry::BgfxGeometry()
:	fVertexBufferHandle( BGFX_INVALID_HANDLE ),
	fDynamicVertexBufferHandle( BGFX_INVALID_HANDLE ),
	fIndexBufferHandle( BGFX_INVALID_HANDLE ),
	fDynamicIndexBufferHandle( BGFX_INVALID_HANDLE ),
	fHasInstanceBuffer( false ),
	fIsTransient( false ),
	fHasTransientVB( false ),
	fHasTransientIB( false ),
	fVertexCount( 0 ),
	fIndexCount( 0 ),
	fInstancesAllocated( 0 ),
	fIsDynamic( false ),
	fHasIndexBuffer( false ),
	fWasStoredOnGPU( false ),
	fLastUpdateFrame( 0 )
{
	InitializeVertexLayout();
}

BgfxGeometry::~BgfxGeometry()
{
	Destroy();
}

void
BgfxGeometry::ResetForReuse()
{
	fVertexBufferHandle = BGFX_INVALID_HANDLE;
	fDynamicVertexBufferHandle = BGFX_INVALID_HANDLE;
	fIndexBufferHandle = BGFX_INVALID_HANDLE;
	fDynamicIndexBufferHandle = BGFX_INVALID_HANDLE;
	fHasInstanceBuffer = false;
	fIsTransient = false;
	fHasTransientVB = false;
	fHasTransientIB = false;
	fVertexCount = 0;
	fIndexCount = 0;
	fInstancesAllocated = 0;
	fIsDynamic = false;
	fHasIndexBuffer = false;
	fWasStoredOnGPU = false;
	fLastUpdateFrame = 0;
	fHandle = NULL;
	// fTransientVB / fTransientIB / fInstanceDataBuffer are stack structs;
	// their contents are irrelevant when the above flags are false.
}

bool
BgfxGeometry::SupportsInstancing()
{
	// bgfx supports instancing natively
	return true;
}

bool
BgfxGeometry::SupportsDivisors()
{
	// bgfx handles instance rate data differently - we always return true
	return true;
}

const char*
BgfxGeometry::InstanceIDSuffix()
{
	// bgfx uses its own instancing system, no suffix needed
	return "";
}

void
BgfxGeometry::CreateStatic( Geometry* geometry )
{
	const Geometry::Vertex* vertexData = geometry->GetVertexData();

	if( !vertexData )
	{
		Rtt_LogException( "Unable to initialize BgfxGeometry. Data is NULL" );
		return;
	}

	const U32 vertexCount = geometry->GetVerticesAllocated();
	const size_t vertexDataSize = vertexCount * sizeof( Geometry::Vertex );

	// Create static vertex buffer
	const bgfx::Memory* vertexMem = bgfx::copy( vertexData, static_cast<uint32_t>( vertexDataSize ) );
	fVertexBufferHandle = bgfx::createVertexBuffer( vertexMem, sVertexLayout );

	// Create index buffer if present
	const Geometry::Index* indexData = geometry->GetIndexData();
	const U32 indexCount = geometry->GetIndicesAllocated();

	if( indexData )
	{
		const size_t indexDataSize = indexCount * sizeof( Geometry::Index );

		const bgfx::Memory* indexMem = bgfx::copy( indexData, static_cast<uint32_t>( indexDataSize ) );
		fIndexBufferHandle = bgfx::createIndexBuffer( indexMem, BGFX_BUFFER_NONE );
		fHasIndexBuffer = true;
	}


	fVertexCount = vertexCount;
	fIsDynamic = false;
}

void
BgfxGeometry::CreateDynamic( Geometry* geometry )
{
	const U32 vertexCount = geometry->GetVerticesAllocated();
	
	// Create dynamic vertex buffer with resize capability
	fDynamicVertexBufferHandle = bgfx::createDynamicVertexBuffer( 
		vertexCount, 
		sVertexLayout, 
		BGFX_BUFFER_ALLOW_RESIZE
	);

	// Create dynamic index buffer if present
	const Geometry::Index* indexData = geometry->GetIndexData();
	if( indexData )
	{
		const U32 indexCount = geometry->GetIndicesAllocated();
		fDynamicIndexBufferHandle = bgfx::createDynamicIndexBuffer( 
			indexCount, 
			BGFX_BUFFER_ALLOW_RESIZE | BGFX_BUFFER_NONE 
		);
		fHasIndexBuffer = true;
	}

	fVertexCount = vertexCount;
	fIsDynamic = true;
}

void
BgfxGeometry::Create( CPUResource* resource )
{
	Rtt_ASSERT( CPUResource::kGeometry == resource->GetType() );
	Geometry* geometry = static_cast<Geometry*>( resource );

	bool shouldStoreOnGPU = geometry->GetStoredOnGPU();
	const Geometry::Index* indexData = geometry->GetIndexData();
	U32 indexCount = geometry->GetIndicesAllocated();


	if( shouldStoreOnGPU )
	{
		SUMMED_TIMING( bgfxgc, "Bgfx Geometry GPU Resource (stored on GPU): Create" );
		CreateStatic( geometry );
		fWasStoredOnGPU = true;
		fLastUpdateFrame = sFrameCount;
	}
	else
	{
		SUMMED_TIMING( bgfxgc, "Bgfx Geometry GPU Resource (transient): Create" );
		// Use transient buffers for pool geometries - no persistent GPU allocation needed
		fIsTransient = true;
		fVertexCount = geometry->GetVerticesAllocated();
		UpdateTransient( geometry );
	}

	fIndexCount = geometry->GetIndicesAllocated();
}

void
BgfxGeometry::UpdateStatic( Geometry* geometry )
{
	const Geometry::Vertex* vertexData = geometry->GetVertexData();

	if( !vertexData )
	{
		Rtt_LogException( "Unable to update BgfxGeometry. Data is NULL" );
		return;
	}

	// Convert static to dynamic for efficient future updates.
	// bgfx static buffers cannot be updated in-place, so switching to dynamic
	// eliminates the destroy+create overhead on each subsequent update.
	DestroyStatic();
	CreateDynamic( geometry );

	// Upload current data to the new dynamic buffer
	const U32 vertexCount = geometry->GetVerticesAllocated();
	const size_t vertexDataSize = vertexCount * sizeof( Geometry::Vertex );
	const bgfx::Memory* mem = bgfx::copy( vertexData, static_cast<uint32_t>( vertexDataSize ) );
	bgfx::update( fDynamicVertexBufferHandle, 0, mem );

	// Update index data if present
	const Geometry::Index* indexData = geometry->GetIndexData();
	if( indexData && fHasIndexBuffer )
	{
		const U32 indexCount = geometry->GetIndicesAllocated();
		const size_t indexDataSize = indexCount * sizeof( Geometry::Index );
		const bgfx::Memory* indexMem = bgfx::copy( indexData, static_cast<uint32_t>( indexDataSize ) );
		bgfx::update( fDynamicIndexBufferHandle, 0, indexMem );
	}

	fWasStoredOnGPU = true;
	fIsDynamic = true;
}

void
BgfxGeometry::UpdateDynamic( Geometry* geometry )
{
	const Geometry::Vertex* vertexData = geometry->GetVertexData();
	if( !vertexData )
	{
		Rtt_LogException( "Unable to update BgfxGeometry. Data is NULL" );
		return;
	}

	const U32 vertexCount = geometry->GetVerticesAllocated();
	const size_t vertexDataSize = vertexCount * sizeof( Geometry::Vertex );

	// Check if we need to resize
	if( vertexCount > fVertexCount )
	{
		// bgfx dynamic buffers auto-resize with BGFX_BUFFER_ALLOW_RESIZE
		// but we need to allocate a larger initial space
		DestroyDynamic();
		CreateDynamic( geometry );
	}

	// Update vertex data
	const bgfx::Memory* mem = bgfx::copy( vertexData, static_cast<uint32_t>( vertexDataSize ) );
	bgfx::update( fDynamicVertexBufferHandle, 0, mem );

	// Update index data if present
	const Geometry::Index* indexData = geometry->GetIndexData();
	if( indexData && fHasIndexBuffer )
	{
		const U32 indexCount = geometry->GetIndicesAllocated();
		const size_t indexDataSize = indexCount * sizeof( Geometry::Index );
		
		const bgfx::Memory* indexMem = bgfx::copy( indexData, static_cast<uint32_t>( indexDataSize ) );
		bgfx::update( fDynamicIndexBufferHandle, 0, indexMem );
	}
}

void
BgfxGeometry::UpdateTransient( Geometry* geometry )
{
	const Geometry::Vertex* vertexData = geometry->GetVertexData();
	if( !vertexData )
	{
		fHasTransientVB = false;
		return;
	}

	const U32 vertexCount = geometry->GetVerticesAllocated();
	if( vertexCount == 0 )
	{
		fHasTransientVB = false;
		return;
	}

	// Check transient buffer availability
	if( bgfx::getAvailTransientVertexBuffer( vertexCount, sVertexLayout ) < vertexCount )
	{
		fHasTransientVB = false;
		return;
	}

	bgfx::allocTransientVertexBuffer( &fTransientVB, vertexCount, sVertexLayout );
	memcpy( fTransientVB.data, vertexData, vertexCount * sizeof( Geometry::Vertex ) );
	fVertexCount = vertexCount;
	fHasTransientVB = true;



	// Allocate transient index buffer if geometry has indices
	const Geometry::Index* indexData = geometry->GetIndexData();
	U32 indexCount = geometry->GetIndicesUsed();
	if( indexData && indexCount > 0 && bgfx::getAvailTransientIndexBuffer( indexCount ) >= indexCount )
	{
		bgfx::allocTransientIndexBuffer( &fTransientIB, indexCount );
		memcpy( fTransientIB.data, indexData, indexCount * sizeof( Geometry::Index ) );
		fHasTransientIB = true;
	}
	else
	{
		fHasTransientIB = false;
	}
}

void
BgfxGeometry::PromoteToStatic( Geometry* geometry )
{
	// Promote dynamic buffer back to static after stability period.
	// Static buffers may reside in GPU-local memory, offering better
	// read performance for geometry that no longer changes.
	const Geometry::Vertex* vertexData = geometry->GetVertexData();
	if( !vertexData )
	{
		return;
	}

	DestroyDynamic();
	CreateStatic( geometry );
	fIsDynamic = false;
}

void
BgfxGeometry::Update( CPUResource* resource )
{
	SUMMED_TIMING( bgfxgu, "Bgfx Geometry GPU Resource: Update" );

	Rtt_ASSERT( CPUResource::kGeometry == resource->GetType() );
	Geometry* geometry = static_cast<Geometry*>( resource );

	if( fIsTransient )
	{
		UpdateTransient( geometry );
	}
	else if( fIsDynamic )
	{
		// Auto-promote: if this was a StoredOnGPU geometry that was converted
		// to dynamic (on first update), and it has been stable for many frames,
		// promote back to static for optimal GPU rendering performance.
		if( fWasStoredOnGPU && (sFrameCount - fLastUpdateFrame) > kPromotionThreshold )
		{
			PromoteToStatic( geometry );
		}
		else
		{
			UpdateDynamic( geometry );
		}
		fLastUpdateFrame = sFrameCount;
	}
	else
	{
		UpdateStatic( geometry );
		fLastUpdateFrame = sFrameCount;
	}

	fIndexCount = geometry->GetIndicesAllocated();
	geometry->ClearGPUDirty();
}

void
BgfxGeometry::DestroyStatic()
{
	if( bgfx::isValid( fVertexBufferHandle ) )
	{
		bgfx::destroy( fVertexBufferHandle );
		fVertexBufferHandle = BGFX_INVALID_HANDLE;
	}
	
	if( bgfx::isValid( fIndexBufferHandle ) )
	{
		bgfx::destroy( fIndexBufferHandle );
		fIndexBufferHandle = BGFX_INVALID_HANDLE;
	}
}

void
BgfxGeometry::DestroyDynamic()
{
	if( bgfx::isValid( fDynamicVertexBufferHandle ) )
	{
		bgfx::destroy( fDynamicVertexBufferHandle );
		fDynamicVertexBufferHandle = BGFX_INVALID_HANDLE;
	}
	
	if( bgfx::isValid( fDynamicIndexBufferHandle ) )
	{
		bgfx::destroy( fDynamicIndexBufferHandle );
		fDynamicIndexBufferHandle = BGFX_INVALID_HANDLE;
	}
}

void
BgfxGeometry::Destroy()
{
	if( fIsTransient )
	{
		// Transient buffers have no persistent handles to destroy
		fHasTransientVB = false;
		fHasTransientIB = false;
	}
	else if( fIsDynamic )
	{
		DestroyDynamic();
	}
	else
	{
		DestroyStatic();
	}

	fVertexCount = 0;
	fIndexCount = 0;
	fHasIndexBuffer = false;
}

void
BgfxGeometry::Bind()
{
	// bgfx doesn't require explicit binding like GL
	// The vertex buffer is set during Draw() via SetVertexBuffer
}

void
BgfxGeometry::SetVertexBuffer( U32 offset, U32 count )
{
	if( fIsTransient )
	{
		if( fHasTransientVB )
		{
			bgfx::setVertexBuffer( 0, &fTransientVB, static_cast<uint32_t>( offset ), static_cast<uint32_t>( count ) );
		}
		// No transient VB available - skip
	}
	else if( fIsDynamic )
	{
		if( bgfx::isValid( fDynamicVertexBufferHandle ) )
		{
			bgfx::setVertexBuffer( 0, fDynamicVertexBufferHandle, static_cast<uint32_t>( offset ), static_cast<uint32_t>( count ) );
		}
	}
	else
	{
		if( bgfx::isValid( fVertexBufferHandle ) )
		{
			bgfx::setVertexBuffer( 0, fVertexBufferHandle, static_cast<uint32_t>( offset ), static_cast<uint32_t>( count ) );
		}
	}
}

void
BgfxGeometry::SetIndexBuffer( U32 offset, U32 count )
{
	if( fIsTransient )
	{
		if( fHasTransientIB )
		{
			bgfx::setIndexBuffer( &fTransientIB, static_cast<uint32_t>( offset ), static_cast<uint32_t>( count ) );
		}
		return;
	}

	if( !fHasIndexBuffer )
	{
		return;
	}

	if( fIsDynamic )
	{
		if( bgfx::isValid( fDynamicIndexBufferHandle ) )
		{
			bgfx::setIndexBuffer( fDynamicIndexBufferHandle, static_cast<uint32_t>( offset ), static_cast<uint32_t>( count ) );
		}
	}
	else
	{
		if( bgfx::isValid( fIndexBufferHandle ) )
		{
			bgfx::setIndexBuffer( fIndexBufferHandle, static_cast<uint32_t>( offset ), static_cast<uint32_t>( count ) );
		}
	}
}

bgfx::InstanceDataBuffer*
BgfxGeometry::AcquireInstanceBuffer( U32 count )
{
	if( count == 0 )
	{
		return NULL;
	}

	// Allocate instance data buffer from bgfx
	// Each instance data is a 4x4 matrix (16 floats) by default
	// But we can use custom instance data layout
	bgfx::allocInstanceDataBuffer( &fInstanceDataBuffer, count, sizeof( float ) * 16 );
	fHasInstanceBuffer = true;
	return &fInstanceDataBuffer;
}

void
BgfxGeometry::SetInstanceDataBuffer( bgfx::InstanceDataBuffer* buffer, U32 count )
{
	if( buffer && count > 0 )
	{
		bgfx::setInstanceDataBuffer( buffer );
	}
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------


#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
