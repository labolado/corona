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
#include "Display/Rtt_GradientPaint.h"
#include "Display/Rtt_LineObject.h"
#include "Display/Rtt_LuaLibDisplay.h"
#include "Display/Rtt_Paint.h"
#include "Display/Rtt_RectPath.h"
#include "Display/Rtt_Shader.h"
#include "Display/Rtt_ShaderFactory.h"
#include "Display/Rtt_ShapeObject.h"
#include "Display/Rtt_StageObject.h"
#include "Display/Rtt_TextureFactory.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_MPlatformDevice.h"
#include "Rtt_PlatformDisplayObject.h"
#include "Rtt_Runtime.h"
#include "Rtt_PhysicsWorld.h"
#include "CoronaLua.h"

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
// LuaDisplayObjectProxyVTable
//
// ----------------------------------------------------------------------------

/*
#define Rtt_ASSERT_PROXY_TYPE( L, index, T )    \
    Rtt_ASSERT(                                    \
        IsProxyUsingCompatibleDelegate(            \
            LuaProxy::GetProxy((L),(index)),    \
            Lua ## T ## ProxyVTable::Constant() ) )


#define Rtt_WARN_SIM_PROXY_TYPE2( L, index, T, API_T )        \
    Rtt_WARN_SIM(                                            \
        Rtt_VERIFY( IsProxyUsingCompatibleDelegate(            \
            LuaProxy::GetProxy((L),(index)),                \
            Lua ## T ## ProxyVTable::Constant() ) ),        \
        ( "ERROR: Argument(%d) is not of a %s\n", index, #API_T ) )

#define Rtt_WARN_SIM_PROXY_TYPE( L, index, T )    Rtt_WARN_SIM_PROXY_TYPE2( L, index, T, T )
*/

const LuaDisplayObjectProxyVTable&
LuaDisplayObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaDisplayObjectProxyVTable::translate( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );
    if ( o )
    {
        Real x = luaL_checkreal( L, 2 );
        Real y = luaL_checkreal( L, 3 );

        o->Translate( x, y );
    }

    return 0;
}

int
LuaDisplayObjectProxyVTable::scale( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( o )
    {
        Real sx = luaL_checkreal( L, 2 );
        Real sy = luaL_checkreal( L, 3 );

        o->Scale( sx, sy, false );
    }

    return 0;
}

int
LuaDisplayObjectProxyVTable::rotate( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( o )
    {
        Real deltaTheta = luaL_checkreal( L, 2 );

        o->Rotate( deltaTheta );
    }

    return 0;
}

static int
getParent( lua_State *L )
{
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
    CoronaLuaWarning(L, "[Deprecated display object API] Replace object:getParent() with object.parent");
#endif

    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    int result = o ? 1 : 0;
    if ( o )
    {
        // Orphans do not have parents
        GroupObject* parent = ( ! o->IsOrphan() ? o->GetParent() : NULL );
        if ( parent )
        {
            parent->GetProxy()->PushTable( L );
        }
        else
        {
            Rtt_ASSERT( o->GetStage() == o || o->IsOrphan() );
            lua_pushnil( L );
        }
    }

    return result;
}

static int
setReferencePoint( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    int result = 0;
    if ( o )
    {
        if ( ! o->IsV1Compatibility() )
        {
            luaL_error( L, "ERROR: object:setReferencePoint() is only available in graphicsCompatibility 1.0 mode. Use anchor points instead." );
        }
        else
        {
            bool anchorChildren = true;
        
            if ( lua_isnil( L, 2 ) )
            {
                Rtt_TRACE_SIM( ( "WARNING: object:setReferencePoint() was given a 'nil' value. The behavior is not defined.\n" ) );
                o->ResetReferencePoint();

                anchorChildren = false; // Restore to base case.
            }
            else
            {
                Rtt_WARN_SIM( lua_islightuserdata( L, 2 ), ( "WARNING: Invalid reference point constant passed to object:setReferencePoint()\n" ) );

                DisplayObject::ReferencePoint location = (DisplayObject::ReferencePoint)EnumForUserdata(
                    LuaLibDisplay::ReferencePoints(),
                    lua_touserdata( L, 2 ),
                    DisplayObject::kNumReferencePoints,
                    DisplayObject::kReferenceCenter );
                o->SetReferencePoint( LuaContext::GetRuntime( L )->Allocator(), location );
            }
            
            GroupObject *g = o->AsGroupObject();
            if ( g )
            {
                g->SetAnchorChildren( anchorChildren );
            }
        }
    }

    return result;
}

