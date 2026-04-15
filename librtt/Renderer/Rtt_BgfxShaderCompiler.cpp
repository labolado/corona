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

#include "Renderer/Rtt_BgfxShaderCompiler.h"
#include "Renderer/Rtt_BgfxProgram.h"
#include "Core/Rtt_Assert.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// Static member initialization
std::string BgfxShaderCompiler::s_shadercPath;
std::string BgfxShaderCompiler::s_bgfxIncludeDir;
std::string BgfxShaderCompiler::s_varyingDefPath;
std::unordered_map<std::string, std::vector<uint8_t>> BgfxShaderCompiler::s_cache;

// Auto-detect paths based on source tree layout.
// __FILE__ = .../librtt/Renderer/Rtt_BgfxShaderCompiler.cpp
// Project root = __FILE__ minus "librtt/Renderer/Rtt_BgfxShaderCompiler.cpp"
static std::string GetProjectRoot()
{
    std::string thisFile(__FILE__);
    const char* suffix = "librtt/Renderer/Rtt_BgfxShaderCompiler.cpp";
    size_t pos = thisFile.rfind(suffix);
    if (pos != std::string::npos)
        return thisFile.substr(0, pos);
    return "";
}

static bool s_autoDetected = false;

static void AutoDetectPaths()
{
    if (s_autoDetected) return;
    s_autoDetected = true;

    std::string root = GetProjectRoot();
    if (root.empty()) return;

    struct stat st;

    // shaderc binary
    std::string shadercPath = root + "external/bgfx/.build/osx-arm64/bin/shadercRelease";
    if (stat(shadercPath.c_str(), &st) == 0)
        BgfxShaderCompiler::SetShadercPath(shadercPath.c_str());

    // bgfx include dir (contains bgfx_shader.sh)
    std::string bgfxInclude = root + "external/bgfx/src";
    if (stat(bgfxInclude.c_str(), &st) == 0)
        BgfxShaderCompiler::SetBgfxIncludeDir(bgfxInclude.c_str());

    // varying.def.sc
    std::string varyingDef = root + "librtt/Display/Shader/bgfx/varying.def.sc";
    if (stat(varyingDef.c_str(), &st) == 0)
        BgfxShaderCompiler::SetVaryingDefPath(varyingDef.c_str());
}

// Inline bgfx shader compatibility definitions.
// Replaces #include <bgfx_shader.sh> so the .sc templates are self-contained
// and don't depend on external include paths (required for ESSL cross-compilation).
static const char kBgfxShaderInline[] =
    "\n"
    "// --- bgfx shader compatibility (inlined) ---\n"
    "#if BGFX_SHADER_LANGUAGE_GLSL\n"
    "  #define SAMPLER2D(_name, _reg) uniform sampler2D _name\n"
    "  #define SAMPLER3D(_name, _reg) uniform sampler3D _name\n"
    "  #define SAMPLERCUBE(_name, _reg) uniform samplerCube _name\n"
    "  #define mul(_a, _b) ((_a) * (_b))\n"
    "  #define saturate(_x) clamp(_x, 0.0, 1.0)\n"
    "  #define CONST(_x) const _x\n"
    "  #define atan2(_x, _y) atan(_x, _y)\n"
    "  vec2 vec2_splat(float _x) { return vec2(_x, _x); }\n"
    "  vec3 vec3_splat(float _x) { return vec3(_x, _x, _x); }\n"
    "  vec4 vec4_splat(float _x) { return vec4(_x, _x, _x, _x); }\n"
    "  float rcp(float _a) { return 1.0/_a; }\n"
    "  vec2  rcp(vec2  _a) { return vec2(1.0)/_a; }\n"
    "  vec3  rcp(vec3  _a) { return vec3(1.0)/_a; }\n"
    "  vec4  rcp(vec4  _a) { return vec4(1.0)/_a; }\n"
    "  #if BGFX_SHADER_LANGUAGE_GLSL >= 130\n"
    "    #define texture2D(_sampler, _coord) texture(_sampler, _coord)\n"
    "    #define texture2DLod(_sampler, _coord, _lod) textureLod(_sampler, _coord, _lod)\n"
    "    #define texture2DLodOffset(_sampler, _coord, _lod, _offset) textureLodOffset(_sampler, _coord, _lod, _offset)\n"
    "    #define texture2DBias(_sampler, _coord, _bias) texture(_sampler, _coord, _bias)\n"
    "  #else\n"
    "    #define texture2DBias(_sampler, _coord, _bias) texture2D(_sampler, _coord, _bias)\n"
    "  #endif\n"
    "#else\n"
    "  // Metal / HLSL path\n"
    "  #define CONST(_x) static const _x\n"
    "  #define REGISTER(_type, _reg) register(_type ## _reg)\n"
    "  #define dFdx(_x) ddx(_x)\n"
    "  #define dFdy(_y) ddy(-(_y))\n"
    "  #define inversesqrt(_x) rsqrt(_x)\n"
    "  #define fract(_x) frac(_x)\n"
    "  float rcp(float _a) { return 1.0/_a; }\n"
    "  vec2  rcp(vec2  _a) { return vec2(1.0)/_a; }\n"
    "  vec3  rcp(vec3  _a) { return vec3(1.0)/_a; }\n"
    "  vec4  rcp(vec4  _a) { return vec4(1.0)/_a; }\n"
    "  struct BgfxSampler2D { SamplerState m_sampler; Texture2D m_texture; };\n"
    "  vec4 bgfxTexture2D(BgfxSampler2D _sampler, vec2 _coord) {\n"
    "    return _sampler.m_texture.Sample(_sampler.m_sampler, _coord);\n"
    "  }\n"
    "  vec4 bgfxTexture2DLod(BgfxSampler2D _sampler, vec2 _coord, float _level) {\n"
    "    return _sampler.m_texture.SampleLevel(_sampler.m_sampler, _coord, _level);\n"
    "  }\n"
    "  vec4 bgfxTexture2DLodOffset(BgfxSampler2D _sampler, vec2 _coord, float _level, ivec2 _offset) {\n"
    "    return _sampler.m_texture.SampleLevel(_sampler.m_sampler, _coord, _level, _offset);\n"
    "  }\n"
    "  vec4 bgfxTexture2DBias(BgfxSampler2D _sampler, vec2 _coord, float _bias) {\n"
    "    return _sampler.m_texture.SampleBias(_sampler.m_sampler, _coord, _bias);\n"
    "  }\n"
    "  vec4 bgfxTexture2DProj(BgfxSampler2D _sampler, vec3 _coord) {\n"
    "    vec2 coord = _coord.xy * rcp(_coord.z);\n"
    "    return _sampler.m_texture.Sample(_sampler.m_sampler, coord);\n"
    "  }\n"
    "  vec2 bgfxTextureSize(BgfxSampler2D _sampler, int _lod) {\n"
    "    vec2 result; float mips;\n"
    "    _sampler.m_texture.GetDimensions(_lod, result.x, result.y, mips);\n"
    "    return result;\n"
    "  }\n"
    "  #define SAMPLER2D(_name, _reg) \\\n"
    "    uniform SamplerState _name ## Sampler : REGISTER(s, _reg); \\\n"
    "    uniform Texture2D _name ## Texture : REGISTER(t, _reg); \\\n"
    "    static BgfxSampler2D _name = { _name ## Sampler, _name ## Texture }\n"
    "  #define sampler2D BgfxSampler2D\n"
    "  #define texture2D(_sampler, _coord) bgfxTexture2D(_sampler, _coord)\n"
    "  #define texture2DLod(_sampler, _coord, _level) bgfxTexture2DLod(_sampler, _coord, _level)\n"
    "  #define texture2DLodOffset(_sampler, _coord, _lod, _offset) bgfxTexture2DLodOffset(_sampler, _coord, _lod, _offset)\n"
    "  #define texture2DBias(_sampler, _coord, _bias) bgfxTexture2DBias(_sampler, _coord, _bias)\n"
    "  #define texture2DProj(_sampler, _coord) bgfxTexture2DProj(_sampler, _coord)\n"
    "  #define mul(_a, _b) mul(_a, _b)\n"
    "  #define mix lerp\n"
    "  float mod(float x, float y) { return x - y * floor(x/y); }\n"
    "  vec2  mod(vec2  x, vec2  y) { return x - y * floor(x/y); }\n"
    "  vec3  mod(vec3  x, vec3  y) { return x - y * floor(x/y); }\n"
    "  vec4  mod(vec4  x, vec4  y) { return x - y * floor(x/y); }\n"
    "  vec2  mod(vec2  x, float y) { return x - y * floor(x/y); }\n"
    "  vec3  mod(vec3  x, float y) { return x - y * floor(x/y); }\n"
    "  vec4  mod(vec4  x, float y) { return x - y * floor(x/y); }\n"
    "  vec2 vec2_splat(float _x) { return vec2(_x, _x); }\n"
    "  vec3 vec3_splat(float _x) { return vec3(_x, _x, _x); }\n"
    "  vec4 vec4_splat(float _x) { return vec4(_x, _x, _x, _x); }\n"
    "#endif // BGFX_SHADER_LANGUAGE_GLSL\n"
    "// --- end bgfx shader compatibility ---\n"
    "\n";

