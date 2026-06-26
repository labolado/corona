#include "Core/Rtt_Config.h"
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )

////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// Author: Labo Lado, laboladoapp@gmail.com
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Display/Rtt_LuaLibSDFInstance.h"

#include "Display/Rtt_Display.h"
#include "Display/Rtt_LuaLibDisplay.h"
#include "Display/Rtt_SDFGroupObject.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_Runtime.h"

#include "Corona/CoronaLua.h"

#include <cstring>

namespace Rtt
{

enum
{
	kFieldX = 0x001,
	kFieldY = 0x002,
	kFieldW = 0x004,
	kFieldH = 0x008,
	kFieldRotation = 0x010,
	kFieldShapeId = 0x020,
	kFieldR = 0x040,
	kFieldG = 0x080,
	kFieldB = 0x100,
	kFieldA = 0x200
};

static bool
ReadNumberField( lua_State* L, int tableIndex, const char* key, float& value )
{
	lua_getfield( L, tableIndex, key );
	const bool result = lua_isnumber( L, -1 );
	if ( result )
	{
		value = (float)lua_tonumber( L, -1 );
	}
	lua_pop( L, 1 );
	return result;
}

static bool
ReadByteField( lua_State* L, int tableIndex, const char* key, U8& value )
{
	lua_getfield( L, tableIndex, key );
	const bool result = lua_isnumber( L, -1 );
	if ( result )
	{
		int n = (int)lua_tointeger( L, -1 );
		if ( n < 0 ) n = 0;
		if ( n > 7 ) n = 7;
		value = (U8)n;
	}
	lua_pop( L, 1 );
	return result;
}

static SDFGroupObject*
ToSDFGroup( lua_State* L, int index )
{
	SDFGroupObject* group = (SDFGroupObject*)LuaProxy::GetProxyableObject( L, index );
	Rtt_WARN_SIM_PROXY_TYPE( L, index, SDFGroupObject );
	return group;
}

static U32
ReadShapeTable( lua_State* L, int index, SDFInstanceSlot& slot )
{
	U32 mask = 0;
	if ( ReadNumberField( L, index, "x", slot.x ) ) mask |= kFieldX;
	if ( ReadNumberField( L, index, "y", slot.y ) ) mask |= kFieldY;
	if ( ReadNumberField( L, index, "w", slot.w ) ) mask |= kFieldW;
	if ( ReadNumberField( L, index, "h", slot.h ) ) mask |= kFieldH;
	if ( ReadNumberField( L, index, "rotation", slot.rotation ) ) mask |= kFieldRotation;
	if ( ReadByteField( L, index, "shapeId", slot.shapeId ) ) mask |= kFieldShapeId;
	if ( ReadNumberField( L, index, "r", slot.r ) ) mask |= kFieldR;
	if ( ReadNumberField( L, index, "g", slot.g ) ) mask |= kFieldG;
	if ( ReadNumberField( L, index, "b", slot.b ) ) mask |= kFieldB;
	if ( ReadNumberField( L, index, "a", slot.a ) ) mask |= kFieldA;
	return mask;
}

static SDFInstanceSlot
DefaultSlot()
{
	SDFInstanceSlot slot;
	slot.x = 0.0f;
	slot.y = 0.0f;
	slot.w = 1.0f;
	slot.h = 1.0f;
	slot.rotation = 0.0f;
	slot.shapeId = 0;
	slot.nVerts = 5;
	slot.r = 1.0f;
	slot.g = 1.0f;
	slot.b = 1.0f;
	slot.a = 1.0f;
	slot.active = true;
	return slot;
}

const LuaSDFGroupObjectProxyVTable&
LuaSDFGroupObjectProxyVTable::Constant()
{
	static const Self kVTable;
	return kVTable;
}

int
LuaSDFGroupObjectProxyVTable::addShape( lua_State* L )
{
	SDFGroupObject* group = ToSDFGroup( L, 1 );
	if ( !group || !lua_istable( L, 2 ) )
	{
		lua_pushnil( L );
		return 1;
	}

	SDFInstanceSlot slot = DefaultSlot();
	ReadShapeTable( L, 2, slot );

	const int slotId = group->AddShape( slot );
	if ( slotId < 0 )
	{
		lua_pushnil( L );
		return 1;
	}

	lua_pushinteger( L, slotId + 1 );
	return 1;
}

int
LuaSDFGroupObjectProxyVTable::updateShape( lua_State* L )
{
	SDFGroupObject* group = ToSDFGroup( L, 1 );
	if ( !group || !lua_isnumber( L, 2 ) || !lua_istable( L, 3 ) )
	{
		lua_pushboolean( L, 0 );
		return 1;
	}

	SDFInstanceSlot slot = DefaultSlot();
	const U32 mask = ReadShapeTable( L, 3, slot );
	const int slotId = (int)lua_tointeger( L, 2 ) - 1;
	lua_pushboolean( L, group->UpdateShape( slotId, slot, mask ) ? 1 : 0 );
	return 1;
}

int
LuaSDFGroupObjectProxyVTable::removeShape( lua_State* L )
{
	SDFGroupObject* group = ToSDFGroup( L, 1 );
	if ( !group || !lua_isnumber( L, 2 ) )
	{
		lua_pushboolean( L, 0 );
		return 1;
	}

	const int slotId = (int)lua_tointeger( L, 2 ) - 1;
	lua_pushboolean( L, group->RemoveShape( slotId ) ? 1 : 0 );
	return 1;
}

int
LuaSDFGroupObjectProxyVTable::clearShapes( lua_State* L )
{
	SDFGroupObject* group = ToSDFGroup( L, 1 );
	if ( group )
	{
		group->ClearShapes();
	}
	return 0;
}

int
LuaSDFGroupObjectProxyVTable::numShapes( lua_State* L )
{
	SDFGroupObject* group = ToSDFGroup( L, 1 );
	if ( group )
	{
		lua_pushinteger( L, group->GetShapeCount() );
		return 1;
	}
	return 0;
}

int
LuaSDFGroupObjectProxyVTable::ValueForKey( lua_State* L, const MLuaProxyable& object, const char key[], bool overrideRestriction ) const
{
	const SDFGroupObject& group = static_cast< const SDFGroupObject& >( (const DisplayObject&)object );
	int result = 1;

	if ( strcmp( key, "addShape" ) == 0 )
	{
		lua_pushcfunction( L, addShape );
	}
	else if ( strcmp( key, "updateShape" ) == 0 )
	{
		lua_pushcfunction( L, updateShape );
	}
	else if ( strcmp( key, "removeShape" ) == 0 )
	{
		lua_pushcfunction( L, removeShape );
	}
	else if ( strcmp( key, "clearShapes" ) == 0 )
	{
		lua_pushcfunction( L, clearShapes );
	}
	else if ( strcmp( key, "numShapes" ) == 0 )
	{
		lua_pushcfunction( L, numShapes );
	}
	else if ( strcmp( key, "capacity" ) == 0 )
	{
		lua_pushinteger( L, group.GetCapacity() );
	}
	else
	{
		result = Super::ValueForKey( L, object, key, overrideRestriction );
	}

	return result;
}

bool
LuaSDFGroupObjectProxyVTable::SetValueForKey( lua_State* L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
	return Super::SetValueForKey( L, object, key, valueIndex );
}

const LuaProxyVTable&
LuaSDFGroupObjectProxyVTable::Parent() const
{
	return Super::Constant();
}

void
LuaLibSDFInstance::Initialize( lua_State* )
{
}

void
LuaLibSDFInstance::RegisterDisplayFunctions( lua_State* L )
{
	lua_getglobal( L, "display" );
	if ( lua_istable( L, -1 ) )
	{
		lua_pushcfunction( L, SDFInstance_newSDFGroup );
		lua_setfield( L, -2, "newSDFGroup" );
	}
	lua_pop( L, 1 );
}

int
SDFInstance_newSDFGroup( lua_State* L )
{
	Runtime* runtime = LuaContext::GetRuntime( L );
	if ( !runtime )
	{
		return 0;
	}

	Display& display = runtime->GetDisplay();
	Rtt_Allocator* allocator = display.GetRuntime().GetAllocator();

	int nextArg = 1;
	GroupObject* parent = LuaLibDisplay::GetParent( L, nextArg );

	int capacity = 64;
	if ( lua_isnumber( L, nextArg ) )
	{
		capacity = (int)lua_tointeger( L, nextArg );
	}
	if ( capacity < 1 )
	{
		capacity = 1;
	}

	SDFGroupObject* group = SDFGroupObject::New( allocator, display, capacity );
	if ( !group )
	{
		lua_pushnil( L );
		return 1;
	}

	return LuaLibDisplay::AssignParentAndPushResult( L, display, group, parent );
}

} // namespace Rtt

#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV
