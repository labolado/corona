//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Renderer/Rtt_BgfxProgram.h"
#include "Renderer/Rtt_BgfxShaderCompiler.h"
#include "Renderer/Rtt_Program.h"
#include "Display/Rtt_ShaderResource.h"
#include "Display/Rtt_ShaderTypes.h"
#include "Core/Rtt_Assert.h"
#include <string.h>
#include <stdio.h>

// Platform-specific precompiled shader data
#if defined(Rtt_ANDROID_ENV)
    #include "Renderer/Rtt_BgfxShaderData_essl.h"
    #include "Renderer/Rtt_BgfxShaderData_effects_essl.h"
    #define S_VS_DEFAULT s_vs_default_essl
    #define S_VS_DEFAULT_SIZE s_vs_default_essl_size
    #define S_FS_DEFAULT s_fs_default_essl
    #define S_FS_DEFAULT_SIZE s_fs_default_essl_size
#else
    // macOS and iOS both use Metal
    #include "Renderer/Rtt_BgfxShaderData_metal.h"
    #include "Renderer/Rtt_BgfxShaderData_effects_metal.h"
    #define S_VS_DEFAULT s_vs_default_metal
    #define S_VS_DEFAULT_SIZE s_vs_default_metal_size
    #define S_FS_DEFAULT s_fs_default_metal
    #define S_FS_DEFAULT_SIZE s_fs_default_metal_size
#endif

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// VersionData implementation
BgfxProgram::VersionData::VersionData()
    : fProgram(BGFX_INVALID_HANDLE)
    , fVertexShader(BGFX_INVALID_HANDLE)
    , fFragmentShader(BGFX_INVALID_HANDLE)
    , fHeaderNumLines(0)
    , fAttemptedCreation(false)
{
    for (U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i)
    {
        fTimestamps[i] = 0;
    }
}

void BgfxProgram::VersionData::Reset()
{
    fProgram = BGFX_INVALID_HANDLE;
    fVertexShader = BGFX_INVALID_HANDLE;
    fFragmentShader = BGFX_INVALID_HANDLE;
    fHeaderNumLines = 0;
    fAttemptedCreation = false;
    
    for (U32 i = 0; i < Uniform::kNumBuiltInVariables; ++i)
    {
        fTimestamps[i] = 0;
    }
}

bool BgfxProgram::VersionData::IsValid() const
{
    return bgfx::isValid(fProgram) && 
           bgfx::isValid(fVertexShader) && 
           bgfx::isValid(fFragmentShader);
}

// ----------------------------------------------------------------------------

BgfxProgram::BgfxProgram()
    : fResource(NULL)
    , fUniformsCreated(false)
{
    // Initialize all uniform handles to invalid
    fUniformViewProjectionMatrix = BGFX_INVALID_HANDLE;
    fUniformMaskMatrix0 = BGFX_INVALID_HANDLE;
    fUniformMaskMatrix1 = BGFX_INVALID_HANDLE;
    fUniformMaskMatrix2 = BGFX_INVALID_HANDLE;
    fUniformTotalTime = BGFX_INVALID_HANDLE;
    fUniformDeltaTime = BGFX_INVALID_HANDLE;
    fUniformTexelSize = BGFX_INVALID_HANDLE;
    fUniformContentScale = BGFX_INVALID_HANDLE;
    fUniformContentSize = BGFX_INVALID_HANDLE;
    fUniformUserData0 = BGFX_INVALID_HANDLE;
    fUniformUserData1 = BGFX_INVALID_HANDLE;
    fUniformUserData2 = BGFX_INVALID_HANDLE;
    fUniformUserData3 = BGFX_INVALID_HANDLE;
    
    fUniformTexFlags = BGFX_INVALID_HANDLE;

    fSamplerFill0 = BGFX_INVALID_HANDLE;
    fSamplerFill1 = BGFX_INVALID_HANDLE;
    fSamplerMask0 = BGFX_INVALID_HANDLE;
    fSamplerMask1 = BGFX_INVALID_HANDLE;
    fSamplerMask2 = BGFX_INVALID_HANDLE;

    // Initialize all version data
    for (U32 i = 0; i < Program::kNumVersions; ++i)
    {
        fData[i].Reset();
    }
}

