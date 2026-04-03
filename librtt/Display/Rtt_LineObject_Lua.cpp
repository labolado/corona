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
#include "Display/Rtt_LineObject.h"
#include "Display/Rtt_LuaLibDisplay.h"
#include "Display/Rtt_Paint.h"
#include "Display/Rtt_Shader.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_Runtime.h"
#include "CoronaLua.h"

#include "Core/Rtt_StringHash.h"

#include <string.h>

#include "Rtt_Lua.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

bool
DisplayPath::ExtensionAdapter::IsLineObject( const DisplayObject* object )
{
    const LuaProxyVTable* vtable = &object->ProxyVTable();
    
    return &LuaLineObjectProxyVTable::Constant() == vtable;
}

bool
DisplayPath::ExtensionAdapter::IsFillPaint( const DisplayObject* object, const Paint* paint )
{
    return !IsLineObject( object ) && static_cast< const ShapeObject* >( object )->GetPath().GetFill() == paint;

}

Geometry*
DisplayPath::ExtensionAdapter::GetGeometry( DisplayObject* object, bool isFill )
{
    if (IsLineObject( object ))
    {
        return static_cast< LineObject* >( object )->GetPath().GetStrokeGeometry();
    }
    
    else
    {
        ClosedPath& path = static_cast< ShapeObject* >( object )->GetPath();
        ShapePath& shapePath = static_cast< ShapePath& >( path );

        return isFill ? shapePath.GetFillGeometry() : shapePath.GetStrokeGeometry();
    }
}

const Geometry *
DisplayPath::ExtensionAdapter::GetGeometry( const DisplayObject* object, bool isFill )
{
    if (IsLineObject( object ))
    {
        return static_cast< const LineObject* >( object )->GetPath().GetStrokeGeometry();
    }
    
    else
    {
        const ClosedPath& path = static_cast< const ShapeObject* >( object )->GetPath();
        const ShapePath& shapePath = static_cast< const ShapePath& >( path );

        return isFill ? shapePath.GetFillGeometry() : shapePath.GetStrokeGeometry();
    }
}

static const DisplayPath::ExtensionAdapter&
ExtensionAdapterStrokeConstant()
{
    static const DisplayPath::ExtensionAdapter kAdapter( false );
    return kAdapter;
}

const LuaLineObjectProxyVTable&
LuaLineObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaLineObjectProxyVTable::setStrokeColor( lua_State *L )
{
    LineObject* o = (LineObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );

    if ( o )
    {
        if ( ! o->GetPath().GetStroke() )
        {
            Paint* p = LuaLibDisplay::LuaNewColor( L, 2, o->IsByteColorRange() );
            o->SetStroke( p );
        }
        else
        {
            Color c = LuaLibDisplay::toColor( L, 2, o->IsByteColorRange() );
            o->SetStrokeColor( c );
        }
    }

    return 0;
}

static Paint*
DefaultPaint( lua_State *L, bool isBytes )
{
    lua_pushnumber( L, 1.0 ); // gray value, i.e. white

    Paint* p = LuaLibDisplay::LuaNewColor( L, lua_gettop( L ), isBytes );

    lua_pop( L, 1 );

    return p;
}

int
LuaLineObjectProxyVTable::setStrokeVertexColor( lua_State *L )
{
    LineObject* o = (LineObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );

    if ( o )
    {
        OpenPath& path = o->GetPath();
        if ( ! path.GetStroke() )
        {
            o->SetStroke( DefaultPaint( L, o->IsByteColorRange() ) );
        }

        U32 index = lua_tointeger( L, 2 ) - 1U;
        Color c = LuaLibDisplay::toColor( L, 3, o->IsByteColorRange() );

        if (path.SetStrokeVertexColor( index, c ))
        {
            path.GetObserver()->Invalidate( DisplayObject::kGeometryFlag | DisplayObject::kColorFlag );
        }
    }

    return 0;
}

// object.stroke
int
LuaLineObjectProxyVTable::setStroke( lua_State *L )
{
    // This thin wrapper is necessary for Lua::PushCachedFunction().
    return setStroke( L, 2 );
}

