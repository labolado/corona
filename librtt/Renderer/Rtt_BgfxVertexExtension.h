//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BgfxVertexExtension_H__
#define _Rtt_BgfxVertexExtension_H__

#include "Core/Rtt_Types.h"

// ----------------------------------------------------------------------------
//
// Shared mapping for vertex format-extension attributes on the bgfx backend.
//
// Unlike GL (which binds arbitrarily-named attributes such as "a_wh" via
// glBindAttribLocation), bgfx attributes are a fixed enum of semantics
// (Position, TexCoord0..7, Color0..3, Normal, Tangent, Bitangent, ...). The
// base 2D vertex layout already occupies Position, TexCoord0, Color0,
// TexCoord1 and TexCoord2/3/4 (mask UVs), so an effect's vertexExtension
// attributes must be routed into the remaining spare slots below.
//
// The i-th extension attribute (iterating the FormatExtensionList after
// SortNames()) is assigned to slot i. BOTH the generated vertex shader
// (Rtt_BgfxShaderCompiler) and the per-geometry vertex layout
// (Rtt_BgfxGeometry) consume this same table in the same order, so the data
// and the shader stay aligned. The matching attribute type declarations live
// in librtt/Display/Shader/bgfx/varying.def.sc.
//
// ----------------------------------------------------------------------------

namespace Rtt
{

// Shader-side attribute names (as used in a .sc "$input ..." list and the
// "#define Corona<Name> a_slot" macros), in slot order.
static const char* const kBgfxExtensionAttribNames[] =
{
	"a_texcoord5",
	"a_texcoord6",
	"a_texcoord7",
	"a_normal",
	"a_tangent",
	"a_bitangent",
	"a_color1",
	"a_color2",
	"a_color3",
};

static const U32 kBgfxExtensionAttribSlotCount =
	sizeof( kBgfxExtensionAttribNames ) / sizeof( kBgfxExtensionAttribNames[0] );

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BgfxVertexExtension_H__
