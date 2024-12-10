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

#include "b2GLESDebugDraw.h"

#include "Display/Rtt_Display.h"
#include "Display/Rtt_DisplayObject.h"
#include "Rtt_LuaAux.h"
#include "Rtt_LuaLibPhysics.h"
#include "Rtt_Runtime.h"
#include "Rtt_PhysicsContactListener.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

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

static void* EnqueueTask( b2TaskCallback* taskCallback, int32_t itemCount, int32_t minRange, void* taskContext, void* userContext )
{
	PhysicsWorld* world = static_cast<PhysicsWorld*>( userContext );
	if ( world->fTaskCount < maxTasks )
	{
		// Rtt_Log("EnqueueTask, fTaskCount %d", world->fTaskCount);
		PhysicsTask& task = world->fTasks[ world->fTaskCount ];
		task.m_SetSize = itemCount;
		task.m_MinRange = minRange;
		task.m_task = taskCallback;
		task.m_taskContext = taskContext;
		world->fScheduler.AddTaskSetToPipe( &task );
		++world->fTaskCount;
		return &task;
	}
	else
	{
		// This is not fatal but the maxTasks should be increased
		Rtt_Log("Warning: Task queue full, executing task immediately (PhysicsWorld).");

		taskCallback( 0, itemCount, 0, taskContext );
		return nullptr;
	}
}

static void FinishTask( void* taskPtr, void* userContext )
{
	if (taskPtr != nullptr)
	{
		PhysicsTask* task = static_cast<PhysicsTask*>( taskPtr );
		PhysicsWorld* world = static_cast<PhysicsWorld*>( userContext );
		world->fScheduler.WaitforTask( task );
	}
}

static bool PreSolveCallbackFunction( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Manifold* manifold, void* context )
{
	PhysicsContactListener* contactListener = (PhysicsContactListener*) context;
	return contactListener->PreSolve( shapeIdA, shapeIdB, manifold );
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
	fGroundBodyId( b2_nullBodyId ),
	fSubStepCount( kSubStepCount ),
	fVelocityIterations( kVelocityIterations ),
	fPositionIterations( kPositionIterations ),
	fFrameInterval( -1.0f ),
	fTimeStep( -1.0f ), // Set time step equal to frame interval
	fTimeScale( 1.0f ),
	fTimePrevious( -1.f ),
	fTimeRemainder( 0.0f ),
	fNumSteps(1)
{
	fWorkerCount = b2MinInt( 8, b2MaxInt((int)enki::GetNumHardwareThreads() / 4, 1) );
	// fWorkerCount = 1;
	fScheduler.Initialize( fWorkerCount );
	fTaskCount = 0;
}

PhysicsWorld::~PhysicsWorld()
{
	// if ( fWorld )
	// {
	// 	fWorld->SetContactListener( NULL );
	// }

	StopWorld();
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

		// fWorld = Rtt_NEW( Allocator(), b2World( gravity ) );
		// fWorldDestructionListener = Rtt_NEW( Allocator(), PhysicsDestructionListener );
		// fWorld->SetDestructionListener( fWorldDestructionListener );
		b2WorldDef worldDef = b2DefaultWorldDef();
		// Rtt_Log("PhysicsWorld::StartWorld, workerCount = %d", fWorkerCount);
		if ( fWorkerCount > 1 ) {
			worldDef.workerCount = fWorkerCount;
			worldDef.enqueueTask = EnqueueTask;
			worldDef.finishTask = FinishTask;
			worldDef.userTaskContext = this;
		}
		worldDef.gravity = gravity;
		worldDef.enableSleep = !noSleep;
		b2WorldId worldId = b2CreateWorld( &worldDef );

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

// 		uint32 debugFlags =
// 			b2Draw::e_shapeBit |
// 			b2Draw::e_jointBit |
// //			b2Draw::e_aabbBit |
// 			b2Draw::e_pairBit |
// 			b2Draw::e_centerOfMassBit |
// 			b2Draw::e_particleBit;
// 		fWorldDebugDraw->AppendFlags( debugFlags );

		// fWorld->SetDebugDraw( fWorldDebugDraw );

		// Initialize a ground body, so that joints can be attached to "the world"
		// b2BodyDef bd;
		// bd.userData = const_cast< void* >( LuaLibPhysics::GetGroundBodyUserdata() );
		// fGroundBody = fWorld->CreateBody(&bd);
		b2BodyDef bd = b2DefaultBodyDef();
		bd.type = b2_staticBody;
		bd.userData = const_cast< void* >( LuaLibPhysics::GetGroundBodyUserdata() );
		fGroundBodyId = b2CreateBody( fWorld->GetWorldId(), &bd );
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.filter = { 1, 0, 0 };
		shapeDef.isSensor = true;
		shapeDef.enableContactEvents = false;
		shapeDef.enableSensorEvents = false;
		shapeDef.enablePreSolveEvents = false;
		b2Segment segment = { {-20.0f, 0.0f}, {20.0f, 0.0f} };
		b2CreateSegmentShape( fGroundBodyId, &shapeDef, &segment );

		b2World_SetPreSolveCallback( fWorld->GetWorldId(), PreSolveCallbackFunction, fWorldContactListener );
	}

	SetProperty( kIsWorldRunning, true );
}

