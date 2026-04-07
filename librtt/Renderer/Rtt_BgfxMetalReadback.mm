#import <Metal/Metal.h>
#import "Rtt_BgfxMetalReadback.h"

bool BgfxMetal_ReadTextureSync(
    void* nativeTexPtr,
    uint32_t x, uint32_t y, uint32_t w, uint32_t h,
    void* outBuffer )
{
    if( !nativeTexPtr || !outBuffer || w == 0 || h == 0 )
    {
        return false;
    }

    id<MTLTexture> texture = (__bridge id<MTLTexture>)nativeTexPtr;
    if( !texture )
    {
        return false;
    }

    // For managed/private textures on Apple Silicon (UMA),
    // getBytes works directly without a GPU blit.
    MTLRegion region = MTLRegionMake2D(x, y, w, h);
    NSUInteger bytesPerRow = w * 4; // RGBA8 = 4 bytes per pixel

    @try
    {
        [texture getBytes:outBuffer
              bytesPerRow:bytesPerRow
               fromRegion:region
              mipmapLevel:0];
    }
    @catch( NSException* e )
    {
        NSLog(@"BgfxMetal_ReadTextureSync: exception %@", e);
        return false;
    }

    return true;
}
