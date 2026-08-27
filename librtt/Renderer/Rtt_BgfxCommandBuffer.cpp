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

#include "Renderer/Rtt_BgfxCommandBuffer.h"
#include "Renderer/Rtt_CPUResource.h"

#include "Renderer/Rtt_BgfxFrameBufferObject.h"
#include "Renderer/Rtt_BgfxGeometry.h"
#include "Renderer/Rtt_BgfxProgram.h"
#include "Renderer/Rtt_BgfxTexture.h"
#include "Renderer/Rtt_FrameBufferObject.h"
#include "Renderer/Rtt_Geometry_Renderer.h"
#include "Renderer/Rtt_Program.h"
#include "Renderer/Rtt_Texture.h"
#include "Renderer/Rtt_Uniform.h"
#include "Renderer/Rtt_Renderer.h"
#include "Renderer/Rtt_FormatExtensionList.h"
#include "Display/Rtt_ShaderData.h"
#include "Display/Rtt_ShaderResource.h"
#include "Display/Rtt_InstancedBatchRenderer.h"
#include "Display/Rtt_SDFRenderer.h"
#include "Core/Rtt_Config.h"
#include "Core/Rtt_Allocator.h"
#include "Core/Rtt_Assert.h"
#include "Core/Rtt_Math.h"
#include "Core/Rtt_CrashReporter.h"

#include <stdlib.h>
#include <string.h>
#include <cstddef>
#if !defined( Rtt_ANDROID_ENV )
#include <unistd.h>
#endif

#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#endif

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// Static members
U32 BgfxCommandBuffer::gUniformTimestamp = 0;
bgfx::ViewId BgfxCommandBuffer::sNextViewId = 1;  // Start at 1, 0 is default screen
std::unordered_map<std::string, bgfx::UniformHandle> BgfxCommandBuffer::sUniformHandleCache;

// Batch statistics
BgfxCommandBuffer::BatchStats BgfxCommandBuffer::sBatchStats = { 0, 0, 0, 0, 0, 0 };
bool BgfxCommandBuffer::sBatchingEnabled = true;

// D1' strip→indexed-tri conversion toggle (default ON, MASK_PV_STRIP_INDEX=0 to disable)
static bool sStripIndexConvert = true;
static bool sCheckedStripEnv = false;

static U8
MaskCountFromVersion( Program::Version version )
{
    return version == Program::kWireframe ? 0 : static_cast<U8>( version );
}

static Program::Version
BgfxProgramVersionForMaskCount( Program::Version version )
{
    return version == Program::kWireframe ? Program::kWireframe : Program::kMaskCount3;
}

// Bake per-vertex mask UVs (TexCoord2/3/4) into a transient vertex copy for
// stored-on-GPU geometry that bypassed Renderer::BakeMaskUVsIntoVertices.
// Such geometry takes the direct-bind branch in Rtt_Renderer (no per-frame
// vertex copy/bake), so its mask-UV slots hold the uninitialized values from
// mesh creation. Under an active mask the unified default FS samples the mask
// at those UVs and the fragment collapses to zero alpha (Issue #62). The mask
// matrices come from the draw's uniform snapshot; an absent matrix yields 0,
// matching BakeMaskUVsIntoVertices for inactive mask levels.
static void
BakeStoredGeometryMaskUVs( Geometry::Vertex* tv, U32 vertexCount,
    const Rtt::Real* m0, const Rtt::Real* m1, const Rtt::Real* m2 )
{
    for( U32 vi = 0; vi < vertexCount; ++vi )
    {
        Rtt::Real x = tv[vi].x, y = tv[vi].y;
        tv[vi].maskU0 = m0 ? ( m0[0] * x + m0[3] * y + m0[6] ) : Rtt_REAL_0;
        tv[vi].maskV0 = m0 ? ( m0[1] * x + m0[4] * y + m0[7] ) : Rtt_REAL_0;
        tv[vi].maskU1 = m1 ? ( m1[0] * x + m1[3] * y + m1[6] ) : Rtt_REAL_0;
        tv[vi].maskV1 = m1 ? ( m1[1] * x + m1[4] * y + m1[7] ) : Rtt_REAL_0;
        tv[vi].maskU2 = m2 ? ( m2[0] * x + m2[3] * y + m2[6] ) : Rtt_REAL_0;
        tv[vi].maskV2 = m2 ? ( m2[1] * x + m2[4] * y + m2[7] ) : Rtt_REAL_0;
    }
}

// Flag: setPlatformData was called (e.g. after lock-screen), force bgfx::reset on next SetViewport
static bool sPlatformDataChanged = false;

// Cached reset flags from bgfx::init — reused in SetViewport to avoid losing FLIP_AFTER_RENDER etc.
// FLIP_AFTER_RENDER: present frame N AFTER rendering N (not before), so setSkipPresent(true)
// before bgfx::frame() correctly skips frame N's presentation without racing with frame N-1.
static uint32_t sResetFlags = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4 | BGFX_RESET_FLIP_AFTER_RENDER;

// Last screen backbuffer size applied via bgfx::reset().
static uint16_t sLastBackbufferWidth = 0;
static uint16_t sLastBackbufferHeight = 0;

// Frame counter (used for diagnostic logging)
static uint32_t sFrameNum = 0;

void BgfxCommandBuffer::NotifyPlatformDataChanged()
{
    sPlatformDataChanged = true;
}

void BgfxCommandBuffer::FlushPlatformDataChange()
{
    if (sPlatformDataChanged && sLastBackbufferWidth > 0 && sLastBackbufferHeight > 0)
    {
        Rtt_LogException("FlushPlatformDataChange: forcing bgfx::reset(%d,%d) + frame() BEFORE resource creation",
            sLastBackbufferWidth, sLastBackbufferHeight);
        bgfx::reset(sLastBackbufferWidth, sLastBackbufferHeight, sResetFlags);
        ++Renderer::sBgfxContextGeneration;
        sPlatformDataChanged = false;
        // Submit an empty frame so bgfx fully processes the context change
        // before any new programs/textures are created
        bgfx::frame();
    }
}

void BgfxCommandBuffer::PrepareForResourceCreation()
{
    FlushPlatformDataChange();
}

void BgfxCommandBuffer::SetCachedResetFlags(uint32_t flags)
{
    sResetFlags = flags;
}

// ----------------------------------------------------------------------------

BgfxCommandBuffer::BgfxCommandBuffer( Rtt_Allocator* allocator )
:   CommandBuffer( allocator ),
    fCurrentView( 0 ),
    fMaxFboViewId( 0 ),
    fCurrentGeometry( NULL ),
    fCurrentProgram( NULL ),
    fCurrentVersion( Program::kMaskCount0 ),
    fCurrentMaskCount( 0 ),
    fNextDrawVertexExtList( NULL ),
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
    fSkipCurrentFbo( false ),
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
    ClearUniformCache();
}

void
BgfxCommandBuffer::Initialize()
{
    // bgfx initialization is handled by BgfxRenderer
    InitializeFBO();

    // Configure clear state after InitializeFBO assigns the actual screen view ID.
    bgfx::setViewClear( fDefaultView,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        0x00000000,
        fClearDepth,
        fClearStencil );

    InitializeCachedParams();

    // Query max texture size from bgfx caps
    GetMaxTextureSize();

    // Set statics so Renderer::GetMaxTextureSize() returns bgfx value
    // instead of GL's glGetIntegerv (which fails without GL context)
    const bgfx::Caps* caps = bgfx::getCaps();
    Renderer::sIsBgfxRenderer = true;
    Renderer::sBgfxMaxTextureSize = caps->limits.maxTextureSize;
    Renderer::sBgfxSupportsHighPrecision = true;
    Renderer::sBgfxVendorString = "bgfx";
    Renderer::sBgfxRendererString = bgfx::getRendererName(caps->rendererType);
    Renderer::sBgfxVersionString = "";
    Renderer::sBgfxMaxUniformVectors = 256;
    Renderer::sBgfxMaxVertexTextureUnits = 4;
    Rtt_LogException("BgfxCommandBuffer: maxTextureSize = %u (renderer=%d)",
        caps->limits.maxTextureSize, caps->rendererType);
}

