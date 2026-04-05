////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Display/Rtt_TextureAtlas_Lua.h"
#include "Display/Rtt_TextureAtlas.h"

#include "Display/Rtt_Display.h"
#include "Display/Rtt_ImageFrame.h"
#include "Display/Rtt_ImageSheet.h"
#include "Display/Rtt_TextureResource.h"
#include "Rtt_Lua.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaLibSystem.h"
#include "Rtt_Runtime.h"

#include "Corona/CoronaLua.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

const char TextureAtlasUserdata::kMetatableName[] = "TextureAtlas";

// ----------------------------------------------------------------------------
// Lua methods
// ----------------------------------------------------------------------------

static TextureAtlas*
GetAtlasSelf( lua_State* L )
{
	TextureAtlasUserdata* ud = (TextureAtlasUserdata*)Lua::CheckUserdata( L, 1, TextureAtlasUserdata::kMetatableName );
	return ud ? ud->GetAtlas() : NULL;
}

// atlas:getFrame( name ) -> table { x, y, w, h, u0, v0, u1, v1 }
static int
atlas_getFrame( lua_State* L )
{
	TextureAtlas* atlas = GetAtlasSelf( L );
	if ( !atlas ) return 0;

	const char* name = luaL_checkstring( L, 2 );
	const TextureAtlas::Frame* frame = atlas->GetFrame( name );

	if ( !frame )
	{
		lua_pushnil( L );
		return 1;
	}

	lua_createtable( L, 0, 10 );

	lua_pushstring( L, frame->name.c_str() );
	lua_setfield( L, -2, "name" );

	lua_pushinteger( L, frame->x );
	lua_setfield( L, -2, "x" );
	lua_pushinteger( L, frame->y );
	lua_setfield( L, -2, "y" );
	lua_pushinteger( L, frame->w );
	lua_setfield( L, -2, "width" );
	lua_pushinteger( L, frame->h );
	lua_setfield( L, -2, "height" );

	lua_pushnumber( L, frame->u0 );
	lua_setfield( L, -2, "u0" );
	lua_pushnumber( L, frame->v0 );
	lua_setfield( L, -2, "v0" );
	lua_pushnumber( L, frame->u1 );
	lua_setfield( L, -2, "u1" );
	lua_pushnumber( L, frame->v1 );
	lua_setfield( L, -2, "v1" );

	lua_pushinteger( L, frame->srcW );
	lua_setfield( L, -2, "sourceWidth" );
	lua_pushinteger( L, frame->srcH );
	lua_setfield( L, -2, "sourceHeight" );

	return 1;
}

// atlas:has( name ) -> bool
static int
atlas_has( lua_State* L )
{
	TextureAtlas* atlas = GetAtlasSelf( L );
	if ( !atlas ) return 0;

	const char* name = luaL_checkstring( L, 2 );
	lua_pushboolean( L, atlas->HasFrame( name ) );
	return 1;
}

// atlas:list() -> { "a.png", "b.png", ... }
static int
atlas_list( lua_State* L )
{
	TextureAtlas* atlas = GetAtlasSelf( L );
	if ( !atlas ) return 0;

	int count = atlas->GetFrameCount();
	lua_createtable( L, count, 0 );

	for ( int i = 0; i < count; i++ )
	{
		const TextureAtlas::Frame& frame = atlas->GetFrameByIndex( i );
		lua_pushstring( L, frame.name.c_str() );
		lua_rawseti( L, -2, i + 1 );
	}

	return 1;
}

// atlas.frameCount (read-only property)
static int
atlas_index( lua_State* L )
{
	TextureAtlas* atlas = GetAtlasSelf( L );
	if ( !atlas ) return 0;

	const char* key = lua_tostring( L, 2 );
	if ( !key ) return 0;

	if ( strcmp( key, "frameCount" ) == 0 )
	{
		lua_pushinteger( L, atlas->GetFrameCount() );
		return 1;
	}
	else if ( strcmp( key, "width" ) == 0 )
	{
		lua_pushinteger( L, atlas->GetWidth() );
		return 1;
	}
	else if ( strcmp( key, "height" ) == 0 )
	{
		lua_pushinteger( L, atlas->GetHeight() );
		return 1;
	}

	// Fall through to metatable methods
	luaL_getmetatable( L, TextureAtlasUserdata::kMetatableName );
	lua_pushvalue( L, 2 );
	lua_rawget( L, -2 );
	return 1;
}

// atlas:removeSelf()
static int
atlas_removeSelf( lua_State* L )
{
	TextureAtlasUserdata* ud = (TextureAtlasUserdata*)Lua::CheckUserdata( L, 1, TextureAtlasUserdata::kMetatableName );
	if ( ud && ud->GetAtlas() )
	{
		Rtt_DELETE( ud->GetAtlas() );
		ud->ClearAtlas();
	}
	return 0;
}

// __gc
static int
atlas_gc( lua_State* L )
{
	TextureAtlasUserdata* ud = (TextureAtlasUserdata*)Lua::ToUserdata( L, 1, TextureAtlasUserdata::kMetatableName );
	if ( ud && ud->GetAtlas() )
	{
		Rtt_DELETE( ud->GetAtlas() );
		ud->ClearAtlas();
	}
	return 0;
}

// ----------------------------------------------------------------------------
// Metatable registration
// ----------------------------------------------------------------------------

