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

// [OLD] stage:setFocus( object )
// [NEW] stage:setFocus( object [, touchId] )
int
LuaStageObjectProxyVTable::setFocus( lua_State *L )
{
    StageObject* o = (StageObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, StageObject );

    if ( o )
    {
        // By default, assume this is a call to set global focus (i.e. old behavior)
        bool isGlobal = true;
        DisplayObject* focus = NULL;

        if ( lua_istable( L, 2 ) )
        {
            focus = (DisplayObject*)LuaProxy::GetProxyableObject( L, 2 );
            Rtt_WARN_SIM_PROXY_TYPE( L, 2, DisplayObject );

            // If the optional touchId arg exists, then we are using the new behavior
            // If it doesn't, then isGlobal remains true and we use the old behavior
            if ( lua_type( L, 3 ) != LUA_TNONE )
            {
                const void *touchId = lua_touserdata( L, 3 );

                const MPlatformDevice& device = LuaContext::GetRuntime( L )->Platform().GetDevice();
                if ( device.DoesNotify( MPlatformDevice::kMultitouchEvent ) )
                {
                    // If optional parameter supplied, set per-object focus instead of global
                    isGlobal = false;
                    o->SetFocus( focus, touchId );
                }
                else
                {
                    // The new API maps to old behavior when we are *not* multitouch
                    if ( ! touchId )
                    {
                        focus = NULL;
                    }
                }
            }
        }

        if ( isGlobal )
        {
            o->SetFocus( focus );
        }
    }

    return 0;
}

const LuaStageObjectProxyVTable&
LuaStageObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaStageObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, StageObject );

    if ( ! key )
    {
        // If there's no key, we'll may have a table index to look up which is handled
        // by LuaGroupObjectProxyVTable::ValueForKey()
        return Super::ValueForKey( L, object, key );
    }

    int result = 1;

    static const char * keys[] =
    {
        "setFocus",            // 0
    };
    static const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 1, 0, 1, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    int index = hash->Lookup( key );
    switch ( index )
    {
        case 0:
            {
                Lua::PushCachedFunction( L, Self::setFocus );
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
        String stageProperties(LuaContext::GetRuntime( L )->Allocator());

        DumpObjectProperties( L, object, keys, numKeys, stageProperties );

        lua_pushfstring( L, "{ %s, %s }", stageProperties.GetString(), lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

const LuaProxyVTable&
LuaStageObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
