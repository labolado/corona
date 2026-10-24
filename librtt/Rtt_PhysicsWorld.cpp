//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Rtt_PhysicsWorld.h"
#include "Rtt_ParticleSystemObject.h"
#include "Rtt_FixedStepScheduler.h"
#include "Rtt_LuaContext.h"
#include <map>
#include <mutex>
#include <string>
#include <cmath>

#include "b2GLESDebugDraw.h"

#include "Display/Rtt_Display.h"
#include "Display/Rtt_DisplayObject.h"
#include "Rtt_LuaAux.h"
#include "Rtt_LuaLibPhysics.h"
#include "Rtt_Runtime.h"
#include "Rtt_PhysicsContactListener.h"

#if defined( _WIN32 )
	#include <Windows.h>
	#include <cstdlib>
#elif defined( __APPLE__ )
	#include <sys/sysctl.h>
	#include <unistd.h>
#elif defined( __ANDROID__ ) || defined( __linux__ ) || defined( __EMSCRIPTEN__ )
	#include <unistd.h>
	// #include <cstdio>
#endif

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

namespace
{
// Side storage preserves PhysicsWorld's existing object layout and vtable.
// Physics/display APIs and this scheduler run on the Runtime's owning thread.
struct FixedStepState
{
    bool enabled = false;
    bool executing = false;
    bool faulted = false;
    bool coupledFault = false;
    float dt = 1.0f / 60.0f;
    int subSteps = 8;
    int maxSteps = 8;
    double speed = 1.0;
    double budget = 0.0;
    unsigned long long index = 0;
    unsigned long long generation = 0;
    unsigned long long interruption = 0;
    Runtime *runtime = NULL;
    int listener = LUA_NOREF;
    std::string error;
    FixedStepScheduler::Client client = {};
};
std::map<PhysicsWorld *, FixedStepState> sFixedSteps;
std::mutex sFixedStepsMutex;

FixedStepState *FindFixedStep( PhysicsWorld& physics )
{
    std::lock_guard<std::mutex> lock( sFixedStepsMutex );
    auto it = sFixedSteps.find( &physics );
    return it == sFixedSteps.end() ? NULL : &it->second;
}

FixedStepState& GetFixedStep( PhysicsWorld& physics )
{
    std::lock_guard<std::mutex> lock( sFixedStepsMutex );
    return sFixedSteps[&physics];
}

PhysicsWorld& FixedPhysics( lua_State *L )
{
    return LuaContext::GetRuntime( L )->GetPhysicsWorld();
}

double FixedOption( lua_State *L, const char *key, double fallback )
{
    lua_getfield( L, 1, key );
    double value = fallback;
    if ( ! lua_isnil( L, -1 ) ) { value = luaL_checknumber( L, -1 ); }
    lua_pop( L, 1 );
    return value;
}

void FixedNumber( lua_State *L, const char *key, double value )
{
    lua_pushnumber( L, value );
    lua_setfield( L, -2, key );
}

bool FixedCallback( PhysicsWorld& physics, FixedStepState& state, const char *phase,
                    unsigned long long index )
{
    if ( state.listener == LUA_NOREF ) { return true; }
    lua_State *L = state.runtime->VMContext().L();
    RuntimeGuard guard( *state.runtime );
    const int top = lua_gettop( L );
    const unsigned long long generation = state.generation;
    lua_rawgeti( L, LUA_REGISTRYINDEX, state.listener );
    lua_createtable( L, 0, 5 );
    lua_pushliteral( L, "physicsStep" ); lua_setfield( L, -2, "name" );
    lua_pushstring( L, phase ); lua_setfield( L, -2, "phase" );
    FixedNumber( L, "stepIndex", (double)index );
    FixedNumber( L, "dt", state.dt );
    // Before: time at the start of the proposed step. After: completed time.
    FixedNumber( L, "simulationTime", (double)(index - (phase[0] == 'b' ? 1 : 0)) * state.dt );
    // Catch locally so an error cannot unwind through Box2D or retry partial commands.
    const int status = lua_pcall( L, 1, 0, 0 );
    if ( status != 0 )
    {
        const char *message = lua_tostring( L, -1 );
        Rtt_Log( "physics step listener error (%s): %s\n", phase, message ? message : "non-string error" );
        // A callback may have stopped and created a different world before failing.
        // Never mark that new timeline as having executed the old step.
        state.error = message ? message : "non-string error";
        state.faulted = true;
        physics.PauseWorld();
    }
    lua_settop( L, top );
    return status == 0 && state.generation == generation;
}
}

const char *FixedStepScheduler::Attach(PhysicsWorld& physics, const Client& client, float dt)
{
    if (!client.owner || !client.prepare || !client.invalidate ||
        (!client.wholeStep && !client.advance) ||
        (client.wholeStep && (!client.retain || !client.release)))
    { return "invalid coupled client callbacks"; }
    FixedStepState *state = FindFixedStep(physics);
    if (!state || !state->enabled || state->dt != dt || dt != 1.0f/480.0f || state->subSteps != 1)
    { return "coupled soft world requires fixed timeStep=1/480 and subSteps=1"; }
    if (state->executing || state->faulted || !physics.IsWorldValid())
    { return "cannot attach soft world during a step, after a fault, or without physics.start"; }
    if (state->client.owner) { return "only one coupled soft world is supported"; }
    const char *error = client.prepare(client.owner, physics);
    if (!error) { state->client = client; }
    return error;
}

void FixedStepScheduler::Detach(PhysicsWorld& physics, void *owner)
{
    FixedStepState *state = FindFixedStep(physics);
    if (state && state->client.owner == owner) { state->client = {}; }
}

void FixedStepScheduler::Invalidate(PhysicsWorld& physics)
{
    FixedStepState *state = FindFixedStep(physics);
    if (state && state->client.owner)
    {
        Client client = state->client;
        state->client = {};
        client.invalidate(client.owner);
    }
}

int FixedStepScheduler::Configure( lua_State *L )
{
    PhysicsWorld& physics = FixedPhysics( L );
    FixedStepState *previous = FindFixedStep( physics );
    if ( previous && previous->coupledFault && physics.IsWorldValid() )
    { return luaL_error( L, "stop the faulted coupled world before changing fixed step mode" ); }
    if ( previous && previous->client.owner )
    { return luaL_error( L, "remove the coupled soft world before changing fixed step mode" ); }
    if ( previous && previous->executing )
    { return luaL_error( L, "setFixedStepMode cannot change mode during a physics step" ); }
    if ( physics.IsWorldValid() && physics.GetWorld()->IsLocked() )
    { return luaL_error( L, "setFixedStepMode cannot run while the world is locked" ); }
    if ( lua_isnil( L, 1 ) || (lua_isboolean( L, 1 ) && !lua_toboolean( L, 1 )) )
    {
        if ( previous ) { previous->enabled = false; Reset( physics ); }
        // Legacy wall-clock mode must not catch up time spent in the opt-in mode.
        physics.fTimePrevious = -1.f;
        physics.fTimeRemainder = 0.f;
        return 0;
    }
    luaL_checktype( L, 1, LUA_TTABLE );
    if ( previous && previous->faulted && physics.IsWorldValid() )
    { return luaL_error( L, "stop the faulted world before reconfiguring fixed step mode" ); }
    const double dt = FixedOption( L, "timeStep", 1.0 / 60.0 );
    const double subSteps = FixedOption( L, "subSteps", physics.GetSubSteps() );
    const double speed = FixedOption( L, "speed", 1 );
    const double maxSteps = FixedOption( L, "maxStepsPerFrame", 8 );
    if ( !std::isfinite(dt) || dt <= 0 || dt > 1 || (float)dt <= 0 ||
         !std::isfinite(subSteps) || subSteps < 1 || subSteps > 128 || floor(subSteps) != subSteps ||
         !std::isfinite(speed) || speed <= 0 || speed > 64 ||
         !std::isfinite(maxSteps) || maxSteps < 1 || maxSteps > 1024 || floor(maxSteps) != maxSteps )
    { return luaL_error( L, "invalid fixed step options: timeStep (0,1], subSteps [1,128], speed (0,64], maxStepsPerFrame [1,1024]" ); }
    FixedStepState& state = GetFixedStep( physics );
    state.runtime = LuaContext::GetRuntime( L );
    state.dt = (float)dt;
    state.subSteps = (int)subSteps;
    state.speed = speed;
    state.maxSteps = (int)maxSteps;
    state.enabled = true;
    Reset( physics );
    return 0;
}

