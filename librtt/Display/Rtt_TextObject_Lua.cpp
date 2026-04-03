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
#include "Renderer/Rtt_Geometry_Renderer.h"
#include "Rtt_Profiling.h"
#include <string.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

const LuaTextObjectProxyVTable&
LuaTextObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaTextObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }
    
    int result = 1;

    static const char * keys[] =
    {
        "text",              // 0
        "size",              // 1
        "setMask",           // 2
        "setTextColor#",     // 3 - DEPRECATED
        "baselineOffset",    // 4
    };

    static const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 5, 2, 2, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    // TextObject* o = (TextObject*)LuaProxy::GetProxyableObject( L, 1 );
    const TextObject& o = static_cast< const TextObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, TextObject );

    int index = hash->Lookup( key );

    switch ( index )
    {
        case 0: // "text"
            {
                lua_pushstring( L, o.GetText() );
            }
            break;
        case 1: // "size"
            {
                lua_pushnumber( L, Rtt_RealToFloat( o.GetSize() ) );
            }
            break;
        case 2: // setMask
            {
                // Disable o:setMask() calls for TextObjects. The mask is already used up by the text itself.
                result = 0;
            }
            break;
        case 3: // setTextColor
            {
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
                if ( o.IsV1Compatibility() )
                {
                    CoronaLuaWarning(L, "o:setTextColor() is deprecated. Use o:setFillColor() instead");
                }
#endif
                Lua::PushCachedFunction( L, LuaShapeObjectProxyVTable::setFillColor );
            }
            break;
        case 4:
            {
                lua_pushnumber(L, o.GetBaselineOffset() );
            }
            break;
        default:
            {
                result = Super::ValueForKey( L, object, key, overrideRestriction );
            }
            break;
    }

    // If we retrieved the "_properties" key from the super, merge it with the local properties
    if (result == 1 && strcmp( key, "_properties" ) == 0 )
    {
        String properties(LuaContext::GetRuntime( L )->Allocator());
        const char *prefix = "";
        const char *postfix = "";

        DumpObjectProperties( L, object, keys, numKeys, properties );

        // "EmbossedTextObjects" are derived from "TextObjects" so
        // we need to emit complete JSON in those cases so we add the enclosing braces if
        // this is a "TextObject" but not if it's forming part of of a larger object
        if (strcmp(o.GetObjectDesc(), "TextObject") == 0)
        {
            prefix = "{ ";
            postfix = " }";
        }

        // Combine this object's properties with those of the super that were pushed above
        lua_pushfstring( L, "%s%s, %s%s", prefix, properties.GetString(), lua_tostring( L, -1 ), postfix );

        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

bool
LuaTextObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    // TextObject* o = (TextObject*)LuaProxy::GetProxyableObject( L, 1 );
    TextObject& o = static_cast< TextObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, TextObject );

    bool result = true;

    static const char * keys[] =
    {
        "text",            // 0
        "size"            // 1
    };
    static const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 2, 0, 1, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    int index = hash->Lookup( key );

    switch ( index )
    {
    case 0:
        {
            o.SetText( lua_tostring( L, valueIndex ) );
        }
        break;
    case 1:
        {
            o.SetSize( luaL_toreal( L, valueIndex ) );
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
LuaTextObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
