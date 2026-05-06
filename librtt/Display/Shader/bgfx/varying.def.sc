vec3 v_TexCoord   : TEXCOORD5 = vec3(0.0, 0.0, 0.0);
vec4 v_ColorScale : COLOR0    = vec4(1.0, 1.0, 1.0, 1.0);
vec4 v_UserData   : TEXCOORD6 = vec4(0.0, 0.0, 0.0, 0.0);
vec2 v_MaskUV0    : TEXCOORD2 = vec2(0.0, 0.0);
vec2 v_MaskUV1    : TEXCOORD3 = vec2(0.0, 0.0);
vec2 v_MaskUV2    : TEXCOORD4 = vec2(0.0, 0.0);

vec2 v_feathering_edges            : TEXCOORD7  = vec2(0.0, 0.0);
vec2 v_fromPos                     : TEXCOORD8  = vec2(0.0, 0.0);
vec2 v_N                           : TEXCOORD9  = vec2(0.0, 0.0);
vec2 v_slot_size                   : TEXCOORD10 = vec2(0.0, 0.0);
vec2 v_sample_uv_offset            : TEXCOORD11 = vec2(0.0, 0.0);
vec3 v_transform0                  : TEXCOORD12 = vec3(0.0, 0.0, 0.0);
vec3 v_transform1                  : TEXCOORD13 = vec3(0.0, 0.0, 0.0);
vec3 v_transform2                  : TEXCOORD14 = vec3(0.0, 0.0, 0.0);
float v_minimum_full_radius        : TEXCOORD15 = 0.0;
vec2 v_opennessOffsetMatrix0       : TEXCOORD16 = vec2(0.0, 0.0);
vec2 v_opennessOffsetMatrix1       : TEXCOORD17 = vec2(0.0, 0.0);
vec2 v_feathering_edges_radians    : TEXCOORD18 = vec2(0.0, 0.0);

vec4 v_Custom0                     : TEXCOORD19 = vec4(0.0, 0.0, 0.0, 0.0);
vec4 v_Custom1                     : TEXCOORD20 = vec4(0.0, 0.0, 0.0, 0.0);
vec4 v_Custom2                     : TEXCOORD21 = vec4(0.0, 0.0, 0.0, 0.0);
vec4 v_Custom3                     : TEXCOORD22 = vec4(0.0, 0.0, 0.0, 0.0);

vec3 a_position  : POSITION;
vec3 a_texcoord0 : TEXCOORD0;
vec4 a_color0    : COLOR0;
vec4 a_texcoord1 : TEXCOORD1;
vec2 a_texcoord2 : TEXCOORD2;
vec2 a_texcoord3 : TEXCOORD3;
vec2 a_texcoord4 : TEXCOORD4;
