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

#include "Renderer/Rtt_BgfxFrameBufferObject.h"

#include "Renderer/Rtt_FrameBufferObject.h"
#include "Renderer/Rtt_Texture.h"
#include "Renderer/Rtt_BgfxTexture.h"
#include "Core/Rtt_Assert.h"

#include "Rtt_Profiling.h"

#include <stdlib.h>

// ----------------------------------------------------------------------------

#define ENABLE_DEBUG_PRINT	0

#if ENABLE_DEBUG_PRINT
	#define DEBUG_PRINT( ... ) Rtt_LogException( __VA_ARGS__ );
#else
	#define DEBUG_PRINT( ... )
#endif

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// Static members
bgfx::ViewId BgfxFrameBufferObject::sNextViewId = 1; // Start at 1, reserve 0 for default
bgfx::ViewId BgfxFrameBufferObject::sFreeViewIds[BgfxFrameBufferObject::kMaxFreeViewIds] = {};
unsigned int BgfxFrameBufferObject::sFreeViewIdCount = 0;
unsigned int BgfxFrameBufferObject::sLiveCount = 0;
unsigned int BgfxFrameBufferObject::sPeakLiveCount = 0;
unsigned int BgfxFrameBufferObject::sCreateCount = 0;
unsigned int BgfxFrameBufferObject::sDestroyCount = 0;
unsigned int BgfxFrameBufferObject::sCreateFailCount = 0;
unsigned int BgfxFrameBufferObject::sExhaustCount = 0;
bool BgfxFrameBufferObject::sLoggedViewIdExhausted = false;

bool
BgfxFrameBufferObject::IsDiagEnabled()
{
	const char* value = getenv( "BGFX_FBO_DIAG" );
	return value && value[0] && value[0] != '0';
}

bgfx::ViewId
BgfxFrameBufferObject::AllocateViewId()
{
	if( sFreeViewIdCount > 0 )
	{
		return sFreeViewIds[--sFreeViewIdCount];
	}

	if( sNextViewId <= kMaxViewId )
	{
		return sNextViewId++;
	}

	++sExhaustCount;
	if( ! sLoggedViewIdExhausted )
	{
		Rtt_LogException( "bgfx: Out of FBO view IDs; reusing view %u as a non-zero fallback.\n", kMaxViewId );
		sLoggedViewIdExhausted = true;
	}
	return kMaxViewId;
}

void
BgfxFrameBufferObject::ReleaseViewId( bgfx::ViewId viewId )
{
	if( viewId < kFirstViewId || viewId > kMaxViewId )
	{
		return;
	}

	if( sFreeViewIdCount < kMaxFreeViewIds )
	{
		sFreeViewIds[sFreeViewIdCount++] = viewId;
	}
}

void
BgfxFrameBufferObject::ResetViewIdAllocator()
{
	sNextViewId = kFirstViewId;
	sFreeViewIdCount = 0;
	sLiveCount = 0;
	sPeakLiveCount = 0;
	sCreateCount = 0;
	sDestroyCount = 0;
	sCreateFailCount = 0;
	sExhaustCount = 0;
	sLoggedViewIdExhausted = false;
}

BgfxFrameBufferObject::BgfxFrameBufferObject()
:	fHandle( BGFX_INVALID_HANDLE ),
	fTextureHandle( BGFX_INVALID_HANDLE ),
	fViewId( 0 )
{
}

BgfxFrameBufferObject::~BgfxFrameBufferObject()
{
	Destroy();
}

void
BgfxFrameBufferObject::Create( CPUResource* resource )
{
	Rtt_ASSERT( CPUResource::kFrameBufferObject == resource->GetType() );
	FrameBufferObject* fbo = static_cast<FrameBufferObject*>( resource );

	SUMMED_TIMING( bgfxfc, "Bgfx Framebuffer GPU Resource: Create" );

	// Get texture from FBO
	Texture* texture = fbo->GetTexture();
	Rtt_ASSERT( texture );

	// Get the GPU resource for the texture
	BgfxTexture* bgfxTexture = static_cast<BgfxTexture*>( texture->GetGPUResource() );
	Rtt_ASSERT( bgfxTexture );

	fTextureHandle = bgfxTexture->GetHandle();
	Rtt_ASSERT( bgfx::isValid( fTextureHandle ) );

	// Allocate a view ID for this FBO
	fViewId = AllocateViewId();
	++sCreateCount;
	++sLiveCount;
	if( sLiveCount > sPeakLiveCount )
	{
		sPeakLiveCount = sLiveCount;
	}

	// Create framebuffer from texture handle
	// destroyTextures = false - texture is managed by BgfxTexture
	fHandle = bgfx::createFrameBuffer( 1, &fTextureHandle, false );

	if( !bgfx::isValid( fHandle ) )
	{
		++sCreateFailCount;
		Rtt_LogException( "ERROR: BgfxFrameBufferObject create FAILED (textureHandle.idx=%u, viewId=%u). Offscreen rendering will be black.\n",
			fTextureHandle.idx, fViewId );
	}
	else
	{
		// Clear FBO to transparent immediately after creation.
		// Metal FBO textures default to alpha=1.0; without this,
		// dropshadow blur/shadow FBOs sample solid alpha and
		// render as opaque gray blocks instead of subtle shadows.
		bgfx::setViewFrameBuffer( fViewId, fHandle );
		bgfx::setViewClear( fViewId,
			BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
			0x00000000, 1.0f, 0 );
		bgfx::touch( fViewId );
	}

	DEBUG_PRINT( "%s : bgfx framebuffer handle: %d, view: %d\n",
				__FUNCTION__,
				fHandle.idx,
				fViewId );

	if( IsDiagEnabled() )
	{
		Rtt_LogException( "BGFX_FBO_DIAG create count=%u live=%u peak=%u valid=%u texture=%u view=%u next=%u failures=%u exhausts=%u\n",
			sCreateCount,
			sLiveCount,
			sPeakLiveCount,
			bgfx::isValid( fHandle ) ? 1 : 0,
			fTextureHandle.idx,
			fViewId,
			sNextViewId,
			sCreateFailCount,
			sExhaustCount );
	}
}

