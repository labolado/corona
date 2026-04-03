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
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_ParticleSystemObject.h"
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

const LuaParticleSystemObjectProxyVTable&
LuaParticleSystemObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaParticleSystemObjectProxyVTable::CreateGroup( lua_State *L )
{
    ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );

    if ( o )
    {
        o->CreateGroup( L );
    }

    return 0;
}

int
LuaParticleSystemObjectProxyVTable::CreateParticle( lua_State *L )
{
    ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );

    if ( o )
    {
        o->CreateParticle( L );
    }

    return 0;
}

int
LuaParticleSystemObjectProxyVTable::DestroyParticlesInShape( lua_State *L )
{
    ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );

    if ( o )
    {
        return o->DestroyParticlesInShape( L );
    }

    return 0;
}

int
LuaParticleSystemObjectProxyVTable::RayCast( lua_State *L )
{
    ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );

    if ( o )
    {
        return o->RayCast( L );
    }

    return 0;
}

int
LuaParticleSystemObjectProxyVTable::QueryRegion( lua_State *L )
{
    ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );

    if ( o )
    {
        return o->QueryRegion( L );
    }

    return 0;
}

int
LuaParticleSystemObjectProxyVTable::ApplyForce( lua_State *L )
{
    ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );

    if ( o )
    {
        o->ApplyForce( L );
    }

    return 0;
}

int
LuaParticleSystemObjectProxyVTable::ApplyLinearImpulse( lua_State *L )
{
    ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );

    if ( o )
    {
        o->ApplyLinearImpulse( L );
    }

    return 0;
}

// IMPORTANT: This list MUST be kept in sync with the "ParticleSystemObject_keys".
enum
{
    // Read-write properties.
    kParticleSystemObject_particleDensity,
    kParticleSystemObject_particleRadius,
    kParticleSystemObject_particleDamping,
    kParticleSystemObject_particleStrictContactCheck,
    kParticleSystemObject_particleMaxCount,
    kParticleSystemObject_particleGravityScale,
    kParticleSystemObject_particleDestructionByAge,
    kParticleSystemObject_particlePaused,
    kParticleSystemObject_imageRadius,

    // Read-only property.
    kParticleSystemObject_particleMass,
    kParticleSystemObject_particleCount,

    // Methods.
    kParticleSystemObject_ApplyForce,
    kParticleSystemObject_ApplyLinearImpulse,
    kParticleSystemObject_CreateGroup,
    kParticleSystemObject_CreateParticle,
    kParticleSystemObject_DestroyParticlesInShape,
    kParticleSystemObject_QueryRegion,
    kParticleSystemObject_RayCast,
};

static const char * ParticleSystemObject_keys[] =
{
    // Read-write properties.
    "particleDensity",
    "particleRadius",
    "particleDamping",
    "particleStrictContactCheck",
    "particleMaxCount",
    "particleGravityScale",
    "particleDestructionByAge",
    "particlePaused",
    "imageRadius",

    // Read-only property.
    "particleMass",
    "particleCount",

    // Methods.
    "applyForce",
    "applyLinearImpulse",
    "createGroup",
    "createParticle",
    "destroyParticles",
    "queryRegion",
    "rayCast",
};

static StringHash*
GetParticleSystemObjectHash( lua_State *L )
{
    static StringHash sHash( *LuaContext::GetAllocator( L ), ParticleSystemObject_keys, sizeof( ParticleSystemObject_keys ) / sizeof(const char *), 19, 28, 2, __FILE__, __LINE__ );
    return &sHash;
}

