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

const LuaEmitterObjectProxyVTable&
LuaEmitterObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaEmitterObjectProxyVTable::start( lua_State *L )
{
    EmitterObject* o = (EmitterObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, EmitterObject );

    if ( o )
    {
#if 0
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
#else
        o->Start();
#endif
    }

    return 0;
}

int
LuaEmitterObjectProxyVTable::stop( lua_State *L )
{
    EmitterObject* o = (EmitterObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, EmitterObject );

    if ( o )
    {
        o->Stop();
    }

    return 0;
}

int
LuaEmitterObjectProxyVTable::pause( lua_State *L )
{
    EmitterObject* o = (EmitterObject*)LuaProxy::GetProxyableObject( L, 1 );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, EmitterObject );

    if ( o )
    {
        o->Pause();
    }

    return 0;
}

// IMPORTANT: This list MUST be kept in sync with the "EmitterObject_keys".
enum
{
    // Read-write properties.
    kEmitterObject_AbsolutePosition,
    kEmitterObject_GravityX,
    kEmitterObject_GravityY,
    kEmitterObject_StartColorR,
    kEmitterObject_StartColorG,
    kEmitterObject_StartColorB,
    kEmitterObject_StartColorA,
    kEmitterObject_StartColorVarianceR,
    kEmitterObject_StartColorVarianceG,
    kEmitterObject_StartColorVarianceB,
    kEmitterObject_StartColorVarianceA,
    kEmitterObject_FinishColorR,
    kEmitterObject_FinishColorG,
    kEmitterObject_FinishColorB,
    kEmitterObject_FinishColorA,
    kEmitterObject_FinishColorVarianceR,
    kEmitterObject_FinishColorVarianceG,
    kEmitterObject_FinishColorVarianceB,
    kEmitterObject_FinishColorVarianceA,
    kEmitterObject_StartParticleSize,
    kEmitterObject_StartParticleSizeVariance,
    kEmitterObject_FinishParticleSize,
    kEmitterObject_FinishParticleSizeVariance,
    kEmitterObject_MaxRadius,
    kEmitterObject_MaxRadiusVariance,
    kEmitterObject_MinRadius,
    kEmitterObject_MinRadiusVariance,
    kEmitterObject_RotateDegreesPerSecond,
    kEmitterObject_RotateDegreesPerSecondVariance,
    kEmitterObject_RotationStart,
    kEmitterObject_RotationStartVariance,
    kEmitterObject_RotationEnd,
    kEmitterObject_RotationEndVariance,
    kEmitterObject_Speed,
    kEmitterObject_SpeedVariance,
    kEmitterObject_EmissionRateInParticlesPerSeconds,
    kEmitterObject_RadialAcceleration,
    kEmitterObject_RadialAccelerationVariance,
    kEmitterObject_TangentialAcceleration,
    kEmitterObject_TangentialAccelerationVariance,
    kEmitterObject_SourcePositionVarianceX,
    kEmitterObject_SourcePositionVarianceY,
    kEmitterObject_RotationInDegrees,
    kEmitterObject_RotationInDegreesVariance,
    kEmitterObject_ParticleLifespanInSeconds,
    kEmitterObject_ParticleLifespanInSecondsVariance,
    kEmitterObject_Duration,

    // Read Only Property
    kEmitterObject_MaxParticles,

    // Methods.
    kEmitterObject_Start,
    kEmitterObject_Stop,
    kEmitterObject_Pause,

    // Read-only properties.
    kEmitterObject_State,
};

static const char * EmitterObject_keys[] =
{
    // Read-write properties.
    "absolutePosition",
    "gravityx",
    "gravityy",
    "startColorRed",
    "startColorGreen",
    "startColorBlue",
    "startColorAlpha",
    "startColorVarianceRed",
    "startColorVarianceGreen",
    "startColorVarianceBlue",
    "startColorVarianceAlpha",
    "finishColorRed",
    "finishColorGreen",
    "finishColorBlue",
    "finishColorAlpha",
    "finishColorVarianceRed",
    "finishColorVarianceGreen",
    "finishColorVarianceBlue",
    "finishColorVarianceAlpha",
    "startParticleSize",
    "startParticleSizeVariance",
    "finishParticleSize",
    "finishParticleSizeVariance",
    "maxRadius",
    "maxRadiusVariance",
    "minRadius",
    "minRadiusVariance",
    "rotatePerSecond",
    "rotatePerSecondVariance",
    "rotationStart",
    "rotationStartVariance",
    "rotationEnd",
    "rotationEndVariance",
    "speed",
    "speedVariance",
    "emissionRateInParticlesPerSeconds",
    "radialAcceleration",
    "radialAccelVariance",
    "tangentialAcceleration",
    "tangentialAccelVariance",
    "sourcePositionVariancex",
    "sourcePositionVariancey",
    "angle",
    "angleVariance",
    "particleLifespan",
    "particleLifespanVariance",
    "duration",

    // read only properties
    "maxParticles",

    // Methods.
    "start",
    "stop",
    "pause",

    // Read-only properties.
    "state",
};

