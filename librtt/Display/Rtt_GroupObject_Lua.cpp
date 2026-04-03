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
#include "Display/Rtt_DisplayObject.h"
#include "Display/Rtt_GroupObject.h"
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

const LuaGroupObjectProxyVTable&
LuaGroupObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaGroupObjectProxyVTable::Insert( lua_State *L, GroupObject *parent )
{
    int index = (int) lua_tointeger( L, 2 );

	ENABLE_SUMMED_TIMING( true );

    int childIndex = 3; // index of child object (table owned by proxy)
    if ( 0 == index )
    {
        // Optional index arg missing, so insert at end
        --childIndex;
        index = parent->NumChildren();
    }
    else
    {
        // Map Lua indices to C++ indices
        --index;
    }
    Rtt_ASSERT( index >= 0 );
    Rtt_ASSERT( lua_istable( L, childIndex ) );

    // Default to false if no arg specified
    bool resetTransform = lua_toboolean( L, childIndex + 1 ) != 0;

    DisplayObject* child = (DisplayObject*)LuaProxy::GetProxyableObject( L, childIndex );
    if ( child != parent )
    {
        if ( ! child->IsRenderedOffScreen() )
        {
            GroupObject* oldParent = child->GetParent();

            // Display an error if they're indexing beyond the array (bug http://bugs.coronalabs.com/?18838 )
            const S32 maxIndex = parent->NumChildren();
            if ( index > maxIndex || index < 0 )
            {
                CoronaLuaWarning(L, "group index %d out of range (should be 1 to %d)", (index+1), maxIndex );
            }
            
			SUMMED_TIMING( pi, "Group: Insert (into parent)" );
			
            parent->Insert( index, child, resetTransform );

			SUMMED_TIMING( ai, "Group: Insert (post-parent insert)" );

            // Detect re-insertion of a child back onto the display --- when a
            // child is placed into a new parent that has a canvas and the oldParent
            // was the Orphanage(), then re-acquire a lua ref for the proxy
            if ( oldParent != parent )
            {
                StageObject* canvas = parent->GetStage();
                if ( canvas && oldParent == canvas->GetDisplay().Orphanage() )
                {
                    lua_pushvalue( L, childIndex ); // push table representing child
                    child->GetProxy()->AcquireTableRef( L ); // reacquire a ref for table
                    lua_pop( L, 1 );

                    child->WillMoveOnscreen();
                }
            }
        }
        else
        {
            CoronaLuaWarning( L, "Insertion failed: display objects that are owned by offscreen resources cannot be inserted into groups" );
        }
    }
    else
    {
        luaL_error( L, "ERROR: attempt to insert display object into itself" );
    }

	ENABLE_SUMMED_TIMING( false );

    return 0;
}

int
LuaGroupObjectProxyVTable::insert( lua_State *L )
{
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, GroupObject );
    GroupObject *parent = (GroupObject*)LuaProxy::GetProxyableObject( L, 1 );
    return Insert( L, parent );
}

// Removes child at index from parent and pushes onto the stack. Pushes nil
// if index is invalid.
void
LuaDisplayObjectProxyVTable::PushAndRemove( lua_State *L, GroupObject* parent, S32 index )
{
    if ( index >= 0 )
    {
        Rtt_ASSERT( parent );

		// Offscreen objects (i.e. ones in the orphanage) do have a stage
		StageObject *stage = parent->GetStage();
		if ( stage )
		{
			Rtt_ASSERT( LuaContext::GetRuntime( L )->GetDisplay().HitTestOrphanage() != parent
						&& LuaContext::GetRuntime( L )->GetDisplay().Orphanage() != parent );

			SUMMED_TIMING( par1, "Object: PushAndRemove (release)" );

            DisplayObject* child = parent->Release( index );

            if (child != NULL)
            {
				SUMMED_TIMING( par2, "Object: PushAndRemove (rest)" );

                // If child is the same as global focus, clear global focus
                DisplayObject *globalFocus = stage->GetFocus();
                if ( globalFocus == child )
                {
                    stage->SetFocus( NULL );
                }

                // Always the per-object focus
                stage->SetFocus( child, NULL );
                child->SetFocusId( NULL ); // Defer removal from the focus object array

                child->RemovedFromParent( L, parent );

                // We need to return table, so push it on stack
                Rtt_ASSERT( child->IsReachable() );
                LuaProxy* proxy = child->GetProxy();
                proxy->PushTable( L );

                // Rtt_TRACE( ( "release table ref(%x)\n", lua_topointer( L, -1 ) ) );

                // Anytime we add to the Orphanage, it means the DisplayObject is no
                // longer on the display. Therefore, we should luaL_unref the
                // DisplayObject's table. If it's later re-inserted, then we simply
                // luaL_ref the incoming table.
                Display& display = LuaContext::GetRuntime( L )->GetDisplay();


                // NOTE: Snapshot renamed to HitTest orphanage to clarify usage
                // TODO: Remove snapshot orphanage --- or verify that we still need it?
                // Note on the snapshot orphanage. We use this list to determine
                // which proxy table refs need to be released the table ref once
                // we're done with the snapshot. If the object is reinserted in
                // LuaGroupObjectProxyVTable::Insert(), then it is implicitly
                // removed from the snapshot orphanage --- thus, in that method,
                // nothing special needs to be done, b/c the proxy table wasn't
                // released yet.
                GroupObject& offscreenGroup =
                * ( child->IsUsedByHitTest() ? display.HitTestOrphanage() : display.Orphanage() );
                offscreenGroup.Insert( -1, child, false );

#ifdef Rtt_PHYSICS
                child->RemoveExtensions();
#endif
                
                child->DidMoveOffscreen();
            }
        }
        else
        {
            luaL_error( L, "ERROR: attempt to remove an object that's already been removed from the stage or whose parent/ancestor group has already been removed" );

            // Rtt_ASSERT( LuaContext::GetRuntime( L )->GetDisplay().HitTestOrphanage() == parent
            //             || LuaContext::GetRuntime( L )->GetDisplay().Orphanage() == parent );
        }
    }
    else
    {
        lua_pushnil( L );
    }
}

