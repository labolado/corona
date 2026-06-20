//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxGeometry_H__
#define _Rtt_BgfxGeometry_H__

#include "Renderer/Rtt_GPUResource.h"
#include "Renderer/Rtt_Geometry_Renderer.h"
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

class FormatExtensionList;

// ----------------------------------------------------------------------------

class BgfxGeometry : public GPUResource
{
    public:
        typedef GPUResource Super;
        typedef BgfxGeometry Self;

    public:
        BgfxGeometry();
        virtual ~BgfxGeometry();

        /// Reset all state fields so this instance can be reused from the pool.
        /// Must be called after Destroy() and before recycling.
        void ResetForReuse();

        static bool SupportsInstancing();
        static bool SupportsDivisors();
        static const char* InstanceIDSuffix();

        bool StoredOnGPU() const { return bgfx::isValid( fVertexBufferHandle ) || fWasStoredOnGPU; }

        virtual void Create( CPUResource* resource );
        virtual void Update( CPUResource* resource );
        virtual void Destroy();
        virtual bool IsPoolable() const { return true; }

        void Bind();
        void SetVertexBuffer( U32 offset, U32 count );
        void SetIndexBuffer( U32 offset, U32 count );

        // Instance data buffer support
        bgfx::InstanceDataBuffer* AcquireInstanceBuffer( U32 count );
        void SetInstanceDataBuffer( bgfx::InstanceDataBuffer* buffer, U32 count );

        // Debug accessors
        bool IsDynamic() const { return fIsDynamic; }
        bool IsTransient() const { return fIsTransient; }
        bool HasStaticVB() const { return bgfx::isValid( fVertexBufferHandle ); }
        bool HasStaticIB() const { return bgfx::isValid( fIndexBufferHandle ); }
        bgfx::VertexBufferHandle GetStaticVBHandle() const { return fVertexBufferHandle; }
        bgfx::DynamicVertexBufferHandle GetDynamicVBHandle() const { return fDynamicVertexBufferHandle; }
        static const bgfx::VertexLayout& GetVertexLayout() { InitializeVertexLayout(); return sVertexLayout; }

        // Static geometry cache: frame counter for auto-promotion
        static void AdvanceFrame() { ++sFrameCount; }
        static U32 GetFrameCount() { return sFrameCount; }

    private:
        static void InitializeVertexLayout();

        // Vertex format-extension (vertexExtension) support. When a geometry
        // carries vertex-rate extension data, it must be uploaded with a
        // per-geometry layout that appends the extension attributes after the
        // base 68-byte vertex. Geometries without an extension take the shared
        // static layout and the base vertex data unchanged (zero overhead).
        bool GeometryHasVertexExtension( Geometry* geometry ) const;
        const bgfx::VertexLayout& ResolveLayout( Geometry* geometry );
        static void BuildExtendedLayout( const FormatExtensionList* list, bgfx::VertexLayout& outLayout );
        // Returns the upload source pointer and sets the per-vertex stride and
        // total byte size. For extension geometries this splices the base
        // vertices into the extended buffer and reports the padded stride.
        const Geometry::Vertex* PrepareVertexUpload( Geometry* geometry, U32 vertexCount,
                                                     U32& outStrideBytes, U32& outTotalBytes );

        void CreateStatic( Geometry* geometry );
        void CreateDynamic( Geometry* geometry );
        void UpdateStatic( Geometry* geometry );
        void UpdateDynamic( Geometry* geometry );
        void UpdateTransient( Geometry* geometry );
        void PromoteToStatic( Geometry* geometry );
        void DestroyStatic();
        void DestroyDynamic();

    private:
        // Vertex layout is shared across all geometries
        static bgfx::VertexLayout sVertexLayout;
        static bool sLayoutInitialized;
        static U32 sFrameCount;

        // Per-geometry extended layout, built lazily when this geometry carries
        // vertex-rate format-extension data. Unused (fHasExtLayout false) for
        // the common no-extension case.
        bgfx::VertexLayout fExtLayout;
        bool fHasExtLayout;

        // Auto-promotion threshold: frames of stability before dynamic to static
        static const U32 kPromotionThreshold = 120;

        // Handles
        bgfx::VertexBufferHandle fVertexBufferHandle;
        bgfx::DynamicVertexBufferHandle fDynamicVertexBufferHandle;
        bgfx::IndexBufferHandle fIndexBufferHandle;
        bgfx::DynamicIndexBufferHandle fDynamicIndexBufferHandle;
        
        // Instance buffer for instancing
        bgfx::InstanceDataBuffer fInstanceDataBuffer;
        bool fHasInstanceBuffer;

        // Transient vertex buffer (for pool geometries)
        bgfx::TransientVertexBuffer fTransientVB;
        bgfx::TransientIndexBuffer fTransientIB;
        bool fIsTransient;
        bool fHasTransientVB;
        bool fHasTransientIB = false;
        Geometry* fPendingGeometry;  // lazy transient: defer alloc to SetVertexBuffer

        // State
        U32 fVertexCount;
        U32 fIndexCount;
        U32 fInstancesAllocated;
        bool fIsDynamic;
        bool fHasIndexBuffer;
        bool fWasStoredOnGPU;
        U32 fLastUpdateFrame;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxGeometry_H__
