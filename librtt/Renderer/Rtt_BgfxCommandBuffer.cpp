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

#include "Renderer/Rtt_BgfxCommandBuffer.h"

#include "Renderer/Rtt_BgfxFrameBufferObject.h"
#include "Renderer/Rtt_BgfxGeometry.h"
#include "Renderer/Rtt_BgfxProgram.h"
#include "Renderer/Rtt_BgfxTexture.h"
#include "Renderer/Rtt_FrameBufferObject.h"
#include "Renderer/Rtt_Geometry_Renderer.h"
#include "Renderer/Rtt_Program.h"
#include "Renderer/Rtt_Texture.h"
#include "Renderer/Rtt_Uniform.h"
#include "Renderer/Rtt_FormatExtensionList.h"
#include "Display/Rtt_ShaderData.h"
#include "Display/Rtt_ShaderResource.h"
#include "Core/Rtt_Config.h"
#include "Core/Rtt_Allocator.h"
#include "Core/Rtt_Assert.h"
#include "Core/Rtt_Math.h"

#include <string.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// Static members
U32 BgfxCommandBuffer::gUniformTimestamp = 0;
bgfx::ViewId BgfxCommandBuffer::sNextViewId = 1;  // Start at 1, 0 is default screen

// Debug frame tracking globals
uint32_t gBgfxDrawFrame = 0;
uint32_t gBgfxDrawsThisFrame = 0;

// ----------------------------------------------------------------------------

BgfxCommandBuffer::BgfxCommandBuffer( Rtt_Allocator* allocator )
:   CommandBuffer( allocator ),
    fCurrentView( 0 ),
    fCurrentGeometry( NULL ),
    fCurrentProgram( NULL ),
    fCurrentVersion( Program::kMaskCount0 ),
    fBlendEnabled( true ),
    fBlendState( BGFX_STATE_BLEND_FUNC_SEPARATE(
        BGFX_STATE_BLEND_SRC_ALPHA,
        BGFX_STATE_BLEND_INV_SRC_ALPHA,
        BGFX_STATE_BLEND_ONE,
        BGFX_STATE_BLEND_INV_SRC_ALPHA ) ),
    fBlendEquation( 0 ),
    fScissorEnabled( false ),
    fScissorRect(),
    fMsaaEnabled( false ),
    fClearDepth( 1.0f ),
    fClearStencil( 0 ),
    fDefaultView( 0 ),
    fCustomCommands( allocator ),
    fInstanceCount( 0 ),
    fInstanceData( NULL )
{
    for( U32 i = 0; i < kMaxTextureUnits; ++i )
    {
        fBoundTextures[i] = NULL;
    }

    for( U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i )
    {
        fUniformUpdates[i].uniform = NULL;
        fUniformUpdates[i].timestamp = 0;
    }

    for( U32 i = 0; i < kNumQueryableParams; ++i )
    {
        fCachedQuery[i] = -1;
    }

    fDeferredCmds.reserve( 512 );
}

BgfxCommandBuffer::~BgfxCommandBuffer()
{
}

void
BgfxCommandBuffer::Initialize()
{
    // bgfx initialization is handled by BgfxRenderer
    // Here we just set up the default view
    bgfx::setViewClear( fDefaultView,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        0x00000000,
        fClearDepth,
        fClearStencil );

    InitializeFBO();
    InitializeCachedParams();

    // Query max texture size
    GetMaxTextureSize();
}

void
BgfxCommandBuffer::InitializeFBO()
{
    // Default view is 0 (screen)
    fDefaultView = 0;
    fCurrentView = fDefaultView;
}

void
BgfxCommandBuffer::InitializeCachedParams()
{
    for( int i = 0; i < kNumQueryableParams; i++ )
    {
        fCachedQuery[i] = -1;
    }
}

void
BgfxCommandBuffer::CacheQueryParam( CommandBuffer::QueryableParams param )
{
    switch( param )
    {
        case CommandBuffer::kMaxTextureSize:
        {
            const bgfx::Caps* caps = bgfx::getCaps();
            fCachedQuery[param] = caps->limits.maxTextureSize;
            break;
        }
        default:
            break;
    }
}

void
BgfxCommandBuffer::Denitialize()
{
    fDeferredCmds.clear();
}

