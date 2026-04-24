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

bgfx::ViewId
BgfxFrameBufferObject::AllocateViewId()
{
	// Allocate next available view ID for FBO rendering.
	//
	// FBO views are confined to 1..199 so they never collide with the "screen"
	// view range 200..254 owned by BgfxRenderer (Issue #027): primary Runtime
	// uses view 200, secondary Runtimes get 201, 202, ... via BgfxContext.
	// Without this cap, sNextViewId would roll through 200+ and an FBO could
	// write into the primary's swap chain — e.g., Welcome window would show
	// the Game's FBO content.
	//
	// RISK (pre-existing): sNextViewId never resets on resume. After
	// ReleaseGPUResources() + FBO recreation, old view IDs are leaked.
	// Wraparound prevents crash, but after many cycles, IDs may collide
	// with deferred command state. Fix if needed: reset in ReleaseGPUResources().
	bgfx::ViewId id = sNextViewId;
	if( sNextViewId < 199 )
	{
		sNextViewId++;
	}
	else
	{
		sNextViewId = 1;
	}
	return id;
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

	// Create framebuffer from texture handle
	// destroyTextures = false - texture is managed by BgfxTexture
	fHandle = bgfx::createFrameBuffer( 1, &fTextureHandle, false );

	DEBUG_PRINT( "%s : bgfx framebuffer handle: %d, view: %d\n",
				__FUNCTION__,
				fHandle.idx,
				fViewId );
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
		bgfx::setViewFrameBuffer( fViewId, BGFX_INVALID_HANDLE );
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
