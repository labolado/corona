//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Rtt_LuaProxyVTable.h"

#include "Display/Rtt_Display.h"
#include "Display/Rtt_DisplayDefaults.h"
#include "Display/Rtt_LuaLibDisplay.h"
#include "Display/Rtt_ShapeObject.h"
#include "Display/Rtt_SpriteObject.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_Runtime.h"
#include "CoronaLua.h"

#include <string.h>

#include "Rtt_Lua.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

const LuaSpriteObjectProxyVTable&
LuaSpriteObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaSpriteObjectProxyVTable::play( lua_State *L )
{
    SpriteObject *o = (SpriteObject*)LuaProxy::GetProxyableObject( L, 1 );
    
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, SpriteObject );
    
    if ( o )
    {
        o->Play( L );
    }

    return 0;
}

int
LuaSpriteObjectProxyVTable::pause( lua_State *L )
{
    SpriteObject *o = (SpriteObject*)LuaProxy::GetProxyableObject( L, 1 );
    
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, SpriteObject );
    
    if ( o )
    {
        o->Pause();
    }

    return 0;
}

int
LuaSpriteObjectProxyVTable::setSequence( lua_State *L )
{
    SpriteObject *o = (SpriteObject*)LuaProxy::GetProxyableObject( L, 1 );
    
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, SpriteObject );
    
    if ( o )
    {
        const char *name = lua_tostring( L, 2 );
        o->SetSequence( name );
    }

    return 0;
}

int
LuaSpriteObjectProxyVTable::setFrame( lua_State *L )
{
	SpriteObject *o = (SpriteObject*)LuaProxy::GetProxyableObject( L, 1 );
	
	Rtt_WARN_SIM_PROXY_TYPE( L, 1, SpriteObject );
	
	if ( o )
	{
		int index = (int) lua_tointeger( L, 2 );
		if ( index < 1 )
		{
			CoronaLuaWarning(L, "sprite:setFrame() given invalid index (%d). Using index of 1 instead", index);
			index = 1;
		}
		else if ( index > o->GetNumFrames() )
		{
			CoronaLuaWarning(L, "sprite:setFrame() given invalid index (%d). Using index of %d instead", index, o->GetNumFrames() );
			index = o->GetNumFrames();
		}
		o->SetFrame( index - 1 ); // Lua is 1-based
	}

	return 0;
}

int
LuaSpriteObjectProxyVTable::useFrameForAnchors( lua_State *L )
{
	SpriteObject *o = (SpriteObject*)LuaProxy::GetProxyableObject( L, 1 );
	
	Rtt_WARN_SIM_PROXY_TYPE( L, 1, SpriteObject );
	
	if ( o )
	{
		int index;
		if ( lua_isnoneornil( L, 2 ) )
		{
			index = o->GetFrame();
		}		
		else
		{
			index = (int) lua_tointeger( L, 2 );
			if ( index < 1 )
			{
				CoronaLuaWarning(L, "sprite:useFrameForAnchors() given invalid index (%d). Using index of 1 instead", index);
				index = 1;
			}
			else if ( index > o->GetNumFrames() )
			{
				CoronaLuaWarning(L, "sprite:useFrameForAnchors() given invalid index (%d). Using index of %d instead", index, o->GetNumFrames() );
				index = o->GetNumFrames();
			}
			--index; // Lua is 1-based
		}
		o->UseFrameForAnchors( index ); // Lua is 1-based
	}

    return 0;
}

int
LuaSpriteObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }
    
    int result = 1;

    static const char * keys[] =
    {
        // Read-write properties
        "timeScale",    // 0
    
        // Read-only properties
        "frame",        // 1
        "numFrames",    // 2
        "isPlaying",    // 3
        "sequence",        // 4

		// Methods
		"play",			// 5
		"pause",		// 6
		"setSequence",	// 7
		"setFrame",		// 8
		"useFrameForAnchors"	// 9
	};
	static const int numKeys = sizeof( keys ) / sizeof( const char * );
	static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 10, 25, 7, __FILE__, __LINE__ );
	StringHash *hash = &sHash;

    int index = hash->Lookup( key );

    const SpriteObject& o = static_cast< const SpriteObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, SpriteObject );

	switch ( index )
	{
	case 0:
		{
			Real timeScale = o.GetTimeScale();
			lua_pushnumber( L, Rtt_RealToFloat( timeScale ) );
		}
		break;
	case 1:
		{
			int currentFrame = o.GetFrame() + 1; // Lua is 1-based
			lua_pushinteger( L, currentFrame );
		}
		break;
	case 2:
		{
			lua_pushinteger( L, o.GetNumFrames() );
		}
		break;
	case 3:
		{
			lua_pushboolean( L, o.IsPlaying() );
		}
		break;
	case 4:
		{
			const char *sequenceName = o.GetSequence();
			if ( sequenceName )
			{
				lua_pushstring( L, sequenceName );
			}
			else
			{
				lua_pushnil( L );
			}
		}
		break;
	case 5:
		{
			Lua::PushCachedFunction( L, Self::play );
		}
		break;
	case 6:
		{
			Lua::PushCachedFunction( L, Self::pause );
		}
		break;
	case 7:
		{
			Lua::PushCachedFunction( L, Self::setSequence );
		}
		break;
	case 8:
		{
			Lua::PushCachedFunction( L, Self::setFrame );
		}
		break;
	case 9:
		{
			Lua::PushCachedFunction( L, Self::useFrameForAnchors );
		}
		break;
	default:
		{
			result = Super::ValueForKey( L, object, key, overrideRestriction );
		}
		break;
	}

    // If we retrieved the "_properties" key from the super, merge it with the local properties
    if ( result == 1 && strcmp( key, "_properties" ) == 0 )
    {
        String spriteProperties(LuaContext::GetRuntime( L )->Allocator());

        DumpObjectProperties( L, object, keys, numKeys, spriteProperties );

        lua_pushfstring( L, "{ %s, %s }", spriteProperties.GetString(), lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

bool
LuaSpriteObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    SpriteObject& o = static_cast< SpriteObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, SpriteObject );

    bool result = true;

    static const char * keys[] =
    {
        // Read-write properties
        "timeScale",    // 0
    
        // Read-only properties
        "frame",        // 1
        "numFrames",    // 2
        "isPlaying",    // 3
        "sequence",        // 4
    };
    static const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 5, 1, 1, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    int index = hash->Lookup( key );

    switch ( index )
    {
    case 0:
        {
            Real timeScale = Rtt_FloatToReal( (float)lua_tonumber( L, valueIndex ) );
            Real min = Rtt_FloatToReal( 0.05f );
            Real max = Rtt_FloatToReal( 20.0f );
            if ( timeScale < min )
            {
                CoronaLuaWarning(L, "sprite.timeScale must be >= %g. Using %g", min, min);
                timeScale = min;
            }
            else if ( timeScale < min )
            {
                CoronaLuaWarning(L, "sprite.timeScale must be <= %g. Using %g", max, max);
                timeScale = max;
            }
            o.SetTimeScale( timeScale );
        }
        break;

    case 1:
    case 2:
    case 3:
    case 4:
        {
            // Read-only properties
            // no-op
        }
        break;

    default:
        {
            result = Super::SetValueForKey( L, object, key, valueIndex );
        }
        break;
    }

    return result;
}

const LuaProxyVTable&
LuaSpriteObjectProxyVTable::Parent() const
{
    return Super::Constant();
}

// ----------------------------------------------------------------------------

} // namespace Rtt

} // namespace Rtt

// ----------------------------------------------------------------------------