// The bgfx .sc fragment shader template for custom effects.
// Split into header (always included) and user data uniforms (conditionally included).
// User data uniforms are skipped when the preamble already declares them,
// because the preamble may use different types (e.g. vec2 instead of vec4).
static const char kFragmentScInputBase[] =
    "$input v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2";

static const char kFragmentScHeaderBody[] =
    "\n"
    "// Sampler uniforms\n"
    "SAMPLER2D(u_FillSampler0, 0);\n"
    "SAMPLER2D(u_FillSampler1, 1);\n"
    "SAMPLER2D(u_MaskSampler0, 2);\n"
    "SAMPLER2D(u_MaskSampler1, 3);\n"
    "SAMPLER2D(u_MaskSampler2, 4);\n"
    "\n"
    "// Time and data uniforms (packed in vec4)\n"
    "uniform vec4 u_TotalTime;\n"
    "uniform vec4 u_DeltaTime;\n"
    "uniform vec4 u_TexelSize;\n"
    "uniform vec4 u_ContentScale;\n"
    "uniform vec4 u_ContentSize;\n"
    "\n";

// User data uniforms — only emitted when preamble doesn't already declare them.
static const char* kUserDataUniformDecls[] = {
    "uniform vec4 u_UserData0;\n",
    "uniform vec4 u_UserData1;\n",
    "uniform vec4 u_UserData2;\n",
    "uniform vec4 u_UserData3;\n",
};
static const char* kUserDataUniformNames[] = {
    "u_UserData0", "u_UserData1", "u_UserData2", "u_UserData3",
};
static const int kNumUserDataUniforms = 4;

static const char kFragmentScFooter[] =
    "// Texture flags\n"
    "uniform vec4 u_TexFlags;\n"
    "\n"
    "// Solar2D macros for shader compatibility\n"
    "#define CoronaColorScale(color) (v_ColorScale * (color))\n"
    "#define CoronaVertexUserData v_UserData\n"
    "#define CoronaTotalTime u_TotalTime.x\n"
    "#define CoronaDeltaTime u_DeltaTime.x\n"
    "#define CoronaTexelSize u_TexelSize\n"
    "#define CoronaContentScale u_ContentScale.xy\n"
    "#define CoronaSampler0 u_FillSampler0\n"
    "#define CoronaSampler1 u_FillSampler1\n"
    "\n";

// Helper: check if a preamble already declares a uniform by name.
// Looks for "uniform <type> <name>" pattern to avoid false positives from comments.
static bool PreambleDeclaresUniform(const std::string& preamble, const char* uniformName)
{
    size_t pos = 0;
    while ((pos = preamble.find(uniformName, pos)) != std::string::npos)
    {
        // Walk backward to check if preceded by "uniform <type> "
        // Simple heuristic: search for "uniform" before this position on same logical line
        size_t lineStart = preamble.rfind('\n', pos);
        if (lineStart == std::string::npos) lineStart = 0; else ++lineStart;
        std::string line = preamble.substr(lineStart, pos - lineStart + strlen(uniformName));
        if (line.find("uniform") != std::string::npos)
        {
            return true;
        }
        pos += strlen(uniformName);
    }
    return false;
}

// Helper: build the $input line with optional custom varying slots
static std::string BuildFragmentInputLine(const VaryingMapping& varyings)
{
    std::string line = kFragmentScInputBase;
    // Collect all used v_Custom slots (including multi-slot array varyings)
    bool usedSlots[4] = {false, false, false, false};
    for (const auto& p : varyings)
    {
        int baseNum = 0;
        if (p.second.slot.size() > 8) baseNum = p.second.slot[8] - '0';
        if (p.second.arraySize > 0)
        {
            int slotsNeeded = (p.second.arraySize + 1) / 2;
            for (int s = 0; s < slotsNeeded && (baseNum + s) < 4; s++)
                usedSlots[baseNum + s] = true;
        }
        else
        {
            if (baseNum >= 0 && baseNum < 4) usedSlots[baseNum] = true;
        }
    }
    for (int i = 0; i < 4; i++)
    {
        if (usedSlots[i])
        {
            char slot[16];
            snprintf(slot, sizeof(slot), ", v_Custom%d", i);
            line += slot;
        }
    }
    return line;
}

// Helper: remove "varying <type> <name>;" declarations from source
static std::string RemoveVaryingDeclarations(const std::string& src)
{
    std::string result;
    std::istringstream iss(src);
    std::string line;
    while (std::getline(iss, line))
    {
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos)
        {
            std::string content = line.substr(firstNonSpace);
            if (content.find("varying") == 0 &&
                (content.size() <= 7 || content[7] == ' ' || content[7] == '\t'))
            {
                continue; // skip varying declaration
            }
        }
        result += line + "\n";
    }
    return result;
}

// Helper: for array varyings, get the packed slot expression for element i.
// vec2[N] packs 2 elements per vec4 slot: [0]→slot.xy, [1]→slot.zw, [2]→(slot+1).xy, etc.
static std::string GetArrayElementExpr(const std::string& baseSlot, int elementIndex)
{
    int slotOffset = elementIndex / 2;
    bool isSecondHalf = (elementIndex % 2) == 1;

    char buf[32];
    // Parse base slot number from "v_Custom0" → 0
    int baseNum = 0;
    if (baseSlot.size() > 8 && baseSlot[8] >= '0' && baseSlot[8] <= '3')
        baseNum = baseSlot[8] - '0';

    snprintf(buf, sizeof(buf), "v_Custom%d.%s", baseNum + slotOffset, isSecondHalf ? "zw" : "xy");
    return std::string(buf);
}

// Helper: replace user varying names with v_CustomN slots (word-boundary safe)
// For array varyings, replaces "name[i]" with packed slot expressions.
static std::string ReplaceVaryingNames(const std::string& src, const VaryingMapping& varyings)
{
    std::string result = src;
    for (const auto& pair : varyings)
    {
        const std::string& varName = pair.first;
        const VaryingInfo& info = pair.second;

        if (info.arraySize > 0)
        {
            // Array varying: replace "name[i]" with packed slot expression
            for (int i = 0; i < info.arraySize; i++)
            {
                char pattern[128];
                snprintf(pattern, sizeof(pattern), "%s[%d]", varName.c_str(), i);
                std::string replacement = GetArrayElementExpr(info.slot, i);

                size_t pos = 0;
                std::string pat(pattern);
                while ((pos = result.find(pat, pos)) != std::string::npos)
                {
                    bool leftOk = (pos == 0) || (!isalnum(result[pos - 1]) && result[pos - 1] != '_');
                    if (leftOk)
                    {
                        result.replace(pos, pat.size(), replacement);
                        pos += replacement.size();
                    }
                    else
                    {
                        pos += pat.size();
                    }
                }
            }
        }
        else
        {
            // Scalar varying: direct name replacement
            const std::string& slotName = info.slot;
            size_t pos = 0;
            while ((pos = result.find(varName, pos)) != std::string::npos)
            {
                bool leftOk = (pos == 0) || (!isalnum(result[pos - 1]) && result[pos - 1] != '_');
                size_t endPos = pos + varName.size();
                bool rightOk = (endPos >= result.size()) || (!isalnum(result[endPos]) && result[endPos] != '_');
                if (leftOk && rightOk)
                {
                    result.replace(pos, varName.size(), slotName);
                    pos += slotName.size();
                }
                else
                {
                    pos += varName.size();
                }
            }
        }
    }
    return result;
}

// Build the complete template, skipping user data uniforms that preamble already declares.
static std::string BuildFragmentTemplate(const std::string& preamble,
                                          const VaryingMapping& varyings = VaryingMapping())
{
    std::string result = BuildFragmentInputLine(varyings) + "\n";
    result += kBgfxShaderInline;
    result += kFragmentScHeaderBody;

    // Emit user data uniforms only if preamble doesn't already declare them
    result += "// User data uniforms\n";
    for (int i = 0; i < kNumUserDataUniforms; ++i)
    {
        if (!PreambleDeclaresUniform(preamble, kUserDataUniformNames[i]))
        {
            result += kUserDataUniformDecls[i];
        }
    }
    result += "\n";

    result += kFragmentScFooter;
    return result;
}

void BgfxShaderCompiler::SetShadercPath(const char* path)
{
    s_shadercPath = path ? path : "";
}

void BgfxShaderCompiler::SetBgfxIncludeDir(const char* path)
{
    s_bgfxIncludeDir = path ? path : "";
}