BgfxProgram::~BgfxProgram()
{
    Destroy();
}

void BgfxProgram::Create(CPUResource* resource)
{
    Rtt_ASSERT(CPUResource::kProgram == resource->GetType());
    fResource = resource;
    
    // Create global uniforms (only once)
    if (!fUniformsCreated)
    {
        CreateUniforms();
        fUniformsCreated = true;
    }
}

void BgfxProgram::Update(CPUResource* resource)
{
    Rtt_ASSERT(CPUResource::kProgram == resource->GetType());
    
    // Update all versions that have been created
    for (U32 i = 0; i < Program::kNumVersions; ++i)
    {
        if (fData[i].fAttemptedCreation)
        {
            UpdateVersion(static_cast<Program::Version>(i), fData[i]);
        }
    }
}

void BgfxProgram::Destroy()
{
    // Destroy all program versions
    for (U32 i = 0; i < Program::kNumVersions; ++i)
    {
        VersionData& data = fData[i];
        
        if (bgfx::isValid(data.fProgram))
        {
            bgfx::destroy(data.fProgram);
            data.fProgram = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(data.fVertexShader))
        {
            bgfx::destroy(data.fVertexShader);
            data.fVertexShader = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(data.fFragmentShader))
        {
            bgfx::destroy(data.fFragmentShader);
            data.fFragmentShader = BGFX_INVALID_HANDLE;
        }
        
        data.Reset();
    }
    
    // Destroy global uniforms
    DestroyUniforms();
    
    fResource = NULL;
}

void BgfxProgram::Bind(Program::Version version)
{
    Rtt_ASSERT(version < Program::kNumVersions);

    VersionData& data = fData[version];

    // Lazy creation if not already attempted
    if (!data.fAttemptedCreation)
    {
        CreateVersion(version, data);
    }
}

bgfx::ProgramHandle BgfxProgram::GetHandle(Program::Version version) const
{
    Rtt_ASSERT(version < Program::kNumVersions);
    return fData[version].fProgram;
}

bgfx::UniformHandle BgfxProgram::GetUniformHandle(Uniform::Name name) const
{
    Rtt_ASSERT(name < Uniform::kNumBuiltInVariables);
    
    switch (name)
    {
        case Uniform::kViewProjectionMatrix: return fUniformViewProjectionMatrix;
        case Uniform::kMaskMatrix0:          return fUniformMaskMatrix0;
        case Uniform::kMaskMatrix1:          return fUniformMaskMatrix1;
        case Uniform::kMaskMatrix2:          return fUniformMaskMatrix2;
        case Uniform::kTotalTime:            return fUniformTotalTime;
        case Uniform::kDeltaTime:            return fUniformDeltaTime;
        case Uniform::kTexelSize:            return fUniformTexelSize;
        case Uniform::kContentScale:         return fUniformContentScale;
        case Uniform::kContentSize:          return fUniformContentSize;
        case Uniform::kUserData0:            return fUniformUserData0;
        case Uniform::kUserData1:            return fUniformUserData1;
        case Uniform::kUserData2:            return fUniformUserData2;
        case Uniform::kUserData3:            return fUniformUserData3;
        default:
            Rtt_ASSERT_MSG(false, "Unknown uniform name");
            return BGFX_INVALID_HANDLE;
    }
}

bgfx::UniformHandle BgfxProgram::GetSamplerHandle(U32 unit) const
{
    Rtt_ASSERT(unit < 5);
    
    switch (unit)
    {
        case 0: return fSamplerFill0;
        case 1: return fSamplerFill1;
        case 2: return fSamplerMask0;
        case 3: return fSamplerMask1;
        case 4: return fSamplerMask2;
        default:
            Rtt_ASSERT_MSG(false, "Unknown sampler unit");
            return BGFX_INVALID_HANDLE;
    }
}

