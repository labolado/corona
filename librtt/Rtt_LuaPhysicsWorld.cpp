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
#include "Rtt_LuaProxy.h"
#include "Rtt_LuaProxyVTable.h"
#include "Display/Rtt_DisplayObject.h"
#include "box2d/box2d.h"
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
const char *kBinding = "_physicsWorldBinding";
const double kRad = 0.017453292519943295;
const size_t kInactive = (size_t)-1;
struct Pose
{
    float x, y, angle;
};
struct Binding
{
    LuaProxy *proxy = NULL; // The proxy userdata is retained even after removeSelf().
    bool interpolate = true;
    Pose previous, current;
};
struct Body
{
    b2BodyId id = b2_nullBodyId;
    size_t serial = 0;
    size_t activeIndex = kInactive;
    unsigned int fixtureCount = 0;
    std::unique_ptr<Binding> binding; // No interpolation state for unbound bodies.
};
struct World
{
    b2WorldId id = b2_nullWorldId;
    bool alive = true, busy = false, syncing = false, paused = false, faulted = false;
    unsigned long long index = 0, interruption = 0;
    float scale = 30, dt = 1.0f / 60;
    int subSteps = 8, cap = 8;
    double speed = 1, budget = 0, renderTime = 0;
    size_t lastViewUpdates = 0, bindingCount = 0;
    std::string error;
    std::vector<std::unique_ptr<Body>> bodies;
    std::vector<b2JointId> joints;
    std::vector<size_t> freeBodies, freeJoints;
    std::vector<Body *> active;
};
struct Handle
{
    World *world;
    size_t index;
    b2BodyId bodyId;
    b2JointId jointId;
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
        b.binding->previous = b.binding->current = ReadPose( w, b );
        Activate( w, b );
    }
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
    bool interpolate = true;
    if ( !lua_isnoneornil( L, 3 ) )
    {
        luaL_checktype( L, 3, LUA_TTABLE );
        Options( L, 3, { "interpolation" } );
        interpolate = Flag( L, 3, "interpolation", true );
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
    b.binding->interpolate = interpolate;
    ResetPose( w, b );
    return 0;
}
void SyncViews( lua_State *L, World &w, int wi, double alpha )
{
    // Visual time never runs backwards after manual sync, cap saturation or pause.
    const double target = w.index ? (double)w.index - 1 + alpha : 0;
    w.renderTime = std::max( w.renderTime, target );
    alpha = w.index ? std::max( 0.0, std::min( 1.0, w.renderTime - ( (double)w.index - 1 ) ) ) : 1;
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
        const Pose &a = binding.previous;
        const Pose &z = binding.current;
        double t = binding.interpolate ? alpha : 1;
        float angleDelta = b2UnwindAngle( z.angle - a.angle );
        // Preserve exact endpoints; interpolation must not introduce subtract/add
        // restoration drift when presenting the completed physical pose.
        double x = t == 1 ? z.x : a.x + ( (double)z.x - a.x ) * t;
        double y = t == 1 ? z.y : a.y + ( (double)z.y - a.y ) * t;
        double angle = t == 1 ? z.angle : a.angle + angleDelta * t;
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
b2ShapeDef ShapeOptions( lua_State *L, int ti, Body &b )
{
    b2ShapeDef d = b2DefaultShapeDef();
    d.density = (float)Option( L, ti, "density", 1 );
    d.material.friction = (float)Option( L, ti, "friction", 0.3 );
    d.material.restitution = (float)Option( L, ti, "bounce", 0 );
    if ( d.density < 0 || d.material.friction < 0 || d.material.restitution < 0 ||
         d.material.restitution > 1 )
        luaL_error( L, "invalid density, friction or bounce" );
    d.isSensor = Flag( L, ti, "isSensor", false );
    d.enableContactEvents = true;
    d.enableSensorEvents = true;
    d.userData = (void *)(uintptr_t)( b.fixtureCount + 1 );
    return d;
}
int BodyCircle( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2, { "radius", "x", "y", "density", "friction", "bounce", "isSensor" } );
    b2ShapeDef d = ShapeOptions( L, 2, b );
    float r = (float)( Option( L, 2, "radius", 0 ) / w.scale );
    if ( r <= 0 )
        return luaL_error( L, "radius must be positive" );
    b2Circle c = {
        { (float)( Option( L, 2, "x", 0 ) / w.scale ), (float)( Option( L, 2, "y", 0 ) / w.scale ) }, r };
    b2CreateCircleShape( b.id, &d, &c );
    ++b.fixtureCount;
    lua_pushvalue( L, 1 );
    return 1;
}
int BodyBox( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2,
             { "halfWidth", "halfHeight", "x", "y", "angle", "density", "friction", "bounce", "isSensor" } );
    b2ShapeDef d = ShapeOptions( L, 2, b );
    float hx = (float)( Option( L, 2, "halfWidth", 0 ) / w.scale ),
          hy = (float)( Option( L, 2, "halfHeight", 0 ) / w.scale );
    if ( hx <= 0 || hy <= 0 )
        return luaL_error( L, "box half extents must be positive" );
    b2Vec2 center = { (float)( Option( L, 2, "x", 0 ) / w.scale ),
                      (float)( Option( L, 2, "y", 0 ) / w.scale ) };
    b2Polygon p =
        b2MakeOffsetBox( hx, hy, center, b2MakeRot( (float)( Option( L, 2, "angle", 0 ) * kRad ) ) );
    b2CreatePolygonShape( b.id, &d, &p );
    ++b.fixtureCount;
    lua_pushvalue( L, 1 );
    return 1;
}
int BodyPolygon( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    luaL_checktype( L, 2, LUA_TTABLE );
    Options( L, 2, { "shape", "density", "friction", "bounce", "isSensor" } );
    b2ShapeDef d = ShapeOptions( L, 2, b );
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
    b2Polygon polygon = b2MakePolygon( &hull, 0 );
    b2CreatePolygonShape( b.id, &d, &polygon );
    ++b.fixtureCount;
    lua_pushvalue( L, 1 );
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
int JointDestroy( lua_State *L )
{
    b2JointId id = GetJoint( L );
    Handle &h = GetHandle( L, 1, kJoint );
    Mutable( L, *h.world );
    PushWorld( L, 1 );
    ReleaseJoint( L, *h.world, lua_gettop( L ), id );
    b2DestroyJoint( id );
    return 0;
}
int JointIndex( lua_State *L )
{
    b2JointId j = GetJoint( L );
    const char *key = luaL_checkstring( L, 2 );
    if ( !strcmp( key, "jointAngle" ) )
        lua_pushnumber( L, b2RevoluteJoint_GetCurrentAngle( j ) / kRad );
    else if ( !strcmp( key, "motorSpeed" ) )
        lua_pushnumber( L, b2RevoluteJoint_GetTargetMotorSpeed( j ) / kRad );
    else if ( !strcmp( key, "maxMotorTorque" ) )
        lua_pushnumber( L, b2RevoluteJoint_GetMaxMotorTorque( j ) );
    else if ( !strcmp( key, "isMotorEnabled" ) )
        lua_pushboolean( L, b2RevoluteJoint_IsMotorEnabled( j ) );
    else
    {
        luaL_getmetatable( L, kJoint );
        lua_pushvalue( L, 2 );
        lua_rawget( L, -2 );
    }
    return 1;
}
int JointNewIndex( lua_State *L )
{
    b2JointId j = GetJoint( L );
    Mutable( L, Owner( L, 1, kJoint ) );
    const char *key = luaL_checkstring( L, 2 );
    if ( !strcmp( key, "motorSpeed" ) )
        b2RevoluteJoint_SetTargetMotorSpeed( j, (float)( Number( L, 3 ) * kRad ) );
    else if ( !strcmp( key, "maxMotorTorque" ) )
    {
        float v = (float)Number( L, 3 );
        if ( v < 0 )
            return luaL_error( L, "negative motor torque" );
        b2RevoluteJoint_SetMaxMotorTorque( j, v );
    }
    else if ( !strcmp( key, "isMotorEnabled" ) )
    {
        luaL_checktype( L, 3, LUA_TBOOLEAN );
        b2RevoluteJoint_EnableMotor( j, lua_toboolean( L, 3 ) != 0 );
    }
    else
        return luaL_error( L, "unknown or read-only joint property" );
    return 0;
}
int WorldJoint( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    const char *type = luaL_checkstring( L, 2 );
    if ( strcmp( type, "pivot" ) )
        return luaL_error( L, "only pivot joints are supported" );
    Body &a = GetBody( L, 3 );
    Body &b = GetBody( L, 4 );
    if ( &Owner( L, 3, kBody ) != &w || &Owner( L, 4, kBody ) != &w || &a == &b )
        return luaL_error( L, "joint bodies must be distinct and belong to this world" );
    b2Pos anchor = { (float)( Number( L, 5 ) / w.scale ), (float)( Number( L, 6 ) / w.scale ) };
    b2RevoluteJointDef d = b2DefaultRevoluteJointDef();
    d.base.bodyIdA = a.id;
    d.base.bodyIdB = b.id;
    d.base.localFrameA.p = b2Body_GetLocalPoint( a.id, anchor );
    d.base.localFrameB.p = b2Body_GetLocalPoint( b.id, anchor );
    d.base.localFrameA.q = b2Rot_identity;
    d.base.localFrameB.q = b2InvMulRot( b2Body_GetRotation( b.id ), b2Body_GetRotation( a.id ) );
    size_t index;
    if ( w.freeJoints.empty() )
    {
        index = w.joints.size();
        w.joints.push_back( b2_nullJointId );
    }
    else
    {
        index = w.freeJoints.back();
        w.freeJoints.pop_back();
    }
    d.base.userData = (void *)(uintptr_t)( index + 1 );
    w.joints[index] = b2CreateRevoluteJoint( w.id, &d );
    Handle *h = (Handle *)lua_newuserdata( L, sizeof( Handle ) );
    *h = { &w, index, b2_nullBodyId, w.joints[index] };
    luaL_getmetatable( L, kJoint );
    lua_setmetatable( L, -2 );
    StoreHandle( L, 1, "joints", index );
    return 1;
}
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
        lua_pushstring( L, e.began ? "began" : "ended" );
        lua_setfield( L, -2, "phase" );
        Bool( L, "isSensor", e.sensor );
        Field( L, "stepIndex", (double)w.index );
        Field( L, "simulationTime", w.index * (double)w.dt );
        PushHandle( L, wi, "bodies", a->serial );
        lua_setfield( L, -2, "body1" );
        PushHandle( L, wi, "bodies", b->serial );
        lua_setfield( L, -2, "body2" );
        Field( L, "element1", (double)(uintptr_t)b2Shape_GetUserData( e.a ) );
        Field( L, "element2", (double)(uintptr_t)b2Shape_GetUserData( e.b ) );
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
    for ( Body *b : w.active )
        b->binding->previous = b->binding->current;
    b2World_Step( w.id, w.dt, w.subSteps, NULL, NULL );
    ++w.index;
    if ( w.bindingCount )
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
    return 0;
}
int WorldState( lua_State *L )
{
    World &w = GetWorld( L, 1, false );
    lua_createtable( L, 0, 13 );
    Bool( L, "destroyed", !w.alive );
    Bool( L, "paused", w.paused );
    Bool( L, "faulted", w.faulted );
    Field( L, "stepIndex", (double)w.index );
    Field( L, "simulationTime", w.index * (double)w.dt );
    Field( L, "timeStep", w.dt );
    Field( L, "subSteps", w.subSteps );
    Field( L, "speed", w.speed );
    Field( L, "bodySlots", w.bodies.size() );
    Field( L, "jointSlots", w.joints.size() );
    Field( L, "pendingSteps", w.budget );
    Field( L, "activeBindings", w.active.size() );
    Field( L, "viewUpdates", w.lastViewUpdates );
    Field( L, "pixelsPerMeter", w.scale );
    if ( w.faulted )
    {
        lua_pushstring( L, w.error.c_str() );
        lua_setfield( L, -2, "error" );
    }
    return 1;
}
int WorldRay( lua_State *L )
{
    World &w = GetWorld( L );
    if ( w.syncing || b2World_IsLocked( w.id ) )
        return luaL_error( L, "world is locked" );
    float x = (float)( Number( L, 2 ) / w.scale ), y = (float)( Number( L, 3 ) / w.scale );
    b2Vec2 end = { (float)( Number( L, 4 ) / w.scale ), (float)( Number( L, 5 ) / w.scale ) };
    b2RayResult result =
        b2World_CastRayClosest( w.id, { x, y }, { end.x - x, end.y - y }, b2DefaultQueryFilter() );
    if ( !result.hit )
    {
        lua_pushnil( L );
        return 1;
    }
    Body *b = (Body *)b2Body_GetUserData( b2Shape_GetBody( result.shapeId ) );
    lua_createtable( L, 0, 6 );
    PushHandle( L, 1, "bodies", b->serial );
    lua_setfield( L, -2, "body" );
    Field( L, "x", result.point.x * w.scale );
    Field( L, "y", result.point.y * w.scale );
    Field( L, "normalX", result.normal.x );
    Field( L, "normalY", result.normal.y );
    Field( L, "fraction", result.fraction );
    return 1;
}
void Destroy( lua_State *L, World &w, int wi )
{
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
void Metatable( lua_State *L, const char *name, const luaL_Reg *methods )
{
    luaL_newmetatable( L, name );
    luaL_register( L, NULL, methods );
    lua_pushliteral( L, "protected physics handle" );
    lua_setfield( L, -2, "__metatable" );
    lua_pop( L, 1 );
}
} // anonymous namespace

