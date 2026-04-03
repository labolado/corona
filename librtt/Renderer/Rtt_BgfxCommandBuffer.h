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
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

class BgfxCommandBuffer : public CommandBuffer
{
public:
    typedef CommandBuffer Super;
    typedef BgfxCommandBuffer Self;

public:
    BgfxCommandBuffer(Rtt_Allocator* allocator);
    virtual ~BgfxCommandBuffer();

    virtual void Initialize();
    virtual void Denitialize();
    virtual void ClearUserUniforms();

    virtual void BindFrameBufferObject(FrameBufferObject* fbo, bool asDrawBuffer = false);
    virtual void CaptureRect(FrameBufferObject* fbo, Texture& texture, const Rect& rect, const Rect& rawRect);
    virtual void BindGeometry(Geometry* geometry);
    virtual void BindTexture(Texture* texture, U32 unit);
    virtual void BindUniform(Uniform* uniform, U32 unit);
    virtual void BindProgram(Program* program, Program::Version version);
    virtual void BindInstancing(U32 count, Geometry::Vertex* instanceData);
    virtual void BindVertexFormat(FormatExtensionList* extensionList, U16 fullCount, U16 vertexSize, U32 offset);
    virtual void SetBlendEnabled(bool enabled);
    virtual void SetBlendFunction(const BlendMode& mode);
    virtual void SetBlendEquation(RenderTypes::BlendEquation equation);
    virtual void SetViewport(int x, int y, int width, int height);
    virtual void SetScissorEnabled(bool enabled);
    virtual void SetScissorRegion(int x, int y, int width, int height);
    virtual void SetMultisampleEnabled(bool enabled);
    virtual void ClearDepth(Real depth);
    virtual void ClearStencil(U32 stencil);
    virtual void Clear(Real r, Real g, Real b, Real a);
    virtual void Draw(U32 offset, U32 count, Geometry::PrimitiveType type);
    virtual void DrawIndexed(U32 offset, U32 count, Geometry::PrimitiveType type);
    virtual S32 GetCachedParam(CommandBuffer::QueryableParams param);

    virtual void AddCommand(const CoronaCommand* command);
    virtual void IssueCommand(U16 id, const void* data, U32 size);

    virtual const unsigned char* GetBaseAddress() const;

    virtual bool WriteNamedUniform(const char* uniformName, const void* data, unsigned int size);

    virtual Real Execute(bool measureGPU);

    virtual bool HasFramebufferBlit(bool* canScale) const;
    virtual void GetVertexAttributes(VertexAttributeSupport& support) const;

private:
    virtual void InitializeFBO();
    virtual void InitializeCachedParams();
    virtual void CacheQueryParam(CommandBuffer::QueryableParams param);

private:
    // bgfx view ID for this command buffer
    bgfx::ViewId fViewId;

    // Cached parameters
    S32 fCachedParams[kNumQueryableParams];

    // Current state (recorded between Bind* calls and Draw)
    class BgfxGeometry* fCurrentGeometry;
    class BgfxProgram* fCurrentProgram;
    class BgfxTexture* fBoundTextures[8]; // Max texture units
    Program::Version fCurrentVersion;

    // State flags
    bool fBlendEnabled;
    bool fScissorEnabled;
    bool fMultisampleEnabled;

    // Blend state
    uint64_t fBlendState;
    uint64_t fBlendEquation;

    // Viewport and scissor
    int fViewport[4];
    int fScissorRect[4];

    // Clear values
    float fClearDepth;
    U32 fClearStencil;

    // Sampler uniforms (cached)
    static bgfx::UniformHandle sSamplerUniforms[8];
    static bool sSamplersInitialized;

    void InitializeSamplers();
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxCommandBuffer_H__
