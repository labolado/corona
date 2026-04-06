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
// This matches the structure used by built-in effects like fs_filter_brightness.sc.
static const char kFragmentScTemplate[] =
    "$input v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2\n"
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
    "\n"
    "// User data uniforms\n"
    "uniform vec4 u_UserData0;\n"
    "uniform vec4 u_UserData1;\n"
    "uniform vec4 u_UserData2;\n"
    "uniform vec4 u_UserData3;\n"
    "\n"
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

std::string BgfxShaderCompiler::TransformFragmentKernel(const char* kernel)
{
    if (!kernel || !*kernel) return "";

    std::string src(kernel);

    // 1. Strip precision macros
    src = StripPrecisionMacros(src);

    // 2. Find FragmentKernel function signature
    //    Pattern: "vec4 FragmentKernel( vec2 <paramName> )"
    //    or: "vec4 FragmentKernel(vec2 <paramName>)"
    size_t funcPos = src.find("FragmentKernel");
    if (funcPos == std::string::npos)
    {
        // No FragmentKernel function — return raw source wrapped in main()
        // This handles edge cases where the kernel is already in main() format
        std::string result = kFragmentScTemplate;
        result += "void main()\n{\n";
        result += src;
        result += "\n}\n";
        return result;
    }

    // 2b. Extract any code before FragmentKernel (helper functions, constants, etc.)
    //     Walk back from funcPos to find the return type ("vec4") to get the real start
    std::string preamble;
    {
        size_t declStart = funcPos;
        // Walk back over whitespace and the return type
        if (declStart > 0)
        {
            size_t pos = declStart - 1;
            // Skip whitespace
            while (pos > 0 && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r'))
                --pos;
            // Find start of return type token (e.g. "vec4")
            size_t tokenEnd = pos + 1;
            while (pos > 0 && (isalnum(src[pos]) || src[pos] == '_'))
                --pos;
            if (!isalnum(src[pos]) && src[pos] != '_')
                ++pos;
            declStart = pos;
        }
        if (declStart > 0)
        {
            preamble = src.substr(0, declStart);
        }
    }

    // Find the opening parenthesis of the parameter list
    size_t parenOpen = src.find('(', funcPos);
    size_t parenClose = src.find(')', parenOpen);
    if (parenOpen == std::string::npos || parenClose == std::string::npos)
        return "";

    // Extract parameter name (the last word before ')')
    std::string paramStr = src.substr(parenOpen + 1, parenClose - parenOpen - 1);
    // paramStr is like "vec2 texCoord" or " vec2 uv "
    // Find the last token
    std::string paramName;
    {
        std::istringstream iss(paramStr);
        std::string token;
        while (iss >> token) { paramName = token; }
    }
    if (paramName.empty()) paramName = "texCoord";

    // 3. Find the function body (between { and })
    size_t bodyOpen = src.find('{', parenClose);
    if (bodyOpen == std::string::npos) return "";

    size_t bodyClose = FindMatchingBrace(src, bodyOpen);
    if (bodyClose == std::string::npos) return "";

    std::string body = src.substr(bodyOpen + 1, bodyClose - bodyOpen - 1);

    // 4. Declare a local variable initialized from the varying, then replace
    //    the parameter name with it. This avoids writing to the varying input
    //    (v_TexCoord) which can fail on Metal shaders.
    //    Use word boundary replacement to avoid replacing substrings.
    {
        std::string localVar = "_" + paramName;
        std::string localDecl = "vec2 " + localVar + " = v_TexCoord.xy;\n";
        body = "\n    " + localDecl + body;

        size_t pos = localDecl.size() + 5; // skip past the declaration we just inserted
        while ((pos = body.find(paramName, pos)) != std::string::npos)
        {
            // Check word boundaries
            bool leftBoundary = (pos == 0) || (!isalnum(body[pos - 1]) && body[pos - 1] != '_');
            size_t endPos = pos + paramName.size();
            bool rightBoundary = (endPos >= body.size()) || (!isalnum(body[endPos]) && body[endPos] != '_');

            if (leftBoundary && rightBoundary)
            {
                body.replace(pos, paramName.size(), localVar);
                pos += localVar.size();
            }
            else
            {
                pos += paramName.size();
            }
        }
    }

    // 5. Replace "return <expr>;" with "gl_FragColor = <expr>;"
    {
        size_t pos = 0;
        while ((pos = body.find("return", pos)) != std::string::npos)
        {
            // Check word boundaries
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
                {
                    pos = afterReturn;
                }
            }
            else
            {
                pos = afterReturn;
            }
        }
    }

    // 6. Build the complete .sc source
    std::string result = kFragmentScTemplate;

    // Include helper functions/constants defined before FragmentKernel
    if (!preamble.empty())
    {
        result += preamble;
        result += "\n";
    }

    result += "void main()\n{";
    result += body;
    result += "}\n";

    // Diagnostic: print the generated .sc code
    Rtt_LogException("=== TransformFragmentKernel generated .sc ===\n%s\n=== END .sc ===\n", result.c_str());

    return result;
}

std::string BgfxShaderCompiler::TransformVertexKernel(const char* kernel)
{
    // For now, custom vertex kernels are not supported in bgfx mode.
    // The default vertex shader will be used.
    (void)kernel;
    return "";
}

// ----------------------------------------------------------------------------
// Shader compilation via external shaderc binary
// ----------------------------------------------------------------------------

bool BgfxShaderCompiler::CompileShader(const std::string& scSource, char shaderType,
                                       std::vector<uint8_t>& outBinary, std::string& outError)
{
    if (!IsAvailable())
    {
        outError = "shaderc binary not found or not executable";
        return false;
    }

    // Create temp directory
    const char* tmpDir = "/tmp/solar2d_shader_compile";
    mkdir(tmpDir, 0755);

    // Write .sc source to temp file
    std::string scPath = std::string(tmpDir) + "/custom_shader." + shaderType + "s.sc";
    std::string binPath = std::string(tmpDir) + "/custom_shader." + shaderType + "s.bin";

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
        // Clean up temp files
        unlink(scPath.c_str());
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

    // Clean up temp files
    unlink(scPath.c_str());
    unlink(binPath.c_str());

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

    // Transform fragment kernel → .sc format
    std::string fragSc = TransformFragmentKernel(kernelFrag);
    if (fragSc.empty())
    {
        outError = "Failed to transform fragment kernel to .sc format";
        return false;
    }

    // Compile fragment shader
    std::vector<uint8_t> fsBinary;
    std::string fsError;
    if (!CompileShader(fragSc, 'f', fsBinary, fsError))
    {
        outError = "Fragment shader compilation failed: " + fsError;
        return false;
    }

    // Cache the compiled fragment shader
    char fsKey[256];
    snprintf(fsKey, sizeof(fsKey), "fs_%s_%s.bin", category, name);
    CacheCompiledShader(fsKey, fsBinary);

    // For vertex shader: use default (don't compile custom)
    // The default VS binary from the embedded table will be used
    // Only compile custom VS if provided and different from default
    if (kernelVert && *kernelVert)
    {
        std::string vertSc = TransformVertexKernel(kernelVert);
        if (!vertSc.empty())
        {
            std::vector<uint8_t> vsBinary;
            std::string vsError;
            if (CompileShader(vertSc, 'v', vsBinary, vsError))
            {
                char vsKey[256];
                snprintf(vsKey, sizeof(vsKey), "vs_%s_%s.bin", category, name);
                CacheCompiledShader(vsKey, vsBinary);
            }
            // VS compilation failure is non-fatal — default VS will be used
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
