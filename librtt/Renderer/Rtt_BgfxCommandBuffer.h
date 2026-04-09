//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxCommandBuffer_H__
#define _Rtt_BgfxCommandBuffer_H__

#include "Renderer/Rtt_CommandBuffer.h"
#include "Renderer/Rtt_Uniform.h"
#include "Renderer/Rtt_RenderTypes.h"
#include <bgfx/bgfx.h>
#include <vector>

// ----------------------------------------------------------------------------

struct CoronaCommand;

namespace Rtt
{

// ----------------------------------------------------------------------------

class BgfxGeometry;
class BgfxProgram;
class BgfxTexture;
class BgfxFrameBufferObject;
class Geometry;
class Texture;
class Program;
class FrameBufferObject;

// ----------------------------------------------------------------------------

// Deferred command: captures all state needed for replay in Execute()
struct DeferredCmd
{
    enum Type { kBindFBO, kSetViewport, kClear, kDraw, kDrawIndexed, kCaptureRect };
    Type type;

    // kBindFBO
    FrameBufferObject* fbo;

    // kSetViewport
    uint16_t vpX, vpY, vpW, vpH;

    // kClear
    float clearR, clearG, clearB, clearA;
    float clearDepth;
    uint8_t clearStencil;

    // kDraw / kDrawIndexed
    Geometry* geometry;
    Texture* textures[8];
    Program* program;
    Program::Version programVersion;
    U32 offset, count;
    Geometry::PrimitiveType primitiveType;
    uint64_t bgfxState;
    bool scissorEnabled;
    int scissorX, scissorY, scissorW, scissorH;

    // kCaptureRect
    FrameBufferObject* captureFbo;
    Texture* captureTexture;
    float captureRectXMin, captureRectYMin, captureRectXMax, captureRectYMax;
    float captureRawXMin, captureRawYMin, captureRawXMax, captureRawYMax;
    U32 captureTexW, captureTexH;

    // Uniform snapshots (copied values, not pointers)
    struct UniformSnapshot
    {
        bool valid;
        U32 size;
        U8 data[64]; // Max: Mat4 = 16 floats = 64 bytes
    };
    UniformSnapshot uniforms[Uniform::kNumBuiltInVariables];

    // Named uniform snapshots (for custom effects via WriteNamedUniform)
    struct NamedUniformSnapshot
    {
        char name[64];
        U8 data[64]; // Max: Mat4 = 16 floats = 64 bytes
        unsigned int size; // byte size of data
    };
    static const int kMaxNamedUniforms = 16;
    int namedUniformCount;
    NamedUniformSnapshot namedUniforms[kMaxNamedUniforms];

    // Instance draw data (opaque, cast to InstanceDrawData* by bgfx backend)
    void* instanceDraw;

    DeferredCmd() : type(kDraw), fbo(NULL), vpX(0), vpY(0), vpW(0), vpH(0),
        clearR(0), clearG(0), clearB(0), clearA(0), clearDepth(1.0f), clearStencil(0),
        geometry(NULL), program(NULL), programVersion(Program::kMaskCount0),
        offset(0), count(0), primitiveType(Geometry::kTriangles),
        bgfxState(0), scissorEnabled(false), scissorX(0), scissorY(0), scissorW(0), scissorH(0),
        captureFbo(NULL), captureTexture(NULL),
        captureRectXMin(0), captureRectYMin(0), captureRectXMax(0), captureRectYMax(0),
        captureRawXMin(0), captureRawYMin(0), captureRawXMax(0), captureRawYMax(0),
        captureTexW(0), captureTexH(0),
        namedUniformCount(0),
        instanceDraw(NULL)
    {
        for (int i = 0; i < 8; i++) textures[i] = NULL;
        for (int i = 0; i < Uniform::kNumBuiltInVariables; i++)
        {
            uniforms[i].valid = false;
            uniforms[i].size = 0;
        }
    }
};

// ----------------------------------------------------------------------------

//
class BgfxCommandBuffer : public CommandBuffer
{
    public:
        typedef CommandBuffer Super;
        typedef BgfxCommandBuffer Self;

        bool HasFramebufferBlit( bool * canScale ) const;
        void GetVertexAttributes( VertexAttributeSupport & support ) const;

    public:
        BgfxCommandBuffer( Rtt_Allocator* allocator );
        virtual ~BgfxCommandBuffer();

        virtual void Initialize();
        virtual void Denitialize();

        virtual void ClearUserUniforms();

        // These methods now capture state for deferred execution
        virtual void BindFrameBufferObject( FrameBufferObject* fbo, bool asDrawBuffer );
        virtual void CaptureRect( FrameBufferObject* fbo, Texture& texture, const Rect& rect, const Rect& rawRect );
        virtual void BindGeometry( Geometry* geometry );
        virtual void BindTexture( Texture* texture, U32 unit );
        virtual void BindUniform( Uniform* uniform, U32 unit );
        virtual void BindProgram( Program* program, Program::Version version );
        virtual void BindInstancing( U32 count, Geometry::Vertex* instanceData );
        virtual void BindVertexFormat( FormatExtensionList* list, U16 fullCount, U16 vertexSize, U32 offset );
        virtual void SetBlendEnabled( bool enabled );
        virtual void SetBlendFunction( const BlendMode& mode );
        virtual void SetBlendEquation( RenderTypes::BlendEquation mode );
        virtual void SetViewport( int x, int y, int width, int height );
        virtual void SetScissorEnabled( bool enabled );
        virtual void SetScissorRegion( int x, int y, int width, int height );
        virtual void SetMultisampleEnabled( bool enabled );
        virtual void ClearDepth( Real depth );
        virtual void ClearStencil( U32 stencil );
        virtual void Clear( Real r, Real g, Real b, Real a );
        virtual void Draw( U32 offset, U32 count, Geometry::PrimitiveType type );
        virtual void DrawIndexed( U32 offset, U32 count, Geometry::PrimitiveType type );
        virtual S32 GetCachedParam( CommandBuffer::QueryableParams param );