void
BgfxCommandBuffer::ClearUserUniforms()
{
    fUniformUpdates[Uniform::kMaskMatrix0].uniform = NULL;
    fUniformUpdates[Uniform::kMaskMatrix1].uniform = NULL;
    fUniformUpdates[Uniform::kMaskMatrix2].uniform = NULL;
    fUniformUpdates[Uniform::kUserData0].uniform = NULL;
    fUniformUpdates[Uniform::kUserData1].uniform = NULL;
    fUniformUpdates[Uniform::kUserData2].uniform = NULL;
    fUniformUpdates[Uniform::kUserData3].uniform = NULL;
}

bool
BgfxCommandBuffer::HasFramebufferBlit( bool * canScale ) const
{
    if( canScale )
    {
        *canScale = true;
    }
    return true;
}

void
BgfxCommandBuffer::GetVertexAttributes( VertexAttributeSupport & support ) const
{
    support.maxCount = 16;
    support.hasInstancing = BgfxGeometry::SupportsInstancing();
    support.hasDivisors = BgfxGeometry::SupportsDivisors();
    support.hasPerInstance = support.hasDivisors;
    support.suffix = BgfxGeometry::InstanceIDSuffix();
}

// ============================================================================
// Deferred state capture methods
// These store CPU resource pointers and state - NO bgfx calls except view config
// ============================================================================

void
BgfxCommandBuffer::BindFrameBufferObject( FrameBufferObject* fbo, bool asDrawBuffer )
{
    Rtt_UNUSED( asDrawBuffer );

    // Defer FBO binding - GPU resource may not exist yet
    DeferredCmd cmd;
    cmd.type = DeferredCmd::kBindFBO;
    cmd.fbo = fbo;
    fDeferredCmds.push_back( cmd );
}

void
BgfxCommandBuffer::CaptureRect( FrameBufferObject* fbo, Texture& texture, const Rect& rect, const Rect& rawRect )
{
    // CaptureRect needs GPU resources - for now, defer is not implemented
    // This is rarely called during normal rendering
    // TODO: defer CaptureRect if needed
    Rtt_UNUSED( fbo );
    Rtt_UNUSED( texture );
    Rtt_UNUSED( rect );
    Rtt_UNUSED( rawRect );
}

void
BgfxCommandBuffer::BindGeometry( Geometry* geometry )
{
    // Store CPU resource pointer - resolve to GPU in Execute()
    fCurrentGeometry = geometry;
}

void
BgfxCommandBuffer::BindTexture( Texture* texture, U32 unit )
{
    Rtt_ASSERT( unit < kMaxTextureUnits );

    if( unit < kMaxTextureUnits )
    {
        // Debug: log bind texture
        {
            static int sDbg = 0;
            if (sDbg < 20)
            {
                BgfxTexture* gpuTex = texture ? static_cast<BgfxTexture*>(texture->GetGPUResource()) : NULL;
                Rtt_LogException("BGFX_BIND_TEX: unit=%u fmt=%d\n",
                    unit, gpuTex ? gpuTex->GetCachedFormat() : -1);
                sDbg++;
            }
        }
        // Store CPU resource pointer - resolve to GPU in Execute()
        fBoundTextures[unit] = texture;
    }
}

void
BgfxCommandBuffer::BindUniform( Uniform* uniform, U32 unit )
{
    Rtt_ASSERT( unit < Uniform::kNumBuiltInVariables );

    if( unit < Uniform::kNumBuiltInVariables )
    {
        UniformUpdate& update = fUniformUpdates[unit];
        update.uniform = uniform;
        update.timestamp = gUniformTimestamp++;
        // Don't apply to GPU program here - deferred to Execute()
    }
}

void
BgfxCommandBuffer::BindProgram( Program* program, Program::Version version )
{
    if( program )
    {
        // Store CPU resource pointer - resolve to GPU in Execute()
        fCurrentProgram = program;
        fCurrentVersion = version;

        // AcquireTimeTransform accesses CPU-side shader metadata, safe to call now
        AcquireTimeTransform( program->GetShaderResource() );
    }
    else
    {
        fCurrentProgram = NULL;
    }
}

void
BgfxCommandBuffer::BindInstancing( U32 count, Geometry::Vertex* instanceData )
{
    Rtt_LogException( "bgfx backend: instancing not yet implemented\n" );
    fInstanceCount = count;
    fInstanceData = instanceData;
}

void
BgfxCommandBuffer::BindVertexFormat( FormatExtensionList* list, U16 fullCount, U16 vertexSize, U32 offset )
{
    Rtt_UNUSED( list );
    Rtt_UNUSED( fullCount );
    Rtt_UNUSED( vertexSize );
    Rtt_UNUSED( offset );
}