// object:removeSelf()
static int
removeSelf( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );
    int result = 0;

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( o )
    {
        if ( ! o->IsRenderedOffScreen() )
        {
            GroupObject* parent = o->GetParent();

            if (parent != NULL)
            {
                S32 index = parent->Find( *o );

                LuaDisplayObjectProxyVTable::PushAndRemove( L, parent, index );

                result = 1;
            }
            else
            {
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
                CoronaLuaWarning(L, "object:removeSelf() cannot be called on objects with no parent" );
#endif
            }
        }
        else
        {
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
            CoronaLuaWarning(L, "object:removeSelf() can only be called on objects in the scene graph. Objects that are not directly in the scene, such as a snapshot's group, cannot be removed directly" );
#endif
        }
    }
    else
    {
        lua_pushnil( L );
        result = 1;
    }

    return result;
}

static int
localToContent( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    int result = 0;
    if ( o )
    {
        Real x = luaL_checkreal( L, 2 );
        Real y = luaL_checkreal( L, 3 );

        Vertex2 v = { x, y };
        o->LocalToContent( v );

        lua_pushnumber( L, v.x );
        lua_pushnumber( L, v.y );
        result = 2;
    }

    return result;
}

static int
contentToLocal( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( Rtt_VERIFY( o ) )
    {
        Vertex2 v = { luaL_toreal( L, 2 ), luaL_toreal( L, 3 ) };
        o->ContentToLocal( v );
        lua_pushnumber( L, Rtt_RealToFloat( v.x ) );
        lua_pushnumber( L, Rtt_RealToFloat( v.y ) );
    }

    return 2;
}

// object:toFront()
static int
toFront( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( Rtt_VERIFY( o ) )
    {
        GroupObject *parent = o->GetParent();

        if (parent != NULL)
        {
            parent->Insert( -1, o, false );
        }
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
        else
        {
            CoronaLuaWarning(L, "DisplayObject:toFront() cannot be used on a snapshot group or texture canvas cache");
        }
#endif
    }
    return 0;
}

// object:toBack()
static int
toBack( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( Rtt_VERIFY( o ) )
    {
        GroupObject *parent = o->GetParent();

        if (parent != NULL)
        {
            parent->Insert( 0, o, false );
        }
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
        else
        {
            CoronaLuaWarning(L, "DisplayObject:toBack() cannot be used on a snapshot group or texture canvas cache");
        }
#endif
    }
    return 0;
}

// object:setMask( mask )
static int
setMask( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( Rtt_VERIFY( o ) )
    {
        Runtime *runtime = LuaContext::GetRuntime( L ); Rtt_ASSERT( runtime );

        BitmapMask *mask = NULL;

        if ( lua_isuserdata( L, 2 ) )
        {
            FilePath **ud = (FilePath **)luaL_checkudata( L, 2, FilePath::kMetatableName );
            if ( ud )
            {
                FilePath *maskData = *ud;
                if ( maskData )
                {
                    mask = BitmapMask::Create( * runtime, * maskData );
                }
            }
        }

        o->SetMask( runtime->Allocator(), mask );
    }
    return 0;
}

// object:_setHasListener( name, value )
static int
setHasListener( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( Rtt_VERIFY( o ) )
    {
        const char *name = lua_tostring( L, 2 );
        DisplayObject::ListenerMask mask = DisplayObject::MaskForString( name );
        if ( DisplayObject::kUnknownListener != mask )
        {
            bool value = lua_toboolean( L, 3 );
            o->SetHasListener( mask, value );
        }
    }
    return 0;
}

