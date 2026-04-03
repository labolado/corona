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

#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_PlatformDisplayObject.h"
#include "CoronaLua.h"

#include <string.h>

#include "Rtt_Lua.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

int
LuaPlatformDisplayObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }

    // PlatformDisplayObject* o = (PlatformDisplayObject*)LuaProxy::GetProxyableObject( L, 1 );
    const PlatformDisplayObject& o = static_cast< const PlatformDisplayObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, PlatformDisplayObject );

    int result = o.ValueForKey( L, key ) || Super::ValueForKey( L, object, key, overrideRestriction );

    if ( 0 == result )
    {
        if ( strcmp( "getNativeProperty", key ) == 0 )
        {
            lua_pushlightuserdata( L, const_cast< PlatformDisplayObject * >( & o ) );
            lua_pushcclosure( L, PlatformDisplayObject::getNativeProperty, 1 );
            result = 1;
        }
        else if ( strcmp( "setNativeProperty", key ) == 0 )
        {
            lua_pushlightuserdata( L, const_cast< PlatformDisplayObject * >( & o ) );
            lua_pushcclosure( L, PlatformDisplayObject::setNativeProperty, 1 );
            result = 1;
        }
    }

    // If we retrieved the "_properties" key from the super, merge it with the local properties
    if ( result == 1 && strcmp( key, "_properties" ) == 0 )
    {
        String textProperties(LuaContext::GetRuntime( L )->Allocator());

        // TODO: Implement DumpObjectProperties for PlatformDisplayObjects
        // DumpObjectProperties( L, object, keys, numKeys, textProperties );

        // lua_pushfstring( L, "{ %s, %s }", textProperties.GetString(), lua_tostring( L, -1 ) );
        lua_pushfstring( L, "{ %s }", lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

bool
LuaPlatformDisplayObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    // PlatformDisplayObject* o = (PlatformDisplayObject*)LuaProxy::GetProxyableObject( L, 1 );
    PlatformDisplayObject& o = static_cast< PlatformDisplayObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, PlatformDisplayObject );

    bool result =
        o.SetValueForKey( L, key, valueIndex )
        || Super::SetValueForKey( L, object, key, valueIndex );

    return result;
}

// ----------------------------------------------------------------------------

// Need explicit default constructor for const use by C++ spec
LuaPlatformTextFieldObjectProxyVTable::LuaPlatformTextFieldObjectProxyVTable()
    : LuaPlatformDisplayObjectProxyVTable()
{
}

const LuaPlatformTextFieldObjectProxyVTable&
LuaPlatformTextFieldObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

const LuaProxyVTable&
LuaPlatformTextFieldObjectProxyVTable::Parent() const
{
    return Super::Constant();
}

// ----------------------------------------------------------------------------

// Need explicit default constructor for const use by C++ spec
LuaPlatformTextBoxObjectProxyVTable::LuaPlatformTextBoxObjectProxyVTable()
    : LuaPlatformDisplayObjectProxyVTable()
{
}

const LuaPlatformTextBoxObjectProxyVTable&
LuaPlatformTextBoxObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

const LuaProxyVTable&
LuaPlatformTextBoxObjectProxyVTable::Parent() const
{
    return Super::Constant();
}

// ----------------------------------------------------------------------------

// Need explicit default constructor for const use by C++ spec
LuaPlatformMapViewObjectProxyVTable::LuaPlatformMapViewObjectProxyVTable()
    : LuaPlatformDisplayObjectProxyVTable()
{
}

const LuaPlatformMapViewObjectProxyVTable&
LuaPlatformMapViewObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

const LuaProxyVTable&
LuaPlatformMapViewObjectProxyVTable::Parent() const
{
    return Super::Constant();
}

// ----------------------------------------------------------------------------

// Need explicit default constructor for const use by C++ spec
LuaPlatformWebViewObjectProxyVTable::LuaPlatformWebViewObjectProxyVTable()
    : LuaPlatformDisplayObjectProxyVTable()
{
}

const LuaPlatformWebViewObjectProxyVTable&
LuaPlatformWebViewObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

const LuaProxyVTable&
LuaPlatformWebViewObjectProxyVTable::Parent() const
{
    return Super::Constant();
}

// ----------------------------------------------------------------------------

// Need explicit default constructor for const use by C++ spec
LuaPlatformVideoObjectProxyVTable::LuaPlatformVideoObjectProxyVTable()
    : LuaPlatformDisplayObjectProxyVTable()
{
}

const LuaPlatformVideoObjectProxyVTable&
LuaPlatformVideoObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

const LuaProxyVTable&
LuaPlatformVideoObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
