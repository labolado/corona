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

// ----------------------------------------------------------------------------

struct CoronaCommand;

namespace Rtt
{

// ----------------------------------------------------------------------------

class BgfxGeometry;
class BgfxProgram;
class BgfxTexture;
class BgfxFrameBufferObject;

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

        // Generate the appropriate bgfx commands to accomplish the
        // specified state changes.
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

        // Execute all buffered commands.
        virtual Real Execute( bool measureGPU );
    
    private:
        virtual void InitializeFBO();
        virtual void InitializeCachedParams();
        virtual void CacheQueryParam( CommandBuffer::QueryableParams param );

    private:
        // State management helpers
        void ApplyUniforms();
        uint64_t ToBgfxBlendState( const BlendMode& mode ) const;
        uint64_t ToBgfxBlendFactor( BlendMode::Param param ) const;
        uint64_t ToBgfxPrimitiveType( Geometry::PrimitiveType type ) const;

    private:
        static const U32 kMaxTextureUnits = 8;

        // Current state
        bgfx::ViewId fCurrentView;
        BgfxGeometry* fCurrentGeometry;
        BgfxProgram* fCurrentProgram;
        Program::Version fCurrentVersion;
        BgfxTexture* fBoundTextures[kMaxTextureUnits];

        // Blend state
        bool fBlendEnabled;
        uint64_t fBlendState;           // BGFX_STATE_BLEND_* combination
        uint64_t fBlendEquation;        // BGFX_STATE_BLEND_EQUATION_* 

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

        // Cached params
        S32 fCachedQuery[kNumQueryableParams];

        // Custom commands
        LightPtrArray< const CoronaCommand > fCustomCommands;

        // Uniform updates
        struct UniformUpdate
        {
            Uniform* uniform;
            U32 timestamp;
        };
        UniformUpdate fUniformUpdates[Uniform::kNumBuiltInVariables];
        static U32 gUniformTimestamp;

        // Instance data
        U32 fInstanceCount;
        Geometry::Vertex* fInstanceData;

        // View ID allocator for FBOs
        static bgfx::ViewId sNextViewId;
        static bgfx::ViewId AllocateViewId();
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxCommandBuffer_H__