void BgfxShaderCompiler::SetVaryingDefPath(const char* path)
{
    s_varyingDefPath = path ? path : "";
}

bool BgfxShaderCompiler::IsAvailable()
{
    AutoDetectPaths();
    if (s_shadercPath.empty()) return false;
    struct stat st;
    return (stat(s_shadercPath.c_str(), &st) == 0 && (st.st_mode & S_IXUSR));
}

// ----------------------------------------------------------------------------
// Kernel transformation: Solar2D GLSL → bgfx .sc
// ----------------------------------------------------------------------------

// Helper: strip Solar2D precision macros (P_COLOR, P_UV, etc.)
static std::string StripPrecisionMacros(const std::string& src)
{
    std::string result = src;
    // These macros are defined as empty on desktop GL
    const char* macros[] = { "P_COLOR ", "P_UV ", "P_DEFAULT ", "P_POSITION ", "P_NORMAL ", "P_RANDOM ",
                              "lowp ", "mediump ", "highp " };
    for (const char* macro : macros)
    {
        size_t pos;
        while ((pos = result.find(macro)) != std::string::npos)
        {
            result.erase(pos, strlen(macro));
        }
    }
    return result;
}

// Helper: find matching closing brace for an opening brace at position pos
static size_t FindMatchingBrace(const std::string& src, size_t openPos)
{
    int depth = 0;
    for (size_t i = openPos; i < src.size(); ++i)
    {
        if (src[i] == '{') ++depth;
        else if (src[i] == '}') { --depth; if (depth == 0) return i; }
    }
    return std::string::npos;
}

// ----------------------------------------------------------------------------
// ParseCustomVaryings: scan vertex + fragment kernels for "varying <type> <name>;"
// Returns mapping of user names → v_CustomN slots (max 4).
// ----------------------------------------------------------------------------

VaryingMapping BgfxShaderCompiler::ParseCustomVaryings(const char* kernelVert, const char* kernelFrag)
{
    VaryingMapping mapping;
    int slotIndex = 0;

    auto parseSource = [&](const char* raw)
    {
        if (!raw || !*raw) return;
        std::string src = StripPrecisionMacros(std::string(raw));
        size_t pos = 0;
        while ((pos = src.find("varying", pos)) != std::string::npos)
        {
            bool leftOk = (pos == 0) || (!isalnum(src[pos - 1]) && src[pos - 1] != '_');
            size_t afterKw = pos + 7;
            bool rightOk = (afterKw < src.size()) && (src[afterKw] == ' ' || src[afterKw] == '\t');
            if (!leftOk || !rightOk) { pos = afterKw; continue; }

            size_t semi = src.find(';', afterKw);
            if (semi == std::string::npos) { pos = afterKw; continue; }

            std::string decl = src.substr(afterKw, semi - afterKw);
            std::istringstream iss(decl);
            std::string type, name;
            iss >> type >> name;

            // Check for array varying: "vec2 blurCoords[5]"
            int arraySize = 0;
            std::string baseName = name;
            size_t bracketPos = name.find('[');
            if (bracketPos != std::string::npos)
            {
                baseName = name.substr(0, bracketPos);
                size_t closeBracket = name.find(']', bracketPos);
                if (closeBracket != std::string::npos)
                    arraySize = atoi(name.substr(bracketPos + 1, closeBracket - bracketPos - 1).c_str());
            }

            if (!baseName.empty() && mapping.find(baseName) == mapping.end())
            {
                if (arraySize > 0)
                {
                    // Array varying: pack vec2[N] into vec4 slots (2 elements per slot)
                    // Only vec2 arrays are supported (most common for UV coordinates)
                    int slotsNeeded = (arraySize + 1) / 2; // ceil(N/2) for vec2
                    if (type != "vec2")
                    {
                        Rtt_LogException("WARNING: Array varying '%s' type '%s' not supported (only vec2 arrays), skipping\n",
                            baseName.c_str(), type.c_str());
                    }
                    else if (slotIndex + slotsNeeded > 4)
                    {
                        Rtt_LogException("WARNING: Array varying '%s[%d]' needs %d slots but only %d available, skipping\n",
                            baseName.c_str(), arraySize, slotsNeeded, 4 - slotIndex);
                    }
                    else
                    {
                        char slot[16];
                        snprintf(slot, sizeof(slot), "v_Custom%d", slotIndex);
                        mapping[baseName] = { type, std::string(slot), arraySize };
                        slotIndex += slotsNeeded;
                    }
                }
                else
                {
                    // Scalar varying
                    if (slotIndex >= 4)
                    {
                        Rtt_LogException("WARNING: Too many custom varyings (max 4), skipping '%s'\n", baseName.c_str());
                    }
                    else
                    {
                        char slot[16];
                        snprintf(slot, sizeof(slot), "v_Custom%d", slotIndex);
                        mapping[baseName] = { type, std::string(slot), 0 };
                        ++slotIndex;
                    }
                }
            }
            pos = semi + 1;
        }
    };

    // Parse vertex first so its varyings get lower slot indices
    parseSource(kernelVert);
    parseSource(kernelFrag);

    if (!mapping.empty())
    {
        Rtt_LogException("ParseCustomVaryings: found %d varying(s):\n", (int)mapping.size());
        for (const auto& p : mapping)
            Rtt_LogException("  %s (%s) -> %s\n", p.first.c_str(), p.second.type.c_str(), p.second.slot.c_str());
    }

    return mapping;
}

// ----------------------------------------------------------------------------
// TransformFragmentKernel
// ----------------------------------------------------------------------------