void
BgfxCommandBuffer::InitializeFBO()
{
    // Screen uses high view ID so FBO views (1, 2, 3, ...) render first
    // bgfx renders views in ascending ID order by default
    fDefaultView = 200;
    fCurrentView = fDefaultView;
    fMaxFboViewId = 0;
    fSkipCurrentFbo = false;

    // CRITICAL: Solar2D uses painter's algorithm (draw order = layer order).
    // Without Sequential mode, bgfx may reorder draws by state for performance,
    // causing incorrect overlapping (e.g., background drawn over foreground).
    bgfx::setViewMode( fDefaultView, bgfx::ViewMode::Sequential );

    // Pre-register the default view with a dummy rect so bgfx knows about it
    // before Solar2D Splash submits its first draw calls. Without this, the
    // Splash phase (~45 draws before real setViewRect) triggers a false
    // BLACK_SCREEN_DETECTED diagnostic (numViews==0 while numDraw>0).
    bgfx::setViewRect( fDefaultView, 0, 0, 1, 1 );
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
            Rtt_LogException("BgfxCommandBuffer: maxTextureSize = %d (renderer=%d)",
                caps->limits.maxTextureSize, caps->rendererType);
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
    ClearUniformCache();
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
    DeferredCmd cmd = {};
    cmd.type = DeferredCmd::kBindFBO;
    cmd.fbo = fbo;
    fDeferredCmds.push_back( cmd );
}