void BgfxProgram::SetUniform(Uniform::Name name, const void* data)
{
    Rtt_ASSERT(data);
    
    bgfx::UniformHandle handle = GetUniformHandle(name);
    if (!bgfx::isValid(handle))
    {
        return;
    }
    
    // Determine number of elements based on uniform type
    U16 numElements = 1;
    
    switch (name)
    {
        case Uniform::kViewProjectionMatrix:
            // mat4 - 4x4 floats
            bgfx::setUniform(handle, data, numElements);
            break;
            
        case Uniform::kMaskMatrix0:
        case Uniform::kMaskMatrix1:
        case Uniform::kMaskMatrix2:
            // bgfx Mat3 expects 9 compact floats, internal expansion to 3xvec4
            bgfx::setUniform(handle, data, numElements);
            break;
            
        case Uniform::kTotalTime:
        case Uniform::kDeltaTime:
        {
            // Scalar uniforms need to be packed into vec4 for bgfx
            float packed[4] = { *static_cast<const float*>(data), 0.0f, 0.0f, 0.0f };
            bgfx::setUniform(handle, packed, numElements);
            break;
        }
            
        case Uniform::kTexelSize:
        case Uniform::kContentScale:
        case Uniform::kContentSize:
        case Uniform::kUserData0:
        case Uniform::kUserData1:
        case Uniform::kUserData2:
        case Uniform::kUserData3:
            // vec4 uniforms
            bgfx::setUniform(handle, data, numElements);
            break;
            
        default:
            Rtt_ASSERT_MSG(false, "Unknown uniform name");
            break;
    }
}

bool BgfxProgram::IsValid(Program::Version version) const
{
    Rtt_ASSERT(version < Program::kNumVersions);
    return fData[version].IsValid();
}

void BgfxProgram::CreateVersion(Program::Version version, VersionData& data)
{
    data.fAttemptedCreation = true;

    // Load precompiled shader binaries
    const bgfx::Memory* vsMem = NULL;
    const bgfx::Memory* fsMem = NULL;

    if (!LoadShaderBinary(version, "vs", vsMem) || !LoadShaderBinary(version, "fs", fsMem))
    {
        Rtt_LogException("Failed to load shader binaries for version %d\n", version);
        return;
    }

    // Patch interface hash: ensure VS.hashOut == FS.hashIn
    // bgfx validates this at createProgram; mismatched precompiled shaders fail silently
    // Binary format: Magic(4) + HashIn(4) + HashOut(4) + ...
    if (vsMem && fsMem && vsMem->size >= 12 && fsMem->size >= 12)
    {
        uint32_t vsHashOut;
        uint32_t fsHashIn;
        memcpy(&vsHashOut, vsMem->data + 8, 4);  // offset 8: hashOut
        memcpy(&fsHashIn, fsMem->data + 4, 4);   // offset 4: hashIn

        if (vsHashOut != fsHashIn)
        {
            // Patch FS hashIn to match VS hashOut
            memcpy(const_cast<uint8_t*>(fsMem->data) + 4, &vsHashOut, 4);
        }

        // Also patch VS hashIn to match FS hashOut (for custom VS + default FS case)
        uint32_t vsHashIn;
        uint32_t fsHashOut;
        memcpy(&vsHashIn, vsMem->data + 4, 4);   // offset 4: hashIn
        memcpy(&fsHashOut, fsMem->data + 8, 4);  // offset 8: hashOut
        if (vsHashIn != fsHashOut && fsHashOut != 0)
        {
            memcpy(const_cast<uint8_t*>(vsMem->data) + 4, &fsHashOut, 4);
        }
    }

    // Create shaders from memory
    data.fVertexShader = bgfx::createShader(vsMem);
    data.fFragmentShader = bgfx::createShader(fsMem);

    if (!bgfx::isValid(data.fVertexShader) || !bgfx::isValid(data.fFragmentShader))
    {
        Rtt_LogException("Failed to create shaders for version %d\n", version);

        if (bgfx::isValid(data.fVertexShader))
        {
            bgfx::destroy(data.fVertexShader);
            data.fVertexShader = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(data.fFragmentShader))
        {
            bgfx::destroy(data.fFragmentShader);
            data.fFragmentShader = BGFX_INVALID_HANDLE;
        }
        return;
    }

    // Create program from shaders
    data.fProgram = bgfx::createProgram(data.fVertexShader, data.fFragmentShader, true);

    {
        Program* prog = static_cast<Program*>(fResource);
        ShaderResource* sr = prog ? prog->GetShaderResource() : NULL;
        Rtt_LogException("CreateVersion: program=%s valid=%d VS.idx=%d FS.idx=%d version=%d\n",
                         sr ? sr->GetName().c_str() : "default",
                         bgfx::isValid(data.fProgram) ? 1 : 0,
                         data.fVertexShader.idx, data.fFragmentShader.idx, version);
    }

    if (!bgfx::isValid(data.fProgram))
    {
        Rtt_LogException("ERROR: Failed to create program for version %d\n", version);
        Rtt_LogException("  VS handle: idx=%d, valid=%d\n",
                         data.fVertexShader.idx, bgfx::isValid(data.fVertexShader) ? 1 : 0);
        Rtt_LogException("  FS handle: idx=%d, valid=%d\n",
                         data.fFragmentShader.idx, bgfx::isValid(data.fFragmentShader) ? 1 : 0);

        // Log shader resource info for debugging
        Program* program = static_cast<Program*>(fResource);
        ShaderResource* shaderRes = program ? program->GetShaderResource() : NULL;
        if (shaderRes)
        {
            Rtt_LogException("  Effect: '%s' category=%d\n",
                             shaderRes->GetName().c_str(), (int)shaderRes->GetCategory());
        }
        Rtt_LogException("  NOTE: On GLES, check logcat for GL link error (BX_TRACE in debug builds).\n");

        // Shaders are destroyed by createProgram on failure if true is passed
        data.fVertexShader = BGFX_INVALID_HANDLE;
        data.fFragmentShader = BGFX_INVALID_HANDLE;
    }
}