int LuaPhysicsWorld::New( lua_State *L )
{
    if ( lua_isnone( L, 1 ) )
        lua_newtable( L );
    else if ( lua_isnil( L, 1 ) )
    {
        lua_newtable( L );
        lua_replace( L, 1 );
    }
    luaL_checktype( L, 1, LUA_TTABLE );
    Options( L, 1,
             { "pixelsPerMeter", "timeStep", "subSteps", "maxStepsPerFrame", "speed", "workerCount",
               "gravityX", "gravityY", "sleepingEnabled", "continuous", "contactRecycleDistance" } );
    double scale = Option( L, 1, "pixelsPerMeter", 30 ), dt = Option( L, 1, "timeStep", 1.0 / 60 );
    double sub = Option( L, 1, "subSteps", 8 ), cap = Option( L, 1, "maxStepsPerFrame", 8 ),
           speed = Option( L, 1, "speed", 1 );
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
    double recycle = Option( L, 1, "contactRecycleDistance", 0 );
    if ( recycle < 0 )
        return luaL_error( L, "negative contact recycle distance" );
    const luaL_Reg bodyMethods[] = { { "__index", BodyIndex },
                                     { "__newindex", BodyNewIndex },
                                     { "addCircle", BodyCircle },
                                     { "addBox", BodyBox },
                                     { "addPolygon", BodyPolygon },
                                     { "getPosition", BodyPosition },
                                     { "setTransform", BodyTransform },
                                     { "getLinearVelocity", BodyVelocity },
                                     { "setLinearVelocity", BodySetVelocity },
                                     { "applyForce", BodyForce },
                                     { "applyLinearImpulse", BodyImpulse },
                                     { "bindView", BodyBind },
                                     { "unbindView", BodyUnbind },
                                     { "destroy", BodyDestroy },
                                     { "removeSelf", BodyDestroy },
                                     { NULL, NULL } };
    const luaL_Reg jointMethods[] = { { "__index", JointIndex },
                                      { "__newindex", JointNewIndex },
                                      { "destroy", JointDestroy },
                                      { "removeSelf", JointDestroy },
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
                                      { "getState", WorldState },
                                      { "rayCast", WorldRay },
                                      { "destroy", WorldDestroy },
                                      { NULL, NULL } };
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
    lua_setfenv( L, -2 );
    w.scale = (float)scale;
    w.dt = (float)dt;
    w.subSteps = (int)sub;
    w.cap = (int)cap;
    w.speed = speed;
    w.id = b2CreateWorld( &def );
    if ( !b2World_IsValid( w.id ) )
    {
        w.alive = false;
        return luaL_error( L, "cannot allocate another Box2D world" );
    }
    b2World_SetContactRecycleDistance( w.id, (float)( recycle / scale ) );
    return 1;
}
} // namespace Rtt
#endif // Rtt_PHYSICS
