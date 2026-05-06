#ifndef _Rtt_BgfxShaderCacheKey_H__
#define _Rtt_BgfxShaderCacheKey_H__

// Single source of truth for the runtime shader cache version suffix.
// Bump this when shader compilation pipeline changes invalidate old binaries.
// Used by both Rtt_BgfxShaderCompiler.cpp and Rtt_BgfxProgram.cpp.
//
// History:
//   v8 — 008 mask-PV: vertex layout grew from 44 to 68 bytes (added
//        TexCoord2/3/4 mask UV slots). Effect shaders runtime-compiled under
//        the old layout would map attributes wrong, so invalidate the cache.
#define BGFX_RUNTIME_SHADER_CACHE_VERSION "v8"

#endif // _Rtt_BgfxShaderCacheKey_H__