std::string BgfxShaderCompiler::TransformFragmentKernel(const char* kernel,
                                                         const VaryingMapping& varyings)
{
    if (!kernel || !*kernel) return "";

    std::string src(kernel);

    // 1. Strip precision macros
    src = StripPrecisionMacros(src);

    // 2. Find FragmentKernel function signature
    size_t funcPos = src.find("FragmentKernel");
    if (funcPos == std::string::npos)
    {
        std::string result = BuildFragmentTemplate(src, varyings);
        result += "void main()\n{\n";
        result += src;
        result += "\n}\n";
        return result;
    }

    // 2b. Extract preamble (code before FragmentKernel)
    std::string preamble;
    {
        size_t declStart = funcPos;
        if (declStart > 0)
        {
            size_t pos = declStart - 1;
            while (pos > 0 && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r'))
                --pos;
            while (pos > 0 && (isalnum(src[pos]) || src[pos] == '_'))
                --pos;
            if (!isalnum(src[pos]) && src[pos] != '_')
                ++pos;
            declStart = pos;
        }
        if (declStart > 0)
            preamble = src.substr(0, declStart);
    }

    // Find parameter name
    size_t parenOpen = src.find('(', funcPos);
    size_t parenClose = src.find(')', parenOpen);
    if (parenOpen == std::string::npos || parenClose == std::string::npos)
        return "";

    std::string paramStr = src.substr(parenOpen + 1, parenClose - parenOpen - 1);
    std::string paramName;
    {
        std::istringstream iss(paramStr);
        std::string token;
        while (iss >> token) { paramName = token; }
    }
    if (paramName.empty()) paramName = "texCoord";

    // 3. Extract function body
    size_t bodyOpen = src.find('{', parenClose);
    if (bodyOpen == std::string::npos) return "";
    size_t bodyClose = FindMatchingBrace(src, bodyOpen);
    if (bodyClose == std::string::npos) return "";
    std::string body = src.substr(bodyOpen + 1, bodyClose - bodyOpen - 1);

    // 4. Parameter → local variable
    {
        std::string localVar = "_" + paramName;
        std::string localDecl = "vec2 " + localVar + " = (v_TexCoord.z > 0.0) ? v_TexCoord.xy / v_TexCoord.z : v_TexCoord.xy;\n";
        body = "\n    " + localDecl + body;

        size_t pos = localDecl.size() + 5;
        while ((pos = body.find(paramName, pos)) != std::string::npos)
        {
            bool leftOk = (pos == 0) || (!isalnum(body[pos - 1]) && body[pos - 1] != '_');
            size_t endPos = pos + paramName.size();
            bool rightOk = (endPos >= body.size()) || (!isalnum(body[endPos]) && body[endPos] != '_');
            if (leftOk && rightOk)
            {
                body.replace(pos, paramName.size(), localVar);
                pos += localVar.size();
            }
            else
                pos += paramName.size();
        }
    }

    // 5. Replace "return <expr>;" with a block that applies mask sampling then assigns gl_FragColor.
    // This mirrors what the GL shell does: after calling FragmentKernel(), multiply by each active mask sampler.
    // u_TexFlags.y holds the mask count (0=none, 1=one, 2=two, 3=three).
    {
        size_t pos = 0;
        while ((pos = body.find("return", pos)) != std::string::npos)
        {
            bool leftOk = (pos == 0) || (!isalnum(body[pos - 1]) && body[pos - 1] != '_');
            size_t afterReturn = pos + 6;
            bool rightOk = (afterReturn >= body.size()) || (!isalnum(body[afterReturn]) && body[afterReturn] != '_');
            if (leftOk && rightOk)
            {
                size_t semiPos = body.find(';', afterReturn);
                if (semiPos != std::string::npos)
                {
                    std::string expr = body.substr(afterReturn, semiPos - afterReturn);
                    // Build a block that applies mask samplers before assigning gl_FragColor,
                    // matching the GL shell behavior for MASK_COUNT > 0/1/2.
                    // Don't use block scope for _fragResult — some mobile GLES drivers
                    // (PowerVR on Samsung) have scoping bugs with variables declared in {...}.
                    std::string replacement =
                        "gl_FragColor =" + expr + ";\n"
                        "    if (u_TexFlags.y > 0.5) gl_FragColor *= texture2D(u_MaskSampler0, v_MaskUV0).r;\n"
                        "    if (u_TexFlags.y > 1.5) gl_FragColor *= texture2D(u_MaskSampler1, v_MaskUV1).r;\n"
                        "    if (u_TexFlags.y > 2.5) gl_FragColor *= texture2D(u_MaskSampler2, v_MaskUV2).r;\n";
                    body.replace(pos, semiPos - pos + 1, replacement);
                    pos += replacement.size();
                }
                else
                    pos = afterReturn;
            }
            else
                pos = afterReturn;
        }
    }

    // 6. Process varyings in preamble and body
    // Always remove varying declarations — "varying" is not valid in bgfx .sc.
    // For mapped varyings, names get replaced with v_CustomN.
    // For array varyings (skipped by ParseCustomVaryings), the declaration is
    // removed and the shader falls back to default behavior.
    preamble = RemoveVaryingDeclarations(preamble);
    if (!varyings.empty())
    {
        preamble = ReplaceVaryingNames(preamble, varyings);
        body = ReplaceVaryingNames(body, varyings);
    }

    // 6b. Expand single-arg vector constructors for Metal compatibility
    // vec4(0.0) -> vec4(0.0, 0.0, 0.0, 0.0)
    auto ExpandVectorConstructors = [](std::string& code) {
        // Pattern: vec2(v), vec3(v), vec4(v) where v is a number
        // Replace with vec2(v, v), vec3(v, v, v), vec4(v, v, v, v)
        const std::pair<const char*, int> patterns[] = {
            {"vec2", 2}, {"vec3", 3}, {"vec4", 4}
        };
        for (const auto& pat : patterns)
        {
            const char* prefix = pat.first;
            int count = pat.second;
            size_t pos = 0;
            while ((pos = code.find(prefix, pos)) != std::string::npos)
            {
                size_t parenOpen = code.find('(', pos);
                if (parenOpen == std::string::npos || parenOpen > pos + 5) { ++pos; continue; }
                // Find matching closing paren (handles nested parens)
                int depth = 1;
                size_t parenClose = std::string::npos;
                for (size_t i = parenOpen + 1; i < code.size(); ++i)
                {
                    if (code[i] == '(') ++depth;
                    else if (code[i] == ')') { --depth; if (depth == 0) { parenClose = i; break; } }
                }
                if (parenClose == std::string::npos) { ++pos; continue; }
                std::string inner = code.substr(parenOpen + 1, parenClose - parenOpen - 1);
                // Trim whitespace
                size_t start = inner.find_first_not_of(" \t");
                size_t end = inner.find_last_not_of(" \t");
                if (start == std::string::npos) { ++pos; continue; }
                std::string arg = inner.substr(start, end - start + 1);
                // Check if single argument (no comma at top level, respecting nested parens)
                bool hasTopLevelComma = false;
                {
                    int d = 0;
                    for (size_t i = 0; i < arg.size(); ++i)
                    {
                        if (arg[i] == '(') ++d;
                        else if (arg[i] == ')') --d;
                        else if (arg[i] == ',' && d == 0) { hasTopLevelComma = true; break; }
                    }
                }
                if (hasTopLevelComma) { ++pos; continue; }
                // Check if it's a number or identifier
                bool isNumber = !arg.empty() && (isdigit(arg[0]) || arg[0] == '-' || arg[0] == '.');
                bool isIdentifier = !arg.empty() && (isalpha(arg[0]) || arg[0] == '_');
                if (!isNumber && !isIdentifier) { ++pos; continue; }
                // Skip if arg is a swizzle that already provides enough components
                // e.g. vec3(color.rgb) should NOT expand — .rgb already has 3 components
                {
                    size_t dotPos = arg.rfind('.');
                    if (dotPos != std::string::npos && dotPos + 1 < arg.size())
                    {
                        std::string swiz = arg.substr(dotPos + 1);
                        bool isSwizzle = !swiz.empty();
                        for (char c : swiz)
                        {
                            if (!(c == 'x' || c == 'y' || c == 'z' || c == 'w' ||
                                  c == 'r' || c == 'g' || c == 'b' || c == 'a' ||
                                  c == 's' || c == 't' || c == 'p' || c == 'q'))
                            {
                                isSwizzle = false;
                                break;
                            }
                        }
                        if (isSwizzle && (int)swiz.size() >= count)
                        {
                            pos = parenClose + 1;
                            continue;
                        }
                    }
                }
                // Build replacement: arg, arg, ...
                std::string replacement = prefix + std::string("(") + arg;
                for (int i = 1; i < count; ++i) replacement += ", " + arg;
                replacement += ")";
                code.replace(pos, parenClose - pos + 1, replacement);
                pos += replacement.size();
            }
        }
    };
    ExpandVectorConstructors(preamble);
    ExpandVectorConstructors(body);

    // 6c. Replace dual-arg atan(y, x) with atan2(y, x) for HLSL/Metal compatibility
    // GLSL atan(y, x) is atan2(y, x) in HLSL/Metal. Single-arg atan(x) is unchanged.
    auto ReplaceAtanWithAtan2 = [](std::string& code) {
        size_t pos = 0;
        while ((pos = code.find("atan", pos)) != std::string::npos)
        {
            // Skip if it's already atan2
            if (pos + 4 < code.size() && code[pos + 4] == '2') { pos += 5; continue; }
            // Skip if part of a longer identifier (e.g. "atan" inside "atan2" or "myatan")
            if (pos > 0 && (isalnum(code[pos - 1]) || code[pos - 1] == '_')) { ++pos; continue; }
            if (pos + 4 < code.size() && code[pos + 4] != '(') { ++pos; continue; }
            size_t parenOpen = pos + 4;
            // Find matching closing paren
            int depth = 1;
            size_t parenClose = std::string::npos;
            for (size_t i = parenOpen + 1; i < code.size(); ++i)
            {
                if (code[i] == '(') ++depth;
                else if (code[i] == ')') { --depth; if (depth == 0) { parenClose = i; break; } }
            }
            if (parenClose == std::string::npos) { ++pos; continue; }
            // Count top-level commas to distinguish single vs dual arg
            int commaCount = 0;
            {
                int d = 0;
                for (size_t i = parenOpen + 1; i < parenClose; ++i)
                {
                    if (code[i] == '(') ++d;
                    else if (code[i] == ')') --d;
                    else if (code[i] == ',' && d == 0) ++commaCount;
                }
            }
            if (commaCount == 1)
            {
                // Dual-arg: atan(y, x) -> atan2(y, x)
                code.replace(pos, 4, "atan2");
                pos += 5; // skip past "atan2"
            }
            else
            {
                pos = parenClose + 1;
            }
        }
    };
    ReplaceAtanWithAtan2(preamble);
    ReplaceAtanWithAtan2(body);

    // 7. Build complete .sc source
    std::string result = BuildFragmentTemplate(preamble, varyings);

    if (!preamble.empty())
    {
        result += preamble;
        result += "\n";
    }

    result += "void main()\n{";
    result += body;
    result += "}\n";

    Rtt_LogException("=== TransformFragmentKernel generated .sc ===\n%s\n=== END .sc ===\n", result.c_str());
    return result;
}

// ----------------------------------------------------------------------------
// TransformVertexKernel
// ----------------------------------------------------------------------------

std::string BgfxShaderCompiler::TransformVertexKernel(const char* kernel,
                                                       const VaryingMapping& varyings)
{
    if (!kernel || !*kernel) return "";

    std::string src(kernel);
    src = StripPrecisionMacros(src);

    // Find VertexKernel function
    size_t funcPos = src.find("VertexKernel");
    if (funcPos == std::string::npos)
    {
        Rtt_LogException("TransformVertexKernel: no VertexKernel function found\n");
        return "";
    }

    // Extract preamble (code before VertexKernel declaration)
    std::string preamble;
    {
        size_t declStart = funcPos;
        if (declStart > 0)
        {
            size_t p = declStart - 1;
            while (p > 0 && (src[p] == ' ' || src[p] == '\t' || src[p] == '\n' || src[p] == '\r'))
                --p;
            while (p > 0 && (isalnum(src[p]) || src[p] == '_'))
                --p;
            if (!isalnum(src[p]) && src[p] != '_')
                ++p;
            declStart = p;
        }
        if (declStart > 0)
            preamble = src.substr(0, declStart);
    }

    // Extract parameter name
    size_t parenOpen = src.find('(', funcPos);
    size_t parenClose = src.find(')', parenOpen);
    if (parenOpen == std::string::npos || parenClose == std::string::npos) return "";

    std::string paramStr = src.substr(parenOpen + 1, parenClose - parenOpen - 1);
    std::string paramName;
    {
        std::istringstream iss(paramStr);
        std::string token;
        while (iss >> token) paramName = token;
    }
    if (paramName.empty()) paramName = "position";

    // Extract function body
    size_t bodyOpen = src.find('{', parenClose);
    if (bodyOpen == std::string::npos) return "";
    size_t bodyClose = FindMatchingBrace(src, bodyOpen);
    if (bodyClose == std::string::npos) return "";
    std::string body = src.substr(bodyOpen + 1, bodyClose - bodyOpen - 1);

    // Clean preamble: remove varying declarations, replace varying names
    preamble = RemoveVaryingDeclarations(preamble);
    preamble = ReplaceVaryingNames(preamble, varyings);

    // Replace varying names in body
    body = ReplaceVaryingNames(body, varyings);

    // Replace parameter name with _position (word-boundary safe)
    {
        size_t pos = 0;
        while ((pos = body.find(paramName, pos)) != std::string::npos)
        {
            bool leftOk = (pos == 0) || (!isalnum(body[pos - 1]) && body[pos - 1] != '_');
            size_t endPos = pos + paramName.size();
            bool rightOk = (endPos >= body.size()) || (!isalnum(body[endPos]) && body[endPos] != '_');
            if (leftOk && rightOk)
            {
                body.replace(pos, paramName.size(), "_position");
                pos += 9; // strlen("_position")
            }
            else
                pos += paramName.size();
        }
    }

    // Replace "return <expr>;" → "gl_Position = mul(u_ViewProjectionMatrix, vec4(<expr>, 0.0, 1.0));"
    {
        size_t pos = 0;
        while ((pos = body.find("return", pos)) != std::string::npos)
        {
            bool leftOk = (pos == 0) || (!isalnum(body[pos - 1]) && body[pos - 1] != '_');
            size_t afterReturn = pos + 6;
            bool rightOk = (afterReturn >= body.size()) || (!isalnum(body[afterReturn]) && body[afterReturn] != '_');
            if (leftOk && rightOk)
            {
                size_t semiPos = body.find(';', afterReturn);
                if (semiPos != std::string::npos)
                {
                    std::string expr = body.substr(afterReturn, semiPos - afterReturn);
                    // Trim whitespace
                    size_t fs = expr.find_first_not_of(" \t\n\r");
                    size_t ls = expr.find_last_not_of(" \t\n\r");
                    if (fs != std::string::npos)
                        expr = expr.substr(fs, ls - fs + 1);

                    std::string replacement = "gl_Position = mul(u_ViewProjectionMatrix, vec4("
                                              + expr + ", 0.0, 1.0));";
                    body.replace(pos, semiPos - pos + 1, replacement);
                    pos += replacement.size();
                }
                else
                    pos = afterReturn;
            }
            else
                pos = afterReturn;
        }
    }

    // Build $output line — collect all used v_Custom slots
    bool usedSlots[4] = {false, false, false, false};
    for (const auto& p : varyings)
    {
        int baseNum = 0;
        if (p.second.slot.size() > 8) baseNum = p.second.slot[8] - '0';
        if (p.second.arraySize > 0)
        {
            int slotsNeeded = (p.second.arraySize + 1) / 2;
            for (int s = 0; s < slotsNeeded && (baseNum + s) < 4; s++)
                usedSlots[baseNum + s] = true;
        }
        else
        {
            if (baseNum >= 0 && baseNum < 4) usedSlots[baseNum] = true;
        }
    }
    std::string outputLine = "$output v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2";
    for (int i = 0; i < 4; i++)
    {
        if (usedSlots[i])
        {
            char slot[16];
            snprintf(slot, sizeof(slot), ", v_Custom%d", i);
            outputLine += slot;
        }
    }

    // Build complete .sc source
    std::string result;
    result += "$input a_position, a_texcoord0, a_color0, a_texcoord1\n";
    result += outputLine + "\n";
    result += kBgfxShaderInline;
    result += "uniform mat4 u_ViewProjectionMatrix;\n";
    result += "uniform mat3 u_MaskMatrix0;\n";
    result += "uniform mat3 u_MaskMatrix1;\n";
    result += "uniform mat3 u_MaskMatrix2;\n";
    result += "uniform vec4 u_TotalTime;\n";
    result += "uniform vec4 u_DeltaTime;\n";
    result += "uniform vec4 u_TexelSize;\n";
    result += "uniform vec4 u_ContentScale;\n";
    result += "uniform vec4 u_ContentSize;\n";
    result += "\n";

    // User data uniforms (skip duplicates from preamble)
    for (int i = 0; i < kNumUserDataUniforms; ++i)
    {
        if (!PreambleDeclaresUniform(preamble, kUserDataUniformNames[i]))
            result += kUserDataUniformDecls[i];
    }
    result += "\n";

    result += "#define CoronaVertexUserData a_texcoord1\n";
    result += "#define CoronaTexCoord a_texcoord0.xy\n";
    result += "#define CoronaTotalTime u_TotalTime.x\n";
    result += "#define CoronaDeltaTime u_DeltaTime.x\n";
    result += "#define CoronaTexelSize u_TexelSize\n";
    result += "#define CoronaContentScale u_ContentScale.xy\n";
    result += "\n";
    result += "// GL attribute compatibility — map GL names to bgfx equivalents\n";
    result += "#define a_TexCoord a_texcoord0\n";
    result += "#define a_UserData a_texcoord1\n";
    result += "\n";

    // User preamble (helper functions, uniforms — varying decls already removed)
    if (!preamble.empty())
    {
        result += preamble;
        result += "\n";
    }

    result += "void main()\n{\n";
    result += "    // Standard varying passthrough\n";
    result += "    v_TexCoord = a_texcoord0.xyz;\n";
    result += "    v_ColorScale = a_color0;\n";
    result += "    v_UserData = a_texcoord1;\n";
    result += "    vec3 maskPos = vec3(a_position.xy, 1.0);\n";
    result += "    v_MaskUV0 = (mul(u_MaskMatrix0, maskPos)).xy;\n";
    result += "    v_MaskUV1 = (mul(u_MaskMatrix1, maskPos)).xy;\n";
    result += "    v_MaskUV2 = (mul(u_MaskMatrix2, maskPos)).xy;\n";
    result += "\n";

    // Initialize all used custom varying slots to zero
    for (int i = 0; i < 4; i++)
    {
        if (usedSlots[i])
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "    v_Custom%d = vec4(0.0, 0.0, 0.0, 0.0);\n", i);
            result += buf;
        }
    }

    result += "\n    // User vertex kernel\n";
    result += "    vec2 _position = a_position.xy;\n";
    result += body;
    result += "\n}\n";

    Rtt_LogException("=== TransformVertexKernel generated .sc ===\n%s\n=== END .sc ===\n", result.c_str());
    return result;
}

