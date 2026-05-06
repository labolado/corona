$input a_position, a_texcoord0, a_color0, a_texcoord1, a_texcoord2, a_texcoord3
$output v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2

#define MASK_COUNT 2
#include "vs_default_kernel.sh"
