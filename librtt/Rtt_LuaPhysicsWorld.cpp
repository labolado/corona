//////////////////////////////////////////////////////////////////////////////
// Independent Box2D worlds and optional one-way display interpolation.
// This file is part of the Solar2D engine. See the repository LICENSE.
//////////////////////////////////////////////////////////////////////////////
#include "Core/Rtt_Build.h"
#ifdef Rtt_PHYSICS
#include "Rtt_LuaPhysicsWorld.h"
#include "Rtt_LuaContext.h"
#include "Rtt_Runtime.h"
#include "Rtt_PhysicsWorld.h"
#include "Rtt_PhysicsCompoundEdges.h"
#include "Rtt_PhysicsCapsule.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_LuaProxyVTable.h"
#include "Display/Rtt_DisplayObject.h"
#include "SoftBody/Rtt_LuaSoftWorldBridge.h"
#include "box2d/box2d.h"
#include "box2d/constants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <initializer_list>
#include <string>
#include <vector>

namespace Rtt
{
namespace
{
const char *kWorld = "Solar2D.physicsWorld";
const char *kBody = "Solar2D.physicsWorld.body";
const char *kJoint = "Solar2D.physicsWorld.joint";
const char *kShape = "Solar2D.physicsWorld.shape";
const char *kChain = "Solar2D.physicsWorld.chain";
const char *kBinding = "_physicsWorldBinding";
const double kRad = 0.017453292519943295;
const size_t kInactive = (size_t)-1;
// Contact policies may inspect independent worlds, but cannot mutate any of them.
thread_local bool sContactPolicyActive = false;
enum class Interpolation
{
    Auto,
    On,
    Off
};
struct Pose
{
    float x, y, angle;
};
struct Binding
{
    LuaProxy *proxy = NULL; // The proxy userdata is retained even after removeSelf().
    Pose previous, current;
};
struct Body
{
    b2BodyId id = b2_nullBodyId;
    size_t serial = 0;
    size_t activeIndex = kInactive;
    unsigned int fixtureCount = 0;
    std::vector<size_t> shapes, chains;
    std::unique_ptr<Binding> binding; // No interpolation state for unbound bodies.
};
struct Shape
{
    b2ShapeId id = b2_nullShapeId;
    size_t serial = 0, body = 0, chain = kInactive;
    unsigned int element = 0;
    int segmentIndex = 0;
};
struct Chain
{
    b2ChainId id = b2_nullChainId;
    size_t serial = 0, body = 0;
    unsigned int element = 0;
    std::vector<size_t> segments;
};
struct CompoundEdge
{
    b2BodyId bodyId;
    b2ShapeId shapeId;
    b2Vec2 point1, point2, normal;
};
struct World
{
    b2WorldId id = b2_nullWorldId;
    b2BodyId touchGround = b2_nullBodyId; // Private, shapeless anchor shared by touch joints.
    bool alive = true, busy = false, syncing = false, paused = false, faulted = false;
    unsigned long long index = 0, interruption = 0;
    float scale = 30, dt = 1.0f / 60;
    int subSteps = 8, cap = 8;
    bool compoundInternalEdgeSuppression = false;
    std::vector<CompoundEdge> compoundEdges;
    double speed = 1, budget = 0, renderTime = 0;
    Interpolation interpolation = Interpolation::Auto;
    bool interpolating = false;
    double frameInterval = 1.0 / 60, viewAlpha = 1;
    size_t poseHistoryCopies = 0;
    size_t lastViewUpdates = 0, bindingCount = 0;
    std::string error;
    std::vector<std::unique_ptr<Body>> bodies;
    std::vector<b2JointId> joints;
    std::vector<std::unique_ptr<Shape>> shapes;
    std::vector<std::unique_ptr<Chain>> chains;
    std::vector<size_t> freeBodies, freeJoints;
    std::vector<size_t> freeShapes, freeChains;
    std::vector<Body *> active;
    SoftSimulationLua::IndependentClient soft{};
    bool hasContactPolicy = false;
    lua_State *policyL = NULL; // Borrowed only for the duration of OneStep.
    int policyWorldIndex = 0;
    size_t policyCalls = 0;
};
// Independent worlds own these flags; compound suppression precedes Lua policy.
// Geometry changes rebuild only this body's entries, outside the locked step.
void RefreshCompoundEdges( World &w, b2BodyId bodyId, bool rebuild = true )
{
    if ( !w.compoundInternalEdgeSuppression )
        return;
    auto &edges = w.compoundEdges;
    edges.erase( std::remove_if( edges.begin(), edges.end(), [&]( const CompoundEdge &edge ) {
                     return B2_ID_EQUALS( edge.bodyId, bodyId );
                 } ), edges.end() );
    if ( !rebuild )
        return;
    std::vector<b2ShapeId> shapes( b2Body_GetShapeCount( bodyId ) );
    b2Body_GetShapes( bodyId, shapes.data(), (int)shapes.size() );
    for ( b2ShapeId shape : shapes )
        b2Shape_EnablePreSolveEvents( shape, false );
    PhysicsCompoundEdges::Build( bodyId, edges, []( b2ShapeId shape ) {
        b2Shape_EnablePreSolveEvents( shape, true );
    } );
}
bool CompoundPreContinuous( b2ShapeId a, b2ShapeId b, b2Vec2 point, b2Vec2 normal, void *context )
{
    return !PhysicsCompoundEdges::Suppress( static_cast<World *>( context )->compoundEdges, a, b,
                                           point, normal );
}
void CompoundPreSolve( b2ShapeId a, b2ShapeId b, b2Manifold *manifold, void *context )
{
    if ( manifold->pointCount == 0 )
        return;
    int deepest = 0;
    for ( int i = 1; i < manifold->pointCount; ++i )
        if ( manifold->points[i].separation < manifold->points[deepest].separation )
            deepest = i;
    b2Vec2 origin = b2Body_GetPosition( b2Shape_GetBody( a ) );
    if ( !CompoundPreContinuous( a, b, b2Add( origin, manifold->points[deepest].anchorA ),
                                manifold->normal, context ) )
        manifold->pointCount = 0;
}
struct Handle
{
    World *world;
    size_t index;
    b2BodyId bodyId;
    b2JointId jointId;
};
struct ShapeHandle
{
    World *world;
    size_t index;
    b2ShapeId id;
};
struct ChainHandle
{
    World *world;
    size_t index;
    b2ChainId id;
};
double Number( lua_State *L, int i )
{
    double v = luaL_checknumber( L, i );
    if ( !std::isfinite( v ) || std::abs( v ) > 1e20 )
        luaL_error( L, "finite number within +/-1e20 required" );
    return v;
}
// The initial API intentionally has a small capability set. Reject unknown
// definition fields so a missing feature cannot silently change a business scene.
void Options( lua_State *L, int table, std::initializer_list<const char *> allowed )
{
    lua_pushnil( L );
    while ( lua_next( L, table ) )
    {
        if ( lua_type( L, -2 ) != LUA_TSTRING )
            luaL_error( L, "option keys must be strings" );
        const char *key = lua_tostring( L, -2 );
        bool known = false;
        for ( const char *name : allowed )
            known = known || strcmp( key, name ) == 0;
        if ( !known )
            luaL_error( L, "unsupported physics option '%s'", key );
        lua_pop( L, 1 );
    }
}
double Option( lua_State *L, int table, const char *key, double value )
{
    lua_getfield( L, table, key );
    if ( !lua_isnil( L, -1 ) )
        value = Number( L, -1 );
    lua_pop( L, 1 );
    return value;
}
bool Flag( lua_State *L, int table, const char *key, bool value )
{
    lua_getfield( L, table, key );
    if ( !lua_isnil( L, -1 ) )
    {
        luaL_checktype( L, -1, LUA_TBOOLEAN );
        value = lua_toboolean( L, -1 ) != 0;
    }
    lua_pop( L, 1 );
    return value;
}
void Field( lua_State *L, const char *key, double v )
{
    lua_pushnumber( L, v );
    lua_setfield( L, -2, key );
}
void Bool( lua_State *L, const char *key, bool v )
{
    lua_pushboolean( L, v );
    lua_setfield( L, -2, key );
}
World &GetWorld( lua_State *L, int i = 1, bool alive = true )
{
    World *w = *(World **)luaL_checkudata( L, i, kWorld );
    if ( !w || ( alive && !w->alive ) )
        luaL_error( L, "physics world is destroyed" );
    return *w;
}
void Mutable( lua_State *L, World &w )
{
    if ( sContactPolicyActive )
        luaL_error( L, "contact policy cannot mutate physics worlds" );
    if ( !w.alive || w.syncing || b2World_IsLocked( w.id ) )
        luaL_error( L, "physics world is destroyed, locked, or synchronizing views" );
}
Handle &GetHandle( lua_State *L, int i, const char *kind )
{
    Handle *h = (Handle *)luaL_checkudata( L, i, kind );
    if ( !h->world->alive )
        luaL_error( L, "physics world is destroyed" );
    return *h;
}
Body &GetBody( lua_State *L, int i = 1 )
{
    Handle &h = GetHandle( L, i, kBody );
    Body &b = *h.world->bodies[h.index];
    if ( !B2_ID_EQUALS( b.id, h.bodyId ) || !b2Body_IsValid( b.id ) )
        luaL_error( L, "physics body is destroyed" );
    return b;
}
b2JointId GetJoint( lua_State *L, int i = 1 )
{
    Handle &h = GetHandle( L, i, kJoint );
    b2JointId j = h.world->joints[h.index];
    if ( !B2_ID_EQUALS( j, h.jointId ) || !b2Joint_IsValid( j ) )
        luaL_error( L, "physics joint is destroyed" );
    return j;
}
World &Owner( lua_State *L, int i, const char *kind )
{
    return *GetHandle( L, i, kind ).world;
}
void PushWorld( lua_State *L, int handle )
{
    lua_getfenv( L, handle );
    lua_getfield( L, -1, "world" );
    lua_remove( L, -2 );
}
void PushHandle( lua_State *L, int wi, const char *kind, size_t index )
{
    lua_getfenv( L, wi );
    lua_getfield( L, -1, kind );
    lua_rawgeti( L, -1, (int)index + 1 );
    lua_remove( L, -2 );
    lua_remove( L, -2 );
}
void StoreHandle( lua_State *L, int wi, const char *kind, size_t index )
{
    int hi = lua_gettop( L );
    lua_newtable( L );
    lua_pushvalue( L, wi );
    lua_setfield( L, -2, "world" );
    lua_setfenv( L, hi );
    lua_getfenv( L, wi );
    lua_getfield( L, -1, kind );
    lua_pushvalue( L, hi );
    lua_rawseti( L, -2, (int)index + 1 );
    lua_pop( L, 2 );
}
bool ValidShape( const ShapeHandle &h )
{
    return h.world->alive && h.index < h.world->shapes.size() &&
           B2_ID_EQUALS( h.world->shapes[h.index]->id, h.id ) && b2Shape_IsValid( h.id );
}
bool ValidChain( const ChainHandle &h )
{
    return h.world->alive && h.index < h.world->chains.size() &&
           B2_ID_EQUALS( h.world->chains[h.index]->id, h.id ) && b2Chain_IsValid( h.id );
}
ShapeHandle &GetShape( lua_State *L )
{
    auto *h = (ShapeHandle *)luaL_checkudata( L, 1, kShape );
    if ( !ValidShape( *h ) )
        luaL_error( L, "physics shape is destroyed" );
    return *h;
}
ChainHandle &GetChain( lua_State *L )
{
    auto *h = (ChainHandle *)luaL_checkudata( L, 1, kChain );
    if ( !ValidChain( *h ) )
        luaL_error( L, "physics chain is destroyed" );
    return *h;
}
double IntegerOption( lua_State *L, int table, const char *key, double value, double low, double high )
{
    value = Option( L, table, key, value );
    if ( value < low || value > high || value != std::floor( value ) )
        luaL_error( L, "%s requires an integer in the supported range", key );
    return value;
}
// Lua numbers represent all 32-bit masks exactly. Do not silently round the
// 64-bit native filter into a double. Larger bitsets need a separate API contract.
b2Filter ReadFilter( lua_State *L, int table )
{
    luaL_checktype( L, table, LUA_TTABLE );
    Options( L, table, { "categoryBits", "maskBits", "groupIndex" } );
    b2Filter f = b2DefaultFilter();
    f.categoryBits = (uint64_t)IntegerOption( L, table, "categoryBits", 1, 0, UINT32_MAX );
    f.maskBits = (uint64_t)IntegerOption( L, table, "maskBits", UINT32_MAX, 0, UINT32_MAX );
    f.groupIndex = (int)IntegerOption( L, table, "groupIndex", 0, INT32_MIN, INT32_MAX );
    return f;
}
b2Filter FilterOption( lua_State *L, int table )
{
    b2Filter f = b2DefaultFilter();
    f.maskBits = UINT32_MAX;
    lua_getfield( L, table, "filter" );
    if ( !lua_isnil( L, -1 ) )
        f = ReadFilter( L, lua_gettop( L ) );
    lua_pop( L, 1 );
    return f;
}
void PushFilter( lua_State *L, b2Filter f )
{
    lua_createtable( L, 0, 3 );
    Field( L, "categoryBits", (double)f.categoryBits );
    Field( L, "maskBits", (double)f.maskBits );
    Field( L, "groupIndex", f.groupIndex );
}
void ForgetHandle( lua_State *L, int wi, const char *kind, size_t slot )
{
    lua_getfenv( L, wi );
    lua_getfield( L, -1, kind );
    lua_pushnil( L );
    lua_rawseti( L, -2, (int)slot + 1 );
    lua_pop( L, 2 );
}
void EraseSlot( std::vector<size_t> &slots, size_t slot )
{
    slots.erase( std::remove( slots.begin(), slots.end(), slot ), slots.end() );
}
void ReleaseShape( lua_State *L, World &w, int wi, size_t slot, bool unlinkBody = true )
{
    Shape &s = *w.shapes[slot];
    if ( unlinkBody )
        EraseSlot( w.bodies[s.body]->shapes, slot );
    s.id = b2_nullShapeId;
    w.freeShapes.push_back( slot );
    ForgetHandle( L, wi, "shapes", slot );
}
void ReleaseChain( lua_State *L, World &w, int wi, size_t slot )
{
    Chain &c = *w.chains[slot];
    // Remove a long terrain's entries in one pass, preserving enumeration order.
    auto &shapes = w.bodies[c.body]->shapes;
    shapes.erase( std::remove_if( shapes.begin(), shapes.end(),
                                  [&]( size_t shape ) { return w.shapes[shape]->chain == slot; } ),
                  shapes.end() );
    for ( size_t shape : c.segments )
        ReleaseShape( L, w, wi, shape, false );
    c.segments.clear();
    EraseSlot( w.bodies[c.body]->chains, slot );
    c.id = b2_nullChainId;
    w.freeChains.push_back( slot );
    ForgetHandle( L, wi, "chains", slot );
}
// All native shapes, including chain segments, have stable identity in queries
// and events. Lua handles retain the world; the world owns the native storage.
void RegisterShape( lua_State *L, World &w, Body &b, int wi, b2ShapeId id, unsigned int element,
                    size_t chain = kInactive, int segmentIndex = 0 )
{
    Shape *s;
    if ( w.freeShapes.empty() )
    {
        s = new Shape;
        s->serial = w.shapes.size();
        w.shapes.emplace_back( s );
    }
    else
    {
        s = w.shapes[w.freeShapes.back()].get();
        w.freeShapes.pop_back();
    }
    s->id = id;
    s->body = b.serial;
    s->element = element;
    s->chain = chain;
    s->segmentIndex = segmentIndex;
    b.shapes.push_back( s->serial );
    b2Shape_SetUserData( id, s );
    auto *h = (ShapeHandle *)lua_newuserdata( L, sizeof( ShapeHandle ) );
    *h = { &w, s->serial, id };
    luaL_getmetatable( L, kShape );
    lua_setmetatable( L, -2 );
    StoreHandle( L, wi, "shapes", s->serial );
}
int ReturnShape( lua_State *L, World &w, Body &b, b2ShapeId id )
{
    if ( !b2Shape_IsValid( id ) )
        return luaL_error( L, "shape creation failed" );
    PushWorld( L, 1 );
    RegisterShape( L, w, b, lua_gettop( L ), id, ++b.fixtureCount );
    RefreshCompoundEdges( w, b.id );
    return 1;
}
int ShapeValid( lua_State *L )
{
    lua_pushboolean( L, ValidShape( *(ShapeHandle *)luaL_checkudata( L, 1, kShape ) ) );
    return 1;
}
int ShapeTestPoint( lua_State *L )
{
    ShapeHandle &h = GetShape( L );
    World &w = *h.world;
    if ( w.syncing || b2World_IsLocked( w.id ) )
        return luaL_error( L, "world is locked" );
    b2Pos point = { (float)( Number( L, 2 ) / w.scale ), (float)( Number( L, 3 ) / w.scale ) };
    lua_pushboolean( L, b2Shape_TestPoint( h.id, point ) );
    return 1;
}
int ShapeIndex( lua_State *L )
{
    // isValid remains callable after destruction.
    const char *key = luaL_checkstring( L, 2 );
    if ( !strcmp( key, "isValid" ) )
    {
        lua_pushcfunction( L, ShapeValid );
        return 1;
    }
    ShapeHandle &h = GetShape( L );
    Shape &s = *h.world->shapes[h.index];
    if ( !strcmp( key, "density" ) )
        lua_pushnumber( L, b2Shape_GetDensity( h.id ) );
    else if ( !strcmp( key, "friction" ) )
        lua_pushnumber( L, b2Shape_GetFriction( h.id ) );
    else if ( !strcmp( key, "bounce" ) )
        lua_pushnumber( L, b2Shape_GetRestitution( h.id ) );
    else if ( !strcmp( key, "isSensor" ) )
        lua_pushboolean( L, b2Shape_IsSensor( h.id ) );
    else if ( !strcmp( key, "filter" ) )
        PushFilter( L, b2Shape_GetFilter( h.id ) );
    else if ( !strcmp( key, "body" ) || !strcmp( key, "chain" ) )
    {
        PushWorld( L, 1 );
        if ( !strcmp( key, "body" ) )
            PushHandle( L, lua_gettop( L ), "bodies", s.body );
        else if ( s.chain != kInactive )
            PushHandle( L, lua_gettop( L ), "chains", s.chain );
        else
            lua_pushnil( L );
    }
    else if ( !strcmp( key, "element" ) )
        lua_pushinteger( L, s.element );
    else if ( !strcmp( key, "segmentIndex" ) )
        lua_pushinteger( L, s.segmentIndex );
    else if ( !strcmp( key, "type" ) )
    {
        b2ShapeType type = b2Shape_GetType( h.id );
        lua_pushstring( L, type == b2_circleShape         ? "circle"
                           : type == b2_capsuleShape      ? "capsule"
                           : type == b2_chainSegmentShape ? "chainSegment"
                                                          : "polygon" );
    }
    else
    {
        luaL_getmetatable( L, kShape );
        lua_pushvalue( L, 2 );
        lua_rawget( L, -2 );
    }
    return 1;
}
int ShapeNewIndex( lua_State *L )
{
    ShapeHandle &h = GetShape( L );
    Mutable( L, *h.world );
    const char *key = luaL_checkstring( L, 2 );
    if ( !strcmp( key, "filter" ) )
        b2Shape_SetFilter( h.id, ReadFilter( L, 3 ) );
    else if ( !strcmp( key, "density" ) || !strcmp( key, "friction" ) || !strcmp( key, "bounce" ) )
    {
        double v = Number( L, 3 );
        if ( v < 0 || ( !strcmp( key, "bounce" ) && v > 1 ) )
            return luaL_error( L, "invalid shape material value" );
        if ( !strcmp( key, "density" ) )
        {
            if ( h.world->shapes[h.index]->chain != kInactive )
                return luaL_error( L, "chain segments have no mass" );
            b2Shape_SetDensity( h.id, (float)v, true );
        }
        else if ( !strcmp( key, "friction" ) )
            b2Shape_SetFriction( h.id, (float)v );
        else
            b2Shape_SetRestitution( h.id, (float)v );
    }
    else
        return luaL_error( L, "unknown or read-only shape property; recreate to change sensor state" );
    return 0;
}
int ShapeDestroy( lua_State *L )
{
    ShapeHandle &h = GetShape( L );
    Mutable( L, *h.world );
    if ( h.world->shapes[h.index]->chain != kInactive )
        return luaL_error( L, "destroy the owning chain, not an individual segment" );
    b2BodyId bodyId = b2Shape_GetBody( h.id );
    b2DestroyShape( h.id, true );
    RefreshCompoundEdges( *h.world, bodyId );
    PushWorld( L, 1 );
    ReleaseShape( L, *h.world, lua_gettop( L ), h.index );
    return 0;
}
int ChainValid( lua_State *L )
{
    lua_pushboolean( L, ValidChain( *(ChainHandle *)luaL_checkudata( L, 1, kChain ) ) );
    return 1;
}
int ChainDestroy( lua_State *L )
{
    ChainHandle &h = GetChain( L );
    Mutable( L, *h.world );
    b2DestroyChain( h.id );
    PushWorld( L, 1 );
    ReleaseChain( L, *h.world, lua_gettop( L ), h.index );
    return 0;
}
int ChainSegments( lua_State *L )
{
    ChainHandle &h = GetChain( L );
    PushWorld( L, 1 );
    int wi = lua_gettop( L );
    const auto &segments = h.world->chains[h.index]->segments;
    lua_createtable( L, (int)segments.size(), 0 );
    int index = 0;
    for ( size_t slot : segments )
    {
        PushHandle( L, wi, "shapes", slot );
        lua_rawseti( L, -2, ++index );
    }
    return 1;
}
int BodyShapes( lua_State *L )
{
    Body &b = GetBody( L );
    PushWorld( L, 1 );
    int wi = lua_gettop( L );
    lua_createtable( L, (int)b.shapes.size(), 0 );
    int index = 0;
    for ( size_t slot : b.shapes )
    {
        PushHandle( L, wi, "shapes", slot );
        lua_rawseti( L, -2, ++index );
    }
    return 1;
}
int BodyMass( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    b2MassData mass = b2Body_GetMassData( b.id );
    b2Pos center = b2Body_GetWorldCenter( b.id );
    lua_createtable( L, 0, 6 );
    Field( L, "mass", mass.mass );
    Field( L, "localCenterX", mass.center.x * w.scale );
    Field( L, "localCenterY", mass.center.y * w.scale );
    Field( L, "worldCenterX", center.x * w.scale );
    Field( L, "worldCenterY", center.y * w.scale );
    Field( L, "rotationalInertia", mass.rotationalInertia );
    return 1;
}
void Activate( World &w, Body &b )
{
    if ( b.binding && b.activeIndex == kInactive )
    {
        b.activeIndex = w.active.size();
        w.active.push_back( &b );
    }
}
void Deactivate( World &w, Body &b )
{
    if ( b.activeIndex == kInactive )
        return;
    w.active[b.activeIndex] = w.active.back();
    w.active.back()->activeIndex = b.activeIndex;
    w.active.pop_back();
    b.activeIndex = kInactive;
}
Pose ReadPose( World &w, Body &b )
{
    b2Pos p = b2Body_GetPosition( b.id );
    return { p.x * w.scale, p.y * w.scale, b2Rot_GetAngle( b2Body_GetRotation( b.id ) ) };
}
void ClearBinding( lua_State *L, World &w, Body &b, int bi )
{
    if ( !b.binding )
        return;
    lua_getfenv( L, bi );
    lua_getfield( L, -1, "view" );
    if ( lua_istable( L, -1 ) )
    {
        lua_pushstring( L, kBinding );
        lua_pushnil( L );
        lua_rawset( L, -3 );
    }
    lua_pop( L, 1 );
    lua_pushnil( L );
    lua_setfield( L, -2, "view" );
    lua_pushnil( L );
    lua_setfield( L, -2, "proxy" );
    lua_pop( L, 1 );
    Deactivate( w, b );
    b.binding.reset();
    --w.bindingCount;
}
void ResetPose( World &w, Body &b )
{
    if ( b.binding )
    {
        b.binding->current = ReadPose( w, b );
        if ( w.interpolating )
            b.binding->previous = b.binding->current;
        Activate( w, b );
    }
}
Interpolation ReadInterpolation( lua_State *L, int index )
{
    if ( lua_isboolean( L, index ) )
        return lua_toboolean( L, index ) ? Interpolation::On : Interpolation::Off;
    if ( lua_type( L, index ) == LUA_TSTRING && !strcmp( lua_tostring( L, index ), "auto" ) )
        return Interpolation::Auto;
    luaL_error( L, "world interpolation must be 'auto', true or false" );
    return Interpolation::Auto;
}
void UpdateInterpolation( World &w )
{
    double rate = w.frameInterval / w.dt * w.speed;
    // Test the cadence, not this frame's alpha: a half-speed frame with zero
    // remainder still needs history for the following zero-step render frame.
    bool aligned = rate >= 1 && rate == std::floor( rate ) && w.budget == std::floor( w.budget );
    bool enabled = w.interpolation == Interpolation::On ||
        ( w.interpolation == Interpolation::Auto && rate <= w.cap && !aligned );
    if ( enabled == w.interpolating )
        return;
    w.interpolating = enabled;
    w.poseHistoryCopies = 0;
    if ( enabled )
        for ( const auto &body : w.bodies )
            if ( body->binding )
                body->binding->previous = body->binding->current;
    if ( w.soft.owner )
        w.soft.setInterpolation( w.soft.owner, enabled );
}
int BodyUnbind( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    ClearBinding( L, w, b, 1 );
    return 0;
}
int BodyBind( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    LuaProxy *proxy = LuaProxy::GetProxy( L, 2 );
    const LuaProxyVTable *v = proxy ? &proxy->Delegate() : NULL;
    while ( v && v != &LuaDisplayObjectProxyVTable::Constant() )
    {
        const LuaProxyVTable *parent = &v->Parent();
        v = parent == v ? NULL : parent;
    }
    if ( !v || !proxy->Object() )
        return luaL_error( L, "bindView requires a live display object" );
    DisplayObject *o = static_cast<DisplayObject *>( proxy->Object() );
    if ( o->GetExtensions() || o->IsOrphan() )
        return luaL_error( L, "bindView requires an on-stage view without a physics body" );
    lua_pushstring( L, kBinding );
    lua_rawget( L, 2 );
    bool claimed = !lua_isnil( L, -1 );
    lua_pop( L, 1 );
    if ( claimed )
        return luaL_error( L, "view is already bound; unbind it first" );
    if ( !lua_isnoneornil( L, 3 ) )
    {
        luaL_checktype( L, 3, LUA_TTABLE );
        lua_getfield( L, 3, "interpolation" );
        if ( !lua_isnil( L, -1 ) )
            return luaL_error( L, "configure interpolation on the physics world, not bindView" );
        lua_pop( L, 1 );
        Options( L, 3, {} );
    }
    ClearBinding( L, w, b, 1 );
    lua_getfenv( L, 1 );
    lua_pushvalue( L, 2 );
    lua_setfield( L, -2, "view" );
    lua_pushliteral( L, "_proxy" );
    lua_rawget( L, 2 );
    lua_setfield( L, -2, "proxy" );
    lua_pop( L, 1 );
    lua_pushstring( L, kBinding );
    lua_pushlightuserdata( L, &b );
    lua_rawset( L, 2 );
    b.binding.reset( new Binding );
    ++w.bindingCount;
    b.binding->proxy = proxy;
    ResetPose( w, b );
    return 0;
}
void SyncViews( lua_State *L, World &w, int wi, double alpha )
{
    if ( !w.interpolating )
        alpha = 1;
    // Visual time never runs backwards after manual sync, cap saturation or pause.
    const double target = w.index ? (double)w.index - 1 + alpha : 0;
    w.renderTime = std::max( w.renderTime, target );
    alpha = w.index ? std::max( 0.0, std::min( 1.0, w.renderTime - ( (double)w.index - 1 ) ) ) : 1;
    w.viewAlpha = alpha;
    w.lastViewUpdates = 0;
    w.syncing = true;
    for ( size_t i = 0; i < w.active.size(); )
    {
        Body &b = *w.active[i];
        Binding &binding = *b.binding;
        DisplayObject *o = static_cast<DisplayObject *>( binding.proxy->Object() );
        if ( !o || o->IsOrphan() || o->GetExtensions() )
        {
            PushHandle( L, wi, "bodies", b.serial );
            ClearBinding( L, w, b, -1 );
            lua_pop( L, 1 );
            continue;
        }
        const Pose &z = binding.current;
        const Pose &a = w.interpolating ? binding.previous : z;
        double t = alpha;
        // Preserve exact endpoints; interpolation must not introduce subtract/add
        // restoration drift when presenting the completed physical pose.
        double x = t == 1 ? z.x : a.x + ( (double)z.x - a.x ) * t;
        double y = t == 1 ? z.y : a.y + ( (double)z.y - a.y ) * t;
        double angle = t == 1 ? z.angle : a.angle + b2UnwindAngle( z.angle - a.angle ) * t;
        o->SetGeometricProperty( kOriginX, (Real)x );
        o->SetGeometricProperty( kOriginY, (Real)y );
        o->SetGeometricProperty( kRotation, (Real)( angle / kRad ) );
        ++w.lastViewUpdates;
        bool same = a.x == z.x && a.y == z.y && a.angle == z.angle;
        if ( !b2Body_IsAwake( b.id ) && ( t == 1 || same ) )
            Deactivate( w, b );
        else
            ++i;
    }
    if ( w.soft.owner )
    {
        lua_getfenv( L, wi );
        lua_getfield( L, -1, "softSystem" );
        w.soft.syncViews( L, w.soft.owner, lua_gettop( L ), alpha );
        lua_pop( L, 2 );
    }
    w.syncing = false;
}
int BodyTransform( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    float x = (float)( Number( L, 2 ) / w.scale ), y = (float)( Number( L, 3 ) / w.scale );
    float angle = (float)( Number( L, 4 ) * kRad );
    if ( !lua_isnoneornil( L, 5 ) )
    {
        luaL_checktype( L, 5, LUA_TTABLE );
        Options( L, 5, { "snap" } );
        if ( !Flag( L, 5, "snap", true ) )
            return luaL_error( L, "setTransform supports snap=true only" );
    }
    b2Body_SetTransform( b.id, { x, y }, b2MakeRot( angle ) );
    ResetPose( w, b );
    return 0;
}
int BodyPosition( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    b2Pos p = b2Body_GetPosition( b.id );
    lua_pushnumber( L, p.x * w.scale );
    lua_pushnumber( L, p.y * w.scale );
    return 2;
}
int BodyVelocity( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    b2Vec2 v = b2Body_GetLinearVelocity( b.id );
    lua_pushnumber( L, v.x * w.scale );
    lua_pushnumber( L, v.y * w.scale );
    return 2;
}
int BodySetVelocity( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    float x = (float)( Number( L, 2 ) / w.scale ), y = (float)( Number( L, 3 ) / w.scale );
    b2Body_SetLinearVelocity( b.id, { x, y } );
    return 0;
}
int BodyForce( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    float x = (float)Number( L, 2 ), y = (float)Number( L, 3 );
    b2Pos point = b2Body_GetWorldCenter( b.id );
    if ( !lua_isnoneornil( L, 4 ) )
    {
        point.x = (float)( Number( L, 4 ) / w.scale );
        point.y = (float)( Number( L, 5 ) / w.scale );
    }
    b2Body_ApplyForce( b.id, { x, y }, point, true );
    return 0;
}
int BodyImpulse( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    float x = (float)Number( L, 2 ), y = (float)Number( L, 3 );
    b2Pos point = b2Body_GetWorldCenter( b.id );
    if ( !lua_isnoneornil( L, 4 ) )
    {
        point.x = (float)( Number( L, 4 ) / w.scale );
        point.y = (float)( Number( L, 5 ) / w.scale );
    }
    b2Body_ApplyLinearImpulse( b.id, { x, y }, point, true );
    return 0;
}
int BodyTorque( lua_State *L )
{
    Body &b = GetBody( L );
    Mutable( L, Owner( L, 1, kBody ) );
    b2Body_ApplyTorque( b.id, (float)Number( L, 2 ), true );
    return 0;
}
int BodyAngularImpulse( lua_State *L )
{
    Body &b = GetBody( L );
    Mutable( L, Owner( L, 1, kBody ) );
    b2Body_ApplyAngularImpulse( b.id, (float)Number( L, 2 ), true );
    return 0;
}
int BodyWorldPoint( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    b2Vec2 p = { (float)( Number( L, 2 ) / w.scale ), (float)( Number( L, 3 ) / w.scale ) };
    b2Pos result = b2Body_GetWorldPoint( b.id, p );
    lua_pushnumber( L, result.x * w.scale );
    lua_pushnumber( L, result.y * w.scale );
    return 2;
}
int BodyLocalPoint( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    b2Pos p = { (float)( Number( L, 2 ) / w.scale ), (float)( Number( L, 3 ) / w.scale ) };
    b2Vec2 result = b2Body_GetLocalPoint( b.id, p );
    lua_pushnumber( L, result.x * w.scale );
    lua_pushnumber( L, result.y * w.scale );
    return 2;
}
void ReleaseJoint( lua_State *L, World &w, int wi, b2JointId joint )
{
    size_t slot = (size_t)(uintptr_t)b2Joint_GetUserData( joint ) - 1;
    w.joints[slot] = b2_nullJointId;
    w.freeJoints.push_back( slot );
    lua_getfenv( L, wi );
    lua_getfield( L, -1, "joints" );
    lua_pushnil( L );
    lua_rawseti( L, -2, (int)slot + 1 );
    lua_pop( L, 2 );
}
int BodyDestroy( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    ClearBinding( L, w, b, 1 );
    PushWorld( L, 1 );
    int wi = lua_gettop( L );
    int count = b2Body_GetJointCount( b.id );
    std::vector<b2JointId> attached( count );
    if ( count )
        b2Body_GetJoints( b.id, attached.data(), count );
    for ( b2JointId joint : attached )
        ReleaseJoint( L, w, wi, joint );
    while ( !b.chains.empty() )
        ReleaseChain( L, w, wi, b.chains.back() );
    while ( !b.shapes.empty() )
        ReleaseShape( L, w, wi, b.shapes.back() );
    RefreshCompoundEdges( w, b.id, false );
    b2DestroyBody( b.id );
    b.id = b2_nullBodyId;
    b.fixtureCount = 0;
    w.freeBodies.push_back( b.serial );
    lua_getfenv( L, wi );
    lua_getfield( L, -1, "bodies" );
    lua_pushnil( L );
    lua_rawseti( L, -2, (int)b.serial + 1 );
    return 0;
}
b2ShapeDef ShapeOptions( lua_State *L, int ti )
{
    b2ShapeDef d = b2DefaultShapeDef();
    double density = Option( L, ti, "density", 1 );
    d.density = (float)density;
    d.material.friction = (float)Option( L, ti, "friction", 0.3 );
    d.material.restitution = (float)Option( L, ti, "bounce", 0 );
    if ( density < 0 || d.material.friction < 0 || d.material.restitution < 0 ||
         d.material.restitution > 1 )
        luaL_error( L, "invalid density, friction or bounce" );
    d.isSensor = Flag( L, ti, "isSensor", false );
    d.enableContactEvents = true;
    d.enableSensorEvents = true;
    d.filter = FilterOption( L, ti );
    return d;
}
int BodyCircle( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2, { "radius", "x", "y", "density", "friction", "bounce", "isSensor", "filter" } );
    b2ShapeDef d = ShapeOptions( L, 2 );
    float r = (float)( Option( L, 2, "radius", 0 ) / w.scale );
    if ( r <= 0 )
        return luaL_error( L, "radius must be positive" );
    b2Circle c = {
        { (float)( Option( L, 2, "x", 0 ) / w.scale ), (float)( Option( L, 2, "y", 0 ) / w.scale ) }, r };
    return ReturnShape( L, w, b, b2CreateCircleShape( b.id, &d, &c ) );
}
int BodyCapsule( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2, { "x1", "y1", "x2", "y2", "radius", "density", "friction", "bounce",
                    "isSensor", "filter" } );
    b2ShapeDef d = ShapeOptions( L, 2 );
    b2Capsule c = {
        { (float)( Option( L, 2, "x1", 0 ) / w.scale ), (float)( Option( L, 2, "y1", 0 ) / w.scale ) },
        { (float)( Option( L, 2, "x2", 0 ) / w.scale ), (float)( Option( L, 2, "y2", 0 ) / w.scale ) },
        (float)( Option( L, 2, "radius", 0 ) / w.scale ) };
    if ( c.radius <= 0 )
        return luaL_error( L, "radius must be positive" );
    return ReturnShape( L, w, b, PhysicsCapsule::Create( b.id, &d, c ) );
}
float PolygonRadius( lua_State *L, const World &w )
{
    lua_getfield( L, 2, "radius" );
    lua_getfield( L, 2, "roundness" );
    if ( !lua_isnil( L, -2 ) && !lua_isnil( L, -1 ) )
        luaL_error( L, "specify radius or roundness, not both" );
    double value = !lua_isnil( L, -2 )   ? Number( L, -2 )
                   : !lua_isnil( L, -1 ) ? Number( L, -1 )
                                          : 0;
    lua_pop( L, 2 );
    if ( value < 0 )
        luaL_error( L, "polygon radius must be nonnegative" );
    return (float)( value / w.scale );
}
int BodyBox( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2,
             { "halfWidth", "halfHeight", "x", "y", "angle", "radius", "roundness", "density",
               "friction", "bounce", "isSensor", "filter" } );
    b2ShapeDef d = ShapeOptions( L, 2 );
    float radius = PolygonRadius( L, w );
    float hx = (float)( Option( L, 2, "halfWidth", 0 ) / w.scale ),
          hy = (float)( Option( L, 2, "halfHeight", 0 ) / w.scale );
    if ( hx <= 0 || hy <= 0 )
        return luaL_error( L, "box half extents must be positive" );
    b2Vec2 center = { (float)( Option( L, 2, "x", 0 ) / w.scale ),
                      (float)( Option( L, 2, "y", 0 ) / w.scale ) };
    b2Polygon p =
        b2MakeOffsetBox( hx, hy, center, b2MakeRot( (float)( Option( L, 2, "angle", 0 ) * kRad ) ) );
    p.radius = radius;
    return ReturnShape( L, w, b, b2CreatePolygonShape( b.id, &d, &p ) );
}
int BodyPolygon( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2,
             { "shape", "radius", "roundness", "density", "friction", "bounce", "isSensor", "filter" } );
    b2ShapeDef d = ShapeOptions( L, 2 );
    float radius = PolygonRadius( L, w );
    lua_getfield( L, 2, "shape" );
    luaL_checktype( L, -1, LUA_TTABLE );
    size_t length = lua_objlen( L, -1 );
    if ( length < 6 || length > 2 * B2_MAX_POLYGON_VERTICES || length % 2 )
        return luaL_error( L, "polygon requires 3..B2_MAX_POLYGON_VERTICES x/y pairs" );
    b2Vec2 vertices[B2_MAX_POLYGON_VERTICES];
    for ( size_t i = 0; i < length / 2; ++i )
    {
        lua_rawgeti( L, -1, (int)i * 2 + 1 );
        vertices[i].x = (float)( Number( L, -1 ) / w.scale );
        lua_pop( L, 1 );
        lua_rawgeti( L, -1, (int)i * 2 + 2 );
        vertices[i].y = (float)( Number( L, -1 ) / w.scale );
        lua_pop( L, 1 );
    }
    b2Hull hull = b2ComputeHull( vertices, (int)length / 2 );
    if ( hull.count < 3 || !b2ValidateHull( &hull ) )
        return luaL_error( L, "degenerate polygon hull" );
    b2Polygon polygon = b2MakePolygon( &hull, radius );
    return ReturnShape( L, w, b, b2CreatePolygonShape( b.id, &d, &polygon ) );
}
int ChainBody( lua_State *L )
{
    ChainHandle &h = GetChain( L );
    PushWorld( L, 1 );
    PushHandle( L, lua_gettop( L ), "bodies", h.world->chains[h.index]->body );
    return 1;
}
int ChainFilter( lua_State *L )
{
    ChainHandle &h = GetChain( L );
    Mutable( L, *h.world );
    b2Filter filter = ReadFilter( L, 2 );
    for ( size_t slot : h.world->chains[h.index]->segments )
        b2Shape_SetFilter( h.world->shapes[slot]->id, filter );
    return 0;
}
int ChainMaterial( lua_State *L )
{
    ChainHandle &h = GetChain( L );
    Mutable( L, *h.world );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2, { "friction", "bounce" } );
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = (float)Option( L, 2, "friction", .3 );
    material.restitution = (float)Option( L, 2, "bounce", 0 );
    if ( material.friction < 0 || material.restitution < 0 || material.restitution > 1 )
        return luaL_error( L, "invalid chain material" );
    b2Chain_SetAllSurfaceMaterials( h.id, &material );
    return 0;
}
b2Vec2 GhostOption( lua_State *L, const char *key, b2Vec2 fallback, float scale )
{
    lua_getfield( L, 2, key );
    if ( !lua_isnil( L, -1 ) )
    {
        int table = lua_gettop( L );
        luaL_checktype( L, table, LUA_TTABLE );
        Options( L, table, { "x", "y" } );
        lua_getfield( L, table, "x" );
        fallback.x = (float)( Number( L, -1 ) / scale );
        lua_pop( L, 1 );
        lua_getfield( L, table, "y" );
        fallback.y = (float)( Number( L, -1 ) / scale );
        lua_pop( L, 1 );
    }
    lua_pop( L, 1 );
    return fallback;
}
int BodyChain( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    if ( b2Body_GetType( b.id ) != b2_staticBody )
        return luaL_error( L, "chains require a static body" );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2, { "points", "isLoop", "ghost1", "ghost2", "friction", "bounce", "filter" } );
    b2ChainDef def = b2DefaultChainDef();
    def.isLoop = Flag( L, 2, "isLoop", false );
    def.filter = FilterOption( L, 2 );
    def.enableSensorEvents = true;
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = (float)Option( L, 2, "friction", .3 );
    material.restitution = (float)Option( L, 2, "bounce", 0 );
    if ( material.friction < 0 || material.restitution < 0 || material.restitution > 1 )
        return luaL_error( L, "invalid chain material" );
    lua_getfield( L, 2, "points" );
    luaL_checktype( L, -1, LUA_TTABLE );
    int table = lua_gettop( L );
    size_t length = lua_objlen( L, table );
    if ( length % 2 || length < ( def.isLoop ? 6 : 4 ) || length > 8192 )
        return luaL_error( L, "chain requires 2..4096 points (3 for a loop)" );
    int count = (int)length / 2;
    // Lua owns scratch storage, including when argument validation longjmps.
    auto *points = (b2Vec2 *)lua_newuserdata( L, count * sizeof( b2Vec2 ) );
    for ( int i = 0; i < count; ++i )
    {
        lua_rawgeti( L, table, 2 * i + 1 );
        points[i].x = (float)( Number( L, -1 ) / w.scale );
        lua_pop( L, 1 );
        lua_rawgeti( L, table, 2 * i + 2 );
        points[i].y = (float)( Number( L, -1 ) / w.scale );
        lua_pop( L, 1 );
        if ( std::abs( points[i].x ) > 1e4 || std::abs( points[i].y ) > 1e4 )
            return luaL_error( L, "chain local points must be within +/-10000 metres" );
    }
    const float slopSquared = B2_LINEAR_SLOP * B2_LINEAR_SLOP;
    int segments = def.isLoop ? count : count - 1;
    for ( int i = 0; i < segments; ++i )
        if ( b2DistanceSquared( points[i], points[( i + 1 ) % count] ) <= slopSquared )
            return luaL_error( L, "chain neighbours must be farther apart than Box2D linear slop" );
    if ( def.isLoop )
    {
        for ( const char *key : { "ghost1", "ghost2" } )
        {
            lua_getfield( L, 2, key );
            if ( !lua_isnil( L, -1 ) )
                return luaL_error( L, "loop chains do not take ghost points" );
            lua_pop( L, 1 );
        }
    }
    else
    {
        def.ghost1 = GhostOption( L, "ghost1", b2Sub( b2MulSV( 2, points[0] ), points[1] ), w.scale );
        def.ghost2 =
            GhostOption( L, "ghost2", b2Sub( b2MulSV( 2, points[count - 1] ), points[count - 2] ), w.scale );
        if ( std::abs( def.ghost1.x ) > 2e4 || std::abs( def.ghost1.y ) > 2e4 ||
             std::abs( def.ghost2.x ) > 2e4 || std::abs( def.ghost2.y ) > 2e4 ||
             b2DistanceSquared( def.ghost1, points[0] ) <= slopSquared ||
             b2DistanceSquared( def.ghost2, points[count - 1] ) <= slopSquared )
            return luaL_error( L, "invalid or too-close chain ghost point" );
    }
    def.points = points;
    def.pointCount = count;
    def.materials = &material;
    def.materialCount = 1;
    auto *ids = (b2ShapeId *)lua_newuserdata( L, segments * sizeof( b2ShapeId ) );
    b2ChainId id = b2CreateChain( b.id, &def );
    if ( !b2Chain_IsValid( id ) )
        return luaL_error( L, "chain creation failed" );
    b2Chain_GetSegments( id, ids, segments );
    Chain *chain;
    if ( w.freeChains.empty() )
    {
        chain = new Chain;
        chain->serial = w.chains.size();
        w.chains.emplace_back( chain );
    }
    else
    {
        chain = w.chains[w.freeChains.back()].get();
        w.freeChains.pop_back();
    }
    chain->id = id;
    chain->body = b.serial;
    chain->element = ++b.fixtureCount;
    b.chains.push_back( chain->serial );
    PushWorld( L, 1 );
    int wi = lua_gettop( L );
    for ( int i = 0; i < segments; ++i )
    {
        RegisterShape( L, w, b, wi, ids[i], chain->element, chain->serial, i + 1 );
        chain->segments.push_back( b.shapes.back() );
        lua_pop( L, 1 );
    }
    auto *h = (ChainHandle *)lua_newuserdata( L, sizeof( ChainHandle ) );
    *h = { &w, chain->serial, id };
    luaL_getmetatable( L, kChain );
    lua_setmetatable( L, -2 );
    StoreHandle( L, wi, "chains", chain->serial );
    return 1;
}
int BodyIndex( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    const char *key = luaL_checkstring( L, 2 );
    if ( !strcmp( key, "x" ) || !strcmp( key, "y" ) )
    {
        b2Pos p = b2Body_GetPosition( b.id );
        lua_pushnumber( L, ( !strcmp( key, "x" ) ? p.x : p.y ) * w.scale );
    }
    else if ( !strcmp( key, "rotation" ) )
        lua_pushnumber( L, b2Rot_GetAngle( b2Body_GetRotation( b.id ) ) / kRad );
    else if ( !strcmp( key, "angularVelocity" ) )
        lua_pushnumber( L, b2Body_GetAngularVelocity( b.id ) / kRad );
    else if ( !strcmp( key, "mass" ) )
        lua_pushnumber( L, b2Body_GetMass( b.id ) );
    else if ( !strcmp( key, "isAwake" ) )
        lua_pushboolean( L, b2Body_IsAwake( b.id ) );
    else if ( !strcmp( key, "gravityScale" ) )
        lua_pushnumber( L, b2Body_GetGravityScale( b.id ) );
    else if ( !strcmp( key, "linearDamping" ) )
        lua_pushnumber( L, b2Body_GetLinearDamping( b.id ) );
    else if ( !strcmp( key, "angularDamping" ) )
        lua_pushnumber( L, b2Body_GetAngularDamping( b.id ) );
    else if ( !strcmp( key, "isBullet" ) )
        lua_pushboolean( L, b2Body_IsBullet( b.id ) );
    else if ( !strcmp( key, "isSleepingAllowed" ) )
        lua_pushboolean( L, b2Body_IsSleepingEnabled( b.id ) );
    else if ( !strcmp( key, "isFixedRotation" ) )
        lua_pushboolean( L, b2Body_GetMotionLocks( b.id ).angularZ );
    else if ( !strcmp( key, "isActive" ) )
        lua_pushboolean( L, b2Body_IsEnabled( b.id ) );
    else if ( !strcmp( key, "bodyType" ) )
    {
        b2BodyType type = b2Body_GetType( b.id );
        lua_pushstring( L, type == b2_dynamicBody     ? "dynamic"
                           : type == b2_kinematicBody ? "kinematic"
                                                      : "static" );
    }
    else
    {
        luaL_getmetatable( L, kBody );
        lua_pushvalue( L, 2 );
        lua_rawget( L, -2 );
        if ( lua_isnil( L, -1 ) )
        {
            lua_pop( L, 2 );
            lua_getfenv( L, 1 );
            lua_pushvalue( L, 2 );
            lua_rawget( L, -2 );
        }
    }
    return 1;
}
int BodyNewIndex( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    const char *key = luaL_checkstring( L, 2 );
    if ( !strcmp( key, "x" ) || !strcmp( key, "y" ) || !strcmp( key, "rotation" ) )
    {
        b2Pos p = b2Body_GetPosition( b.id );
        b2Rot q = b2Body_GetRotation( b.id );
        double v = Number( L, 3 );
        if ( !strcmp( key, "x" ) )
            p.x = (float)( v / w.scale );
        else if ( !strcmp( key, "y" ) )
            p.y = (float)( v / w.scale );
        else
            q = b2MakeRot( (float)( v * kRad ) );
        b2Body_SetTransform( b.id, p, q );
        ResetPose( w, b );
    }
    else if ( !strcmp( key, "angularVelocity" ) )
        b2Body_SetAngularVelocity( b.id, (float)( Number( L, 3 ) * kRad ) );
    else if ( !strcmp( key, "gravityScale" ) )
        b2Body_SetGravityScale( b.id, (float)Number( L, 3 ) );
    else if ( !strcmp( key, "linearDamping" ) || !strcmp( key, "angularDamping" ) )
    {
        float value = (float)Number( L, 3 );
        if ( value < 0 )
            return luaL_error( L, "negative damping" );
        if ( !strcmp( key, "linearDamping" ) )
            b2Body_SetLinearDamping( b.id, value );
        else
            b2Body_SetAngularDamping( b.id, value );
    }
    else if ( !strcmp( key, "isBullet" ) || !strcmp( key, "isSleepingAllowed" ) ||
              !strcmp( key, "isFixedRotation" ) || !strcmp( key, "isActive" ) )
    {
        luaL_checktype( L, 3, LUA_TBOOLEAN );
        bool value = lua_toboolean( L, 3 ) != 0;
        if ( !strcmp( key, "isBullet" ) )
            b2Body_SetBullet( b.id, value );
        else if ( !strcmp( key, "isSleepingAllowed" ) )
            b2Body_EnableSleeping( b.id, value );
        else if ( !strcmp( key, "isFixedRotation" ) )
        {
            b2MotionLocks locks = b2Body_GetMotionLocks( b.id );
            locks.angularZ = value;
            b2Body_SetMotionLocks( b.id, locks );
        }
        else
        {
            if ( value )
                b2Body_Enable( b.id );
            else
                b2Body_Disable( b.id );
            ResetPose( w, b );
        }
    }
    else if ( !strcmp( key, "bodyType" ) )
    {
        const char *value = luaL_checkstring( L, 3 );
        b2BodyType type;
        if ( !strcmp( value, "static" ) )
            type = b2_staticBody;
        else if ( !strcmp( value, "kinematic" ) )
            type = b2_kinematicBody;
        else if ( !strcmp( value, "dynamic" ) )
            type = b2_dynamicBody;
        else
            return luaL_error( L, "unknown body type" );
        if ( type != b2_staticBody && !b.chains.empty() )
            return luaL_error( L, "chain bodies must remain static" );
        b2Body_SetType( b.id, type );
        ResetPose( w, b );
    }
    else if ( !strcmp( key, "isAwake" ) )
    {
        luaL_checktype( L, 3, LUA_TBOOLEAN );
        b2Body_SetAwake( b.id, lua_toboolean( L, 3 ) != 0 );
    }
    else if ( !strcmp( key, "mass" ) || !strcmp( key, "world" ) || !strcmp( key, "view" ) ||
              !strcmp( key, "proxy" ) )
        return luaL_error( L, "read-only/reserved body property" );
    else
    {
        lua_getfenv( L, 1 );
        lua_pushvalue( L, 2 );
        lua_pushvalue( L, 3 );
        lua_rawset( L, -3 );
    }
    return 0;
}
int WorldBody( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2,
             { "x", "y", "rotation", "type", "gravityScale", "linearDamping", "angularDamping",
               "isSleepingAllowed", "isAwake", "isFixedRotation", "isBullet" } );
    b2BodyDef d = b2DefaultBodyDef();
    d.type = b2_dynamicBody;
    lua_getfield( L, 2, "type" );
    const char *type = lua_isnil( L, -1 ) ? "dynamic" : luaL_checkstring( L, -1 );
    if ( !strcmp( type, "static" ) )
        d.type = b2_staticBody;
    else if ( !strcmp( type, "kinematic" ) )
        d.type = b2_kinematicBody;
    else if ( strcmp( type, "dynamic" ) )
        return luaL_error( L, "unknown body type" );
    lua_pop( L, 1 );
    d.position = { (float)( Option( L, 2, "x", 0 ) / w.scale ), (float)( Option( L, 2, "y", 0 ) / w.scale ) };
    d.rotation = b2MakeRot( (float)( Option( L, 2, "rotation", 0 ) * kRad ) );
    d.gravityScale = (float)Option( L, 2, "gravityScale", 1 );
    d.linearDamping = (float)Option( L, 2, "linearDamping", 0 );
    d.angularDamping = (float)Option( L, 2, "angularDamping", 0 );
    if ( d.linearDamping < 0 || d.angularDamping < 0 )
        return luaL_error( L, "negative damping" );
    d.enableSleep = Flag( L, 2, "isSleepingAllowed", true );
    d.isAwake = Flag( L, 2, "isAwake", true );
    d.motionLocks.angularZ = Flag( L, 2, "isFixedRotation", false );
    d.isBullet = Flag( L, 2, "isBullet", false );
    Body *b;
    if ( w.freeBodies.empty() )
    {
        b = new Body;
        b->serial = w.bodies.size();
        w.bodies.emplace_back( b );
    }
    else
    {
        b = w.bodies[w.freeBodies.back()].get();
        w.freeBodies.pop_back();
    }
    d.userData = b;
    b->id = b2CreateBody( w.id, &d );
    Handle *h = (Handle *)lua_newuserdata( L, sizeof( Handle ) );
    *h = { &w, b->serial, b->id, b2_nullJointId };
    luaL_getmetatable( L, kBody );
    lua_setmetatable( L, -2 );
    StoreHandle( L, 1, "bodies", b->serial );
    return 1;
}
#include "Rtt_LuaPhysicsWorldJoints.h"