void
BgfxCommandBuffer::SetBlendEnabled( bool enabled )
{
    fBlendEnabled = enabled;
}

uint64_t
BgfxCommandBuffer::ToBgfxBlendFactor( BlendMode::Param param ) const
{
    switch( param )
    {
        case BlendMode::kZero:              return BGFX_STATE_BLEND_ZERO;
        case BlendMode::kOne:               return BGFX_STATE_BLEND_ONE;
        case BlendMode::kSrcColor:          return BGFX_STATE_BLEND_SRC_COLOR;
        case BlendMode::kOneMinusSrcColor:  return BGFX_STATE_BLEND_INV_SRC_COLOR;
        case BlendMode::kDstColor:          return BGFX_STATE_BLEND_DST_COLOR;
        case BlendMode::kOneMinusDstColor:  return BGFX_STATE_BLEND_INV_DST_COLOR;
        case BlendMode::kSrcAlpha:          return BGFX_STATE_BLEND_SRC_ALPHA;
        case BlendMode::kOneMinusSrcAlpha:  return BGFX_STATE_BLEND_INV_SRC_ALPHA;
        case BlendMode::kDstAlpha:          return BGFX_STATE_BLEND_DST_ALPHA;
        case BlendMode::kOneMinusDstAlpha:  return BGFX_STATE_BLEND_INV_DST_ALPHA;
        case BlendMode::kSrcAlphaSaturate:  return BGFX_STATE_BLEND_SRC_ALPHA_SAT;
        default:
            Rtt_ASSERT_NOT_REACHED();
            return BGFX_STATE_BLEND_ONE;
    }
}

uint64_t
BgfxCommandBuffer::ToBgfxBlendState( const BlendMode& mode ) const
{
    uint64_t srcColor = ToBgfxBlendFactor( mode.fSrcColor );
    uint64_t dstColor = ToBgfxBlendFactor( mode.fDstColor );
    uint64_t srcAlpha = ToBgfxBlendFactor( mode.fSrcAlpha );
    uint64_t dstAlpha = ToBgfxBlendFactor( mode.fDstAlpha );

    return BGFX_STATE_BLEND_FUNC_SEPARATE( srcColor, dstColor, srcAlpha, dstAlpha );
}

void
BgfxCommandBuffer::SetBlendFunction( const BlendMode& mode )
{
    fBlendState = ToBgfxBlendState( mode );
}

void
BgfxCommandBuffer::SetBlendEquation( RenderTypes::BlendEquation mode )
{
    switch( mode )
    {
        case RenderTypes::kSubtractEquation:
            fBlendEquation = BGFX_STATE_BLEND_EQUATION( BGFX_STATE_BLEND_EQUATION_SUB );
            break;
        case RenderTypes::kReverseSubtractEquation:
            fBlendEquation = BGFX_STATE_BLEND_EQUATION( BGFX_STATE_BLEND_EQUATION_REVSUB );
            break;
        case RenderTypes::kAddEquation:
        default:
            fBlendEquation = BGFX_STATE_BLEND_EQUATION( BGFX_STATE_BLEND_EQUATION_ADD );
            break;
    }
}

void
BgfxCommandBuffer::SetViewport( int x, int y, int width, int height )
{
    // bgfx::reset is safe to call immediately for resolution changes
    static uint16_t sLastWidth = 0, sLastHeight = 0;
    uint16_t w = static_cast<uint16_t>( width );
    uint16_t h = static_cast<uint16_t>( height );
    if( w != sLastWidth || h != sLastHeight )
    {
        bgfx::reset( w, h, BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4 );
        sLastWidth = w;
        sLastHeight = h;
    }

    // Defer setViewRect to Execute (needs correct view ID from FBO)
    DeferredCmd cmd;
    cmd.type = DeferredCmd::kSetViewport;
    cmd.vpX = static_cast<uint16_t>( x );
    cmd.vpY = static_cast<uint16_t>( y );
    cmd.vpW = w;
    cmd.vpH = h;
    fDeferredCmds.push_back( cmd );
}

void
BgfxCommandBuffer::SetScissorEnabled( bool enabled )
{
    fScissorEnabled = enabled;
}