// ----------------------------------------------------------------------------
// Shader compilation via external shaderc binary
// ----------------------------------------------------------------------------

bool BgfxShaderCompiler::CompileShader(const std::string& scSource, char shaderType,
                                       std::vector<uint8_t>& outBinary, std::string& outError,
                                       const char* effectName)
{
    if (!IsAvailable())
    {
        outError = "shaderc binary not found or not executable";
        return false;
    }

    // Create temp directory
    const char* tmpDir = "/tmp/solar2d_shader_compile";
    mkdir(tmpDir, 0755);

    // Write .sc source to temp file — use effect name for easier debugging
    std::string nameTag = effectName ? effectName : "custom";
    std::string scPath = std::string(tmpDir) + "/" + nameTag + "." + shaderType + "s.sc";
    std::string binPath = std::string(tmpDir) + "/" + nameTag + "." + shaderType + "s.bin";

    {
        std::ofstream ofs(scPath);
        if (!ofs.is_open())
        {
            outError = "Failed to write temp .sc file: " + scPath;
            return false;
        }
        ofs << scSource;
    }

    // Build shaderc command
    std::string cmd = s_shadercPath;
    cmd += " -f " + scPath;
    cmd += " -o " + binPath;
    cmd += " --type ";
    cmd += shaderType;
#if defined(Rtt_ANDROID_ENV)
    cmd += " --platform android";
    cmd += " -p 320_es";
#elif defined(Rtt_IPHONE_ENV)
    cmd += " --platform ios";
    cmd += " -p metal";
#else
    cmd += " --platform osx";
    cmd += " -p metal";
#endif

    // Add include directories
    if (!s_bgfxIncludeDir.empty())
    {
        cmd += " -i " + s_bgfxIncludeDir;
    }

    // Add varying.def.sc path
    if (!s_varyingDefPath.empty())
    {
        cmd += " --varyingdef " + s_varyingDefPath;
    }

    // Capture stderr for error messages
    cmd += " 2>&1";

    // Execute shaderc
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        outError = "Failed to execute shaderc";
        return false;
    }

    std::string shadercOutput;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe))
    {
        shadercOutput += buffer;
    }
    int exitCode = pclose(pipe);

    if (exitCode != 0)
    {
        outError = "shaderc compilation failed:\n" + shadercOutput;
        // Keep .sc file for debugging (don't delete on failure)
        unlink(binPath.c_str());
        return false;
    }

    // Read compiled binary
    std::ifstream ifs(binPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        outError = "Failed to read compiled shader binary: " + binPath;
        unlink(scPath.c_str());
        return false;
    }

    size_t fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    outBinary.resize(fileSize);
    ifs.read(reinterpret_cast<char*>(outBinary.data()), fileSize);

    // Keep .sc and .bin files in /tmp/solar2d_shader_compile/ for debugging
    // (previously deleted — now retained for inspection)

    return true;
}