int FixedStepScheduler::SetSpeed( lua_State *L )
{
    const double speed = luaL_checknumber( L, 1 );
    FixedStepState *state = FindFixedStep( FixedPhysics( L ) );
    if ( !state || !state->enabled ) { return luaL_error( L, "setSimulationSpeed requires fixed step mode" ); }
    if ( !std::isfinite(speed) || speed <= 0 || speed > 64 )
    { return luaL_error( L, "simulation speed must be finite and in (0,64]" ); }
    state->speed = speed;
    return 0;
}

int FixedStepScheduler::SetListener( lua_State *L )
{
    if ( !lua_isnil( L, 1 ) ) { luaL_checktype( L, 1, LUA_TFUNCTION ); }
    PhysicsWorld& physics = FixedPhysics( L );
    FixedStepState& state = GetFixedStep( physics );
    state.runtime = LuaContext::GetRuntime( L );
    if ( state.listener != LUA_NOREF ) { luaL_unref( L, LUA_REGISTRYINDEX, state.listener ); }
    state.listener = LUA_NOREF;
    if ( !lua_isnil( L, 1 ) )
    {
        lua_pushvalue( L, 1 );
        state.listener = luaL_ref( L, LUA_REGISTRYINDEX );
    }
    return 0;
}

int FixedStepScheduler::GetState( lua_State *L )
{
    PhysicsWorld& physics = FixedPhysics( L );
    FixedStepState *state = FindFixedStep( physics );
    lua_createtable( L, 0, 11 );
    lua_pushboolean( L, state && state->enabled ); lua_setfield( L, -2, "enabled" );
    lua_pushboolean( L, state && state->faulted ); lua_setfield( L, -2, "faulted" );
    lua_pushboolean( L, physics.IsProperty( PhysicsWorld::kIsWorldRunning ) ); lua_setfield( L, -2, "running" );
    FixedNumber( L, "stepIndex", state ? (double)state->index : 0 );
    FixedNumber( L, "simulationTime", state ? (double)state->index * state->dt : 0 );
    FixedNumber( L, "pendingSteps", state ? state->budget : 0 );
    if ( state )
    {
        FixedNumber( L, "timeStep", state->dt );
        FixedNumber( L, "subSteps", state->subSteps );
        FixedNumber( L, "speed", state->speed );
        FixedNumber( L, "maxStepsPerFrame", state->maxSteps );
        if ( state->faulted ) { lua_pushstring( L, state->error.c_str() ); lua_setfield( L, -2, "error" ); }
    }
    return 1;
}

void FixedStepScheduler::Reset( PhysicsWorld& physics )
{
    FixedStepState *state = FindFixedStep( physics );
    if ( !state ) { return; }
    ++state->generation;
    ++state->interruption;
    state->index = 0;
    state->budget = 0;
    state->faulted = false;
    state->coupledFault = false;
    state->error.clear();
    // Do not clear executing: stop/start inside a callback must not reenter Step.
}

void FixedStepScheduler::Interrupt( PhysicsWorld& physics )
{
    FixedStepState *state = FindFixedStep( physics );
    if ( state ) { ++state->interruption; }
}

void FixedStepScheduler::Forget( PhysicsWorld& physics )
{
    // Registry references belong to the Runtime VM and are released at VM teardown.
    // Its lifetime may already have ended when PhysicsWorld is destroyed.
    std::lock_guard<std::mutex> lock( sFixedStepsMutex );
    sFixedSteps.erase( &physics );
}

void FixedStepScheduler::SyncDisplay( PhysicsWorld& physics )
{
    const b2BodyEvents events = b2World_GetBodyEvents( physics.GetWorldId() );
    std::vector<b2BodyMoveEvent> moves;
    if ( events.moveCount ) { moves.assign( events.moveEvents, events.moveEvents + events.moveCount ); }
    for ( const auto& event : moves )
    {
        if ( !b2Body_IsValid( event.bodyId ) ) { continue; }
        // A preSolve/particle callback may have removed a display object. Do not
        // dereference the cached event.userData; fetch the surviving body's value.
        void *data = b2Body_GetUserData( event.bodyId );
        if ( !data ) { physics.DestroyPhysicsBody( event.bodyId ); continue; }
        if ( data == LuaLibPhysics::GetGroundBodyUserdata() ) { continue; }
        DisplayObject *object = static_cast<DisplayObject *>(data);
        if ( object->IsOrphan() ) { continue; }
        b2Vec2 position = event.transform.p;
        position *= physics.GetPixelsPerMeter();
        Real angle = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2Rot_GetAngle(event.transform.q) ) );
        object->SetExtensionsLocked( true );
        object->SetGeometricProperty( kOriginX, position.x );
        object->SetGeometricProperty( kOriginY, position.y );
        object->SetGeometricProperty( kRotation, angle );
        object->SetExtensionsLocked( false );
    }
    physics.FlushDeferredBodyDestructions();
}

bool FixedStepScheduler::DispatchEvents( PhysicsWorld& physics, unsigned long long generation )
{
    // Copy ALL arrays before dispatch: existing (unlocked) post-step collision
    // callbacks can remove bodies, stop, or replace the world. Preserve that API.
    const b2ContactEvents contacts = b2World_GetContactEvents( physics.GetWorldId() );
    const b2SensorEvents sensors = b2World_GetSensorEvents( physics.GetWorldId() );
    std::vector<b2ContactBeginTouchEvent> begins;
    std::vector<b2ContactEndTouchEvent> ends;
    std::vector<b2ContactHitEvent> hits;
    std::vector<b2SensorBeginTouchEvent> sensorBegins;
    std::vector<b2SensorEndTouchEvent> sensorEnds;
    if (contacts.beginCount) begins.assign(contacts.beginEvents, contacts.beginEvents + contacts.beginCount);
    if (contacts.endCount) ends.assign(contacts.endEvents, contacts.endEvents + contacts.endCount);
    if (contacts.hitCount) hits.assign(contacts.hitEvents, contacts.hitEvents + contacts.hitCount);
    if (sensors.beginCount) sensorBegins.assign(sensors.beginEvents, sensors.beginEvents + sensors.beginCount);
    if (sensors.endCount) sensorEnds.assign(sensors.endEvents, sensors.endEvents + sensors.endCount);
    auto alive = [&]() { return FindFixedStep(physics)->generation == generation && physics.IsWorldValid(); };
    for (const auto& e : begins)
    {
        if (b2Contact_IsValid(e.contactId) && b2Shape_IsValid(e.shapeIdA) && b2Shape_IsValid(e.shapeIdB))
            physics.fWorldContactListener->BeginContact(e.shapeIdA, e.shapeIdB, e.contactId);
        if (!alive()) return false;
    }
    for (const auto& e : ends)
    {
        if (b2Shape_IsValid(e.shapeIdA) && b2Shape_IsValid(e.shapeIdB))
            physics.fWorldContactListener->EndContact(e.shapeIdA, e.shapeIdB);
        if (!alive()) return false;
    }
    for (auto& e : hits)
    {
        if (b2Shape_IsValid(e.shapeIdA) && b2Shape_IsValid(e.shapeIdB))
            physics.fWorldContactListener->BeginContactHit(&e);
        if (!alive()) return false;
    }
    for (const auto& e : sensorBegins)
    {
        if (b2Shape_IsValid(e.sensorShapeId) && b2Shape_IsValid(e.visitorShapeId))
            physics.fWorldContactListener->BeginContact(e.sensorShapeId, e.visitorShapeId, b2_nullContactId);
        if (!alive()) return false;
    }
    for (const auto& e : sensorEnds)
    {
        if (b2Shape_IsValid(e.sensorShapeId) && b2Shape_IsValid(e.visitorShapeId))
            physics.fWorldContactListener->EndContact(e.sensorShapeId, e.visitorShapeId);
        if (!alive()) return false;
    }
    return true;
}