void
BgfxCommandBuffer::SetScissorRegion( int x, int y, int width, int height )
{
    fScissorRect.x = x;
    fScissorRect.y = y;
    fScissorRect.w = width;
    fScissorRect.h = height;
}

void
BgfxCommandBuffer::SetMultisampleEnabled( bool enabled )
{
    fMsaaEnabled = enabled;
}

void
BgfxCommandBuffer::ClearDepth( Real depth )
{
    fClearDepth = static_cast<float>( depth );
}

void
BgfxCommandBuffer::ClearStencil( U32 stencil )
{
    fClearStencil = static_cast<uint8_t>( stencil & 0xFF );
}

void
BgfxCommandBuffer::Clear( Real r, Real g, Real b, Real a )
{
    // Defer clear to Execute (needs correct view ID from FBO)
    DeferredCmd cmd;
    cmd.type = DeferredCmd::kClear;
    cmd.clearR = static_cast<float>( r );
    cmd.clearG = static_cast<float>( g );
    cmd.clearB = static_cast<float>( b );
    cmd.clearA = static_cast<float>( a );
    cmd.clearDepth = fClearDepth;
    cmd.clearStencil = fClearStencil;
    fDeferredCmds.push_back( cmd );
}

uint64_t
BgfxCommandBuffer::ToBgfxPrimitiveType( Geometry::PrimitiveType type ) const
{
    switch( type )
    {
        case Geometry::kTriangleStrip:
            return BGFX_STATE_PT_TRISTRIP;
        case Geometry::kTriangleFan:
            return 0;
        case Geometry::kTriangles:
        case Geometry::kIndexedTriangles:
            return 0;
        case Geometry::kLines:
        case Geometry::kLineLoop:
            return BGFX_STATE_PT_LINES;
        default:
            return 0;
    }
}

void
BgfxCommandBuffer::SnapshotUniforms( DeferredCmd& cmd )
{
    for( U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i )
    {
        const UniformUpdate& update = fUniformUpdates[i];
        if( update.uniform && update.uniform->GetData() )
        {
            U32 size = update.uniform->GetSizeInBytes();
            if( size > 64 ) size = 64;  // Safety clamp
            cmd.uniforms[i].valid = true;
            cmd.uniforms[i].size = size;
            memcpy( cmd.uniforms[i].data, update.uniform->GetData(), size );
        }
        else
        {
            cmd.uniforms[i].valid = false;
            cmd.uniforms[i].size = 0;
        }
    }
}

// ============================================================================
// Draw/DrawIndexed - package current state into DeferredCmd
// ============================================================================

void
BgfxCommandBuffer::Draw( U32 offset, U32 count, Geometry::PrimitiveType type )
{
    if( !fCurrentGeometry )
    {
        return;
    }
    if( !fCurrentProgram )
    {
        return;
    }

    // Build state flags
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    if( fBlendEnabled )
    {
        state |= fBlendState;
    }
    if( fBlendEquation != 0 )
    {
        state |= fBlendEquation;
    }
    if( fMsaaEnabled )
    {
        state |= BGFX_STATE_MSAA;
    }

    uint64_t primType = ToBgfxPrimitiveType( type );
    if( primType != 0 )
    {
        state |= primType;
    }

    // Package into deferred command
    DeferredCmd cmd;
    cmd.type = DeferredCmd::kDraw;
    cmd.geometry = fCurrentGeometry;
    cmd.program = fCurrentProgram;
    cmd.programVersion = fCurrentVersion;
    cmd.offset = offset;
    cmd.count = count;
    cmd.primitiveType = type;
    cmd.bgfxState = state;
    cmd.scissorEnabled = fScissorEnabled;
    cmd.scissorX = fScissorRect.x;
    cmd.scissorY = fScissorRect.y;
    cmd.scissorW = fScissorRect.w;
    cmd.scissorH = fScissorRect.h;

    for( U32 i = 0; i < kMaxTextureUnits; ++i )
    {
        cmd.textures[i] = fBoundTextures[i];
    }

    SnapshotUniforms( cmd );

    fDeferredCmds.push_back( cmd );
}

