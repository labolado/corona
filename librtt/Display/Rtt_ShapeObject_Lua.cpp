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

#include "Display/Rtt_BitmapPaint.h"
#include "Display/Rtt_ClosedPath.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_DisplayDefaults.h"
#include "Display/Rtt_GradientPaint.h"
#include "Display/Rtt_LuaLibDisplay.h"
#include "Display/Rtt_Paint.h"
#include "Display/Rtt_RectPath.h"
#include "Display/Rtt_Shader.h"
#include "Display/Rtt_ShapeObject.h"
#include "Display/Rtt_TextureFactory.h"
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

const LuaShapeObjectProxyVTable&
LuaShapeObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaShapeObjectProxyVTable::setFillColor( lua_State *L )
{
    ShapeObject* o = (ShapeObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ShapeObject );

    if ( o )
    {
        if ( lua_istable( L, 2 ) )
        {
            GradientPaint *gradient = LuaLibDisplay::LuaNewGradientPaint( L, 2 );
            if ( gradient )
            {
                o->SetFill( gradient );

                // Early return
                return 0;
            }
        }

        if ( ! o->GetPath().GetFill() )
        {
            Paint* p = LuaLibDisplay::LuaNewColor( L, 2, o->IsByteColorRange() );
            o->SetFill( p );
        }
        else
        {
            Color c = LuaLibDisplay::toColor( L, 2, o->IsByteColorRange() );
            o->SetFillColor( c );
        }
    }

    return 0;
}