int
LuaParticleSystemObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }
    
    int result = 1;
    StringHash *hash = GetParticleSystemObjectHash( L );
    int index = hash->Lookup( key );

    // ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    const ParticleSystemObject& o = static_cast< const ParticleSystemObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );
    const b2ParticleSystem *b2ps = o.GetB2ParticleSystem();

    switch ( index )
    {
    case kParticleSystemObject_particleDensity:
        {
            lua_pushnumber( L, b2ps->GetDensity() );
        }
        break;

    case kParticleSystemObject_particleRadius:
        {
            PhysicsWorld &physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();

            float world_scale_in_pixels_per_meter = physics.GetPixelsPerMeter();

            lua_pushnumber( L, ( b2ps->GetRadius() * world_scale_in_pixels_per_meter ) );
        }
        break;

    case kParticleSystemObject_particleDamping:
        {
            lua_pushnumber( L, b2ps->GetDamping() );
        }
        break;

    case kParticleSystemObject_particleStrictContactCheck:
        {
            lua_pushboolean( L, b2ps->GetStrictContactCheck() );
        }
        break;

    case kParticleSystemObject_particleMaxCount:
        {
            lua_pushnumber( L, b2ps->GetMaxParticleCount() );
        }
        break;

    case kParticleSystemObject_particleGravityScale:
        {
            lua_pushnumber( L, b2ps->GetGravityScale() );
        }
        break;

    case kParticleSystemObject_particleDestructionByAge:
        {
            lua_pushboolean( L, b2ps->GetDestructionByAge() );
        }
        break;

    case kParticleSystemObject_particlePaused:
        {
            lua_pushboolean( L, b2ps->GetPaused() );
        }
        break;

    case kParticleSystemObject_imageRadius:
        {
            lua_pushnumber( L, o.GetParticleRenderRadiusInContentUnits() );
        }
        break;

    case kParticleSystemObject_particleMass:
        {
            lua_pushnumber( L, b2ps->GetParticleMass() );
        }
        break;

    case kParticleSystemObject_particleCount:
        {
            lua_pushnumber( L, b2ps->GetParticleCount() );
        }
        break;

    case kParticleSystemObject_ApplyForce:
        {
            Lua::PushCachedFunction( L, Self::ApplyForce );
        }
        break;

    case kParticleSystemObject_ApplyLinearImpulse:
        {
            Lua::PushCachedFunction( L, Self::ApplyLinearImpulse );
        }
        break;

    case kParticleSystemObject_CreateGroup:
        {
            Lua::PushCachedFunction( L, Self::CreateGroup );
        }
        break;

    case kParticleSystemObject_CreateParticle:
        {
            Lua::PushCachedFunction( L, Self::CreateParticle );
        }
        break;

    case kParticleSystemObject_DestroyParticlesInShape:
        {
            Lua::PushCachedFunction( L, Self::DestroyParticlesInShape );
        }
        break;

    case kParticleSystemObject_QueryRegion:
        {
            Lua::PushCachedFunction( L, Self::QueryRegion );
        }
        break;

    case kParticleSystemObject_RayCast:
        {
            Lua::PushCachedFunction( L, Self::RayCast );
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
        String psProperties(LuaContext::GetRuntime( L )->Allocator());
        const char **keys = NULL;
        const int numKeys = hash->GetKeys(keys);

        DumpObjectProperties( L, object, keys, numKeys, psProperties );

        lua_pushfstring( L, "{ %s, %s }", psProperties.GetString(), lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

bool
LuaParticleSystemObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    // ParticleSystemObject* o = (ParticleSystemObject*)LuaProxy::GetProxyableObject( L, 1 );
    ParticleSystemObject& o = static_cast< ParticleSystemObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, ParticleSystemObject );
    b2ParticleSystem *b2ps = o.GetB2ParticleSystem();

    bool result = true;

    StringHash *hash = GetParticleSystemObjectHash( L );
    int index = hash->Lookup( key );

    switch ( index )
    {
    case kParticleSystemObject_particleDensity:
        {
            b2ps->SetDensity( luaL_toreal( L, valueIndex ) );
        }
        break;

    case kParticleSystemObject_particleRadius:
        {
            PhysicsWorld &physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();

            float world_scale_in_meters_per_pixel = physics.GetMetersPerPixel();

            b2ps->SetRadius( luaL_toreal( L, valueIndex ) * world_scale_in_meters_per_pixel );
        }
        break;

    case kParticleSystemObject_particleDamping:
        {
            b2ps->SetDamping( luaL_toreal( L, valueIndex ) );
        }
        break;

    case kParticleSystemObject_particleStrictContactCheck:
        {
            b2ps->SetStrictContactCheck( !! lua_toboolean( L, valueIndex ) );
        }
        break;

    case kParticleSystemObject_particleMaxCount:
        {
            b2ps->SetMaxParticleCount( luaL_toreal( L, valueIndex ) );
        }
        break;

    case kParticleSystemObject_particleGravityScale:
        {
            b2ps->SetGravityScale( luaL_toreal( L, valueIndex ) );
        }
        break;

    case kParticleSystemObject_particleDestructionByAge:
        {
            b2ps->SetDestructionByAge( !! lua_toboolean( L, valueIndex ) );
        }
        break;

    case kParticleSystemObject_particlePaused:
        {
            b2ps->SetPaused( !! lua_toboolean( L, valueIndex ) );
        }
        break;

    case kParticleSystemObject_imageRadius:
        {
            o.SetParticleRenderRadiusInContentUnits( luaL_toreal( L, valueIndex ) );
        }
        break;

    case kParticleSystemObject_particleMass:
    case kParticleSystemObject_particleCount:
    case kParticleSystemObject_ApplyForce:
    case kParticleSystemObject_ApplyLinearImpulse:
    case kParticleSystemObject_CreateGroup:
    case kParticleSystemObject_CreateParticle:
    case kParticleSystemObject_DestroyParticlesInShape:
    case kParticleSystemObject_QueryRegion:
    case kParticleSystemObject_RayCast:
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
LuaParticleSystemObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