        virtual void AddCommand( const CoronaCommand * command );
        virtual void IssueCommand( U16 id, const void * data, U32 size );

        virtual const unsigned char * GetBaseAddress() const { return NULL; }

        virtual bool WriteNamedUniform( const char * uniformName, const void * data, unsigned int size );
        virtual void SetPendingInstanceDraw( void* data );

        // Set the screen capture texture handle (for CaptureRect source)
        void SetScreenCaptureTexture( bgfx::TextureHandle handle ) { fScreenCaptureTexture = handle; }

        // Execute all deferred commands - called after Swap creates GPU resources
        virtual Real Execute( bool measureGPU );

    private:
        virtual void InitializeFBO();
        virtual void InitializeCachedParams();
        virtual void CacheQueryParam( CommandBuffer::QueryableParams param );

    private:
        // Build bgfx blend state
        uint64_t ToBgfxBlendState( const BlendMode& mode ) const;
        uint64_t ToBgfxBlendFactor( BlendMode::Param param ) const;
        uint64_t ToBgfxPrimitiveType( Geometry::PrimitiveType type ) const;

        // Snapshot current uniform state into a DeferredCmd
        void SnapshotUniforms( DeferredCmd& cmd );

        // Execute helpers
        void ExecuteBindFBO( const DeferredCmd& cmd );
        void ExecuteSetViewport( const DeferredCmd& cmd );
        void ExecuteClear( const DeferredCmd& cmd );
        void ExecuteDraw( const DeferredCmd& cmd );
        void ExecuteDrawIndexed( const DeferredCmd& cmd );
        void ExecuteCaptureRect( const DeferredCmd& cmd );
        void SetTexFlagsUniform( BgfxProgram* prog, const DeferredCmd& cmd );
        void ApplyNamedUniforms( const DeferredCmd& cmd );

        // Draw call batching
        bool CanBatchDraws( const DeferredCmd& a, const DeferredCmd& b ) const;
        size_t ExecuteBatchedDraws( size_t startIdx );

    public:
        // Batch statistics (reset each frame)
        struct BatchStats
        {
            U32 totalDrawCmds;    // Draw commands before batching
            U32 actualSubmits;    // Actual bgfx::submit calls after batching
            U32 batchCount;       // Number of batches formed
            U32 maxBatchSize;     // Largest batch in this frame
            U32 drawCount;        // kDraw commands this frame
            U32 drawIndexedCount; // kDrawIndexed commands this frame
        };
        static BatchStats sBatchStats;
        static bool sBatchingEnabled;

        // Call this after bgfx::setPlatformData() to force bgfx::reset() on next SetViewport
        // (needed after lock-screen on Android to recreate the EGL surface)
        static void NotifyPlatformDataChanged();

    private:
        static const U32 kMaxTextureUnits = 8;

        // Current state (CPU resource pointers - resolved to GPU in Execute)
        Geometry* fCurrentGeometry;
        Program* fCurrentProgram;
        Program::Version fCurrentVersion;
        Texture* fBoundTextures[kMaxTextureUnits];

        // Blend state
        bool fBlendEnabled;
        uint64_t fBlendState;
        uint64_t fBlendEquation;

        // Scissor state
        bool fScissorEnabled;
        struct { int x, y, w, h; } fScissorRect;

        // MSAA state
        bool fMsaaEnabled;

        // Clear values
        float fClearDepth;
        uint8_t fClearStencil;

        // Default FBO view
        bgfx::ViewId fDefaultView;
        bgfx::ViewId fCurrentView;

        // Cached params
        S32 fCachedQuery[kNumQueryableParams];

        // Custom commands
        LightPtrArray< const CoronaCommand > fCustomCommands;

        // Uniform tracking (store CPU-side uniform pointers)
        struct UniformUpdate
        {
            Uniform* uniform;
            U32 timestamp;
        };
        UniformUpdate fUniformUpdates[Uniform::kNumBuiltInVariables];
        static U32 gUniformTimestamp;

        // Instance data (legacy)
        U32 fInstanceCount;
        Geometry::Vertex* fInstanceData;

        // Pending instance draw (for GPU instancing via BatchObject)
        void* fPendingInstanceDraw;

        // Screen capture texture - set by BgfxRenderer when scene is rendered to an
        // offscreen FBO instead of the backbuffer, enabling CaptureRect to blit from it
        bgfx::TextureHandle fScreenCaptureTexture;

        // Pending named uniforms (set via WriteNamedUniform, snapshotted into DeferredCmd)
        struct PendingNamedUniform
        {
            char name[64];
            U8 data[64];
            unsigned int size;
        };
        std::vector<PendingNamedUniform> fPendingNamedUniforms;

        // Deferred command list
        std::vector<DeferredCmd> fDeferredCmds;

        // View ID allocator for FBOs
        static bgfx::ViewId sNextViewId;
        static bgfx::ViewId AllocateViewId();
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxCommandBuffer_H__