// object.stroke
int
LuaLineObjectProxyVTable::setStroke( lua_State *L, int valueIndex )
{
    LineObject* o = (LineObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );

    if ( Rtt_VERIFY( o ) )
    {
        if ( ! o->IsRestricted()
             || ! o->GetStage()->GetDisplay().ShouldRestrict( Display::kLineStroke ) )
        {
            // Use factory method to create paint
            Paint *paint = LuaLibDisplay::LuaNewPaint( L, valueIndex );

            o->SetStroke( paint );
        }
    }
    return 0;
}

int
LuaLineObjectProxyVTable::append( lua_State *L )
{
    LineObject* o = (LineObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );

    if ( o )
    {
        // number of parameters (excluding self)
        int numArgs = lua_gettop( L ) - 1;

        // iMax must be even
        for ( int i = 2, iMax = (numArgs & ~0x1); i <= iMax; i+=2 )
        {
            Vertex2 v = { luaL_checkreal( L, i ), luaL_checkreal( L, i + 1 ) };
            o->Append( v );
        }

        Geometry* geometry = o->GetPath().GetStrokeGeometry();
        Geometry::ExtensionBlock* block = geometry->GetExtensionBlock();
        
        if (block)
        {
            block->UpdateData( geometry->GetStoredOnGPU(), o->GetPath().GetStrokeVertexCount() );
        }
    }

    return 0;
}