// ----------------------------------------------------------------------------
// Runtime binary construction (no shaderc needed)
// ----------------------------------------------------------------------------

// Parse uniform declarations from .sc source.
// Recognizes:
//   SAMPLER2D(name, reg);  → Sampler type, regIndex = reg
//   uniform vec4 name;     → Vec4 type
//   uniform mat3 name;     → Mat3 type
//   uniform mat4 name;     → Mat4 type
std::vector<BgfxShaderCompiler::UniformEntry>
BgfxShaderCompiler::ParseUniformsFromSc(const std::string& scSource)
{
    std::vector<UniformEntry> uniforms;
    std::istringstream iss(scSource);
    std::string line;
    uint16_t samplerIndex = 0;
    uint16_t vec4Index = 0;

    while (std::getline(iss, line))
    {
        // Skip comments and empty lines
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        if (line[first] == '/' && first + 1 < line.size() && line[first + 1] == '/') continue;
        if (line[first] == '#') continue;  // preprocessor

        // Check for SAMPLER2D(name, reg);
        size_t samplerPos = line.find("SAMPLER2D(");
        if (samplerPos == std::string::npos) samplerPos = line.find("SAMPLER3D(");
        if (samplerPos == std::string::npos) samplerPos = line.find("SAMPLERCUBE(");

        if (samplerPos != std::string::npos)
        {
            size_t parenOpen = line.find('(', samplerPos);
            size_t parenClose = line.find(')', parenOpen);
            if (parenOpen != std::string::npos && parenClose != std::string::npos)
            {
                std::string args = line.substr(parenOpen + 1, parenClose - parenOpen - 1);
                // Parse "name, reg"
                size_t comma = args.find(',');
                if (comma != std::string::npos)
                {
                    std::string name = args.substr(0, comma);
                    std::string regStr = args.substr(comma + 1);
                    // Trim whitespace
                    size_t ns = name.find_first_not_of(" \t");
                    size_t ne = name.find_last_not_of(" \t");
                    if (ns != std::string::npos) name = name.substr(ns, ne - ns + 1);
                    size_t rs = regStr.find_first_not_of(" \t");
                    size_t re = regStr.find_last_not_of(" \t");
                    if (rs != std::string::npos) regStr = regStr.substr(rs, re - rs + 1);

                    UniformEntry u;
                    u.name = name;
                    u.type = 0;  // Sampler
                    u.num = 1;
                    u.regIndex = (uint16_t)atoi(regStr.c_str());
                    u.regCount = 1;
                    uniforms.push_back(u);
                    if (u.regIndex >= samplerIndex) samplerIndex = u.regIndex + 1;
                }
            }
            continue;
        }

        // Check for "uniform <type> <name>;"
        size_t uniformPos = line.find("uniform");
        if (uniformPos == std::string::npos) continue;
        // Make sure it's a keyword boundary
        if (uniformPos > 0 && (isalnum(line[uniformPos - 1]) || line[uniformPos - 1] == '_')) continue;
        size_t afterUniform = uniformPos + 7;
        if (afterUniform >= line.size() || (line[afterUniform] != ' ' && line[afterUniform] != '\t')) continue;

        // Extract type and name
        std::string rest = line.substr(afterUniform);
        std::istringstream riss(rest);
        std::string typeStr, nameStr;
        riss >> typeStr >> nameStr;
        if (nameStr.empty()) continue;

        // Remove trailing semicolon
        if (!nameStr.empty() && nameStr.back() == ';')
            nameStr.pop_back();
        if (nameStr.empty()) continue;

        // Skip sampler2D — these are handled by SAMPLER2D() macro
        if (typeStr == "sampler2D" || typeStr == "sampler3D" || typeStr == "samplerCube")
            continue;

        UniformEntry u;
        u.name = nameStr;
        u.num = 1;

        if (typeStr == "vec4")      { u.type = 2; u.regCount = 1; }
        else if (typeStr == "vec3")  { u.type = 2; u.regCount = 1; } // bgfx packs as vec4
        else if (typeStr == "vec2")  { u.type = 2; u.regCount = 1; } // bgfx packs as vec4
        else if (typeStr == "float") { u.type = 2; u.regCount = 1; } // bgfx packs as vec4
        else if (typeStr == "mat3") { u.type = 3; u.regCount = 3; }
        else if (typeStr == "mat4") { u.type = 4; u.regCount = 4; }
        else continue;  // Skip truly unsupported types

        u.regIndex = vec4Index;
        vec4Index += u.regCount;
        uniforms.push_back(u);
    }

    return uniforms;
}

// GLSL-only macros for ESSL output. No Metal/HLSL code, no #if branching.
// This avoids issues with mobile GL drivers that choke on complex #else blocks.
static const char kEsslGlslMacros[] =
    "// --- GLSL compatibility macros ---\n"
    "#define SAMPLER2D(_name, _reg) uniform sampler2D _name\n"
    "#define SAMPLER3D(_name, _reg) uniform sampler3D _name\n"
    "#define SAMPLERCUBE(_name, _reg) uniform samplerCube _name\n"
    "#define mul(_a, _b) ((_a) * (_b))\n"
    "#define saturate(_x) clamp(_x, 0.0, 1.0)\n"
    "#define atan2(_x, _y) atan(_x, _y)\n"
    "#define texture2D(_sampler, _coord) texture(_sampler, _coord)\n"
    "#define texture2DLod(_sampler, _coord, _lod) textureLod(_sampler, _coord, _lod)\n"
    "#define texture2DLodOffset(_sampler, _coord, _lod, _offset) textureLodOffset(_sampler, _coord, _lod, _offset)\n"
    "#define texture2DBias(_sampler, _coord, _bias) texture(_sampler, _coord, _bias)\n"
    "// Safe fract: some GLES drivers return negative values for fract() on negative inputs.\n"
    "// Use explicit formula (x - floor(x)) which is always correct per GLSL spec.\n"
    "float _solar2d_fract(float x) { return x - floor(x); }\n"
    "vec2  _solar2d_fract(vec2  x) { return x - floor(x); }\n"
    "vec3  _solar2d_fract(vec3  x) { return x - floor(x); }\n"
    "vec4  _solar2d_fract(vec4  x) { return x - floor(x); }\n"
    "#define fract _solar2d_fract\n"
    "vec2 vec2_splat(float _x) { return vec2(_x, _x); }\n"
    "vec3 vec3_splat(float _x) { return vec3(_x, _x, _x); }\n"
    "vec4 vec4_splat(float _x) { return vec4(_x, _x, _x, _x); }\n"
    "float rcp(float _a) { return 1.0/_a; }\n"
    "vec2  rcp(vec2  _a) { return vec2(1.0)/_a; }\n"
    "vec3  rcp(vec3  _a) { return vec3(1.0)/_a; }\n"
    "vec4  rcp(vec4  _a) { return vec4(1.0)/_a; }\n"
    "// --- end GLSL compatibility macros ---\n"
    "\n";