void BgfxProgram::UpdateVersion(Program::Version version, VersionData& data)
{
    // Destroy existing program and shaders
    if (bgfx::isValid(data.fProgram))
    {
        bgfx::destroy(data.fProgram);
        data.fProgram = BGFX_INVALID_HANDLE;
    }
    // Note: Shaders are destroyed with the program, so we don't need to destroy them separately
    data.fVertexShader = BGFX_INVALID_HANDLE;
    data.fFragmentShader = BGFX_INVALID_HANDLE;
    
    // Recreate from updated binaries
    data.fAttemptedCreation = false;
    CreateVersion(version, data);
}

void BgfxProgram::ResetVersion(VersionData& data)
{
    data.Reset();
}

// Helper: look up a shader in the embedded effects table by filename.
// Returns true and sets outData/outSize if found.
static bool FindEffectShader(const char* filename, const unsigned char*& outData, size_t& outSize)
{
    for (int i = 0; i < s_bgfxShaderTableCount; ++i)
    {
        if (strcmp(s_bgfxShaderTable[i].filename, filename) == 0)
        {
            outData = s_bgfxShaderTable[i].data;
            outSize = s_bgfxShaderTable[i].size;
            return true;
        }
    }
    return false;
}

bool BgfxProgram::LoadShaderBinary(Program::Version version, const char* type, const bgfx::Memory*& outMem)
{
    const unsigned char* data = NULL;
    size_t size = 0;

    // Try to find effect-specific shader via ShaderResource name/category.
    // When both VS and FS exist, use the paired set. When only one exists
    // (e.g. generator effects have FS only, some filters have VS only),
    // use the available one and fall through to the default for the other.
    Program* program = static_cast<Program*>(fResource);
    ShaderResource* shaderRes = program ? program->GetShaderResource() : NULL;

    if (shaderRes)
    {
        const std::string& name = shaderRes->GetName();
        ShaderTypes::Category category = shaderRes->GetCategory();

        if (category != ShaderTypes::kCategoryDefault && !name.empty())
        {
            const char* categoryStr = ShaderTypes::StringForCategory(category);

            // Check that BOTH vs and fs exist before using either
            char vsFilename[128], fsFilename[128];
            snprintf(vsFilename, sizeof(vsFilename), "vs_%s_%s.bin", categoryStr, name.c_str());
            snprintf(fsFilename, sizeof(fsFilename), "fs_%s_%s.bin", categoryStr, name.c_str());

            const unsigned char* vsData = NULL; size_t vsSize = 0;
            const unsigned char* fsData = NULL; size_t fsSize = 0;
            bool hasVs = FindEffectShader(vsFilename, vsData, vsSize);
            bool hasFs = FindEffectShader(fsFilename, fsData, fsSize);

            if (hasVs && hasFs)
            {
                // Both shaders available — use the requested one
                if (strcmp(type, "vs") == 0) { data = vsData; size = vsSize; }
                else                         { data = fsData; size = fsSize; }
            }
            else if (!hasVs && hasFs)
            {
                // Only FS in embedded table (typical for generator effects that
                // use standard varyings and don't need a custom VS).
                // Use the embedded FS for fragment requests; VS falls through
                // to the default below, which is compatible.
                if (strcmp(type, "fs") == 0) { data = fsData; size = fsSize; }
            }
            else if (hasVs && !hasFs)
            {
                // Only VS in embedded table (filter with custom vertex kernel
                // but standard fragment shader, e.g. wobble).
                // Use the embedded VS; FS falls through to default.
                if (strcmp(type, "vs") == 0) { data = vsData; size = vsSize; }
            }
            else
            {
                // Not in embedded table — check runtime-compiled cache
                const char* cacheKey = (strcmp(type, "vs") == 0) ? vsFilename : fsFilename;
                const unsigned char* cachedData = NULL;
                size_t cachedSize = 0;

                if (BgfxShaderCompiler::FindCachedShader(cacheKey, cachedData, cachedSize))
                {
                    data = cachedData;
                    size = cachedSize;
                    Rtt_LogException("LoadShaderBinary: found cached %s for '%s' (%zu bytes)\n",
                                     type, cacheKey, cachedSize);
                }
                else if (strcmp(type, "vs") == 0 && !hasVs)
                {
                    // No VS in cache — use default VS (this is expected for custom effects)
                    // Only log error if FS is also missing (means compilation failed or wasn't attempted)
                    const unsigned char* fsCachedData = NULL;
                    size_t fsCachedSize = 0;
                    if (!hasFs && !BgfxShaderCompiler::FindCachedShader(fsFilename, fsCachedData, fsCachedSize))
                    {
                        Rtt_LogException("ERROR: Custom effect '%s' (category '%s') has no compiled bgfx/Metal shader. "
                            "Falling back to default shader — the effect WILL NOT render correctly.\n",
                            name.c_str(), categoryStr);
                    }
                }
            }
        }
    }

    // Fall back to default shaders
    if (!data)
    {
        if (strcmp(type, "vs") == 0)
        {
            data = S_VS_DEFAULT;
            size = S_VS_DEFAULT_SIZE;
        }
        else if (strcmp(type, "fs") == 0)
        {
            data = S_FS_DEFAULT;
            size = S_FS_DEFAULT_SIZE;
        }
        else
        {
            Rtt_LogException("Unknown shader type: %s\n", type);
            return false;
        }
    }

    if (!data || size == 0)
    {
        Rtt_LogException("No embedded shader data for type: %s\n", type);
        return false;
    }

    // Copy embedded data into bgfx-managed memory
    outMem = bgfx::copy(data, (uint32_t)size);
    return true;
}

