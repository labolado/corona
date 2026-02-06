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

#include "Rtt_PhysicsContactListener.h"

#include "Display/Rtt_DisplayObject.h"
#include "Rtt_Runtime.h"
#include "Rtt_Event.h"
#include "Rtt_LuaContext.h"
#include "Rtt_PhysicsContact.h"
#include "Rtt_PhysicsWorld.h"

#include "box2d/box2d.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

PhysicsContactListener::PhysicsContactListener( Runtime& runtime )
:	fRuntime( runtime )
{
}

void
PhysicsContactListener::BeginContact(b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2ContactId contactId)
{
	const PhysicsWorld& physics = fRuntime.GetPhysicsWorld();

	if ( ! physics.IsProperty( PhysicsWorld::kCollisionListenerExists ) )
	{
		// Nothing to do.
		return;
	}

	const char phase[] = "began";

	// b2Fixture *fixtureA = contact->GetFixtureA();
	// b2Fixture *fixtureB = contact->GetFixtureB();

	size_t fixtureIndex1 = (size_t)b2Shape_GetUserData( shapeIdA );
	size_t fixtureIndex2 = (size_t)b2Shape_GetUserData( shapeIdB );

	b2BodyId bodyA = b2Shape_GetBody( shapeIdA );
	b2BodyId bodyB = b2Shape_GetBody( shapeIdB );

	DisplayObject *object1 = static_cast< DisplayObject* >( b2Body_GetUserData(bodyA) );
	DisplayObject *object2 = static_cast< DisplayObject* >( b2Body_GetUserData(bodyB) );

	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	//// Lots of redundant code here.
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	////
	////
	// Get the out_position.
	b2Vec2 position( b2Vec2_zero );

	// Real normalImpulse = 0.0f;
	// Real tangentImpulse = 0.0f;

	int capacity = b2Shape_GetContactCapacity( shapeIdA );
	// b2ContactData contactData[capacity];
	// b2Manifold manifold = { 0 };
	// int count = b2Shape_GetContactData( shapeIdA, contactData, capacity );
	// for ( int i = 0; i < count; ++i )
	// {
	// 	if ( B2_ID_EQUALS( contactData[i].shapeIdB, shapeIdB ) ) {
	// 		manifold = contactData[i].manifold;
	// 		break;
	// 	}
	// }
	// It's possible for manifold->pointCount to be 0 (in the case of sensors).
	// b2Manifold *manifold = contact->GetManifold();
	if ( b2Contact_IsValid( contactId ) )
	{
		b2ContactData data = b2Contact_GetData( contactId );
		Real scale = physics.GetPixelsPerMeter();

		// "1": If we don't average all positions, then we only return the first one.
		int32 point_count = ( physics.GetAverageCollisionPositions() ? data.manifold.pointCount : 1 );

		if( physics.GetReportCollisionsInContentCoordinates() )
		{
			// Get the contact points in content-space.
			// b2WorldManifold worldManifold;
			// contact->GetWorldManifold( &worldManifold );

			// Sum.
			for ( int32 i = 0;
					i < point_count;
					++i )
			{
				position += data.manifold.points[ i ].point;
				// normalImpulse = b2MaxFloat( normalImpulse, manifold.points[ i ].normalImpulse );
				// tangentImpulse = b2MaxFloat( tangentImpulse, manifold.points[ i ].tangentImpulse );
			}
		}
		else
		{
			// Get the contact points in local-space.

			// Sum.
			for ( int32 i = 0;
					i < point_count;
					++i )
			{
				// position += manifold->points[ i ].localPoint;
				position += data.manifold.points[ i ].anchorA;
				// normalImpulse = b2MaxFloat( normalImpulse, manifold.points[ i ].normalImpulse );
				// tangentImpulse = b2MaxFloat( tangentImpulse, manifold.points[ i ].tangentImpulse );
			}
		}

		// Average.
		position *= ( 1.0f / (Real)point_count );

		// Scale.
		position *= scale;
	}
	////
	////
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////

	if ( object1 && ! object1->IsOrphan()
		 && object2 && ! object2->IsOrphan() )
	{
		// UserdataWrapper *contactWrapper = PhysicsContact::CreateWrapper( fRuntime.VMContext().LuaState(), contact );
		{
			CollisionEvent e( * object1, * object2, position.x, position.y, (int) fixtureIndex1, (int) fixtureIndex2, phase );
			// e.SetContact( contactWrapper );
			e.SetContact( NULL );

			fRuntime.DispatchEvent( e );
		}
		// contactWrapper->Invalidate();
	}
}

void
PhysicsContactListener::EndContact(b2ShapeId shapeIdA, b2ShapeId shapeIdB)
{
	const PhysicsWorld& physics = fRuntime.GetPhysicsWorld();

	if ( ! physics.IsProperty( PhysicsWorld::kCollisionListenerExists ) )
	{
		// Nothing to do.
		return;
	}

	const char phase[] = "ended";

	// b2Fixture *fixtureA = contact->GetFixtureA();
	// b2Fixture *fixtureB = contact->GetFixtureB();

	size_t fixtureIndex1 = (size_t)b2Shape_GetUserData( shapeIdA );
	size_t fixtureIndex2 = (size_t)b2Shape_GetUserData( shapeIdB );

	b2BodyId bodyA = b2Shape_GetBody( shapeIdA );
	b2BodyId bodyB = b2Shape_GetBody( shapeIdB );

	DisplayObject *object1 = static_cast< DisplayObject* >( b2Body_GetUserData(bodyA) );
	DisplayObject *object2 = static_cast< DisplayObject* >( b2Body_GetUserData(bodyB) );

	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	//// Lots of redundant code here.
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	////
	////
	// Get the out_position.
	b2Vec2 position( b2Vec2_zero );

	/*
	// It's possible for manifold->pointCount to be 0 (in the case of sensors).
	b2Manifold *manifold = contact->GetManifold();
	if( manifold->pointCount )
	{
		Real scale = physics.GetPixelsPerMeter();

		// "1": If we don't average all positions, then we only return the first one.
		int32 point_count = ( physics.GetAverageCollisionPositions() ? manifold->pointCount : 1 );

		if( physics.GetReportCollisionsInContentCoordinates() )
		{
			// Get the contact points in content-space.
			b2WorldManifold worldManifold;
			contact->GetWorldManifold( &worldManifold );

			// Sum.
			for ( int32 i = 0;
					i < point_count;
					++i )
			{
				position += worldManifold.points[ i ];
			}
		}
		else
		{
			// Get the contact points in local-space.

			// Sum.
			for ( int32 i = 0;
					i < point_count;
					++i )
			{
				position += manifold->points[ i ].localPoint;
			}
		}

		// Average.
		position *= ( 1.0f / (Real)point_count );

		// Scale.
		position *= scale;
	}
	*/
	////
	////
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////

	if ( object1 && ! object1->IsOrphan()
		 && object2 && ! object2->IsOrphan() )
	{
		// UserdataWrapper *contactWrapper = PhysicsContact::CreateWrapper( fRuntime.VMContext().LuaState(), contact );
		{
			CollisionEvent e( * object1, * object2, position.x, position.y, (int) fixtureIndex1, (int) fixtureIndex2, phase );
			// e.SetContact( contactWrapper );
			e.SetContact( NULL );

			fRuntime.DispatchEvent( e );
		}
		// contactWrapper->Invalidate();
	}
}

void PhysicsContactListener::BeginContactHit( b2ContactHitEvent *hitEvent )
{
	const PhysicsWorld& physics = fRuntime.GetPhysicsWorld();

	if ( ! physics.IsProperty( PhysicsWorld::kHitCollisionListenerExists ) )
	{
		// Nothing to do.
		return;
	}

	size_t fixtureIndex1 = (size_t)b2Shape_GetUserData( hitEvent->shapeIdA );
	size_t fixtureIndex2 = (size_t)b2Shape_GetUserData( hitEvent->shapeIdB );

	b2BodyId bodyA = b2Shape_GetBody( hitEvent->shapeIdA );
	b2BodyId bodyB = b2Shape_GetBody( hitEvent->shapeIdB );

	DisplayObject *object1 = static_cast< DisplayObject* >( b2Body_GetUserData(bodyA) );
	DisplayObject *object2 = static_cast< DisplayObject* >( b2Body_GetUserData(bodyB) );

	if ( object1 && ! object1->IsOrphan()
		 && object2 && ! object2->IsOrphan() )
	{
		HitCollisionEvent e( * object1, * object2, hitEvent->point.x, hitEvent->point.y, (int) fixtureIndex1, (int) fixtureIndex2,
			hitEvent->approachSpeed, hitEvent->normal.x, hitEvent->normal.y );
		e.SetContact( NULL );

		fRuntime.DispatchEvent( e );
	}
}

bool
PhysicsContactListener::PreSolve( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Vec2 point, b2Vec2 normal )
{
	const PhysicsWorld& physics = fRuntime.GetPhysicsWorld();

	if ( ! physics.IsProperty( PhysicsWorld::kPreCollisionListenerExists ) )
	{
		// Nothing to do.
		return true;
	}

	size_t fixtureIndex1 = (size_t)b2Shape_GetUserData( shapeIdA );
	size_t fixtureIndex2 = (size_t)b2Shape_GetUserData( shapeIdB );

	b2BodyId bodyA = b2Shape_GetBody( shapeIdA );
	b2BodyId bodyB = b2Shape_GetBody( shapeIdB );

	DisplayObject *object1 = static_cast< DisplayObject* >( b2Body_GetUserData(bodyA) );
	DisplayObject *object2 = static_cast< DisplayObject* >( b2Body_GetUserData(bodyB) );

	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	//// Lots of redundant code here.
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	////
	////
	// Get the out_position.
	// b2Vec2 position( b2Vec2_zero );

	// It's possible for manifold->pointCount to be 0 (in the case of sensors).
	// b2Manifold *manifold = contact->GetManifold();
	// if( manifold->pointCount )
	// {
	// 	Real scale = physics.GetPixelsPerMeter();

	// 	// "1": If we don't average all positions, then we only return the first one.
	// 	int32 point_count = ( physics.GetAverageCollisionPositions() ? manifold->pointCount : 1 );

	// 	if( physics.GetReportCollisionsInContentCoordinates() )
	// 	{
	// 		// Get the contact points in content-space.
	// 		// b2WorldManifold worldManifold;
	// 		// contact->GetWorldManifold( &worldManifold );

	// 		// Sum.
	// 		for ( int32 i = 0;
	// 				i < point_count;
	// 				++i )
	// 		{
	// 			position += manifold->points[ i ].point;
	// 		}
	// 	}
	// 	else
	// 	{
	// 		// Get the contact points in local-space.

	// 		// Sum.
	// 		for ( int32 i = 0;
	// 				i < point_count;
	// 				++i )
	// 		{
	// 			position += b2Body_GetLocalPoint( bodyA, manifold->points[ i ].point );
	// 		}
	// 	}

	// 	// Average.
	// 	position *= ( 1.0f / (Real)point_count );

	// 	// Scale.
	// 	position *= scale;
	// }
	////
	////
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////

	if ( object1 && ! object1->IsOrphan()
		 && object2 && ! object2->IsOrphan() )
	{

		float separation = 0.0f;
		// for ( int i = 0; i < manifold->pointCount; ++i )
		// {
		// 	float s = manifold->points[i].separation;
		// 	separation = separation < s ? separation : s;
		// }
		// contact->separation = separation;

		bool isEnabled = true;
		{
			Box2dPreSolveTempContact contact;
			contact.separation = separation;
			contact.normalX = normal.x;
			contact.normalY = normal.y;

			PreCollisionEvent e( * object1, * object2, point.x, point.y, fixtureIndex1, fixtureIndex2, fRuntime, &contact );
			{
				std::lock_guard<std::mutex> lock(fDispatchEventMutex);
				isEnabled = e.DispatchWithResult( fRuntime.VMContext().L(), fRuntime );
			}
		}
		return isEnabled;
	}
	return true;
}

