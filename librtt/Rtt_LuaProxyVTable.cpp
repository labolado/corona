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

#include "Rtt_FilePath.h"
#include "Display/Rtt_BitmapMask.h"
#include "Display/Rtt_BitmapPaint.h"
#include "Display/Rtt_ClosedPath.h"
#include "Display/Rtt_ContainerObject.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_DisplayDefaults.h"
#include "Display/Rtt_DisplayObject.h"
#include "Display/Rtt_EmbossedTextObject.h"
#include "Display/Rtt_GradientPaint.h"
#include "Display/Rtt_LineObject.h"
#include "Display/Rtt_LuaLibDisplay.h"
#include "Display/Rtt_Paint.h"
#include "Display/Rtt_RectPath.h"
#include "Display/Rtt_Shader.h"
#include "Display/Rtt_ShaderFactory.h"
#include "Display/Rtt_ShapeObject.h"
#include "Display/Rtt_SnapshotObject.h"
#include "Display/Rtt_SpriteObject.h"
#include "Display/Rtt_StageObject.h"
#include "Display/Rtt_TextObject.h"
#include "Display/Rtt_TextureFactory.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_MPlatformDevice.h"
#include "Rtt_PlatformDisplayObject.h"
#include "Rtt_Runtime.h"
#include "Rtt_PhysicsWorld.h"
#include "CoronaLua.h"

#include "Rtt_ParticleSystemObject.h"
#include "Display/Rtt_EmitterObject.h"

#include "Core/Rtt_StringHash.h"

#include <string.h>

#include "Rtt_Lua.h"
#include "Rtt_Profiling.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
//
// LuaProxyVTable
//
// ----------------------------------------------------------------------------
    
// Not sure of benefit of using lua c closures...
// Seems like it'd use more memory
// #define USE_C_CLOSURE

#ifdef USE_C_CLOSURE
const char LuaProxyVTable::kDelegateKey[] = "Delegate";
#endif
/*
LuaProxyVTable::Self*
LuaProxyVTable::GetSelf( lua_State *L, int index )
{
    Self** p = (Self**)luaL_checkudata( L, index, kDelegateKey );
    return ( p ? *p : NULL );
}

int
LuaProxyVTable::__index( lua_State *L )
{
    int result = 0;

    Self* self = GetSelf( L, 1 );
    if ( self )
    {
        const char* key = lua_tostring( L, 2 );
        result = self->ValueForKey( L, key );
    }

    return result;
}

int
LuaProxyVTable::__newindex( lua_State *L )
{
    Self* self = GetSelf( L, 1 );
    if ( self )
    {
        const char* key = lua_tostring( L, 2 );
        self->SetValueForKey( L, key, 3 );
    }

    return 0;
}

int
LuaProxyVTable::__gcMeta( lua_State *L )
{
    Self* self = GetSelf( L, 1 );
    if ( self )
    {
        Self** userdata = & self;
        Rtt_ASSERT( (Self**)luaL_checkudata( L, index, kDelegateKey ) == userdata );

        *userdata = NULL;
        Rtt_DELETE( self );
    }
}
*/

bool
LuaProxyVTable::SetValueForKey( lua_State *, MLuaProxyable&, const char [], int ) const
{
    return false;
}

/*
int
LuaProxyVTable::Length( lua_State * ) const
{
    return 0;
}
*/

const LuaProxyVTable&
LuaProxyVTable::Parent() const
{
    return * this;
}

// ----------------------------------------------------------------------------

#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
// Proxy's delegate or an ancestor must match expected
bool
LuaProxyVTable::IsProxyUsingCompatibleDelegate( const LuaProxy* proxy, const Self& expected )
{
    // if proxy is NULL, skip the check
    bool result = ( NULL == proxy );

    if ( ! result )
    {
        for( const LuaProxyVTable *child = & proxy->Delegate(), *parent = & child->Parent();
             ! result;
             child = parent, parent = & child->Parent() )
        {
            result = ( child == & expected );
            if ( child == parent ) { break; }
        }
    }

    return result;
}
#endif // Rtt_DEBUG

// This implements introspection
bool
LuaProxyVTable::DumpObjectProperties( lua_State *L, const MLuaProxyable& object, const char **keys, const int numKeys, String& result ) const
{
    Rtt_LUA_STACK_GUARD( L );
    const int bufLen = 10240;
    char buf[bufLen];

    // JSON encode the value of each key
    for (int k = 0; k < numKeys; k++)
    {
        Rtt_LUA_STACK_GUARD( L );

        if (strchr(keys[k], '#'))
        {
            // Deprecated property, skip it
            continue;
        }

        // Note that the "overrideRestriction" parameter is set to true so that we don't
        // restrict access to certain properties based on license tier (this means that
        // Starter users can see some properties they can't use but makes debugger logic
        // much easier)
        int res = ValueForKey(L, object, keys[k], true);

        if (res > 0)
        {
            buf[0] = '\0';

            CoronaLuaPropertyToJSON(L, -1, keys[k], buf, bufLen, 0);

            if (! result.IsEmpty() && strlen(buf) > 0)
            {
                result.Append(", ");
            }

            result.Append(buf);

            lua_pop( L, res );
        }
    }

    return true;
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