int WorldListener( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    const char *key = lua_toboolean( L, lua_upvalueindex( 1 ) ) ? "collisionListener" : "stepListener";
    if ( !lua_isnil( L, 2 ) )
        luaL_checktype( L, 2, LUA_TFUNCTION );
    lua_getfenv( L, 1 );
    lua_pushvalue( L, 2 );
    lua_setfield( L, -2, key );
    return 0;
}
bool Call( lua_State *L, World &w, int top )
{
    const int status = lua_pcall( L, 1, 0, 0 );
    if ( status )
    {
        const char *error = lua_tostring( L, -1 );
        w.error = error ? error : "non-string callback error";
        w.faulted = true;
        w.paused = true;
        ++w.interruption;
        Rtt_Log( "independent physics callback error: %s\n", w.error.c_str() );
    }
    lua_settop( L, top );
    return !status && w.alive;
}
bool StepCallback( lua_State *L, World &w, int wi, const char *phase )
{
    const int top = lua_gettop( L );
    lua_getfenv( L, wi );
    lua_getfield( L, -1, "stepListener" );
    lua_remove( L, -2 );
    if ( lua_isnil( L, -1 ) )
    {
        lua_pop( L, 1 );
        return true;
    }
    lua_createtable( L, 0, 6 );
    lua_pushliteral( L, "physicsStep" );
    lua_setfield( L, -2, "name" );
    lua_pushstring( L, phase );
    lua_setfield( L, -2, "phase" );
    lua_pushvalue( L, wi );
    lua_setfield( L, -2, "world" );
    Field( L, "stepIndex", (double)w.index + ( phase[0] == 'b' ? 1 : 0 ) );
    Field( L, "dt", w.dt );
    Field( L, "simulationTime", (double)w.index * w.dt );
    return Call( L, w, top );
}
#include "Rtt_LuaPhysicsWorldContactPolicy.h"

