//////////////////////////////////////////////////////////////////////////////
// Private implementation included inside Rtt_LuaPhysicsWorld.cpp's namespace.
// Read-only Lua contact decisions on the thread invoking the native world step.
// This file is part of the Solar2D engine. See the repository LICENSE.
//////////////////////////////////////////////////////////////////////////////
struct ContactPolicyEvent
{
    b2ShapeId a, b;
    b2Pos point;
    b2Vec2 normal;
    float separation = 0, friction = 0, bounce = 0;
    bool continuous = false, enabled = true, material = false;
};
// Run event allocation, Lua, and return validation under one protected call.
// No luaL_error may unwind through Box2D or leave its borrowed contact active.
int InvokeContactPolicy( lua_State *L )
{
    World &w = GetWorld( L );
    auto &decision = *(ContactPolicyEvent *)lua_touserdata( L, 2 );
    lua_getfenv( L, 1 );
    lua_getfield( L, -1, "contactPolicy" );
    lua_remove( L, -2 );
    lua_createtable( L, 0, 18 );
    lua_pushliteral( L, "contactPolicy" );
    lua_setfield( L, -2, "name" );
    lua_pushstring( L, decision.continuous ? "preContinuous" : "preSolve" );
    lua_setfield( L, -2, "phase" );
    lua_pushvalue( L, 1 );
    lua_setfield( L, -2, "world" );
    Field( L, "stepIndex", (double)w.index + 1 );
    Field( L, "simulationTime", (double)w.index * w.dt );
    Field( L, "dt", w.dt );
    Field( L, "x", decision.point.x * w.scale );
    Field( L, "y", decision.point.y * w.scale );
    Field( L, "normalX", decision.normal.x );
    Field( L, "normalY", decision.normal.y );
    Bool( L, "isSensor", false );
    if ( !decision.continuous )
    {
        Field( L, "separation", decision.separation * w.scale );
        Field( L, "friction", decision.friction );
        Field( L, "bounce", decision.bounce );
    }
    auto *a = (Shape *)b2Shape_GetUserData( decision.a );
    auto *b = (Shape *)b2Shape_GetUserData( decision.b );
    PushHandle( L, 1, "bodies", a->body );
    lua_setfield( L, -2, "body1" );
    PushHandle( L, 1, "bodies", b->body );
    lua_setfield( L, -2, "body2" );
    PushHandle( L, 1, "shapes", a->serial );
    lua_setfield( L, -2, "shape1" );
    PushHandle( L, 1, "shapes", b->serial );
    lua_setfield( L, -2, "shape2" );
    Field( L, "element1", a->element );
    Field( L, "element2", b->element );
    lua_call( L, 1, 1 );
    if ( lua_isnil( L, -1 ) )
        return 0;
    luaL_checktype( L, -1, LUA_TTABLE );
    ContactPolicyEvent result = decision;
    int options = lua_gettop( L );
    lua_pushnil( L );
    while ( lua_next( L, options ) )
    {
        if ( lua_type( L, -2 ) != LUA_TSTRING )
            return luaL_error( L, "contact policy keys must be strings" );
        const char *key = lua_tostring( L, -2 );
        if ( !strcmp( key, "isEnabled" ) )
        {
            luaL_checktype( L, -1, LUA_TBOOLEAN );
            result.enabled = lua_toboolean( L, -1 ) != 0;
        }
        else if ( !strcmp( key, "friction" ) || !strcmp( key, "bounce" ) )
        {
            if ( decision.continuous )
                return luaL_error( L, "contact material is only available in preSolve" );
            double value = Number( L, -1 );
            if ( value < 0 )
                return luaL_error( L, "contact material must be nonnegative" );
            if ( !strcmp( key, "friction" ) )
                result.friction = (float)value;
            else
                result.bounce = (float)value;
            result.material = true;
        }
        else
            return luaL_error( L, "unknown contact policy field '%s'", key );
        lua_pop( L, 1 );
    }
    decision = result; // Atomic result: malformed returns leave native defaults.
    return 0;
}
void EvaluateContactPolicy( World &w, ContactPolicyEvent &decision )
{
    if ( !w.hasContactPolicy || !w.policyL || w.faulted )
        return;
    lua_State *L = w.policyL;
    int top = lua_gettop( L );
    lua_pushcfunction( L, InvokeContactPolicy );
    lua_pushvalue( L, w.policyWorldIndex );
    lua_pushlightuserdata( L, &decision );
    ++w.policyCalls;
    sContactPolicyActive = true;
    int status = lua_pcall( L, 2, 0, 0 );
    sContactPolicyActive = false;
    if ( status )
    {
        const char *error = lua_tostring( L, -1 );
        w.error = error ? error : "non-string contact policy error";
        w.faulted = w.paused = true;
        ++w.interruption;
        Rtt_Log( "independent contact policy error: %s\n", w.error.c_str() );
    }
    lua_settop( L, top );
}
bool PolicyPreContinuous( b2ShapeId a, b2ShapeId b, b2Vec2 point, b2Vec2 normal, void *context )
{
    World &w = *(World *)context;
    if ( !w.hasContactPolicy )
        return true;
    ContactPolicyEvent decision;
    decision.a = a;
    decision.b = b;
    decision.point = point;
    decision.normal = normal;
    decision.continuous = true;
    EvaluateContactPolicy( w, decision );
    return decision.enabled;
}
void PolicyPreSolve( b2ShapeId a, b2ShapeId b, b2Manifold *manifold, void *context )
{
    World &w = *(World *)context;
    if ( manifold->pointCount == 0 || !w.hasContactPolicy )
        return;
    int deepest = 0;
    for ( int i = 1; i < manifold->pointCount; ++i )
        if ( manifold->points[i].separation < manifold->points[deepest].separation )
            deepest = i;
    ContactPolicyEvent decision;
    decision.a = a;
    decision.b = b;
    decision.point = b2OffsetPos( b2Body_GetPosition( b2Shape_GetBody( a ) ), manifold->points[deepest].anchorA );
    decision.normal = manifold->normal;
    decision.separation = manifold->points[deepest].separation;
    b2World_GetPreSolveMaterial( w.id, &decision.friction, &decision.bounce );
    EvaluateContactPolicy( w, decision );
    if ( !decision.enabled )
        manifold->pointCount = 0;
    else if ( decision.material )
        b2World_SetPreSolveMaterial( w.id, decision.friction, decision.bounce );
}
void RefreshContactPolicy( World &w )
{
    bool enabled = w.hasContactPolicy;
    b2World_SetPreSolveCallback( w.id, enabled ? PolicyPreSolve : NULL,
                                enabled ? PolicyPreContinuous : NULL, enabled ? &w : NULL );
    b2World_EnableGlobalPreSolveEvents( w.id, w.hasContactPolicy && w.contactPolicyGlobal );
}
int WorldContactPolicy( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    if ( !lua_isnil( L, 2 ) )
        luaL_checktype( L, 2, LUA_TFUNCTION );
    bool globalMode = true;
    if ( !lua_isnoneornil( L, 3 ) )
    {
        luaL_checktype( L, 3, LUA_TTABLE );
        Options( L, 3, { "global" } );
        globalMode = Flag( L, 3, "global", true );
    }
    lua_getfenv( L, 1 );
    lua_pushvalue( L, 2 );
    lua_setfield( L, -2, "contactPolicy" );
    bool hadPolicy = w.hasContactPolicy;
    w.hasContactPolicy = !lua_isnil( L, 2 );
    w.contactPolicyGlobal = w.hasContactPolicy ? globalMode : true;
    RefreshContactPolicy( w );
    // Sleeping contacts must revisit their material/enable decisions after a change.
    if ( hadPolicy || w.hasContactPolicy )
        for ( const auto &body : w.bodies )
            if ( B2_IS_NON_NULL( body->id ) && b2Body_GetType( body->id ) == b2_dynamicBody )
                b2Body_SetAwake( body->id, true );
    return 0;
}
