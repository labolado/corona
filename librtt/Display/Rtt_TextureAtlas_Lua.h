////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_TextureAtlas_Lua_H__
#define _Rtt_TextureAtlas_Lua_H__

#include "Core/Rtt_Macros.h"

struct lua_State;

// ----------------------------------------------------------------------------

namespace Rtt
{

class TextureAtlas;

// ----------------------------------------------------------------------------

// Lua userdata wrapper for TextureAtlas
class TextureAtlasUserdata
{
	Rtt_CLASS_NO_COPIES( TextureAtlasUserdata )

	public:
		static const char kMetatableName[];

		// Register metatable
		static void Initialize( lua_State* L );

		// Push new userdata wrapping atlas
		static void PushUserdata( lua_State* L, TextureAtlas* atlas );

		// Check if stack index is an atlas userdata
		static bool IsAtlas( lua_State* L, int index );

		// Get atlas from stack (returns NULL on type mismatch)
		static TextureAtlas* ToAtlas( lua_State* L, int index );

		// Get atlas from stack (lua error on type mismatch)
		static TextureAtlas* CheckAtlas( lua_State* L, int index );

	public:
		TextureAtlasUserdata( TextureAtlas* atlas );

		TextureAtlas* GetAtlas() const { return fAtlas; }
		void ClearAtlas() { fAtlas = NULL; }

	private:
		TextureAtlas* fAtlas;
};

// graphics.newAtlas() implementation
int TextureAtlas_newAtlas( lua_State* L );

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_TextureAtlas_Lua_H__
