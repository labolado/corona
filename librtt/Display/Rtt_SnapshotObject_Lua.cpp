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

const LuaSnapshotObjectProxyVTable&
LuaSnapshotObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaSnapshotObjectProxyVTable::Invalidate( lua_State *L )
{
    SnapshotObject* o = (SnapshotObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, SnapshotObject );

    if ( o )
    {
        const char *value = lua_tostring( L, 2 );
        SnapshotObject::RenderFlag flag = SnapshotObject::RenderFlagForString( value );
        o->SetDirty( flag );
    }

    return 0;
}

static StringHash *
GetSnapshotHash( lua_State *L )
{
    static const char *keys[] =
    {
        "group",            // 0 (read-only)
        "invalidate",        // 1 (read-only)
        "textureFilter",    // 2
        "textureWrapX",        // 3
        "textureWrapY",        // 4
        "clearColor",        // 5
        "canvas",            // 6 (read-only)
        "canvasMode",        // 7
    };
    const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 8, 6, 1, __FILE__, __LINE__ );
    return &sHash;
}

int
LuaSnapshotObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key )
    {
        return 0;
    }
    
    int result = 1;

    StringHash *sHash = GetSnapshotHash( L );

    const SnapshotObject& o = static_cast< const SnapshotObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, SnapshotObject );

    int index = sHash->Lookup( key );

    switch ( index )
    {
    case 0:
        {
            o.GetGroup().GetProxy()->PushTable( L );
            result = 1;
        }
        break;
    case 1:
        {
            Lua::PushCachedFunction( L, Self::Invalidate );
        }
        break;
    case 2:
        {
            const char *str = RenderTypes::StringForTextureFilter( o.GetTextureFilter() );
            lua_pushstring( L, str );
         }
        break;
    case 3:
        {
            const char *str = RenderTypes::StringForTextureWrap( o.GetTextureWrapX() );
            lua_pushstring( L, str );
        }
        break;
    case 4:
        {
            const char *str = RenderTypes::StringForTextureWrap( o.GetTextureWrapY() );
            lua_pushstring( L, str );
        }
        break;
    case 5:
        {
            result = LuaLibDisplay::PushColorChannels( L, o.GetClearColor(), false );
        }
        break;
    case 6:
        {
            o.GetCanvas().GetProxy()->PushTable( L );
        }
        break;
    case 7:
        {
            const char *str = SnapshotObject::StringForCanvasMode( o.GetCanvasMode() );
            lua_pushstring( L, str );
        }
        break;
    default:
        {
            result = Super::ValueForKey( L, object, key, overrideRestriction );
        }
        break;
    }

    // Because this is effectively a derived class, we will have successfully gotten a value
    // for the "_properties" key from the parent and we now need to combine that with the
    // properties of the child
    if ( result == 1 && strcmp( key, "_properties" ) == 0 )
    {
        String snapshotProperties(LuaContext::GetRuntime( L )->Allocator());
        const char **keys = NULL;
        const int numKeys = sHash->GetKeys(keys);

        DumpObjectProperties( L, object, keys, numKeys, snapshotProperties );

        lua_pushfstring( L, "{ %s, %s }", snapshotProperties.GetString(), lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

bool
LuaSnapshotObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    bool result = true;

    StringHash *sHash = GetSnapshotHash( L );

    SnapshotObject& o = static_cast< SnapshotObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, SnapshotObject );

    int index = sHash->Lookup( key );

    switch ( index )
    {
    case 0:
    case 1:
    case 6:
        {
            // No-op for read-only property
            CoronaLuaWarning(L, "the '%s' property of snapshot objects is read-only", key);
        }
        break;
    case 2:
        {
            const char *str = lua_tostring( L, valueIndex );
            o.SetTextureFilter( RenderTypes::TextureFilterForString( str ) );
        }
        break;
    case 3:
        {
            const char *str = lua_tostring( L, valueIndex );
            o.SetTextureWrapX( RenderTypes::TextureWrapForString( str ) );
        }
        break;
    case 4:
        {
            const char *str = lua_tostring( L, valueIndex );
            o.SetTextureWrapY( RenderTypes::TextureWrapForString( str ) );
        }
        break;
    case 5:
        {
            Color c = ColorZero();
            LuaLibDisplay::ArrayToColor( L, valueIndex, c, false );
            o.SetClearColor( c );
        }
        break;
    case 7:
        {
            const char *str = lua_tostring( L, valueIndex );
            o.SetCanvasMode( SnapshotObject::CanvasModeForString( str ) );
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
LuaSnapshotObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