void
BgfxCommandBuffer::CaptureRect( FrameBufferObject* fbo, Texture& texture, const Rect& rect, const Rect& rawRect )
{
    DeferredCmd cmd = {};
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
        fCurrentVersion = BgfxProgramVersionForMaskCount( version );
        fCurrentMaskCount = MaskCountFromVersion( version );

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
BgfxCommandBuffer::SetNextDrawVertexExtension( const FormatExtensionList* extensionList )
{
    fNextDrawVertexExtList = extensionList;
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
    uint16_t w = static_cast<uint16_t>( width );
    uint16_t h = static_cast<uint16_t>( height );

    // Defer setViewRect to Execute (needs correct view ID from FBO)
    DeferredCmd cmd = {};
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
    DeferredCmd cmd = {};
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
            if( size < 16 )
            {
                memset( cmd.uniforms[i].data + size, 0, 16 - size );
            }
        }
        else
        {
            cmd.uniforms[i].valid = false;
            cmd.uniforms[i].size = 0;
        }
    }

    // Snapshot pending named uniforms into side table
    int count = (int)fPendingNamedUniforms.size();
    if( count > DeferredCmd::kMaxNamedUniforms )
    {
        count = DeferredCmd::kMaxNamedUniforms;
    }
    cmd.namedUniformCount = count;
    if( count > 0 )
    {
        cmd.namedUniformOffset = (int)fNamedUniformSideTable.size();
        for( int i = 0; i < count; ++i )
        {
            DeferredCmd::NamedUniformSnapshot snap = {};
            strncpy( snap.name, fPendingNamedUniforms[i].name, 63 );
            snap.name[63] = '\0';
            memcpy( snap.data, fPendingNamedUniforms[i].data, fPendingNamedUniforms[i].size );
            snap.size = fPendingNamedUniforms[i].size;
            fNamedUniformSideTable.push_back( snap );
        }
    }
    else
    {
        cmd.namedUniformOffset = -1;
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
    DeferredCmd cmd = {};
    cmd.type = DeferredCmd::kDraw;
    cmd.geometry = fCurrentGeometry;
    cmd.vertexExtList = fNextDrawVertexExtList;
    fNextDrawVertexExtList = NULL; // consume — do not leak into the next draw
    cmd.program = fCurrentProgram;
    cmd.programVersion = fCurrentVersion;
    cmd.maskCount = fCurrentMaskCount;
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

    DeferredCmd cmd = {};
    cmd.type = DeferredCmd::kDrawIndexed;
    cmd.geometry = fCurrentGeometry;
    cmd.vertexExtList = fNextDrawVertexExtList;
    fNextDrawVertexExtList = NULL; // consume — do not leak into the next draw
    cmd.program = fCurrentProgram;
    cmd.programVersion = fCurrentVersion;
    cmd.maskCount = fCurrentMaskCount;
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
    fSkipCurrentFbo = false;
    if( cmd.fbo && CPUResource::IsAlive(cmd.fbo) )
    {
        void* gpuRes = cmd.fbo->GetGPUResource();
        BgfxFrameBufferObject* bgfxFbo = static_cast<BgfxFrameBufferObject*>( gpuRes );
        if( bgfxFbo && bgfxFbo->IsActive() )
        {
            fCurrentView = bgfxFbo->GetViewId();
            if( fCurrentView > fMaxFboViewId )
            {
                fMaxFboViewId = fCurrentView;
            }
            bgfx::setViewFrameBuffer( fCurrentView, bgfxFbo->GetHandle() );
            bgfx::setViewMode( fCurrentView, bgfx::ViewMode::Sequential );
            if( cmd.fbo->GetMustClear() )
            {
                // Default offscreen shader passes need a transparent render target.
                // Metal can otherwise load stale alpha and make blur/dropshadow
                // sample a solid block.
                bgfx::setViewClear( fCurrentView,
                    BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                    0x00000000, 1.0f, 0 );
            }
            else
            {
                // Preserve contents for accumulative targets such as canvas
                // tex:invalidate() without "cache". Explicit Clear commands
                // still override this in the same frame.
                bgfx::setViewClear( fCurrentView, BGFX_CLEAR_NONE );
            }
        }
        else
        {
            fCurrentView = fDefaultView;
            fSkipCurrentFbo = true;
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
    if( fSkipCurrentFbo )
    {
        return;
    }

    if( fCurrentView == fDefaultView
        && ( cmd.vpW != sLastBackbufferWidth || cmd.vpH != sLastBackbufferHeight || sPlatformDataChanged ) )
    {
        // RISK: bgfx::reset() clears all view state (clear color, view mode, etc.)
        // set during Initialize(). If this reset fires here (e.g. window resize),
        // view 0's Sequential mode and clear color are lost until next Initialize().
        // This may cause Issue #10 (intermittent black screen after resume) on screens
        // that rely on view state set only once in Initialize().
        // Potential fix: re-apply setViewClear + setViewMode after reset().
        bgfx::reset( cmd.vpW, cmd.vpH, sResetFlags );
        ++Renderer::sBgfxContextGeneration;
        sLastBackbufferWidth = cmd.vpW;
        sLastBackbufferHeight = cmd.vpH;
        sPlatformDataChanged = false;

        // bgfx::reset() invalidates all view state (clear color, view mode, etc.).
        // Re-apply Sequential mode on ALL views to prevent stale Default mode
        // from producing rendering artifacts on certain drivers (e.g. Mali-G76).
        // But clear only for fDefaultView; FBO views are configured when bound.
        const uint32_t numViews = bgfx::getCaps()->limits.maxViews;
        for (uint32_t v = 0; v < numViews; ++v) {
            bgfx::setViewMode( (bgfx::ViewId)v, bgfx::ViewMode::Sequential );
        }
        bgfx::setViewClear( fDefaultView,
            BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
            0x00000000,
            fClearDepth,
            fClearStencil );
    }

    bgfx::setViewRect( fCurrentView, cmd.vpX, cmd.vpY, cmd.vpW, cmd.vpH );
}

void
BgfxCommandBuffer::ExecuteClear( const DeferredCmd& cmd )
{
    if( fSkipCurrentFbo )
    {
        return;
    }

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
    if( cmd.textures[0] && CPUResource::IsAlive(cmd.textures[0]) )
    {
        BgfxTexture* tex = static_cast<BgfxTexture*>( cmd.textures[0]->GetGPUResource() );
        if( tex && tex->GetCachedFormat() == static_cast<S32>( Texture::kAlpha ) )
        {
            texFlags[0] = 1.0f;
        }
    }
    // Pass mask count so shader knows how many mask samplers to apply.
    texFlags[1] = static_cast<float>( cmd.maskCount );
    // sRGB-correct alpha blending (#30): gate the shader-side fill/vertex color
    // sRGB->linear decode. Off (default) => unchanged behavior.
    texFlags[2] = Renderer::sSRGBAlphaBlending ? 1.0f : 0.0f;
    bgfx::UniformHandle texFlagsHandle = prog->GetTexFlagsHandle();
    if( bgfx::isValid( texFlagsHandle ) )
    {
        bgfx::setUniform( texFlagsHandle, texFlags );
    }
}

void
BgfxCommandBuffer::ApplyNamedUniforms( const DeferredCmd& cmd )
{
    if( cmd.namedUniformCount <= 0 || cmd.namedUniformOffset < 0 )
    {
        return;
    }
    for( int i = 0; i < cmd.namedUniformCount; ++i )
    {
        const DeferredCmd::NamedUniformSnapshot& nu = fNamedUniformSideTable[cmd.namedUniformOffset + i];
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

        // Build cache key from name, type, and element count
        std::string key = std::string( nu.name ) + "|" + std::to_string( static_cast<int>( utype ) ) + "|" + std::to_string( numElements );

        // Look up or create cached uniform handle
        auto it = sUniformHandleCache.find( key );
        bgfx::UniformHandle handle = BGFX_INVALID_HANDLE;
        bool found = ( it != sUniformHandleCache.end() && bgfx::isValid( it->second ) );
        if( found )
        {
            handle = it->second;
        }
        else
        {
            handle = bgfx::createUniform( nu.name, utype, numElements );
            if( bgfx::isValid( handle ) )
            {
                sUniformHandleCache[key] = handle;
            }
        }

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
            // Keep handle in cache — don't destroy until ClearUniformCache
        }
    }
}

void
BgfxCommandBuffer::ClearUniformCache()
{
    for( auto& entry : sUniformHandleCache )
    {
        if( bgfx::isValid( entry.second ) )
        {
            bgfx::destroy( entry.second );
        }
    }
    sUniformHandleCache.clear();
}

void
BgfxCommandBuffer::ExecuteDraw( const DeferredCmd& cmd )
{
    if( fSkipCurrentFbo )
    {
        return;
    }

    // (diagnostic removed - texture binding verified correct)

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

        // bgfx view/projection are identity in this backend. Supply Solar2D's
        // view-projection matrix as the draw transform so u_modelViewProj is valid.
        if ( cmd.uniforms[Uniform::kViewProjectionMatrix].valid )
        {
            bgfx::setTransform( cmd.uniforms[Uniform::kViewProjectionMatrix].data );
        }

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
            if ( cmd.textures[i] && CPUResource::IsAlive(cmd.textures[i]) )
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
                                bgfx::setTexture( i, sampler, texHandle, tex->GetSamplerFlags() );
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
    if ( !cmd.geometry || !CPUResource::IsAlive(cmd.geometry) )
    {
        return;
    }
    BgfxGeometry* geo = static_cast<BgfxGeometry*>( cmd.geometry->GetGPUResource() );
    if( !geo )
    {
        return;
    }

    if ( !cmd.program || !CPUResource::IsAlive(cmd.program) )
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
        static int sDrawInvalidCount = 0;
        if( ++sDrawInvalidCount <= 3 )
        {
            ShaderResource* sr = cmd.program
                ? static_cast<Program*>(cmd.program)->GetShaderResource() : NULL;
            Rtt_LogException("WARNING: ExecuteDraw skipped: invalid program handle for effect='%s' "
                "(version=%d) — shader load/compile failed; draw will not appear\n",
                sr ? sr->GetName().c_str() : "(default)", (int)cmd.programVersion);
        }
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

    // Pooled/batched geometry carrying a vertex-format extension: the pool
    // buffer holds interleaved (base+extension) units, so it must be bound with
    // the extended (factor*68-byte) layout and addressed in real-vertex units.
    // cmd.offset is in base-vertex slots; convert it by the (1+extraCount) factor.
    const FormatExtensionList* extList =
        ( cmd.vertexExtList && cmd.vertexExtList->HasVertexRateData() ) ? cmd.vertexExtList : NULL;
    const U32 extFactor = extList ? ( 1 + extList->ExtraVertexCount() ) : 1;
    const U32 drawOffset = extList ? ( cmd.offset / extFactor ) : cmd.offset;

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
                indices[i * 3 + 0] = static_cast<uint16_t>( drawOffset );
                indices[i * 3 + 1] = static_cast<uint16_t>( drawOffset + i + 1 );
                indices[i * 3 + 2] = static_cast<uint16_t>( drawOffset + i + 2 );
            }

            bgfx::setState( cmd.bgfxState );

            if( cmd.scissorEnabled )
            {
                bgfx::setScissor( cmd.scissorX, cmd.scissorY, cmd.scissorW, cmd.scissorH );
            }

            if( extList )
            {
                geo->SetVertexBufferExt( cmd.offset, cmd.count, extList );
            }
            else
            {
                geo->SetVertexBuffer( cmd.offset, cmd.count );
            }
            bgfx::setIndexBuffer( &tib );

            // Set textures
            for( U32 i = 0; i < kMaxTextureUnits; i++ )
            {
                if( cmd.textures[i] && CPUResource::IsAlive(cmd.textures[i]) )
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
                                bgfx::setTexture( i, sampler, texHandle, tex->GetSamplerFlags() );
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

        const Rtt::Real* m0 = cmd.uniforms[Uniform::kMaskMatrix0].valid
            ? reinterpret_cast<const Rtt::Real*>( cmd.uniforms[Uniform::kMaskMatrix0].data ) : NULL;
        const Rtt::Real* m1 = cmd.uniforms[Uniform::kMaskMatrix1].valid
            ? reinterpret_cast<const Rtt::Real*>( cmd.uniforms[Uniform::kMaskMatrix1].data ) : NULL;
        const Rtt::Real* m2 = cmd.uniforms[Uniform::kMaskMatrix2].valid
            ? reinterpret_cast<const Rtt::Real*>( cmd.uniforms[Uniform::kMaskMatrix2].data ) : NULL;

        if( geo->StoredOnGPU() && ( m0 || m1 || m2 ) )
        {
            // Active mask + stored-on-GPU: transient copy needed to bake per-vertex
            // mask UVs — the static bind path never runs BakeMaskUVsIntoVertices.
            const Geometry::Vertex* vertexData = cmd.geometry->GetVertexData();
            U32 vertexCount = cmd.geometry->GetVerticesUsed();
            if( !vertexData || vertexCount == 0 )
            {
                return;
            }
            bgfx::TransientVertexBuffer tvb;
            if( bgfx::getAvailTransientVertexBuffer( vertexCount, BgfxGeometry::GetVertexLayout() ) < vertexCount )
            {
                return;
            }
            bgfx::allocTransientVertexBuffer( &tvb, vertexCount, BgfxGeometry::GetVertexLayout() );
            memcpy( tvb.data, vertexData, vertexCount * sizeof( Geometry::Vertex ) );
            BakeStoredGeometryMaskUVs( reinterpret_cast<Geometry::Vertex*>( tvb.data ), vertexCount, m0, m1, m2 );
            bgfx::setVertexBuffer( 0, &tvb, cmd.offset, cmd.count );
        }
        else if( extList )
        {
            // Per-draw vertex-extension override. Pass cmd.geometry explicitly so
            // SetVertexBufferExt can still source the interleaved units when its
            // lazy fPendingGeometry has already been consumed this frame (Metal/
            // GLES) — otherwise it falls back to the 68-byte bind and reads the
            // 136-byte units at the wrong stride, exploding the geometry (#079).
            geo->SetVertexBufferExt( cmd.offset, cmd.count, extList, cmd.geometry );
        }
        else
        {
            geo->SetVertexBuffer( cmd.offset, cmd.count );
        }

        // Set textures
        for( U32 i = 0; i < kMaxTextureUnits; i++ )
        {
            if( cmd.textures[i] && CPUResource::IsAlive(cmd.textures[i]) )
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
                            bgfx::setTexture( i, sampler, texHandle, tex->GetSamplerFlags() );
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
    if( fSkipCurrentFbo )
    {
        return;
    }

    if ( !cmd.geometry || !CPUResource::IsAlive(cmd.geometry) )
    {
        return;
    }
    BgfxGeometry* geo = static_cast<BgfxGeometry*>( cmd.geometry->GetGPUResource() );
    if( !geo )
    {
        return;
    }

    if ( !cmd.program || !CPUResource::IsAlive(cmd.program) )
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
        static int sDrawIdxInvalidCount = 0;
        if( ++sDrawIdxInvalidCount <= 3 )
        {
            ShaderResource* sr = cmd.program
                ? static_cast<Program*>(cmd.program)->GetShaderResource() : NULL;
            Rtt_LogException("WARNING: ExecuteDrawIndexed skipped: invalid program handle for effect='%s' "
                "(version=%d) — shader load/compile failed; draw will not appear\n",
                sr ? sr->GetName().c_str() : "(default)", (int)cmd.programVersion);
        }
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

    // Resolve the effective vertex-rate extension list for this indexed draw:
    // either the per-draw pool override (cmd.vertexExtList) or the geometry's
    // own list (stored-on-GPU indexed geometry carries it via CreateStatic).
    // When present, the interleaved (base+extension) units must be bound with
    // the extended (factor*68-byte) layout — the fixed 68-byte path would read
    // the wrong bytes and garble the fill (#079).
    const FormatExtensionList* idxExtList = NULL;
    if( cmd.vertexExtList && cmd.vertexExtList->HasVertexRateData() )
    {
        idxExtList = cmd.vertexExtList;
    }
    else
    {
        const FormatExtensionList* ownList = cmd.geometry->GetExtensionList();
        if( ownList && ownList->HasVertexRateData() )
        {
            idxExtList = ownList;
        }
    }

    if( !geo->IsTransient() && geo->HasStaticVB() && geo->HasStaticIB() )
    {
        // storedOnGPU: use existing static buffers directly. CreateStatic built
        // the (extended) layout from the geometry's own extension list, so the
        // static VB is already correct for extension geometry.
        geo->SetVertexBuffer( 0, cmd.geometry->GetVerticesUsed() );
        geo->SetIndexBuffer( 0, cmd.geometry->GetIndicesUsed() );
    }
    else
    {
        // Transient/dynamic path: copy data into transient buffers
        const Geometry::Index* indexData = cmd.geometry->GetIndexData();
        U32 indexCount = cmd.geometry->GetIndicesUsed();
        if( !indexData || indexCount == 0 )
        {
            return;
        }

        bgfx::TransientIndexBuffer tib;
        if( bgfx::getAvailTransientIndexBuffer( indexCount ) < indexCount )
        {
            return;
        }
        bgfx::allocTransientIndexBuffer( &tib, indexCount );
        memcpy( tib.data, indexData, indexCount * sizeof( Geometry::Index ) );

        const Geometry::Vertex* vertexData = cmd.geometry->GetVertexData();
        U32 vertexCount = cmd.geometry->GetVerticesUsed();
        if( !vertexData || vertexCount == 0 )
        {
            return;
        }

        // Vertex-rate extension geometry on the transient indexed path: bind the
        // extended (factor*68) layout and upload the spliced extended data, so
        // the GPU steps the interleaved units correctly (#079). The index buffer
        // above is unchanged (indices address real vertices, which map 1:1).
        if( idxExtList )
        {
            U32 strideBytes = 0, vertexDataSize = 0;
            const Geometry::Vertex* uploadData =
                geo->PrepareVertexUploadExt( cmd.geometry, idxExtList, vertexCount, strideBytes, vertexDataSize );
            const bgfx::VertexLayout& extLayout = geo->ResolveLayoutExt( cmd.geometry, idxExtList );
            if( uploadData && bgfx::getAvailTransientVertexBuffer( vertexCount, extLayout ) >= vertexCount )
            {
                bgfx::TransientVertexBuffer etvb;
                bgfx::allocTransientVertexBuffer( &etvb, vertexCount, extLayout );
                memcpy( etvb.data, uploadData, vertexDataSize );
                bgfx::setVertexBuffer( 0, &etvb );
                bgfx::setIndexBuffer( &tib );
                // Set textures, then submit (skip the fixed-68B path below).
                for( U32 i = 0; i < kMaxTextureUnits; i++ )
                {
                    if( cmd.textures[i] && CPUResource::IsAlive(cmd.textures[i]) )
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
                                    bgfx::setTexture( i, sampler, texHandle, tex->GetSamplerFlags() );
                                }
                            }
                        }
                    }
                }
                bgfx::submit( fCurrentView, programHandle );
                return;
            }
            // Insufficient transient space or no data → fall through to the
            // fixed-68B path (degraded fill, but no garbage/crash).
        }

        bgfx::TransientVertexBuffer tvb;
        if( bgfx::getAvailTransientVertexBuffer( vertexCount, BgfxGeometry::GetVertexLayout() ) < vertexCount )
        {
            return;
        }
        bgfx::allocTransientVertexBuffer( &tvb, vertexCount, BgfxGeometry::GetVertexLayout() );
        memcpy( tvb.data, vertexData, vertexCount * sizeof( Geometry::Vertex ) );

        // Stored-on-GPU geometry skips Renderer::BakeMaskUVsIntoVertices (the
        // stored-on-GPU branch binds the buffer directly, without the per-frame
        // vertex copy that bakes mask UVs). Issue #40 forces such geometry onto
        // this transient path, so its per-vertex mask-UV slots (a_texcoord2/3/4)
        // still hold the uninitialized values from mesh creation. Under an active
        // mask the unified default FS samples the mask at those UVs and the
        // primitive collapses to zero alpha. Bake only when a mask is active;
        // without an active mask the FS never samples the slots so baking is
        // unnecessary work.
        if( geo->StoredOnGPU() )
        {
            const Rtt::Real* m0 = cmd.uniforms[Uniform::kMaskMatrix0].valid
                ? reinterpret_cast<const Rtt::Real*>( cmd.uniforms[Uniform::kMaskMatrix0].data ) : NULL;
            const Rtt::Real* m1 = cmd.uniforms[Uniform::kMaskMatrix1].valid
                ? reinterpret_cast<const Rtt::Real*>( cmd.uniforms[Uniform::kMaskMatrix1].data ) : NULL;
            const Rtt::Real* m2 = cmd.uniforms[Uniform::kMaskMatrix2].valid
                ? reinterpret_cast<const Rtt::Real*>( cmd.uniforms[Uniform::kMaskMatrix2].data ) : NULL;
            if( m0 || m1 || m2 )
            {
                BakeStoredGeometryMaskUVs( reinterpret_cast<Geometry::Vertex*>( tvb.data ), vertexCount, m0, m1, m2 );
            }
        }

        bgfx::setVertexBuffer( 0, &tvb );
        bgfx::setIndexBuffer( &tib );
    }

    // Set textures
    for( U32 i = 0; i < kMaxTextureUnits; i++ )
    {
        if( cmd.textures[i] && CPUResource::IsAlive(cmd.textures[i]) )
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
                        bgfx::setTexture( i, sampler, texHandle, tex->GetSamplerFlags() );
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
    if( fSkipCurrentFbo )
    {
        return;
    }

    // Get the destination texture from the capture FBO
    BgfxFrameBufferObject* dstFbo = NULL;
    bgfx::TextureHandle dstTexHandle = BGFX_INVALID_HANDLE;
    bgfx::ViewId blitView = fCurrentView;

    if( cmd.captureFbo && CPUResource::IsAlive(cmd.captureFbo) )
    {
        void* gpuRes = cmd.captureFbo->GetGPUResource();
        dstFbo = static_cast<BgfxFrameBufferObject*>( gpuRes );
        if( dstFbo && dstFbo->IsActive() )
        {
            dstTexHandle = dstFbo->GetTextureHandle();
            blitView = dstFbo->GetViewId();
        }
    }
    else if( cmd.captureTexture && CPUResource::IsAlive(cmd.captureTexture) )
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

// ============================================================================
// Draw Call Batching
// ============================================================================

// A draw carries vertex-rate format-extension data when either the per-draw
// pool override (cmd.vertexExtList, set for off-GPU pooled geometry) or the
// geometry's OWN extension list (stored-on-GPU indexed geometry, set via
// CreateStatic) has vertex-rate attributes. Such draws use the extended
// (factor*68-byte) interleaved stride and MUST NOT be merged at the fixed
// 68-byte Geometry::Vertex stride (that reads the wrong bytes → garbled fill).
// They fall through to ExecuteDraw / ExecuteDrawIndexed, which bind the
// matching extended layout (#079).
bool
BgfxCommandBuffer::DrawHasVertexExtension( const DeferredCmd& cmd )
{
    if( cmd.vertexExtList && cmd.vertexExtList->HasVertexRateData() )
    {
        return true;
    }
    if( cmd.geometry )
    {
        const FormatExtensionList* own = cmd.geometry->GetExtensionList();
        if( own && own->HasVertexRateData() )
        {
            return true;
        }
    }
    return false;
}

bool
BgfxCommandBuffer::CanBatchDraws( const DeferredCmd& a, const DeferredCmd& b ) const
{
    // Must be same draw type
    if( a.type != b.type ) return false;

    // Only batch kDraw and kDrawIndexed
    if( a.type != DeferredCmd::kDraw && a.type != DeferredCmd::kDrawIndexed ) return false;

    // No instanced draws
    if( a.instanceDraw || b.instanceDraw ) return false;

    // No triangle fans (can't trivially concatenate without index conversion)
    if( a.primitiveType == Geometry::kTriangleFan || b.primitiveType == Geometry::kTriangleFan ) return false;

    // No vertex-format extension draws. Geometry carrying a vertex-rate
    // extension (pool override OR its own list, e.g. stored-on-GPU indexed
    // meshes) holds interleaved (base+extension) units at the extended stride
    // (factor*68 bytes); the batch merge below copies/merges at the fixed
    // 68-byte Geometry::Vertex stride, which would read the wrong bytes (garbled
    // fill). Let these fall through to ExecuteDraw / ExecuteDrawIndexed, which
    // bind the matching extended layout (#079).
    if( DrawHasVertexExtension( a ) || DrawHasVertexExtension( b ) ) return false;

    // No triangle strips unless D1' index conversion is enabled.
    // When enabled, strip batches are reindexed to indexed triangles via
    // transient IBO in ExecuteBatchedDraws.
    bool aStrip = ( a.primitiveType == Geometry::kTriangleStrip );
    bool bStrip = ( b.primitiveType == Geometry::kTriangleStrip );
    if( !sStripIndexConvert && ( aStrip || bStrip ) ) return false;

    // Must be same primitive type, and must be triangle-based
    if( a.primitiveType != b.primitiveType ) return false;
    if( a.primitiveType != Geometry::kTriangles &&
        a.primitiveType != Geometry::kIndexedTriangles &&
        a.primitiveType != Geometry::kTriangleStrip ) return false;

    // Same program and bgfx handle version. Masked draws all use the same
    // canonical version; wireframe remains distinct.
    if( a.program != b.program ) return false;
    if( a.programVersion != b.programVersion ) return false;

    // Mask UVs are baked into the vertex stream, so draws with the same
    // actual mask count can batch across different mask matrices. The count is
    // now a uniform, not a program version, and a batched submit can only set
    // that uniform once.
    if( a.maskCount != b.maskCount ) return false;

    // Same render state (blend, primitive type, MSAA)
    if( a.bgfxState != b.bgfxState ) return false;

    // Same scissor
    if( a.scissorEnabled != b.scissorEnabled ) return false;
    if( a.scissorEnabled )
    {
        if( a.scissorX != b.scissorX || a.scissorY != b.scissorY ||
            a.scissorW != b.scissorW || a.scissorH != b.scissorH ) return false;
    }

    // Same textures (all units)
    for( U32 i = 0; i < 8; ++i )
    {
        if( a.textures[i] != b.textures[i] ) return false;
    }

    // No named uniforms (custom effects can't batch)
    if( a.namedUniformCount > 0 || b.namedUniformCount > 0 ) return false;

    // Must have valid geometry with CPU data
    if( !a.geometry || !b.geometry ) return false;
    if( !CPUResource::IsAlive( a.geometry ) || !CPUResource::IsAlive( b.geometry ) ) return false;

    return true;
}

size_t
BgfxCommandBuffer::ExecuteBatchedDraws( size_t startIdx )
{
    if( fSkipCurrentFbo ) return 0;
    if( !sBatchingEnabled ) return 0;

    const DeferredCmd& first = fDeferredCmds[startIdx];

    // Only batch draw commands
    if( first.type != DeferredCmd::kDraw && first.type != DeferredCmd::kDrawIndexed ) return 0;

    // Skip instanced and fan draws
    if( first.instanceDraw ) return 0;
    if( first.primitiveType == Geometry::kTriangleFan ) return 0;

    // Skip vertex-format extension draws (interleaved extended stride; the merge
    // copies at the fixed 68-byte stride and would garble the fill). These go
    // through ExecuteDraw / ExecuteDrawIndexed with the matching layout (#079).
    // Check the geometry's own list too (stored-on-GPU indexed meshes carry the
    // extension there, not in vertexExtList).
    if( DrawHasVertexExtension( first ) ) return 0;

    // Find the end of the batchable run
    size_t end = startIdx + 1;
    while( end < fDeferredCmds.size() && CanBatchDraws( first, fDeferredCmds[end] ) )
    {
        ++end;
    }

    size_t batchSize = end - startIdx;
    if( batchSize < 2 ) return 0; // Not worth batching a single draw

    bool isIndexed = ( first.type == DeferredCmd::kDrawIndexed );
    bool hasStrip = ( first.primitiveType == Geometry::kTriangleStrip );

    // Indexed draws and strip draws need an index buffer for the merged output
    bool needsIndexBuffer = isIndexed || hasStrip;

    // Calculate total vertices and indices needed
    U32 totalVertices = 0;
    U32 totalIndices = 0;

    for( size_t i = startIdx; i < end; ++i )
    {
        const DeferredCmd& cmd = fDeferredCmds[i];
        if( !cmd.geometry || !CPUResource::IsAlive( cmd.geometry ) ) continue;

        if( isIndexed )
        {
            totalVertices += cmd.geometry->GetVerticesUsed();
            totalIndices += cmd.geometry->GetIndicesUsed();
        }
        else if( hasStrip )
        {
            totalVertices += cmd.count;
            if( cmd.count > 2 )
            {
                totalIndices += ( cmd.count - 2 ) * 3;
            }
        }
        else
        {
            totalVertices += cmd.count;
        }
    }

    if( totalVertices == 0 ) return 0;

    // Check transient buffer availability
    if( bgfx::getAvailTransientVertexBuffer( totalVertices, BgfxGeometry::GetVertexLayout() ) < totalVertices )
    {
        return 0;
    }

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer( &tvb, totalVertices, BgfxGeometry::GetVertexLayout() );

    bgfx::TransientIndexBuffer tib;
    if( needsIndexBuffer )
    {
        if( totalIndices == 0 ) return 0;
        if( bgfx::getAvailTransientIndexBuffer( totalIndices ) < totalIndices )
        {
            return 0;
        }
        bgfx::allocTransientIndexBuffer( &tib, totalIndices );
    }

    // Merge vertex/index data from all draws in the batch
    U32 vertexOffset = 0;
    U32 indexOffset = 0;
    U32 vertexSize = sizeof( Geometry::Vertex );

    for( size_t i = startIdx; i < end; ++i )
    {
        const DeferredCmd& cmd = fDeferredCmds[i];
        if( !cmd.geometry || !CPUResource::IsAlive( cmd.geometry ) ) continue;

        const Geometry::Vertex* srcVertices = cmd.geometry->GetVertexData();
        if( !srcVertices ) continue;

        if( isIndexed )
        {
            U32 vCount = cmd.geometry->GetVerticesUsed();
            U32 iCount = cmd.geometry->GetIndicesUsed();
            const Geometry::Index* srcIndices = cmd.geometry->GetIndexData();

            if( !srcIndices || iCount == 0 || vCount == 0 ) continue;

            // Copy vertices
            memcpy( tvb.data + vertexOffset * vertexSize,
                    srcVertices, vCount * vertexSize );

            // Copy indices with rebasing
            uint16_t* dstIndices = reinterpret_cast<uint16_t*>( tib.data ) + indexOffset;
            for( U32 j = 0; j < iCount; ++j )
            {
                dstIndices[j] = static_cast<uint16_t>( srcIndices[j] + vertexOffset );
            }

            vertexOffset += vCount;
            indexOffset += iCount;
        }
        else if( hasStrip )
        {
            // Strip→indexed tri conversion
            U32 vCount = cmd.count;
            if( vCount == 0 ) continue;

            // Copy vertices
            memcpy( tvb.data + vertexOffset * vertexSize,
                    srcVertices + cmd.offset, vCount * vertexSize );

            // Generate indices for strip topology
            if( vCount > 2 )
            {
                uint32_t triCount = vCount - 2;
                uint16_t* dstIndices = reinterpret_cast<uint16_t*>( tib.data ) + indexOffset;
                for( uint32_t t = 0; t < triCount; ++t )
                {
                    if( ( t & 1u ) == 0 )
                    {
                        dstIndices[t * 3 + 0] = static_cast<uint16_t>( vertexOffset + t );
                        dstIndices[t * 3 + 1] = static_cast<uint16_t>( vertexOffset + t + 1 );
                        dstIndices[t * 3 + 2] = static_cast<uint16_t>( vertexOffset + t + 2 );
                    }
                    else
                    {
                        dstIndices[t * 3 + 0] = static_cast<uint16_t>( vertexOffset + t + 1 );
                        dstIndices[t * 3 + 1] = static_cast<uint16_t>( vertexOffset + t );
                        dstIndices[t * 3 + 2] = static_cast<uint16_t>( vertexOffset + t + 2 );
                    }
                }
                indexOffset += triCount * 3;
            }

            vertexOffset += vCount;
        }
        else
        {
            // Plain triangles: just copy vertices
            U32 vCount = cmd.count;
            memcpy( tvb.data + vertexOffset * vertexSize,
                    srcVertices + cmd.offset, vCount * vertexSize );
            vertexOffset += vCount;
        }
    }

    if( vertexOffset == 0 ) return 0;

    // Resolve GPU resources from the first command
    if( !first.program || !CPUResource::IsAlive( first.program ) ) return 0;
    BgfxProgram* prog = static_cast<BgfxProgram*>( first.program->GetGPUResource() );
    if( !prog ) return 0;

    prog->Bind( first.programVersion );
    bgfx::ProgramHandle programHandle = prog->GetHandle( first.programVersion );
    if( !bgfx::isValid( programHandle ) ) return 0;

    // Apply uniforms from first command (same for all batchable draws)
    for( U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i )
    {
        if( first.uniforms[i].valid )
        {
            prog->SetUniform( static_cast<Uniform::Name>( i ), first.uniforms[i].data );
        }
    }

    SetTexFlagsUniform( prog, first );

    // For strip batches, submit as indexed triangles (clear TRISTRIP flag)
    uint64_t submitState = first.bgfxState;
    if( hasStrip )
    {
        submitState &= ~BGFX_STATE_PT_TRISTRIP;
    }
    bgfx::setState( submitState );

    if( first.scissorEnabled )
    {
        bgfx::setScissor( first.scissorX, first.scissorY, first.scissorW, first.scissorH );
    }

    bgfx::setVertexBuffer( 0, &tvb, 0, vertexOffset );
    if( needsIndexBuffer )
    {
        bgfx::setIndexBuffer( &tib, 0, indexOffset );
    }

    // Set textures
    for( U32 i = 0; i < kMaxTextureUnits; ++i )
    {
        if( first.textures[i] && CPUResource::IsAlive( first.textures[i] ) )
        {
            BgfxTexture* tex = static_cast<BgfxTexture*>( first.textures[i]->GetGPUResource() );
            if( tex )
            {
                bgfx::TextureHandle texHandle = tex->GetHandle();
                if( bgfx::isValid( texHandle ) )
                {
                    bgfx::UniformHandle sampler = prog->GetSamplerHandle( i );
                    if( bgfx::isValid( sampler ) )
                    {
                        bgfx::setTexture( i, sampler, texHandle, tex->GetSamplerFlags() );
                    }
                }
            }
        }
    }

    bgfx::submit( fCurrentView, programHandle );

    // Update stats
    if( batchSize > sBatchStats.maxBatchSize )
    {
        sBatchStats.maxBatchSize = static_cast<U32>( batchSize );
    }
    sBatchStats.batchCount++;

    return batchSize;
}

Real
BgfxCommandBuffer::Execute( bool measureGPU )
{
    Rtt_UNUSED( measureGPU );
#ifdef TRACY_ENABLE
    ZoneScopedN("Execute");
#endif

    // Reset view to default before replaying commands
    fCurrentView = fDefaultView;

    // CRITICAL: Reset framebuffer bindings for every FBO view ever used.
    // bgfx::setViewFrameBuffer is persistent across frames. Stale bindings
    // from previous scenes cause rendering failures after scene transitions.
    // fMaxFboViewId is monotonic, so scenes using fewer FBOs still clear
    // bindings left by scenes that used more.
    // Vulkan: bisect shows high-water-mark reset causes regression on Adreno 730; use full reset
    // GLES: high-water-mark is safe and faster
    {
#ifdef TRACY_ENABLE
        ZoneScopedN("ViewReset");
#endif
        if( bgfx::getCaps()->rendererType == bgfx::RendererType::Vulkan )
        {
            bgfx::ViewId numViews = (bgfx::ViewId)bgfx::getCaps()->limits.maxViews;
            for( bgfx::ViewId v = 0; v < numViews; ++v )
                bgfx::setViewFrameBuffer( v, BGFX_INVALID_HANDLE );
        }
        else
        {
            for( uint32_t v = 0; v <= fMaxFboViewId; ++v )
                bgfx::setViewFrameBuffer( (bgfx::ViewId)v, BGFX_INVALID_HANDLE );
        }
    }

    // bgfx::reset() invalidates view state and must not happen after offscreen
    // views have already submitted work in this replay. Pre-scan the deferred
    // stream for the first default-view viewport and perform any required reset
    // before replaying FBO draws.
    bool scanDefaultView = true;
    for( const DeferredCmd& cmd : fDeferredCmds )
    {
        if( cmd.type == DeferredCmd::kBindFBO )
        {
            scanDefaultView = !( cmd.fbo && CPUResource::IsAlive( cmd.fbo ) );
        }
        else if( cmd.type == DeferredCmd::kSetViewport && scanDefaultView )
        {
            if( cmd.vpW != sLastBackbufferWidth || cmd.vpH != sLastBackbufferHeight || sPlatformDataChanged )
            {
                bgfx::reset( cmd.vpW, cmd.vpH, sResetFlags );
                ++Renderer::sBgfxContextGeneration;
                sLastBackbufferWidth = cmd.vpW;
                sLastBackbufferHeight = cmd.vpH;
                sPlatformDataChanged = false;

                const uint32_t numViews = bgfx::getCaps()->limits.maxViews;
                for( uint32_t v = 0; v < numViews; ++v )
                {
                    bgfx::setViewMode( (bgfx::ViewId)v, bgfx::ViewMode::Sequential );
                }
                bgfx::setViewClear( fDefaultView,
                    BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                    0x00000000,
                    fClearDepth,
                    fClearStencil );
            }
            break;
        }
    }

    // Reset batch stats for this frame
    sBatchStats.totalDrawCmds = 0;
    sBatchStats.actualSubmits = 0;
    sBatchStats.batchCount = 0;
    sBatchStats.maxBatchSize = 0;
    sBatchStats.drawCount = 0;
    sBatchStats.drawIndexedCount = 0;

    // Check env var for batching toggle (first frame only)
    static bool sCheckedEnv = false;
    if( !sCheckedEnv )
    {
        const char* env = getenv( "SOLAR2D_BATCH" );
        if( env && strcmp( env, "0" ) == 0 )
        {
            sBatchingEnabled = false;
            Rtt_LogException( "Draw call batching DISABLED via SOLAR2D_BATCH=0\n" );
        }
        else
        {
            Rtt_LogException( "Draw call batching ENABLED\n" );
        }
        sCheckedEnv = true;
    }

    // Check env var for strip→indexed-tri conversion (first frame only)
    // Default is ON; set MASK_PV_STRIP_INDEX=0 to disable.
    if( !sCheckedStripEnv )
    {
        const char* env = getenv( "MASK_PV_STRIP_INDEX" );
        if( env && strcmp( env, "0" ) == 0 )
        {
            sStripIndexConvert = false;
        }
        sCheckedStripEnv = true;
    }

    // Replay all deferred commands - GPU resources are now available (Swap has run)
    {
#ifdef TRACY_ENABLE
    ZoneScopedN("DeferredReplay");
#endif
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
            case DeferredCmd::kDrawIndexed:
            {
                if( fSkipCurrentFbo )
                {
                    break;
                }

                sBatchStats.totalDrawCmds++;
                if( cmd.type == DeferredCmd::kDraw )
                    sBatchStats.drawCount++;
                else
                    sBatchStats.drawIndexedCount++;

                // Try to batch consecutive compatible draws
                size_t batched = ExecuteBatchedDraws( i );
                if( batched > 0 )
                {
                    // Count remaining draws in the batch
                    for( size_t j = i + 1; j < i + batched; ++j )
                    {
                        if( fDeferredCmds[j].type == DeferredCmd::kDraw ||
                            fDeferredCmds[j].type == DeferredCmd::kDrawIndexed )
                        {
                            sBatchStats.totalDrawCmds++;
                        }
                    }
                    sBatchStats.actualSubmits++;
                    i += batched - 1; // Skip batched commands (loop will ++i)
                }
                else
                {
                    // Execute unbatched
                    if( cmd.type == DeferredCmd::kDraw )
                        ExecuteDraw( cmd );
                    else
                        ExecuteDrawIndexed( cmd );
                    sBatchStats.actualSubmits++;
                }
                break;
            }
            case DeferredCmd::kCaptureRect:
                ExecuteCaptureRect( cmd );
                break;
            case DeferredCmd::kDrawSDF:
                ExecuteDrawSDF( cmd );
                sBatchStats.totalDrawCmds++;
                sBatchStats.actualSubmits++;
                break;
        }
    }
    } // DeferredReplay

    // Log batch stats periodically (every 300 frames ~ 5 seconds at 60fps)
    if( sFrameNum % 300 == 0 && sBatchStats.totalDrawCmds > 0 )
    {
        Rtt_LogException( "Batch stats: %u draws -> %u submits (%u batches, max %u) [draw:%u indexed:%u]\n",
            sBatchStats.totalDrawCmds, sBatchStats.actualSubmits,
            sBatchStats.batchCount, sBatchStats.maxBatchSize,
            sBatchStats.drawCount, sBatchStats.drawIndexedCount );
    }


    sFrameNum++;

    // Advance static geometry cache frame counter for auto-promotion
    // BgfxGeometry::AdvanceFrame(); // TODO: re-enable when static geometry caching is merged

    // Scene-transition flash fix (Metal only; Vulkan has swapchain hazards with skipPresent).
    // Triggers:
    //   1. Draw-count drop: near-zero dip, large (>20) sudden drop,
    //      medium drop (>6 after 30+ stable frames, else >8),
    //      gradual multi-step drop (2+ consecutive falling frames, cumulative >=8),
    //      OR sustained below-baseline state (>12 below recent stable baseline).
    //   2. De-occlusion: window was occluded (drawable=0x0) and now has a real drawable.