void BgfxProgram::CreateUniforms()
{
    // Create built-in uniforms (global, created once)
    // Note: bgfx doesn't support float uniforms directly, so we use Vec4
    // and pack float values in the .x component
    
    fUniformViewProjectionMatrix = bgfx::createUniform(
        "u_ViewProjectionMatrix", bgfx::UniformType::Mat4);
    
    fUniformMaskMatrix0 = bgfx::createUniform("u_MaskMatrix0", bgfx::UniformType::Mat3);
    fUniformMaskMatrix1 = bgfx::createUniform("u_MaskMatrix1", bgfx::UniformType::Mat3);
    fUniformMaskMatrix2 = bgfx::createUniform("u_MaskMatrix2", bgfx::UniformType::Mat3);
    
    // Float uniforms packed in vec4.x
    fUniformTotalTime = bgfx::createUniform(
        "u_TotalTime", bgfx::UniformType::Vec4);
    fUniformDeltaTime = bgfx::createUniform(
        "u_DeltaTime", bgfx::UniformType::Vec4);
    
    fUniformTexelSize = bgfx::createUniform(
        "u_TexelSize", bgfx::UniformType::Vec4);
    
    // vec2 packed in vec4.xy
    fUniformContentScale = bgfx::createUniform(
        "u_ContentScale", bgfx::UniformType::Vec4);
    fUniformContentSize = bgfx::createUniform(
        "u_ContentSize", bgfx::UniformType::Vec4);
    
    fUniformUserData0 = bgfx::createUniform(
        "u_UserData0", bgfx::UniformType::Vec4);
    fUniformUserData1 = bgfx::createUniform(
        "u_UserData1", bgfx::UniformType::Vec4);
    fUniformUserData2 = bgfx::createUniform(
        "u_UserData2", bgfx::UniformType::Vec4);
    fUniformUserData3 = bgfx::createUniform(
        "u_UserData3", bgfx::UniformType::Vec4);
    
    // Texture flags: .x = 1.0 for alpha-only texture (needs R->A swizzle)
    fUniformTexFlags = bgfx::createUniform(
        "u_TexFlags", bgfx::UniformType::Vec4);

    // Create sampler uniforms
    fSamplerFill0 = bgfx::createUniform(
        "u_FillSampler0", bgfx::UniformType::Sampler);
    fSamplerFill1 = bgfx::createUniform(
        "u_FillSampler1", bgfx::UniformType::Sampler);
    fSamplerMask0 = bgfx::createUniform(
        "u_MaskSampler0", bgfx::UniformType::Sampler);
    fSamplerMask1 = bgfx::createUniform(
        "u_MaskSampler1", bgfx::UniformType::Sampler);
    fSamplerMask2 = bgfx::createUniform(
        "u_MaskSampler2", bgfx::UniformType::Sampler);
}