int
LuaGroupObjectProxyVTable::Remove( lua_State *L, GroupObject *parent )
{
    Rtt_ASSERT( ! lua_isnil( L, 1 ) );

    S32 index = -1;
    if ( lua_istable( L, 2 ) )
    {
        DisplayObject* child = (DisplayObject*)LuaProxy::GetProxyableObject( L, 2 );
        if ( child )
        {
            index = parent->Find( * child );

#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
            if (index < 0)
            {
                CoronaLuaWarning(L, "objectGroup:remove(): invalid object reference (most likely object is not in group)");
            }
#endif
        }
    }
    else
    {
        // Lua indices start at 1
        index = (int) lua_tointeger( L, 2 ) - 1;

#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
        if (index < 0 || index > parent->NumChildren())
        {
            CoronaLuaWarning(L, "objectGroup:remove(): index of %ld out of range (should be 1 to %d)", lua_tointeger( L, 2 ), parent->NumChildren());
        }
#endif
    }

    PushAndRemove( L, parent, index );

    return 1;
}

// group:remove( indexOrChild )
int
LuaGroupObjectProxyVTable::Remove( lua_State *L )
{
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, GroupObject );
    GroupObject *parent = (GroupObject*)LuaProxy::GetProxyableObject( L, 1 );
    return Remove( L, parent );
}

int
LuaGroupObjectProxyVTable::PushChild( lua_State *L, const GroupObject& o )
{
    int result = 0;

    int index = (int) lua_tointeger( L, 2 ) - 1; // Map Lua index to C index
    if ( index >= 0 )
    {
        // GroupObject* o = (GroupObject*)LuaProxy::GetProxyableObject( L, 1 );

        if ( index < o.NumChildren() )
        {
            const DisplayObject& child = o.ChildAt( index );
            LuaProxy* childProxy = child.GetProxy();

            if (childProxy != NULL)
            {
                result = childProxy->PushTable( L );
            }
        }
    }

    return result;
}

int
LuaGroupObjectProxyVTable::PushMethod( lua_State *L, const GroupObject& o, const char *key ) const
{
    int result = 1;

	static const char * keys[] =
	{
		"insert",			// 0
		"remove",			// 1
		"numChildren",		// 2
		"anchorChildren"	// 3
	};
    static const int numKeys = sizeof( keys ) / sizeof( const char * );
	static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 4, 0, 1, __FILE__, __LINE__ );
	StringHash *hash = &sHash;

	int index = hash->Lookup( key );
	switch ( index )
	{
	case 0:
		{
			Lua::PushCachedFunction( L, Self::insert );
			result = 1;
		}
		break;
	case 1:
		{
			Lua::PushCachedFunction( L, Self::Remove );
			result = 1;
		}
		break;
	case 2:
		{
			// GroupObject* o = (GroupObject*)LuaProxy::GetProxyableObject( L, 1 );
			lua_pushinteger( L, o.NumChildren() );
			result = 1;
		}
		break;
	case 3:
		{
			lua_pushboolean( L, o.IsAnchorChildren() );
			result = 1;
		}
		break;
	default:
		{
            result = 0;
        }
        break;
    }

    if ( result == 0 && strcmp( key, "_properties" ) == 0 )
    {
        String snapshotProperties(LuaContext::GetRuntime( L )->Allocator());
        const char **keys = NULL;
        const int numKeys = hash->GetKeys(keys);

        DumpObjectProperties( L, o, keys, numKeys, snapshotProperties );
        Super::ValueForKey( L, o, "_properties", true );

        lua_pushfstring( L, "{ %s, %s }", snapshotProperties.GetString(), lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

int
LuaGroupObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    int result = 0;

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, GroupObject );
    const GroupObject& o = static_cast< const GroupObject& >( object );

    if ( lua_type( L, 2 ) == LUA_TNUMBER )
    {
        result = PushChild( L, o );
    }
    else if ( key )
    {
        result = PushMethod( L, o, key );

        if ( 0 == result )
        {
            result = Super::ValueForKey( L, object, key, overrideRestriction );
        }
    }

    return result;
}

bool
LuaGroupObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    bool result = true;

    Rtt_WARN_SIM_PROXY_TYPE( L, 1, GroupObject );

    if ( 0 == strcmp( key, "anchorChildren" ) )
    {
        GroupObject& o = static_cast< GroupObject& >( object );

        o.SetAnchorChildren( !! lua_toboolean( L, valueIndex ) );
        
#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
        if ( o.IsV1Compatibility() )
        {
            CoronaLuaWarning(L, "group.anchorChildren is only supported in graphics 2.0. Your mileage may vary in graphicsCompatibility 1.0 mode");
        }
#endif
    }
    else
    {
        result = Super::SetValueForKey( L, object, key, valueIndex );
    }

    return result;
}

const LuaProxyVTable&
LuaGroupObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
