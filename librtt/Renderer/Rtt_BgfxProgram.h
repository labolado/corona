//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxProgram_H__
#define _Rtt_BgfxProgram_H__

#include "Renderer/Rtt_GPUResource.h"
#include "Renderer/Rtt_Program.h"
#include "Renderer/Rtt_Uniform.h"
#include "Core/Rtt_Assert.h"
#include <bgfx/bgfx.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

class BgfxProgram : public GPUResource
{
public:
    typedef GPUResource Super;
    typedef BgfxProgram Self;

public:
    BgfxProgram();
    virtual ~BgfxProgram();

    virtual void Create(CPUResource* resource);
    virtual void Update(CPUResource* resource);
    virtual void Destroy();
    virtual void Bind(Program::Version version);

    // Get program handle for specific version
    bgfx::ProgramHandle GetHandle(Program::Version version) const;

    // Get uniform handle for built-in uniform
    bgfx::UniformHandle GetUniformHandle(Uniform::Name name) const;
    
    // Get sampler uniform handle
    bgfx::UniformHandle GetSamplerHandle(U32 unit) const;

    // Get texture flags uniform handle (for alpha texture swizzle)
    bgfx::UniformHandle GetTexFlagsHandle() const { return fUniformTexFlags; }

    // 008 mask per-vertex: array size shared with vs_default.sc's
    // u_MaskMatricesArr[N] declaration. Must stay in sync with shader.
    static const U16 kMaskMatricesArrSize = 16;

    // Getter for mat3-array uniform handles. Only meaningful on the default
    // shader program; filter VSes don't reference these arrays so setUniform
    // writes are silently ignored. level: 0..2 (mask depth slot).
    bgfx::UniformHandle GetMaskMatricesArrHandle(U32 level) const
    {
        switch (level)
        {
            case 0: return fUniformMaskMatricesArr0;
            case 1: return fUniformMaskMatricesArr1;
            case 2: return fUniformMaskMatricesArr2;
            default: return BGFX_INVALID_HANDLE;
        }
    }

    // Set uniform value (data points to appropriate type based on uniform)
    void SetUniform(Uniform::Name name, const void* data);

    // Check if program is valid for given version
    bool IsValid(Program::Version version) const;

    // Access to default shader binaries (for use by InstancedBatchRenderer)
    static const unsigned char* GetDefaultFSData();
    static unsigned int GetDefaultFSSize();
    static const unsigned char* GetDefaultVSData();
    static unsigned int GetDefaultVSSize();

private:
    // Per-version data (5 versions: mask0-3 + wireframe)
    struct VersionData
    {
        bgfx::ProgramHandle fProgram;
        bgfx::ShaderHandle fVertexShader;
        bgfx::ShaderHandle fFragmentShader;
        
        // Timestamps for uniform updates
        U32 fTimestamps[Uniform::kNumBuiltInVariables];
        
        // Metadata
        int fHeaderNumLines;
        bool fAttemptedCreation;
        
        VersionData();
        void Reset();
        bool IsValid() const;
    };

    void CreateVersion(Program::Version version, VersionData& data);
    void UpdateVersion(Program::Version version, VersionData& data);
    void ResetVersion(VersionData& data);
    
    // Load precompiled shader binary
    bool LoadShaderBinary(Program::Version version, const char* type, const bgfx::Memory*& outMem);

    // Create all global uniforms (called once)
    void CreateUniforms();
    void DestroyUniforms();

    // (008 mask per-vertex public getters moved above to public section.)

private:
    VersionData fData[Program::kNumVersions];
    CPUResource* fResource;
    
    // Global uniform handles (created once, shared across all versions)
    // Built-in uniforms
    bgfx::UniformHandle fUniformViewProjectionMatrix;
    bgfx::UniformHandle fUniformMaskMatrix0;
    bgfx::UniformHandle fUniformMaskMatrix1;
    bgfx::UniformHandle fUniformMaskMatrix2;
    // 008 mask per-vertex encoding: mat3 array uniforms used by default VS only.
    // Filter VSes still use the single-mat3 fUniformMaskMatrix0/1/2 above.
    // bgfx createUniform returns valid handles regardless of shader contents,
    // so unused handles set values that are simply discarded at draw.
    bgfx::UniformHandle fUniformMaskMatricesArr0;
    bgfx::UniformHandle fUniformMaskMatricesArr1;
    bgfx::UniformHandle fUniformMaskMatricesArr2;
    bgfx::UniformHandle fUniformTotalTime;      // vec4, time in .x
    bgfx::UniformHandle fUniformDeltaTime;      // vec4, delta in .x
    bgfx::UniformHandle fUniformTexelSize;
    bgfx::UniformHandle fUniformContentScale;   // vec4, scale in .xy
    bgfx::UniformHandle fUniformContentSize;    // vec4
    bgfx::UniformHandle fUniformUserData0;
    bgfx::UniformHandle fUniformUserData1;
    bgfx::UniformHandle fUniformUserData2;
    bgfx::UniformHandle fUniformUserData3;
    
    // Texture flags uniform (alpha texture swizzle, etc.)
    bgfx::UniformHandle fUniformTexFlags;

    // Sampler uniforms (5)
    bgfx::UniformHandle fSamplerFill0;
    bgfx::UniformHandle fSamplerFill1;
    bgfx::UniformHandle fSamplerMask0;
    bgfx::UniformHandle fSamplerMask1;
    bgfx::UniformHandle fSamplerMask2;
    
    bool fUniformsCreated;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxProgram_H__