#if !defined( Rtt_ANDROID_ENV )
    {
        static int      sTransitionHold  = 0;
        static uint32_t sPrevDrawCmds    = 0;
        static bool     sPrevOccluded    = false;
        static int      sConsecDrops     = 0;   // consecutive frames where cur < prev
        static uint32_t sCumDrop         = 0;   // cumulative drop across consecutive falling frames
        static uint32_t sStableFrames    = 0;   // consecutive frames with identical draw count
        static uint32_t sBaseline        = 0;   // recently stable draw-count baseline
        static int      sNewStable       = 0;   // frames stable at new below-baseline level
        static bool     sWasLongStable   = false; // recently exited a 15+ frame stable period
        static int      sLongStableTimer = 0;     // countdown: keep sWasLongStable true for N frames
        uint32_t cur  = sBatchStats.totalDrawCmds;
        uint32_t drop = (sPrevDrawCmds > cur) ? (sPrevDrawCmds - cur) : 0;
        bool     curOccluded = bgfx::wasLastFrameOccluded();

        // Consecutive-drop tracking
        if ( cur < sPrevDrawCmds ) { sConsecDrops++; sCumDrop += drop; }
        else                       { sConsecDrops = 0; sCumDrop = 0; }

        // Stability + baseline tracking
        // prevStable: stable count from BEFORE this frame's draw-count change
        uint32_t prevStable = sStableFrames;
        if ( cur == sPrevDrawCmds ) sStableFrames++;
        else                        sStableFrames = 0;
        if ( cur > sBaseline ) sBaseline = cur;                         // track peak upward immediately
        if ( sStableFrames > 60 && cur > 15 ) sBaseline = cur;          // re-anchor after long stable period

        uint32_t dropFromBase = ( sBaseline > cur && sBaseline > 15 ) ? (sBaseline - cur) : 0;

        // Remember "was recently stable for 15+ frames" up to 15 frames after stability breaks.
        // This keeps threshold=3 through leading small drops that reset prevStable.
        if ( prevStable > 15 ) { sWasLongStable = true;  sLongStableTimer = 15; }
        else if ( sLongStableTimer > 0 ) { sLongStableTimer--; }
        else                             { sWasLongStable = false; }
        uint32_t dropThreshold = sWasLongStable ? 3u : 8u;

        // Track truly stable frames at new below-baseline level.
        // Must use cur==prev (not just drop==0) so rising frames during load don't count.
        if ( cur == sPrevDrawCmds && dropFromBase >= 6 ) sNewStable++;
        else                                              sNewStable = 0;

        // Triggers
        if ( cur <= 5 && sPrevDrawCmds > 10 )
        {
            sTransitionHold = 5;    // near-zero dip: old scene removed, new not ready
        }
        else if ( drop > 20 && sTransitionHold < 5 )
        {
            sTransitionHold = 5;    // large sudden drop
        }
        else if ( drop >= dropThreshold && sTransitionHold < 5 )
        {
            sTransitionHold = 5;    // medium drop (threshold adapts after stable period)
        }
        else if ( sConsecDrops >= 2 && sCumDrop >= 8 && sTransitionHold < 5 )
        {
            sTransitionHold = 5;    // gradual multi-step teardown
        }
        else if ( dropFromBase >= 6 && sNewStable < 10 && sTransitionHold == 0 )
        {
            sTransitionHold = 3;    // below baseline, not yet settled at new level
        }
        else if ( sPrevOccluded && !curOccluded && sTransitionHold < 5 )
        {
            sTransitionHold = 5;    // de-occlusion: native panel closed, window visible again
        }

        // Instead of skipping frames (setSkipPresent), use Metal retained-backing:
        // clear without BGFX_CLEAR_COLOR → bgfx sets LoadActionLoad on the Metal render pass
        // → previous frame pixels are the base instead of opaque black.
        // This is equivalent to GL double-buffer retained backing and eliminates the black flash
        // even when detection fires one frame late. False positives show a brief ghost (imperceptible).
        bool protectingThisFrame = false;
        if ( sTransitionHold > 0 )
        {
            if ( sPrevDrawCmds <= 1 && cur > 20 )
            {
                sTransitionHold = 0;   // true zero-draw recovery: release immediately
            }
            else
            {
                --sTransitionHold;
                protectingThisFrame = true;
            }
        }

        // Override view clear for this frame before bgfx::frame() processes it.
        bgfx::setViewClear( fDefaultView,
            protectingThisFrame
                ? (BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL)
                : (BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL),
            0x00000000, fClearDepth, fClearStencil );

        sPrevDrawCmds = cur;
        sPrevOccluded = curOccluded;
    }
