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
#include "Display/Rtt_InstancedBatchRenderer.h"
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
    fInstanceData( NULL ),
    fPendingInstanceDraw( NULL ),
    fScreenCaptureTexture( BGFX_INVALID_HANDLE )
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
    // Screen uses high view ID so FBO views (1, 2, 3, ...) render first
    // bgfx renders views in ascending ID order by default
    fDefaultView = 200;
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
    fPendingNamedUniforms.clear();
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
    DeferredCmd cmd;
    cmd.type = DeferredCmd::kCaptureRect;
    cmd.captureFbo = fbo;
    cmd.captureTexture = &texture;
    cmd.captureRectXMin = rect.xMin;
    cmd.captureRectYMin = rect.yMin;
    cmd.captureRectXMax = rect.xMax;
    cmd.captureRectYMax = rect.yMax;
    cmd.captureRawXMin = rawRect.xMin;
    cmd.captureRawYMin = rawRect.yMin;
    cmd.captureRawXMax = rawRect.xMax;
    cmd.captureRawYMax = rawRect.yMax;
    cmd.captureTexW = texture.GetWidth();
    cmd.captureTexH = texture.GetHeight();
    fDeferredCmds.push_back( cmd );
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
BgfxCommandBuffer::SetPendingInstanceDraw( void* data )
{
    fPendingInstanceDraw = data;
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

    // Snapshot pending named uniforms
    int count = (int)fPendingNamedUniforms.size();
    if( count > DeferredCmd::kMaxNamedUniforms )
    {
        count = DeferredCmd::kMaxNamedUniforms;
    }
    cmd.namedUniformCount = count;
    for( int i = 0; i < count; ++i )
    {
        strncpy( cmd.namedUniforms[i].name, fPendingNamedUniforms[i].name, 63 );
        cmd.namedUniforms[i].name[63] = '\0';
        memcpy( cmd.namedUniforms[i].data, fPendingNamedUniforms[i].data, fPendingNamedUniforms[i].size );
        cmd.namedUniforms[i].size = fPendingNamedUniforms[i].size;
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

    // Capture pending instance draw data
    cmd.instanceDraw = fPendingInstanceDraw;
    fPendingInstanceDraw = NULL; // consume it

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
        void* gpuRes = cmd.fbo->GetGPUResource();
        BgfxFrameBufferObject* bgfxFbo = static_cast<BgfxFrameBufferObject*>( gpuRes );
        if( bgfxFbo )
        {
            fCurrentView = bgfxFbo->GetViewId();
            bgfx::setViewFrameBuffer( fCurrentView, bgfxFbo->GetHandle() );
            bgfx::setViewMode( fCurrentView, bgfx::ViewMode::Sequential );
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
BgfxCommandBuffer::ApplyNamedUniforms( const DeferredCmd& cmd )
{
    for( int i = 0; i < cmd.namedUniformCount; ++i )
    {
        const DeferredCmd::NamedUniformSnapshot& nu = cmd.namedUniforms[i];
        unsigned int numFloats = nu.size / sizeof( float );

        bgfx::UniformType::Enum utype;
        U16 numElements = 1;

        if( numFloats == 9 )
        {
            // Mat3: 9 floats compact format
            utype = bgfx::UniformType::Mat3;
        }
        else if( numFloats == 16 )
        {
            // Mat4: 16 floats
            utype = bgfx::UniformType::Mat4;
        }
        else
        {
            // Vec4: 1-4 floats per element, or arrays of vec4
            utype = bgfx::UniformType::Vec4;
            numElements = ( numFloats + 3 ) / 4;
            if( numElements == 0 ) numElements = 1;
        }

        bgfx::UniformHandle handle = bgfx::createUniform( nu.name, utype, numElements );
        if( bgfx::isValid( handle ) )
        {
            if( utype == bgfx::UniformType::Vec4 && numFloats < 4 )
            {
                // Pad sub-vec4 data to full vec4
                float packed[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                memcpy( packed, nu.data, nu.size );
                bgfx::setUniform( handle, packed, numElements );
            }
            else
            {
                bgfx::setUniform( handle, nu.data, numElements );
            }
            bgfx::destroy( handle ); // Release ref from createUniform
        }
    }
}

void
BgfxCommandBuffer::ExecuteDraw( const DeferredCmd& cmd )
{
    // GPU instancing path (BatchObject)
    if ( cmd.instanceDraw )
    {
        const InstanceDrawData* idd = static_cast<const InstanceDrawData*>( cmd.instanceDraw );
        if ( idd->instanceCount == 0 ) return;
        if ( !bgfx::isValid( idd->programHandle ) ) return;
        if ( !bgfx::isValid( idd->baseQuadVB ) ) return;
        if ( !bgfx::isValid( idd->baseQuadIB ) ) return;

        // Apply uniforms (viewProjection matrix)
        BgfxProgram* prog = cmd.program ? static_cast<BgfxProgram*>( cmd.program->GetGPUResource() ) : NULL;
        if ( prog )
        {
            prog->Bind( cmd.programVersion );
            for ( U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i )
            {
                if ( cmd.uniforms[i].valid )
                {
                    prog->SetUniform( static_cast<Uniform::Name>( i ), cmd.uniforms[i].data );
                }
            }
        }

        bgfx::setState( cmd.bgfxState );

        if ( cmd.scissorEnabled )
        {
            bgfx::setScissor( cmd.scissorX, cmd.scissorY, cmd.scissorW, cmd.scissorH );
        }

        // Set base quad geometry
        bgfx::setVertexBuffer( 0, idd->baseQuadVB );
        bgfx::setIndexBuffer( idd->baseQuadIB );

        // Set instance data
        bgfx::setInstanceDataBuffer( &idd->instanceBuffer );

        // Set textures
        for ( U32 i = 0; i < kMaxTextureUnits; i++ )
        {
            if ( cmd.textures[i] )
            {
                BgfxTexture* tex = static_cast<BgfxTexture*>( cmd.textures[i]->GetGPUResource() );
                if ( tex )
                {
                    bgfx::TextureHandle texHandle = tex->GetHandle();
                    if ( bgfx::isValid( texHandle ) )
                    {
                        if ( prog )
                        {
                            bgfx::UniformHandle sampler = prog->GetSamplerHandle( i );
                            if ( bgfx::isValid( sampler ) )
                            {
                                bgfx::setTexture( i, sampler, texHandle );
                            }
                        }
                    }
                }
            }
        }

        bgfx::submit( fCurrentView, idd->programHandle );
        return;
    }

    // Resolve GPU resources (now available after Swap)
    BgfxGeometry* geo = cmd.geometry ? static_cast<BgfxGeometry*>( cmd.geometry->GetGPUResource() ) : NULL;
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
        }
    }

    // Set texture flags (alpha texture swizzle)
    SetTexFlagsUniform( prog, cmd );

    // Apply named uniforms (custom effects)
    ApplyNamedUniforms( cmd );

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

    // Apply named uniforms (custom effects)
    ApplyNamedUniforms( cmd );

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

void
BgfxCommandBuffer::ExecuteCaptureRect( const DeferredCmd& cmd )
{
    // Get the destination texture from the capture FBO
    BgfxFrameBufferObject* dstFbo = NULL;
    bgfx::TextureHandle dstTexHandle = BGFX_INVALID_HANDLE;
    bgfx::ViewId blitView = fCurrentView;

    if( cmd.captureFbo )
    {
        void* gpuRes = cmd.captureFbo->GetGPUResource();
        dstFbo = static_cast<BgfxFrameBufferObject*>( gpuRes );
        if( dstFbo )
        {
            dstTexHandle = dstFbo->GetTextureHandle();
            blitView = dstFbo->GetViewId();
        }
    }
    else if( cmd.captureTexture )
    {
        void* gpuRes = cmd.captureTexture->GetGPUResource();
        BgfxTexture* bgfxTex = static_cast<BgfxTexture*>( gpuRes );
        if( bgfxTex )
        {
            dstTexHandle = bgfxTex->GetHandle();
        }
    }

    if( !bgfx::isValid( dstTexHandle ) )
    {
        return;
    }

    // Calculate destination coordinates (same logic as GL CaptureRect)
    U32 w = static_cast<U32>( cmd.captureRectXMax - cmd.captureRectXMin );
    U32 h = static_cast<U32>( cmd.captureRectYMax - cmd.captureRectYMin );
    U32 dstX = 0, dstY = 0;

    if( cmd.captureRawXMin < 0 )
    {
        dstX = static_cast<U32>( -cmd.captureRawXMin );
    }

    if( cmd.captureRawYMax > cmd.captureRectYMax )
    {
        dstY = static_cast<U32>( cmd.captureRawYMax - cmd.captureRectYMax );
    }

    // Scale if texture size differs significantly from unclipped rect (non-FBO path)
    if( !cmd.captureFbo )
    {
        float rawW = cmd.captureRawXMax - cmd.captureRawXMin;
        float rawH = cmd.captureRawYMax - cmd.captureRawYMin;
        S32 w1 = cmd.captureTexW, w2 = static_cast<S32>( rawW );
        S32 h1 = cmd.captureTexH, h2 = static_cast<S32>( rawH );

        if( abs( w1 - w2 ) > 5 || abs( h1 - h2 ) > 5 )
        {
            dstX = dstX * w1 / w2;
            dstY = dstY * h1 / h2;
            w = w * w1 / w2;
            h = h * w1 / w2;
        }
    }

    // Source: the screen capture texture (set by BgfxRenderer when available).
    // In bgfx, the backbuffer is not accessible as a texture handle.
    // When fScreenCaptureTexture is valid, we blit from it.
    // Otherwise, this is a no-op (capture not available from backbuffer).
    if( bgfx::isValid( fScreenCaptureTexture ) )
    {
        bgfx::blit(
            blitView,
            dstTexHandle,
            static_cast<uint16_t>( dstX ),
            static_cast<uint16_t>( dstY ),
            fScreenCaptureTexture,
            static_cast<uint16_t>( cmd.captureRectXMin ),
            static_cast<uint16_t>( cmd.captureRectYMin ),
            static_cast<uint16_t>( w ),
            static_cast<uint16_t>( h )
        );
        bgfx::touch( blitView );
    }
}

Real
BgfxCommandBuffer::Execute( bool measureGPU )
{
    Rtt_UNUSED( measureGPU );

    // Reset view to default before replaying commands
    fCurrentView = fDefaultView;
    
    // CRITICAL: Reset ALL views' framebuffer bindings every frame.
    // bgfx::setViewFrameBuffer is persistent across frames. Stale bindings
    // from previous scenes cause rendering failures after scene transitions.
    for( bgfx::ViewId v = 0; v <= fDefaultView; ++v )
    {
        bgfx::setViewFrameBuffer( v, BGFX_INVALID_HANDLE );
    }

    // FBO views use IDs 1-199, screen view uses ID 200
    // bgfx renders views in ascending ID order, so FBOs render before screen
    // No setViewOrder needed - natural ordering handles this

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
            case DeferredCmd::kCaptureRect:
                ExecuteCaptureRect( cmd );
                break;
        }
    }

    sFrameNum++;

    // Ensure screen view is submitted even if no draw commands targeted it
    bgfx::touch(fDefaultView);

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
    if( !uniformName || !data || size == 0 )
    {
        return false;
    }

    U32 nameLength = (U32)strlen( uniformName );
    if( nameLength >= 64 )
    {
        Rtt_Log( "ERROR: Uniform name '%s' is %u characters, max is 63\n", uniformName, nameLength );
        return false;
    }

    if( size > 64 )
    {
        Rtt_Log( "ERROR: Uniform data size %u exceeds max 64 bytes\n", size );
        return false;
    }

    // Store as pending named uniform; will be snapshotted into DeferredCmd at next Draw
    PendingNamedUniform pending;
    strncpy( pending.name, uniformName, 63 );
    pending.name[63] = '\0';
    memcpy( pending.data, data, size );
    pending.size = size;

    // Update existing or append
    for( size_t i = 0; i < fPendingNamedUniforms.size(); ++i )
    {
        if( strcmp( fPendingNamedUniforms[i].name, uniformName ) == 0 )
        {
            fPendingNamedUniforms[i] = pending;
            return true;
        }
    }

    fPendingNamedUniforms.push_back( pending );
    return true;
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