void
BgfxCommandBuffer::DrawIndexed( U32 offset, U32 count, Geometry::PrimitiveType type )
{
    if( !fCurrentGeometry )
    {
        return;
    }
    if( !fCurrentProgram )
    {
        return;
    }

    // Build state flags
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    if( fBlendEnabled )
    {
        state |= fBlendState;
    }
    if( fBlendEquation != 0 )
    {
        state |= fBlendEquation;
    }
    if( fMsaaEnabled )
    {
        state |= BGFX_STATE_MSAA;
    }

    DeferredCmd cmd;
    cmd.type = DeferredCmd::kDrawIndexed;
    cmd.geometry = fCurrentGeometry;
    cmd.program = fCurrentProgram;
    cmd.programVersion = fCurrentVersion;
    cmd.offset = offset;
    cmd.count = count;
    cmd.primitiveType = type;
    cmd.bgfxState = state;
    cmd.scissorEnabled = fScissorEnabled;
    cmd.scissorX = fScissorRect.x;
    cmd.scissorY = fScissorRect.y;
    cmd.scissorW = fScissorRect.w;
    cmd.scissorH = fScissorRect.h;

    for( U32 i = 0; i < kMaxTextureUnits; ++i )
    {
        cmd.textures[i] = fBoundTextures[i];
    }

    SnapshotUniforms( cmd );

    fDeferredCmds.push_back( cmd );
}

// ============================================================================
// Execute - replay deferred commands with GPU resources now available
// ============================================================================

void
BgfxCommandBuffer::ExecuteBindFBO( const DeferredCmd& cmd )
{
    if( cmd.fbo )
    {
        BgfxFrameBufferObject* bgfxFbo = static_cast<BgfxFrameBufferObject*>( cmd.fbo->GetGPUResource() );
        if( bgfxFbo )
        {
            fCurrentView = bgfxFbo->GetViewId();
            bgfx::setViewFrameBuffer( fCurrentView, bgfxFbo->GetHandle() );
        }
    }
    else
    {
        fCurrentView = fDefaultView;
    }
}

void
BgfxCommandBuffer::ExecuteSetViewport( const DeferredCmd& cmd )
{
    bgfx::setViewRect( fCurrentView, cmd.vpX, cmd.vpY, cmd.vpW, cmd.vpH );
}

void
BgfxCommandBuffer::ExecuteClear( const DeferredCmd& cmd )
{
    uint32_t rgba = ( static_cast<uint32_t>( cmd.clearR * 255.0f ) << 24 ) |
                    ( static_cast<uint32_t>( cmd.clearG * 255.0f ) << 16 ) |
                    ( static_cast<uint32_t>( cmd.clearB * 255.0f ) << 8 ) |
                    ( static_cast<uint32_t>( cmd.clearA * 255.0f ) );

    bgfx::setViewClear( fCurrentView,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        rgba, cmd.clearDepth, cmd.clearStencil );
    bgfx::touch( fCurrentView );
}