void
BgfxFrameBufferObject::Update( CPUResource* resource )
{
	SUMMED_TIMING( bgfxfu, "Bgfx Framebuffer GPU Resource: Update" );

	Rtt_ASSERT( CPUResource::kFrameBufferObject == resource->GetType() );
	FrameBufferObject* fbo = static_cast<FrameBufferObject*>( resource );

	// Get texture from FBO
	Texture* texture = fbo->GetTexture();
	Rtt_ASSERT( texture );

	BgfxTexture* bgfxTexture = static_cast<BgfxTexture*>( texture->GetGPUResource() );
	Rtt_ASSERT( bgfxTexture );

	bgfx::TextureHandle newTextureHandle = bgfxTexture->GetHandle();

	// If texture handle changed, recreate framebuffer
	if( newTextureHandle.idx != fTextureHandle.idx )
	{
		if( bgfx::isValid( fHandle ) )
		{
			bgfx::destroy( fHandle );
		}

		fTextureHandle = newTextureHandle;
		fHandle = bgfx::createFrameBuffer( 1, &fTextureHandle, false );

		if( !bgfx::isValid( fHandle ) )
		{
			Rtt_LogException( "ERROR: BgfxFrameBufferObject create FAILED during FBO update (textureHandle.idx=%u, viewId=%u). Offscreen rendering will be black.\n",
				fTextureHandle.idx, fViewId );
		}
	}
}

void
BgfxFrameBufferObject::Destroy()
{
	// CRITICAL: Reset the view's framebuffer binding BEFORE destroying the handle
	// to prevent stale state from interfering with scene transitions.
	// bgfx::setViewFrameBuffer is persistent across frames, so if we don't reset it,
	// the next FBO that happens to use the same view ID will inherit a binding
	// to a destroyed framebuffer handle.
	if( fViewId != 0 )
	{
		if( sLiveCount > 0 )
		{
			--sLiveCount;
		}
		++sDestroyCount;
		bgfx::setViewFrameBuffer( fViewId, BGFX_INVALID_HANDLE );
		if( IsDiagEnabled() )
		{
			Rtt_LogException( "BGFX_FBO_DIAG destroy count=%u live=%u peak=%u view=%u next=%u failures=%u exhausts=%u\n",
				sDestroyCount,
				sLiveCount,
				sPeakLiveCount,
				fViewId,
				sNextViewId,
				sCreateFailCount,
				sExhaustCount );
		}
		ReleaseViewId( fViewId );
		fViewId = 0;
	}

	if( bgfx::isValid( fHandle ) )
	{
		bgfx::destroy( fHandle );
		fHandle = BGFX_INVALID_HANDLE;
	}

	fTextureHandle = BGFX_INVALID_HANDLE;

	DEBUG_PRINT( "%s : bgfx framebuffer destroyed\n",
				__FUNCTION__ );
}

void
BgfxFrameBufferObject::Bind( bool asDrawBuffer )
{
	// In bgfx, we use setViewFrameBuffer to bind a framebuffer to a view
	// The view ID is determined by the FBO
	if( bgfx::isValid( fHandle ) )
	{
		bgfx::setViewFrameBuffer( fViewId, fHandle );
	}
}

bool
BgfxFrameBufferObject::HasFramebufferBlit( bool* canScale )
{
	// bgfx always supports blitting
	if( canScale )
	{
		*canScale = true;
	}
	return true;
}

void
BgfxFrameBufferObject::Blit( 
	bgfx::ViewId dstView,
	bgfx::TextureHandle dstTexture,
	U16 dstX,
	U16 dstY,
	bgfx::TextureHandle srcTexture,
	U16 srcX,
	U16 srcY,
	U16 width,
	U16 height )
{
	// Use bgfx::blit for framebuffer blitting
	bgfx::blit( 
		dstView,
		dstTexture,
		0,      // dstMip
		dstX,
		dstY,
		0,      // dstZ
		srcTexture,
		0,      // srcMip
		srcX,
		srcY,
		0,      // srcZ
		width,
		height,
		1       // depth
	);
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------


#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
