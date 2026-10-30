//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#ifdef Rtt_PHYSICS

#include "Rtt_PhysicsContact.h"

// TODO: see which of these are actually needed here
#include "Rtt_Lua.h"
#include "Rtt_LuaLibPhysics.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_Runtime.h"
#include "Rtt_LuaProxyVTable.h"
#include "Rtt_LuaContext.h"
#include "Rtt_PhysicsWorld.h"

#include "box2d/box2d.h"
#include <cmath>
#include <cfloat>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

const char PhysicsContact::kMetatableName[] = "physics.contact"; // unique identifier for this userdata type

UserdataWrapper *
PhysicsContact::CreateWrapper( const ResourceHandle< lua_State >& luaStateHandle, Box2dPreSolveTempContact *contact )
{
	lua_State* L = luaStateHandle.Dereference();
	if ( ! L )
	{
		return NULL;
	}

	// Lua owns wrapper (which has a weak pointer to joint)
	UserdataWrapper *result = Rtt_NEW(
		LuaContext::GetAllocator( L ),
		UserdataWrapper( luaStateHandle, contact, PhysicsContact::kMetatableName ) );

	return result;
}

Box2dPreSolveTempContact*
PhysicsContact::GetContact( lua_State *L, int index )
{
	Box2dPreSolveTempContact *result = NULL;

	UserdataWrapper **ud = (UserdataWrapper **)luaL_checkudata( L, index, Self::kMetatableName );
	if ( ud )
	{
		UserdataWrapper *wrapper = *ud;

		result = (Box2dPreSolveTempContact*)wrapper->Dereference();
	}

	return result;
}

// The native material is borrowed only during discrete pre-solve. In particular,
// pre-continuous uses the same Lua event but has no contact material to edit.
static bool
GetContactMaterial( lua_State *L, float& friction, float& bounce )
{
	const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
	return physics.IsWorldValid()
		&& b2World_GetPreSolveMaterial( physics.GetWorldId(), &friction, &bounce );
}

static int
SetContactMaterial( lua_State *L, bool setFriction )
{
	float friction, bounce;
	if ( ! GetContactMaterial( L, friction, bounce ) )
	{
		return luaL_error( L, "contact material is only writable during discrete preCollision; check contact.isMaterialEditable" );
	}
	luaL_checktype( L, 3, LUA_TNUMBER );
	lua_Number value = lua_tonumber( L, 3 );
	if ( ! std::isfinite( value ) || value < 0 || value > FLT_MAX )
	{
		return luaL_error( L, "contact material must be a finite nonnegative float" );
	}
	if ( setFriction )
	{
		friction = (float)value;
	}
	else
	{
		bounce = (float)value;
	}
	const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
	b2World_SetPreSolveMaterial( physics.GetWorldId(), friction, bounce );
	return 0;
}

int
PhysicsContact::ValueForKey( lua_State *L )
{
	int result = 0;    // number of args pushed on the stack

	Box2dPreSolveTempContact *contact = GetContact( L, 1 );

	if ( contact )
	{
		const char *key = luaL_checkstring( L, 2 );

		if ( 0 == strcmp( "isTouching", key ) )
		{
			// lua_pushboolean( L, contact->IsTouching() );
		}
		else if ( 0 == strcmp( "isEnabled", key ) )
		{
			// lua_pushboolean( L, contact->IsEnabled() );
			lua_pushboolean( L, contact->isEnabled );
			result = 1;
		}
		else if ( 0 == strcmp( "separation", key ) )
		{
			lua_pushnumber( L, contact->separation );
			result = 1;
		}
		else if ( 0 == strcmp( "friction", key ) )
		{
			float friction, bounce;
			if ( GetContactMaterial( L, friction, bounce ) )
			{
				lua_pushnumber( L, friction );
				result = 1;
			}
		}
		else if ( 0 == strcmp( "bounce", key ) )
		{
			float friction, bounce;
			if ( GetContactMaterial( L, friction, bounce ) )
			{
				lua_pushnumber( L, bounce );
				result = 1;
			}
		}
		else if ( 0 == strcmp( "isMaterialEditable", key ) )
		{
			float friction, bounce;
			lua_pushboolean( L, GetContactMaterial( L, friction, bounce ) );
			result = 1;
		}
		else if ( 0 == strcmp( "tangentSpeed", key ) )
		{
			// lua_pushnumber( L, contact->GetTangentSpeed() );
		}
		else
		{
			result = 0;
		}
	}

	return result;
}

int
PhysicsContact::SetValueForKey( lua_State *L )
{
	Box2dPreSolveTempContact *contact = GetContact( L, 1 );

	if ( contact )
	{
		const char *key = luaL_checkstring( L, 2 );

		if ( 0 == strcmp( "isEnabled", key ) )
		{
			// contact->SetEnabled( lua_toboolean( L, 3 ) );
			contact->isEnabled = lua_toboolean( L, 3 );
		}
		else if ( 0 == strcmp( "friction", key ) )
		{
			return SetContactMaterial( L, true );
		}
		else if ( 0 == strcmp( "bounce", key ) )
		{
			return SetContactMaterial( L, false );
		}
		else if ( 0 == strcmp( "tangentSpeed", key ) )
		{
			// contact->SetTangentSpeed( lua_tonumber( L, 3 ) );
		}
	}

	return 0;
}


int
PhysicsContact::Finalizer( lua_State *L )
{
	UserdataWrapper **ud = (UserdataWrapper **)luaL_checkudata( L, 1, Self::kMetatableName );
	if ( ud )
	{
		UserdataWrapper *wrapper = *ud;

		Rtt_DELETE( wrapper );
	}

	return 0;
}

// Call this to init metatable
void
PhysicsContact::Initialize( lua_State *L )
{
	Rtt_LUA_STACK_GUARD( L );

	const luaL_Reg kVTable[] =
	{
		{ "__index", Self::ValueForKey },
		{ "__newindex", Self::SetValueForKey },
		{ "__gc", Self::Finalizer },
		{ NULL, NULL }
	};

	Lua::InitializeMetatable( L, Self::kMetatableName, kVTable );
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // Rtt_PHYSICS
