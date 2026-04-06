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

// The bgfx .sc fragment shader template for custom effects.
// Split into header (always included) and user data uniforms (conditionally included).
// User data uniforms are skipped when the preamble already declares them,
// because the preamble may use different types (e.g. vec2 instead of vec4).
static const char kFragmentScInputBase[] =
    "$input v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2";

static const char kFragmentScHeaderBody[] =
    "\n"
    "#include <bgfx_shader.sh>\n"
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
    // Append custom varying slots in order (v_Custom0, v_Custom1, ...)
    for (int i = 0; i < 4; i++)
    {
        char slot[16];
        snprintf(slot, sizeof(slot), "v_Custom%d", i);
        for (const auto& p : varyings)
        {
            if (p.second.slot == slot)
            {
                line += ", ";
                line += slot;
                break;
            }
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

// Helper: replace user varying names with v_CustomN slots (word-boundary safe)
static std::string ReplaceVaryingNames(const std::string& src, const VaryingMapping& varyings)
{
    std::string result = src;
    for (const auto& pair : varyings)
    {
        const std::string& varName = pair.first;
        const std::string& slotName = pair.second.slot;
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
    return result;
}

// Build the complete template, skipping user data uniforms that preamble already declares.
static std::string BuildFragmentTemplate(const std::string& preamble,
                                          const VaryingMapping& varyings = VaryingMapping())
{
    std::string result = BuildFragmentInputLine(varyings) + "\n";
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
    const char* macros[] = { "P_COLOR ", "P_UV ", "P_DEFAULT ", "P_POSITION ", "P_NORMAL ", "P_RANDOM " };
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

            if (!name.empty() && mapping.find(name) == mapping.end())
            {
                if (slotIndex >= 4)
                {
                    Rtt_LogException("WARNING: Too many custom varyings (max 4), skipping '%s'\n", name.c_str());
                }
                else
                {
                    char slot[16];
                    snprintf(slot, sizeof(slot), "v_Custom%d", slotIndex);
                    mapping[name] = { type, std::string(slot) };
                    ++slotIndex;
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
        std::string localDecl = "vec2 " + localVar + " = v_TexCoord.xy;\n";
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

    // 5. Replace "return <expr>;" with "gl_FragColor = <expr>;"
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
                    std::string replacement = "gl_FragColor =" + expr + ";";
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
    if (!varyings.empty())
    {
        preamble = RemoveVaryingDeclarations(preamble);
        preamble = ReplaceVaryingNames(preamble, varyings);
        body = ReplaceVaryingNames(body, varyings);
    }

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

    // Build $output line
    std::string outputLine = "$output v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2";
    for (int i = 0; i < 4; i++)
    {
        char slot[16];
        snprintf(slot, sizeof(slot), "v_Custom%d", i);
        for (const auto& p : varyings)
        {
            if (p.second.slot == slot)
            {
                outputLine += ", ";
                outputLine += slot;
                break;
            }
        }
    }

    // Build complete .sc source
    std::string result;
    result += "$input a_position, a_texcoord0, a_color0, a_texcoord1\n";
    result += outputLine + "\n";
    result += "\n#include <bgfx_shader.sh>\n\n";
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

    // User preamble (helper functions, uniforms — varying decls already removed)
    if (!preamble.empty())
    {
        result += preamble;
        result += "\n";
    }

    result += "void main()\n{\n";
    result += "    // Standard varying passthrough\n";
    result += "    v_TexCoord = vec3(a_texcoord0.xy, 0.0);\n";
    result += "    v_ColorScale = a_color0;\n";
    result += "    v_UserData = a_texcoord1;\n";
    result += "    vec3 maskPos = vec3(a_position.xy, 1.0);\n";
    result += "    v_MaskUV0 = (mul(u_MaskMatrix0, maskPos)).xy;\n";
    result += "    v_MaskUV1 = (mul(u_MaskMatrix1, maskPos)).xy;\n";
    result += "    v_MaskUV2 = (mul(u_MaskMatrix2, maskPos)).xy;\n";
    result += "\n";

    // Initialize custom varyings to default
    for (int i = 0; i < 4; i++)
    {
        char slot[16];
        snprintf(slot, sizeof(slot), "v_Custom%d", i);
        for (const auto& p : varyings)
        {
            if (p.second.slot == slot)
            {
                result += "    " + std::string(slot) + " = vec4(0.0, 0.0, 0.0, 0.0);\n";
                break;
            }
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
    cmd += " --platform osx";
    cmd += " -p metal";

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

    // Compile fragment shader — pass effect name for debug file naming
    std::vector<uint8_t> fsBinary;
    std::string fsError;
    char effectTag[256];
    snprintf(effectTag, sizeof(effectTag), "%s_%s", category, name);
    if (!CompileShader(fragSc, 'f', fsBinary, fsError, effectTag))
    {
        outError = "Fragment shader compilation failed: " + fsError;
        return false;
    }

    // Cache the compiled fragment shader
    char fsKey[256];
    snprintf(fsKey, sizeof(fsKey), "fs_%s_%s.bin", category, name);
    CacheCompiledShader(fsKey, fsBinary);

    // Compile custom vertex shader if provided
    if (kernelVert && *kernelVert)
    {
        std::string vertSc = TransformVertexKernel(kernelVert, varyings);
        if (!vertSc.empty())
        {
            std::vector<uint8_t> vsBinary;
            std::string vsError;
            if (CompileShader(vertSc, 'v', vsBinary, vsError, effectTag))
            {
                char vsKey[256];
                snprintf(vsKey, sizeof(vsKey), "vs_%s_%s.bin", category, name);
                CacheCompiledShader(vsKey, vsBinary);
            }
            else
            {
                // VS compilation failure is non-fatal — default VS will be used
                Rtt_LogException("WARNING: Custom vertex shader compilation failed for '%s.%s': %s\n"
                                 "Falling back to default vertex shader.\n",
                                 category, name, vsError.c_str());
            }
        }
    }

    Rtt_LogException("Custom effect '%s.%s' compiled successfully for bgfx/Metal\n", category, name);
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
