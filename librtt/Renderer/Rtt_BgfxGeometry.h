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

// ----------------------------------------------------------------------------

class BgfxGeometry : public GPUResource
{
    public:
        typedef GPUResource Super;
        typedef BgfxGeometry Self;

    public:
        BgfxGeometry();
        virtual ~BgfxGeometry();

        static bool SupportsInstancing();
        static bool SupportsDivisors();
        static const char* InstanceIDSuffix();

        bool StoredOnGPU() const { return bgfx::isValid( fVertexBufferHandle ); }

        virtual void Create( CPUResource* resource );
        virtual void Update( CPUResource* resource );
        virtual void Destroy();

        void Bind();
        void SetVertexBuffer( U32 offset, U32 count );
        void SetIndexBuffer( U32 offset, U32 count );

        // Instance data buffer support
        bgfx::InstanceDataBuffer* AcquireInstanceBuffer( U32 count );
        void SetInstanceDataBuffer( bgfx::InstanceDataBuffer* buffer, U32 count );

        // Debug accessors
        bool IsDynamic() const { return fIsDynamic; }
        bool IsTransient() const { return fIsTransient; }
        bgfx::VertexBufferHandle GetStaticVBHandle() const { return fVertexBufferHandle; }
        bgfx::DynamicVertexBufferHandle GetDynamicVBHandle() const { return fDynamicVertexBufferHandle; }
        static const bgfx::VertexLayout& GetVertexLayout() { InitializeVertexLayout(); return sVertexLayout; }

    private:
        static void InitializeVertexLayout();
        void CreateStatic( Geometry* geometry );
        void CreateDynamic( Geometry* geometry );
        void UpdateStatic( Geometry* geometry );
        void UpdateDynamic( Geometry* geometry );
        void UpdateTransient( Geometry* geometry );
        void DestroyStatic();
        void DestroyDynamic();

    private:
        // Vertex layout is shared across all geometries
        static bgfx::VertexLayout sVertexLayout;
        static bool sLayoutInitialized;

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
        bool fIsTransient;
        bool fHasTransientVB;

        // State
        U32 fVertexCount;
        U32 fIndexCount;
        U32 fInstancesAllocated;
        bool fIsDynamic;
        bool fHasIndexBuffer;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxGeometry_H__
