#ifndef _Rtt_BgfxMetalReadback_H__
#define _Rtt_BgfxMetalReadback_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Synchronously read texture pixels using Metal API.
/// Returns true on success, false if Metal is not available or readback fails.
/// @param nativeTexPtr  Native Metal texture pointer (id<MTLTexture>), obtained via bgfx::overrideInternal
/// @param x, y, w, h   Region to read
/// @param outBuffer     Output buffer, must be at least w*h*4 bytes (RGBA8)
bool BgfxMetal_ReadTextureSync(
    void* nativeTexPtr,
    uint32_t x, uint32_t y, uint32_t w, uint32_t h,
    void* outBuffer );

#ifdef __cplusplus
}
#endif

#endif // _Rtt_BgfxMetalReadback_H__