bool FixedStepScheduler::Step( PhysicsWorld& physics, float frameInterval )
{
    FixedStepState *state = FindFixedStep( physics );
    if ( !state || !state->enabled ) { return false; }
    if ( state->executing || state->faulted || !physics.IsWorldValid() ||
         !physics.IsProperty( PhysicsWorld::kIsWorldRunning ) ) { return true; }
    struct ExecutionScope
    {
        FixedStepState& state;
        ExecutionScope(FixedStepState& s) : state(s) { state.executing = true; }
        ~ExecutionScope() { state.executing = false; }
    } execution(*state);
    const auto generation = state->generation;
    const auto interruption = state->interruption;
    // Render-budget policy: no wall-clock catch-up, fractional and capped whole
    // steps remain pending. Speed changes in callbacks affect the next frame grant.
    const double budget = state->budget + (double)frameInterval / state->dt * state->speed;
    if ( !std::isfinite(budget) || budget > 9007199254740991.0 )
    {
        state->faulted = true;
        state->error = "pendingSteps exceeds the exact integer range";
        physics.PauseWorld();
        return true;
    }
    state->budget = budget;
    for ( int count = 0; count < state->maxSteps && state->budget >= 1.0; ++count )
    {
        if ( state->index >= 9007199254740991ULL )
        {
            state->faulted = true;
            state->error = "stepIndex exceeds Lua's exact integer range";
            physics.PauseWorld();
            break;
        }
        const auto next = state->index + 1;
        if ( !FixedCallback( physics, *state, "before", next ) ) { break; }
        if ( state->generation != generation || !physics.IsWorldValid() ) { break; }
        // A pause in before finishes this already-prepared step (no duplicate
        // force application on resume). A stop cancels it by changing generation.
        const char *error = state->client.owner ? state->client.prepare(state->client.owner, physics) : NULL;
        if (error)
        {
            state->coupledFault = true;
            state->faulted = true;
            state->error = error;
            physics.PauseWorld();
            break;
        }
        bool entered = true;
        if (state->client.wholeStep)
        {
            // Snapshot before native entry: preCollision may detach this client.
            const Client client = state->client;
            client.retain(client.owner);
            entered = false;
            error = client.wholeStep(client.owner, physics, entered);
            // Copy diagnostics before release can delete the native owner.
            if (error) { state->error = error; }
            client.release(client.owner);
            error = error ? state->error.c_str() : NULL;
            if (!entered && !error) { error = "coupled step did not enter the rigid world"; }
        }
        else
        {
            physics.GetWorld()->Step( state->dt, state->subSteps );
            error = state->client.owner ? state->client.advance(state->client.owner, physics) : NULL;
        }
        if (entered)
        {
            state->budget -= 1.0;
            state->index = next;
        }
        if (error)
        {
            state->coupledFault = true;
            state->faulted = true;
            state->error = error;
            physics.PauseWorld();
            SyncDisplay(physics);
            break;
        }
        SyncDisplay( physics );
        if ( !DispatchEvents( physics, generation ) ) { break; }
        if ( !FixedCallback( physics, *state, "after", next ) ) { break; }
        if ( state->generation != generation || state->interruption != interruption ||
             !physics.IsProperty( PhysicsWorld::kIsWorldRunning ) ) { break; }
    }
    return true;
}

// These iterations are reasonable default values. See http://www.box2d.org/forum/viewtopic.php?f=8&t=4396 for discussion.
const S32 kSubStepCount = 4;
const S32 kVelocityIterations = 8;
const S32 kPositionIterations = 3;

// ----------------------------------------------------------------------------

// class PhysicsDestructionListener : public b2DestructionListener
// {
// 	public:
// 		virtual void SayGoodbye(b2Joint* joint);
// 		virtual void SayGoodbye(b2Fixture* fixture);
// };

// void
// PhysicsDestructionListener::SayGoodbye(b2Joint* joint)
// {
// 	UserdataWrapper *wrapper = (UserdataWrapper *)joint->GetUserData();

// 	// Check that wrapper is valid. If Lua GC'd the wrapper, then we already set the joint's ud to NULL
// 	if ( wrapper && UserdataWrapper::GetFinalizedValue() != wrapper )
// 	{
// 		wrapper->Invalidate();
// 	}
// }

// void
// PhysicsDestructionListener::SayGoodbye(b2Fixture* fixture)
// {
// }

// ----------------------------------------------------------------------------
// Box2D passes the manifold; report its deepest point to the listener as before and
// disable the contact for this step when the listener returns false.
static void PreSolveCallbackFunction( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Manifold* manifold, void* context )
{
	if ( manifold->pointCount == 0 )
	{
		return;
	}

	// The anchors are relative to bodyA's origin during pre-solve.
	b2Vec2 originA = b2Body_GetPosition( b2Shape_GetBody( shapeIdA ) );
	int deepest = 0;
	for ( int i = 1; i < manifold->pointCount; ++i )
	{
		if ( manifold->points[i].separation < manifold->points[deepest].separation )
		{
			deepest = i;
		}
	}

	const b2ManifoldPoint& mp = manifold->points[deepest];
	PhysicsContactListener* contactListener = (PhysicsContactListener*) context;
	if ( ! contactListener->PreSolve( shapeIdA, shapeIdB, originA + mp.anchorA, manifold->normal, mp.separation ) )
	{
		manifold->pointCount = 0;
	}
}

// Continuous collision time of impact: there is no manifold, the separation is zero.
static bool PreContinuousCallbackFunction( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Vec2 point, b2Vec2 normal, void* context )
{
	PhysicsContactListener* contactListener = (PhysicsContactListener*) context;
	return contactListener->PreSolve( shapeIdA, shapeIdB, point, normal, 0.0f );
}