// Transform .sc source to pure ESSL 300 for runtime use (Android/iOS OpenGL ES).
// Strategy: Strip kBgfxShaderInline (which has Metal/HLSL in #else blocks that
// confuse some mobile GL drivers) and replace with clean GLSL-only macros.
std::string BgfxShaderCompiler::TransformScToESSL(const std::string& scSource, char shaderType)
{
    std::string result;
    result.reserve(scSource.size() + 1024);

    // Start with #version so bgfx passes source directly to glShaderSource
    result += "#version 300 es\n";
    result += "precision highp float;\n";
    result += "precision mediump int;\n";
    result += "\n";

    // Map varying names to types
    auto getVaryingType = [](const std::string& name) -> std::string {
        if (name == "v_TexCoord") return "vec3";
        if (name == "v_ColorScale") return "vec4";
        if (name == "v_UserData") return "vec4";
        if (name.find("v_MaskUV") == 0) return "vec2";
        if (name.find("v_Custom") == 0) return "vec4";
        if (name == "a_position") return "vec4";
        if (name == "a_texcoord0") return "vec4";
        if (name == "a_color0") return "vec4";
        if (name == "a_texcoord1") return "vec4";
        return "vec4";
    };

    // Parse $input/$output and generate proper ESSL 300 in/out declarations.
    // Strip kBgfxShaderInline block (between marker comments) — replaced with kEsslGlslMacros.
    std::istringstream iss(scSource);
    std::string line;
    std::string varyingDecls;
    std::string bodyLines;
    bool inBgfxInline = false;

    while (std::getline(iss, line))
    {
        if (!line.empty() && line[0] == '$')
        {
            bool isInput = (line.find("$input") == 0);
            bool isOutput = (line.find("$output") == 0);
            if (isInput || isOutput)
            {
                size_t start = line.find(' ');
                if (start != std::string::npos)
                {
                    std::istringstream vss(line.substr(start));
                    std::string token;
                    while (std::getline(vss, token, ','))
                    {
                        size_t fs = token.find_first_not_of(" \t");
                        size_t ls = token.find_last_not_of(" \t\r");
                        if (fs != std::string::npos)
                        {
                            std::string name = token.substr(fs, ls - fs + 1);
                            if (isInput && (shaderType == 'V' || shaderType == 'v'))
                                varyingDecls += "in " + getVaryingType(name) + " " + name + ";\n";
                            else if (isInput)
                                varyingDecls += "in " + getVaryingType(name) + " " + name + ";\n";
                            else // isOutput (vertex shader)
                                varyingDecls += "out " + getVaryingType(name) + " " + name + ";\n";
                        }
                    }
                }
            }
            continue;
        }

        // Strip kBgfxShaderInline block (contains Metal/HLSL code in #else that
        // confuses Samsung and other mobile GL driver preprocessors)
        if (line.find("// --- bgfx shader compatibility (inlined) ---") != std::string::npos)
        {
            inBgfxInline = true;
            bodyLines += kEsslGlslMacros; // Replace with clean GLSL-only macros
            continue;
        }
        if (inBgfxInline)
        {
            if (line.find("// --- end bgfx shader compatibility ---") != std::string::npos)
                inBgfxInline = false;
            continue; // Skip all lines in the bgfx inline block
        }

        bodyLines += line + "\n";
    }

    // For fragment shaders: define fragment output and gl_FragColor macro
    if (shaderType == 'F' || shaderType == 'f')
    {
        varyingDecls += "out vec4 bgfx_FragColor;\n";
        varyingDecls += "#define gl_FragColor bgfx_FragColor\n";
    }

    // Promote uniform float/vec2/vec3 to vec4 in the ESSL source.
    // bgfx registers ALL non-sampler/matrix uniforms as Vec4 and uses glUniform4fv.
    // If the ESSL declares "uniform float x" but bgfx sets it with glUniform4fv,
    // some GL drivers (PowerVR/Samsung) silently drop the value.
    //
    // Strategy: change declaration to vec4, then replace all body references
    // with swizzled access (name.x / name.xy / name.xyz).
    // The uniform name stays the same so bgfx can find it via glGetUniformLocation.
    {
        struct PromotedUniform { std::string name; std::string swizzle; };
        std::vector<PromotedUniform> promoted;

        struct PatSwizzle { const char* pat; const char* swizzle; };
        PatSwizzle patterns[] = {
            { "uniform float ", ".x" },
            { "uniform vec2 ",  ".xy" },
            { "uniform vec3 ",  ".xyz" },
        };

        // Pass 1: find and promote declarations
        for (const auto& ps : patterns)
        {
            size_t pos = 0;
            size_t patLen = strlen(ps.pat);
            while ((pos = bodyLines.find(ps.pat, pos)) != std::string::npos)
            {
                size_t nameStart = pos + patLen;
                size_t semi = bodyLines.find(';', nameStart);
                if (semi == std::string::npos) { pos = nameStart; continue; }
                std::string raw = bodyLines.substr(nameStart, semi - nameStart);
                // Strip comments
                size_t cm = raw.find("//");
                if (cm != std::string::npos) raw = raw.substr(0, cm);
                // Trim
                size_t ns = raw.find_first_not_of(" \t");
                size_t ne = raw.find_last_not_of(" \t");
                if (ns == std::string::npos) { pos = semi + 1; continue; }
                std::string name = raw.substr(ns, ne - ns + 1);

                promoted.push_back({ name, ps.swizzle });

                // Replace declaration: "uniform float name;" → "uniform vec4 name;"
                std::string replacement = "uniform vec4 " + name + ";";
                bodyLines.replace(pos, semi - pos + 1, replacement);
                pos += replacement.size();
            }
        }

        // Pass 2: replace all references in body with swizzled access (word-boundary safe)
        for (const auto& pu : promoted)
        {
            const std::string& name = pu.name;
            std::string swizzled = name + pu.swizzle;
            size_t pos = 0;
            while ((pos = bodyLines.find(name, pos)) != std::string::npos)
            {
                // Skip if inside the "uniform vec4 name;" declaration itself
                // Check if preceded by "vec4 " (part of the declaration we just wrote)
                if (pos >= 5)
                {
                    std::string before5 = bodyLines.substr(pos - 5, 5);
                    if (before5 == "vec4 ") { pos += name.size(); continue; }
                }

                bool leftOk = (pos == 0) || (!isalnum(bodyLines[pos - 1]) && bodyLines[pos - 1] != '_');
                size_t endPos = pos + name.size();
                bool rightOk = (endPos >= bodyLines.size()) || (!isalnum(bodyLines[endPos]) && bodyLines[endPos] != '_' && bodyLines[endPos] != '.');
                // Skip if already swizzled (next char is '.')
                if (endPos < bodyLines.size() && bodyLines[endPos] == '.') { pos = endPos; continue; }

                if (leftOk && rightOk)
                {
                    bodyLines.replace(pos, name.size(), swizzled);
                    pos += swizzled.size();
                }
                else
                    pos += name.size();
            }
        }
    }

    // Insert varying declarations, then the body
    result += varyingDecls;
    result += "\n";
    result += bodyLines;

    return result;
}

// Extract hashIn/hashOut from a precompiled bgfx shader binary.
bool BgfxShaderCompiler::ExtractInterfaceHash(const unsigned char* data, size_t size,
                                               uint32_t& outHashIn, uint32_t& outHashOut)
{
    if (!data || size < 12) return false;

    uint32_t magic;
    memcpy(&magic, data, 4);

    // Validate magic: byte 1='S', byte 2='H'
    if (((magic >> 8) & 0xFF) != 'S' || ((magic >> 16) & 0xFF) != 'H')
        return false;

    memcpy(&outHashIn, data + 4, 4);

    // Version check: if version >= 6, hashOut is a separate field
    uint8_t version = (magic >> 24) & 0xFF;
    if (version >= 6)
    {
        memcpy(&outHashOut, data + 8, 4);
    }
    else
    {
        outHashOut = outHashIn;
    }

    return true;
}

