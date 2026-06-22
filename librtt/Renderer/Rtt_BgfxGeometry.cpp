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
#include "Renderer/Rtt_FormatExtensionList.h"
#include "Renderer/Rtt_BgfxVertexExtension.h"
#include "Corona/CoronaGraphics.h"
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
		// 68 bytes stride layout (H2 unified mask VS):
		// Position(3f) + TexCoord0(3f) + Color0(4ub normalized) + TexCoord1(4f)
		// + TexCoord2/3/4(2f each, mask UV slots consumed by the default VS)
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
	fPendingGeometry( nullptr ),
	fVertexCount( 0 ),
	fIndexCount( 0 ),
	fInstancesAllocated( 0 ),
	fIsDynamic( false ),
	fHasIndexBuffer( false ),
	fWasStoredOnGPU( false ),
	fLastUpdateFrame( 0 ),
	fHasExtLayout( false ),
	fPoolExtList( NULL ),
	fPoolExtInterleaved( false )
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
	fPendingGeometry = nullptr;
	fVertexCount = 0;
	fIndexCount = 0;
	fInstancesAllocated = 0;
	fIsDynamic = false;
	fHasIndexBuffer = false;
	fWasStoredOnGPU = false;
	fLastUpdateFrame = 0;
	fHasExtLayout = false;
	fPoolExtList = NULL;
	fPoolExtInterleaved = false;
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

// bgfx attribute slots that carry vertex format-extension attributes, in the
// SAME order as kBgfxExtensionAttribNames (Rtt_BgfxVertexExtension.h). The
// generated vertex shader references the names; the layout here binds the
// matching enum — both must stay aligned.
static bgfx::Attrib::Enum
ExtensionSlotAttrib( U32 index )
{
	static const bgfx::Attrib::Enum kSlots[] =
	{
		bgfx::Attrib::TexCoord5,
		bgfx::Attrib::TexCoord6,
		bgfx::Attrib::TexCoord7,
		bgfx::Attrib::Normal,
		bgfx::Attrib::Tangent,
		bgfx::Attrib::Bitangent,
		bgfx::Attrib::Color1,
		bgfx::Attrib::Color2,
		bgfx::Attrib::Color3,
	};
	static_assert( sizeof( kSlots ) / sizeof( kSlots[0] ) == kBgfxExtensionAttribSlotCount,
		"extension slot enum table must match kBgfxExtensionAttribNames" );
	return kSlots[ index ];
}

const FormatExtensionList*
BgfxGeometry::EffectiveExtList( Geometry* geometry ) const
{
	const FormatExtensionList* list = geometry->GetExtensionList();
	if ( NULL != list )
	{
		return list;
	}
	// Pool/batched geometry carries no list of its own; use the per-draw override.
	return fPoolExtList;
}

bool
BgfxGeometry::GeometryHasVertexExtension( Geometry* geometry ) const
{
	const FormatExtensionList* list = EffectiveExtList( geometry );
	return NULL != list && list->HasVertexRateData();
}