// Core count used as the fallback when the big.LITTLE split can't be
// determined. On macOS and Windows we count physical cores so SMT/hyperthread
// siblings don't inflate the worker pool; Linux/Emscripten use the logical
// count (the big.LITTLE detection above already covers the ARM case).
static int
GetFallbackCoreCount()
{
#if defined( _WIN32 )
	// Each RelationProcessorCore entry is one physical core (SMT siblings are
	// folded into a single entry).
	DWORD length = 0;
	GetLogicalProcessorInformationEx( RelationProcessorCore, NULL, &length );
	if ( length > 0 )
	{
		SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *buffer =
			(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)malloc( length );
		if ( buffer )
		{
			int physicalCores = 0;
			if ( GetLogicalProcessorInformationEx( RelationProcessorCore, buffer, &length ) )
			{
				char *ptr = (char *)buffer;
				char *end = ptr + length;
				while ( ptr < end )
				{
					SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *info =
						(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)ptr;
					if ( info->Relationship == RelationProcessorCore )
					{
						++physicalCores;
					}
					ptr += info->Size;
				}
			}
			free( buffer );
			if ( physicalCores > 0 )
			{
				return physicalCores;
			}
		}
	}

	// Query failed: fall back to the logical processor count.
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	return (int)sysinfo.dwNumberOfProcessors;
#elif defined( __APPLE__ )
	// Physical cores on Intel Macs (the Apple Silicon case is handled by
	// perflevel0 before we ever reach this fallback).
	int physicalCores = 0;
	size_t size = sizeof( physicalCores );
	if ( sysctlbyname( "hw.physicalcpu", &physicalCores, &size, NULL, 0 ) == 0 && physicalCores > 0 )
	{
		return physicalCores;
	}
	return (int)sysconf( _SC_NPROCESSORS_ONLN );
#elif defined( __linux__ ) || defined( __EMSCRIPTEN__ )
	return (int)sysconf( _SC_NPROCESSORS_ONLN );
#else
	return 1;
#endif
}

// #if defined( __ANDROID__ ) || defined( __linux__ )
// static long
// ReadCpuMaxFreq( int cpuIndex )
// {
// 	char path[64];
// 	snprintf( path, sizeof( path ), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpuIndex );

// 	long freq = 0;
// 	FILE *f = fopen( path, "r" );
// 	if ( f )
// 	{
// 		if ( fscanf( f, "%ld", &freq ) != 1 )
// 		{
// 			freq = 0;
// 		}
// 		fclose( f );
// 	}

// 	return freq;
// }
// #endif

// See PhysicsWorld::GetNumHardwareThreads() for the rationale. Returns the
// count of performance-class cores, or the fallback core count when undetectable.
static int
DetectPerformanceCoreCount()
{
#if defined( __APPLE__ )
	// hw.perflevel0 is the performance ("P") cluster on Apple Silicon and
	// A-series SoCs (macOS 11.3+ / iOS 14+); perflevel1, when present, is the
	// efficiency ("E") cluster we intentionally skip.
	int perfCores = 0;
	size_t size = sizeof( perfCores );
	if ( sysctlbyname( "hw.perflevel0.logicalcpu", &perfCores, &size, NULL, 0 ) == 0 && perfCores > 0 )
	{
		return perfCores;
	}
// #elif defined( __ANDROID__ ) || defined( __linux__ )
// 	// No portable big.LITTLE API on Android: infer the performance cluster
// 	// from CPU max frequency and count the cores tied for the highest value.
// 	// Two passes so we don't have to buffer per-core data.
// 	int total = (int)sysconf( _SC_NPROCESSORS_CONF );
// 	if ( total > 0 )
// 	{
// 		long maxFreq = 0;
// 		for ( int i = 0; i < total; ++i )
// 		{
// 			long freq = ReadCpuMaxFreq( i );
// 			if ( freq > maxFreq )
// 			{
// 				maxFreq = freq;
// 			}
// 		}

// 		if ( maxFreq > 0 )
// 		{
// 			int perfCores = 0;
// 			for ( int i = 0; i < total; ++i )
// 			{
// 				if ( ReadCpuMaxFreq( i ) == maxFreq )
// 				{
// 					++perfCores;
// 				}
// 			}

// 			if ( perfCores > 0 )
// 			{
// 				return perfCores;
// 			}
// 		}
// 	}
#endif
	// Desktop / web / detection failure: every core is fair game.
	return GetFallbackCoreCount();
}

PhysicsWorld::PhysicsWorld( Rtt_Allocator& allocator )
:	fAllocator( allocator ),
	fWorldDebugDraw( NULL ),
	// fWorldDestructionListener( NULL ),
	fWorldContactListener( NULL ),
	fReportCollisionsInContentCoordinates( false ),
	fLuaAssertEnabled( false ),
	fAverageCollisionPositions( false ),
	fProperties( 0 ),
	fWorld( NULL ),
	// fWorldId( b2_nullWorldId ),
	fPixelsPerMeter( 30.0f ), // default on iPhone
	// fGroundBody( NULL ),
	fSubStepCount( kSubStepCount ),
	fVelocityIterations( kVelocityIterations ),
	fPositionIterations( kPositionIterations ),
	fFrameInterval( -1.0f ),
	fTimeStep( -1.0f ), // Set time step equal to frame interval
	fTimeScale( 1.0f ),
	fTimePrevious( -1.f ),
	fTimeRemainder( 0.0f ),
	fNumSteps(1),
	fCompoundInternalEdgeSuppressionEnabled( false )
{
	fMouseBodies.reserve( estimateMaxMouseBodies );

	// GetNumHardwareThreads() already reports performance-class cores only.
	// Leave one for the main/render thread; the rest form Box2D's worker pool.
	const int perfCores = GetNumHardwareThreads();
	fWorkerCount = b2MinInt( 8, b2MaxInt( perfCores, 1 ) );
}

PhysicsWorld::~PhysicsWorld()
{
	// if ( fWorld )
	// {
	// 	fWorld->SetContactListener( NULL );
	// }

	StopWorld();
	FixedStepScheduler::Forget( *this );
}

void
PhysicsWorld::Initialize( float frameInterval )
{
	Rtt_ASSERT( fFrameInterval < 0.f );

	fFrameInterval = frameInterval;
}

void
PhysicsWorld::WillDestroyDisplay()
{
	// if ( fWorld )
	// {
	// 	fWorld->SetContactListener( NULL );
	// }
}

