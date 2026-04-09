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

// Mapping from user varying name (e.g. "outlineColor") to bgfx slot info.
struct VaryingInfo
{
    std::string type;   // e.g. "vec4", "vec3", "vec2", "float"
    std::string slot;   // e.g. "v_Custom0"
    int arraySize;      // 0 = scalar, >0 = array (e.g. 5 for vec2[5])
};
// For scalar varyings: "outlineColor" → { "vec4", "v_Custom0", 0 }
// For array varyings: "blurCoords" → { "vec2", "v_Custom0", 5 }
//   Array elements are packed into vec4 slots:
//     name[0] → v_Custom0.xy, name[1] → v_Custom0.zw,
//     name[2] → v_Custom1.xy, name[3] → v_Custom1.zw, etc.
typedef std::unordered_map<std::string, VaryingInfo> VaryingMapping;

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

    // Parse custom varying declarations from vertex and fragment kernels.
    // Returns a mapping of user varying names to bgfx v_CustomN slots.
    static VaryingMapping ParseCustomVaryings(const char* kernelVert, const char* kernelFrag);

    // Transform a Solar2D GLSL fragment kernel into bgfx .sc format.
    // Input: the raw kernel string from defineEffect (FragmentKernel function).
    // Output: complete .sc source ready for shaderc compilation.
    static std::string TransformFragmentKernel(const char* kernel,
                                                const VaryingMapping& varyings = VaryingMapping());

    // Transform a Solar2D GLSL vertex kernel into bgfx .sc format.
    static std::string TransformVertexKernel(const char* kernel,
                                              const VaryingMapping& varyings = VaryingMapping());

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

    // Construct a bgfx shader binary in-memory without shaderc.
    // For GLES backends, the binary wraps GLSL/ESSL source that bgfx
    // passes directly to glShaderSource/glCompileShader.
    // shaderType: 'V' for vertex, 'F' for fragment
    // interfaceHash: the varying interface hash that must match between VS and FS.
    //   For FS: hashIn must equal the VS's hashOut (the "output interface").
    //   For VS: hashOut must equal the FS's hashIn (the "input interface").
    //   Use ExtractInterfaceHash() to read this from precompiled shader binaries.
    static bool ConstructShaderBinary(
        const std::string& shaderSource,
        char shaderType,
        std::vector<uint8_t>& outBinary,
        uint32_t interfaceHash = 0);

    // Extract hashIn and hashOut from a precompiled bgfx shader binary.
    // Returns false if the binary is too small or has an invalid magic number.
    static bool ExtractInterfaceHash(const unsigned char* data, size_t size,
                                     uint32_t& outHashIn, uint32_t& outHashOut);

    // Transform .sc source into pure ESSL suitable for ConstructShaderBinary.
    // Strips $input/$output, resolves SAMPLER2D/mul macros, adds #version header.
    static std::string TransformScToESSL(const std::string& scSource, char shaderType);

    // Parse uniform declarations from .sc source for binary construction.
    struct UniformEntry {
        std::string name;
        uint8_t type;       // bgfx UniformType: 0=Sampler, 2=Vec4, 3=Mat3, 4=Mat4
        uint8_t num;        // array count (1 for non-array)
        uint16_t regIndex;  // register/binding index
        uint16_t regCount;  // register count
    };
    static std::vector<UniformEntry> ParseUniformsFromSc(const std::string& scSource);

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