void
BgfxCommandBuffer::SetTexFlagsUniform( BgfxProgram* prog, const DeferredCmd& cmd )
{
    // Check if fill texture (unit 0) is alpha-only format
    float texFlags[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if( cmd.textures[0] )
    {
        BgfxTexture* tex = static_cast<BgfxTexture*>( cmd.textures[0]->GetGPUResource() );
        if( tex && tex->GetCachedFormat() == static_cast<S32>( Texture::kAlpha ) )
        {
            texFlags[0] = 1.0f;
        }
    }
    // Pass mask count so shader knows how many mask samplers to apply
    texFlags[1] = static_cast<float>( cmd.programVersion );
    bgfx::UniformHandle texFlagsHandle = prog->GetTexFlagsHandle();
    if( bgfx::isValid( texFlagsHandle ) )
    {
        bgfx::setUniform( texFlagsHandle, texFlags );
    }
}

void
BgfxCommandBuffer::ExecuteDraw( const DeferredCmd& cmd )
{
    // Resolve GPU resources (now available after Swap)
    BgfxGeometry* geo = static_cast<BgfxGeometry*>( cmd.geometry->GetGPUResource() );
    if( !geo )
    {
        return;
    }

    BgfxProgram* prog = static_cast<BgfxProgram*>( cmd.program->GetGPUResource() );
    if( !prog )
    {
        return;
    }

    // Trigger lazy shader creation
    prog->Bind( cmd.programVersion );

    bgfx::ProgramHandle programHandle = prog->GetHandle( cmd.programVersion );
    if( !bgfx::isValid( programHandle ) )
    {
        return;
    }

    // Apply uniforms from snapshot
    for( U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i )
    {
        if( cmd.uniforms[i].valid )
        {
            prog->SetUniform( static_cast<Uniform::Name>( i ), cmd.uniforms[i].data );
            // Debug: log mask matrix uniforms (kMaskMatrix0=1, kMaskMatrix1=2, kMaskMatrix2=3)
            if (i >= Uniform::kMaskMatrix0 && i <= Uniform::kMaskMatrix2)
            {
                static int sDbg = 0;
                if (sDbg < 20)
                {
                    const float* m = reinterpret_cast<const float*>( cmd.uniforms[i].data );
                    Rtt_LogException("BGFX_MASK_MAT: m=[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f]\n",
                        m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
                    sDbg++;
                }
            }
        }
    }

    // Set texture flags (alpha texture swizzle)
    SetTexFlagsUniform( prog, cmd );

    // Debug: log draw call
    {
        static int sDbg = 0;
        if (sDbg < 20)
        {
            Rtt_LogException("BGFX_DRAW: off=%u cnt=%u ver=%d\n",
                cmd.offset, cmd.count, (int)cmd.programVersion);
            sDbg++;
        }
    }

    // Handle TriangleFan conversion
    if( cmd.primitiveType == Geometry::kTriangleFan )
    {
        if( cmd.count < 3 )
        {
            return;
        }

        uint32_t triCount = cmd.count - 2;

        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientIndexBuffer( &tib, triCount * 3 );

        if( tib.data )
        {
            uint16_t* indices = reinterpret_cast<uint16_t*>( tib.data );
            for( uint32_t i = 0; i < triCount; i++ )
            {
                indices[i * 3 + 0] = static_cast<uint16_t>( cmd.offset );
                indices[i * 3 + 1] = static_cast<uint16_t>( cmd.offset + i + 1 );
                indices[i * 3 + 2] = static_cast<uint16_t>( cmd.offset + i + 2 );
            }

            bgfx::setState( cmd.bgfxState );

            if( cmd.scissorEnabled )
            {
                bgfx::setScissor( cmd.scissorX, cmd.scissorY, cmd.scissorW, cmd.scissorH );
            }

            geo->SetVertexBuffer( cmd.offset, cmd.count );
            bgfx::setIndexBuffer( &tib );

            // Set textures
            for( U32 i = 0; i < kMaxTextureUnits; i++ )
            {
                if( cmd.textures[i] )
                {
                    BgfxTexture* tex = static_cast<BgfxTexture*>( cmd.textures[i]->GetGPUResource() );
                    if( tex )
                    {
                        bgfx::TextureHandle texHandle = tex->GetHandle();
                        if( bgfx::isValid( texHandle ) )
                        {
                            bgfx::UniformHandle sampler = prog->GetSamplerHandle( i );
                            if( bgfx::isValid( sampler ) )
                            {
                                bgfx::setTexture( i, sampler, texHandle );
                            }
                        }
                    }
                }
            }

            bgfx::submit( fCurrentView, programHandle );
        }
    }
    else
    {
        bgfx::setState( cmd.bgfxState );

        if( cmd.scissorEnabled )
        {
            bgfx::setScissor( cmd.scissorX, cmd.scissorY, cmd.scissorW, cmd.scissorH );
        }

        geo->SetVertexBuffer( cmd.offset, cmd.count );

        // Set textures
        for( U32 i = 0; i < kMaxTextureUnits; i++ )
        {
            if( cmd.textures[i] )
            {
                BgfxTexture* tex = static_cast<BgfxTexture*>( cmd.textures[i]->GetGPUResource() );
                if( tex )
                {
                    bgfx::TextureHandle texHandle = tex->GetHandle();
                    if( bgfx::isValid( texHandle ) )
                    {
                        bgfx::UniformHandle sampler = prog->GetSamplerHandle( i );
                        if( bgfx::isValid( sampler ) )
                        {
                            bgfx::setTexture( i, sampler, texHandle );
                        }
                    }
                }
            }
        }

        bgfx::submit( fCurrentView, programHandle );
    }
}

void
BgfxCommandBuffer::ExecuteDrawIndexed( const DeferredCmd& cmd )
{
    BgfxGeometry* geo = static_cast<BgfxGeometry*>( cmd.geometry->GetGPUResource() );
    if( !geo )
    {
        return;
    }

    BgfxProgram* prog = static_cast<BgfxProgram*>( cmd.program->GetGPUResource() );
    if( !prog )
    {
        return;
    }

    prog->Bind( cmd.programVersion );

    bgfx::ProgramHandle programHandle = prog->GetHandle( cmd.programVersion );
    if( !bgfx::isValid( programHandle ) )
    {
        return;
    }

    // Apply uniforms from snapshot
    for( U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i )
    {
        if( cmd.uniforms[i].valid )
        {
            prog->SetUniform( static_cast<Uniform::Name>( i ), cmd.uniforms[i].data );
        }
    }

    // Set texture flags (alpha texture swizzle)
    SetTexFlagsUniform( prog, cmd );

    bgfx::setState( cmd.bgfxState );

    if( cmd.scissorEnabled )
    {
        bgfx::setScissor( cmd.scissorX, cmd.scissorY, cmd.scissorW, cmd.scissorH );
    }

    geo->SetVertexBuffer( 0, 0 );  // Full vertex buffer
    geo->SetIndexBuffer( cmd.offset, cmd.count );

    // Set textures
    for( U32 i = 0; i < kMaxTextureUnits; i++ )
    {
        if( cmd.textures[i] )
        {
            BgfxTexture* tex = static_cast<BgfxTexture*>( cmd.textures[i]->GetGPUResource() );
            if( tex )
            {
                bgfx::TextureHandle texHandle = tex->GetHandle();
                if( bgfx::isValid( texHandle ) )
                {
                    bgfx::UniformHandle sampler = prog->GetSamplerHandle( i );
                    if( bgfx::isValid( sampler ) )
                    {
                        bgfx::setTexture( i, sampler, texHandle );
                    }
                }
            }
        }
    }

    bgfx::submit( fCurrentView, programHandle );
}

Real
BgfxCommandBuffer::Execute( bool measureGPU )
{
    Rtt_UNUSED( measureGPU );

    // Reset view to default before replaying commands
    fCurrentView = fDefaultView;

    static uint32_t sFrameNum = 0;

    // Replay all deferred commands - GPU resources are now available (Swap has run)
    for( size_t i = 0; i < fDeferredCmds.size(); ++i )
    {
        const DeferredCmd& cmd = fDeferredCmds[i];
        switch( cmd.type )
        {
            case DeferredCmd::kBindFBO:
                ExecuteBindFBO( cmd );
                break;
            case DeferredCmd::kSetViewport:
                ExecuteSetViewport( cmd );
                break;
            case DeferredCmd::kClear:
                ExecuteClear( cmd );
                break;
            case DeferredCmd::kDraw:
                ExecuteDraw( cmd );
                break;
            case DeferredCmd::kDrawIndexed:
                ExecuteDrawIndexed( cmd );
                break;
        }
    }

    // Per-frame tracking
    {
        sFrameNum++;
        extern uint32_t gBgfxDrawFrame;
        extern uint32_t gBgfxDrawsThisFrame;
        gBgfxDrawFrame = sFrameNum;
        gBgfxDrawsThisFrame = 0;
    }

    // Ensure view 0 is submitted even if no draw commands targeted it
    bgfx::touch(0);

    // Submit frame to bgfx
    bgfx::frame();

    // Clear deferred commands for next frame
    fDeferredCmds.clear();

    // Reset instance data for next frame
    fInstanceCount = 0;
    fInstanceData = NULL;

    // Increment timestamp for uniform tracking
    gUniformTimestamp++;

    return 0.0f;
}

S32
BgfxCommandBuffer::GetCachedParam( CommandBuffer::QueryableParams param )
{
    S32 result = -1;
    if( param < kNumQueryableParams )
    {
        result = fCachedQuery[param];
    }

    Rtt_ASSERT_MSG( result != -1, "Parameter not cached" );

    return result;
}

void
BgfxCommandBuffer::AddCommand( const CoronaCommand* command )
{
    fCustomCommands.Append( command );
}

void
BgfxCommandBuffer::IssueCommand( U16 id, const void * data, U32 size )
{
    Rtt_UNUSED( id );
    Rtt_UNUSED( data );
    Rtt_UNUSED( size );
}

bool
BgfxCommandBuffer::WriteNamedUniform( const char * uniformName, const void * data, unsigned int size )
{
    Rtt_UNUSED( uniformName );
    Rtt_UNUSED( data );
    Rtt_UNUSED( size );
    return false;
}

bgfx::ViewId
BgfxCommandBuffer::AllocateViewId()
{
    if( sNextViewId < 255 )
    {
        return sNextViewId++;
    }

    Rtt_LogException( "bgfx: Out of view IDs!\n" );
    return 0;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