void
PhysicsWorld::StartWorld( Runtime& runtime, bool noSleep )
{
	if ( ! fWorld )
	{
		Rtt_ASSERT( ! IsProperty( kIsWorldRunning ) );

		// Note that gravity is oriented along positive y-axis in Corona coordinates
		// (we are flipping the "handedness" of the Box2d world to make the coordinate system the same as Corona)
		// Hence, in the shape API we declare polygon coordinates in clockwise (rather than counterclockwise) order, due to this world inversion

		// Default to Earthlike gravity
		// b2Vec2 gravity( 0.0f, 9.8f );
		b2Vec2 gravity = {0.0f, 9.8f};

		SetVelocityIterations( kVelocityIterations );
		SetPositionIterations( kPositionIterations );
		SetTimeStep( -1.f ); // Set time step equal to frame interval
		fTimePrevious = -1.f;
		fTimeRemainder = 0.f;
		FixedStepScheduler::Reset( *this );

		// fWorld = Rtt_NEW( Allocator(), b2World( gravity ) );
		// fWorldDestructionListener = Rtt_NEW( Allocator(), PhysicsDestructionListener );
		// fWorld->SetDestructionListener( fWorldDestructionListener );
		b2WorldDef worldDef = b2DefaultWorldDef();
		// Rtt_Log("PhysicsWorld::StartWorld, workerCount = %d", fWorkerCount);

		if ( fWorkerCount > 1 ) {
			worldDef.workerCount = fWorkerCount;
			// worldDef.enqueueTask = EnqueueTask;
			// worldDef.finishTask = FinishTask;
			worldDef.userTaskContext = this;
		}
		worldDef.gravity = gravity;
		worldDef.enableSleep = !noSleep;
		b2WorldId worldId = b2CreateWorld( &worldDef );
		b2World_EnableGlobalPreSolveEvents(
			worldId,
			IsProperty( kRuntimePreCollisionListenerExists ) );

		fWorld = Rtt_NEW( Allocator(), b2LiquidWorld( worldId ) );

		// The noSleep flag sets whether to simulate inactive bodies, or allow them to "sleep" after a few seconds
		// of no interaction. The recommended default is to allow sleep. Our exposed boolean should be the opposite,
		// so that it can default to false, as expected for all Corona booleans.
		// fWorld->SetAllowSleeping( !noSleep );
		// b2World_EnableSleeping(fWorldId, !noSleep);

		// More world setup
		fWorldContactListener = Rtt_NEW( Allocator(), PhysicsContactListener( runtime ) );
		fWorld->SetContactListener( fWorldContactListener );

		fWorldDebugDraw = Rtt_NEW( Allocator(), b2GLESDebugDraw( runtime.GetDisplay() ) );

		// uint32 debugFlags =
		// 	b2Draw::e_shapeBit |
		// 	b2Draw::e_jointBit |
		// 	// b2Draw::e_aabbBit |
		// 	b2Draw::e_pairBit |
		// 	b2Draw::e_centerOfMassBit |
		// 	b2Draw::e_particleBit;
		// fWorldDebugDraw->AppendFlags( debugFlags );

		// fWorld->SetDebugDraw( fWorldDebugDraw );

		// Initialize a ground body, so that joints can be attached to "the world"
		// b2BodyDef bd;
		// bd.userData = const_cast< void* >( LuaLibPhysics::GeGetMouseBodyIdtGroundBodyUserdata() );
		// fGroundBody = fWorld->CreateBody(&bd);
		// b2BodyDef bd = b2DefaultBodyDef();
		// bd.type = b2_kinematicBody;
		// bd.enableSleep = false;
		// bd.userData = const_cast< void* >( LuaLibPhysics::GetGroundBodyUserdata() );
		// fMouseBodyId = b2CreateBody( fWorld->GetWorldId(), &bd );
		// b2ShapeDef shapeDef = b2DefaultShapeDef();
		// shapeDef.filter = { 1, 0, 0 };
		// shapeDef.isSensor = true;
		// shapeDef.enableContactEvents = false;
		// shapeDef.enablePreSolveEvents = false;
		// b2Segment segment = { {-20.0f, 0.0f}, {20.0f, 0.0f} };
		// b2CreateSegmentShape( fMouseBodyId, &shapeDef, &segment );

		b2World_SetPreSolveCallback( fWorld->GetWorldId(), PreSolveCallbackFunction, PreContinuousCallbackFunction, fWorldContactListener );
	}

	SetProperty( kIsWorldRunning, true );
}

void
PhysicsWorld::PauseWorld()
{
	FixedStepScheduler::Interrupt( *this );
	if ( fWorld )
	{
		SetProperty( kIsWorldRunning, false );
	}
}

void
PhysicsWorld::ResumeWorld()
{
	if ( fWorld )
	{
		SetProperty( kIsWorldRunning, true );
	}
}

void
PhysicsWorld::onSuspended()
{
}

void
PhysicsWorld::onResumed()
{
}

void
PhysicsWorld::StopWorld()
{
	FixedStepScheduler::Invalidate( *this );
	FixedStepScheduler::Reset( *this );
	if ( fWorld )
	{
		SetProperty( kIsWorldRunning, false );

		ParticleSystemObject::DetachWorld( fWorld );
		// LiquidFun queries the world lock while destroying its particle systems.
		// Keep the Box2D world alive until that cleanup has completed.
		const b2WorldId worldId = fWorld->GetWorldId();
		Rtt_DELETE( fWorld );
		fWorld = NULL;
		b2DestroyWorld( worldId );
		fPhysicsBodies.clear();
		fDeferredBodyDestructions.clear();
		fCompoundInternalEdgePreSolveShapes.clear();
		fCompoundInternalEdges.clear();

		// Clear mouse body pool (all IDs are now invalid after world destruction)
		fMouseBodies.clear();

		Rtt_DELETE( fWorldContactListener );
		fWorldContactListener = NULL;

		Rtt_DELETE( fWorldDebugDraw );
		fWorldDebugDraw = NULL;
	}
}

void
PhysicsWorld::SetProperty( Properties mask, bool value )
{
	const Properties p = fProperties;
	fProperties = ( value ? p | mask : p & ~mask );
}

void
PhysicsWorld::SetRuntimePreCollisionListenerExists( bool value )
{
	SetProperty( kRuntimePreCollisionListenerExists, value );
	if ( fWorld )
	{
		b2World_EnableGlobalPreSolveEvents( fWorld->GetWorldId(), value );
	}
}

void
PhysicsWorld::RegisterPhysicsBody( b2BodyId bodyId )
{
	if ( ! b2Body_IsValid( bodyId ) )
	{
		return;
	}

	for ( size_t i = 0; i < fPhysicsBodies.size(); ++i )
	{
		if ( B2_ID_EQUALS( fPhysicsBodies[i], bodyId ) )
		{
			return;
		}
	}

	fPhysicsBodies.push_back( bodyId );
	if ( fCompoundInternalEdgeSuppressionEnabled )
	{
		BuildCompoundInternalEdges( bodyId );
	}
}

void
PhysicsWorld::RemoveCompoundInternalEdges( b2BodyId bodyId, bool disablePreSolveEvents )
{
	size_t writeIndex = 0;
	for ( size_t i = 0; i < fCompoundInternalEdgePreSolveShapes.size(); ++i )
	{
		const CompoundInternalEdgePreSolveShape& entry = fCompoundInternalEdgePreSolveShapes[i];
		if ( B2_ID_EQUALS( entry.bodyId, bodyId ) )
		{
			if ( disablePreSolveEvents && b2Shape_IsValid( entry.shapeId ) )
			{
				b2Shape_EnablePreSolveEvents( entry.shapeId, false );
			}
		}
		else
		{
			fCompoundInternalEdgePreSolveShapes[writeIndex++] = entry;
		}
	}
	fCompoundInternalEdgePreSolveShapes.resize( writeIndex );

	writeIndex = 0;
	for ( size_t i = 0; i < fCompoundInternalEdges.size(); ++i )
	{
		const CompoundInternalEdge& edge = fCompoundInternalEdges[i];
		if ( B2_ID_EQUALS( edge.bodyId, bodyId ) == false )
		{
			fCompoundInternalEdges[writeIndex++] = edge;
		}
	}
	fCompoundInternalEdges.resize( writeIndex );
}

void
PhysicsWorld::UnregisterPhysicsBody( b2BodyId bodyId )
{
	size_t writeIndex = 0;
	for ( size_t i = 0; i < fPhysicsBodies.size(); ++i )
	{
		if ( B2_ID_EQUALS( fPhysicsBodies[i], bodyId ) == false )
		{
			fPhysicsBodies[writeIndex++] = fPhysicsBodies[i];
		}
	}
	fPhysicsBodies.resize( writeIndex );

	RemoveCompoundInternalEdges( bodyId, true );
}

