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

#include "Display/Rtt_BatchObject_Lua.h"
#include "Display/Rtt_BatchObject.h"
#include "Display/Rtt_TextureAtlas.h"
#include "Display/Rtt_TextureAtlas_Lua.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_LuaLibDisplay.h"
#include "Rtt_Lua.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_LuaProxyVTable.h"
#include "Rtt_Runtime.h"

#include "Corona/CoronaLua.h"

#include <cstring>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

const char BatchSlotProxy::kMetatableName[] = "BatchSlotProxy";

// ----------------------------------------------------------------------------
// SlotProxy metatable methods
// ----------------------------------------------------------------------------

// slot:remove()
static int
slotProxy_remove( lua_State* L )
{
	BatchSlotProxy* proxy = (BatchSlotProxy*)luaL_checkudata( L, 1, BatchSlotProxy::kMetatableName );
	if ( proxy && proxy->fBatch )
	{
		proxy->fBatch->RemoveSlot( proxy->fSlotId );
		proxy->fBatch = NULL; // invalidate
	}
	return 0;
}

// __index: slot.x, slot.y, slot.rotation, slot.scaleX, slot.scaleY, slot.alpha, slot.isVisible
static int
slotProxy_index( lua_State* L )
{
	BatchSlotProxy* proxy = (BatchSlotProxy*)luaL_checkudata( L, 1, BatchSlotProxy::kMetatableName );
	const char* key = luaL_checkstring( L, 2 );

	if ( !proxy || !proxy->fBatch )
	{
		lua_pushnil( L );
		return 1;
	}

	const BatchObject::Slot* slot = proxy->fBatch->GetSlot( proxy->fSlotId );
	if ( !slot )
	{
		lua_pushnil( L );
		return 1;
	}

	if ( strcmp( key, "x" ) == 0 )
	{
		lua_pushnumber( L, slot->x );
	}
	else if ( strcmp( key, "y" ) == 0 )
	{
		lua_pushnumber( L, slot->y );
	}
	else if ( strcmp( key, "rotation" ) == 0 )
	{
		lua_pushnumber( L, slot->rotation );
	}
	else if ( strcmp( key, "scaleX" ) == 0 || strcmp( key, "xScale" ) == 0 )
	{
		lua_pushnumber( L, slot->scaleX );
	}
	else if ( strcmp( key, "scaleY" ) == 0 || strcmp( key, "yScale" ) == 0 )
	{
		lua_pushnumber( L, slot->scaleY );
	}
	else if ( strcmp( key, "alpha" ) == 0 )
	{
		lua_pushnumber( L, slot->alpha );
	}
	else if ( strcmp( key, "isVisible" ) == 0 )
	{
		lua_pushboolean( L, slot->isVisible );
	}
	else if ( strcmp( key, "remove" ) == 0 )
	{
		// Return the remove method
		lua_pushcfunction( L, slotProxy_remove );
	}
	else
	{
		lua_pushnil( L );
	}

	return 1;
}

// __newindex: slot.x = 100, etc.
static int
slotProxy_newindex( lua_State* L )
{
	BatchSlotProxy* proxy = (BatchSlotProxy*)luaL_checkudata( L, 1, BatchSlotProxy::kMetatableName );
	const char* key = luaL_checkstring( L, 2 );

	if ( !proxy || !proxy->fBatch )
	{
		return 0;
	}

	BatchObject::Slot* slot = proxy->fBatch->GetSlot( proxy->fSlotId );
	if ( !slot )
	{
		return 0;
	}

	if ( strcmp( key, "x" ) == 0 )
	{
		slot->x = (Real)lua_tonumber( L, 3 );
	}
	else if ( strcmp( key, "y" ) == 0 )
	{
		slot->y = (Real)lua_tonumber( L, 3 );
	}
	else if ( strcmp( key, "rotation" ) == 0 )
	{
		slot->rotation = (Real)lua_tonumber( L, 3 );
	}
	else if ( strcmp( key, "scaleX" ) == 0 || strcmp( key, "xScale" ) == 0 )
	{
		slot->scaleX = (Real)lua_tonumber( L, 3 );
	}
	else if ( strcmp( key, "scaleY" ) == 0 || strcmp( key, "yScale" ) == 0 )
	{
		slot->scaleY = (Real)lua_tonumber( L, 3 );
	}
	else if ( strcmp( key, "alpha" ) == 0 )
	{
		slot->alpha = (Real)lua_tonumber( L, 3 );
	}
	else if ( strcmp( key, "isVisible" ) == 0 )
	{
		slot->isVisible = lua_toboolean( L, 3 ) ? true : false;
	}
	else
	{
		return 0;
	}

	slot->isDirty = true;
	// Mark batch vertices as dirty so they get rebuilt
	proxy->fBatch->Invalidate( DisplayObject::kGeometryFlag );

	return 0;
}

