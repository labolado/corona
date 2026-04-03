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
    // Nothing to clean up - bgfx handles its own cleanup
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
    // bgfx always supports blit
    if( canScale )
    {
        *canScale = true;
    }
    return true;
}

void
BgfxCommandBuffer::GetVertexAttributes( VertexAttributeSupport & support ) const
{
    support.maxCount = 16;  // Arbitrary reasonable limit
    support.hasInstancing = BgfxGeometry::SupportsInstancing();
    support.hasDivisors = BgfxGeometry::SupportsDivisors();
    support.hasPerInstance = support.hasDivisors;
    support.suffix = BgfxGeometry::InstanceIDSuffix();
}

void
BgfxCommandBuffer::BindFrameBufferObject( FrameBufferObject* fbo, bool asDrawBuffer )
{
    Rtt_UNUSED( asDrawBuffer );

    if( fbo )
    {
        BgfxFrameBufferObject* bgfxFbo = static_cast<BgfxFrameBufferObject*>( fbo->GetGPUResource() );
        if( bgfxFbo )
        {
            fCurrentView = bgfxFbo->GetViewId();
            bgfx::setViewFrameBuffer( fCurrentView, bgfxFbo->GetHandle() );
        }
    }
    else
    {
        // Unbind - return to default view
        fCurrentView = fDefaultView;
    }
}

void
BgfxCommandBuffer::CaptureRect( FrameBufferObject* fbo, Texture& texture, const Rect& rect, const Rect& rawRect )
{
    BgfxTexture* bgfxTexture = static_cast<BgfxTexture*>( texture.GetGPUResource() );
    if( !bgfxTexture )
    {
        return;
    }

    // Calculate dimensions
    U32 x = 0, w = static_cast<U32>( rect.xMax - rect.xMin );
    U32 y = 0, h = static_cast<U32>( rect.yMax - rect.yMin );

    if( rawRect.xMin < 0 )
    {
        x = static_cast<U32>( -rawRect.xMin );
    }

    if( rawRect.yMax > rect.yMax )
    {
        y = static_cast<U32>( rawRect.yMax - rect.yMax );
    }

    // Adjust for size differences
    S32 w1 = texture.GetWidth();
    S32 w2 = static_cast<S32>( rawRect.xMax - rawRect.xMin );
    S32 h1 = texture.GetHeight();
    S32 h2 = static_cast<S32>( rawRect.yMax - rawRect.yMin );

    if( ( abs( w1 - w2 ) > 5 || abs( h1 - h2 ) > 5 ) )
    {
        x = x * w1 / w2;
        y = y * h1 / h2;
        w = w * w1 / w2;
        h = h * h1 / h2;
    }

    // Use bgfx blit
    bgfx::ViewId view = fCurrentView;
    bgfx::TextureHandle dstTex = bgfxTexture->GetHandle();
    
    // Source is the current FBO's texture or backbuffer
    bgfx::TextureHandle srcTex = BGFX_INVALID_HANDLE;
    if( fbo )
    {
        BgfxFrameBufferObject* bgfxFbo = static_cast<BgfxFrameBufferObject*>( fbo->GetGPUResource() );
        if( bgfxFbo )
        {
            srcTex = bgfxFbo->GetTextureHandle();
        }
    }

    // Note: If srcTex is invalid, blit from backbuffer (view)
    if( bgfx::isValid( dstTex ) )
    {
        bgfx::blit( view, 
            dstTex, 
            static_cast<uint16_t>( x ), 
            static_cast<uint16_t>( y ),
            srcTex,
            static_cast<uint16_t>( rect.xMin ), 
            static_cast<uint16_t>( rect.yMin ),
            static_cast<uint16_t>( w ), 
            static_cast<uint16_t>( h ) );
    }
}

void
BgfxCommandBuffer::BindGeometry( Geometry* geometry )
{
    if( geometry )
    {
        fCurrentGeometry = static_cast<BgfxGeometry*>( geometry->GetGPUResource() );
    }
    else
    {
        fCurrentGeometry = NULL;
    }
}

