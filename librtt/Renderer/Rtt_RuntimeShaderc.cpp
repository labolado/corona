#include "Core/Rtt_Config.h"

#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )

#include <string>
#include <vector>
#include <cstring>
#include <ostream>

#include <bx/bx.h>
#include <bx/file.h>
#include <spirv-tools/libspirv.h>

#define SHADERC_CONFIG_HAS_D3DCOMPILER 0
#define SHADERC_CONFIG_HAS_DXC 0
#define SHADERC_CONFIG_HAS_TINT 0
#define SHADERC_CONFIG_HAS_GLSL_OPTIMIZER 0
#define SHADERC_CONFIG_HAS_GLSLANG 1
#define ENABLE_HLSL 1  // Required for glslang to recognize EShSourceHlsl (used by SPIRV path)

#include "external/bgfx/tools/shaderc/shaderc.h"

namespace bgfx
{

namespace
{

bool writeBackendStub(const char* backend, bx::WriterI* messageWriter)
{
    writef(messageWriter, "%s backend not compiled in runtime wrapper\n", backend);
    return false;
}

} // namespace

bool compileGLSLShader(const Options&, uint32_t, const std::string&, bx::WriterI*, bx::WriterI* messageWriter)
{
    return writeBackendStub("GLSL", messageWriter);
}

bool compileHLSLShader(const Options&, uint32_t, const std::string&, bx::WriterI*, bx::WriterI* messageWriter)
{
    return writeBackendStub("HLSL", messageWriter);
}

bool compileDxilShader(const Options&, uint32_t, const std::string&, bx::WriterI*, bx::WriterI* messageWriter)
{
    return writeBackendStub("DXIL", messageWriter);
}

bool compileMetalShader(const Options&, uint32_t, const std::string&, bx::WriterI*, bx::WriterI* messageWriter)
{
    return writeBackendStub("Metal", messageWriter);
}

bool compilePSSLShader(const Options&, uint32_t, const std::string&, bx::WriterI*, bx::WriterI* messageWriter)
{
    return writeBackendStub("PSSL", messageWriter);
}

bool compileWgslShader(const Options&, uint32_t, const std::string&, bx::WriterI*, bx::WriterI* messageWriter)
{
    return writeBackendStub("WGSL", messageWriter);
}

const char* getPsslPreamble()
{
    return "";
}

} // namespace bgfx

namespace glslang
{

void SpirvToolsDisassemble(std::ostream&, const std::vector<unsigned int>&, spv_target_env)
{
}

} // namespace glslang

#define TinyStlAllocator RuntimeShadercTinyStlAllocator
#define fatal runtimeShadercFatal
#define trace runtimeShadercTrace
#define getUniformTypeName runtimeShadercGetUniformTypeName
#define nameToUniformTypeEnum runtimeShadercNameToUniformTypeEnum
#define s_uniformTypeName runtimeShadercUniformTypeName
#define main shaderc_main
#include "external/bgfx/tools/shaderc/shaderc.cpp"
#undef main

#define g_allocator g_shaderc_allocator
#include "external/bgfx/tools/shaderc/shaderc_spirv.cpp"
#undef g_allocator
#undef s_uniformTypeName
#undef nameToUniformTypeEnum
#undef getUniformTypeName
#undef trace
#undef fatal
#undef TinyStlAllocator

namespace Rtt
{

namespace
{

class VectorWriter : public bx::WriterI
{
public:
    explicit VectorWriter(std::vector<uint8_t>& out)
    : fOut(out)
    {
        fOut.clear();
    }

    virtual int32_t write(const void* data, int32_t size, bx::Error* err) override
    {
        BX_UNUSED(err);
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        fOut.insert(fOut.end(), bytes, bytes + size);
        return size;
    }

private:
    std::vector<uint8_t>& fOut;
};

class StringWriter : public bx::WriterI
{
public:
    explicit StringWriter(std::string& out)
    : fOut(out)
    {
        fOut.clear();
    }

    virtual int32_t write(const void* data, int32_t size, bx::Error* err) override
    {
        BX_UNUSED(err);
        fOut.append(static_cast<const char*>(data), static_cast<size_t>(size));
        return size;
    }

private:
    std::string& fOut;
};

} // namespace

bool compileShaderRuntime(const std::string& sourcePath,
                          const std::string& sourceText,
                          const std::string& varyingText,
                          char shaderType,
                          const std::vector<std::string>& includeDirs,
                          const std::string& defines,
                          std::vector<uint8_t>& outBinary,
                          std::string& outLog)
{
    bgfx::Options options;
    options.shaderType = shaderType;
    options.platform = "android";
    options.profile = "spirv";
    options.inputFilePath = sourcePath.c_str();
    options.raw = false;
    options.debugInformation = false;

    for (const std::string& includeDir : includeDirs)
    {
        if (!includeDir.empty())
        {
            options.includeDirs.push_back(includeDir.c_str());
        }
    }

    if (!defines.empty())
    {
        options.defines.push_back(defines);
    }

    const uint32_t sourceLen = static_cast<uint32_t>(sourceText.size());
    const size_t bufferSize = static_cast<size_t>(sourceLen) + 16384 + 1;
    char* shaderBuffer = new char[bufferSize];
    std::memcpy(shaderBuffer, sourceText.data(), sourceLen);
    shaderBuffer[sourceLen] = '\n';
    std::memset(shaderBuffer + sourceLen + 1, 0, bufferSize - sourceLen - 1);

    VectorWriter shaderWriter(outBinary);
    StringWriter messageWriter(outLog);

    bool result = bgfx::compileShader(varyingText.c_str(),
                               "",
                               shaderBuffer,
                               sourceLen,
                               options,
                               &shaderWriter,
                               &messageWriter);
    // NOTE: do NOT delete[] shaderBuffer — bgfx::compileShader takes
    // ownership and frees it internally (shaderc.cpp:1605-1614).
    return result;
}

} // namespace Rtt

#endif // !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )
