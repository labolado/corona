//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#ifdef Rtt_PHYSICS

#include "Rtt_PhysicsJoint.h"

// TODO: see which of these are actually needed here
#include "Rtt_Lua.h"
#include "Rtt_LuaLibPhysics.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_PhysicsWorld.h"
#include "Rtt_Runtime.h"
#include "Rtt_LuaProxyVTable.h"
#include "Rtt_LuaContext.h"

#include "box2d/box2d.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

const char PhysicsJoint::kMetatableName[] = "physics.joint"; // unique identifier for this userdata type

b2JointId
PhysicsJoint::GetJoint( lua_State *L, int index )
{
	b2JointId result = b2_nullJointId;

	JointUserdataWrapper **ud = (JointUserdataWrapper **)luaL_checkudata( L, index, Self::kMetatableName );
	if ( ud )
	{
		JointUserdataWrapper *wrapper = *ud;

		// result = (b2Joint*)wrapper->Dereference();
		result = wrapper->Dereference();
	}

	return result;
}

int
PhysicsJoint::getAnchorA( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is common to all joint types
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		b2Vec2 anchor = b2Body_GetWorldPoint(b2Joint_GetBodyA(baseJoint), b2Joint_GetLocalFrameA(baseJoint).p);
		Rtt_Real px = Rtt_RealMul( Rtt_FloatToReal( anchor.x ), scale );
		Rtt_Real py = Rtt_RealMul( Rtt_FloatToReal( anchor.y ), scale );

		lua_pushnumber( L, px );
		lua_pushnumber( L, py );
	}

	return 2;
}

int
PhysicsJoint::getAnchorB( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is common to all joint types
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		b2Vec2 anchor = b2Body_GetWorldPoint(b2Joint_GetBodyB(baseJoint), b2Joint_GetLocalFrameB(baseJoint).p);
		Rtt_Real px = Rtt_RealMul( Rtt_FloatToReal( anchor.x ), scale );
		Rtt_Real py = Rtt_RealMul( Rtt_FloatToReal( anchor.y ), scale );

		lua_pushnumber( L, px );
		lua_pushnumber( L, py );
	}

	return 2;
}

// ----------------------------------------------------------------------------

static bool
ShouldGetLocalAnchor( b2JointType jointType )
{
	bool result = false;

	switch ( jointType )
	{
		case b2_distanceJoint:
		case b2_revoluteJoint:
		case b2_prismaticJoint:
		// case b2_frictionJoint:
		case b2_wheelJoint:
		case b2_weldJoint:
		// case b2_ropeJoint:
			result = true;
			break;

		default:
			break;
	}

	return result;
}

/*
template < typename T >
static const b2Vec2& GetLocalAnchorA( b2JointId baseJoint )
{
	T *joint = static_cast< T * >( baseJoint ); Rtt_ASSERT( b2Joint_IsValid(baseJoint) );
	return joint->GetLocalAnchorA();
}

template < typename T >
static const b2Vec2& GetLocalAnchorB( b2JointId baseJoint )
{
	T *joint = static_cast< T * >( baseJoint ); Rtt_ASSERT( b2Joint_IsValid(baseJoint) );
	return joint->GetLocalAnchorB();
}

typedef const b2Vec2& ( *GetLocalAnchorCallback )( b2Joint * );

static GetLocalAnchorCallback
GetLocalAnchorACallback( b2JointType jointType )
{
	Rtt_ASSERT( ShouldGetLocalAnchor( jointType ) );
	GetLocalAnchorCallback result = NULL;
	switch ( jointType )
	{
		case e_distanceJoint:
			result = & GetLocalAnchorA< b2DistanceJoint >;
			break;
		case e_revoluteJoint:
			result = & GetLocalAnchorA< b2RevoluteJoint >;
			break;
		case e_prismaticJoint:
			result = & GetLocalAnchorA< b2PrismaticJoint >;
			break;
		case e_frictionJoint:
			result = & GetLocalAnchorA< b2FrictionJoint >;
			break;
		case e_wheelJoint:
			result = & GetLocalAnchorA< b2WheelJoint >;
			break;
		case e_weldJoint:
			result = & GetLocalAnchorA< b2WeldJoint >;
			break;
		case e_ropeJoint:
			result = & GetLocalAnchorA< b2RopeJoint >;
			break;

		default:
			break;
	}

	return result;
}

static GetLocalAnchorCallback
GetLocalAnchorBCallback( b2JointType jointType )
{
	Rtt_ASSERT( ShouldGetLocalAnchor( jointType ) );
	GetLocalAnchorCallback result = NULL;
	switch ( jointType )
	{
		case e_distanceJoint:
			result = & GetLocalAnchorB< b2DistanceJoint >;
			break;
		case e_revoluteJoint:
			result = & GetLocalAnchorB< b2RevoluteJoint >;
			break;
		case e_prismaticJoint:
			result = & GetLocalAnchorB< b2PrismaticJoint >;
			break;
		case e_frictionJoint:
			result = & GetLocalAnchorB< b2FrictionJoint >;
			break;
		case e_wheelJoint:
			result = & GetLocalAnchorB< b2WheelJoint >;
			break;
		case e_weldJoint:
			result = & GetLocalAnchorB< b2WeldJoint >;
			break;
		case e_ropeJoint:
			result = & GetLocalAnchorB< b2RopeJoint >;
			break;

		default:
			break;
	}

	return result;
}
*/

// ----------------------------------------------------------------------------

bool
PhysicsJoint::HasLocalAnchor( b2JointId jointId )
{
	return ShouldGetLocalAnchor( b2Joint_GetType(jointId) );
}

b2Vec2
PhysicsJoint::GetLocalAnchorA( b2JointId jointId )
{
	b2JointType jointType = b2Joint_GetType(jointId);
	if ( ShouldGetLocalAnchor( jointType ) )
	{
		// GetLocalAnchorCallback Callback = GetLocalAnchorACallback( jointType );
		// return Callback( & joint );
		return b2Joint_GetLocalFrameA(jointId).p;
	}

	return b2Vec2_zero;
}

b2Vec2
PhysicsJoint::GetLocalAnchorB( b2JointId jointId )
{
	b2JointType jointType = b2Joint_GetType(jointId);
	if ( ShouldGetLocalAnchor( jointType ) )
	{
		// GetLocalAnchorCallback Callback = GetLocalAnchorBCallback( jointType );
		// return Callback( & joint );
		return b2Joint_GetLocalFrameB(jointId).p;
	}

	return b2Vec2_zero;
}

// ----------------------------------------------------------------------------

int
PhysicsJoint::getLocalAnchorA( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is common to all joint types
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		b2Vec2 anchor = GetLocalAnchorA(baseJoint);
		// GetLocalAnchorCallback callback =
		// 	(GetLocalAnchorCallback)lua_touserdata( L, lua_upvalueindex( 1 ) ); Rtt_ASSERT( callback );
		// anchor = ( * callback )( baseJoint );

		Rtt_Real px = Rtt_RealMul( Rtt_FloatToReal( anchor.x ), scale );
		Rtt_Real py = Rtt_RealMul( Rtt_FloatToReal( anchor.y ), scale );

		lua_pushnumber( L, px );
		lua_pushnumber( L, py );
	}

	return 2;
}

int
PhysicsJoint::getLocalAnchorB( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is common to all joint types
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		b2Vec2 anchor = GetLocalAnchorB(baseJoint);
		// GetLocalAnchorCallback callback =
		// 	(GetLocalAnchorCallback)lua_touserdata( L, lua_upvalueindex( 1 ) ); Rtt_ASSERT( callback );
		// anchor = ( * callback )( baseJoint );

		Rtt_Real px = Rtt_RealMul( Rtt_FloatToReal( anchor.x ), scale );
		Rtt_Real py = Rtt_RealMul( Rtt_FloatToReal( anchor.y ), scale );

		lua_pushnumber( L, px );
		lua_pushnumber( L, py );
	}

	return 2;
}

// ----------------------------------------------------------------------------

#ifdef Rtt_DEBUG
static bool
ShouldGetLocalAxis( b2JointType jointType )
{
	bool result = false;

	switch ( jointType )
	{
		case b2_prismaticJoint:
		case b2_wheelJoint:
			result = true;
			break;

		default:
			break;
	}

	return result;
}
#endif // Rtt_DEBUG

/*
template < typename T >
static const b2Vec2& GetLocalAxisA( b2JointId baseJoint )
{
	T *joint = static_cast< T * >( baseJoint ); Rtt_ASSERT( baseJoint );
	return joint->GetLocalAxisA();
}

typedef const b2Vec2& ( *GetLocalAxisCallback )( b2Joint * );

static GetLocalAxisCallback
GetLocalAxisACallback( b2JointType jointType )
{
	Rtt_ASSERT( ShouldGetLocalAxis( jointType ) );
	GetLocalAxisCallback result = NULL;
	switch ( jointType )
	{
		case e_prismaticJoint:
			result = & GetLocalAxisA< b2PrismaticJoint >;
			break;
		case e_wheelJoint:
			result = & GetLocalAxisA< b2WheelJoint >;
			break;

		default:
			break;
	}

	return result;
}
*/

int
PhysicsJoint::getLocalAxis( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is common to all joint types
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		b2Vec2 anchor = b2Body_GetWorldPoint(b2Joint_GetBodyB(baseJoint), b2Joint_GetLocalFrameB(baseJoint).p);
		Rtt_Real px = Rtt_RealMul( Rtt_FloatToReal( anchor.x ), scale );
		Rtt_Real py = Rtt_RealMul( Rtt_FloatToReal( anchor.y ), scale );
		// Rtt_Real px = Rtt_RealMul( Rtt_FloatToReal( baseJoint->GetAnchorB().x ), scale );
		// Rtt_Real py = Rtt_RealMul( Rtt_FloatToReal( baseJoint->GetAnchorB().y ), scale );

		lua_pushnumber( L, px );
		lua_pushnumber( L, py );
	}

	return 2;
}

// ----------------------------------------------------------------------------

int
PhysicsJoint::getReactionForce( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is common to all joint types
	if ( b2Joint_IsValid(baseJoint) )
	{
		Runtime& runtime = * LuaContext::GetRuntime( L );
		float inverseDeltaTime = (float)runtime.GetFPS();
		b2Vec2 force = b2Joint_GetConstraintForce(baseJoint);
		Rtt_Real px = Rtt_FloatToReal( force.x );
		Rtt_Real py = Rtt_FloatToReal( force.y );

		lua_pushnumber( L, px );
		lua_pushnumber( L, py );
	}

	return 2;
}


int
PhysicsJoint::setRotationLimits( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is for revolute ("pivot") joints only
	if ( b2Joint_IsValid(baseJoint) )
	{
		Rtt_Real lowerLimit = Rtt_RealDegreesToRadians( lua_tonumber( L, 2 ) );
		Rtt_Real upperLimit = Rtt_RealDegreesToRadians( lua_tonumber( L, 3 ) );

		// b2RevoluteJoint *joint = (b2RevoluteJoint*)baseJoint;

		// joint->SetLimits( Rtt_RealToFloat( lowerLimit ), Rtt_RealToFloat( upperLimit ) );
		b2RevoluteJoint_SetLimits(baseJoint, Rtt_RealToFloat( lowerLimit ), Rtt_RealToFloat( upperLimit ) );
	}

	return 0;
}


int
PhysicsJoint::getRotationLimits( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is for revolute ("pivot") joints only
	if ( b2Joint_IsValid(baseJoint) )
	{
		// b2RevoluteJoint *joint = (b2RevoluteJoint*)baseJoint;

		Rtt_Real lowerLimit = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2RevoluteJoint_GetLowerLimit(baseJoint) ) );
		Rtt_Real upperLimit = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2RevoluteJoint_GetUpperLimit(baseJoint) ) );

		lua_pushnumber( L, lowerLimit );
		lua_pushnumber( L, upperLimit );
	}

	return 2;
}

int
PhysicsJoint::setLimits( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is for prismatic ("piston") joints only
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		Rtt_Real lowerLimit = Rtt_RealDiv( luaL_toreal( L, 2 ), scale );
		Rtt_Real upperLimit = Rtt_RealDiv( luaL_toreal( L, 3 ), scale );

		if ( upperLimit < lowerLimit )
		{
			Swap( lowerLimit, upperLimit );
			Rtt_TRACE_SIM( ( "WARNING: In the call to pistonJoint:setLimits(), it looks like you reversed "
							 "the lower and upper limits. We assume you meant to set the lower limit as "
							 "%g and the upper limit as %g.\n", lowerLimit, upperLimit ) );
		}

		// b2PrismaticJoint *joint = (b2PrismaticJoint*)baseJoint;
		// joint->SetLimits( Rtt_RealToFloat( lowerLimit ), Rtt_RealToFloat( upperLimit ) );
		b2PrismaticJoint_SetLimits( baseJoint, Rtt_RealToFloat( lowerLimit ), Rtt_RealToFloat( upperLimit ) );
	}

	return 0;
}

int
PhysicsJoint::getLimits( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is for prismatic ("piston") joints only
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		// b2PrismaticJoint *joint = (b2PrismaticJoint*)baseJoint; // assumption: this cast is OK for both cases (otherwise check type)

		Rtt_Real lowerLimit = Rtt_RealMul( Rtt_FloatToReal( b2PrismaticJoint_GetLowerLimit(baseJoint) ), scale );
		Rtt_Real upperLimit = Rtt_RealMul( Rtt_FloatToReal( b2PrismaticJoint_GetUpperLimit(baseJoint) ), scale );

		lua_pushnumber( L, lowerLimit );
		lua_pushnumber( L, upperLimit );
	}

	return 2;
}

int
PhysicsJoint::setMotorJointLinearVelocity( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is for motor joints only
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		Rtt_Real ox = Rtt_RealDiv( luaL_toreal( L, 2 ), scale );
		Rtt_Real oy = Rtt_RealDiv( luaL_toreal( L, 3 ), scale );

		b2Vec2 offset = { ox, oy };
		b2MotorJoint_SetLinearVelocity(baseJoint, offset);
	}

	return 0;
}

int
PhysicsJoint::getMotorJointLinearVelocity( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is for prismatic ("piston") joints only
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		Real scale = physics.GetPixelsPerMeter();

		b2Vec2 offset = b2MotorJoint_GetLinearVelocity( baseJoint );
		Rtt_Real ox = Rtt_RealMul( Rtt_FloatToReal( offset.x ), scale );
		Rtt_Real oy = Rtt_RealMul( Rtt_FloatToReal( offset.y ), scale );

		lua_pushnumber( L, ox );
		lua_pushnumber( L, oy );
	}

	return 2;
}

int
PhysicsJoint::getGroundAnchorA( lua_State *L )
{
	// b2JointId baseJoint = GetJoint( L, 1 );

	// // This is for pulley joints only
	// if ( b2Joint_IsValid(baseJoint) )
	// {
	// 	const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
	// 	Real scale = physics.GetPixelsPerMeter();

	// 	b2PulleyJoint *joint = (b2PulleyJoint*)baseJoint;

	// 	Rtt_Real px = Rtt_RealMul( Rtt_FloatToReal( joint->GetGroundAnchorA().x ), scale );
	// 	Rtt_Real py = Rtt_RealMul( Rtt_FloatToReal( joint->GetGroundAnchorA().y ), scale );

	// 	lua_pushnumber( L, px );
	// 	lua_pushnumber( L, py );
	// }
	lua_pushnumber( L, 0.0f );
	lua_pushnumber( L, 0.0f );

	return 2;
}

int
PhysicsJoint::getGroundAnchorB( lua_State *L )
{
	// b2JointId baseJoint = GetJoint( L, 1 );

	// This is for pulley joints only
	// if ( b2Joint_IsValid(baseJoint) )
	// {
	// 	const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
	// 	Real scale = physics.GetPixelsPerMeter();

	// 	b2PulleyJoint *joint = (b2PulleyJoint*)baseJoint;

	// 	Rtt_Real px = Rtt_RealMul( Rtt_FloatToReal( joint->GetGroundAnchorB().x ), scale );
	// 	Rtt_Real py = Rtt_RealMul( Rtt_FloatToReal( joint->GetGroundAnchorB().y ), scale );

	// 	lua_pushnumber( L, px );
	// 	lua_pushnumber( L, py );
	// }
	lua_pushnumber( L, 0.0f );
	lua_pushnumber( L, 0.0f );

	return 2;
}

int
PhysicsJoint::getTarget( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is for mouse ("touch") joints only
	if ( b2Joint_IsValid(baseJoint) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();

		// b2MouseJoint *joint = (b2MouseJoint*)baseJoint;

		b2Vec2 v( b2Body_GetWorldPoint( b2Joint_GetBodyA( baseJoint ) , b2Joint_GetLocalFrameA(baseJoint).p ) * physics.GetPixelsPerMeter() );

		lua_pushnumber( L, v.x );
		lua_pushnumber( L, v.y );
	}

	return 2;
}

int
PhysicsJoint::setTarget( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	// This is for mouse ("touch") joints only
	if ( b2Joint_IsValid( baseJoint ) )
	{
		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();

		b2Vec2 v = { (float)lua_tonumber( L, 2 ),
					(float)lua_tonumber( L, 3 ) };

		v *= physics.GetMetersPerPixel();

		// b2MouseJoint *joint = (b2MouseJoint*)baseJoint;
		// joint->SetTarget( v );
		// b2Transform localFrameB = b2Joint_GetLocalFrameB( baseJoint );
		// localFrameB.p = b2Body_GetLocalPoint( b2Joint_GetBodyB( baseJoint ), v );
		// b2Joint_SetLocalFrameA( baseJoint, localFrameB );
		// b2Body_SetTargetTransform( b2Joint_GetBodyA( baseJoint ), { v, b2Rot_identity }, physics.GetTimeStep() );
		b2Body_SetTransform( b2Joint_GetBodyA( baseJoint ), v, b2Rot_identity );
	}

	return 0;
}

int
PhysicsJoint::removeSelf( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	if ( b2Joint_IsValid(baseJoint) )
	{
		// Disconnect joint and wrapper from each other

		// (1) Remove wrapper's ref to joint
		JointUserdataWrapper *wrapper = (JointUserdataWrapper*)b2Joint_GetUserData( baseJoint );
		if ( wrapper && JointUserdataWrapper::GetFinalizedValue() != wrapper )
		{
			wrapper->Invalidate();
		}

		// (2) Remove joint's ref to wrapper.
		// Any joint with a finalized userdata value to signal it should be
		// destroyed after the world step (similar to body destruction cycle)
		// baseJoint->SetUserData( UserdataWrapper::GetFinalizedValue() );
		b2Joint_SetUserData( baseJoint, JointUserdataWrapper::GetFinalizedValue() );
		b2DestroyJoint(baseJoint, true);
	}

	return 0;
}

/*
static const char *
StringForLimitState( b2LimitState value )
{
	static const char kInactiveLimit[] = "inactive";
	static const char kLowerLimit[] = "lower";
	static const char kUpperLimit[] = "upper";
	static const char kEqualLimit[] = "equal";

	const char *result = kInactiveLimit;

	switch ( value )
	{
		case e_atLowerLimit:
			result = kLowerLimit;
			break;
		case e_atUpperLimit:
			result = kUpperLimit;
			break;
		case e_equalLimits:
			result = kEqualLimit;
			break;
		default:
			break;
	}

	return result;
}
*/

