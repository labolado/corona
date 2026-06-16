//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// sRGB-correct alpha blending (#30): premult-aware sRGB->linear decode of the
// perceptual vertex/fill color (v_ColorScale).
//
// v_ColorScale arrives premultiplied (Paint::UpdateColor does rgb *= a), so we
// un-premultiply before the sRGB decode and re-premultiply after; alpha itself
// stays linear. White and the 0.0/1.0 endpoints are decode-identity, so
// untinted objects are unaffected.
//
// Decoding once in the VERTEX stage gives every fragment shader a linear
// v_ColorScale for free, so no per-fragment-shader edits are needed across the
// ~260 effect shaders. Gated by u_TexFlags.z (set from the opt-in flag); when
// the flag is OFF the macro is the identity, so behaviour is byte-identical.

#ifndef SRGB_COLORSCALE_SH
#define SRGB_COLORSCALE_SH

// Texture flags shared with the fragment stage: .x alpha-only, .y mask count,
// .z sRGB-correct alpha blending enabled.
uniform vec4 u_TexFlags;

vec4 srgbDecodeColorScale(vec4 cs)
{
    if (cs.a > 0.0)
    {
        vec3 unpremult = cs.rgb / cs.a;
        unpremult = mix(unpremult / 12.92,
                        pow((unpremult + 0.055) / 1.055, vec3(2.4)),
                        step(vec3(0.04045), unpremult));
        cs.rgb = unpremult * cs.a;
    }
    return cs;
}

#define SRGB_DECODE_COLORSCALE(cs) ((u_TexFlags.z > 0.5) ? srgbDecodeColorScale(cs) : (cs))

#endif // SRGB_COLORSCALE_SH
