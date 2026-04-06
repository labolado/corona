//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxShaderCompiler_H__
#define _Rtt_BgfxShaderCompiler_H__

#include <string>
#include <vector>
#include <unordered_map>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// Runtime compiler for Solar2D custom GLSL shaders → bgfx/Metal binary.
// Transforms GLSL kernel source to bgfx .sc format, then compiles
// via the shaderc binary (glslang → SPIR-V → spirv-cross → Metal).
class BgfxShaderCompiler
{
public:
    // Set paths needed for compilation (call once at startup)
    static void SetShadercPath(const char* path);
    static void SetBgfxIncludeDir(const char* path);
    static void SetVaryingDefPath(const char* path);

    // Transform a Solar2D GLSL fragment kernel into bgfx .sc format.
    // Input: the raw kernel string from defineEffect (FragmentKernel function).
    // Output: complete .sc source ready for shaderc compilation.
    static std::string TransformFragmentKernel(const char* kernel);

    // Transform a Solar2D GLSL vertex kernel into bgfx .sc format.
    static std::string TransformVertexKernel(const char* kernel);

    // Compile a .sc source string to bgfx binary using the shaderc binary.
    // shaderType: 'f' for fragment, 'v' for vertex
    // Returns true on success; on failure, outError contains the error message.
    static bool CompileShader(const std::string& scSource, char shaderType,
                              std::vector<uint8_t>& outBinary, std::string& outError,
                              const char* effectName = NULL);

    // Compile a custom effect's fragment (and optionally vertex) kernel.
    // Stores results in the shader cache on success.
    // Returns true if at least the fragment shader compiled successfully.
    static bool CompileCustomEffect(const char* category, const char* name,
                                    const char* kernelFrag, const char* kernelVert,
                                    std::string& outError);

    // Cache management
    static bool FindCachedShader(const char* key, const unsigned char*& outData, size_t& outSize);
    static void CacheCompiledShader(const char* key, const std::vector<uint8_t>& data);
    static void EvictCompiledShader(const char* key);
    static void ClearCache();

    // Check if runtime compilation is available (shaderc binary exists)
    static bool IsAvailable();

private:
    static std::string s_shadercPath;
    static std::string s_bgfxIncludeDir;
    static std::string s_varyingDefPath;
    static std::unordered_map<std::string, std::vector<uint8_t>> s_cache;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxShaderCompiler_H__
