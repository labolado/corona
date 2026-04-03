//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Solar2D Bgfx Varying Definitions
// 
// This file defines the varying (interpolated) variables passed from
// vertex shader to fragment shader.
//
// Format: type name : SEMANTIC
// Semantics: COLOR0-7, TEXCOORD0-7

// Vertex position (from vertex to fragment for potential use)
vec3 v_TexCoord   : TEXCOORD0;  // UV coordinates (xy) + optional z
vec4 v_ColorScale : COLOR0;     // Vertex color multiplier
vec4 v_UserData   : TEXCOORD1;  // Custom user data from vertex attribute

// Mask UVs (conditionally compiled based on MASK_COUNT)
// These are defined with TEXCOORD semantics for proper interpolation
#ifdef MASK_COUNT
#if MASK_COUNT > 0
vec2 v_MaskUV0    : TEXCOORD2;  // Mask 0 UV coordinates
#endif
#if MASK_COUNT > 1
vec2 v_MaskUV1    : TEXCOORD3;  // Mask 1 UV coordinates
#endif
#if MASK_COUNT > 2
vec2 v_MaskUV2    : TEXCOORD4;  // Mask 2 UV coordinates
#endif
#endif