int
LuaShapeObjectProxyVTable::setStrokeColor( lua_State *L )
{
    ShapeObject* o = (ShapeObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ShapeObject );

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

// object.fill
int
LuaShapeObjectProxyVTable::setFill( lua_State *L, int valueIndex )
{
    ShapeObject* o = (ShapeObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ShapeObject );

    if ( Rtt_VERIFY( o ) )
    {
        if ( ! o->IsRestricted()
             || ! o->GetStage()->GetDisplay().ShouldRestrict( Display::kObjectFill ) )
        {
            // Use factory method to create paint
            Paint *paint = LuaLibDisplay::LuaNewPaint( L, valueIndex );

            o->SetFill( paint );
        }
    }
    return 0;
}

// object.stroke
int
LuaShapeObjectProxyVTable::setStroke( lua_State *L, int valueIndex )
{
    ShapeObject* o = (ShapeObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ShapeObject );

    if ( Rtt_VERIFY( o ) )
    {
        if ( ! o->IsRestricted()
             || ! o->GetStage()->GetDisplay().ShouldRestrict( Display::kObjectStroke ) )
        {
            // Use factory method to create paint
            Paint *paint = LuaLibDisplay::LuaNewPaint( L, valueIndex );

            o->SetStroke( paint );
        }
    }
    return 0;
}

int
LuaShapeObjectProxyVTable::setFillVertexColor( lua_State *L )
{
    ShapeObject* o = (ShapeObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ShapeObject );

    if ( o )
    {
        ShapePath& path = static_cast< ShapePath& >( o->GetPath() );
        if ( ! path.GetFill() )
        {
            o->SetFill( DefaultPaint( L, o->IsByteColorRange() ) );
        }

        U32 index = lua_tointeger( L, 2 ) - 1U;
        Color c = LuaLibDisplay::toColor( L, 3, o->IsByteColorRange() );

        if (path.SetFillVertexColor( index, c ))
        {
            path.GetObserver()->Invalidate( DisplayObject::kGeometryFlag | DisplayObject::kColorFlag );
        }
    }

    return 0;
}

int
LuaShapeObjectProxyVTable::setStrokeVertexColor( lua_State *L )
{
    ShapeObject* o = (ShapeObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ShapeObject );

    if ( o )
    {
        ShapePath& path = static_cast< ShapePath& >( o->GetPath() );
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

static const DisplayPath::ExtensionAdapter&
ExtensionAdapterFillConstant()
{
    static const DisplayPath::ExtensionAdapter kAdapter( true );
    return kAdapter;
}

int
LuaShapeObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }
    
    int result = 1;

    static const char * keys[] =
    {
        "path",                // 0
        "fill",                // 1
        "stroke",            // 2
        "blendMode",        // 3
        "setFillColor",        // 4
        "setStrokeColor",    // 5
        "strokeWidth",        // 6
        "innerstrokeWidth",    // 7
        "setFillVertexColor",    // 8
        "fillVertexCount",        // 9
        "setStrokeVertexColor", // 10
        "strokeVertexCount",    // 11
        "fillExtendedData",     // 12
        "strokeExtendedData",   // 13
    };
    const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 14, 19, 2, __FILE__, __LINE__ );
    StringHash *hash = &sHash;
    int index = hash->Lookup( key );

    // ShapeObject* o = (ShapeObject*)LuaProxy::GetProxyableObject( L, 1 );
    const ShapeObject& o = static_cast< const ShapeObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ShapeObject );

    switch ( index )
    {
    case 0:
        {
            if ( overrideRestriction
                 || ! o.IsRestricted()
                 || ! o.GetStage()->GetDisplay().ShouldRestrict( Display::kObjectPath ) )
            {
                o.GetPath().PushProxy( L );
            }
            else
            {
                lua_pushnil( L );
            }
        }
        break;
    case 1:
        {
            if ( overrideRestriction
                 || ! o.IsRestricted()
                 || ! o.GetStage()->GetDisplay().ShouldRestrict( Display::kObjectFill ) )
            {
                const Paint *paint = o.GetPath().GetFill();
                if ( paint )
                {
                    paint->PushProxy( L );
                }
                else
                {
                    lua_pushnil( L );
                }
            }
            else
            {
                lua_pushnil( L );
            }
        }
        break;
    case 2:
        {
            if ( overrideRestriction
                 || ! o.IsRestricted()
                 || ! o.GetStage()->GetDisplay().ShouldRestrict( Display::kObjectStroke ) )
            {
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
            else
            {
                lua_pushnil( L );
            }
        }
        break;
    case 3:
        {
            RenderTypes::BlendType blend = o.GetBlend();
            lua_pushstring( L, RenderTypes::StringForBlendType( blend ) );
        }
        break;
    case 4:
        {
            Lua::PushCachedFunction( L, Self::setFillColor );
        }
        break;
    case 5:
        {
            Lua::PushCachedFunction( L, Self::setStrokeColor );
        }
        break;
    case 6:
        {
            lua_pushinteger( L, o.GetStrokeWidth() );
        }
        break;
    case 7:
        {
            lua_pushinteger( L, o.GetInnerStrokeWidth() );
        }
        break;
    case 8:
        {
            Lua::PushCachedFunction( L, Self::setFillVertexColor );
        }
        break;
    case 9:
        {
            lua_pushinteger( L, static_cast< const ShapePath& >( o.GetPath() ).GetFillVertexCount() );
        }
        break;
    case 10:
        {
            Lua::PushCachedFunction( L, Self::setStrokeVertexColor );
        }
        break;
    case 11:
        {
            lua_pushinteger( L, static_cast< const ShapePath& >( o.GetPath() ).GetStrokeVertexCount() );
        }
        break;
    case 12:
    case 13:
        {
            bool isFill = 12 == index;
            const ShapePath& path = static_cast< const ShapePath& >( o.GetPath() );
            Geometry* geometry = isFill ? path.GetFillGeometry() : path.GetStrokeGeometry();
            Geometry::ExtensionBlock* block = geometry->GetExtensionBlock();
                   
            if ( block )
            {
                if (!block->fProxy)
                {
                    block->fProxy = LuaUserdataProxy::New( L, const_cast< ShapeObject* >( &o ) );
                    block->fProxy->SetAdapter( isFill ? &ExtensionAdapterFillConstant() : &ExtensionAdapterStrokeConstant() );
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

    // Because this is effectively a derived class, we will have successfully gotten a value
    // for the "_properties" key from the parent and we now need to combine that with the
    // properties of the child
    if (result == 1 && strcmp( key, "_properties" ) == 0 )
    {
        String properties(LuaContext::GetRuntime( L )->Allocator());
        const char *prefix = "";
        const char *postfix = "";

        DumpObjectProperties( L, object, keys, numKeys, properties );

        // Some objects are derived from "ShapeObjects" but some are just "ShapeObjects and
        // we need to emit complete JSON in those cases so we add the enclosing braces if
        // this is a "ShapeObject"
        if (strcmp(o.GetObjectDesc(), "ShapeObject") == 0 || strcmp(o.GetObjectDesc(), "ImageObject") == 0)
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

static void
AssignDefaultStrokeColor( lua_State *L, ShapeObject& o )
{
    if ( ! o.GetPath().GetStroke() )
    {
        const Runtime* runtime = LuaContext::GetRuntime( L );
        
        SharedPtr< TextureResource > resource = runtime->GetDisplay().GetTextureFactory().GetDefault();
        Paint *p = Paint::NewColor(
                            runtime->Allocator(),
                            resource, runtime->GetDisplay().GetDefaults().GetStrokeColor() );
        o.SetStroke( p );
    }
}

bool
LuaShapeObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    // ShapeObject* o = (ShapeObject*)LuaProxy::GetProxyableObject( L, 1 );
    ShapeObject& o = static_cast< ShapeObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ShapeObject );

    bool result = true;

    static const char * keys[] =
    {
        "fill",                // 0
        "stroke",            // 1
        "blendMode",        // 2
        "strokeWidth",        // 3
        "innerStrokeWidth", // 4
        "fillExtension",     // 5
        "strokeExtension"    // 6
    };
    const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 7, 10, 2, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    int index = hash->Lookup( key );
    switch ( index )
    {
    case 0:
        {
            setFill( L, valueIndex );
        }
        break;
    case 1:
        {
            setStroke( L, valueIndex );
        }
        break;
    case 2:
        {
            const char *v = lua_tostring( L, valueIndex );
            RenderTypes::BlendType blend = RenderTypes::BlendTypeForString( v );
            if ( RenderTypes::IsRestrictedBlendType( blend ) )
            {
                if ( o.IsRestricted()
                     && o.GetStage()->GetDisplay().ShouldRestrict( Display::kObjectBlendMode ) )
                {
                    CoronaLuaWarning(L, "using 'normal' blend because '%s' is a premium feature",
                        RenderTypes::StringForBlendType( blend ) );
                    blend = RenderTypes::kNormal;
                }
            }
            o.SetBlend( blend );
        }
        break;
    case 3:
        {
            U8 width = lua_tointeger( L, valueIndex );

            U8 innerWidth = width >> 1;
            o.SetInnerStrokeWidth( innerWidth );

            U8 outerWidth = width - innerWidth;
            o.SetOuterStrokeWidth( outerWidth );

            AssignDefaultStrokeColor( L, o );
        }
        break;
    case 4:
        {
            o.SetInnerStrokeWidth( lua_tointeger( L, valueIndex ) );

            AssignDefaultStrokeColor( L, o );
        }
        break;
    case 5:
    case 6:
        {
            bool isFill = 5 == index;
            ShapePath& path = static_cast< ShapePath& >( o.GetPath() );
            Geometry* geometry = isFill ? path.GetFillGeometry() : path.GetStrokeGeometry();
            const char* name = lua_tostring( L, valueIndex );
            SharedPtr<FormatExtensionList>* extensionList = LuaContext::GetRuntime( L )->GetDisplay().GetShaderFactory().GetExtensionList( name );
            
            if (extensionList)
            {
                if (!geometry->GetExtensionList())
                {
                    Geometry::ExtensionBlock* block = geometry->EnsureExtension();
                    
                    block->SetExtensionList( *extensionList );
                    block->UpdateData( geometry->GetStoredOnGPU(), isFill ? path.GetFillVertexCount() : path.GetStrokeVertexCount() );
                    path.GetObserver()->Invalidate( DisplayObject::kGeometryFlag );
                }
                
                else
                {
                    CoronaLuaWarning( L, "Shape object already has a %s extension.\n", isFill ? "fill" : "stroke" );
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
LuaShapeObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