void
BgfxGeometry::BuildExtendedLayout( const FormatExtensionList* list, bgfx::VertexLayout& outLayout )
{
	// Base 68-byte layout, byte-identical to sVertexLayout.
	outLayout
		.begin()
		.add( bgfx::Attrib::Position,  3, bgfx::AttribType::Float )
		.add( bgfx::Attrib::TexCoord0, 3, bgfx::AttribType::Float )
		.add( bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true )
		.add( bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float )
		.add( bgfx::Attrib::TexCoord2, 2, bgfx::AttribType::Float )
		.add( bgfx::Attrib::TexCoord3, 2, bgfx::AttribType::Float )
		.add( bgfx::Attrib::TexCoord4, 2, bgfx::AttribType::Float );

	const U32 kBaseStride = (U32) sizeof( Geometry::Vertex ); // 68
	U32 runningOffset = kBaseStride;

	// Extension attributes live in the region after the base vertex, at
	// byte offset (kBaseStride + attribute.offset) — matches GL's
	// BindExtensionAttribute (offsetExtra = sizeof(Vertex)).
	list->SortNames();
	const U32 count = list->GetAttributeCount();
	for ( U32 i = 0; i < count && i < kBgfxExtensionAttribSlotCount; ++i )
	{
		S32 attrIdx = -1;
		list->FindNameByAttribute( i, &attrIdx );
		const FormatExtensionList::Attribute& attr =
			list->GetAttributes()[ attrIdx >= 0 ? (U32) attrIdx : i ];

		const U32 targetOffset = kBaseStride + attr.offset;
		if ( targetOffset > runningOffset )
		{
			outLayout.skip( (uint8_t)( targetOffset - runningOffset ) );
			runningOffset = targetOffset;
		}

		// Use the attribute's real type/normalized (a "byte" type stays
		// Uint8; do not infer normalization from the bgfx slot).
		const bgfx::AttribType::Enum type =
			( kAttributeType_Byte == attr.type )
				? bgfx::AttribType::Uint8
				: bgfx::AttribType::Float;

		outLayout.add( ExtensionSlotAttrib( i ), attr.components, type, 0 != attr.normalized );
		runningOffset += attr.GetSize();
	}

	// Pad the stride to the full extended vertex size so the layout steps
	// over the whole (base + extension) unit, matching GetWritableExtendedVertexData.
	const U32 fullStride = (U32) FormatExtensionList::GetVertexSize( list );
	if ( fullStride > runningOffset )
	{
		outLayout.skip( (uint8_t)( fullStride - runningOffset ) );
	}
	outLayout.end();
}

const bgfx::VertexLayout&
BgfxGeometry::ResolveLayout( Geometry* geometry )
{
	if ( GeometryHasVertexExtension( geometry ) )
	{
		const FormatExtensionList* list = EffectiveExtList( geometry );

		// For pooled geometry the override list can change between frames (the
		// same pool object is reused), so always rebuild from the current list.
		// For a geometry that owns its (stable) list, build once and cache.
		const bool isPoolOverride = ( NULL == geometry->GetExtensionList() );
		if ( isPoolOverride || !fHasExtLayout )
		{
			BuildExtendedLayout( list, fExtLayout );
			fHasExtLayout = true;
		}
		return fExtLayout;
	}
	return sVertexLayout;
}

const Geometry::Vertex*
BgfxGeometry::PrepareVertexUpload( Geometry* geometry, U32 vertexCount,
                                   U32& outStrideBytes, U32& outTotalBytes )
{
	if ( GeometryHasVertexExtension( geometry ) )
	{
		const FormatExtensionList* list = EffectiveExtList( geometry );

		// Pool/batched path: the main buffer already holds interleaved
		// (base+extension) units written by Renderer::CopyExtendedVertexData.
		// Upload it raw with the extended stride — no re-splice.
		if ( fPoolExtInterleaved )
		{
			outStrideBytes = (U32) FormatExtensionList::GetVertexSize( list );
			outTotalBytes = vertexCount * outStrideBytes;
			return geometry->GetVertexData();
		}

		const Geometry::Vertex* base = geometry->GetVertexData();
		Geometry::Vertex* extended = geometry->GetWritableExtendedVertexData();

		if ( NULL != base && NULL != extended )
		{
			// Splice base vertices into slot 0 of each extended unit; the
			// extension values already occupy the trailing slots (mirrors
			// GLGeometry::SpliceVertexRateData).
			const U32 total = 1 + list->ExtraVertexCount();
			for ( U32 i = 0, j = 0; j < vertexCount; i += total, ++j )
			{
				extended[ i ] = base[ j ];
			}

			outStrideBytes = (U32) FormatExtensionList::GetVertexSize( list );
			outTotalBytes = vertexCount * outStrideBytes;
			return extended;
		}
		// Fall through to base data if the extended buffer is unavailable.
	}

	outStrideBytes = (U32) sizeof( Geometry::Vertex );
	outTotalBytes = vertexCount * outStrideBytes;
	return geometry->GetVertexData();
}