static StringHash*
GetEmitterObjectHash( lua_State *L )
{
    static StringHash sHash( *LuaContext::GetAllocator( L ), EmitterObject_keys, sizeof( EmitterObject_keys ) / sizeof(const char *), 52, 12, 14, __FILE__, __LINE__ );
    return &sHash;
}

int
LuaEmitterObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }
    
    int result = 1;

    StringHash *hash = GetEmitterObjectHash( L );
    int index = hash->Lookup( key );

    // EmitterObject* o = (EmitterObject*)LuaProxy::GetProxyableObject( L, 1 );
    const EmitterObject& o = static_cast< const EmitterObject& >( object );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, EmitterObject );

    switch ( index )
    {
    case kEmitterObject_AbsolutePosition:
        {
            GroupObject *p = o.GetAbsolutePosition();
            if(p == NULL)
            {
                lua_pushboolean( L, false );
            }
            else if(p == EMITTER_ABSOLUTE_PARENT)
            {
                lua_pushboolean( L, false );
            }
            else
            {
                p->GetProxy()->PushTable( L );
            }
        }
        break;
    case kEmitterObject_GravityX:
        {
            lua_pushnumber( L, o.GetGravity().x );
        }
        break;
    case kEmitterObject_GravityY:
        {
            lua_pushnumber( L, o.GetGravity().y );
        }
        break;
    case kEmitterObject_StartColorR:
        {
            lua_pushnumber( L, o.GetStartColor().r );
        }
        break;
    case kEmitterObject_StartColorG:
        {
            lua_pushnumber( L, o.GetStartColor().g );
        }
        break;
    case kEmitterObject_StartColorB:
        {
            lua_pushnumber( L, o.GetStartColor().b );
        }
        break;
    case kEmitterObject_StartColorA:
        {
            lua_pushnumber( L, o.GetStartColor().a );
        }
        break;
    case kEmitterObject_StartColorVarianceR:
        {
            lua_pushnumber( L, o.GetStartColorVariance().r );
        }
        break;
    case kEmitterObject_StartColorVarianceG:
        {
            lua_pushnumber( L, o.GetStartColorVariance().g );
        }
        break;
    case kEmitterObject_StartColorVarianceB:
        {
            lua_pushnumber( L, o.GetStartColorVariance().b );
        }
        break;
    case kEmitterObject_StartColorVarianceA:
        {
            lua_pushnumber( L, o.GetStartColorVariance().a );
        }
        break;
    case kEmitterObject_FinishColorR:
        {
            lua_pushnumber( L, o.GetFinishColor().r );
        }
        break;
    case kEmitterObject_FinishColorG:
        {
            lua_pushnumber( L, o.GetFinishColor().g );
        }
        break;
    case kEmitterObject_FinishColorB:
        {
            lua_pushnumber( L, o.GetFinishColor().b );
        }
        break;
    case kEmitterObject_FinishColorA:
        {
            lua_pushnumber( L, o.GetFinishColor().a );
        }
        break;
    case kEmitterObject_FinishColorVarianceR:
        {
            lua_pushnumber( L, o.GetFinishColorVariance().r );
        }
        break;
    case kEmitterObject_FinishColorVarianceG:
        {
            lua_pushnumber( L, o.GetFinishColorVariance().g );
        }
        break;
    case kEmitterObject_FinishColorVarianceB:
        {
            lua_pushnumber( L, o.GetFinishColorVariance().b );
        }
        break;
    case kEmitterObject_FinishColorVarianceA:
        {
            lua_pushnumber( L, o.GetFinishColorVariance().a );
        }
        break;
    case kEmitterObject_StartParticleSize:
        {
            lua_pushnumber( L, o.GetStartParticleSize() );
        }
        break;
    case kEmitterObject_StartParticleSizeVariance:
        {
            lua_pushnumber( L, o.GetStartParticleSizeVariance() );
        }
        break;
    case kEmitterObject_FinishParticleSize:
        {
            lua_pushnumber( L, o.GetFinishParticleSize() );
        }
        break;
    case kEmitterObject_FinishParticleSizeVariance:
        {
            lua_pushnumber( L, o.GetFinishParticleSizeVariance() );
        }
        break;
    case kEmitterObject_MaxRadius:
        {
            lua_pushnumber( L, o.GetMaxRadius() );
        }
        break;
    case kEmitterObject_MaxRadiusVariance:
        {
            lua_pushnumber( L, o.GetMaxRadiusVariance() );
        }
        break;
    case kEmitterObject_MinRadius:
        {
            lua_pushnumber( L, o.GetMinRadius() );
        }
        break;
    case kEmitterObject_MinRadiusVariance:
        {
            lua_pushnumber( L, o.GetMinRadiusVariance() );
        }
        break;
    case kEmitterObject_RotateDegreesPerSecond:
        {
            lua_pushnumber( L, o.GetRotateDegreesPerSecond() );
        }
        break;
    case kEmitterObject_RotateDegreesPerSecondVariance:
        {
            lua_pushnumber( L, o.GetRotateDegreesPerSecondVariance() );
        }
        break;
    case kEmitterObject_RotationStart:
        {
            lua_pushnumber( L, o.GetRotationStart() );
        }
        break;
    case kEmitterObject_RotationStartVariance:
        {
            lua_pushnumber( L, o.GetRotationStartVariance() );
        }
        break;
    case kEmitterObject_RotationEnd:
        {
            lua_pushnumber( L, o.GetRotationEnd() );
        }
        break;
    case kEmitterObject_RotationEndVariance:
        {
            lua_pushnumber( L, o.GetRotationEndVariance() );
        }
        break;
    case kEmitterObject_Speed:
        {
            lua_pushnumber( L, o.GetSpeed() );
        }
        break;
    case kEmitterObject_SpeedVariance:
        {
            lua_pushnumber( L, o.GetSpeedVariance() );
        }
        break;
    case kEmitterObject_EmissionRateInParticlesPerSeconds:
        {
            lua_pushnumber( L, o.GetEmissionRateInParticlesPerSeconds() );
        }
        break;
    case kEmitterObject_RadialAcceleration:
        {
            lua_pushnumber( L, o.GetRadialAcceleration() );
        }
        break;
    case kEmitterObject_RadialAccelerationVariance:
        {
            lua_pushnumber( L, o.GetRadialAccelerationVariance() );
        }
        break;
    case kEmitterObject_TangentialAcceleration:
        {
            lua_pushnumber( L, o.GetTangentialAcceleration() );
        }
        break;
    case kEmitterObject_TangentialAccelerationVariance:
        {
            lua_pushnumber( L, o.GetTangentialAccelerationVariance() );
        }
        break;
    case kEmitterObject_SourcePositionVarianceX:
        {
            lua_pushnumber( L, o.GetSourcePositionVariance().x );
        }
        break;
    case kEmitterObject_SourcePositionVarianceY:
        {
            lua_pushnumber( L, o.GetSourcePositionVariance().y );
        }
        break;
    case kEmitterObject_RotationInDegrees:
        {
            lua_pushnumber( L, o.GetRotationInDegrees() );
        }
        break;
    case kEmitterObject_RotationInDegreesVariance:
        {
            lua_pushnumber( L, o.GetRotationInDegreesVariance() );
        }
        break;
    case kEmitterObject_ParticleLifespanInSeconds:
        {
            lua_pushnumber( L, o.GetParticleLifespanInSeconds() );
        }
        break;
    case kEmitterObject_ParticleLifespanInSecondsVariance:
        {
            lua_pushnumber( L, o.GetParticleLifespanInSecondsVariance() );
        }
        break;
    case kEmitterObject_Duration:
        {
            lua_pushnumber( L, o.GetDuration() );
        }
        break;
    case kEmitterObject_MaxParticles:
        {
            lua_pushinteger( L, o.GetMaxParticles() );
        }
        break;
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    case kEmitterObject_Start:
        {
            Lua::PushCachedFunction( L, Self::start );
        }
        break;
    case kEmitterObject_Stop:
        {
            Lua::PushCachedFunction( L, Self::stop );
        }
        break;
    case kEmitterObject_Pause:
        {
            Lua::PushCachedFunction( L, Self::pause );
        }
        break;

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    case kEmitterObject_State:
        {
            lua_pushstring( L, EmitterObject::GetStringForState( o.GetState() ) );
        }
        break;

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    default:
        {
            result = Super::ValueForKey( L, object, key, overrideRestriction );
        }
        break;
    }

    if ( result == 1 && strcmp( key, "_properties" ) == 0 )
    {
        String emitterProperties(LuaContext::GetRuntime( L )->Allocator());
        const char **keys = NULL;
        const int numKeys = hash->GetKeys(keys);

        DumpObjectProperties( L, object, keys, numKeys, emitterProperties );

        lua_pushfstring( L, "{ %s, %s }", emitterProperties.GetString(), lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

bool
LuaEmitterObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    if ( ! key ) { return false; }

    // EmitterObject* o = (EmitterObject*)LuaProxy::GetProxyableObject( L, 1 );
    EmitterObject& o = static_cast< EmitterObject& >( object );
    //const int numKeys = sizeof( EmitterObject_keys ) / sizeof( const char * );
    Rtt_WARN_SIM_PROXY_TYPE( L, 1, EmitterObject );

    bool result = true;

    StringHash *hash = GetEmitterObjectHash( L );
    int index = hash->Lookup( key );

    switch ( index )
    {
    case kEmitterObject_AbsolutePosition:
        {
            GroupObject *parentGroup = NULL;
            if(lua_istable(L, valueIndex))
            {
                DisplayObject* parent = (DisplayObject*)LuaProxy::GetProxyableObject( L, valueIndex );
                if(parent)
                {
                    parentGroup = parent->AsGroupObject();
                }
            }

            if(parentGroup == NULL)
            {
                o.SetAbsolutePosition( lua_toboolean( L, valueIndex )?EMITTER_ABSOLUTE_PARENT:NULL );
            }
            else
            {
                o.SetAbsolutePosition(parentGroup);
                if(!o.ValidateEmitterParent())
                {
                    CoronaLuaWarning(L, "if '%s' of Emitter Object is set to group object, it has to be one of it's parents", key);
                }
            }
        }
        break;
    case kEmitterObject_GravityX:
        {
            o.GetGravity().x = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_GravityY:
        {
            o.GetGravity().y = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartColorR:
        {
            o.GetStartColor().r = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartColorG:
        {
            o.GetStartColor().g = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartColorB:
        {
            o.GetStartColor().b = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartColorA:
        {
            o.GetStartColor().a = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartColorVarianceR:
        {
            o.GetStartColorVariance().r = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartColorVarianceG:
        {
            o.GetStartColorVariance().g = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartColorVarianceB:
        {
            o.GetStartColorVariance().b = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartColorVarianceA:
        {
            o.GetStartColorVariance().a = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_FinishColorR:
        {
            o.GetFinishColor().r = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_FinishColorG:
        {
            o.GetFinishColor().g = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_FinishColorB:
        {
            o.GetFinishColor().b = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_FinishColorA:
        {
            o.GetFinishColor().a = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_FinishColorVarianceR:
        {
            o.GetFinishColorVariance().r = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_FinishColorVarianceG:
        {
            o.GetFinishColorVariance().g = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_FinishColorVarianceB:
        {
            o.GetFinishColorVariance().b = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_FinishColorVarianceA:
        {
            o.GetFinishColorVariance().a = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_StartParticleSize:
        {
            o.SetStartParticleSize( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_StartParticleSizeVariance:
        {
            o.SetStartParticleSizeVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_FinishParticleSize:
        {
            o.SetFinishParticleSize( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_FinishParticleSizeVariance:
        {
            o.SetFinishParticleSizeVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_MaxRadius:
        {
            o.SetMaxRadius( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_MaxRadiusVariance:
        {
            o.SetMaxRadiusVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_MinRadius:
        {
            o.SetMinRadius( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_MinRadiusVariance:
        {
            o.SetMinRadiusVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RotateDegreesPerSecond:
        {
            o.SetRotateDegreesPerSecond( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RotateDegreesPerSecondVariance:
        {
            o.SetRotateDegreesPerSecondVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RotationStart:
        {
            o.SetRotationStart( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RotationStartVariance:
        {
            o.SetRotationStartVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RotationEnd:
        {
            o.SetRotationEnd( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RotationEndVariance:
        {
            o.SetRotationEndVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_Speed:
        {
            o.SetSpeed( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_SpeedVariance:
        {
            o.SetSpeedVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_EmissionRateInParticlesPerSeconds:
        {
            o.SetEmissionRateInParticlesPerSeconds( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RadialAcceleration:
        {
            o.SetRadialAcceleration( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RadialAccelerationVariance:
        {
            o.SetRadialAccelerationVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_TangentialAcceleration:
        {
            o.SetTangentialAcceleration( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_TangentialAccelerationVariance:
        {
            o.SetTangentialAccelerationVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_SourcePositionVarianceX:
        {
            o.GetSourcePositionVariance().x = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_SourcePositionVarianceY:
        {
            o.GetSourcePositionVariance().y = luaL_toreal( L, valueIndex );
        }
        break;
    case kEmitterObject_RotationInDegrees:
        {
            o.SetRotationInDegrees( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_RotationInDegreesVariance:
        {
            o.SetRotationInDegreesVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_ParticleLifespanInSeconds:
        {
            o.SetParticleLifespanInSeconds( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_ParticleLifespanInSecondsVariance:
        {
            o.SetParticleLifespanInSecondsVariance( luaL_toreal( L, valueIndex ) );
        }
        break;
    case kEmitterObject_Duration:
        {
            o.SetDuration( luaL_toreal( L, valueIndex ) );
        }
        break;

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    case kEmitterObject_MaxParticles:
    case kEmitterObject_Start:
    case kEmitterObject_Stop:
    case kEmitterObject_Pause:
    case kEmitterObject_State:
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
LuaEmitterObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