void
BatchSlotProxy::Initialize( lua_State* L )
{
	const luaL_Reg kVTable[] =
	{
		{ "remove", slotProxy_remove },
		{ "__index", slotProxy_index },
		{ "__newindex", slotProxy_newindex },
		{ NULL, NULL }
	};

	Lua::InitializeMetatable( L, kMetatableName, kVTable );
}

void
BatchSlotProxy::PushProxy( lua_State* L, BatchObject* batch, int slotId )
{
	BatchSlotProxy* proxy = (BatchSlotProxy*)lua_newuserdata( L, sizeof( BatchSlotProxy ) );
	proxy->fBatch = batch;
	proxy->fSlotId = slotId;
	luaL_getmetatable( L, kMetatableName );
	lua_setmetatable( L, -2 );
}

// ----------------------------------------------------------------------------
// LuaBatchObjectProxyVTable
// ----------------------------------------------------------------------------

const LuaBatchObjectProxyVTable&
LuaBatchObjectProxyVTable::Constant()
{
	static const Self kVTable;
	return kVTable;
}

// batch:add( "frameName", x, y [, opts] )
int
LuaBatchObjectProxyVTable::add( lua_State* L )
{
	BatchObject* batch = (BatchObject*)LuaProxy::GetProxyableObject( L, 1 );
	Rtt_WARN_SIM_PROXY_TYPE( L, 1, BatchObject );

	if ( !batch ) return 0;

	TextureAtlas* atlas = batch->GetAtlas();
	if ( !atlas ) return 0;

	const char* frameName = luaL_checkstring( L, 2 );
	Real x = lua_isnumber( L, 3 ) ? (Real)lua_tonumber( L, 3 ) : Rtt_REAL_0;
	Real y = lua_isnumber( L, 4 ) ? (Real)lua_tonumber( L, 4 ) : Rtt_REAL_0;

	// Find frame by name
	const TextureAtlas::Frame* frame = atlas->GetFrame( frameName );
	if ( !frame )
	{
		CoronaLuaWarning( L, "batch:add() - frame '%s' not found in atlas", frameName );
		lua_pushnil( L );
		return 1;
	}

	// Find frame index
	int frameIndex = -1;
	for ( int i = 0, count = atlas->GetFrameCount(); i < count; i++ )
	{
		if ( atlas->GetFrameByIndex( i ).name == frameName )
		{
			frameIndex = i;
			break;
		}
	}

	int slotId = batch->AddSlot( frameIndex, x, y );

	// Apply optional properties
	if ( lua_istable( L, 5 ) )
	{
		BatchObject::Slot* slot = batch->GetSlot( slotId );
		if ( slot )
		{
			lua_getfield( L, 5, "rotation" );
			if ( lua_isnumber( L, -1 ) ) slot->rotation = (Real)lua_tonumber( L, -1 );
			lua_pop( L, 1 );

			lua_getfield( L, 5, "xScale" );
			if ( lua_isnumber( L, -1 ) ) slot->scaleX = (Real)lua_tonumber( L, -1 );
			lua_pop( L, 1 );

			lua_getfield( L, 5, "yScale" );
			if ( lua_isnumber( L, -1 ) ) slot->scaleY = (Real)lua_tonumber( L, -1 );
			lua_pop( L, 1 );

			lua_getfield( L, 5, "alpha" );
			if ( lua_isnumber( L, -1 ) ) slot->alpha = (Real)lua_tonumber( L, -1 );
			lua_pop( L, 1 );
		}
	}

	// Return SlotProxy
	BatchSlotProxy::PushProxy( L, batch, slotId );
	return 1;
}