void BgfxProgram::DestroyUniforms()
{
    // Destroy all uniform handles
    auto destroyIfValid = [](bgfx::UniformHandle& handle)
    {
        if (bgfx::isValid(handle))
        {
            bgfx::destroy(handle);
            handle = BGFX_INVALID_HANDLE;
        }
    };
    
    destroyIfValid(fUniformViewProjectionMatrix);
    destroyIfValid(fUniformMaskMatrix0);
    destroyIfValid(fUniformMaskMatrix1);
    destroyIfValid(fUniformMaskMatrix2);
    destroyIfValid(fUniformTotalTime);
    destroyIfValid(fUniformDeltaTime);
    destroyIfValid(fUniformTexelSize);
    destroyIfValid(fUniformContentScale);
    destroyIfValid(fUniformContentSize);
    destroyIfValid(fUniformUserData0);
    destroyIfValid(fUniformUserData1);
    destroyIfValid(fUniformUserData2);
    destroyIfValid(fUniformUserData3);
    
    destroyIfValid(fUniformTexFlags);
    destroyIfValid(fSamplerFill0);
    destroyIfValid(fSamplerFill1);
    destroyIfValid(fSamplerMask0);
    destroyIfValid(fSamplerMask1);
    destroyIfValid(fSamplerMask2);
    
    fUniformsCreated = false;
}

const unsigned char*
BgfxProgram::GetDefaultFSData()
{
    return S_FS_DEFAULT;
}

unsigned int
BgfxProgram::GetDefaultFSSize()
{
    return S_FS_DEFAULT_SIZE;
}

const unsigned char*
BgfxProgram::GetDefaultVSData()
{
    return S_VS_DEFAULT;
}

unsigned int
BgfxProgram::GetDefaultVSSize()
{
    return S_VS_DEFAULT_SIZE;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
