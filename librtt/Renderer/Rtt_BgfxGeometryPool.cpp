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

#include "Renderer/Rtt_BgfxGeometryPool.h"
#include "Renderer/Rtt_BgfxGeometry.h"
#include "Core/Rtt_Allocator.h"

#include <string.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

BgfxGeometryPool::BgfxGeometryPool( Rtt_Allocator* allocator )
:   fAllocator( allocator ),
    fFreeList( NULL ),
    fFreeCount( 0 ),
    fFreeCapacity( 0 )
{
}

BgfxGeometryPool::~BgfxGeometryPool()
{
    Shutdown();
}

BgfxGeometry*
BgfxGeometryPool::Acquire()
{
    if( fFreeCount > 0 )
    {
        return fFreeList[--fFreeCount];
    }

    return Rtt_NEW( fAllocator, BgfxGeometry() );
}

void
BgfxGeometryPool::Recycle( BgfxGeometry* geo )
{
    if( !geo )
    {
        return;
    }

    if( fFreeCount < kMaxPoolSize )
    {
        // Grow the free list array if needed
        if( fFreeCount >= fFreeCapacity )
        {
            U32 newCapacity = fFreeCapacity > 0 ? fFreeCapacity * 2 : 8;
            if( newCapacity > kMaxPoolSize )
            {
                newCapacity = kMaxPoolSize;
            }

            BgfxGeometry** newList = (BgfxGeometry**)Rtt_MALLOC(
                fAllocator, sizeof(BgfxGeometry*) * newCapacity );

            if( fFreeList )
            {
                memcpy( newList, fFreeList, sizeof(BgfxGeometry*) * fFreeCount );
                Rtt_FREE( fFreeList );
            }

            fFreeList = newList;
            fFreeCapacity = newCapacity;
        }

        geo->ResetForReuse();
        fFreeList[fFreeCount++] = geo;
    }
    else
    {
        Rtt_DELETE( geo );
    }
}

void
BgfxGeometryPool::Shutdown()
{
    for( U32 i = 0; i < fFreeCount; ++i )
    {
        Rtt_DELETE( fFreeList[i] );
    }

    if( fFreeList )
    {
        Rtt_FREE( fFreeList );
    }

    fFreeList = NULL;
    fFreeCount = 0;
    fFreeCapacity = 0;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