#endif

    // Ensure screen view is submitted even if no draw commands targeted it
    bgfx::touch( fDefaultView );
#ifdef TRACY_ENABLE
    TracyPlot("DrawCmds",  (int64_t)sBatchStats.totalDrawCmds);
    TracyPlot("Submits",   (int64_t)sBatchStats.actualSubmits);
    TracyPlot("Batches",   (int64_t)sBatchStats.batchCount);
    {
        ZoneScopedN("bgfx::frame");
#endif
    bgfx::frame();
#ifdef TRACY_ENABLE
    }
    FrameMark;
#endif

    // Clear deferred commands and side tables for next frame
    fDeferredCmds.clear();
    fNamedUniformSideTable.clear();

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
BgfxCommandBuffer::ExecuteDrawSDF( const DeferredCmd& cmd )
{
    if ( fSkipCurrentFbo ) return;

    SDFRenderer& sdf = SDFRenderer::Instance();
    if ( !sdf.IsAvailable() ) return;

    bgfx::ProgramHandle program = sdf.GetProgram(
        static_cast<SDFRenderer::ShapeType>( cmd.sdfShapeType ) );
    if ( !bgfx::isValid( program ) ) return;

    const bgfx::VertexLayout& layout = BgfxGeometry::GetVertexLayout();

    if ( bgfx::getAvailTransientVertexBuffer( 4, layout ) < 4 ) return;
    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer( &tvb, 4, layout );

    // Expand packed (x,y,z,u,v) to full Geometry::Vertex layout
    Geometry::Vertex* dst = reinterpret_cast<Geometry::Vertex*>( tvb.data );
    memset( dst, 0, 4 * sizeof( Geometry::Vertex ) );
    for ( int i = 0; i < 4; ++i )
    {
        dst[i].x  = cmd.sdfVerts[i][0];
        dst[i].y  = cmd.sdfVerts[i][1];
        dst[i].z  = cmd.sdfVerts[i][2];
        dst[i].u  = cmd.sdfVerts[i][3];
        dst[i].v  = cmd.sdfVerts[i][4];
        dst[i].rs = dst[i].gs = dst[i].bs = dst[i].as = 255;
    }

    if ( bgfx::getAvailTransientIndexBuffer( 6 ) < 6 ) return;
    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer( &tib, 6 );
    uint16_t* idx = reinterpret_cast<uint16_t*>( tib.data );
    idx[0] = 0; idx[1] = 1; idx[2] = 2;
    idx[3] = 0; idx[4] = 2; idx[5] = 3;

    // Pass VP matrix as the "model" matrix so u_modelViewProj = VP (bgfx view/proj stay identity)
    if ( cmd.uniforms[Uniform::kViewProjectionMatrix].valid )
    {
        bgfx::setTransform( cmd.uniforms[Uniform::kViewProjectionMatrix].data );
    }

    sdf.SetShapeUniforms(
        static_cast<SDFRenderer::ShapeType>( cmd.sdfShapeType ),
        cmd.sdfParams[0], cmd.sdfParams[1],
        cmd.sdfParams[2], cmd.sdfParams[3] );

    if ( cmd.sdfShapeType == (S32)SDFRenderer::kPolygon && cmd.sdfNumPolyVerts >= 3 )
    {
        int cnt = cmd.sdfNumPolyVerts;
        if ( cnt > SDFRenderer::kMaxPolygonVerts ) cnt = SDFRenderer::kMaxPolygonVerts;
        Real polyVerts[ SDFRenderer::kMaxPolygonVerts * 2 ];
        for ( int i = 0; i < cnt * 2; ++i )
            polyVerts[i] = (Real)cmd.sdfPolyVerts[i];
        sdf.SetPolygonUniforms( polyVerts, cnt );
    }

    sdf.SetColorUniforms(
        cmd.sdfFillColor[0],   cmd.sdfFillColor[1],
        cmd.sdfFillColor[2],   cmd.sdfFillColor[3],
        cmd.sdfStrokeColor[0], cmd.sdfStrokeColor[1],
        cmd.sdfStrokeColor[2], cmd.sdfStrokeColor[3] );

    bgfx::setState( cmd.bgfxState );
    if ( cmd.scissorEnabled )
        bgfx::setScissor( cmd.scissorX, cmd.scissorY, cmd.scissorW, cmd.scissorH );

    bgfx::setVertexBuffer( 0, &tvb );
    bgfx::setIndexBuffer( &tib );
    bgfx::submit( fCurrentView, program );
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

void
BgfxCommandBuffer::DrawSDF( const SDFIssueData & data )
{
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    if ( fBlendEnabled )   state |= fBlendState;
    if ( fBlendEquation )  state |= fBlendEquation;
    if ( fMsaaEnabled )    state |= BGFX_STATE_MSAA;

    DeferredCmd cmd = {};
    cmd.type             = DeferredCmd::kDrawSDF;
    cmd.bgfxState        = state;
    cmd.scissorEnabled   = fScissorEnabled;
    cmd.scissorX         = fScissorRect.x;
    cmd.scissorY         = fScissorRect.y;
    cmd.scissorW         = fScissorRect.w;
    cmd.scissorH         = fScissorRect.h;
    cmd.namedUniformCount  = 0;
    cmd.namedUniformOffset = -1;

    memcpy( cmd.sdfVerts,       data.verts,       sizeof( cmd.sdfVerts ) );
    memcpy( cmd.sdfParams,      data.params,      sizeof( cmd.sdfParams ) );
    memcpy( cmd.sdfFillColor,   data.fillColor,   sizeof( cmd.sdfFillColor ) );
    memcpy( cmd.sdfStrokeColor, data.strokeColor, sizeof( cmd.sdfStrokeColor ) );
    cmd.sdfShapeType = data.shapeType;
    memcpy( cmd.sdfPolyVerts, data.polyVerts, sizeof( cmd.sdfPolyVerts ) );
    cmd.sdfNumPolyVerts = data.numPolyVerts;

    SnapshotUniforms( cmd );
    fDeferredCmds.push_back( cmd );
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
    if( sNextViewId <= 255 )
    {
        return sNextViewId++;
    }

    Rtt_LogException( "bgfx: Out of view IDs!\n" );
    return 255;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------


#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