void
PhysicsWorld::DestroyPhysicsBody( b2BodyId bodyId )
{
	if ( ! b2Body_IsValid( bodyId ) )
	{
		UnregisterPhysicsBody( bodyId );
		return;
	}

	if ( fWorld && fWorld->IsLocked() )
	{
		// Box2D does not permit structural changes during callbacks. Clearing the
		// userdata is safe and prevents later callbacks from reaching the removed
		// DisplayObject until destruction is flushed after the step.
		b2Body_SetUserData( bodyId, NULL );
		for ( size_t i = 0; i < fDeferredBodyDestructions.size(); ++i )
		{
			if ( B2_ID_EQUALS( fDeferredBodyDestructions[i], bodyId ) )
			{
				return;
			}
		}
		fDeferredBodyDestructions.push_back( bodyId );
		return;
	}

	UnregisterPhysicsBody( bodyId );
	b2DestroyBody( bodyId );
}

void
PhysicsWorld::FlushDeferredBodyDestructions()
{
	for ( size_t i = 0; i < fDeferredBodyDestructions.size(); ++i )
	{
		b2BodyId bodyId = fDeferredBodyDestructions[i];
		if ( b2Body_IsValid( bodyId ) )
		{
			UnregisterPhysicsBody( bodyId );
			b2DestroyBody( bodyId );
		}
		else
		{
			UnregisterPhysicsBody( bodyId );
		}
	}
	fDeferredBodyDestructions.clear();
}

void
PhysicsWorld::InvalidateCompoundInternalEdges( b2BodyId bodyId )
{
	RemoveCompoundInternalEdges( bodyId, true );
}

void
PhysicsWorld::RefreshCompoundInternalEdges( b2BodyId bodyId )
{
	RemoveCompoundInternalEdges( bodyId, true );
	if ( fCompoundInternalEdgeSuppressionEnabled && b2Body_IsValid( bodyId ) )
	{
		BuildCompoundInternalEdges( bodyId );
	}
}

void
PhysicsWorld::ReleaseCompoundInternalEdgePreSolveOwnership( b2ShapeId shapeId )
{
	size_t writeIndex = 0;
	for ( size_t i = 0; i < fCompoundInternalEdgePreSolveShapes.size(); ++i )
	{
		const CompoundInternalEdgePreSolveShape& entry = fCompoundInternalEdgePreSolveShapes[i];
		if ( B2_ID_EQUALS( entry.shapeId, shapeId ) == false )
		{
			fCompoundInternalEdgePreSolveShapes[writeIndex++] = entry;
		}
	}
	fCompoundInternalEdgePreSolveShapes.resize( writeIndex );
}

void
PhysicsWorld::EnableCompoundInternalEdgePreSolve( b2BodyId bodyId, b2ShapeId shapeId )
{
	if ( b2Shape_ArePreSolveEventsEnabled( shapeId ) )
	{
		return;
	}

	b2Shape_EnablePreSolveEvents( shapeId, true );
	CompoundInternalEdgePreSolveShape entry = { bodyId, shapeId };
	fCompoundInternalEdgePreSolveShapes.push_back( entry );
}

void
PhysicsWorld::BuildCompoundInternalEdges( b2BodyId bodyId )
{
	if ( ! b2Body_IsValid( bodyId ) )
	{
		return;
	}

	int shapeCount = b2Body_GetShapeCount( bodyId );
	if ( shapeCount < 2 )
	{
		return;
	}

	struct PolygonShape
	{
		b2ShapeId shapeId;
		b2Polygon polygon;
	};

	std::vector<b2ShapeId> shapeIds( shapeCount );
	b2Body_GetShapes( bodyId, shapeIds.data(), shapeCount );

	std::vector<PolygonShape> polygons;
	polygons.reserve( shapeCount );
	for ( int i = 0; i < shapeCount; ++i )
	{
		if ( b2Shape_GetType( shapeIds[i] ) == b2_polygonShape )
		{
			PolygonShape entry = { shapeIds[i], b2Shape_GetPolygon( shapeIds[i] ) };
			polygons.push_back( entry );
		}
	}

	if ( polygons.size() < 2 )
	{
		return;
	}

	const float edgeTolerance = 0.5f * 0.005f * b2GetLengthUnitsPerMeter();
	const float edgeToleranceSquared = edgeTolerance * edgeTolerance;
	const float opposingNormalLimit = -0.995f;

	for ( size_t polygonIndexA = 0; polygonIndexA + 1 < polygons.size(); ++polygonIndexA )
	{
		const PolygonShape& polygonShapeA = polygons[polygonIndexA];
		const b2Polygon& polygonA = polygonShapeA.polygon;

		for ( size_t polygonIndexB = polygonIndexA + 1; polygonIndexB < polygons.size(); ++polygonIndexB )
		{
			const PolygonShape& polygonShapeB = polygons[polygonIndexB];
			const b2Polygon& polygonB = polygonShapeB.polygon;

			for ( int edgeIndexA = 0; edgeIndexA < polygonA.count; ++edgeIndexA )
			{
				int nextIndexA = edgeIndexA + 1 < polygonA.count ? edgeIndexA + 1 : 0;
				b2Vec2 pointA1 = polygonA.vertices[edgeIndexA];
				b2Vec2 pointA2 = polygonA.vertices[nextIndexA];

				for ( int edgeIndexB = 0; edgeIndexB < polygonB.count; ++edgeIndexB )
				{
					int nextIndexB = edgeIndexB + 1 < polygonB.count ? edgeIndexB + 1 : 0;
					b2Vec2 pointB1 = polygonB.vertices[edgeIndexB];
					b2Vec2 pointB2 = polygonB.vertices[nextIndexB];

					bool reversedEndpoints = b2DistanceSquared( pointA1, pointB2 ) <= edgeToleranceSquared &&
						b2DistanceSquared( pointA2, pointB1 ) <= edgeToleranceSquared;
					bool opposingNormals = b2Dot( polygonA.normals[edgeIndexA], polygonB.normals[edgeIndexB] ) <= opposingNormalLimit;
					if ( reversedEndpoints == false || opposingNormals == false )
					{
						continue;
					}

					CompoundInternalEdge edgeA = {
						bodyId, polygonShapeA.shapeId, pointA1, pointA2, polygonA.normals[edgeIndexA]
					};
					CompoundInternalEdge edgeB = {
						bodyId, polygonShapeB.shapeId, pointB1, pointB2, polygonB.normals[edgeIndexB]
					};
					fCompoundInternalEdges.push_back( edgeA );
					fCompoundInternalEdges.push_back( edgeB );
					EnableCompoundInternalEdgePreSolve( bodyId, polygonShapeA.shapeId );
					EnableCompoundInternalEdgePreSolve( bodyId, polygonShapeB.shapeId );
				}
			}
		}
	}
}

void
PhysicsWorld::SetCompoundInternalEdgeSuppressionEnabled( bool enabled )
{
	if ( fCompoundInternalEdgeSuppressionEnabled == enabled )
	{
		return;
	}

	fCompoundInternalEdgeSuppressionEnabled = enabled;
	if ( enabled )
	{
		fCompoundInternalEdges.clear();
		fCompoundInternalEdgePreSolveShapes.clear();

		std::vector<b2BodyId> validBodies;
		validBodies.reserve( fPhysicsBodies.size() );
		for ( size_t i = 0; i < fPhysicsBodies.size(); ++i )
		{
			b2BodyId bodyId = fPhysicsBodies[i];
			if ( b2Body_IsValid( bodyId ) )
			{
				validBodies.push_back( bodyId );
				BuildCompoundInternalEdges( bodyId );
			}
		}
		fPhysicsBodies.swap( validBodies );
	}
	else
	{
		for ( size_t i = 0; i < fCompoundInternalEdgePreSolveShapes.size(); ++i )
		{
			b2ShapeId shapeId = fCompoundInternalEdgePreSolveShapes[i].shapeId;
			if ( b2Shape_IsValid( shapeId ) )
			{
				b2Shape_EnablePreSolveEvents( shapeId, false );
			}
		}

		fCompoundInternalEdgePreSolveShapes.clear();
		fCompoundInternalEdges.clear();
	}
}