/*
int
LuaDisplayObjectProxyVTable::length( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    int len = 0;

    if ( o )
    {
        GroupObject* c = o->AsGroupObject();
        if ( c ) { len = c->NumChildren(); }
    }

    lua_pushinteger( L, len );

    return 1;
}
*/
/*
// TODO: too complicated; should break apart into smaller functions???
static int
moveAbove( lua_State *L )
{
    DisplayObject* childToMove = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );
    DisplayObject* dstLocation = (DisplayObject*)LuaProxy::GetProxyableObject( L, 2 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );
    Rtt_WARN_SIM_PROXY_TYPE( L, 2, DisplayObject );

    // Only move if dstLocation is above the child to be moved
    if ( dstLocation
         && childToMove
         && dstLocation->IsAbove( * childToMove ) )
    {
        const StageObject* dstLocationStage = dstLocation->GetStage();
        const StageObject* childToMoveStage = childToMove->GetStage();

        // Only move if neither objects are the canvas
        // And the dstLocation is in the canvas object tree
        if ( dstLocationStage && dstLocationStage != dstLocation
             && childToMoveStage != childToMove )
        {
            GroupObject* parent = dstLocation->GetParent();

            S32 index = parent->Find( *dstLocation );

            Rtt_ASSERT( index >= 0 );
            parent->Insert( index, childToMove );
        }
    }

    return 0;
}
*/

/*
int
LuaDisplayObjectProxyVTable::stageBounds( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( o )
    {
        const Rect& r = o->StageBounds();

        lua_createtable( L, 0, 4 );

        const char xMin[] = "xMin";
        const char yMin[] = "yMin";
        const char xMax[] = "xMax";
        const char yMax[] = "yMax";
        const size_t kLen = sizeof( xMin ) - 1;

        Rtt_STATIC_ASSERT( sizeof(char) == 1 );
        Rtt_STATIC_ASSERT( sizeof(xMin) == sizeof(yMin) );
        Rtt_STATIC_ASSERT( sizeof(xMin) == sizeof(xMax) );
        Rtt_STATIC_ASSERT( sizeof(xMin) == sizeof(yMax) );

        setProperty( L, xMin, kLen, r.xMin );
        setProperty( L, yMin, kLen, r.yMin );
        setProperty( L, xMax, kLen, r.xMax );
        setProperty( L, yMax, kLen, r.yMax );

        return 1;
    }

    return 0;
}

int
LuaDisplayObjectProxyVTable::stageWidth( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( o )
    {
        const Rect& r = o->StageBounds();
        lua_pushinteger( L, Rtt_RealToInt( r.xMax - r.xMin ) );
        return 1;
    }

    return 0;
}

int
LuaDisplayObjectProxyVTable::stageHeight( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    if ( o )
    {
        const Rect& r = o->StageBounds();
        lua_pushinteger( L, Rtt_RealToInt( r.yMax - r.yMin ) );
        return 1;
    }

    return 0;
}
*/

/*
int
LuaDisplayObjectProxyVTable::canvas( lua_State *L )
{
    DisplayObject* o = (DisplayObject*)LuaProxy::GetProxy( L, 1 )->GetProxyableObject();

    LuaStageObject::PushOrCreateProxy( L, o->GetStage() );

    return 1;
}
*/