int
PhysicsJoint::ValueForKey( lua_State *L )
{
	int result = 0;    // number of args pushed on the stack

	b2JointId baseJoint = GetJoint( L, 1 );

	if ( b2Joint_IsValid(baseJoint) )
	{
		const char *key = luaL_checkstring( L, 2 );
		result = 1;

		b2JointType jointType = b2Joint_GetType(baseJoint);

		//////////////////////////////////////////////
		// These methods are common to all joint types

		if ( strcmp( "getAnchorA", key ) == 0 )
		{
			lua_pushcfunction( L, Self::getAnchorA );
		}
		else if ( strcmp( "getAnchorB", key ) == 0 )
		{
			lua_pushcfunction( L, Self::getAnchorB );
		}
		else if ( strcmp( "getReactionForce", key ) == 0 )
		{
			lua_pushcfunction( L, Self::getReactionForce );
		}
		else if ( 0 == strcmp( "reactionTorque", key ) )  // read-only
		{
			Runtime& runtime = * LuaContext::GetRuntime( L );
			float inverseDeltaTime = (float)runtime.GetFPS();
			lua_pushnumber( L, b2Joint_GetConstraintTorque(baseJoint) * inverseDeltaTime );
		}
		else if ( 0 == strcmp( "removeSelf", key ) )
		{
			lua_pushcfunction( L, Self::removeSelf );
		}
		else if ( 0 == strcmp( "isActive", key ) )
		{
			// lua_pushboolean( L, baseJoint->IsActive() );
			lua_pushboolean( L, true );
		}
		else if ( 0 == strcmp( "isCollideConnected", key ) )
		{
			lua_pushboolean( L, b2Joint_GetCollideConnected(baseJoint) );
		}
		else if ( 0 == strcmp( "getLocalAnchorA", key ) && ShouldGetLocalAnchor( jointType ) )
		{
			// lua_pushlightuserdata( L, (void*)GetLocalAnchorACallback( jointType ) );
			// lua_pushcclosure( L, Self::getLocalAnchorA, 1 );
			lua_pushcfunction( L, Self::getLocalAnchorA );
		}
		else if ( 0 == strcmp( "getLocalAnchorB", key ) && ShouldGetLocalAnchor( jointType ) )
		{
			// lua_pushlightuserdata( L, (void*)GetLocalAnchorBCallback( jointType ) );
			// lua_pushcclosure( L, Self::getLocalAnchorB, 1 );
			lua_pushcfunction( L, Self::getLocalAnchorB );
		}
		else
		{
			// No common property found. Look at specific properties depending on joint type

			if ( jointType == b2_distanceJoint )
			{
				//////////////////////////////////////////////////////////////////////////////
				// This is exposed as a "distance" joint in Corona

				// b2DistanceJoint *joint = (b2DistanceJoint*)baseJoint;

				if ( 0 == strcmp( "length", key ) )
				{
					const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					Real scale = physics.GetPixelsPerMeter();
					Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( b2DistanceJoint_GetLength(baseJoint) ), scale );
					lua_pushnumber( L, valuePixels );
				}
				else if ( 0 == strcmp( "frequency", key ) )
				{
					lua_pushnumber( L, b2DistanceJoint_GetSpringHertz(baseJoint) );
				}
				else if ( 0 == strcmp( "dampingRatio", key ) )
				{
					lua_pushnumber( L, b2DistanceJoint_GetSpringDampingRatio(baseJoint) );
				}
				else if ( 0 == strcmp( "isSpringEnabled", key ) )
				{
					lua_pushboolean( L, b2DistanceJoint_IsSpringEnabled(baseJoint) );
				}
				else
				{
					result = 0;
				}
			}
			else if ( jointType == b2_revoluteJoint )
			{
				//////////////////////////////////////////////////////////////////////////////
				// This is exposed as a "pivot" joint in Corona (aka "revolute" in Box2D terms)

				// b2RevoluteJoint *joint = (b2RevoluteJoint*)baseJoint;

				if ( 0 == strcmp( "isMotorEnabled", key ) )
				{
					lua_pushboolean( L, b2RevoluteJoint_IsMotorEnabled(baseJoint) );
				}
				else if ( 0 == strcmp( "motorSpeed", key ) )
				{
					Rtt_Real valueDegrees = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2RevoluteJoint_GetMotorSpeed(baseJoint) ) );
					lua_pushnumber( L, valueDegrees );
				}
				else if ( 0 == strcmp( "motorTorque", key ) )  // read-only
				{
					Runtime& runtime = * LuaContext::GetRuntime( L );
					float inverseDeltaTime = (float)runtime.GetFPS();

					lua_pushnumber( L, b2RevoluteJoint_GetMotorTorque(baseJoint) * inverseDeltaTime );
				}
				else if ( 0 == strcmp( "maxMotorTorque", key ) )
				{
					lua_pushnumber( L, b2RevoluteJoint_GetMaxMotorTorque(baseJoint) );
				}
				else if ( 0 == strcmp( "referenceAngle", key ) )  // read-only
				{
					// Rtt_Real valueDegrees = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( joint->GetReferenceAngle() ) );
					// lua_pushnumber( L, valueDegrees );
					lua_pushnumber( L, 0.0f );
				}
				else if ( 0 == strcmp( "jointAngle", key ) )  // read-only
				{
					Rtt_Real valueDegrees = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2RevoluteJoint_GetAngle(baseJoint) ) );
					lua_pushnumber( L, valueDegrees );
				}
				else if ( 0 == strcmp( "jointSpeed", key ) )  // read-only
				{
					float speed = b2Body_GetAngularVelocity( b2Joint_GetBodyB(baseJoint) ) - b2Body_GetAngularVelocity( b2Joint_GetBodyA(baseJoint) );
					Rtt_Real valueDegrees = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( speed ) );
					lua_pushnumber( L, valueDegrees );
				}
				else if ( 0 == strcmp( "isLimitEnabled", key ) )
				{
					lua_pushboolean( L, b2RevoluteJoint_IsLimitEnabled(baseJoint) );
				}
				else if ( strcmp( "setRotationLimits", key ) == 0 )
				{
					lua_pushcfunction( L, Self::setRotationLimits );
				}
				else if ( strcmp( "getRotationLimits", key ) == 0 )
				{
					lua_pushcfunction( L, Self::getRotationLimits );
				}
				else
				{
					result = 0;
				}
			}
			else if ( jointType == b2_motorJoint )
			{
				//////////////////////////////////////////////////////////////////////////////
				// This is exposed as a "pivot" joint in Corona (aka "revolute" in Box2D terms)

				// b2MotorJoint *joint = (b2MotorJoint*)baseJoint;

				if ( 0 == strcmp( "maxSpringTorque", key ) )
				{
					lua_pushnumber( L, b2MotorJoint_GetMaxSpringTorque(baseJoint) );
				}
				else if ( 0 == strcmp( "maxVelocityTorque", key ) )
				{
					lua_pushnumber( L, b2MotorJoint_GetMaxVelocityTorque(baseJoint) );
				}
				else if ( 0 == strcmp( "maxSpringForce", key ) || 0 == strcmp( "maxForce", key ) )
				{
					lua_pushnumber( L, b2MotorJoint_GetMaxSpringForce(baseJoint) );
				}
				else if ( 0 == strcmp( "maxVelocityForce", key ) )
				{
					lua_pushnumber( L, b2MotorJoint_GetMaxVelocityForce(baseJoint) );
				}
				else if ( 0 == strcmp( "linearHertz", key ) || 0 == strcmp( "frequency", key ) )
				{
					lua_pushnumber( L, b2MotorJoint_GetLinearHertz(baseJoint) );
				}
				else if ( 0 == strcmp( "angularHertz", key ) )
				{
					lua_pushnumber( L, b2MotorJoint_GetAngularHertz(baseJoint) );
				}
				else if ( 0 == strcmp( "linearDampingRatio", key ) || 0 == strcmp( "dampingRatio", key ) )
				{
					lua_pushnumber( L, b2MotorJoint_GetLinearDampingRatio(baseJoint) );
				}
				else if ( 0 == strcmp( "angularDampingRatio", key ) )
				{
					lua_pushnumber( L, b2MotorJoint_GetAngularDampingRatio(baseJoint) );
				}
				else if ( 0 == strcmp( "angularVelocity", key ) )
				{
					Rtt_Real valueDegrees = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2MotorJoint_GetAngularVelocity(baseJoint) ) );
					lua_pushnumber( L, valueDegrees );
				}
				else if ( strcmp( "setLinearVelocity", key ) == 0 )
				{
					lua_pushcfunction( L, Self::setMotorJointLinearVelocity );
				}
				else if ( strcmp( "getLinearVelocity", key ) == 0 )
				{
					lua_pushcfunction( L, Self::getMotorJointLinearVelocity );
				}
				else if ( strcmp( "setTarget", key ) == 0 )
				{
					lua_pushcfunction( L, Self::setTarget );
				}
				else if ( strcmp( "getTarget", key ) == 0 )
				{
					lua_pushcfunction( L, Self::getTarget );
				}
				else
				{
					result = 0;
				}
			}
			else if ( jointType == b2_prismaticJoint )
			{
				//////////////////////////////////////////////////////////////////////////////
				// This is exposed as a "piston" joint in Corona (aka "prismatic" in Box2D terms)

				// b2PrismaticJoint *joint = (b2PrismaticJoint*)baseJoint;

				if ( 0 == strcmp( "isMotorEnabled", key ) )
				{
					lua_pushboolean( L, b2PrismaticJoint_IsMotorEnabled(baseJoint) );
				}
				else if ( 0 == strcmp( "motorSpeed", key ) )
				{
					const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					Real scale = physics.GetPixelsPerMeter();
					Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( b2PrismaticJoint_GetMotorSpeed(baseJoint) ), scale );
					lua_pushnumber( L, valuePixels );
				}
				else if ( 0 == strcmp( "motorForce", key ) ) // read-only
				{
					Runtime& runtime = * LuaContext::GetRuntime( L );
					float inverseDeltaTime = (float)runtime.GetFPS();

					lua_pushnumber( L,  b2PrismaticJoint_GetMotorForce(baseJoint) * inverseDeltaTime );
				}
				else if ( 0 == strcmp( "maxMotorForce", key ) )
				{
					lua_pushnumber( L,  b2PrismaticJoint_GetMaxMotorForce(baseJoint) );
				}
				else if ( strcmp( "getLocalAxisA", key ) == 0 )
				{
					// lua_pushlightuserdata( L, (void *)GetLocalAxisACallback( e_prismaticJoint ) );
					// lua_pushcclosure( L, Self::getLocalAxis, 1 );
					lua_pushcfunction( L, Self::getLocalAxis );
				}
				else if ( 0 == strcmp( "referenceAngle", key ) )  // read-only
				{
					// Rtt_Real valueDegrees = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2PrismaticJoint_ ) );
					// lua_pushnumber( L, valueDegrees );
				}
				else if ( 0 == strcmp( "jointTranslation", key ) )  // read-only
				{
					const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					Real scale = physics.GetPixelsPerMeter();
					Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( b2PrismaticJoint_GetTranslation(baseJoint) ), scale );
					lua_pushnumber( L, valuePixels );
				}
				else if ( 0 == strcmp( "jointSpeed", key ) )  // read-only
				{
					const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					Real scale = physics.GetPixelsPerMeter();
					Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( b2PrismaticJoint_GetSpeed(baseJoint) ), scale );
					lua_pushnumber( L, valuePixels );
				}
				else if ( 0 == strcmp( "isLimitEnabled", key ) )
				{
					lua_pushboolean( L, b2PrismaticJoint_IsLimitEnabled(baseJoint) );
				}
				else if ( strcmp( "setLimits", key ) == 0 )
				{
					lua_pushcfunction( L, Self::setLimits );
				}
				else if ( strcmp( "getLimits", key ) == 0 )
				{
					lua_pushcfunction( L, Self::getLimits );
				}
				else
				{
					result = 0;
				}
			}
			// else if ( jointType == e_frictionJoint )
			// {
			// 	//////////////////////////////////////////////////////////////////////////////
			// 	// This is exposed as a "friction" joint in Corona (also "friction" in Box2D terms)
			// 	// A friction joint is a pivot joint that resists motion, and is therefore "sticky"

			// 	b2FrictionJoint *joint = (b2FrictionJoint*)baseJoint;

			// 	if ( 0 == strcmp( "maxForce", key ) )
			// 	{
			// 		lua_pushnumber( L, joint->GetMaxForce() );
			// 	}
			// 	else if ( 0 == strcmp( "maxTorque", key ) )
			// 	{
			// 		lua_pushnumber( L, joint->GetMaxTorque() );
			// 	}
			// 	else
			// 	{
			// 		result = 0;
			// 	}
			// }
			else if ( jointType == b2_wheelJoint )
			{
				//////////////////////////////////////////////////////////////////////////////
				// This is exposed as a "wheel" joint in Corona (aka "line" in Box2D terms)

				// b2WheelJoint *joint = (b2WheelJoint*)baseJoint;

				if ( 0 == strcmp( "isMotorEnabled", key ) )
				{
					lua_pushboolean( L, b2WheelJoint_IsMotorEnabled(baseJoint) );
				}
				else if ( 0 == strcmp( "motorSpeed", key ) )
				{
					// const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					// Real scale = physics.GetPixelsPerMeter();
					// Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( joint->GetMotorSpeed() ), scale );
					// lua_pushnumber( L, valuePixels );
					Rtt_Real valueDegrees = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2WheelJoint_GetMotorSpeed(baseJoint) ) );
					lua_pushnumber( L, valueDegrees );
				}
				else if ( 0 == strcmp( "motorTorque", key ) )  // read-only
				{
					lua_pushnumber( L, b2WheelJoint_GetMotorTorque(baseJoint) );
				}
				else if ( 0 == strcmp( "maxMotorTorque", key ) )
				{
					lua_pushnumber( L, b2WheelJoint_GetMaxMotorTorque(baseJoint) );
				}
				else if ( strcmp( "getLocalAxisA", key ) == 0 )
				{
					// lua_pushlightuserdata( L, (void *)GetLocalAxisACallback( e_wheelJoint ) );
					// lua_pushcclosure( L, Self::getLocalAxis, 1 );
					lua_pushcfunction( L, Self::getLocalAxis );
				}
				else if ( 0 == strcmp( "jointTranslation", key ) )  // read-only
				{
					const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					Real scale = physics.GetPixelsPerMeter();
					Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( b2WheelJoint_GetJointTranslation(baseJoint) ), scale );
					lua_pushnumber( L, valuePixels );
				}
				else if ( 0 == strcmp( "jointSpeed", key ) )  // read-only
				{
					const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					Real scale = physics.GetPixelsPerMeter();
					// Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( joint->GetJointSpeed() ), scale );
					Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( b2WheelJoint_GetMotorSpeed(baseJoint) ), scale );
					lua_pushnumber( L, valuePixels );
				}
				else if ( strcmp( "springFrequency", key ) == 0 )
				{
					lua_pushnumber( L, b2WheelJoint_GetSpringHertz(baseJoint) );
				}
				else if ( strcmp( "springDampingRatio", key ) == 0 )
				{
					lua_pushnumber( L, b2WheelJoint_GetSpringDampingRatio(baseJoint) );
				}
				else if ( 0 == strcmp( "isSpringEnabled", key ) )
				{
					lua_pushboolean( L, b2WheelJoint_IsSpringEnabled(baseJoint) );
				}
				else
				{
					result = 0;
				}
			}
			// else if ( jointType == e_pulleyJoint )
			// {
			// 	//////////////////////////////////////////////////////////////////////////////
			// 	// This is exposed as a "pulley" joint in Corona

			// 	b2PulleyJoint *joint = (b2PulleyJoint*)baseJoint;

			// 	if ( 0 == strcmp( "getGroundAnchorA", key ) ) // read-only
			// 	{
			// 		lua_pushcfunction( L, Self::getGroundAnchorA );
			// 	}
			// 	else if ( 0 == strcmp( "getGroundAnchorB", key ) ) // read-only
			// 	{
			// 		lua_pushcfunction( L, Self::getGroundAnchorB );
			// 	}
			// 	else if ( 0 == strcmp( "length1", key ) ) // read-only
			// 	{
			// 		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
			// 		Real scale = physics.GetPixelsPerMeter();
			// 		Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( joint->GetLengthA() ), scale );
			// 		lua_pushnumber( L, valuePixels );
			// 	}
			// 	else if ( 0 == strcmp( "length2", key ) ) // read-only
			// 	{
			// 		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
			// 		Real scale = physics.GetPixelsPerMeter();
			// 		Rtt_Real valuePixels = Rtt_RealMul( Rtt_FloatToReal( joint->GetLengthB() ), scale );
			// 		lua_pushnumber( L, valuePixels );
			// 	}
			// 	else if ( 0 == strcmp( "ratio", key ) ) // read-only
			// 	{
			// 		lua_pushnumber( L, joint->GetRatio() );
			// 	}
			// 	else
			// 	{
			// 		result = 0;
			// 	}
			// }
			// else if ( jointType == b2_mouseJoint )
			// {
			// 	//////////////////////////////////////////////////////////////////////////////
			// 	// This is exposed as a "touch" joint in Corona (aka "mouse joint" in Box2D terms)
			// 	// A touch joint is used for dragging objects without overriding the simulation;
			// 	// it creates an elastic link between the current touch point -- or any point submitted
			// 	// by the end developer using SetTarget() -- and the specified body

			// 	// b2MouseJoint *joint = (b2MouseJoint*)baseJoint;

			// 	if ( 0 == strcmp( "maxForce", key ) )
			// 	{
			// 		lua_pushnumber( L, b2MouseJoint_GetMaxForce(baseJoint) );
			// 	}
			// 	else if ( 0 == strcmp( "frequency", key ) )
			// 	{
			// 		lua_pushnumber( L, b2MouseJoint_GetSpringHertz(baseJoint) );
			// 	}
			// 	else if ( 0 == strcmp( "dampingRatio", key ) )
			// 	{
			// 		lua_pushnumber( L, b2MouseJoint_GetSpringDampingRatio(baseJoint) );
			// 	}
			// 	else if ( strcmp( "setTarget", key ) == 0 )
			// 	{
			// 		lua_pushcfunction( L, Self::setTarget );
			// 	}
			// 	else if ( strcmp( "getTarget", key ) == 0 )
			// 	{
			// 		lua_pushcfunction( L, Self::getTarget );
			// 	}
			// 	else
			// 	{
			// 		result = 0;
			// 	}
			// }
			// else if ( jointType == e_gearJoint )
			// {
			// 	//////////////////////////////////////////////////////////////////////////////
			// 	b2GearJoint *joint = (b2GearJoint*)baseJoint;

			// 	if ( 0 == strcmp( "ratio", key ) )
			// 	{
			// 		lua_pushnumber( L, joint->GetRatio() );
			// 	}
			// 	else if ( 0 == strcmp( "joint1", key ) )
			// 	{
			// 		UserdataWrapper *wrapper = (UserdataWrapper *)joint->GetJoint1()->GetUserData();
			// 		wrapper->Push();
			// 	}
			// 	else if ( 0 == strcmp( "joint2", key ) )
			// 	{
			// 		UserdataWrapper *wrapper = (UserdataWrapper *)joint->GetJoint2()->GetUserData();
			// 		wrapper->Push();
			// 	}
			// 	else
			// 	{
			// 		result = 0;
			// 	}
			// }
			else if ( jointType == b2_weldJoint )
			{
				//////////////////////////////////////////////////////////////////////////////
				// b2WeldJoint *joint = (b2WeldJoint*)baseJoint;

				if ( 0 == strcmp( "referenceAngle", key ) )  // read-only
				{
					// Rtt_Real valueDegrees = Rtt_RealRadiansToDegrees( Rtt_FloatToReal( b2Joint_GetReferenceAngle( baseJoint ) ) );
					// lua_pushnumber( L, valueDegrees );
					lua_pushnumber( L, 0.0f );
				}
				else if ( 0 == strcmp( "frequency", key ) )
				{
					lua_pushnumber( L, b2WeldJoint_GetLinearHertz(baseJoint) );
				}
				else if ( 0 == strcmp( "dampingRatio", key ) )
				{
					lua_pushnumber( L, b2WeldJoint_GetLinearDampingRatio(baseJoint) );
				}
				else if ( 0 == strcmp( "angularFrequency", key ) )
				{
					lua_pushnumber( L, b2WeldJoint_GetAngularHertz(baseJoint) );
				}
				else if ( 0 == strcmp( "angulardampingRatio", key ) )
				{
					lua_pushnumber( L, b2WeldJoint_GetAngularDampingRatio(baseJoint) );
				}
				else
				{
					result = 0;
				}
			}
			// else if ( jointType == e_ropeJoint )
			// {
			// 	//////////////////////////////////////////////////////////////////////////////
			// 	b2RopeJoint *joint = (b2RopeJoint*)baseJoint;

			// 	if ( 0 == strcmp( "maxLength", key ) )  // read-only
			// 	{
			// 		const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();

			// 		float value = LuaLibPhysics::FromMKS( LuaLibPhysics::kLengthUnitType, physics, joint->GetMaxLength() );
			// 		lua_pushnumber( L, value );
			// 	}
			// 	else if ( 0 == strcmp( "limitState", key ) )
			// 	{
			// 		// lua_pushstring( L, StringForLimitState( joint->GetLimitState() ) );
			// 		lua_pushstring( L, "missing" );
			// 	}
			// 	else
			// 	{
			// 		result = 0;
			// 	}
			// }
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int
PhysicsJoint::SetValueForKey( lua_State *L )
{
	b2JointId baseJoint = GetJoint( L, 1 );

	if ( b2Joint_IsValid(baseJoint) )
	{
		const char *key = luaL_checkstring( L, 2 );

		b2JointType jointType = b2Joint_GetType(baseJoint);

		//////////////////////////////////////////////
		// These methods are common to all joint types

		if ( 0 == strcmp( "reactionTorque", key ) )
		{
			// No-op for read-only property
		}


		if (jointType == b2_distanceJoint)
		{
			//////////////////////////////////////////////////////////////////////////////
 			// This is exposed as a "distance" joint in Corona

			// b2DistanceJoint *joint = (b2DistanceJoint*)baseJoint;

			if ( 0 == strcmp( "length", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					Real scale = physics.GetPixelsPerMeter();
					Real valueMeters = Rtt_RealDiv( luaL_toreal( L, 3 ), scale );
					b2DistanceJoint_SetLength( baseJoint, valueMeters );
				}
			}
			else if ( 0 == strcmp( "frequency", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					Rtt_Real newValue = luaL_toreal( L, 3 );
					b2DistanceJoint_SetSpringHertz( baseJoint, newValue );
				}
			}
			else if ( 0 == strcmp( "dampingRatio", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					Rtt_Real newValue = luaL_toreal( L, 3 );
					b2DistanceJoint_SetSpringDampingRatio( baseJoint, newValue );
				}
			}
			else if ( 0 == strcmp( "isSpringEnabled", key ) )
			{
				if ( lua_isboolean( L, 3 ) )
				{
					b2DistanceJoint_EnableSpring( baseJoint, lua_toboolean( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
		}

		else if ( jointType == b2_motorJoint )
		{
			//////////////////////////////////////////////////////////////////////////////
			// This is exposed as a "pivot" joint in Corona (aka "revolute" in Box2D terms)

			if ( 0 == strcmp( "maxSpringTorque", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2MotorJoint_SetMaxSpringTorque( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			if ( 0 == strcmp( "maxVelocityTorque", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2MotorJoint_SetMaxVelocityTorque( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "maxSpringForce", key ) || 0 == strcmp( "maxForce", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2MotorJoint_SetMaxSpringForce( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "maxVelocityForce", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2MotorJoint_SetMaxVelocityForce( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "linearHertz", key ) || 0 == strcmp( "frequency", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2MotorJoint_SetLinearHertz( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "angularHertz", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2MotorJoint_SetAngularHertz( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "linearDampingRatio", key ) || 0 == strcmp( "dampingRatio", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2MotorJoint_SetLinearDampingRatio( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "angularDampingRatio", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2MotorJoint_SetAngularDampingRatio( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "angularVelocity", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					Rtt_Real valueRadians = Rtt_RealDegreesToRadians( luaL_toreal( L, 3 ) );
					b2MotorJoint_SetAngularVelocity( baseJoint, Rtt_RealToFloat( valueRadians ) );
				}
			}
		}

		else if (jointType == b2_revoluteJoint)
		{
			//////////////////////////////////////////////////////////////////////////////
			// This is exposed as a "pivot" joint in Corona (aka "revolute" in Box2D terms)

			// b2RevoluteJoint *joint = (b2RevoluteJoint*)baseJoint;

			if ( 0 == strcmp( "isMotorEnabled", key ) )
			{
				if ( lua_isboolean( L, 3 ) )
				{
					b2RevoluteJoint_EnableMotor( baseJoint, lua_toboolean( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "motorSpeed", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					Rtt_Real valueRadians = Rtt_RealDegreesToRadians( luaL_toreal( L, 3 ) );
					b2RevoluteJoint_SetMotorSpeed( baseJoint, Rtt_RealToFloat( valueRadians ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "motorTorque", key ) )
			{
				// No-op for read-only property
			}
			else if ( 0 == strcmp( "maxMotorTorque", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2RevoluteJoint_SetMaxMotorTorque( baseJoint, lua_tonumber( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "isLimitEnabled", key ) )
			{
				if ( lua_isboolean( L, 3 ) )
				{
					b2RevoluteJoint_EnableLimit( baseJoint, lua_toboolean( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "jointAngle", key ) )
			{
				// No-op for read-only property
			}
			else if ( 0 == strcmp( "jointSpeed", key ) )
			{
				// No-op for read-only property
			}
		}

		else if (jointType == b2_prismaticJoint)
		{
			//////////////////////////////////////////////////////////////////////////////
			// This is exposed as a "piston" joint in Corona (aka "prismatic" in Box2D terms)

			// b2PrismaticJoint *joint = (b2PrismaticJoint*)baseJoint;

			if ( 0 == strcmp( "isMotorEnabled", key ) )
			{
				if ( lua_isboolean( L, 3 ) )
				{
					b2PrismaticJoint_EnableMotor( baseJoint, lua_toboolean( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "motorSpeed", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
					Real scale = physics.GetPixelsPerMeter();
					Real valueMeters = Rtt_RealDiv( luaL_toreal( L, 3 ), scale );
					b2PrismaticJoint_SetMotorSpeed( baseJoint, Rtt_RealToFloat( valueMeters ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "motorForce", key ) )
			{
				// No-op for read-only property
			}
			else if ( 0 == strcmp( "maxMotorForce", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2PrismaticJoint_SetMaxMotorForce( baseJoint, lua_tonumber( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "isLimitEnabled", key ) )
			{
				if ( lua_isboolean( L, 3 ) )
				{
					b2PrismaticJoint_EnableLimit( baseJoint, lua_toboolean( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "jointTranslation", key ) )
			{
				// No-op for read-only property
			}
			else if ( 0 == strcmp( "jointSpeed", key ) )
			{
				// No-op for read-only property
			}

		}

		// else if ( jointType == e_frictionJoint )
		// {
		// 	//////////////////////////////////////////////////////////////////////////////
		// 	// This is exposed as a "friction" joint in Corona (also "friction" in Box2D terms)
		// 	// A friction joint is a pivot joint that resists motion, and is therefore "sticky"

		// 	b2FrictionJoint *joint = (b2FrictionJoint*)baseJoint;

		// 	if ( 0 == strcmp( "maxForce", key ) )
		// 	{
		// 		if ( lua_isnumber( L, 3 ) )
		// 		{
		// 			joint->SetMaxForce( lua_tonumber( L, 3 ) );
		// 		}
		// 	}
		// 	else if ( 0 == strcmp( "maxTorque", key ) )
		// 	{
		// 		if ( lua_isnumber( L, 3 ) )
		// 		{
		// 			joint->SetMaxTorque( lua_tonumber( L, 3 ) );
		// 		}
		// 	}
		// }

		else if (jointType == b2_wheelJoint)
		{
			//////////////////////////////////////////////////////////////////////////////
			// This is exposed as a "wheel" joint in Corona (aka "line" in Box2D terms)

			// b2WheelJoint *joint = (b2WheelJoint*)baseJoint;

			if ( 0 == strcmp( "isMotorEnabled", key ) )
			{
				if ( lua_isboolean( L, 3 ) )
				{
					b2WheelJoint_EnableMotor( baseJoint, lua_toboolean( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "motorSpeed", key ) )
			{
				// if ( lua_isnumber( L, 3 ) )
				// {
				// 	const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
				// 	Real scale = physics.GetPixelsPerMeter();
				// 	Rtt_Real valueMeters = Rtt_RealDiv( Rtt_FloatToReal( lua_toboolean( L, 3 ) ), scale );
				// 	joint->SetMotorSpeed( Rtt_RealToFloat( valueMeters ) );
				// }
				if ( lua_isnumber( L, 3 ) )
				{
					Rtt_Real valueRadians = Rtt_RealDegreesToRadians( luaL_toreal( L, 3 ) );
					b2WheelJoint_SetMotorSpeed( baseJoint, Rtt_RealToFloat( valueRadians ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "motorTorque", key ) )
			{
				// No-op for read-only property
			}
			else if ( 0 == strcmp( "maxMotorTorque", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2WheelJoint_SetMaxMotorTorque( baseJoint, lua_tonumber( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
			else if ( 0 == strcmp( "jointTranslation", key ) )
			{
				// No-op for read-only property
			}
			else if ( 0 == strcmp( "jointSpeed", key ) )
			{
				// No-op for read-only property
			}
			else if ( strcmp( "springFrequency", key ) == 0 )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2WheelJoint_SetSpringHertz( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( strcmp( "springDampingRatio", key ) == 0 )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2WheelJoint_SetSpringDampingRatio( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "isSpringEnabled", key ) )
			{
				if ( lua_isboolean( L, 3 ) )
				{
					b2WheelJoint_EnableSpring( baseJoint, lua_toboolean( L, 3 ) );
					b2Joint_WakeBodies( baseJoint );
				}
			}
		}

		// else if ( jointType == e_pulleyJoint )
		// {
		// 	//////////////////////////////////////////////////////////////////////////////
 	// 		// This is exposed as a "pulley" joint in Corona (type-specific methods are read-only)

		// 	// TODO: maxLength1 and maxLength2 settable properties? (check docs)

		// 	// b2PulleyJoint *joint = (b2PulleyJoint*)baseJoint;

		// 	if ( 0 == strcmp( "length1", key ) )
		// 	{
		// 		// No-op for read-only property
		// 	}
		// 	else if ( 0 == strcmp( "length2", key ) )
		// 	{
		// 		// No-op for read-only property
		// 	}
		// 	else if ( 0 == strcmp( "ratio", key ) )
		// 	{
		// 		// No-op for read-only property
		// 	}

		// }

		// else if ( jointType == b2_mouseJoint )
		// {
		// 	//////////////////////////////////////////////////////////////////////////////
		// 	// This is exposed as a "touch" joint in Corona (aka "mouse joint" in Box2D terms)
		// 	// A touch joint is used for dragging objects without overriding the simulation;
		// 	// it creates an elastic link between the current touch point -- or any point submitted
		// 	// by the end developer using SetTarget() -- and the specified body

		// 	// b2MouseJoint *joint = (b2MouseJoint*)baseJoint;

		// 	if ( 0 == strcmp( "maxForce", key ) )
		// 	{
		// 		if ( lua_isnumber( L, 3 ) )
		// 		{
		// 			b2MouseJoint_SetMaxForce( baseJoint, lua_tonumber( L, 3 ) );
		// 		}
		// 	}
		// 	else if ( 0 == strcmp( "frequency", key ) )
		// 	{
		// 		if ( lua_isnumber( L, 3 ) )
		// 		{
		// 			b2MouseJoint_SetSpringHertz( baseJoint, lua_tonumber( L, 3 ) );
		// 		}
		// 	}
		// 	else if ( 0 == strcmp( "dampingRatio", key ) )
		// 	{
		// 		if ( lua_isnumber( L, 3 ) )
		// 		{
		// 			b2MouseJoint_SetSpringDampingRatio( baseJoint, lua_tonumber( L, 3 ) );
		// 		}
		// 	}
		// }

		// else if ( jointType == e_gearJoint )
		// {
		// 	//////////////////////////////////////////////////////////////////////////////
		// 	b2GearJoint *joint = (b2GearJoint*)baseJoint;

		// 	if ( 0 == strcmp( "ratio", key ) )
		// 	{
		// 		if ( lua_isnumber( L, 3 ) )
		// 		{
		// 			joint->SetRatio( lua_tonumber( L, 3 ) );
		// 		}
		// 	}
		// 	else if ( 0 == strcmp( "joint1", key ) )
		// 	{
		// 		// No-op for read-only property
		// 	}
		// 	else if ( 0 == strcmp( "joint2", key ) )
		// 	{
		// 		// No-op for read-only property
		// 	}
		// }

		else if ( jointType == b2_weldJoint )
		{
			//////////////////////////////////////////////////////////////////////////////
			// b2WeldJoint *joint = (b2WeldJoint*)baseJoint;

			if ( 0 == strcmp( "referenceAngle", key ) )  // read-only
			{
				// No-op for read-only property
				if ( lua_isnumber( L, 3 ) )
				{
					Rtt_Real valueRadians = Rtt_RealDegreesToRadians( luaL_toreal( L, 3 ) );
					// b2Joint_SetReferenceAngle( baseJoint, Rtt_RealToFloat( valueRadians ) );
					b2Transform localFrameA = b2Joint_GetLocalFrameA( baseJoint );
					localFrameA.q = b2MakeRot( Rtt_RealToFloat( valueRadians ) );
					b2Joint_SetLocalFrameA( baseJoint, localFrameA );
				}
			}
			else if ( 0 == strcmp( "frequency", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					// joint->SetFrequency( lua_tonumber( L, 3 ) );
					b2WeldJoint_SetLinearHertz( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "dampingRatio", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					// joint->SetDampingRatio( lua_tonumber( L, 3 ) );
					b2WeldJoint_SetLinearDampingRatio( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "angularFrequency", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2WeldJoint_SetAngularHertz(baseJoint, lua_tonumber( L, 3 ) );
				}
			}
			else if ( 0 == strcmp( "angularDampingRatio", key ) )
			{
				if ( lua_isnumber( L, 3 ) )
				{
					b2WeldJoint_SetAngularDampingRatio( baseJoint, lua_tonumber( L, 3 ) );
				}
			}
		}

		// else if ( jointType == e_ropeJoint )
		// {
		// 	//////////////////////////////////////////////////////////////////////////////
		// 	b2RopeJoint *joint = (b2RopeJoint*)baseJoint;

		// 	if ( 0 == strcmp( "maxLength", key ) )
		// 	{
		// 		if ( lua_isnumber( L, 3 ) )
		// 		{
		// 			float value = lua_tonumber( L, 3 );
		// 			const PhysicsWorld& physics = LuaContext::GetRuntime( L )->GetPhysicsWorld();
		// 			value = LuaLibPhysics::ToMKS( LuaLibPhysics::kLengthUnitType, physics, value );
		// 			joint->SetMaxLength( value );
		// 		}
		// 	}
		// 	else if ( 0 == strcmp( "limitState", key ) )
		// 	{
		// 		// No-op for read-only property
		// 	}
		// }

	}

	return 0;
}


int
PhysicsJoint::Finalizer( lua_State *L )
{
	JointUserdataWrapper **ud = (JointUserdataWrapper **)luaL_checkudata( L, 1, Self::kMetatableName );
	if ( ud )
	{
		JointUserdataWrapper *wrapper = *ud;

		b2JointId joint = wrapper->Dereference();
		if ( b2Joint_IsValid(joint) )
		{
			// Make sure joint no longer points to this wrapper that we're about to destroy
			b2Joint_SetUserData( joint, NULL );
		}

		Rtt_DELETE( wrapper );
	}

	return 0;
}

// Call this to init metatable
void
PhysicsJoint::Initialize( lua_State *L )
{
	Rtt_LUA_STACK_GUARD( L );

	const luaL_Reg kVTable[] =
	{
		{ "__index", Self::ValueForKey },
		{ "__newindex", Self::SetValueForKey },
		{ "__gc", Self::Finalizer },
		{ NULL, NULL }
	};

	Lua::InitializeMetatable( L, Self::kMetatableName, kVTable );
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // Rtt_PHYSICS