// batch:clear()
int
LuaBatchObjectProxyVTable::clear( lua_State* L )
{
	BatchObject* batch = (BatchObject*)LuaProxy::GetProxyableObject( L, 1 );
	Rtt_WARN_SIM_PROXY_TYPE( L, 1, BatchObject );

	if ( batch )
	{
		batch->Clear();
	}
	return 0;
}

// batch:count()
int
LuaBatchObjectProxyVTable::count( lua_State* L )
{
	BatchObject* batch = (BatchObject*)LuaProxy::GetProxyableObject( L, 1 );
	Rtt_WARN_SIM_PROXY_TYPE( L, 1, BatchObject );

	if ( batch )
	{
		lua_pushinteger( L, batch->GetCount() );
		return 1;
	}
	return 0;
}

int
LuaBatchObjectProxyVTable::ValueForKey( lua_State* L, const MLuaProxyable& object, const char key[], bool overrideRestriction ) const
{
	int result = 1;

	if ( strcmp( key, "add" ) == 0 )
	{
		lua_pushcfunction( L, add );
	}
	else if ( strcmp( key, "clear" ) == 0 )
	{
		lua_pushcfunction( L, clear );
	}
	else if ( strcmp( key, "count" ) == 0 )
	{
		lua_pushcfunction( L, count );
	}
	else if ( strcmp( key, "numSlots" ) == 0 )
	{
		const BatchObject& batch = static_cast< const BatchObject& >( (const DisplayObject&)object );
		lua_pushinteger( L, batch.GetCount() );
	}
	else
	{
		result = Super::ValueForKey( L, object, key, overrideRestriction );
	}

	return result;
}

bool
LuaBatchObjectProxyVTable::SetValueForKey( lua_State* L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
	return Super::SetValueForKey( L, object, key, valueIndex );
}

const LuaProxyVTable&
LuaBatchObjectProxyVTable::Parent() const
{
	return Super::Constant();
}

// ----------------------------------------------------------------------------
// display.newBatch() implementation
// ----------------------------------------------------------------------------

// display.newBatch( atlas [, capacity] )
int
BatchObject_newBatch( lua_State* L )
{
	int result = 0;

	Runtime* runtime = LuaContext::GetRuntime( L );
	if ( !runtime ) return 0;

	Display& display = runtime->GetDisplay();
	Rtt_Allocator* allocator = display.GetRuntime().GetAllocator();

	int nextArg = 1;

	// Optional parent group
	GroupObject* parent = LuaLibDisplay::GetParent( L, nextArg );

	// First arg: atlas userdata
	if ( !TextureAtlasUserdata::IsAtlas( L, nextArg ) )
	{
		CoronaLuaWarning( L, "display.newBatch() expects an atlas as first argument" );
		lua_pushnil( L );
		return 1;
	}

	TextureAtlas* atlas = TextureAtlasUserdata::ToAtlas( L, nextArg );
	nextArg++;

	if ( !atlas )
	{
		lua_pushnil( L );
		return 1;
	}

	// Optional capacity (default 64)
	int capacity = 64;
	if ( lua_isnumber( L, nextArg ) )
	{
		capacity = (int)lua_tointeger( L, nextArg );
		if ( capacity < 1 ) capacity = 1;
		nextArg++;
	}

	BatchObject* batch = BatchObject::New( allocator, display, atlas, capacity );
	if ( !batch )
	{
		lua_pushnil( L );
		return 1;
	}

	result = LuaLibDisplay::AssignParentAndPushResult( L, display, batch, parent );

	return result;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