struct Contact
{
    b2ShapeId a, b;
    bool began, sensor;
};
bool Contacts( lua_State *L, World &w, int wi )
{
    // Copies are needed only when Lua observes events. Callbacks may delete bodies/world.
    lua_getfenv( L, wi );
    lua_getfield( L, -1, "collisionListener" );
    bool listens = lua_isfunction( L, -1 );
    lua_pop( L, 2 );
    if ( !listens )
        return true;
    std::vector<Contact> list;
    b2ContactEvents c = b2World_GetContactEvents( w.id );
    b2SensorEvents s = b2World_GetSensorEvents( w.id );
    list.reserve( c.beginCount + c.endCount + s.beginCount + s.endCount );
    for ( int i = 0; i < c.beginCount; ++i )
        list.push_back( { c.beginEvents[i].shapeIdA, c.beginEvents[i].shapeIdB, true, false } );
    for ( int i = 0; i < c.endCount; ++i )
        list.push_back( { c.endEvents[i].shapeIdA, c.endEvents[i].shapeIdB, false, false } );
    for ( int i = 0; i < s.beginCount; ++i )
        list.push_back( { s.beginEvents[i].sensorShapeId, s.beginEvents[i].visitorShapeId, true, true } );
    for ( int i = 0; i < s.endCount; ++i )
        list.push_back( { s.endEvents[i].sensorShapeId, s.endEvents[i].visitorShapeId, false, true } );
    for ( const Contact &e : list )
    {
        if ( !b2Shape_IsValid( e.a ) || !b2Shape_IsValid( e.b ) )
            continue;
        Body *a = (Body *)b2Body_GetUserData( b2Shape_GetBody( e.a ) ),
             *b = (Body *)b2Body_GetUserData( b2Shape_GetBody( e.b ) );
        int top = lua_gettop( L );
        lua_getfenv( L, wi );
        lua_getfield( L, -1, "collisionListener" );
        lua_remove( L, -2 );
        if ( !lua_isfunction( L, -1 ) )
        {
            lua_settop( L, top );
            return true;
        }
        lua_createtable( L, 0, 9 );
        lua_pushliteral( L, "collision" );
        lua_setfield( L, -2, "name" );
        lua_pushvalue( L, wi );
        lua_setfield( L, -2, "world" );
        lua_pushstring( L, e.began ? "began" : "ended" );
        lua_setfield( L, -2, "phase" );
        Bool( L, "isSensor", e.sensor );
        Field( L, "stepIndex", (double)w.index );
        Field( L, "simulationTime", w.index * (double)w.dt );
        PushHandle( L, wi, "bodies", a->serial );
        lua_setfield( L, -2, "body1" );
        PushHandle( L, wi, "bodies", b->serial );
        lua_setfield( L, -2, "body2" );
        auto *sa = (Shape *)b2Shape_GetUserData( e.a );
        auto *sb = (Shape *)b2Shape_GetUserData( e.b );
        Field( L, "element1", sa->element );
        Field( L, "element2", sb->element );
        PushHandle( L, wi, "shapes", sa->serial );
        lua_setfield( L, -2, "shape1" );
        PushHandle( L, wi, "shapes", sb->serial );
        lua_setfield( L, -2, "shape2" );
        if ( !Call( L, w, top ) )
            return false;
    }
    return true;
}
bool OneStep( lua_State *L, World &w, int wi )
{
    if ( w.index >= 9007199254740991ULL )
    {
        w.faulted = w.paused = true;
        w.error = "stepIndex exceeds exact integer range";
        return false;
    }
    if ( !StepCallback( L, w, wi, "before" ) )
        return false;
    if ( w.soft.owner )
        if ( const char *error = w.soft.prepare( w.soft.owner ) )
        {
            w.error = error;
            w.faulted = w.paused = true;
            ++w.interruption;
            return false;
        }
    w.poseHistoryCopies = 0;
    if ( w.interpolating )
        for ( Body *b : w.active )
        {
            b->binding->previous = b->binding->current;
            ++w.poseHistoryCopies;
        }
    bool entered = true;
    w.policyCalls = 0;
    w.policyL = L;
    w.policyWorldIndex = wi;
    if ( w.soft.owner )
    {
        // The soft client owns this ONE complete native step, including rigid
        // substeps. Never follow it with a second b2World_Step.
        const auto client = w.soft;
        client.retain( client.owner );
        entered = false;
        const char *error = client.advance( client.owner, entered );
        if ( error || !entered )
        {
            w.error = error ? error : "soft step did not enter the rigid world";
            w.faulted = w.paused = true;
            ++w.interruption;
        }
        client.release( client.owner );
    }
    else
        b2World_Step( w.id, w.dt, w.subSteps, NULL, NULL );
    w.policyL = NULL;
    w.policyWorldIndex = 0;
    if ( entered )
        ++w.index;
    if ( entered && w.bindingCount )
    {
        const b2BodyEvents events = b2World_GetBodyEvents( w.id );
        for ( int i = 0; i < events.moveCount; ++i )
        {
            Body *b = (Body *)events.moveEvents[i].userData;
            if ( b && b->binding )
            {
                b->binding->current = ReadPose( w, *b );
                Activate( w, *b );
            }
        }
    }
    if ( w.faulted )
        return false; // No after/contact callbacks for a failed step; no retry.
    // Physical getters are already current; visual poses remain a separate projection.
    return Contacts( L, w, wi ) && StepCallback( L, w, wi, "after" );
}
struct Busy
{
    World &world;
    Busy( World &w ) : world( w )
    {
        w.busy = true;
    }
    ~Busy()
    {
        world.busy = false;
    }
};
int WorldStep( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    if ( w.busy || w.faulted )
        return luaL_error( L, "world is executing or faulted" );
    if ( w.paused )
    {
        lua_pushinteger( L, 0 );
        return 1;
    }
    Busy busy( w );
    RuntimeGuard guard( *LuaContext::GetRuntime( L ) );
    const auto before = w.index;
    UpdateInterpolation( w );
    OneStep( L, w, 1 );
    lua_pushinteger( L, w.index > before ? 1 : 0 );
    return 1;
}
int WorldAdvance( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    if ( w.busy || w.faulted )
        return luaL_error( L, "world is executing or faulted" );
    double interval = LuaContext::GetRuntime( L )->GetFrameInterval();
    if ( !lua_isnoneornil( L, 2 ) )
        interval = (float)Number( L, 2 );
    if ( interval <= 0 || interval > 1 )
        return luaL_error( L, "nominal frame interval must be in (0,1]" );
    w.frameInterval = interval;
    UpdateInterpolation( w );
    Busy busy( w );
    RuntimeGuard guard( *LuaContext::GetRuntime( L ) );
    int count = 0;
    const auto interruption = w.interruption;
    if ( !w.paused )
    {
        double budget = w.budget + interval / w.dt * w.speed;
        if ( !std::isfinite( budget ) || budget > 9007199254740991.0 )
        {
            w.faulted = w.paused = true;
            w.error = "pendingSteps exceeds exact integer range";
        }
        else
            w.budget = budget;
        while ( !w.faulted && w.budget >= 1 && count < w.cap )
        {
            const auto oldIndex = w.index;
            bool ok = OneStep( L, w, 1 );
            if ( w.index > oldIndex )
            {
                ++count;
                if ( w.alive )
                    w.budget -= 1;
            }
            if ( !w.alive )
                break;
            if ( !ok || w.paused || w.interruption != interruption )
                break;
        }
    }
    if ( w.alive )
        SyncViews( L, w, 1, w.paused || w.budget >= 1 ? 1 : w.budget );
    lua_pushinteger( L, count );
    return 1;
}
int WorldSync( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    if ( w.busy )
        return luaL_error( L, "syncViews cannot run during a physics callback" );
    double alpha = lua_isnoneornil( L, 2 ) ? 1 : Number( L, 2 );
    if ( alpha < 0 || alpha > 1 )
        return luaL_error( L, "alpha must be in [0,1]" );
    UpdateInterpolation( w );
    Busy busy( w );
    SyncViews( L, w, 1, alpha );
    return 0;
}
int WorldPause( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    w.paused = true;
    ++w.interruption;
    return 0;
}
int WorldResume( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    if ( w.faulted )
        return luaL_error( L, "faulted world must be destroyed and recreated" );
    w.paused = false;
    return 0;
}
int WorldSpeed( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    double speed = Number( L, 2 );
    if ( speed <= 0 || speed > 64 )
        return luaL_error( L, "speed must be in (0,64]" );
    w.speed = speed;
    if ( !w.busy )
        UpdateInterpolation( w );
    return 0;
}
int WorldInterpolation( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    w.interpolation = ReadInterpolation( L, 2 );
    if ( !w.busy )
        UpdateInterpolation( w );
    return 0;
}
int WorldState( lua_State *L )
{
    World &w = GetWorld( L, 1, false );
    lua_createtable( L, 0, 13 );
    Bool( L, "destroyed", !w.alive );
    Bool( L, "paused", w.paused );
    Bool( L, "faulted", w.faulted );
    Bool( L, "hasSoftSystem", w.soft.owner != nullptr );
    Field( L, "stepIndex", (double)w.index );
    Field( L, "simulationTime", w.index * (double)w.dt );
    Field( L, "timeStep", w.dt );
    Field( L, "subSteps", w.subSteps );
    Field( L, "speed", w.speed );
    Field( L, "bodySlots", w.bodies.size() );
    Field( L, "jointSlots", w.joints.size() );
    Field( L, "shapeSlots", w.shapes.size() );
    Field( L, "chainSlots", w.chains.size() );
    Field( L, "pendingSteps", w.budget );
    Field( L, "activeBindings", w.active.size() );
    Field( L, "viewUpdates", w.lastViewUpdates );
    if ( w.interpolation == Interpolation::Auto )
        lua_pushliteral( L, "auto" );
    else
        lua_pushboolean( L, w.interpolation == Interpolation::On );
    lua_setfield( L, -2, "interpolation" );
    Bool( L, "interpolationActive", w.interpolating );
    Field( L, "viewAlpha", w.viewAlpha );
    Field( L, "poseHistoryCopies", w.poseHistoryCopies );
    Bool( L, "hasContactPolicy", w.hasContactPolicy );
    Field( L, "contactPolicyCalls", w.policyCalls );
    Field( L, "pixelsPerMeter", w.scale );
    // This one native policy is process-global, including the default world.
    Bool( L, "speculativeCornerPassThrough", b2GetSpeculativeCornerPassThrough() );
    if ( w.alive )
    {
        Field( L, "workerCount", b2World_GetWorkerCount( w.id ) );
        Bool( L, "continuous", b2World_IsContinuousEnabled( w.id ) );
        Bool( L, "sleepingEnabled", b2World_IsSleepingEnabled( w.id ) );
        Field( L, "contactRecycleDistance", b2World_GetContactRecycleDistance( w.id ) * w.scale );
        Bool( L, "compoundInternalEdgeSuppression", w.compoundInternalEdgeSuppression );
        Bool( L, "seamContactFilter", b2World_IsSeamContactFilterEnabled( w.id ) );
        Bool( L, "weldRestitution", b2World_IsWeldRestitutionEnabled( w.id ) );
        Field( L, "restitutionIterations", b2World_GetRestitutionIterations( w.id ) );
        float hertz, damping, speed;
        b2World_GetContactTuning( w.id, &hertz, &damping, &speed );
        Field( L, "contactHertz", hertz );
        Field( L, "contactDampingRatio", damping );
        Field( L, "contactPushSpeed", speed * w.scale );
    }
    if ( w.faulted )
    {
        lua_pushstring( L, w.error.c_str() );
        lua_setfield( L, -2, "error" );
    }
    return 1;
}
struct QueryOptions
{
    b2QueryFilter filter = { UINT32_MAX, UINT32_MAX };
    bool includeSensors = true;
    int bodyType = -1;
};
QueryOptions ReadQueryOptions( lua_State *L, int index )
{
    QueryOptions options;
    if ( lua_isnoneornil( L, index ) )
        return options;
    luaL_checktype( L, index, LUA_TTABLE );
    Options( L, index, { "categoryBits", "maskBits", "includeSensors", "bodyType" } );
    options.filter.categoryBits =
        (uint64_t)IntegerOption( L, index, "categoryBits", UINT32_MAX, 0, UINT32_MAX );
    options.filter.maskBits = (uint64_t)IntegerOption( L, index, "maskBits", UINT32_MAX, 0, UINT32_MAX );
    options.includeSensors = Flag( L, index, "includeSensors", true );
    lua_getfield( L, index, "bodyType" );
    if ( !lua_isnil( L, -1 ) )
    {
        const char *type = luaL_checkstring( L, -1 );
        if ( !strcmp( type, "static" ) )
            options.bodyType = b2_staticBody;
        else if ( !strcmp( type, "kinematic" ) )
            options.bodyType = b2_kinematicBody;
        else if ( !strcmp( type, "dynamic" ) )
            options.bodyType = b2_dynamicBody;
        else
            luaL_error( L, "unknown query body type" );
    }
    lua_pop( L, 1 );
    return options;
}
bool QueryAccepts( b2ShapeId id, const QueryOptions &options )
{
    return ( options.includeSensors || !b2Shape_IsSensor( id ) ) &&
           ( options.bodyType < 0 || b2Body_GetType( b2Shape_GetBody( id ) ) == options.bodyType );
}
struct OverlapQuery
{
    QueryOptions options;
    b2Pos point;
    bool exactPoint;
    std::vector<b2ShapeId> shapes;
};
bool CollectOverlap( b2ShapeId id, void *context )
{
    auto &query = *(OverlapQuery *)context;
    if ( QueryAccepts( id, query.options ) && ( !query.exactPoint || b2Shape_TestPoint( id, query.point ) ) )
        query.shapes.push_back( id );
    return true;
}
int WorldOverlap( lua_State *L, bool exactPoint )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    float x = (float)( Number( L, 2 ) / w.scale ), y = (float)( Number( L, 3 ) / w.scale );
    float x2 = exactPoint ? x : (float)( Number( L, 4 ) / w.scale );
    float y2 = exactPoint ? y : (float)( Number( L, 5 ) / w.scale );
    if ( x2 < x || y2 < y )
        return luaL_error( L, "query region bounds must be ordered" );
    QueryOptions options = ReadQueryOptions( L, exactPoint ? 4 : 6 );
    OverlapQuery query = { options, { x, y }, exactPoint, {} };
    // The tree supplies candidates. Only point queries refine to actual geometry;
    // region queries intentionally keep the broadphase AABB contract.
    b2World_OverlapAABB( w.id, b2Pos_zero, { { x, y }, { x2, y2 } }, options.filter, CollectOverlap, &query );
    // Never call Lua inside a native query. Return the existing, retained handles.
    lua_createtable( L, (int)query.shapes.size(), 0 );
    int index = 0;
    for ( b2ShapeId id : query.shapes )
    {
        auto *shape = (Shape *)b2Shape_GetUserData( id );
        PushHandle( L, 1, "shapes", shape->serial );
        lua_rawseti( L, -2, ++index );
    }
    return 1;
}
int WorldQueryPoint( lua_State *L )
{
    return WorldOverlap( L, true );
}
int WorldQueryRegion( lua_State *L )
{
    return WorldOverlap( L, false );
}
struct RayQuery
{
    QueryOptions options;
    b2RayResult result = {};
};
float CollectRay( b2ShapeId id, b2Pos point, b2Vec2 normal, float fraction, void *context )
{
    auto &query = *(RayQuery *)context;
    if ( !QueryAccepts( id, query.options ) )
        return -1;
    if ( !query.result.hit || fraction < query.result.fraction )
    {
        query.result.hit = true;
        query.result.shapeId = id;
        query.result.point = point;
        query.result.normal = normal;
        query.result.fraction = fraction;
    }
    return query.result.fraction;
}
int WorldRay( lua_State *L )
{
    World &w = GetWorld( L );
    if ( w.syncing || b2World_IsLocked( w.id ) )
        return luaL_error( L, "world is locked" );
    float x = (float)( Number( L, 2 ) / w.scale ), y = (float)( Number( L, 3 ) / w.scale );
    b2Vec2 end = { (float)( Number( L, 4 ) / w.scale ), (float)( Number( L, 5 ) / w.scale ) };
    RayQuery query;
    query.options = ReadQueryOptions( L, 6 );
    b2World_CastRay( w.id, { x, y }, { end.x - x, end.y - y }, query.options.filter, CollectRay, &query );
    const b2RayResult &result = query.result;
    if ( !result.hit )
    {
        lua_pushnil( L );
        return 1;
    }
    Body *b = (Body *)b2Body_GetUserData( b2Shape_GetBody( result.shapeId ) );
    lua_createtable( L, 0, 6 );
    PushHandle( L, 1, "bodies", b->serial );
    lua_setfield( L, -2, "body" );
    auto *shape = (Shape *)b2Shape_GetUserData( result.shapeId );
    PushHandle( L, 1, "shapes", shape->serial );
    lua_setfield( L, -2, "shape" );
    Field( L, "element", shape->element );
    Field( L, "x", result.point.x * w.scale );
    Field( L, "y", result.point.y * w.scale );
    Field( L, "normalX", result.normal.x );
    Field( L, "normalY", result.normal.y );
    Field( L, "fraction", result.fraction );
    return 1;
}
void Destroy( lua_State *L, World &w, int wi )
{
    if ( w.soft.owner )
    {
        lua_getfenv( L, wi );
        lua_getfield( L, -1, "softSystem" );
        w.soft.clearViews( L, w.soft.owner, lua_gettop( L ) );
        lua_pop( L, 2 );
        w.soft.invalidate( w.soft.owner );
        w.soft = {};
    }
    for ( auto &b : w.bodies )
        if ( b->binding )
        {
            PushHandle( L, wi, "bodies", b->serial );
            ClearBinding( L, w, *b, -1 );
            lua_pop( L, 1 );
        }
    if ( w.alive )
        b2DestroyWorld( w.id );
    w.alive = false;
    w.id = b2_nullWorldId;
    w.budget = 0;
    ++w.interruption;
    w.bodies.clear();
    w.joints.clear();
    w.shapes.clear();
    w.chains.clear();
    w.compoundEdges.clear();
    w.hasContactPolicy = false;
    w.freeShapes.clear();
    w.freeChains.clear();
    w.active.clear();
    w.freeBodies.clear();
    w.freeJoints.clear();
    lua_newtable( L );
    lua_setfenv( L, wi ); // Release callbacks and retained handles/views.
}
int WorldDestroy( lua_State *L )
{
    World &w = GetWorld( L, 1, false );
    if ( w.alive )
    {
        Mutable( L, w );
        Destroy( L, w, 1 );
    }
    return 0;
}
int WorldGC( lua_State *L )
{
    World **p = (World **)luaL_checkudata( L, 1, kWorld );
    if ( *p )
    {
        Destroy( L, **p, 1 );
        delete *p;
        *p = NULL;
    }
    return 0;
}
const char *ValidateSoftHost( void *owner )
{
    World &w = *(World *)owner;
    return sContactPolicyActive || !w.alive || w.syncing || b2World_IsLocked( w.id )
               ? "physics world is destroyed, locked, or synchronizing views"
               : nullptr;
}
void DetachSoft( void *owner, void *client )
{
    World &w = *(World *)owner;
    if ( w.soft.owner == client )
        w.soft = {};
}
int WorldNewSoft( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    if ( w.soft.owner || w.faulted )
        return luaL_error( L, "world already has a soft system or is faulted" );
    if ( lua_isnoneornil( L, 2 ) )
    {
        lua_settop( L, 1 );
        lua_newtable( L );
    }
    luaL_checktype( L, 2, LUA_TTABLE );
    // Timing/scale and native world are owned by this world, not soft options.
    Options( L, 2,
             { "enableDynamicContacts", "enableSoftContacts", "profile", "maxBodies", "maxJoints",
               "maxTouches", "maxRigidJoints", "maxNodes", "maxSoftContactRows", "maxInternalSprings",
               "maxChainSegments" } );
    SoftSimulationLua::IndependentHost host = { &w,        w.id, w.scale, w.dt, w.subSteps, ValidateSoftHost,
                                                DetachSoft };
    SoftSimulationLua::NewIndependent( L, host, w.soft );
    w.soft.setInterpolation( w.soft.owner, w.interpolating );
    lua_getfenv( L, 1 );
    lua_pushvalue( L, -2 );
    lua_setfield( L, -2, "softSystem" );
    lua_pop( L, 1 );
    return 1;
}
void Metatable( lua_State *L, const char *name, const luaL_Reg *methods )
{
    luaL_newmetatable( L, name );
    luaL_register( L, NULL, methods );
    lua_pushliteral( L, "protected physics handle" );
    lua_setfield( L, -2, "__metatable" );
    lua_pop( L, 1 );
}
} // anonymous namespace

