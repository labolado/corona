//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __Rtt_PhysicsContactListener__
#define __Rtt_PhysicsContactListener__

#ifdef Rtt_PHYSICS	

#include "box2d/box2d.h"
#include "liquid_callbacks.h"
#include <mutex>

// ----------------------------------------------------------------------------

namespace Rtt
{

class Runtime;
class DisplayObject;

// ----------------------------------------------------------------------------

class PhysicsContactListener : public b2ContactListener
{
	public:
		PhysicsContactListener( Runtime& runtime );

	public:
		// b2ContactListener
		// Fixture <-> Fixture contact.
		void BeginContact( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2ContactId contactId );
		void EndContact( b2ShapeId shapeIdA, b2ShapeId shapeIdB );
		bool PreSolve( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Vec2 point, b2Vec2 normal );
		// virtual void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse);
		void BeginContactHit( b2ContactHitEvent* hitEvent );

		// b2ContactListener
		// Fixture <-> Particle contact.
		virtual void BeginParticleContact( b2ParticleSystem *particleSystem,
									b2ParticleBodyContact *particleBodyContact );
		virtual void EndParticleContact( b2ShapeId fixture,
									b2ParticleSystem *particleSystem,
									int32 particleIndex );

	private:

		/*
		bool GetCollisionParams( b2Contact* contact,
									DisplayObject *&out_object1,
									DisplayObject *&out_object2,
									b2Vec2 &out_position,
									size_t &out_fixtureIndex1,
									size_t &out_fixtureIndex2 );
		*/

		Runtime& fRuntime;
		std::mutex fDispatchEventMutex;
};


// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // Rtt_PHYSICS	

#endif // __Rtt_PhysicsContactListener__