void
TextureAtlasUserdata::Initialize( lua_State* L )
{
	const luaL_Reg kVTable[] =
	{
		{ "getFrame", atlas_getFrame },
		{ "has", atlas_has },
		{ "list", atlas_list },
		{ "removeSelf", atlas_removeSelf },
		{ "__gc", atlas_gc },
		{ "__index", atlas_index },
		{ NULL, NULL }
	};

	Lua::InitializeMetatable( L, kMetatableName, kVTable );
}

void
TextureAtlasUserdata::PushUserdata( lua_State* L, TextureAtlas* atlas )
{
	TextureAtlasUserdata* ud = (TextureAtlasUserdata*)lua_newuserdata( L, sizeof( TextureAtlasUserdata ) );
	new (ud) TextureAtlasUserdata( atlas );
	luaL_getmetatable( L, kMetatableName );
	lua_setmetatable( L, -2 );
}

bool
TextureAtlasUserdata::IsAtlas( lua_State* L, int index )
{
	void* p = lua_touserdata( L, index );
	if ( !p ) return false;

	if ( !lua_getmetatable( L, index ) ) return false;
	luaL_getmetatable( L, kMetatableName );
	bool match = lua_rawequal( L, -1, -2 );
	lua_pop( L, 2 );
	return match;
}

TextureAtlas*
TextureAtlasUserdata::ToAtlas( lua_State* L, int index )
{
	if ( !IsAtlas( L, index ) ) return NULL;
	TextureAtlasUserdata* ud = (TextureAtlasUserdata*)lua_touserdata( L, index );
	return ud ? ud->GetAtlas() : NULL;
}

TextureAtlas*
TextureAtlasUserdata::CheckAtlas( lua_State* L, int index )
{
	TextureAtlasUserdata* ud = (TextureAtlasUserdata*)Lua::CheckUserdata( L, index, kMetatableName );
	return ud ? ud->GetAtlas() : NULL;
}

TextureAtlasUserdata::TextureAtlasUserdata( TextureAtlas* atlas )
:	fAtlas( atlas )
{
}

// ----------------------------------------------------------------------------
// graphics.newAtlas() implementation
// ----------------------------------------------------------------------------

// graphics.newAtlas( { "a.png", "b.png", ... } [, options] )
// graphics.newAtlas( "a.png", "b.png", ... )
// options: { baseDir=system.ResourceDirectory, maxSize=2048, padding=1 }
int
TextureAtlas_newAtlas( lua_State* L )
{
	int result = 0;

	Runtime* runtime = LuaContext::GetRuntime( L );
	if ( !runtime ) return 0;

	Display& display = runtime->GetDisplay();
	Rtt_Allocator* allocator = display.GetRuntime().GetAllocator();

	MPlatform::Directory baseDir = MPlatform::kResourceDir;
	int maxSize = 2048;
	int padding = 1;

	std::vector<std::string> imageNames;

	int nextArg = 1;

	if ( lua_istable( L, nextArg ) )
	{
		// Array form: graphics.newAtlas( { "a.png", "b.png" } [, options] )
		int arrLen = (int)lua_objlen( L, nextArg );
		for ( int i = 1; i <= arrLen; i++ )
		{
			lua_rawgeti( L, nextArg, i );
			if ( lua_isstring( L, -1 ) )
			{
				imageNames.push_back( lua_tostring( L, -1 ) );
			}
			lua_pop( L, 1 );
		}
		nextArg++;

		// Optional options table
		if ( lua_istable( L, nextArg ) )
		{
			lua_getfield( L, nextArg, "baseDir" );
			if ( lua_islightuserdata( L, -1 ) )
			{
				void* p = lua_touserdata( L, -1 );
				baseDir = (MPlatform::Directory)EnumForUserdata(
					LuaLibSystem::Directories(), p,
					MPlatform::kNumDirs, MPlatform::kResourceDir );
			}
			lua_pop( L, 1 );

			lua_getfield( L, nextArg, "maxSize" );
			if ( lua_isnumber( L, -1 ) )
			{
				maxSize = (int)lua_tointeger( L, -1 );
			}
			lua_pop( L, 1 );

			lua_getfield( L, nextArg, "padding" );
			if ( lua_isnumber( L, -1 ) )
			{
				padding = (int)lua_tointeger( L, -1 );
			}
			lua_pop( L, 1 );
		}
	}
	else
	{
		// Varargs form: graphics.newAtlas( "a.png", "b.png", ... )
		int top = lua_gettop( L );
		for ( int i = nextArg; i <= top; i++ )
		{
			if ( lua_isstring( L, i ) )
			{
				imageNames.push_back( lua_tostring( L, i ) );
			}
			else
			{
				break; // stop at first non-string
			}
		}
	}

	if ( imageNames.empty() )
	{
		CoronaLuaWarning( L, "graphics.newAtlas() requires at least one image name" );
		lua_pushnil( L );
		return 1;
	}

	// Convert to C-string array
	std::vector<const char*> cNames( imageNames.size() );
	for ( size_t i = 0; i < imageNames.size(); i++ )
	{
		cNames[i] = imageNames[i].c_str();
	}

	TextureAtlas* atlas = TextureAtlas::Create(
		allocator, display, cNames.data(), (int)cNames.size(),
		baseDir, maxSize, padding );

	if ( atlas )
	{
		TextureAtlasUserdata::PushUserdata( L, atlas );
		result = 1;
	}
	else
	{
		lua_pushnil( L );
		result = 1;
	}

	return result;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