void
PhysicsWorld::PauseWorld()
{
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
#ifdef Rtt_ANDROID_ENV
	fScheduler.Initialize( fWorkerCount );
	fTaskCount = 0;
#endif
}

void
PhysicsWorld::StopWorld()
{
	if ( fWorld )
	{
		SetProperty( kIsWorldRunning, false );

		// fWorld->SetContactListener( NULL );

		b2DestroyWorld( fWorld->GetWorldId() );
		// fWorldId = b2_nullWorldId;

		// The b2World is about to destroy the block allocator that owns
		// the memory for b2Body objects, so we have to pre-emptively
		// iterate over bodies and detach from display object
		// const void *groundBodyUserdata = LuaLibPhysics::GetGroundBodyUserdata();

		// b2World_GetBodyEvents(b2WorldId worldId)
		// for ( b2Body *body = fWorld->GetBodyList();
		// 	  NULL != body;
		// 	  body = body->GetNext() )
		// {
		// 	if ( body->GetUserData() )
		// 	{
		// 		if ( body->GetUserData() != groundBodyUserdata )
		// 		{
		// 			DisplayObject *o = (DisplayObject*)body->GetUserData();
		// 			o->RemoveExtensions();
		// 		}
		// 	}
		// }

		Rtt_DELETE( fWorld );
		fWorld = NULL;

		// These need to outlive fWorld
		// Rtt_DELETE( fWorldDestructionListener );
		// fWorldDestructionListener = NULL;

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
	if ( fWorld && IsProperty( kIsWorldRunning ) )
	// if ( b2World_IsValid( fWorldId ) && IsProperty( kIsWorldRunning ) )
	{
		// Rtt_Log( "PhysicsWorld::StepWorld, world gravity = (%f, %f)", b2World_GetGravity(fWorldId).x, b2World_GetGravity(fWorldId).y );

		// These values may be changed on the fly. TODO: make sure this isn't occurring real overhead, or we should drop back to default values only!
		// S32 velocityIterations = GetVelocityIterations();
		// S32 positionIterations = GetPositionIterations();

		b2LiquidWorld& world = * fWorld;

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
				fTaskCount = 0;
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
					fTaskCount = 0;
					StepEvents();
				}
				tStep -= dt;
			}

			fTimePrevious = tCurrent;
			fTimeRemainder = tStep;
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

			void* userData = event->userData;
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
						Rtt_ASSERT(b2Vec2_IsValid(position));
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
				b2DestroyBody( event->bodyId );
			}
		}

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
		fWorldContactListener->BeginContact( event.shapeIdA, event.shapeIdB, event.manifold );
	}

	for ( int i = 0; i < contactEvents.endCount; ++i )
	{
		b2ContactEndTouchEvent event = contactEvents.endEvents[i];
		if ( b2Shape_IsValid( event.shapeIdA ) && b2Shape_IsValid( event.shapeIdB ) )
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
		fWorldContactListener->BeginContact( event.sensorShapeId, event.visitorShapeId, { 0 } );
	}

	for ( int i = 0; i < sensorEvents.endCount; ++i )
	{
		b2SensorEndTouchEvent event = sensorEvents.endEvents[i];
		if ( b2Shape_IsValid( event.visitorShapeId ) )
		{
			fWorldContactListener->EndContact( event.sensorShapeId, event.visitorShapeId );
		}
	}
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