/*
void
PhysicsContactListener::PostSolve(b2Contact* contact, const b2ContactImpulse* impulse)
{
	const PhysicsWorld& physics = fRuntime.GetPhysicsWorld();

	if ( ! physics.IsProperty( PhysicsWorld::kPostCollisionListenerExists ) )
	{
		// Nothing to do.
		return;
	}

	b2Fixture *fixtureA = contact->GetFixtureA();
	b2Fixture *fixtureB = contact->GetFixtureB();

	size_t fixtureIndex1 = (size_t)fixtureA->GetUserData();
	size_t fixtureIndex2 = (size_t)fixtureB->GetUserData();

	b2Body *bodyA = fixtureA->GetBody();
	b2Body *bodyB = fixtureB->GetBody();

	DisplayObject *object1 = static_cast< DisplayObject* >( bodyA->GetUserData() );
	DisplayObject *object2 = static_cast< DisplayObject* >( bodyB->GetUserData() );

	float32 maxNormalImpulse = 0.0f;
	float32 maxTangentImpulse = 0.0f;

	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	//// Lots of redundant code here.
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	////
	////
	// Get the out_position.
	b2Vec2 position( b2Vec2_zero );

	// It's possible for manifold->pointCount to be 0 (in the case of sensors).
	b2Manifold *manifold = contact->GetManifold();
	if( manifold->pointCount )
	{
		Real scale = physics.GetPixelsPerMeter();

		// "1": If we don't average all positions, then we only return the first one.
		int32 point_count = ( physics.GetAverageCollisionPositions() ? manifold->pointCount : 1 );

		if( physics.GetReportCollisionsInContentCoordinates() )
		{
			// Get the contact points in content-space.
			b2WorldManifold worldManifold;
			contact->GetWorldManifold( &worldManifold );

			// Sum.
			for ( int32 i = 0;
					i < point_count;
					++i )
			{
				position += worldManifold.points[ i ];
			}
		}
		else
		{
			// Get the contact points in local-space.

			// Sum.
			for ( int32 i = 0;
					i < point_count;
					++i )
			{
				position += manifold->points[ i ].localPoint;
			}
		}

		// Average.
		position *= ( 1.0f / (Real)point_count );

		// Scale.
		position *= scale;
	////
	////
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////

		// For the contact forces, we take the maximum within each set
		int32 count = contact->GetManifold()->pointCount;
		for (int32 i = 0; i < count; ++i) {
			maxNormalImpulse = b2Max( maxNormalImpulse, impulse->normalImpulses[i] );
			maxTangentImpulse = b2Max( maxTangentImpulse, impulse->tangentImpulses[i] );
		}
	}

	if ( object1 && ! object1->IsOrphan()
		 && object2 && ! object2->IsOrphan() )
	{
		UserdataWrapper *contactWrapper = PhysicsContact::CreateWrapper( fRuntime.VMContext().LuaState(), contact );
		{
			PostCollisionEvent e( * object1, * object2, position.x, position.y, (int) fixtureIndex1, (int) fixtureIndex2, Rtt_FloatToReal( maxNormalImpulse ), Rtt_FloatToReal( maxTangentImpulse ) );
			e.SetContact( contactWrapper );

			fRuntime.DispatchEvent( e );
		}
		contactWrapper->Invalidate();
	}
}
*/

void PhysicsContactListener::BeginParticleContact( b2ParticleSystem *particleSystem,
											b2ParticleBodyContact *particleBodyContact )
{
	const PhysicsWorld& physics = fRuntime.GetPhysicsWorld();

	if( ! physics.IsProperty( PhysicsWorld::kParticleCollisionListenerExists ) )
	{
		// Nothing to do.
		return;
	}

	fRuntime.DispatchEvent( BeginParticleCollisionEvent( fRuntime,
															particleSystem,
															particleBodyContact ) );
}

void PhysicsContactListener::EndParticleContact( b2ShapeId fixture,
											b2ParticleSystem *particleSystem,
											int32 particleIndex )
{
	const PhysicsWorld& physics = fRuntime.GetPhysicsWorld();

	if( ! physics.IsProperty( PhysicsWorld::kParticleCollisionListenerExists ) )
	{
		// Nothing to do.
		return;
	}

	fRuntime.DispatchEvent( EndParticleCollisionEvent( fRuntime,
														fixture,
														particleSystem,
														particleIndex ) );
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // Rtt_PHYSICS