const bgfx::VertexLayout&
BgfxGeometry::ResolveLayoutExt( Geometry* geometry, const FormatExtensionList* list )
{
	// Always (re)build from the supplied list — the explicit list may differ
	// from the geometry's own (pool override) and must win for this draw.
	if ( NULL != list && list->HasVertexRateData() )
	{
		BuildExtendedLayout( list, fExtLayout );
		fHasExtLayout = true;
		return fExtLayout;
	}
	return sVertexLayout;
}

const Geometry::Vertex*
BgfxGeometry::PrepareVertexUploadExt( Geometry* geometry, const FormatExtensionList* list,
                                      U32 vertexCount, U32& outStrideBytes, U32& outTotalBytes )
{
	if ( NULL != list && list->HasVertexRateData() )
	{
		const Geometry::Vertex* base = geometry->GetVertexData();
		Geometry::Vertex* extended = geometry->GetWritableExtendedVertexData();
		if ( NULL != base && NULL != extended )
		{
			// Splice base vertices into slot 0 of each extended unit; the
			// extension values already occupy the trailing slots (mirrors
			// PrepareVertexUpload's non-pool path).
			const U32 total = 1 + list->ExtraVertexCount();
			for ( U32 i = 0, j = 0; j < vertexCount; i += total, ++j )
			{
				extended[ i ] = base[ j ];
			}
			outStrideBytes = (U32) FormatExtensionList::GetVertexSize( list );
			outTotalBytes = vertexCount * outStrideBytes;
			return extended;
		}
	}
	outStrideBytes = (U32) sizeof( Geometry::Vertex );
	outTotalBytes = vertexCount * outStrideBytes;
	return geometry->GetVertexData();
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
	U32 strideBytes = 0, vertexDataSize = 0;
	const Geometry::Vertex* uploadData = PrepareVertexUpload( geometry, vertexCount, strideBytes, vertexDataSize );

	// Create static vertex buffer
	const bgfx::Memory* vertexMem = bgfx::copy( uploadData, static_cast<uint32_t>( vertexDataSize ) );
	fVertexBufferHandle = bgfx::createVertexBuffer( vertexMem, ResolveLayout( geometry ) );

	if( !bgfx::isValid( fVertexBufferHandle ) )
	{
		Rtt_LogException( "ERROR: BgfxGeometry createVertexBuffer FAILED (vertexCount=%u size=%u). Geometry data will not render.\n",
			vertexCount, vertexDataSize );
	}

	// Create index buffer if present
	const Geometry::Index* indexData = geometry->GetIndexData();
	const U32 indexCount = geometry->GetIndicesAllocated();

	if( indexData )
	{
		const size_t indexDataSize = indexCount * sizeof( Geometry::Index );

		const bgfx::Memory* indexMem = bgfx::copy( indexData, static_cast<uint32_t>( indexDataSize ) );
		fIndexBufferHandle = bgfx::createIndexBuffer( indexMem, BGFX_BUFFER_NONE );
		fHasIndexBuffer = true;

		if( !bgfx::isValid( fIndexBufferHandle ) )
		{
			Rtt_LogException( "ERROR: BgfxGeometry createIndexBuffer FAILED (indexCount=%u size=%zu). Geometry data will not render.\n",
				indexCount, indexDataSize );
		}
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
		ResolveLayout( geometry ),
		BGFX_BUFFER_ALLOW_RESIZE
	);

	if( !bgfx::isValid( fDynamicVertexBufferHandle ) )
	{
		Rtt_LogException( "ERROR: BgfxGeometry createDynamicVertexBuffer FAILED (vertexCount=%u). Geometry data will not render.\n",
			vertexCount );
	}

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

		if( !bgfx::isValid( fDynamicIndexBufferHandle ) )
		{
			Rtt_LogException( "ERROR: BgfxGeometry createDynamicIndexBuffer FAILED (indexCount=%u). Geometry data will not render.\n",
				indexCount );
		}
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
		if( bgfx::getCaps()->rendererType == bgfx::RendererType::Vulkan )
		{
			fVertexCount = geometry->GetVerticesAllocated();
			UpdateTransient( geometry );  // Vulkan: eager alloc (baseline behavior)
		}
		else
		{
			fPendingGeometry = geometry;  // GLES: lazy alloc to SetVertexBuffer
			fHasTransientVB = false;
			fHasTransientIB = false;
		}
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
	U32 strideBytes = 0, vertexDataSize = 0;
	const Geometry::Vertex* uploadData = PrepareVertexUpload( geometry, vertexCount, strideBytes, vertexDataSize );
	const bgfx::Memory* mem = bgfx::copy( uploadData, static_cast<uint32_t>( vertexDataSize ) );
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

	// Check if we need to resize
	if( vertexCount > fVertexCount )
	{
		// bgfx dynamic buffers auto-resize with BGFX_BUFFER_ALLOW_RESIZE
		// but we need to allocate a larger initial space
		DestroyDynamic();
		CreateDynamic( geometry );
	}

	// Update vertex data (extended + spliced when the geometry has an extension)
	U32 strideBytes = 0, vertexDataSize = 0;
	const Geometry::Vertex* uploadData = PrepareVertexUpload( geometry, vertexCount, strideBytes, vertexDataSize );
	const bgfx::Memory* mem = bgfx::copy( uploadData, static_cast<uint32_t>( vertexDataSize ) );
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

	// Extended + spliced data when the geometry carries a vertexExtension.
	U32 strideBytes = 0, vertexDataSize = 0;
	const Geometry::Vertex* uploadData = PrepareVertexUpload( geometry, vertexCount, strideBytes, vertexDataSize );
	const bgfx::VertexLayout& layout = ResolveLayout( geometry );

	// Check transient buffer availability
	if( bgfx::getAvailTransientVertexBuffer( vertexCount, layout ) < vertexCount )
	{
		fHasTransientVB = false;
		return;
	}

	bgfx::allocTransientVertexBuffer( &fTransientVB, vertexCount, layout );
	memcpy( fTransientVB.data, uploadData, vertexDataSize );
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

	// Phase 16 experiment: force stored-on-GPU geometry to use transient path
	// to bypass Adreno 530 GLES DynamicVertexBuffer update latency.
	// Hypothesis: bgfx::update() doesn't reach GPU before the next render reads
	// the buffer, leaving stale (initial white) vertex colors after fade-out.
	if( fWasStoredOnGPU && !fIsTransient )
	{
		DestroyDynamic();
		DestroyStatic();
		fIsTransient = true;
		fVertexCount = geometry->GetVerticesAllocated();
	}

	if( fIsTransient )
	{
		if( bgfx::getCaps()->rendererType == bgfx::RendererType::Vulkan )
		{
			UpdateTransient( geometry );  // Vulkan: eager alloc
		}
		else
		{
			fPendingGeometry = geometry;  // GLES: lazy alloc
			fHasTransientVB = false;
			fHasTransientIB = false;
		}
	}
	else if( fIsDynamic )
	{
		// Auto-promote to static after kPromotionThreshold frames without dirty.
		if( !geometry->IsGPUDirty() && (sFrameCount - fLastUpdateFrame) >= kPromotionThreshold )
		{
			PromoteToStatic( geometry );
		}
		else
		{
			UpdateDynamic( geometry );
			fLastUpdateFrame = sFrameCount;
		}
	}
	else
	{
		// Static: only re-upload if data changed (else skip unnecessary update).
		if( geometry->IsGPUDirty() )
		{
			UpdateStatic( geometry );
		}
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
		fPendingGeometry = nullptr;
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
		if( fPendingGeometry )
		{
			UpdateTransient( fPendingGeometry );
			fPendingGeometry = nullptr;
		}
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
BgfxGeometry::SetVertexBufferExt( U32 offset, U32 count, const FormatExtensionList* poolExtList, Geometry* srcGeometry )
{
	// No (vertex-rate) extension, or no pending source data → fall back to the
	// normal path (byte-identical to the non-extension behavior).
	if ( NULL == poolExtList || !poolExtList->HasVertexRateData() )
	{
		SetVertexBuffer( offset, count );
		return;
	}

	// Only the pool/transient path is reached for batched geometry. If this
	// geometry isn't transient (shouldn't happen for pool geometry), fall back.
	if ( !fIsTransient )
	{
		SetVertexBuffer( offset, count );
		return;
	}

	// #079: on Metal/GLES the lazy fPendingGeometry is consumed by the first
	// (base) bind of the frame, so a later extension bind of the same pool
	// geometry finds it NULL. The caller supplies the pool geometry explicitly
	// as srcGeometry so we can still upload the interleaved units with the
	// extended layout instead of silently falling back to the 68-byte bind.
	Geometry* geometry = fPendingGeometry ? fPendingGeometry : srcGeometry;
	if ( NULL == geometry )
	{
		// Vulkan eager path already uploaded with the base layout; for safety
		// fall back rather than risk a stride mismatch.
		SetVertexBuffer( offset, count );
		return;
	}

	const Geometry::Vertex* vertexData = geometry->GetVertexData();
	if ( NULL == vertexData )
	{
		return;
	}

	const U32 factor = 1 + poolExtList->ExtraVertexCount(); // base + extension slots
	const U32 strideBytes = (U32) FormatExtensionList::GetVertexSize( poolExtList ); // factor*68
	const U32 slotCount = geometry->GetVerticesAllocated(); // capacity in 68-byte slots
	const U32 realVertexCapacity = ( factor > 0 ) ? ( slotCount / factor ) : slotCount;
	if ( 0 == realVertexCapacity )
	{
		return;
	}

	// Build the extended layout for this list (pool override always rebuilds).
	fPoolExtList = poolExtList;
	fPoolExtInterleaved = true;
	const bgfx::VertexLayout& layout = ResolveLayout( geometry );

	const U32 totalBytes = realVertexCapacity * strideBytes; // == slotCount * 68

	bool ok = false;
	if ( bgfx::getAvailTransientVertexBuffer( realVertexCapacity, layout ) >= realVertexCapacity )
	{
		bgfx::allocTransientVertexBuffer( &fTransientVB, realVertexCapacity, layout );
		memcpy( fTransientVB.data, vertexData, totalBytes );
		fVertexCount = realVertexCapacity;
		fHasTransientVB = true;
		fPendingGeometry = nullptr; // consumed

		// Index buffer (if any) for this pool geometry.
		const Geometry::Index* indexData = geometry->GetIndexData();
		U32 indexCount = geometry->GetIndicesUsed();
		if ( indexData && indexCount > 0 && bgfx::getAvailTransientIndexBuffer( indexCount ) >= indexCount )
		{
			bgfx::allocTransientIndexBuffer( &fTransientIB, indexCount );
			memcpy( fTransientIB.data, indexData, indexCount * sizeof( Geometry::Index ) );
			fHasTransientIB = true;
		}
		else
		{
			fHasTransientIB = false;
		}

		// Convert the base-slot offset to a real-vertex offset for the extended
		// (stride = factor*68) layout. count is already in real vertices.
		const U32 realOffset = ( factor > 0 ) ? ( offset / factor ) : offset;
		bgfx::setVertexBuffer( 0, &fTransientVB, static_cast<uint32_t>( realOffset ), static_cast<uint32_t>( count ) );
		ok = true;
	}

	// Clear the per-draw override so a later reuse of this pool geometry without
	// an extension takes the normal 68-byte path.
	fPoolExtList = NULL;
	fPoolExtInterleaved = false;

	Rtt_UNUSED( ok );
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
