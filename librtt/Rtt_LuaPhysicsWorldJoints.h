//////////////////////////////////////////////////////////////////////////////
// Private implementation included inside Rtt_LuaPhysicsWorld.cpp's namespace.
// Joint dispatch and Lua units only; all constraints use the Box2D C API.
// This file is part of the Solar2D engine. See the repository LICENSE.
//////////////////////////////////////////////////////////////////////////////
enum class JointUnit
{
    SI,
    Pixels,
    Degrees
};
enum class JointRange
{
    Signed,
    Nonnegative,
    Length,
    Angle
};
struct JointNumberProperty
{
    const char *name;
    b2JointType type;
    float ( *get )( b2JointId );
    void ( *set )( b2JointId, float );
    JointUnit unit;
    JointRange range;
};
struct JointFlagProperty
{
    const char *name;
    b2JointType type;
    bool ( *get )( b2JointId );
    void ( *set )( b2JointId, bool );
};
float ConstraintFrequency( b2JointId id )
{
    float hertz, damping;
    b2Joint_GetConstraintTuning( id, &hertz, &damping );
    return hertz;
}
float ConstraintDamping( b2JointId id )
{
    float hertz, damping;
    b2Joint_GetConstraintTuning( id, &hertz, &damping );
    return damping;
}
void SetConstraintFrequency( b2JointId id, float value )
{
    b2Joint_SetConstraintTuning( id, value, ConstraintDamping( id ) );
}
void SetConstraintDamping( b2JointId id, float value )
{
    b2Joint_SetConstraintTuning( id, ConstraintFrequency( id ), value );
}
// These tune constraint correction, not a weld's optional soft spring.
const JointNumberProperty kConstraintNumbers[] = {
    { "constraintFrequency", b2_weldJoint, ConstraintFrequency, SetConstraintFrequency,
      JointUnit::SI, JointRange::Nonnegative },
    { "constraintDampingRatio", b2_weldJoint, ConstraintDamping, SetConstraintDamping,
      JointUnit::SI, JointRange::Nonnegative },
};
const JointNumberProperty kJointNumbers[] = {
    { "motorSpeed", b2_revoluteJoint, b2RevoluteJoint_GetTargetMotorSpeed,
      b2RevoluteJoint_SetTargetMotorSpeed, JointUnit::Degrees, JointRange::Signed },
    { "maxMotorTorque", b2_revoluteJoint, b2RevoluteJoint_GetMaxMotorTorque,
      b2RevoluteJoint_SetMaxMotorTorque, JointUnit::SI, JointRange::Nonnegative },
    { "springFrequency", b2_revoluteJoint, b2RevoluteJoint_GetSpringHertz, b2RevoluteJoint_SetSpringHertz,
      JointUnit::SI, JointRange::Nonnegative },
    { "springDampingRatio", b2_revoluteJoint, b2RevoluteJoint_GetSpringDampingRatio,
      b2RevoluteJoint_SetSpringDampingRatio, JointUnit::SI, JointRange::Nonnegative },
    { "springTargetAngle", b2_revoluteJoint, b2RevoluteJoint_GetTargetAngle, b2RevoluteJoint_SetTargetAngle,
      JointUnit::Degrees, JointRange::Angle },
    { "jointAngle", b2_revoluteJoint, b2RevoluteJoint_GetCurrentAngle, NULL, JointUnit::Degrees,
      JointRange::Signed },
    { "length", b2_distanceJoint, b2DistanceJoint_GetRestLength, b2DistanceJoint_SetRestLength,
      JointUnit::Pixels, JointRange::Length },
    { "frequency", b2_distanceJoint, b2DistanceJoint_GetSpringHertz, b2DistanceJoint_SetSpringHertz,
      JointUnit::SI, JointRange::Nonnegative },
    { "dampingRatio", b2_distanceJoint, b2DistanceJoint_GetSpringDampingRatio,
      b2DistanceJoint_SetSpringDampingRatio, JointUnit::SI, JointRange::Nonnegative },
    { "motorSpeed", b2_distanceJoint, b2DistanceJoint_GetTargetMotorSpeed,
      b2DistanceJoint_SetTargetMotorSpeed, JointUnit::Pixels, JointRange::Signed },
    { "maxMotorForce", b2_distanceJoint, b2DistanceJoint_GetMaxMotorForce, b2DistanceJoint_SetMaxMotorForce,
      JointUnit::SI, JointRange::Nonnegative },
    { "currentLength", b2_distanceJoint, b2DistanceJoint_GetCurrentLength, NULL, JointUnit::Pixels,
      JointRange::Signed },
    { "linearFrequency", b2_weldJoint, b2WeldJoint_GetLinearHertz, b2WeldJoint_SetLinearHertz, JointUnit::SI,
      JointRange::Nonnegative },
    { "angularFrequency", b2_weldJoint, b2WeldJoint_GetAngularHertz, b2WeldJoint_SetAngularHertz,
      JointUnit::SI, JointRange::Nonnegative },
    { "linearDampingRatio", b2_weldJoint, b2WeldJoint_GetLinearDampingRatio,
      b2WeldJoint_SetLinearDampingRatio, JointUnit::SI, JointRange::Nonnegative },
    { "angularDampingRatio", b2_weldJoint, b2WeldJoint_GetAngularDampingRatio,
      b2WeldJoint_SetAngularDampingRatio, JointUnit::SI, JointRange::Nonnegative },
    { "motorSpeed", b2_wheelJoint, b2WheelJoint_GetTargetMotorSpeed, b2WheelJoint_SetTargetMotorSpeed,
      JointUnit::Degrees, JointRange::Signed },
    { "maxMotorTorque", b2_wheelJoint, b2WheelJoint_GetMaxMotorTorque, b2WheelJoint_SetMaxMotorTorque,
      JointUnit::SI, JointRange::Nonnegative },
    { "frequency", b2_wheelJoint, b2WheelJoint_GetSpringHertz, b2WheelJoint_SetSpringHertz, JointUnit::SI,
      JointRange::Nonnegative },
    { "dampingRatio", b2_wheelJoint, b2WheelJoint_GetSpringDampingRatio, b2WheelJoint_SetSpringDampingRatio,
      JointUnit::SI, JointRange::Nonnegative },
    { "jointTranslation", b2_wheelJoint, b2WheelJoint_GetJointTranslation, NULL, JointUnit::Pixels,
      JointRange::Signed },
    { "motorSpeed", b2_prismaticJoint, b2PrismaticJoint_GetTargetMotorSpeed,
      b2PrismaticJoint_SetTargetMotorSpeed, JointUnit::Pixels, JointRange::Signed },
    { "maxMotorForce", b2_prismaticJoint, b2PrismaticJoint_GetMaxMotorForce,
      b2PrismaticJoint_SetMaxMotorForce, JointUnit::SI, JointRange::Nonnegative },
    { "jointTranslation", b2_prismaticJoint, b2PrismaticJoint_GetCurrentTranslation, NULL,
      JointUnit::Pixels, JointRange::Signed },
    { "jointSpeed", b2_prismaticJoint, b2PrismaticJoint_GetCurrentSpeed, NULL, JointUnit::Pixels,
      JointRange::Signed },
    { "frequency", b2_motorJoint, b2MotorJoint_GetLinearHertz, b2MotorJoint_SetLinearHertz, JointUnit::SI,
      JointRange::Nonnegative },
    { "dampingRatio", b2_motorJoint, b2MotorJoint_GetLinearDampingRatio, b2MotorJoint_SetLinearDampingRatio,
      JointUnit::SI, JointRange::Nonnegative },
    { "maxForce", b2_motorJoint, b2MotorJoint_GetMaxSpringForce, b2MotorJoint_SetMaxSpringForce,
      JointUnit::SI, JointRange::Nonnegative },
};
const JointFlagProperty kJointFlags[] = {
    { "blockSolve", b2_weldJoint, b2WeldJoint_IsBlockSolveEnabled, b2WeldJoint_EnableBlockSolve },
    { "isMotorEnabled", b2_revoluteJoint, b2RevoluteJoint_IsMotorEnabled, b2RevoluteJoint_EnableMotor },
    { "isLimitEnabled", b2_revoluteJoint, b2RevoluteJoint_IsLimitEnabled, b2RevoluteJoint_EnableLimit },
    { "isSpringEnabled", b2_revoluteJoint, b2RevoluteJoint_IsSpringEnabled, b2RevoluteJoint_EnableSpring },
    { "isMotorEnabled", b2_distanceJoint, b2DistanceJoint_IsMotorEnabled, b2DistanceJoint_EnableMotor },
    { "isLimitEnabled", b2_distanceJoint, b2DistanceJoint_IsLimitEnabled, b2DistanceJoint_EnableLimit },
    { "isSpringEnabled", b2_distanceJoint, b2DistanceJoint_IsSpringEnabled, b2DistanceJoint_EnableSpring },
    { "isMotorEnabled", b2_wheelJoint, b2WheelJoint_IsMotorEnabled, b2WheelJoint_EnableMotor },
    { "isLimitEnabled", b2_wheelJoint, b2WheelJoint_IsLimitEnabled, b2WheelJoint_EnableLimit },
    { "isSpringEnabled", b2_wheelJoint, b2WheelJoint_IsSpringEnabled, b2WheelJoint_EnableSpring },
    { "isMotorEnabled", b2_prismaticJoint, b2PrismaticJoint_IsMotorEnabled, b2PrismaticJoint_EnableMotor },
    { "isLimitEnabled", b2_prismaticJoint, b2PrismaticJoint_IsLimitEnabled, b2PrismaticJoint_EnableLimit },
};
const JointNumberProperty *JointNumber( b2JointType type, const char *key )
{
    if ( type != b2_filterJoint )
        for ( const auto &p : kConstraintNumbers )
            if ( !strcmp( p.name, key ) )
                return &p;
    for ( const auto &p : kJointNumbers )
        if ( p.type == type && !strcmp( p.name, key ) )
            return &p;
    return NULL;
}
const JointFlagProperty *JointFlag( b2JointType type, const char *key )
{
    for ( const auto &p : kJointFlags )
        if ( p.type == type && !strcmp( p.name, key ) )
            return &p;
    return NULL;
}
double JointScale( const World &w, JointUnit unit )
{
    return unit == JointUnit::Pixels ? 1.0 / w.scale : unit == JointUnit::Degrees ? kRad : 1;
}
float ReadJointNumber( lua_State *L, int i, const World &w, const JointNumberProperty &p )
{
    double v = Number( L, i ) * JointScale( w, p.unit );
    if ( ( p.range == JointRange::Nonnegative && v < 0 ) ||
         ( p.range == JointRange::Length && ( v < B2_LINEAR_SLOP || v > B2_HUGE ) ) ||
         ( p.range == JointRange::Angle && std::abs( v ) > 180 * kRad ) )
        luaL_error( L, "joint property '%s' is outside the supported range", p.name );
    return (float)v;
}
JointUnit LimitUnit( lua_State *L, b2JointType type )
{
    if ( type == b2_revoluteJoint )
        return JointUnit::Degrees;
    if ( type == b2_distanceJoint || type == b2_wheelJoint || type == b2_prismaticJoint )
        return JointUnit::Pixels;
    luaL_error( L, "this joint has no limits" );
    return JointUnit::SI;
}
void CheckJointLimits( lua_State *L, b2JointType type, double lower, double upper )
{
    if ( lower > upper || ( type == b2_revoluteJoint && ( lower < -178.2 * kRad || upper > 178.2 * kRad ) ) ||
         ( type == b2_distanceJoint && ( lower < B2_LINEAR_SLOP || upper > B2_HUGE ) ) )
        luaL_error( L, "invalid joint limits" );
}
void SetJointLimits( b2JointId id, b2JointType type, float lower, float upper )
{
    if ( type == b2_revoluteJoint )
        b2RevoluteJoint_SetLimits( id, lower, upper );
    else if ( type == b2_distanceJoint )
        b2DistanceJoint_SetLimits( id, lower, upper );
    else if ( type == b2_prismaticJoint )
        b2PrismaticJoint_SetLimits( id, lower, upper );
    else
        b2WheelJoint_SetLimits( id, lower, upper );
}
int JointGetLimits( lua_State *L )
{
    b2JointId id = GetJoint( L );
    b2JointType type = b2Joint_GetType( id );
    double scale = JointScale( Owner( L, 1, kJoint ), LimitUnit( L, type ) );
    float lower, upper;
    if ( type == b2_revoluteJoint )
    {
        lower = b2RevoluteJoint_GetLowerLimit( id );
        upper = b2RevoluteJoint_GetUpperLimit( id );
    }
    else if ( type == b2_distanceJoint )
    {
        lower = b2DistanceJoint_GetLowerLimit( id );
        upper = b2DistanceJoint_GetUpperLimit( id );
    }
    else if ( type == b2_prismaticJoint )
    {
        lower = b2PrismaticJoint_GetLowerLimit( id );
        upper = b2PrismaticJoint_GetUpperLimit( id );
    }
    else
    {
        lower = b2WheelJoint_GetLowerLimit( id );
        upper = b2WheelJoint_GetUpperLimit( id );
    }
    lua_pushnumber( L, lower / scale );
    lua_pushnumber( L, upper / scale );
    return 2;
}
int JointSetLimits( lua_State *L )
{
    b2JointId id = GetJoint( L );
    World &w = Owner( L, 1, kJoint );
    Mutable( L, w );
    b2JointType type = b2Joint_GetType( id );
    double scale = JointScale( w, LimitUnit( L, type ) );
    double lower = Number( L, 2 ) * scale, upper = Number( L, 3 ) * scale;
    CheckJointLimits( L, type, lower, upper );
    SetJointLimits( id, type, (float)lower, (float)upper );
    b2Joint_WakeBodies( id );
    return 0;
}
int JointDestroy( lua_State *L )
{
    b2JointId id = GetJoint( L );
    World &w = Owner( L, 1, kJoint );
    Mutable( L, w );
    PushWorld( L, 1 );
    ReleaseJoint( L, w, lua_gettop( L ), id );
    b2DestroyJoint( id );
    return 0;
}
int JointAnchors( lua_State *L )
{
    b2JointId id = GetJoint( L );
    World &w = Owner( L, 1, kJoint );
    b2Pos a = b2Body_GetWorldPoint( b2Joint_GetBodyA( id ), b2Joint_GetLocalFrameA( id ).p );
    b2Pos b = b2Body_GetWorldPoint( b2Joint_GetBodyB( id ), b2Joint_GetLocalFrameB( id ).p );
    lua_pushnumber( L, a.x * w.scale );
    lua_pushnumber( L, a.y * w.scale );
    lua_pushnumber( L, b.x * w.scale );
    lua_pushnumber( L, b.y * w.scale );
    return 4;
}
int JointForce( lua_State *L )
{
    b2Vec2 force = b2Joint_GetConstraintForce( GetJoint( L ) );
    lua_pushnumber( L, force.x );
    lua_pushnumber( L, force.y );
    return 2;
}
int JointTorque( lua_State *L )
{
    lua_pushnumber( L, b2Joint_GetConstraintTorque( GetJoint( L ) ) );
    return 1;
}
b2JointId GetTouch( lua_State *L )
{
    b2JointId id = GetJoint( L );
    if ( b2Joint_GetType( id ) != b2_motorJoint )
        luaL_error( L, "target is only supported by touch joints" );
    return id;
}
int JointTarget( lua_State *L )
{
    b2Transform frame = b2Joint_GetLocalFrameA( GetTouch( L ) );
    World &w = Owner( L, 1, kJoint );
    lua_pushnumber( L, frame.p.x * w.scale );
    lua_pushnumber( L, frame.p.y * w.scale );
    return 2;
}
int JointSetTarget( lua_State *L )
{
    b2JointId id = GetTouch( L );
    World &w = Owner( L, 1, kJoint );
    Mutable( L, w );
    b2Transform frame = b2Joint_GetLocalFrameA( id );
    frame.p = { (float)( Number( L, 2 ) / w.scale ), (float)( Number( L, 3 ) / w.scale ) };
    b2Joint_SetLocalFrameA( id, frame );
    b2Joint_WakeBodies( id );
    return 0;
}
int JointIndex( lua_State *L )
{
    b2JointId id = GetJoint( L );
    World &w = Owner( L, 1, kJoint );
    b2JointType type = b2Joint_GetType( id );
    const char *key = luaL_checkstring( L, 2 );
    const auto *number = JointNumber( type, key );
    const auto *flag = JointFlag( type, key );
    if ( number )
        lua_pushnumber( L, number->get( id ) / JointScale( w, number->unit ) );
    else if ( flag )
        lua_pushboolean( L, flag->get( id ) );
    else if ( !strcmp( key, "collideConnected" ) )
        lua_pushboolean( L, b2Joint_GetCollideConnected( id ) );
    else if ( !strcmp( key, "type" ) )
        lua_pushstring( L, type == b2_revoluteJoint    ? "pivot"
                           : type == b2_weldJoint      ? "weld"
                           : type == b2_distanceJoint  ? "distance"
                           : type == b2_wheelJoint     ? "wheel"
                           : type == b2_prismaticJoint ? "piston"
                           : type == b2_filterJoint    ? "fake"
                                                       : "touch" );
    else if ( !strcmp( key, "referenceAngle" ) &&
              ( type == b2_revoluteJoint || type == b2_weldJoint || type == b2_prismaticJoint ) )
        lua_pushnumber( L, b2RelativeAngle( b2Joint_GetLocalFrameB( id ).q, b2Joint_GetLocalFrameA( id ).q ) /
                               kRad );
    else if ( !strcmp( key, "bodyA" ) || !strcmp( key, "bodyB" ) )
    {
        b2BodyId bodyId = !strcmp( key, "bodyA" ) ? b2Joint_GetBodyA( id ) : b2Joint_GetBodyB( id );
        Body *body = (Body *)b2Body_GetUserData( bodyId );
        if ( body )
        {
            PushWorld( L, 1 );
            PushHandle( L, lua_gettop( L ), "bodies", body->serial );
        }
        else
            lua_pushnil( L ); // The private touch anchor is not a public body.
    }
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
    b2JointId id = GetJoint( L );
    World &w = Owner( L, 1, kJoint );
    Mutable( L, w );
    b2JointType type = b2Joint_GetType( id );
    const char *key = luaL_checkstring( L, 2 );
    const auto *number = JointNumber( type, key );
    const auto *flag = JointFlag( type, key );
    if ( number && number->set )
        number->set( id, ReadJointNumber( L, 3, w, *number ) );
    else if ( flag || !strcmp( key, "collideConnected" ) )
    {
        luaL_checktype( L, 3, LUA_TBOOLEAN );
        bool value = lua_toboolean( L, 3 ) != 0;
        if ( flag )
            flag->set( id, value );
        else
            b2Joint_SetCollideConnected( id, value );
    }
    else
        return luaL_error( L, "unknown or read-only joint property '%s'", key );
    b2Joint_WakeBodies( id );
    return 0;
}
// Validate the complete definition before allocating any native body/joint or
// handle slot. Iterate raw fields, so __index cannot change between validation
// and application. Limits are a pair to avoid partial updates.
void ReadJointOptions( lua_State *L, int options, World &w, b2JointType type, bool &hasLimits, float &lower,
                       float &upper )
{
    if ( lua_isnoneornil( L, options ) )
        return;
    luaL_checktype( L, options, LUA_TTABLE );
    bool hasLower = false, hasUpper = false;
    double low = 0, high = 0;
    lua_pushnil( L );
    while ( lua_next( L, options ) )
    {
        if ( lua_type( L, -2 ) != LUA_TSTRING )
            luaL_error( L, "joint option keys must be strings" );
        const char *key = lua_tostring( L, -2 );
        const auto *number = JointNumber( type, key );
        const auto *flag = JointFlag( type, key );
        if ( number && number->set )
            ReadJointNumber( L, -1, w, *number );
        else if ( flag || !strcmp( key, "collideConnected" ) )
            luaL_checktype( L, -1, LUA_TBOOLEAN );
        else if ( !strcmp( key, "lowerLimit" ) || !strcmp( key, "upperLimit" ) )
        {
            double value = Number( L, -1 ) * JointScale( w, LimitUnit( L, type ) );
            if ( !strcmp( key, "lowerLimit" ) )
            {
                hasLower = true;
                low = value;
            }
            else
            {
                hasUpper = true;
                high = value;
            }
        }
        else
            luaL_error( L, "unsupported joint option '%s'", key );
        lua_pop( L, 1 );
    }
    if ( hasLower != hasUpper )
        luaL_error( L, "lowerLimit and upperLimit must be supplied together" );
    if ( hasLower )
    {
        CheckJointLimits( L, type, low, high );
        hasLimits = true;
        lower = (float)low;
        upper = (float)high;
    }
}
int WorldJoint( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    const char *name = luaL_checkstring( L, 2 );
    b2JointType type;
    if ( !strcmp( name, "pivot" ) )
        type = b2_revoluteJoint;
    else if ( !strcmp( name, "weld" ) )
        type = b2_weldJoint;
    else if ( !strcmp( name, "distance" ) )
        type = b2_distanceJoint;
    else if ( !strcmp( name, "wheel" ) )
        type = b2_wheelJoint;
    else if ( !strcmp( name, "piston" ) )
        type = b2_prismaticJoint;
    else if ( !strcmp( name, "fake" ) )
        type = b2_filterJoint;
    else if ( !strcmp( name, "touch" ) )
        type = b2_motorJoint;
    else
        return luaL_error( L, "unsupported joint type" );
    bool touch = type == b2_motorJoint;
    Body &a = GetBody( L, 3 );
    Body &b = touch ? a : GetBody( L, 4 );
    if ( &Owner( L, 3, kBody ) != &w || ( !touch && ( &Owner( L, 4, kBody ) != &w || &a == &b ) ) )
        return luaL_error( L, "joint bodies must be distinct and belong to this world" );
    if ( touch && b2Body_GetType( b.id ) != b2_dynamicBody )
        return luaL_error( L, "touch requires a dynamic body" );
    int first = touch ? 4 : 5;
    b2Pos anchorA = { 0, 0 };
    if ( type != b2_filterJoint )
        anchorA = { (float)( Number( L, first ) / w.scale ), (float)( Number( L, first + 1 ) / w.scale ) };
    b2Pos anchorB = anchorA;
    b2Rot axis = b2Rot_identity;
    int options = type == b2_filterJoint ? 5 : first + 2;
    if ( type == b2_distanceJoint )
    {
        anchorB = { (float)( Number( L, 7 ) / w.scale ), (float)( Number( L, 8 ) / w.scale ) };
        options = 9;
    }
    else if ( type == b2_wheelJoint || type == b2_prismaticJoint )
    {
        double x = Number( L, 7 ), y = Number( L, 8 );
        double length = std::hypot( x, y );
        if ( length < 1e-12 )
            return luaL_error( L, "%s world axis must be nonzero", name );
        axis = { (float)( x / length ), (float)( y / length ) };
        options = 9;
    }
    bool hasLimits = false;
    float lower = 0, upper = 0;
    ReadJointOptions( L, options, w, type, hasLimits, lower, upper );
    // Fix the argument stack before pushing the result when options were omitted.
    lua_settop( L, options );
    b2JointDef base = b2DefaultRevoluteJointDef().base;
    if ( lua_istable( L, options ) )
    {
        // Apply before creation: creating with false would destroy existing
        // contacts even if a subsequent setter immediately enabled collision.
        lua_pushliteral( L, "collideConnected" );
        lua_rawget( L, options );
        base.collideConnected = lua_toboolean( L, -1 ) != 0;
        lua_pop( L, 1 );
    }
    if ( touch )
    {
        if ( B2_IS_NULL( w.touchGround ) )
        {
            b2BodyDef ground = b2DefaultBodyDef();
            w.touchGround = b2CreateBody( w.id, &ground );
        }
        base.bodyIdA = w.touchGround;
        // The shared ground never moves. Each joint owns its own target frame.
        base.localFrameA.p = { (float)anchorA.x, (float)anchorA.y };
    }
    else
    {
        base.bodyIdA = a.id;
        base.localFrameA.p = b2Body_GetLocalPoint( a.id, anchorA );
        if ( type == b2_revoluteJoint || type == b2_weldJoint )
        {
            // Match physics.newJoint's reference frame. Moving the inverse
            // rotation to B is mathematically equivalent, but changes rounding
            // in the angular constraint and can amplify migration differences.
            base.localFrameA.q = b2InvMulRot( b2Body_GetRotation( a.id ), b2Body_GetRotation( b.id ) );
        }
        else
            base.localFrameB.q = b2InvMulRot( b2Body_GetRotation( b.id ), b2Body_GetRotation( a.id ) );
        if ( type == b2_wheelJoint || type == b2_prismaticJoint )
        {
            base.localFrameA.q = b2InvMulRot( b2Body_GetRotation( a.id ), axis );
            base.localFrameB.q = b2InvMulRot( b2Body_GetRotation( b.id ), axis );
        }
    }
    base.bodyIdB = b.id;
    base.localFrameB.p = b2Body_GetLocalPoint( b.id, anchorB );
    b2JointId id = b2_nullJointId;
    if ( type == b2_revoluteJoint )
    {
        b2RevoluteJointDef d = b2DefaultRevoluteJointDef();
        d.base = base;
        id = b2CreateRevoluteJoint( w.id, &d );
    }
    else if ( type == b2_weldJoint )
    {
        b2WeldJointDef d = b2DefaultWeldJointDef();
        d.base = base;
        id = b2CreateWeldJoint( w.id, &d );
    }
    else if ( type == b2_distanceJoint )
    {
        b2DistanceJointDef d = b2DefaultDistanceJointDef();
        d.base = base;
        d.length = b2ClampFloat( (float)std::hypot( anchorB.x - anchorA.x, anchorB.y - anchorA.y ),
                                 B2_LINEAR_SLOP, B2_HUGE );
        id = b2CreateDistanceJoint( w.id, &d );
    }
    else if ( type == b2_wheelJoint )
    {
        b2WheelJointDef d = b2DefaultWheelJointDef();
        d.base = base;
        id = b2CreateWheelJoint( w.id, &d );
    }
    else if ( type == b2_prismaticJoint )
    {
        b2PrismaticJointDef d = b2DefaultPrismaticJointDef();
        d.base = base;
        id = b2CreatePrismaticJoint( w.id, &d );
    }
    else if ( type == b2_filterJoint )
    {
        b2FilterJointDef d = b2DefaultFilterJointDef();
        d.base = base;
        id = b2CreateFilterJoint( w.id, &d );
    }
    else
    {
        b2MotorJointDef d = b2DefaultMotorJointDef();
        d.base = base;
        d.maxSpringForce = 1000 * b2Body_GetMass( b.id );
        d.linearHertz = 7.5f;
        d.linearDampingRatio = 1;
        // Native defaults leave velocity motors and angular spring forces zero.
        id = b2CreateMotorJoint( w.id, &d );
    }
    if ( hasLimits )
        SetJointLimits( id, type, lower, upper );
    if ( lua_istable( L, options ) )
    {
        lua_pushnil( L );
        while ( lua_next( L, options ) )
        {
            const char *key = lua_tostring( L, -2 );
            const auto *number = JointNumber( type, key );
            const auto *flag = JointFlag( type, key );
            if ( number )
                number->set( id, (float)( lua_tonumber( L, -1 ) * JointScale( w, number->unit ) ) );
            else if ( flag )
                flag->set( id, lua_toboolean( L, -1 ) != 0 );
            lua_pop( L, 1 );
        }
    }
    b2Joint_WakeBodies( id );
    size_t index;
    if ( w.freeJoints.empty() )
    {
        index = w.joints.size();
        w.joints.push_back( id );
    }
    else
    {
        index = w.freeJoints.back();
        w.freeJoints.pop_back();
        w.joints[index] = id;
    }
    b2Joint_SetUserData( id, (void *)(uintptr_t)( index + 1 ) );
    Handle *h = (Handle *)lua_newuserdata( L, sizeof( Handle ) );
    *h = { &w, index, b2_nullBodyId, id };
    luaL_getmetatable( L, kJoint );
    lua_setmetatable( L, -2 );
    StoreHandle( L, 1, "joints", index );
    return 1;
}
