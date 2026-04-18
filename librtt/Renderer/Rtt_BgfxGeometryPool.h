//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxGeometryPool_H__
#define _Rtt_BgfxGeometryPool_H__

#include "Core/Rtt_Types.h"

// ----------------------------------------------------------------------------

struct Rtt_Allocator;

namespace Rtt
{

// ----------------------------------------------------------------------------

class BgfxGeometry;

// ----------------------------------------------------------------------------

/// Object pool for BgfxGeometry GPU resources.
///
/// Eliminates new/delete overhead in high-throughput create/destroy cycles.
/// All pooled objects are "empty shells" — their bgfx handles have been
/// destroyed before recycling. Memory overhead is negligible.
///
/// Thread safety: All operations occur on the API thread inside
/// Renderer::Swap(), so no locking is required.
class BgfxGeometryPool
{
public:
    explicit BgfxGeometryPool( Rtt_Allocator* allocator );
    ~BgfxGeometryPool();

    /// Acquire a BgfxGeometry instance from the pool.
    /// If the pool is empty, allocates a new instance.
    BgfxGeometry* Acquire();

    /// Recycle a BgfxGeometry instance back into the pool.
    /// The caller MUST have already called geo->Destroy() before this.
    /// ResetForReuse() is called internally. If the pool is full,
    /// the instance is deleted.
    void Recycle( BgfxGeometry* geo );

    /// Drain the pool, deleting all cached instances.
    void Shutdown();

    /// Maximum number of cached instances.
    static const U32 kMaxPoolSize = 64;

private:
    Rtt_Allocator* fAllocator;
    BgfxGeometry** fFreeList;
    U32 fFreeCount;
    U32 fFreeCapacity;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxGeometryPool_H__