int
LuaLineObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }
    
    int result = 1;

    // deprecated properties have a trailing '#'
    static const char * keys[] =
    {
        "setColor#",        // 0 - DEPRECATED
        "setStrokeColor",    // 1
        "setStroke",        // 2
        "append",            // 3
        "blendMode",        // 4
        "width#",            // 5 - DEPRECATED
        "strokeWidth",        // 6
        "stroke",            // 7
        "anchorSegments",    // 8
        "setStrokeVertexColor", // 9
        "strokeVertexCount",    // 10
        "extendedData"     // 11
    };
    const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 12, 20, 2, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    int index = hash->Lookup( key );
    switch ( index )
    {
    case 0:
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
        if (! static_cast< const LineObject& >( object ).IsV1Compatibility())
        {
            CoronaLuaWarning(L, "line:setColor() is deprecated. Use line:setStrokeColor() instead");
        }
#endif
        // Fall through
    case 1:
        {
            Lua::PushCachedFunction( L, Self::setStrokeColor );
        }
        break;
    case 2:
        {
            Lua::PushCachedFunction( L, Self::setStroke );
        }
        break;
    case 3:
        {
            Lua::PushCachedFunction( L, Self::append );
        }
        break;
    case 4:
        {
            const LineObject& o = static_cast< const LineObject& >( object );
            RenderTypes::BlendType blend = o.GetBlend();
            lua_pushstring( L, RenderTypes::StringForBlendType( blend ) );
        }
        break;
    case 5:
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
            if (! static_cast< const LineObject& >( object ).IsV1Compatibility())
            {
                CoronaLuaWarning(L, "line.width is deprecated. Use line.strokeWidth");
            }
#endif
        // fall through
    case 6:
        {
            const LineObject& o = static_cast< const LineObject& >( object );
            Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );
            lua_pushnumber( L, Rtt_RealToFloat( o.GetStrokeWidth() ) );
        }
        break;
    case 7:
        {
            const LineObject& o = static_cast< const LineObject& >( object );
            Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );
            const Paint *paint = o.GetPath().GetStroke();
            if ( paint )
            {
                paint->PushProxy( L );
            }
            else
            {
                lua_pushnil( L );
            }
        }
        break;
    case 8:
        {
            const LineObject& o = static_cast< const LineObject& >( object );
            Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );
            lua_pushboolean( L, o.ShouldOffsetWithAnchor() );
            result = 1;
        }
        break;
    case 9:
        {
            Lua::PushCachedFunction( L, Self::setStrokeVertexColor );
        }
        break;
    case 10:
        {
            const LineObject& o = static_cast< const LineObject& >( object );
            lua_pushinteger( L, o.GetPath().GetStrokeVertexCount() );
        }
        break;
    case 11:
        {
            const LineObject& line = static_cast< const LineObject& >( object );
            const OpenPath& path = line.GetPath();
            Geometry::ExtensionBlock* block = path.GetStrokeGeometry()->GetExtensionBlock();
                    
            if ( block )
            {
                if (!block->fProxy)
                {
                    block->fProxy = LuaUserdataProxy::New( L, const_cast< LineObject* >( &line ) );
                    block->fProxy->SetAdapter( &ExtensionAdapterStrokeConstant() );
                }
            
                block->fProxy->Push( L );
            }
            else
            {
                lua_pushnil( L );
            }
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
        String lineProperties(LuaContext::GetRuntime( L )->Allocator());

        DumpObjectProperties( L, object, keys, numKeys, lineProperties );

        lua_pushfstring( L, "{ %s, %s }", lineProperties.GetString(), lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

bool
LuaLineObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    bool result = true;

    // LineObject* o = (LineObject*)LuaProxy::GetProxyableObject( L, 1 );
    LineObject& o = static_cast< LineObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );

    static const char * keys[] =
    {
        "setColor",            // 0
        "setStrokeColor",    // 1
        "setStroke",        // 2
        "append",            // 3
        "blendMode",        // 4
        "width",            // 5
        "strokeWidth",        // 6
        "stroke",            // 7
        "anchorSegments",    // 8
        "strokeVertexCount",    // 9
        "setStrokeVertexColor",    // 10
        "strokeExtension"    // 11
    };
    const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 12, 3, 3, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    int index = hash->Lookup( key );
    switch ( index )
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 10:
        // No-op: cannot set property for method
        break;
    case 4:
        {
            const char *v = lua_tostring( L, valueIndex );
            RenderTypes::BlendType blend = RenderTypes::BlendTypeForString( v );
            if ( RenderTypes::IsRestrictedBlendType( blend ) )
            {
                if ( o.IsRestricted()
                     && o.GetStage()->GetDisplay().ShouldRestrict( Display::kLineBlendMode ) )
                {
                    CoronaLuaWarning(L, "using 'normal' blend because '%s' is a premium feature",
                        RenderTypes::StringForBlendType( blend ) );
                    blend = RenderTypes::kNormal;
                }
            }
            o.SetBlend( blend );
        }
        break;
    case 5:
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
            if (! o.IsV1Compatibility())
            {
                CoronaLuaWarning(L, "line.width is deprecated. Use line.strokeWidth");
            }
#endif
        // fall through
    case 6:
        {
            o.SetStrokeWidth( luaL_toreal( L, valueIndex ) );
        }
        break;
    case 7:
        {
            setStroke( L, valueIndex );
        }
       break;
    case 8:
        {
            setAnchorSegments( L, valueIndex );
        }
       break;
    case 9:
        // No-op: cannot set vertex count
        break;
    case 11:
        {
            OpenPath& path = o.GetPath();
            Geometry* geometry = path.GetStrokeGeometry();
            const char* name = lua_tostring( L, valueIndex );
            SharedPtr<FormatExtensionList>* extensionList = LuaContext::GetRuntime( L )->GetDisplay().GetShaderFactory().GetExtensionList( name );
            
            if (extensionList)
            {
                if (!geometry->GetExtensionList())
                {
                    Geometry::ExtensionBlock* block = geometry->EnsureExtension();
                    
                    block->SetExtensionList( *extensionList );
                    block->UpdateData( geometry->GetStoredOnGPU(), path.GetStrokeVertexCount() );
                    path.GetObserver()->Invalidate( DisplayObject::kGeometryFlag );
                }
                
                else
                {
                    CoronaLuaWarning( L, "Line object already has a stroke extension.\n" );
                }
            }
            
            else
            {
                CoronaLuaWarning( L, "Unable to find attribute `%s`", name );
            }
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
LuaLineObjectProxyVTable::Parent() const
{
    return Super::Constant();
}

int
LuaLineObjectProxyVTable::setAnchorSegments( lua_State *L, int valueIndex )
{
    LineObject* o = (LineObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, LineObject );

    if( Rtt_VERIFY( o ) )
    {
        o->SetAnchorSegments( lua_toboolean( L, valueIndex ) );
    }
    return 0;
}


} // namespace Rtt

// ----------------------------------------------------------------------------
