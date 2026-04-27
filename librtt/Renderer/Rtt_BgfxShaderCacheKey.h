#ifndef _Rtt_BgfxShaderCacheKey_H__
#define _Rtt_BgfxShaderCacheKey_H__

// Single source of truth for the runtime shader cache version suffix.
// Bump this when shader compilation pipeline changes invalidate old binaries.
// Used by both Rtt_BgfxShaderCompiler.cpp and Rtt_BgfxProgram.cpp.
#define BGFX_RUNTIME_SHADER_CACHE_VERSION "v7"

#endif // _Rtt_BgfxShaderCacheKey_H__