// Construct a bgfx shader binary in-memory.
// Format: Magic(4) + HashIn(4) + HashOut(4) + UniformCount(2) + Uniforms[] + ShaderSize(4) + ShaderCode(N)
bool BgfxShaderCompiler::ConstructShaderBinary(
    const std::string& shaderSource,
    char shaderType,
    std::vector<uint8_t>& outBinary,
    uint32_t interfaceHash)
{
    // Parse uniforms from the source
    std::vector<UniformEntry> uniforms = ParseUniformsFromSc(shaderSource);

    // Build the ESSL source from the .sc
    std::string esslSource = TransformScToESSL(shaderSource, shaderType);
    if (esslSource.empty())
        return false;

    // Calculate binary size
    size_t binarySize = 4 + 4 + 4 + 2; // magic + hashIn + hashOut + uniformCount
    for (const auto& u : uniforms)
    {
        binarySize += 1 + u.name.size() + 1 + 1 + 2 + 2 + 2 + 2; // nameSize+name+type+num+regIndex+regCount+texInfo+texFormat
    }
    binarySize += 4 + esslSource.size(); // shaderSize + code

    outBinary.resize(binarySize);
    uint8_t* ptr = outBinary.data();

    // Helper lambdas for writing
    auto writeU8 = [&](uint8_t v) { *ptr++ = v; };
    auto writeU16 = [&](uint16_t v) { memcpy(ptr, &v, 2); ptr += 2; };
    auto writeU32 = [&](uint32_t v) { memcpy(ptr, &v, 4); ptr += 4; };
    auto writeBytes = [&](const void* data, size_t len) { memcpy(ptr, data, len); ptr += len; };

    // 1. Magic: [Type][S][H][version=11]
    uint32_t magic = ((uint32_t)shaderType) | ((uint32_t)'S' << 8) | ((uint32_t)'H' << 16) | ((uint32_t)11 << 24);
    writeU32(magic);

    // 2. HashIn — for FS, must match the VS's hashOut (varying interface hash).
    //    For VS, set to 0 (VS hashIn is not checked against anything).
    if (shaderType == 'F' || shaderType == 'f')
        writeU32(interfaceHash);  // FS hashIn = VS hashOut
    else
        writeU32(0);

    // 3. HashOut (version >= 6) — for VS, must match the FS's hashIn.
    //    For FS, set to 0 (FS hashOut is not checked against anything).
    if (shaderType == 'V' || shaderType == 'v')
        writeU32(interfaceHash);  // VS hashOut = FS hashIn
    else
        writeU32(0);

    // 4. Uniform count
    writeU16((uint16_t)uniforms.size());

    // 5. Uniforms
    for (const auto& u : uniforms)
    {
        uint8_t nameLen = (uint8_t)u.name.size();
        writeU8(nameLen);
        writeBytes(u.name.data(), nameLen);
        writeU8(u.type);
        writeU8(u.num);
        writeU16(u.regIndex);
        writeU16(u.regCount);
        writeU16(0); // texInfo (version >= 8)
        writeU16(0); // texFormat (version >= 10)
    }

    // 6. Shader code size
    uint32_t codeSize = (uint32_t)esslSource.size();
    writeU32(codeSize);

    // 7. Shader code (plain text ESSL)
    writeBytes(esslSource.data(), codeSize);

    Rtt_LogException("ConstructShaderBinary: type='%c', %d uniforms, %u bytes ESSL, %zu bytes total\n",
                     shaderType, (int)uniforms.size(), codeSize, outBinary.size());

    // Dump ESSL source for debugging (always, since this is the primary diagnostic for link failures)
    Rtt_LogException("=== Runtime ESSL (%c) ===\n%s\n=== END ESSL ===\n", shaderType, esslSource.c_str());

    // Dump uniform table
    for (size_t i = 0; i < uniforms.size(); ++i)
    {
        const UniformEntry& u = uniforms[i];
        const char* typeNames[] = { "Sampler", "End", "Vec4", "Mat3", "Mat4" };
        const char* typeName = (u.type < 5) ? typeNames[u.type] : "Unknown";
        Rtt_LogException("  uniform[%d]: '%s' type=%s(%d) num=%d reg=%d regCount=%d\n",
                         (int)i, u.name.c_str(), typeName, u.type, u.num, u.regIndex, u.regCount);
    }

    return true;
}

// ----------------------------------------------------------------------------
// CompileCustomEffect
// ----------------------------------------------------------------------------

bool BgfxShaderCompiler::CompileCustomEffect(const char* category, const char* name,
                                              const char* kernelFrag, const char* kernelVert,
                                              std::string& outError)
{
    if (!kernelFrag || !*kernelFrag)
    {
        outError = "No fragment kernel source provided";
        return false;
    }

    // Parse custom varyings from both kernels (shared mapping for consistency)
    VaryingMapping varyings = ParseCustomVaryings(kernelVert, kernelFrag);

    // Transform fragment kernel → .sc format (with varying mapping)
    std::string fragSc = TransformFragmentKernel(kernelFrag, varyings);
    if (fragSc.empty())
    {
        outError = "Failed to transform fragment kernel to .sc format";
        return false;
    }

    char effectTag[256];
    snprintf(effectTag, sizeof(effectTag), "%s_%s", category, name);

    std::vector<uint8_t> fsBinary;
    bool useShadercPath = IsAvailable();

    // Check environment variable to force runtime binary path (for testing)
    const char* forceRuntime = getenv("SOLAR2D_FORCE_RUNTIME_SHADER");
    if (forceRuntime && (strcmp(forceRuntime, "1") == 0 || strcmp(forceRuntime, "yes") == 0))
        useShadercPath = false;

    if (useShadercPath)
    {
        // Path A: shaderc available (macOS dev) — compile via external binary
        std::string fsError;
        if (!CompileShader(fragSc, 'f', fsBinary, fsError, effectTag))
        {
            outError = "Fragment shader compilation failed: " + fsError;
            return false;
        }
    }
    else
    {
        // Path B: no shaderc (Android/iOS) — construct binary in-memory
        Rtt_LogException("Using runtime shader binary construction for '%s.%s' (no shaderc)\n",
                         category, name);

        // Extract the varying interface hash from the default VS binary.
        // bgfx validates: VS.hashOut == FS.hashIn. Our runtime FS must carry
        // the same hashIn as the precompiled default VS's hashOut, otherwise
        // bgfx::createProgram rejects the VS+FS pair silently.
        uint32_t vsHashIn = 0, vsHashOut = 0;
        bool hasCustomVS = (kernelVert && *kernelVert);
        if (!hasCustomVS)
        {
            // No custom VS → will pair with precompiled default VS
            ExtractInterfaceHash(BgfxProgram::GetDefaultVSData(), BgfxProgram::GetDefaultVSSize(),
                                 vsHashIn, vsHashOut);
            Rtt_LogException("Default VS interface hash: hashIn=0x%08x, hashOut=0x%08x\n",
                             vsHashIn, vsHashOut);
        }

        if (!ConstructShaderBinary(fragSc, 'F', fsBinary, vsHashOut))
        {
            outError = "Failed to construct fragment shader binary for " + std::string(effectTag);
            return false;
        }
    }

    // Cache the compiled fragment shader
    char fsKey[256];
    snprintf(fsKey, sizeof(fsKey), "fs_%s_%s.bin", category, name);
    CacheCompiledShader(fsKey, fsBinary);

    // Compile/construct custom vertex shader if provided
    if (kernelVert && *kernelVert)
    {
        std::string vertSc = TransformVertexKernel(kernelVert, varyings);
        if (!vertSc.empty())
        {
            std::vector<uint8_t> vsBinary;

            if (useShadercPath)
            {
                std::string vsError;
                if (CompileShader(vertSc, 'v', vsBinary, vsError, effectTag))
                {
                    char vsKey[256];
                    snprintf(vsKey, sizeof(vsKey), "vs_%s_%s.bin", category, name);
                    CacheCompiledShader(vsKey, vsBinary);
                }
                else
                {
                    Rtt_LogException("WARNING: Custom vertex shader compilation failed for '%s.%s': %s\n"
                                     "Falling back to default vertex shader.\n",
                                     category, name, vsError.c_str());
                }
            }
            else
            {
                // For custom VS+FS pair, extract the FS's hashIn from our just-constructed FS binary
                // so VS.hashOut matches. (When both are runtime, use the same hash.)
                uint32_t fsHashIn = 0, fsHashOut = 0;
                ExtractInterfaceHash(fsBinary.data(), fsBinary.size(), fsHashIn, fsHashOut);
                if (ConstructShaderBinary(vertSc, 'V', vsBinary, fsHashIn))
                {
                    char vsKey[256];
                    snprintf(vsKey, sizeof(vsKey), "vs_%s_%s.bin", category, name);
                    CacheCompiledShader(vsKey, vsBinary);
                }
                else
                {
                    Rtt_LogException("WARNING: Runtime VS binary construction failed for '%s.%s'\n"
                                     "Falling back to default vertex shader.\n",
                                     category, name);
                }
            }
        }
    }

    Rtt_LogException("Custom effect '%s.%s' compiled successfully (%s path)\n",
                     category, name, useShadercPath ? "shaderc" : "runtime");
    return true;
}

// ----------------------------------------------------------------------------
// Cache management
// ----------------------------------------------------------------------------

bool BgfxShaderCompiler::FindCachedShader(const char* key, const unsigned char*& outData, size_t& outSize)
{
    auto it = s_cache.find(key);
    if (it == s_cache.end()) return false;
    outData = it->second.data();
    outSize = it->second.size();
    return true;
}

void BgfxShaderCompiler::CacheCompiledShader(const char* key, const std::vector<uint8_t>& data)
{
    s_cache[key] = data;
}

void BgfxShaderCompiler::EvictCompiledShader(const char* key)
{
    s_cache.erase(key);
}

void BgfxShaderCompiler::ClearCache()
{
    s_cache.clear();
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------


#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
