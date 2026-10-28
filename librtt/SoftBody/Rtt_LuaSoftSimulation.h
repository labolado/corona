#pragma once
// Lua/lifetime adapter for the native simulation API. No solver storage or
// contact-cache access belongs here. Only the assembly runtime is registered.
#include "../../external/box2d_v3/extern/softbody/soft_simulation.h"
#include "../../external/box2d_v3/extern/softbody/soft_box2d_simulation.h"
#include "Rtt_FixedStepScheduler.h"
#include <cmath>
#include <cstring>
#include <new>
#include <limits>

namespace Rtt
{
namespace SoftSimulationLua
{
static const char kWorld[] = "physics.softSimulationWorld";
static const char kBody[] = "physics.softSimulationBody";
static const char kJoint[] = "physics.softSimulationJoint";
static const char kRigidJoint[] = "physics.softSimulationRigidJoint";
static const char kTouch[] = "physics.softSimulationTouch";
static const char kAttachment[] = "physics.softSimulationAttachment";
static int& LiveCount()
{
    static int count = 0;
    return count;
}
struct Instance
{
    SoftBody::Simulation simulation;
    SoftBody::Box2DCoupling coupling;
    PhysicsWorld* physics = nullptr;
    b2WorldId physicsId = b2_nullWorldId;
    float scale = 0;
    float timeStep;
    int subSteps;
    unsigned activeSteps = 0;
    bool invalidated = false, removePending = false;
    Instance( const SoftBody::SimulationDef& def, const SoftBody::Box2DCouplingDef& box )
        : simulation( def ), coupling( simulation, box ), timeStep( def.timeStep ), subSteps( def.subSteps )
    {
        ++LiveCount();
        physicsId = box.world;
    }
    ~Instance()
    {
        if ( physics )
            FixedStepScheduler::Detach( *physics, this );
        --LiveCount();
    }
};
struct Handle
{
    Instance* instance;
};
struct Entity
{
    Handle* owner;
    SoftBody::ObjectId id;
};
static void CheckStep( lua_State* L, Instance* s )
{
    if ( s && s->activeSteps )
        luaL_error( L, "attachment world is executing a native step" );
}
static Handle* WorldHandle( lua_State* L )
{
    return static_cast<Handle*>( luaL_checkudata( L, 1, kWorld ) );
}
static Instance* World( lua_State* L )
{
    auto* s = WorldHandle( L )->instance;
    CheckStep( L, s );
    if ( !s )
        luaL_error( L, "soft world was removed" );
    if ( s->invalidated )
        luaL_error( L, "coupled soft world was invalidated by physics.stop" );
    return s;
}
static Entity* Object( lua_State* L, const char* type, int index = 1 )
{
    auto* e = static_cast<Entity*>( luaL_checkudata( L, index, type ) );
    auto* s = e->owner->instance;
    CheckStep( L, s );
    if ( !s )
        luaL_error( L, "soft world was removed" );
    if ( s->invalidated )
        luaL_error( L, "coupled soft world was invalidated by physics.stop" );
    bool valid = type == kAttachment || type == kRigidJoint ? s->coupling.isValid( e->id ) : s->simulation.isValid( e->id );
    if ( !valid )
        luaL_error( L, "soft object was removed" );
    return e;
}
static void Field( lua_State* L, int table, const char* name )
{
    lua_pushstring( L, name );
    lua_rawget( L, table );
}
static double NumberArg( lua_State* L, int index, double low, double high )
{
    if ( lua_type( L, index ) != LUA_TNUMBER )
        luaL_error( L, "expected a number" );
    double value = lua_tonumber( L, index );
    if ( !std::isfinite( value ) || value < low || value > high )
        luaL_error( L, "number outside supported range" );
    return value;
}
static double Value( lua_State* L, int table, const char* name, double fallback, double low, double high )
{
    Field( L, table, name );
    double value = lua_isnil( L, -1 ) ? fallback : NumberArg( L, -1, low, high );
    lua_pop( L, 1 );
    return value;
}
// Public lengths use Solar2D logical pixels. Native softbody stays metres/y-up.
// Defaults and validation bounds below remain physical, independent of display scale.
static double LengthValue( lua_State* L, int table, const char* name, double fallback, double low, double high )
{
    const double scale = World( L )->scale;
    return Value( L, table, name, fallback * scale, low * scale, high * scale ) / scale;
}
static double LengthArg( lua_State* L, int index, double scale )
{
    return NumberArg( L, index, -1e4 * scale, 1e4 * scale ) / scale;
}
static double AngularSpeedArg( lua_State* L, int index )
{
    const double radiansPerDegree = 3.14159265358979323846 / 180;
    return -NumberArg( L, index, -1e4 / radiansPerDegree, 1e4 / radiansPerDegree ) * radiansPerDegree;
}
static void Number( lua_State* L, const char* key, double value )
{
    lua_pushnumber( L, value );
    lua_setfield( L, -2, key );
}
static void Bool( lua_State* L, const char* key, bool value )
{
    lua_pushboolean( L, value );
    lua_setfield( L, -2, key );
}
static void Check( lua_State* L, bool ok, const char* error )
{
    if ( !ok )
        luaL_error( L, "%s", error ? error : "invalid soft operation" );
}
// Lua owns temporary numeric buffers, so validation errors cannot strand C++
// allocations through luaL_error's longjmp. Keep userdata rooted on the stack.
struct WeightInput
{
    float* values;
    int count;
};
static WeightInput ReadWeights( lua_State* L, const char* name, SoftBody::ObjectId body )
{
    int count = World( L )->simulation.getBodyNodeCount( body );
    Check( L, count > 0, "body state unavailable" );
    Field( L, 2, name );
    luaL_checktype( L, -1, LUA_TTABLE );
    int table = lua_gettop( L );
    if ( lua_objlen( L, table ) != size_t( count ) )
        luaL_error( L, "%s requires one weight per body node (%d)", name, count );
    float* values = static_cast<float*>( lua_newuserdata( L, sizeof( float ) * count ) );
    for ( int i = 0; i < count; ++i )
    {
        lua_rawgeti( L, table, i + 1 );
        values[i] = NumberArg( L, -1, 0, 1 );
        lua_pop( L, 1 );
    }
    return { values, count };
}
static SoftBody::ObjectId BodyField( lua_State* L, const char* field )
{
    Field( L, 2, field );
    auto* e = Object( L, kBody, -1 );
    if ( e->owner != WorldHandle( L ) )
        luaL_error( L, "body belongs to another world" );
    auto id = e->id;
    lua_pop( L, 1 );
    return id;
}
static int Push( lua_State* L, const char* type, SoftBody::ObjectId id )
{
    auto* s = World( L );
    Check( L, id.index >= 0, type == kAttachment || type == kRigidJoint ? s->coupling.lastError() : s->simulation.lastError() );
    auto* e = new ( lua_newuserdata( L, sizeof( Entity ) ) ) Entity{};
    e->owner = WorldHandle( L );
    e->id = id;
    luaL_getmetatable( L, type );
    lua_setmetatable( L, -2 );
    lua_newtable( L );
    lua_pushvalue( L, 1 );
    lua_setfield( L, -2, "world" );
    lua_setfenv( L, -2 );
    return 1;
}
static int RemoveWorld( lua_State* L )
{
    auto* handle = WorldHandle( L );
    auto* s = handle->instance;
    handle->instance = nullptr;
    if ( s )
    {
        if ( s->physics )
            FixedStepScheduler::Detach( *s->physics, s );
        if ( s->activeSteps )
            s->removePending = true;
        else
            delete s;
    }
    return 0;
}
static int Remove( lua_State* L, const char* type )
{
    auto* e = static_cast<Entity*>( luaL_checkudata( L, 1, type ) );
    auto* s = e->owner->instance;
    CheckStep( L, s );
    if ( s )
    {
        if ( type == kAttachment )
            s->coupling.detach( e->id );
        else if ( type == kRigidJoint )
            s->coupling.removeRigidJoint( e->id );
        else
            s->simulation.remove( e->id );
    }
    return 0;
}
static int RemoveBody( lua_State* L )
{
    return Remove( L, kBody );
}
static int RemoveJoint( lua_State* L )
{
    return Remove( L, kJoint );
}
static int RemoveTouch( lua_State* L )
{
    return Remove( L, kTouch );
}
static int RemoveAttachment( lua_State* L )
{
    return Remove( L, kAttachment );
}
// Parse into Lua-owned scratch first; allocate native vectors only after all
// Lua argument validation has succeeded.
struct ShapeInput
{
    SoftBody::Point* nodes = nullptr;
    int* boundary = nullptr;
    int count = 0;
};
static ShapeInput ReadShape( lua_State* L )
{
    Field( L, 2, "shape" );
    if ( lua_isnil( L, -1 ) )
    {
        lua_pop( L, 1 );
        return {};
    }
    luaL_checktype( L, -1, LUA_TTABLE );
    int table = lua_gettop( L );
    lua_pushnil( L );
    while ( lua_next( L, table ) )
    {
        if ( lua_type( L, -2 ) != LUA_TSTRING ||
             ( std::strcmp( lua_tostring( L, -2 ), "nodes" ) && std::strcmp( lua_tostring( L, -2 ), "boundary" ) ) )
            luaL_error( L, "shape supports only nodes and one boundary; put internalSprings on the body definition" );
        lua_pop( L, 1 );
    }
    Field( L, table, "nodes" );
    luaL_checktype( L, -1, LUA_TTABLE );
    size_t size = lua_objlen( L, -1 );
    if ( size < 6 || size % 2 )
        luaL_error( L, "shape.nodes requires at least 3 x,y pairs in local pixels, y down" );
    if ( size / 2 > size_t( World( L )->simulation.availableNodes() ) )
        luaL_error( L, "soft node capacity reached (maxNodes)" );
    int count = int( size / 2 );
    int nodeTable = lua_gettop( L );
    auto* nodes = static_cast<SoftBody::Point*>( lua_newuserdata( L, sizeof( SoftBody::Point ) * count ) );
    for ( int i = 0; i < count; ++i )
        new ( nodes + i ) SoftBody::Point{};
    int* boundary = nullptr;
    for ( int i = 0; i < count * 2; ++i )
    {
        lua_rawgeti( L, nodeTable, i + 1 );
        const double scale = World( L )->scale;
        nodes[i / 2][i % 2] = NumberArg( L, -1, -100 * scale, 100 * scale ) / scale * ( i % 2 ? -1 : 1 );
        lua_pop( L, 1 );
    }
    Field( L, table, "boundary" );
    bool explicitBoundary = !lua_isnil( L, -1 );
    if ( explicitBoundary )
    {
        luaL_checktype( L, -1, LUA_TTABLE );
        if ( lua_objlen( L, -1 ) != size_t( count ) )
            luaL_error( L, "shape.boundary must use every node exactly once" );
        int boundaryTable = lua_gettop( L );
        boundary = static_cast<int*>( lua_newuserdata( L, sizeof( int ) * count ) );
        for ( int i = 0; i < count; ++i )
        {
            lua_rawgeti( L, boundaryTable, i + 1 );
            double value = NumberArg( L, -1, 1, count );
            if ( value != std::floor( value ) )
                luaL_error( L, "shape.boundary requires integer node indices" );
            boundary[i] = int( value ) - 1;
            lua_pop( L, 1 );
        }
    }
    ShapeInput result;
    result.nodes = nodes;
    result.boundary = boundary;
    result.count = count;
    return result;
}
struct SpringInput
{
    SoftBody::SoftSpringDef* values = nullptr;
    int count = 0;
};
static SpringInput ReadInternalSprings( lua_State* L, int nodeCount )
{
    Field( L, 2, "internalSprings" );
    if ( lua_isnil( L, -1 ) )
    {
        lua_pop( L, 1 );
        return {};
    }
    luaL_checktype( L, -1, LUA_TTABLE );
    size_t count = lua_objlen( L, -1 );
    if ( count > size_t( World( L )->simulation.availableInternalSprings() ) )
        luaL_error( L, "soft internal spring capacity reached (maxInternalSprings)" );
    int table = lua_gettop( L );
    auto* values = static_cast<SoftBody::SoftSpringDef*>( lua_newuserdata( L, sizeof( SoftBody::SoftSpringDef ) * count ) );
    for ( size_t i = 0; i < count; ++i )
    {
        new ( values + i ) SoftBody::SoftSpringDef{};
        lua_rawgeti( L, table, int( i + 1 ) );
        luaL_checktype( L, -1, LUA_TTABLE );
        int entry = lua_gettop( L );
        lua_pushnil( L );
        while ( lua_next( L, entry ) )
        {
            if ( lua_type( L, -2 ) != LUA_TSTRING ||
                 ( std::strcmp( lua_tostring( L, -2 ), "nodeA" ) && std::strcmp( lua_tostring( L, -2 ), "nodeB" ) &&
                   std::strcmp( lua_tostring( L, -2 ), "stiffness" ) && std::strcmp( lua_tostring( L, -2 ), "dampingRatio" ) ) )
                luaL_error( L, "internalSprings entries support nodeA, nodeB, stiffness and dampingRatio" );
            lua_pop( L, 1 );
        }
        Field( L, entry, "nodeA" );
        double a = NumberArg( L, -1, 1, nodeCount );
        lua_pop( L, 1 );
        Field( L, entry, "nodeB" );
        double b = NumberArg( L, -1, 1, nodeCount );
        lua_pop( L, 1 );
        if ( a != std::floor( a ) || b != std::floor( b ) )
            luaL_error( L, "internal spring node identities must be integers" );
        values[i].nodeA = int( a ) - 1;
        values[i].nodeB = int( b ) - 1;
        values[i].stiffness = Value( L, entry, "stiffness", 300, .001, 1e8 );
        values[i].dampingRatio = Value( L, entry, "dampingRatio", .3, 0, 10 );
        lua_pop( L, 1 );
    }
    SpringInput result;
    result.values = values;
    result.count = int( count );
    return result;
}
static SoftBody::SoftBodyDef::Pressure ReadPressure( lua_State* L )
{
    SoftBody::SoftBodyDef::Pressure result;
    Field( L, 2, "pressure" );
    if ( !lua_isnil( L, -1 ) )
    {
        luaL_checktype( L, -1, LUA_TTABLE );
        int table = lua_gettop( L );
        lua_pushnil( L );
        while ( lua_next( L, table ) )
        {
            if ( lua_type( L, -2 ) != LUA_TSTRING ||
                 ( std::strcmp( lua_tostring( L, -2 ), "target" ) && std::strcmp( lua_tostring( L, -2 ), "maxPressure" ) &&
                   std::strcmp( lua_tostring( L, -2 ), "rate" ) && std::strcmp( lua_tostring( L, -2 ), "maxAcceleration" ) ) )
                luaL_error( L, "pressure supports target, maxPressure, rate and maxAcceleration" );
            lua_pop( L, 1 );
        }
        result.enabled = true;
        result.maxPressure = Value( L, table, "maxPressure", 1000, .001, 1e6 );
        result.target = Value( L, table, "target", 0, 0, result.maxPressure );
        result.rate = Value( L, table, "rate", 500, .001, 1e6 );
        result.maxAcceleration = Value( L, table, "maxAcceleration", 200, .001, 1000 );
    }
    lua_pop( L, 1 );
    return result;
}
static SoftBody::SoftBodyDef::ReferenceScale ReadReferenceScale( lua_State* L )
{
    SoftBody::SoftBodyDef::ReferenceScale result;
    Field( L, 2, "referenceScale" );
    if ( !lua_isnil( L, -1 ) )
    {
        luaL_checktype( L, -1, LUA_TTABLE );
        int table = lua_gettop( L );
        lua_pushnil( L );
        while ( lua_next( L, table ) )
        {
            if ( lua_type( L, -2 ) != LUA_TSTRING ||
                 ( std::strcmp( lua_tostring( L, -2 ), "target" ) && std::strcmp( lua_tostring( L, -2 ), "min" ) &&
                   std::strcmp( lua_tostring( L, -2 ), "max" ) && std::strcmp( lua_tostring( L, -2 ), "rate" ) &&
                   std::strcmp( lua_tostring( L, -2 ), "maxSpeed" ) ) )
                luaL_error( L, "referenceScale supports target, min, max, rate and maxSpeed" );
            lua_pop( L, 1 );
        }
        result.enabled = true;
        result.min = Value( L, table, "min", .5, .25, 1 );
        result.max = Value( L, table, "max", 2, 1, 4 );
        result.target = Value( L, table, "target", 1, result.min, result.max );
        result.rate = Value( L, table, "rate", 1, .001, 10 );
        result.maxSpeed = LengthValue( L, table, "maxSpeed", 2, .001, 20 );
    }
    lua_pop( L, 1 );
    return result;
}
static int NewBody( lua_State* L )
{
    auto* s = World( L );
    luaL_checktype( L, 2, LUA_TTABLE );
    Field( L, 2, "material" );
    if ( std::strcmp( luaL_checkstring( L, -1 ), "recoverable" ) )
        return luaL_error( L, "assembly mode requires recoverable material" );
    lua_pop( L, 1 );
    SoftBody::ObjectId body;
    {
        SoftBody::SoftBodyDef def;
        def.mass = Value( L, 2, "mass", 1, .001, 1000 );
        def.halfWidth = LengthValue( L, 2, "halfWidth", .35, .01, 100 );
        def.halfHeight = LengthValue( L, 2, "halfHeight", .35, .01, 100 );
        def.position = { float( LengthValue( L, 2, "x", 0, -1e4, 1e4 ) ), float( -LengthValue( L, 2, "y", 0, -1e4, 1e4 ) ) };
        def.frequency = Value( L, 2, "frequency", 6, 0, 1000 );
        def.damping = Value( L, 2, "damping", 40, 0, 1e5 );
        def.materialScale = Value( L, 2, "materialScale", 1, .001, 1e4 );
        def.friction = Value( L, 2, "friction", 0, 0, 10 );
        Field( L, 2, "support" );
        if ( !lua_isnil( L, -1 ) )
        {
            luaL_checktype( L, -1, LUA_TSTRING );
            const char* support = lua_tostring( L, -1 );
            if ( !std::strcmp( support, "world" ) )
                def.support = SoftBody::BodySupport::World;
            else if ( std::strcmp( support, "free" ) )
                return luaL_error( L, "support must be 'free' or 'world'" );
        }
        lua_pop( L, 1 );
        if ( Value( L, 2, "preload", 0, 0, 1e6 ) != 0 )
            return luaL_error( L, "recoverable material requires zero preload" );
        def.pressure = ReadPressure( L );
        def.referenceScale = ReadReferenceScale( L );
        auto shape = ReadShape( L );
        auto springs = ReadInternalSprings( L, shape.count ? shape.count : 16 );
        // All Lua validation is finished. No longjmp may bypass owned vectors.
        if ( shape.count )
            def.shape.nodes.assign( shape.nodes, shape.nodes + shape.count );
        if ( shape.boundary )
            def.shape.boundary.assign( shape.boundary, shape.boundary + shape.count );
        if ( springs.count )
            def.internalSprings.assign( springs.values, springs.values + springs.count );
        body = s->simulation.createBody( def );
    }
    return Push( L, kBody, body );
}
static int NewJoint( lua_State* L )
{
    auto* s = World( L );
    luaL_checktype( L, 2, LUA_TTABLE );
    auto a = BodyField( L, "bodyA" ), b = BodyField( L, "bodyB" );
    auto wa = ReadWeights( L, "weightsA", a ), wb = ReadWeights( L, "weightsB", b );
    float frequency = Value( L, 2, "frequency", 12, .01, 60 );
    float damping = Value( L, 2, "dampingRatio", 1, 0, 10 );
    float gain = Value( L, 2, "motorGain", 8, 0, 1e6 );
    SoftBody::ObjectId id;
    {
        SoftBody::SoftJointDef def;
        def.bodyA = a;
        def.bodyB = b;
        def.weightsA.assign( wa.values, wa.values + wa.count );
        def.weightsB.assign( wb.values, wb.values + wb.count );
        def.frequency = frequency;
        def.dampingRatio = damping;
        def.motorGain = gain;
        id = s->simulation.createJoint( def );
    }
    return Push( L, kJoint, id );
}
static int NewWorldJoint( lua_State* L )
{
    auto* s = World( L );
    luaL_checktype( L, 2, LUA_TTABLE );
    auto body = BodyField( L, "body" );
    auto weights = ReadWeights( L, "weights", body );
    SoftBody::Point anchor{ float( LengthValue( L, 2, "x", 0, -1e4, 1e4 ) ),
                           float( -LengthValue( L, 2, "y", 0, -1e4, 1e4 ) ) };
    float frequency = Value( L, 2, "frequency", 12, .01, 60 );
    float damping = Value( L, 2, "dampingRatio", 1, 0, 10 );
    float gain = Value( L, 2, "motorGain", 8, 0, 1e6 );
    SoftBody::ObjectId id;
    {
        SoftBody::SoftWorldJointDef def;
        def.body = body;
        def.weights.assign( weights.values, weights.values + weights.count );
        def.anchor = anchor;
        def.frequency = frequency;
        def.dampingRatio = damping;
        def.motorGain = gain;
        id = s->simulation.createWorldJoint( def );
    }
    return Push( L, kJoint, id );
}
static int NewRigidJoint( lua_State* L )
{
    auto* s = World( L );
    luaL_checktype( L, 2, LUA_TTABLE );
    auto body = BodyField( L, "body" );
    Field( L, 2, "rigid" );
    auto* attachment = Object( L, kAttachment, -1 );
    if ( attachment->owner != WorldHandle( L ) )
        return luaL_error( L, "rigid attachment belongs to another world" );
    auto rigid = attachment->id;
    lua_pop( L, 1 );
    auto weights = ReadWeights( L, "weights", body );
    float x = LengthValue( L, 2, "localAnchorX", 0, -1e4, 1e4 );
    float y = -LengthValue( L, 2, "localAnchorY", 0, -1e4, 1e4 );
    float frequency = Value( L, 2, "frequency", 30, 1, 60 );
    float damping = Value( L, 2, "dampingRatio", 1, 0, 10 );
    float gain = Value( L, 2, "motorGain", 8, 0, 1e6 );
    SoftBody::ObjectId id;
    {
        SoftBody::SoftRigidJointDef def;
        def.body = body;
        def.rigid = rigid;
        def.weights.assign( weights.values, weights.values + weights.count );
        def.localAnchor = { x, y };
        def.frequency = frequency;
        def.dampingRatio = damping;
        def.motorGain = gain;
        id = s->coupling.createRigidJoint( def );
    }
    return Push( L, kRigidJoint, id );
}
static int RemoveRigidJoint( lua_State* L )
{
    return Remove( L, kRigidJoint );
}
static int RigidJointState( lua_State* L )
{
    auto* e = Object( L, kRigidJoint );
    SoftBody::SoftRigidJointState state;
    Check( L, e->owner->instance->coupling.getRigidJointState( e->id, state ), "rigid pin state unavailable" );
    lua_createtable( L, 0, 7 );
    Bool( L, "enabled", state.enabled );
    Number( L, "errorDistance", state.errorM * e->owner->instance->scale );
    Number( L, "impulseX", state.impulse[0] );
    Number( L, "impulseY", -state.impulse[1] );
    Number( L, "relativeSpeed", -state.relativeSpeed * ( 180 / 3.14159265358979323846 ) );
    Number( L, "motorImpulse", -state.motorImpulse );
    Number( L, "motorWork", state.motorWork );
    return 1;
}
static int SetRigidMotor( lua_State* L )
{
    auto* e = Object( L, kRigidJoint );
    auto& api = e->owner->instance->coupling;
    bool ok = api.setRigidMotor( e->id, AngularSpeedArg( L, 2 ), NumberArg( L, 3, 0, 1e6 ) );
    Check( L, ok, api.lastError() );
    return 0;
}
static int NewTouch( lua_State* L )
{
    auto* s = World( L );
    luaL_checktype( L, 2, LUA_TTABLE );
    auto body = BodyField( L, "body" );
    auto weights = ReadWeights( L, "weights", body );
    SoftBody::Point target{ float( LengthValue( L, 2, "targetX", 0, -1e4, 1e4 ) ),
                            float( -LengthValue( L, 2, "targetY", 0, -1e4, 1e4 ) ) };
    float frequency = Value( L, 2, "frequency", 3, .01, 60 );
    float damping = Value( L, 2, "dampingRatio", 1, 0, 10 );
    float force = Value( L, 2, "maxForce", 50, 0, 1e5 );
    SoftBody::ObjectId id;
    {
        SoftBody::SoftTouchDef def;
        def.body = body;
        def.weights.assign( weights.values, weights.values + weights.count );
        def.target = target;
        def.frequency = frequency;
        def.dampingRatio = damping;
        def.maxForce = force;
        id = s->simulation.createTouch( def );
    }
    return Push( L, kTouch, id );
}
static int SetMotor( lua_State* L )
{
    auto* e = Object( L, kJoint );
    auto& api = e->owner->instance->simulation;
    bool ok = api.setMotor( e->id, AngularSpeedArg( L, 2 ), NumberArg( L, 3, 0, 1e6 ) );
    Check( L, ok, api.lastError() );
    return 0;
}
static int TouchTarget( lua_State* L )
{
    auto* e = Object( L, kTouch );
    auto& api = e->owner->instance->simulation;
    bool ok =
        api.setTouchTarget( e->id, { float( LengthArg( L, 2, e->owner->instance->scale ) ),
                                   -float( LengthArg( L, 3, e->owner->instance->scale ) ) } );
    Check( L, ok, api.lastError() );
    return 0;
}
static int TouchState( lua_State* L )
{
    auto* e = Object( L, kTouch );
    SoftBody::SoftTouchState state;
    Check( L, e->owner->instance->simulation.getTouchState( e->id, state ), "touch state unavailable" );
    lua_createtable( L, 0, 5 );
    Number( L, "x", state.position[0] * e->owner->instance->scale );
    Number( L, "y", -state.position[1] * e->owner->instance->scale );
    Number( L, "targetX", state.target[0] * e->owner->instance->scale );
    Number( L, "targetY", -state.target[1] * e->owner->instance->scale );
    Number( L, "force", state.force );
    return 1;
}
static int ApplyLinearImpulse( lua_State* L )
{
    auto* e = Object( L, kBody );
    if ( lua_gettop( L ) != 3 )
        return luaL_error( L, "applyLinearImpulse expects two impulse components in kg*m/s, y down" );
    auto& api = e->owner->instance->simulation;
    bool ok = api.applyLinearImpulse( e->id,
        { float( NumberArg( L, 2, -1e4, 1e4 ) ), -float( NumberArg( L, 3, -1e4, 1e4 ) ) } );
    Check( L, ok, api.lastError() );
    return 0;
}
static int PressureTarget( lua_State* L )
{
    auto* e = Object( L, kBody );
    if ( lua_gettop( L ) != 2 )
        return luaL_error( L, "setPressureTarget expects nominal pressure in N/m" );
    auto& api = e->owner->instance->simulation;
    bool ok = api.setPressureTarget( e->id, float( NumberArg( L, 2, 0, 1e6 ) ) );
    Check( L, ok, api.lastError() );
    return 0;
}
static int ReferenceScaleTarget( lua_State* L )
{
    auto* e = Object( L, kBody );
    if ( lua_gettop( L ) != 2 )
        return luaL_error( L, "setReferenceScaleTarget expects one uniform scale factor" );
    auto& api = e->owner->instance->simulation;
    bool ok = api.setReferenceScaleTarget( e->id, float( NumberArg( L, 2, .25, 4 ) ) );
    Check( L, ok, api.lastError() );
    return 0;
}
static int BodyState( lua_State* L )
{
    auto* e = Object( L, kBody );
    SoftBody::SoftBodyState state;
    Check( L, e->owner->instance->simulation.getBodyState( e->id, state ), "body state unavailable" );
    lua_createtable( L, 0, 5 );
    Number( L, "id", e->id.index + 1 );
    lua_pushstring( L, state.support == SoftBody::BodySupport::World ? "world" : "free" );
    lua_setfield( L, -2, "support" );
    Number( L, "referenceX", state.referencePosition[0] * e->owner->instance->scale );
    Number( L, "referenceY", -state.referencePosition[1] * e->owner->instance->scale );
    Number( L, "nodeCount", state.nodeCount );
    Number( L, "internalSpringScale", state.internalSpringScale );
    Number( L, "internalSpringWork", state.internalSpringWork );
    Number( L, "internalSpringRecoverySteps", state.internalSpringRecoverySteps );
    Number( L, "pressureTarget", state.pressureTarget );
    Number( L, "pressureLevel", state.pressureLevel );
    Number( L, "pressureApplied", state.pressureApplied );
    Number( L, "pressureScale", state.pressureScale );
    Number( L, "pressureWork", state.pressureWork );
    Number( L, "pressureRecoverySteps", state.pressureRecoverySteps );
    Number( L, "pressureLimitedSteps", state.pressureLimitedSteps );
    Number( L, "referenceScale", state.referenceScale );
    Number( L, "referenceScaleTarget", state.referenceScaleTarget );
    Number( L, "referenceScaleWork", state.referenceScaleWork );
    Number( L, "referenceScaleLimitedSteps", state.referenceScaleLimitedSteps );
    lua_createtable( L, state.nodeCount, 0 );
    for ( int i = 0; i < state.nodeCount; ++i )
    {
        lua_pushinteger( L, state.boundary[i] + 1 );
        lua_rawseti( L, -2, i + 1 );
    }
    lua_setfield( L, -2, "boundary" );
    lua_createtable( L, state.nodeCount * 2, 0 );
    int n = 0;
    for ( int i = 0; i < state.nodeCount; ++i )
        for ( int axis = 0; axis < 2; ++axis )
        {
            lua_pushnumber( L, state.vertices[i][axis] * double( e->owner->instance->scale ) * ( axis ? -1 : 1 ) );
            lua_rawseti( L, -2, ++n );
        }
    lua_setfield( L, -2, "vertices" );
    return 1;
}
static int SetCollision( lua_State* L )
{
    auto* s = World( L );
    auto* a = Object( L, kBody, 2 );
    auto* b = Object( L, kBody, 3 );
    luaL_checktype( L, 4, LUA_TBOOLEAN );
    bool ok = s->simulation.setCollisionEnabled( a->id, b->id, lua_toboolean( L, 4 ) );
    Check( L, ok, s->simulation.lastError() );
    return 0;
}
static int AttachBody( lua_State* L )
{
    auto* s = World( L );
    auto* object = static_cast<DisplayObject*>( LuaProxy::GetProxyableObject( L, 2 ) );
    if ( !object || !object->GetExtensions() )
        return luaL_error( L, "expected a physics display object" );
    return Push( L, kAttachment, s->coupling.attachBody( object->GetExtensions()->GetBody() ) );
}
static int AttachmentCollision( lua_State* L )
{
    auto* a = Object( L, kAttachment );
    auto* b = Object( L, kBody, 2 );
    luaL_checktype( L, 3, LUA_TBOOLEAN );
    auto& api = a->owner->instance->coupling;
    bool ok = api.setCollisionEnabled( a->id, b->id, lua_toboolean( L, 3 ) );
    Check( L, ok, api.lastError() );
    return 0;
}
static int RigidFilter( lua_State* L )
{
    auto* e = Object( L, kBody );
    luaL_checktype( L, 2, LUA_TTABLE );
    double category = Value( L, 2, "categoryBits", 1, 0, 65535 ), mask = Value( L, 2, "maskBits", 65535, 0, 65535 );
    double group = Value( L, 2, "groupIndex", 0, -32768, 32767 );
    if ( category != std::floor( category ) || mask != std::floor( mask ) || group != std::floor( group ) )
        return luaL_error( L, "filter fields must be integers" );
    SoftBody::RigidFilter filter;
    filter.categoryBits = std::uint64_t( category );
    filter.maskBits = std::uint64_t( mask );
    filter.groupIndex = int( group );
    auto& api = e->owner->instance->coupling;
    bool ok = api.setFilter( e->id, filter );
    Check( L, ok, api.lastError() );
    return 0;
}
static int State( lua_State* L )
{
    auto* s = WorldHandle( L )->instance;
    CheckStep( L, s );
    if ( !s )
        return luaL_error( L, "soft world was removed" );
    auto state = s->simulation.getState();
    auto coupled = s->coupling.getState();
    lua_createtable( L, 0, 32 );
    Bool( L, "coupled", true );
    Bool( L, "invalidated", s->invalidated );
    Bool( L, "gameMode", false );
    Bool( L, "assemblyMode", true );
    Bool( L, "wholeStep", true );
    Number( L, "rigidSteps", coupled.entered );
    Number( L, "softSteps", state.completedSteps );
    Number( L, "substep", state.completedSteps );
    Number( L, "time", coupled.completed * double( s->timeStep ) );
    Number( L, "worldSteps", coupled.completed );
    Number( L, "timeStep", s->timeStep );
    Number( L, "subSteps", s->subSteps );
    Number( L, "subStepTime", s->timeStep / s->subSteps );
    Number( L, "liveInstances", LiveCount() );
    Number( L, "pixelsPerMeter", s->scale );
    lua_pushliteral( L, "pixels" ); lua_setfield( L, -2, "coordinateUnits" );
    Bool( L, "recovering", coupled.recovering );
    Number( L, "recoveryEntries", coupled.recoveryEntries );
    Number( L, "recoveryExits", coupled.recoveryExits );
    Number( L, "recoverySteps", coupled.recoverySteps );
    Number( L, "contactDepth", coupled.contactDepth * s->scale );
    Number( L, "warmHits", coupled.warmHits );
    Number( L, "boxContacts", coupled.contactRows );
    Number( L, "chainCandidates", coupled.chainCandidates );
    Number( L, "chainQueries", coupled.chainQueries );
    Number( L, "softContactRows", coupled.softContactRows );
    Number( L, "softContactDepth", coupled.softContactDepth * s->scale );
    Number( L, "softRecoverySteps", coupled.softRecoverySteps );
    Number( L, "softRecoveryEntries", coupled.softRecoveryEntries );
    Number( L, "softRecoveryExits", coupled.softRecoveryExits );
    Number( L, "softRecoveringPairs", coupled.softRecoveringPairs );
    Number( L, "softRecoveryDistance", coupled.softRecoveryDistance * s->scale );
    Number( L, "assemblyPinError", coupled.peakGap * s->scale );
    Number( L, "motorWork", state.motorWork );
    Number( L, "appliedWork", state.appliedWork );
    Number( L, "boundaryWork", 0 );
    Number( L, "energy", state.energy );
    Number( L, "penetration", state.penetration * s->scale );
    Number( L, "pinError", state.pinError * s->scale );
    Number( L, "nodeSpeed", state.nodeSpeed * s->scale );
    Number( L, "centerSpeed", state.centerSpeed * s->scale );
    Number( L, "minAreaRatio", state.minAreaRatio );
    Bool( L, "selfCross", state.selfCross );
    Number( L, "strictEvents", 0 );
    Number( L, "staticPenetration", 0 );
    Number( L, "staticQueries", 0 );
    Number( L, "staticCandidates", 0 );
    Number( L, "maxUnforcedEnergy", state.completedSteps ? 0 : state.energy );
    lua_newtable( L );
    lua_setfield( L, -2, "minimumY" );
    if ( state.failure )
    {
        lua_pushstring( L, state.failure );
        lua_setfield( L, -2, "failure" );
    }
    lua_createtable( L, int( state.vertices.size() ), 0 );
    int n = 0;
    for ( float value : state.vertices )
    {
        lua_pushnumber( L, value * double( s->scale ) * ( n % 2 ? -1 : 1 ) );
        lua_rawseti( L, -2, ++n );
    }
    lua_setfield( L, -2, "vertices" );
    return 1;
}
static int ProfileState( lua_State* L )
{
    auto profile = World( L )->coupling.getProfile();
    lua_createtable( L, 0, SoftBody::CouplingProfile::Count + 8 );
    Number( L, "schemaVersion", 2 );
    Number( L, "projectionCandidates", profile.projectionCandidates );
    Number( L, "projectionCorrections", profile.projectionCorrections );
    Number( L, "gatherCandidates", profile.gatherCandidates );
    Number( L, "cacheComparisons", profile.cacheComparisons );
    Number( L, "rows", profile.rows );
    Bool( L, "enabled", profile.enabled );
    Bool( L, "clockFailed", profile.clockFailed );
    const char* names[] = { "prepare", "materialMotor", "initialLinks", "contactGeometry", "contactSolve",
                            "observe", "finalize",      "nativeWorld",  "advance",         "adapter",
                            "geometryProject", "geometryGather", "geometryCache", "geometryRecovery", "geometrySides" };
    static_assert( sizeof( names ) / sizeof( names[0] ) == SoftBody::CouplingProfile::Count, "profile names" );
    for ( int i = 0; i < SoftBody::CouplingProfile::Count; ++i )
    {
        const auto& sample = profile.samples[i];
        lua_createtable( L, 0, 3 );
        Number( L, "wallMs", sample.wallMs );
        Number( L, "cpuMs", sample.cpuMs );
        Number( L, "calls", sample.calls );
        lua_setfield( L, -2, names[i] );
    }
    return 1;
}
static int Unsupported( lua_State* L )
{
    World( L );
    return luaL_error( L, "unsupported operation in assembly mode" );
}
static int UnsupportedLoad( lua_State* L )
{
    Object( L, kBody );
    return luaL_error( L, "game mode supports touch loads only" );
}
static int ManualStep( lua_State* L )
{
    World( L );
    return luaL_error( L, "coupled soft world is advanced by physics; manual step is forbidden" );
}
static const char* Validate( Instance& s, PhysicsWorld& physics )
{
    if ( s.invalidated || !physics.IsWorldValid() ||
         b2StoreWorldId( s.physicsId ) != b2StoreWorldId( physics.GetWorldId() ) )
        return "coupled physics world is no longer valid";
    if ( s.scale != physics.GetPixelsPerMeter() )
        return "physics scale changed; rebuild coupled world";
    if ( physics.GetWorld()->GetParticleSystemList() )
        return "coupled soft world does not support LiquidFun in the same world";
    return nullptr;
}
static const char* Prepare( void* owner, PhysicsWorld& physics )
{
    auto& s = *static_cast<Instance*>( owner );
    if ( const char* error = Validate( s, physics ) )
        return error;
    return s.coupling.prepare() ? nullptr : s.coupling.lastError();
}
static const char* Advance( void* owner, PhysicsWorld& physics, bool& entered )
{
    auto& s = *static_cast<Instance*>( owner );
    entered = false;
    if ( const char* error = Validate( s, physics ) )
        return error;
    bool success = s.coupling.advance( &entered );
    return success ? nullptr : s.coupling.lastError();
}
static void Invalidate( void* owner )
{
    auto* s = static_cast<Instance*>( owner );
    s->physics = nullptr;
    s->invalidated = true;
}
static void Retain( void* owner )
{
    ++static_cast<Instance*>( owner )->activeSteps;
}
static void Release( void* owner )
{
    auto* s = static_cast<Instance*>( owner );
    if ( --s->activeSteps == 0 && s->removePending )
        delete s;
}
static int New( lua_State* L )
{
    luaL_checktype( L, 1, LUA_TTABLE );
    Field( L, 1, "couplingMode" );
    bool assembly = lua_type( L, -1 ) == LUA_TSTRING && strcmp( lua_tostring( L, -1 ), "assembly" ) == 0;
    lua_pop( L, 1 );
    if ( !assembly )
        return luaL_error( L, "only couplingMode='assembly' is supported; historical soft modes were removed" );
    Field( L, 1, "coupled" );
    bool coupled = lua_isboolean( L, -1 ) && lua_toboolean( L, -1 );
    lua_pop( L, 1 );
    if ( !coupled )
        return luaL_error( L, "assembly mode requires coupled=true" );
    Field( L, 1, "profile" );
    if ( !lua_isnil( L, -1 ) && !lua_isboolean( L, -1 ) )
        return luaL_error( L, "profile must be boolean" );
    bool profile = lua_toboolean( L, -1 );
    lua_pop( L, 1 );
    Field( L, 1, "enableDynamicContacts" );
    if ( !lua_isnil( L, -1 ) && !lua_isboolean( L, -1 ) )
        return luaL_error( L, "enableDynamicContacts must be boolean" );
    bool dynamicContacts = lua_toboolean( L, -1 );
    lua_pop( L, 1 );
    if ( profile && !SoftBody::Box2DCoupling::profilingSupported() )
        return luaL_error( L, "profiling requires thread CPU clock support" );
    SoftBody::SimulationDef def;
    Field( L, 1, "enableSoftContacts" );
    if ( !lua_isnil( L, -1 ) && !lua_isboolean( L, -1 ) )
        return luaL_error( L, "enableSoftContacts must be boolean" );
    def.enableSoftContacts = lua_toboolean( L, -1 );
    lua_pop( L, 1 );
    auto& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
    if ( !physics.IsWorldValid() )
        return luaL_error( L, "coupled soft world requires physics.start" );
    if ( !FixedStepScheduler::GetTiming( physics, def.timeStep, def.subSteps ) )
        return luaL_error( L, "configure physics.setFixedStepMode before creating a soft world" );
    // The physics scheduler is the sole clock. Optional historical fields may
    // assert its configuration, but must never select an independent soft clock.
    double dt = Value( L, 1, "timeStep", def.timeStep, 0, 1 );
    double subs = Value( L, 1, "substeps", def.subSteps, 1, 128 );
    double iterations = Value( L, 1, "iterations", 8, 1, 32 );
    if ( float( dt ) != def.timeStep || subs != def.subSteps )
        return luaL_error( L, "configure timeStep and subSteps with physics.setFixedStepMode" );
    if ( iterations != 8 )
        return luaL_error( L, "soft constraint iterations must be 8" );
    double bodies = Value( L, 1, "maxBodies", 32, 1, 32 ), joints = Value( L, 1, "maxJoints", 64, 0, 64 ),
           touches = Value( L, 1, "maxTouches", 1, 0, 64 );
    if ( bodies != std::floor( bodies ) || joints != std::floor( joints ) || touches != std::floor( touches ) )
        return luaL_error( L, "soft capacities must be integers" );
    double nodes = Value( L, 1, "maxNodes", 512, 3, std::numeric_limits<int>::max() / 4 - 4 );
    double rows = Value( L, 1, "maxSoftContactRows", 512, 1, std::numeric_limits<int>::max() );
    double springs = Value( L, 1, "maxInternalSprings", 2048, 0, std::numeric_limits<int>::max() / 64 );
    if ( nodes != std::floor( nodes ) || rows != std::floor( rows ) || springs != std::floor( springs ) )
        return luaL_error( L, "soft node, contact and internal spring capacities must be integers" );
    def.maxNodes = int( nodes );
    def.maxInternalSprings = int( springs );
    def.maxSoftContactRows = int( rows );
    def.maxBodies = int( bodies );
    def.maxJoints = int( joints );
    def.maxTouches = int( touches );
    SoftBody::Box2DCouplingDef box;
    box.world = physics.GetWorldId();
    box.flipY = true;
    box.profile = profile;
    box.enableDynamicContacts = dynamicContacts;
    double rigidJoints = Value( L, 1, "maxRigidJoints", 64, 0, 64 );
    if ( rigidJoints != std::floor( rigidJoints ) )
        return luaL_error( L, "maxRigidJoints must be an integer" );
    box.maxRigidJoints = int( rigidJoints );
    double chainSegments = Value( L, 1, "maxChainSegments", 256, 1, 4096 );
    if ( chainSegments != std::floor( chainSegments ) )
        return luaL_error( L, "maxChainSegments must be an integer" );
    box.maxChainSegments = int( chainSegments );
    auto* h = static_cast<Handle*>( lua_newuserdata( L, sizeof( Handle ) ) );
    h->instance = nullptr;
    luaL_getmetatable( L, kWorld );
    lua_setmetatable( L, -2 );
    h->instance = new ( std::nothrow ) Instance( def, box );
    if ( !h->instance )
        return luaL_error( L, "cannot allocate soft simulation" );
    auto& s = *h->instance;
    s.scale = physics.GetPixelsPerMeter();
    if ( !s.coupling.ready() )
    {
        const char* error = s.coupling.lastError();
        delete h->instance;
        h->instance = nullptr;
        return luaL_error( L, "%s", error );
    }
    FixedStepScheduler::Client client = { &s, Prepare, nullptr, Invalidate, Advance, Retain, Release };
    if ( const char* error = FixedStepScheduler::Attach( physics, client, def.timeStep, def.subSteps ) )
    {
        delete h->instance;
        h->instance = nullptr;
        return luaL_error( L, "%s", error );
    }
    s.physics = &physics;
    return 1;
}
static void Register( lua_State* L )
{
    const luaL_Reg world[] = { { "createBody", NewBody },
                               { "createPinJoint", NewJoint },
                               { "createTouchJoint", NewTouch },
                               { "attachPhysicsBody", AttachBody },
                               { "attachStaticBody", Unsupported },
                               { "setCollisionEnabled", SetCollision },
                               { "createWorldPinJoint", NewWorldJoint },
                               { "createRigidPinJoint", NewRigidJoint },
                               { "createRigidAttachment", Unsupported },
                               { "createBoundary", Unsupported },
                               { "step", ManualStep },
                               { "getState", State },
                               { "getProfile", ProfileState },
                               { "removeSelf", RemoveWorld },
                               { "__gc", RemoveWorld },
                               { NULL, NULL } };
    const luaL_Reg body[] = { { "getState", BodyState },         { "setRigidFilter", RigidFilter },
                              { "applyLinearImpulse", ApplyLinearImpulse },
                              { "setPressureTarget", PressureTarget },
                              { "setReferenceScaleTarget", ReferenceScaleTarget },
                              { "applyForce", UnsupportedLoad }, { "applyTorque", UnsupportedLoad },
                              { "removeSelf", RemoveBody },      { NULL, NULL } };
    const luaL_Reg joint[] = { { "setMotor", SetMotor }, { "removeSelf", RemoveJoint }, { NULL, NULL } };
    const luaL_Reg rigidJoint[] = { { "setMotor", SetRigidMotor }, { "getState", RigidJointState },
                                  { "removeSelf", RemoveRigidJoint }, { NULL, NULL } };
    const luaL_Reg touch[] = {
        { "setTarget", TouchTarget }, { "getState", TouchState }, { "removeSelf", RemoveTouch }, { NULL, NULL } };
    const luaL_Reg attachment[] = {
        { "setCollisionEnabled", AttachmentCollision }, { "removeSelf", RemoveAttachment }, { NULL, NULL } };
    const char* names[] = { kWorld, kBody, kJoint, kRigidJoint, kTouch, kAttachment };
    const luaL_Reg* methods[] = { world, body, joint, rigidJoint, touch, attachment };
    for ( int i = 0; i < 6; ++i )
    {
        luaL_newmetatable( L, names[i] );
        luaL_register( L, nullptr, methods[i] );
        lua_pushvalue( L, -1 );
        lua_setfield( L, -2, "__index" );
        lua_pop( L, 1 );
    }
    lua_pushcfunction( L, New );
    lua_setfield( L, -2, "newExperimentalSoftWorld" );
}
}
}