int
LuaDisplayObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }
    
    int result = 1;

    // deprecated properties have a trailing '#'
    static const char * keys[] =
    {
        "translate",            // 0
        "scale",                // 1
        "rotate",                // 2
        "getParent",            // 3
        "setReferencePoint",    // 4
        "removeSelf",            // 5
        "localToContent",        // 6
        "contentToLocal",        // 7
        "stageBounds#",         // 8 - DEPRECATED
        "stageWidth#",            // 9 - DEPRECATED
        "stageHeight#",         // 10 - DEPRECATED
        "numChildren#",            // 11 - DEPRECATED
        "length#",                // 12 - DEPRECATED
        "isVisible",            // 13
        "isHitTestable",        // 14
        "alpha",                // 15
        "parent",                // 16
        "stage",                // 17
        "x",                    // 18
        "y",                    // 19
        "anchorX",                // 20
        "anchorY",                // 21
        "contentBounds",        // 22
        "contentWidth",         // 23
        "contentHeight",        // 24
        "toFront",                // 25
        "toBack",                // 26
        "setMask",                // 27
        "maskX",                // 28
        "maskY",                // 29
        "maskScaleX",            // 30
        "maskScaleY",            // 31
        "maskRotation",         // 32
        "isHitTestMasked",        // 33
        "_setHasListener",        // 34
    };
    const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 35, 33, 15, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    int index = hash->Lookup( key );
    switch ( index )
    {
    case 0:
        {
            Lua::PushCachedFunction( L, Self::translate );
        }
        break;
    case 1:
        {
            Lua::PushCachedFunction( L, Self::scale );
        }
        break;
    case 2:
        {
            Lua::PushCachedFunction( L, Self::rotate );
        }
        break;
    case 3:
        {
            Lua::PushCachedFunction( L, getParent );
        }
        break;
    case 4:
        {
            Lua::PushCachedFunction( L, setReferencePoint );
        }
        break;
    case 5:
        {
            Lua::PushCachedFunction( L, removeSelf );
        }
        break;
    case 6:
        {
            Lua::PushCachedFunction( L, localToContent );
        }
        break;
    case 7:
        {
            Lua::PushCachedFunction( L, contentToLocal );
        }
        break;
    case 25:
        {
            Lua::PushCachedFunction( L, toFront );
        }
        break;
    case 26:
        {
            Lua::PushCachedFunction( L, toBack );
        }
        break;
    case 27:
        {
            Lua::PushCachedFunction( L, setMask );
        }
        break;
    case 34:
        {
            Lua::PushCachedFunction( L, setHasListener );
        }
        break;
    default:
        {
            // DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );
            const DisplayObject& o = static_cast< const DisplayObject& >( object );
            Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

            switch ( index )
            {
            case 8:
            case 22:
                {
//                    Rtt_WARN_SIM( strcmp( "stageBounds", key ) != 0, ( "WARNING: object.stageBounds has been deprecated. Use object.contentBounds instead\n" ) );

                    const Rect& r = o.StageBounds();

    // Good way to catch autorotate bugs for bouncebehavior:
    // Rtt_ASSERT( r.xMin >= 0 );

                    lua_createtable( L, 0, 4 );

                    const char xMin[] = "xMin";
                    const char yMin[] = "yMin";
                    const char xMax[] = "xMax";
                    const char yMax[] = "yMax";
                    const size_t kLen = sizeof( xMin ) - 1;

                    Rtt_STATIC_ASSERT( sizeof(char) == 1 );
                    Rtt_STATIC_ASSERT( sizeof(xMin) == sizeof(yMin) );
                    Rtt_STATIC_ASSERT( sizeof(xMin) == sizeof(xMax) );
                    Rtt_STATIC_ASSERT( sizeof(xMin) == sizeof(yMax) );

                    Real xMinRect = r.xMin;
                    Real yMinRect = r.yMin;
                    Real xMaxRect = r.xMax;
                    Real yMaxRect = r.yMax;

                    if ( r.IsEmpty() )
                    {
                        xMinRect = yMinRect = xMaxRect = yMaxRect = Rtt_REAL_0;
                    }

                    setProperty( L, xMin, kLen, xMinRect );
                    setProperty( L, yMin, kLen, yMinRect );
                    setProperty( L, xMax, kLen, xMaxRect );
                    setProperty( L, yMax, kLen, yMaxRect );
                }
                break;
            case 9:
            case 23:
                {
                    Rtt_WARN_SIM( strcmp( "stageWidth", key ) != 0, ( "WARNING: object.stageWidth has been deprecated. Use object.contentWidth instead\n" ) );

                    const Rect& r = o.StageBounds();
                    lua_pushinteger( L, Rtt_RealToInt( r.xMax - r.xMin ) );
                }
                break;
            case 10:
            case 24:
                {
                    Rtt_WARN_SIM( strcmp( "stageHeight", key ) != 0, ( "WARNING: object.stageHeight has been deprecated. Use object.contentHeight instead\n" ) );

                    const Rect& r = o.StageBounds();
                    lua_pushinteger( L, Rtt_RealToInt( r.yMax - r.yMin ) );
                }
                break;
            case 11:
                {
                    lua_pushnil( L );
                }
                break;
            case 12:
                {
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
                    CoronaLuaWarning(L, "[Deprecated display object property] Replace object.length with group.numChildren");
#endif
                    int len = 0;
                    const GroupObject* c = const_cast< DisplayObject& >( o ).AsGroupObject();
                    if ( c ) { len = c->NumChildren(); }

                    lua_pushinteger( L, len );
                }
                break;
            case 13:
                {
                    lua_pushboolean( L, o.IsVisible() );
                }
                break;
            case 14:
                {
                    lua_pushboolean( L, o.IsHitTestable() );
                }
                break;
            case 15:
                {
                    lua_Number alpha = (float)o.Alpha() / 255.0;
                    lua_pushnumber( L, alpha );
                }
                break;
            case 16:
                {
                    const StageObject *stage = o.GetStage();

                    // Only onscreen objects have a parent
                    if ( stage
                         && ( stage->IsOnscreen() || stage->IsRenderedOffScreen() ) )
                    {
                        GroupObject* parent = o.GetParent();
                        if ( parent )
                        {
                            parent->GetProxy()->PushTable( L );
                        }
                        else
                        {
                            // Stage objects and objects rendered offscreen have no parent,
                            // so push nil
                            Rtt_ASSERT( o.IsRenderedOffScreen() || o.GetStage() == & o );
                            lua_pushnil( L );
                        }
                    }
                    else
                    {
                        // Objects that have been removed effectively have no parent,
                        // so push nil. Do NOT push the offscreen parent.
                        lua_pushnil( L );
                    }
                }
                break;
            case 17:
                {
                    const StageObject* stage = o.GetStage();
                    if ( stage && stage->IsOnscreen() )
                    {
                        stage->GetProxy()->PushTable( L );
                    }
                    else
                    {
                        lua_pushnil( L );
                    }
                }
                break;
            case 18:
                {
                    Rtt_Real value = o.GetGeometricProperty( kOriginX );

                    if ( o.IsV1Compatibility() && o.IsV1ReferencePointUsed() )
                    {
                        Vertex2 p = o.GetAnchorOffset();
                        value -= p.x;
                    }
                    lua_pushnumber( L, value );
                }
                break;
            case 19:
                {
                    Rtt_Real value = o.GetGeometricProperty( kOriginY );

                    if ( o.IsV1Compatibility() && o.IsV1ReferencePointUsed() )
                    {
                        Vertex2 p = o.GetAnchorOffset();
                        value -= p.y;
                    }
                    lua_pushnumber( L, value );
                }
                break;
            case 20:
                {
                    Real anchorX = o.GetAnchorX();
                    lua_pushnumber( L, anchorX );
                }
                break;
            case 21:
                {
                    Real anchorY = o.GetAnchorY();
                    lua_pushnumber( L, anchorY );
                }
                break;
            case 28:
                {
                    Rtt_Real value = o.GetMaskGeometricProperty( kOriginX );
                    lua_pushnumber( L, value );
                }
                break;
            case 29:
                {
                    Rtt_Real value = o.GetMaskGeometricProperty( kOriginY );
                    lua_pushnumber( L, value );
                }
                break;
            case 30:
                {
                    Rtt_Real value = o.GetMaskGeometricProperty( kScaleX );
                    lua_pushnumber( L, value );
                }
                break;
            case 31:
                {
                    Rtt_Real value = o.GetMaskGeometricProperty( kScaleY );
                    lua_pushnumber( L, value );
                }
                break;
            case 32:
                {
                    Rtt_Real value = o.GetMaskGeometricProperty( kRotation );
                    lua_pushnumber( L, value );
                }
                break;
            case 33:
                {
                    lua_pushboolean( L, o.IsHitTestMasked() );
                }
                break;

            default:
                {
                    GeometricProperty p = DisplayObject::PropertyForKey( LuaContext::GetAllocator( L ), key );
                    if ( p < kNumGeometricProperties )
                    {
                        lua_pushnumber( L, Rtt_RealToFloat( o.GetGeometricProperty( p ) ) );
                    }
                    else
                    {
                        result = 0;
                    }
                }
                break;
            }
        }
        break;
    }

    // We handle this outside the switch statement (and thus keys[]) so we can enumerate all the keys[] and not include it
    if (result == 0 && strncmp(key, "_properties", strlen(key)) == 0)
    {
        String displayProperties(LuaContext::GetRuntime( L )->Allocator());
        String geometricProperties(LuaContext::GetRuntime( L )->Allocator());
        const char **geometricKeys = NULL;
        const int geometricNumKeys = DisplayObject::KeysForProperties(geometricKeys);

        // "GeometricProperties" are derived from the object's geometry and thus handled separately from other properties
        const DisplayObject& o = static_cast< const DisplayObject& >( object );
        for ( int i = 0; i < geometricNumKeys; i++ )
        {
            const int bufLen = 10240;
            char buf[bufLen];

            GeometricProperty p = DisplayObject::PropertyForKey( LuaContext::GetAllocator( L ), geometricKeys[i] );

            if (strchr(geometricKeys[i], '#'))
            {
                // Deprecated property, skip it
                continue;
            }

            if ( p < kNumGeometricProperties )
            {
                snprintf(buf, bufLen, "\"%s\": %g", geometricKeys[i], Rtt_RealToFloat( o.GetGeometricProperty( p ) ) );

                if (! geometricProperties.IsEmpty() && strlen(buf) > 0)
                {
                    geometricProperties.Append(", ");
                }
                
                geometricProperties.Append(buf);
            }
        }

        DumpObjectProperties( L, object, keys, numKeys, displayProperties );

        const LuaProxyVTable *extensions = LuaProxy::GetProxy(L, 1)->GetExtensionsDelegate();
        if ( extensions )
        {
            result = extensions->ValueForKey( L, object, key );

            if (result == 1)
            {
                displayProperties.Append( ", " );
                displayProperties.Append( lua_tostring( L, -1 ) );
            }
        }

        lua_pushfstring( L, "%s, %s", geometricProperties.GetString(), displayProperties.GetString() );

        result = 1;
    }
    else if ( result == 0 && strcmp( key, "_type" ) == 0 )
    {
        const DisplayObject& o = static_cast< const DisplayObject& >( object );

        lua_pushstring( L, o.GetObjectDesc() );

        result = 1;
    }
    else if ( result == 0 && strcmp( key, "_defined" ) == 0 )
    {
        const DisplayObject& o = static_cast< const DisplayObject& >( object );

        lua_pushstring( L, o.fWhereDefined );

        result = 1;
    }
    else if ( result == 0 && strcmp( key, "_lastChange" ) == 0 )
    {
        const DisplayObject& o = static_cast< const DisplayObject& >( object );

        lua_pushstring( L, o.fWhereChanged );

        result = 1;
    }

    return result;
}