bool
PhysicsWorld::ShouldSuppressCompoundInternalEdge( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Vec2 point, b2Vec2 normal ) const
{
	if ( fCompoundInternalEdgeSuppressionEnabled == false ||
		 ! b2Shape_IsValid( shapeIdA ) || ! b2Shape_IsValid( shapeIdB ) )
	{
		return false;
	}

	b2ShapeType shapeTypeA = b2Shape_GetType( shapeIdA );
	b2ShapeType shapeTypeB = b2Shape_GetType( shapeIdB );
	bool polygonIsA = shapeTypeA == b2_polygonShape && shapeTypeB == b2_circleShape;
	bool polygonIsB = shapeTypeA == b2_circleShape && shapeTypeB == b2_polygonShape;
	if ( polygonIsA == false && polygonIsB == false )
	{
		return false;
	}

	b2ShapeId polygonShapeId = polygonIsA ? shapeIdA : shapeIdB;
	b2Vec2 outwardNormal = polygonIsA ? normal : b2Vec2{ -normal.x, -normal.y };
	b2BodyId polygonBodyId = b2Shape_GetBody( polygonShapeId );
	b2Vec2 localPoint = b2Body_GetLocalPoint( polygonBodyId, point );
	b2Vec2 localNormal = b2Body_GetLocalVector( polygonBodyId, outwardNormal );

	const float normalAlignmentLimit = 0.995f;
	const float edgeTolerance = 0.5f * 0.005f * b2GetLengthUnitsPerMeter();
	for ( size_t i = 0; i < fCompoundInternalEdges.size(); ++i )
	{
		const CompoundInternalEdge& edge = fCompoundInternalEdges[i];
		if ( B2_ID_EQUALS( edge.shapeId, polygonShapeId ) == false ||
			 b2Dot( edge.normal, localNormal ) < normalAlignmentLimit )
		{
			continue;
		}

		b2Vec2 edgeVector = b2Sub( edge.point2, edge.point1 );
		float edgeLength = b2Length( edgeVector );
		if ( edgeLength <= edgeTolerance )
		{
			continue;
		}

		float projection = b2Dot( b2Sub( localPoint, edge.point1 ), edgeVector );
		float projectionTolerance = edgeTolerance * edgeLength;
		if ( projection >= -projectionTolerance &&
			 projection <= edgeLength * edgeLength + projectionTolerance )
		{
			return true;
		}
	}

	return false;
}

void
PhysicsWorld::SetTimeStep( float newValue )
{
	if ( newValue > 0.f )
	{
		fTimeStep = newValue;
	}
	else if ( newValue < 0.f )
	{
		fTimeStep = fFrameInterval;
	}
	else
	{
		fTimeStep = Rtt_REAL_0;
		fTimePrevious = -1.f;
	}
}

void
PhysicsWorld::SetReportCollisionsInContentCoordinates( bool enabled )
{
	fReportCollisionsInContentCoordinates = enabled;
}

bool
PhysicsWorld::GetReportCollisionsInContentCoordinates() const
{
	return fReportCollisionsInContentCoordinates;
}

void
PhysicsWorld::SetLuaAssertEnabled( bool enabled )
{
	fLuaAssertEnabled = enabled;
}

bool
PhysicsWorld::GetLuaAssertEnabled() const
{
	return fLuaAssertEnabled;
}

void
PhysicsWorld::SetAverageCollisionPositions( bool enabled )
{
	fAverageCollisionPositions = enabled;
}

bool
PhysicsWorld::GetAverageCollisionPositions() const
{
	return fAverageCollisionPositions;
}

void
PhysicsWorld::DebugDraw( Renderer &renderer ) const
{
	if( ! fWorld )
	// if ( !b2World_IsValid(fWorldId) )
	{
		// Nothing to do.
		return;
	}

	fWorldDebugDraw->DrawDebugData( * this, renderer );
//	fWorldDebugDraw->Begin( *this,
//							renderer );
//	{
//		LuaLibPhysics::DebugDraw( fWorld,
//									fWorldDebugDraw,
//									GetMetersPerPixel() );
//	}
//	fWorldDebugDraw->End();
//
}