b2BodyId LuaPhysicsWorld::GetBodyId( lua_State *L, int index, b2WorldId owner )
{
    Body &body = GetBody( L, index );
    if ( b2StoreWorldId( b2Body_GetWorld( body.id ) ) != b2StoreWorldId( owner ) )
        luaL_error( L, "rigid body belongs to another physics world" );
    return body.id;
}

int LuaPhysicsWorld::New( lua_State *L )
{
    if ( sContactPolicyActive )
        return luaL_error( L, "contact policy cannot create physics worlds" );
    if ( lua_isnone( L, 1 ) )
        lua_newtable( L );
    else if ( lua_isnil( L, 1 ) )
    {
        lua_newtable( L );
        lua_replace( L, 1 );
    }
    luaL_checktype( L, 1, LUA_TTABLE );
    Options( L, 1,
             { "pixelsPerMeter", "timeStep", "subSteps", "maxStepsPerFrame", "speed", "workerCount", "interpolation",
               "gravityX", "gravityY", "sleepingEnabled", "continuous", "contactRecycleDistance",
               "compoundInternalEdgeSuppression", "seamContactFilter", "weldRestitution",
               "restitutionIterations", "contactHertz", "contactDampingRatio", "contactPushSpeed" } );
    double scale = Option( L, 1, "pixelsPerMeter", 30 ), dt = Option( L, 1, "timeStep", 1.0 / 60 );
    double sub = Option( L, 1, "subSteps", 8 ), cap = Option( L, 1, "maxStepsPerFrame", 8 ),
           speed = Option( L, 1, "speed", 1 );
    lua_getfield( L, 1, "interpolation" );
    Interpolation interpolation = lua_isnil( L, -1 ) ? Interpolation::Auto : ReadInterpolation( L, -1 );
    lua_pop( L, 1 );
    double workers =
        Option( L, 1, "workerCount", LuaContext::GetRuntime( L )->GetPhysicsWorld().GetWorkerCount() );
    if ( scale < 1e-6 || scale > 1e9 || dt <= 0 || dt > 1 || (float)dt <= 0 || sub < 1 || sub > 128 ||
         sub != floor( sub ) || cap < 1 || cap > 1024 || cap != floor( cap ) || speed <= 0 || speed > 64 ||
         workers < 1 || workers > 8 || workers != floor( workers ) )
        return luaL_error( L, "invalid world scale, timestep, subSteps, frame cap, speed or workerCount" );
    if ( !std::isfinite( 1.0f / ( (float)dt / (int)sub ) ) )
        return luaL_error( L, "substep duration must have a finite reciprocal" );
    b2WorldDef def = b2DefaultWorldDef();
    def.gravity = { (float)Option( L, 1, "gravityX", 0 ), (float)Option( L, 1, "gravityY", 9.8 ) };
    def.enableSleep = Flag( L, 1, "sleepingEnabled", true );
    def.enableContinuous = Flag( L, 1, "continuous", true );
    def.workerCount = (int)workers;
    // Keep Box2D's native contact reuse unless the caller explicitly overrides it.
    double recycle = Option( L, 1, "contactRecycleDistance", B2_CONTACT_RECYCLE_DISTANCE * scale );
    if ( recycle < 0 )
        return luaL_error( L, "negative contact recycle distance" );
    bool compound = Flag( L, 1, "compoundInternalEdgeSuppression", false );
    bool seam = Flag( L, 1, "seamContactFilter", false );
    bool weld = Flag( L, 1, "weldRestitution", false );
    double iterations = Option( L, 1, "restitutionIterations", def.restitutionIterations );
    double hertz = Option( L, 1, "contactHertz", def.contactHertz );
    double damping = Option( L, 1, "contactDampingRatio", def.contactDampingRatio );
    double pushSpeed = Option( L, 1, "contactPushSpeed", def.contactSpeed * scale ) / scale;
    if ( iterations < 0 || iterations > B2_MAX_RESTITUTION_ITERATIONS || iterations != floor( iterations ) ||
         hertz < 0 || damping < 0 || pushSpeed < 0 )
        return luaL_error( L, "invalid restitution iterations or contact tuning" );
    def.restitutionIterations = (int)iterations;
    def.contactHertz = (float)hertz;
    def.contactDampingRatio = (float)damping;
    def.contactSpeed = (float)pushSpeed;
    const luaL_Reg bodyMethods[] = { { "__index", BodyIndex },
                                     { "__newindex", BodyNewIndex },
                                     { "addCircle", BodyCircle },
                                     { "addCapsule", BodyCapsule },
                                     { "addBox", BodyBox },
                                     { "addPolygon", BodyPolygon },
                                     { "addChain", BodyChain },
                                     { "getShapes", BodyShapes },
                                     { "getMassData", BodyMass },
                                     { "getPosition", BodyPosition },
                                     { "setTransform", BodyTransform },
                                     { "getLinearVelocity", BodyVelocity },
                                     { "setLinearVelocity", BodySetVelocity },
                                     { "applyForce", BodyForce },
                                     { "applyLinearImpulse", BodyImpulse },
                                     { "applyTorque", BodyTorque },
                                     { "applyAngularImpulse", BodyAngularImpulse },
                                     { "getWorldPoint", BodyWorldPoint },
                                     { "getLocalPoint", BodyLocalPoint },
                                     { "bindView", BodyBind },
                                     { "unbindView", BodyUnbind },
                                     { "destroy", BodyDestroy },
                                     { "removeSelf", BodyDestroy },
                                     { NULL, NULL } };
    const luaL_Reg jointMethods[] = { { "__index", JointIndex },
                                      { "__newindex", JointNewIndex },
                                      { "destroy", JointDestroy },
                                      { "removeSelf", JointDestroy },
                                      { "getLimits", JointGetLimits },
                                      { "setLimits", JointSetLimits },
                                      { "getAnchors", JointAnchors },
                                      { "getReactionForce", JointForce },
                                      { "getReactionTorque", JointTorque },
                                      { "getTarget", JointTarget },
                                      { "setTarget", JointSetTarget },
                                      { NULL, NULL } };
    const luaL_Reg worldMethods[] = { { "__gc", WorldGC },
                                      { "newBody", WorldBody },
                                      { "newJoint", WorldJoint },
                                      { "step", WorldStep },
                                      { "advanceFrame", WorldAdvance },
                                      { "syncViews", WorldSync },
                                      { "pause", WorldPause },
                                      { "resume", WorldResume },
                                      { "setSimulationSpeed", WorldSpeed },
                                      { "setInterpolation", WorldInterpolation },
                                      { "getState", WorldState },
                                      { "newSoftSystem", WorldNewSoft },
                                      { "setContactPolicy", WorldContactPolicy },
                                      { "rayCast", WorldRay },
                                      { "queryPoint", WorldQueryPoint },
                                      { "queryRegion", WorldQueryRegion },
                                      { "destroy", WorldDestroy },
                                      { NULL, NULL } };
    const luaL_Reg shapeMethods[] = { { "__index", ShapeIndex },
                                      { "__newindex", ShapeNewIndex },
                                      { "destroy", ShapeDestroy },
                                      { "removeSelf", ShapeDestroy },
                                      { "isValid", ShapeValid },
                                      { "testPoint", ShapeTestPoint },
                                      { NULL, NULL } };
    const luaL_Reg chainMethods[] = { { "getSegments", ChainSegments }, { "getBody", ChainBody },
                                      { "setFilter", ChainFilter },     { "setMaterial", ChainMaterial },
                                      { "destroy", ChainDestroy },      { "removeSelf", ChainDestroy },
                                      { "isValid", ChainValid },        { NULL, NULL } };
    Metatable( L, kShape, shapeMethods );
    Metatable( L, kChain, chainMethods );
    luaL_getmetatable( L, kChain );
    lua_pushvalue( L, -1 );
    lua_setfield( L, -2, "__index" );
    lua_pop( L, 1 );
    Metatable( L, kBody, bodyMethods );
    Metatable( L, kJoint, jointMethods );
    Metatable( L, kWorld, worldMethods );
    luaL_getmetatable( L, kWorld );
    lua_pushvalue( L, -1 );
    lua_setfield( L, -2, "__index" );
    lua_pushboolean( L, 0 );
    lua_pushcclosure( L, WorldListener, 1 );
    lua_setfield( L, -2, "setStepListener" );
    lua_pushboolean( L, 1 );
    lua_pushcclosure( L, WorldListener, 1 );
    lua_setfield( L, -2, "setCollisionListener" );
    lua_pop( L, 1 );
    World **p = (World **)lua_newuserdata( L, sizeof( World * ) );
    *p = new World;
    World &w = **p;
    luaL_getmetatable( L, kWorld );
    lua_setmetatable( L, -2 );
    lua_newtable( L );
    lua_newtable( L );
    lua_setfield( L, -2, "bodies" );
    lua_newtable( L );
    lua_setfield( L, -2, "joints" );
    lua_newtable( L );
    lua_setfield( L, -2, "shapes" );
    lua_newtable( L );
    lua_setfield( L, -2, "chains" );
    lua_setfenv( L, -2 );
    w.scale = (float)scale;
    w.dt = (float)dt;
    w.subSteps = (int)sub;
    w.cap = (int)cap;
    w.speed = speed;
    w.interpolation = interpolation;
    w.frameInterval = LuaContext::GetRuntime( L )->GetFrameInterval();
    UpdateInterpolation( w );
    w.id = b2CreateWorld( &def );
    if ( !b2World_IsValid( w.id ) )
    {
        w.alive = false;
        return luaL_error( L, "cannot allocate another Box2D world" );
    }
    b2World_SetContactRecycleDistance( w.id, (float)( recycle / scale ) );
    b2World_EnableSeamContactFilter( w.id, seam );
    b2World_EnableWeldRestitution( w.id, weld );
    w.compoundInternalEdgeSuppression = compound;
    RefreshContactPolicy( w );
    return 1;
}
} // namespace Rtt
#endif // Rtt_PHYSICS