bool
LuaDisplayObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    // DisplayObject* o = (DisplayObject*)LuaProxy::GetProxyableObject( L, 1 );
    DisplayObject& o = static_cast< DisplayObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, DisplayObject );

    bool result = true;

    static const char * keys[] =
    {
        "isVisible",            // 0
        "isHitTestable",        // 1
        "alpha",                // 2
        "parent",                // 3
        "stage",                // 4
        "x",                    // 5
        "y",                    // 6
        "anchorX",                // 7
        "anchorY",                // 8
        "stageBounds",            // 9
        "maskX",                // 10
        "maskY",                // 11
        "maskScaleX",            // 12
        "maskScaleY",            // 13
        "maskRotation",         // 14
        "isHitTestMasked",        // 15
    };
    const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 16, 12, 6, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    int index = hash->Lookup( key );
    switch ( index )
    {
    case 0:
        {
            o.SetVisible( lua_toboolean( L, valueIndex ) != 0 );
        }
        break;
    case 1:
        {
            o.SetHitTestable( lua_toboolean( L, valueIndex ) != 0 );
        }
        break;
    case 2:
        {
            /* too verbose:
            Rtt_WARN_SIM(
                lua_tonumber( L, valueIndex ) >= 0. && lua_tonumber( L, valueIndex ) <= 1.0,
                ( "WARNING: Attempt to set object.alpha to %g which is outside valid range. It will be clamped to the range [0,1]\n", lua_tonumber( L, valueIndex ) ) );
             */

            // Explicitly declare T b/c of crappy gcc compiler used by Symbian
            lua_Integer alpha = (lua_Integer)(lua_tonumber( L, valueIndex ) * 255.0f);
            lua_Integer value = Min( (lua_Integer)255, alpha );
            U8 newValue = Max( (lua_Integer)0, value );

            o.SetAlpha( newValue );
        }
        break;
    case 3:
        {
            // No-op for read-only property
        }
        break;
    case 4:
        {
            // No-op for read-only property
        }
        break;
    case 5:
        {
            Real newValue = luaL_toreal( L, valueIndex );

            if ( o.IsV1Compatibility() && o.IsV1ReferencePointUsed() )
            {
                Vertex2 p = o.GetAnchorOffset();
                newValue += p.x;
            }

            o.SetGeometricProperty( kOriginX, newValue );
        }
        break;
    case 6:
        {
            Real newValue = luaL_toreal( L, valueIndex );

            if ( o.IsV1Compatibility() && o.IsV1ReferencePointUsed() )
            {
                Vertex2 p = o.GetAnchorOffset();
                newValue += p.y;
            }

            o.SetGeometricProperty( kOriginY, newValue );
        }
        break;
    case 7:
        {
            if ( lua_type( L, valueIndex ) == LUA_TNUMBER )
            {
                Real newValue = luaL_toreal( L, valueIndex );
                if ( o.GetStage()->GetDisplay().GetDefaults().IsAnchorClamped() )
                {
                    newValue = Clamp( newValue, Rtt_REAL_0, Rtt_REAL_1 );
                }
                o.SetAnchorX( newValue );
                
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
                if ( o.IsV1Compatibility() )
                {
                    CoronaLuaWarning(L, "o.anchorX is only supported in graphics 2.0. Your mileage may vary in graphicsCompatibility 1.0 mode");
                }
#endif
            }
            else
            {
                luaL_error( L, "ERROR: o.anchorX can only be set to a number.\n" );
            }
            
        }
        break;
    case 8:
        {
            if ( lua_type( L, valueIndex) == LUA_TNUMBER )
            {
                Real newValue = luaL_toreal( L, valueIndex );
                if ( o.GetStage()->GetDisplay().GetDefaults().IsAnchorClamped() )
                {
                    newValue = Clamp( newValue, Rtt_REAL_0, Rtt_REAL_1 );
                }
                o.SetAnchorY( newValue );
                
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
                if ( o.IsV1Compatibility() )
                {
                    CoronaLuaWarning(L, "o.anchorY is only supported in graphics 2.0. Your mileage may vary in graphicsCompatibility 1.0 mode");
                }
#endif
            }
            else
            {
                luaL_error( L, "ERROR: o.anchorY can only be set to a number.\n" );
            }
            
        }
        break;
    case 9:
        {
            // No-op for read-only keys
        }
        break;
    case 10:
        {
            Real newValue = luaL_toreal( L, valueIndex );
            o.SetMaskGeometricProperty( kOriginX, newValue );
        }
        break;
    case 11:
        {
            Real newValue = luaL_toreal( L, valueIndex );
            o.SetMaskGeometricProperty( kOriginY, newValue );
        }
        break;
    case 12:
        {
            Real newValue = luaL_toreal( L, valueIndex );
            o.SetMaskGeometricProperty( kScaleX, newValue );
        }
        break;
    case 13:
        {
            Real newValue = luaL_toreal( L, valueIndex );
            o.SetMaskGeometricProperty( kScaleY, newValue );
        }
        break;
    case 14:
        {
            Real newValue = luaL_toreal( L, valueIndex );
            o.SetMaskGeometricProperty( kRotation, newValue );
        }
        break;
    case 15:
        {
            o.SetHitTestMasked( lua_toboolean( L, valueIndex ) != 0 );
        }
        break;
    default:
        {
            GeometricProperty p = DisplayObject::PropertyForKey( LuaContext::GetAllocator( L ), key );
            if ( p < kNumGeometricProperties )
            {
                Real newValue = luaL_toreal( L, valueIndex );
                o.SetGeometricProperty( p, newValue );
            }
            else if ( ! lua_isnumber( L, 2 ) )
            {
                result = false;
            }
        }
        break;
    }

    // We changed a property so record where we are so that "_lastChange" will be available later to say where it happened
    // (this is a noop on non-debug builds because lua_where returns an empty string)
    if (result)
    {
        luaL_where(L, 1);
        const char *where = lua_tostring( L, -1 );

        if (where[0] != 0)
        {
            if (o.fWhereChanged != NULL)
            {
                free((void *) o.fWhereChanged);
            }

            // If this fails, the pointer will be NULL and that's handled gracefully
            o.fWhereChanged = strdup(where);
        }

        lua_pop(L, 1);
    }

    return result;
}


} // namespace Rtt

// ----------------------------------------------------------------------------