void
PhysicsWorld::StepWorld( double elapsedMS )
{
	if ( FixedStepScheduler::Step( *this, fFrameInterval ) ) { return; }
	if ( fWorld && IsProperty( kIsWorldRunning ) )
	// if ( b2World_IsValid( fWorldId ) && IsProperty( kIsWorldRunning ) )
	{
		// Rtt_Log( "PhysicsWorld::StepWorld, world gravity = (%f, %f)", b2World_GetGravity(fWorldId).x, b2World_GetGravity(fWorldId).y );

		// These values may be changed on the fly. TODO: make sure this isn't occurring real overhead, or we should drop back to default values only!
		// S32 velocityIterations = GetVelocityIterations();
		// S32 positionIterations = GetPositionIterations();

		b2LiquidWorld& world = * fWorld;

		bool stepped = false;
		float dt = GetTimeStep();
		if ( dt > Rtt_REAL_0 )
		{
			// world.SetAutoClearForces(false);
			for (S32 i = 0; i < fNumSteps; ++i)
			{
				// if (i == fNumSteps - 1) {
				// 	world.SetAutoClearForces(true);
				// }
				// Simulation timesteps are driven by the render frame rate
				// world.Step( dt * fTimeScale, velocityIterations, positionIterations );
				// Rtt_Log( "PhysicsWorld::StepWorld A, timeStep = %f, step=%d, fSubStepCount=%d", dt * fTimeScale, i, fSubStepCount );
				// b2World_Step(fWorldId, dt * fTimeScale, fSubStepCount);
				world.Step(dt * fTimeScale, fSubStepCount);
				stepped = true;
				StepEvents();
			}
		}
		else
		{
			dt = fFrameInterval;

			// Simulation timesteps match actual time with an error <= dt
			// For more info: http://gafferongames.com/game-physics/fix-your-timestep/
			// NOTE: times are in seconds, not milliseconds
			float tCurrent = elapsedMS * 0.001f;
			float tPrevious = ( fTimePrevious > 0.f
				? fTimePrevious
				: ( tCurrent - dt ) );

			 // time elapsed between current and previous frame plus the remainder from the previous step
			float tStep = ( tCurrent - tPrevious ) + fTimeRemainder;

			while ( tStep >= dt )
			{
				// world.SetAutoClearForces(false);
				for (S32 i = 0; i < fNumSteps; ++i)
				{
					// if (i == fNumSteps - 1) {
					// 	world.SetAutoClearForces(true);
					// }
					// world.Step( dt * fTimeScale, velocityIterations, positionIterations );
					// Rtt_Log( "PhysicsWorld::StepWorld B, timeStep = %f, tStep = %f, step=%d", dt, tStep, i );
					// b2World_Step(fWorldId, dt * fTimeScale, fSubStepCount);
					world.Step(dt * fTimeScale, fSubStepCount);
					stepped = true;
					StepEvents();
				}
				tStep -= dt;
			}

			fTimePrevious = tCurrent;
			fTimeRemainder = tStep;
		}

		// A zero-step frame still exposes the previous step's move events. Those
		// transforms may be stale and their display objects may have been removed.
		if ( ! stepped )
		{
			FlushDeferredBodyDestructions();
			return;
		}

		Real scale = GetPixelsPerMeter();

		const void *groundBodyUserdata = LuaLibPhysics::GetGroundBodyUserdata();

		// Iterate over bodies, and update sprites (display objects)
		b2BodyEvents events = b2World_GetBodyEvents(world.GetWorldId());
		// for ( b2Body *body = world.GetBodyList(), *nextBody = NULL;
		// 	  NULL != body;
		// 	  body = nextBody )
		for (int i = 0; i < events.moveCount; ++i)
		{
			const b2BodyMoveEvent* event = events.moveEvents + i;
			// Prefetch next body in case we delete body
			// nextBody = body->GetNext();

			if ( ! b2Body_IsValid( event->bodyId ) ) { continue; }
			// Collision callbacks may have removed the object since this event was
			// generated. Read the surviving body's current pointer, not its snapshot.
			void* userData = b2Body_GetUserData( event->bodyId );
			// if ( body->GetUserData() )
			// Rtt_Log( "PhysicsWorld::StepWorld, index = %d, userData is not ground: %d,  userdata exists: %d", i, userData != groundBodyUserdata, userData != nullptr );
			if ( userData )
			{
				if ( userData != groundBodyUserdata )
				{
					// DisplayObject *o = (DisplayObject*)body->GetUserData();
					DisplayObject *o = (DisplayObject*)userData;
					if ( ! o->IsOrphan() )
					{
						// While updating DisplayObject transform based on Box2d body,
						// inhibit updates to corresponding Box2d body.
						o->SetExtensionsLocked( true );

						// b2Vec2 position = body->GetPosition();
						// Rtt_ASSERT(position.IsValid());
						b2Vec2 position = event->transform.p;
						Rtt_ASSERT(b2IsValidVec2(position));
						// b2Vec2 position2 = b2Body_GetPosition(event->bodyId);
						// Rtt_Log( "PhysicsWorld::StepWorld1, index = %d, position = (%f, %f), scale = %f", i, position.x, position.y, scale);
						// Rtt_Log( "PhysicsWorld::StepWorld2, index = %d, position2 = (%f, %f)", i, position2.x, position2.y);
						position *= scale;
						// Rtt_Log( "PhysicsWorld::StepWorld2, index = %d, position = (%f, %f), scale = %f", i, position.x, position.y, scale);
						Real angle = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2Rot_GetAngle(event->transform.q) ) );
						o->SetGeometricProperty( kOriginX, position.x );
						o->SetGeometricProperty( kOriginY, position.y );
						o->SetGeometricProperty( kRotation, angle );

						o->SetExtensionsLocked( false );
					}
				}
			}
			else
			{
				// We assume that any body with no UserData should be destroyed here, since the UserData initially stores the corresponding
				// Corona display object on body construction, and is then set to NULL when the corresponding display object has been deleted.
				// world.DestroyBody( body );
				DestroyPhysicsBody( event->bodyId );
			}
		}

		FlushDeferredBodyDestructions();

		/*
		void *finalizedUserdata = UserdataWrapper::GetFinalizedValue();
		// Iterate over joints, and remove any that the user has deleted
		for ( b2Joint *joint = world.GetJointList(), *nextJoint = NULL;
			  NULL != joint;
			  joint = nextJoint )
		{
			// Prefetch next joint in case we delete joint
			nextJoint = joint->GetNext();

			if ( finalizedUserdata == joint->GetUserData() )
			{
				// We assume that any joint with no UserData should be destroyed here, since the UserData initially stores the corresponding
				// UserdataWrapper on joint construction, and is then set to NULL when the user calls joint:removeSelf().
				world.DestroyJoint( joint );
			}
		}
		*/
	}
}

void
PhysicsWorld::StepEvents() {
	b2ContactEvents contactEvents = b2World_GetContactEvents( fWorld->GetWorldId() );
	for ( int i = 0; i < contactEvents.beginCount; ++i )
	{
		b2ContactBeginTouchEvent event = contactEvents.beginEvents[i];
		if ( b2Contact_IsValid( event.contactId ) )
		{
			fWorldContactListener->BeginContact( event.shapeIdA, event.shapeIdB, event.contactId );
		}
	}

	for ( int i = 0; i < contactEvents.endCount; ++i )
	{
		b2ContactEndTouchEvent event = contactEvents.endEvents[i];
		if ( b2Shape_IsValid( event.shapeIdA ) && b2Shape_IsValid( event.shapeIdB ))
		{
			fWorldContactListener->EndContact( event.shapeIdA, event.shapeIdB );
		}
	}

	for ( int i = 0; i < contactEvents.hitCount; ++i )
	{
		b2ContactHitEvent* event = contactEvents.hitEvents + i;
		fWorldContactListener->BeginContactHit( event );
	}

	b2SensorEvents sensorEvents = b2World_GetSensorEvents( fWorld->GetWorldId() );
	for ( int i = 0; i < sensorEvents.beginCount; ++i )
	{
		b2SensorBeginTouchEvent event = sensorEvents.beginEvents[i];
		fWorldContactListener->BeginContact( event.sensorShapeId, event.visitorShapeId, b2_nullContactId );
	}

	for ( int i = 0; i < sensorEvents.endCount; ++i )
	{
		b2SensorEndTouchEvent event = sensorEvents.endEvents[i];
		if ( b2Shape_IsValid( event.sensorShapeId ) && b2Shape_IsValid( event.visitorShapeId ) )
		{
			fWorldContactListener->EndContact( event.sensorShapeId, event.visitorShapeId );
		}
	}
}

b2BodyId PhysicsWorld::FetchUsableMouseBodyId() {
	b2BodyId foundMouseBodyId = b2_nullBodyId;
	int foundIndex = -1;
	for (int i = 0; i < fMouseBodies.size(); ++i) {
		if ( b2Body_GetJointCount( fMouseBodies[i] ) == 0 ) {
			foundMouseBodyId = fMouseBodies[i];
			foundIndex = i;
			break;
		}
	}
	// Rtt_Log( "FetchUsableMouseBodyId %d", foundIndex );
	if ( ! b2Body_IsValid( foundMouseBodyId ) ) {
		b2BodyDef bd = b2DefaultBodyDef();
		bd.type = b2_kinematicBody;
		bd.enableSleep = false;
		bd.userData = const_cast< void* >( LuaLibPhysics::GetGroundBodyUserdata() );
		foundMouseBodyId = b2CreateBody( fWorld->GetWorldId(), &bd );
		fMouseBodies.emplace_back( foundMouseBodyId );
	}
	return foundMouseBodyId;
}

void PhysicsWorld::SetWorkerCount( int newValue )
{
	fWorkerCount = b2MaxInt( newValue, 1 );
	if ( fWorld )
	{
		b2World_SetWorkerCount( fWorld->GetWorldId(), fWorkerCount );
	}
}

// Number of CPU cores worth handing Box2D worker threads.
//
// On big.LITTLE mobile SoCs (Apple A-series, most ARM Android) the
// raw core count over-reports usable parallelism: a worker placed on
// a slow efficiency core stalls every sub-step barrier on the laggard
// and drains battery for little gain. We report only the
// performance-class ("big") cores, falling back to the total core
// count when the layout can't be detected (desktop, web, or a failed
// query).
int PhysicsWorld::GetNumHardwareThreads() const
{
	// The performance-core count is constant for the process lifetime, so
	// the (potentially sysfs-reading) detection runs only once.
	static const int sThreadCount = DetectPerformanceCoreCount();
	return sThreadCount;
}
// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