void
BgfxCommandBuffer::BindTexture( Texture* texture, U32 unit )
{
    Rtt_ASSERT( unit < kMaxTextureUnits );
    
    if( unit < kMaxTextureUnits )
    {
        if( texture )
        {
            fBoundTextures[unit] = static_cast<BgfxTexture*>( texture->GetGPUResource() );
        }
        else
        {
            fBoundTextures[unit] = NULL;
        }
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

        // Apply uniform immediately for bgfx
        if( fCurrentProgram && uniform )
        {
            fCurrentProgram->SetUniform( static_cast<Uniform::Name>( unit ), uniform->GetData() );
        }
    }
}

void
BgfxCommandBuffer::BindProgram( Program* program, Program::Version version )
{
    if( program )
    {
        fCurrentProgram = static_cast<BgfxProgram*>( program->GetGPUResource() );
        fCurrentVersion = version;

        // Trigger lazy shader creation
        if( fCurrentProgram )
        {
            fCurrentProgram->Bind( version );
        }

        // Apply all pending uniforms
        ApplyUniforms();

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
    
    // Store for later use in Draw
    fInstanceCount = count;
    fInstanceData = instanceData;
}

void
BgfxCommandBuffer::BindVertexFormat( FormatExtensionList* list, U16 fullCount, U16 vertexSize, U32 offset )
{
    // bgfx vertex layout is fixed, handled by BgfxGeometry
    // This method is for custom vertex attributes which we'll handle
    // when we implement custom shader attributes
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
    // Update bgfx resolution if the viewport size changed (handles Retina scale factor timing)
    static uint16_t sLastWidth = 0, sLastHeight = 0;
    uint16_t w = static_cast<uint16_t>( width );
    uint16_t h = static_cast<uint16_t>( height );
    if( w != sLastWidth || h != sLastHeight )
    {
        bgfx::reset( w, h, BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4 );
        sLastWidth = w;
        sLastHeight = h;
    }
    bgfx::setViewRect( fCurrentView, static_cast<uint16_t>( x ), static_cast<uint16_t>( y ), w, h );
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
    // Pack into uint32_t RGBA format (bgfx expects RGBA, not ABGR)
    uint32_t rgba = ( static_cast<uint32_t>( r * 255.0f ) << 24 ) |
                    ( static_cast<uint32_t>( g * 255.0f ) << 16 ) |
                    ( static_cast<uint32_t>( b * 255.0f ) << 8 ) |
                    ( static_cast<uint32_t>( a * 255.0f ) );

    bgfx::setViewClear( fCurrentView,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        rgba, fClearDepth, fClearStencil );
}

uint64_t
BgfxCommandBuffer::ToBgfxPrimitiveType( Geometry::PrimitiveType type ) const
{
    switch( type )
    {
        case Geometry::kTriangleStrip:
            return BGFX_STATE_PT_TRISTRIP;
        case Geometry::kTriangleFan:
            // bgfx doesn't support triangle fan - will be converted to indexed triangles
            return 0;
        case Geometry::kTriangles:
        case Geometry::kIndexedTriangles:
            return 0;  // Default is triangles
        case Geometry::kLines:
        case Geometry::kLineLoop:
            return BGFX_STATE_PT_LINES;
        default:
            return 0;
    }
}

void
BgfxCommandBuffer::Draw( U32 offset, U32 count, Geometry::PrimitiveType type )
{
    if( !fCurrentGeometry )
    {
        Rtt_LogException( "bgfx: Draw called without bound geometry\n" );
        return;
    }

    if( !fCurrentProgram )
    {
        Rtt_LogException( "bgfx: Draw called without bound program\n" );
        return;
    }

    bgfx::ProgramHandle program = fCurrentProgram->GetHandle( fCurrentVersion );
    if( !bgfx::isValid( program ) )
    {
        Rtt_LogException( "bgfx: Draw called with invalid program\n" );
        return;
    }

    // Apply any pending uniforms
    ApplyUniforms();

    {
        // Log first draw of first 5 frames
        if (gBgfxDrawFrame < 5 && gBgfxDrawsThisFrame == 0)
        {
            bool hasVP = (fUniformUpdates[Uniform::kViewProjectionMatrix].uniform != NULL);
            bool vbValid = fCurrentGeometry ?
                (fCurrentGeometry->IsDynamic() ?
                    bgfx::isValid(fCurrentGeometry->GetDynamicVBHandle()) :
                    bgfx::isValid(fCurrentGeometry->GetStaticVBHandle())) : false;

            fprintf(stderr, "BGFX_DRAW frame=%u: view=%d prog=%d type=%d off=%u cnt=%u hasVP=%d vbValid=%d isDynamic=%d\n",
                    gBgfxDrawFrame, fCurrentView, program.idx, type, offset, count, hasVP, vbValid,
                    fCurrentGeometry ? fCurrentGeometry->IsDynamic() : -1);

            if (hasVP)
            {
                const float* vp = (const float*)fUniformUpdates[Uniform::kViewProjectionMatrix].uniform->GetData();
                fprintf(stderr, "  VP diag=[%f %f %f] trans=[%f %f]\n", vp[0], vp[5], vp[10], vp[12], vp[13]);
            }

            fprintf(stderr, "  TEX0=%s", fBoundTextures[0] ? "bound" : "NULL");
            if (fBoundTextures[0])
            {
                bgfx::TextureHandle th = fBoundTextures[0]->GetHandle();
                fprintf(stderr, " idx=%d valid=%d", th.idx, bgfx::isValid(th));
            }
            fprintf(stderr, "\n");
        }
        gBgfxDrawsThisFrame++;
    }

    // Handle TriangleFan conversion
    if( type == Geometry::kTriangleFan )
    {
        if( count < 3 )
        {
            return;  // Not enough vertices for a triangle
        }

        // Convert triangle fan to indexed triangles
        // Fan: V0,V1,V2,...,Vn -> Triangles: (V0,V1,V2), (V0,V2,V3), ..., (V0,Vn-1,Vn)
        uint32_t triCount = count - 2;
        
        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientIndexBuffer( &tib, triCount * 3 );
        
        if( tib.data )
        {
            uint16_t* indices = reinterpret_cast<uint16_t*>( tib.data );
            for( uint32_t i = 0; i < triCount; i++ )
            {
                indices[i * 3 + 0] = static_cast<uint16_t>( offset );           // V0
                indices[i * 3 + 1] = static_cast<uint16_t>( offset + i + 1 );   // V(i+1)
                indices[i * 3 + 2] = static_cast<uint16_t>( offset + i + 2 );   // V(i+2)
            }

            // 1. Build state
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
            bgfx::setState( state );

            // 2. Set scissor
            if( fScissorEnabled )
            {
                bgfx::setScissor( fScissorRect.x, fScissorRect.y, fScissorRect.w, fScissorRect.h );
            }
            else
            {
                bgfx::setScissor( 0, 0, 0, 0 );  // Disable scissor
            }

            // 3. Set geometry
            fCurrentGeometry->SetVertexBuffer( offset, count );
            bgfx::setIndexBuffer( &tib );

            // 4. Set textures
            for( U32 i = 0; i < kMaxTextureUnits; i++ )
            {
                if( fBoundTextures[i] )
                {
                    bgfx::TextureHandle texHandle = fBoundTextures[i]->GetHandle();
                    if( bgfx::isValid( texHandle ) )
                    {
                        bgfx::UniformHandle sampler = fCurrentProgram->GetSamplerHandle( i );
                        if( bgfx::isValid( sampler ) )
                        {
                            bgfx::setTexture( i, sampler, texHandle );
                        }
                    }
                }
            }

            // 5. Submit
            bgfx::submit( fCurrentView, program );
        }
    }
    else
    {
        // 1. Build state
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

        // Add primitive type
        uint64_t primType = ToBgfxPrimitiveType( type );
        if( primType != 0 )
        {
            state |= primType;
        }

        bgfx::setState( state );

        // 2. Set scissor
        if( fScissorEnabled )
        {
            bgfx::setScissor( fScissorRect.x, fScissorRect.y, fScissorRect.w, fScissorRect.h );
        }
        else
        {
            bgfx::setScissor( 0, 0, 0, 0 );  // Disable scissor
        }

        // 3. Set geometry
        fCurrentGeometry->SetVertexBuffer( offset, count );

        // 4. Set textures
        for( U32 i = 0; i < kMaxTextureUnits; i++ )
        {
            if( fBoundTextures[i] )
            {
                bgfx::TextureHandle texHandle = fBoundTextures[i]->GetHandle();
                if( bgfx::isValid( texHandle ) )
                {
                    bgfx::UniformHandle sampler = fCurrentProgram->GetSamplerHandle( i );
                    if( bgfx::isValid( sampler ) )
                    {
                        bgfx::setTexture( i, sampler, texHandle );
                    }
                }
            }
        }

        // 5. Submit
        bgfx::submit( fCurrentView, program );
    }
}

void
BgfxCommandBuffer::DrawIndexed( U32 offset, U32 count, Geometry::PrimitiveType type )
{
    if( !fCurrentGeometry )
    {
        Rtt_LogException( "bgfx: DrawIndexed called without bound geometry\n" );
        return;
    }

    if( !fCurrentProgram )
    {
        Rtt_LogException( "bgfx: DrawIndexed called without bound program\n" );
        return;
    }

    bgfx::ProgramHandle program = fCurrentProgram->GetHandle( fCurrentVersion );
    if( !bgfx::isValid( program ) )
    {
        Rtt_LogException( "bgfx: DrawIndexed called with invalid program\n" );
        return;
    }

    // Apply any pending uniforms
    ApplyUniforms();

    // 1. Build state
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

    // Add primitive type (only triangles supported for indexed)
    if( type == Geometry::kIndexedTriangles )
    {
        // Default is triangles, no flag needed
    }

    bgfx::setState( state );

    // 2. Set scissor
    if( fScissorEnabled )
    {
        bgfx::setScissor( fScissorRect.x, fScissorRect.y, fScissorRect.w, fScissorRect.h );
    }
    else
    {
        bgfx::setScissor( 0, 0, 0, 0 );  // Disable scissor
    }

    // 3. Set geometry (both vertex and index buffers)
    fCurrentGeometry->SetVertexBuffer( 0, 0 );  // Full vertex buffer
    fCurrentGeometry->SetIndexBuffer( offset, count );

    // 4. Set textures
    for( U32 i = 0; i < kMaxTextureUnits; i++ )
    {
        if( fBoundTextures[i] )
        {
            bgfx::TextureHandle texHandle = fBoundTextures[i]->GetHandle();
            if( bgfx::isValid( texHandle ) )
            {
                bgfx::UniformHandle sampler = fCurrentProgram->GetSamplerHandle( i );
                if( bgfx::isValid( sampler ) )
                {
                    bgfx::setTexture( i, sampler, texHandle );
                }
            }
        }
    }

    // 5. Submit
    bgfx::submit( fCurrentView, program );
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

    // Custom commands not yet supported in bgfx backend
    Rtt_LogException( "bgfx backend: custom commands not yet supported\n" );
}

bool
BgfxCommandBuffer::WriteNamedUniform( const char * uniformName, const void * data, unsigned int size )
{
    Rtt_UNUSED( uniformName );
    Rtt_UNUSED( data );
    Rtt_UNUSED( size );

    // Named uniforms not yet supported
    Rtt_LogException( "bgfx backend: WriteNamedUniform not yet implemented\n" );
    return false;
}

Real 
BgfxCommandBuffer::Execute( bool measureGPU )
{
    Rtt_UNUSED( measureGPU );

    // Ensure view 0 is submitted even without draw calls
    bgfx::touch(0);

    // Per-frame debug logging
    {
        static uint32_t sFrameNum = 0;
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 0, 0x0f, "bgfx frame=%u view=%d", sFrameNum, fCurrentView);
        if (sFrameNum < 5)
        {
            fprintf(stderr, "BGFX_FRAME[%u]: submitting frame\n", sFrameNum);
        }
        sFrameNum++;
        // Reset per-draw counter for next frame's Draw() logging
        extern uint32_t gBgfxDrawFrame;
        extern uint32_t gBgfxDrawsThisFrame;
        gBgfxDrawFrame = sFrameNum;
        gBgfxDrawsThisFrame = 0;
    }

    // bgfx::frame() submits all queued draw calls and swaps buffers
    bgfx::frame();

    // Reset instance data for next frame
    fInstanceCount = 0;
    fInstanceData = NULL;

    // Increment timestamp for uniform tracking
    gUniformTimestamp++;

    return 0.0f;
}

void
BgfxCommandBuffer::ApplyUniforms()
{
    if( !fCurrentProgram )
    {
        return;
    }

    for( U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i )
    {
        const UniformUpdate& update = fUniformUpdates[i];
        if( update.uniform )
        {
            fCurrentProgram->SetUniform( static_cast<Uniform::Name>( i ), update.uniform->GetData() );
        }
    }
}

bgfx::ViewId
BgfxCommandBuffer::AllocateViewId()
{
    // bgfx supports up to 256 views (0-255)
    // 0 is reserved for default screen
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
