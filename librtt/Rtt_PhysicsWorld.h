//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_PhysicsWorld_H__
#define _Rtt_PhysicsWorld_H__

// ----------------------------------------------------------------------------

#include "box2d/box2d.h"
#include "liquid_world.h"

#include <vector>

// class b2Body;
// class b2DebugDraw;
// class b2World;
// class b2DestructionListener;

namespace Rtt
{

class b2GLESDebugDraw;
class PhysicsContactListener;
class Runtime;
class Renderer;

static constexpr int32_t estimateMaxMouseBodies = 32;

// ----------------------------------------------------------------------------

class PhysicsWorld
{
	public:
		enum
		{
			// Physics-related
			kIsWorldRunning						= 0x01,
			kCollisionListenerExists			= 0x02,
			kPreCollisionListenerExists			= 0x04,
			kPostCollisionListenerExists		= 0x08,
			kParticleCollisionListenerExists	= 0x10,
			kHitCollisionListenerExists	= 0x20,
			kRuntimePreCollisionListenerExists = 0x40,
		};

		typedef U32 Properties;

	public:
		PhysicsWorld( Rtt_Allocator& allocator );
		~PhysicsWorld();

	public:
		void Initialize( float frameInterval );

	public:
		void WillDestroyDisplay();

	public:
		void StartWorld( Runtime& runtime, bool noSleep );
		void PauseWorld();
		void ResumeWorld();
		void StopWorld();
		void onSuspended();
		void onResumed();

		b2BodyId FetchUsableMouseBodyId();

		b2LiquidWorld* GetWorld() const { return fWorld; }
		b2WorldId GetWorldId() const { return fWorld->GetWorldId(); }
		// b2Body* GetGroundBody() const { return fGroundBody; }

		bool IsWorldValid() const { return fWorld != NULL && b2World_IsValid(fWorld->GetWorldId()); }

	public:
		Rtt_Allocator *Allocator() const { return & fAllocator; }
		bool IsProperty( Properties mask ) const { return (fProperties & mask) != 0; }
		void ToggleProperty( Properties mask ) { fProperties ^= mask; }
		void SetProperty( Properties mask, bool value );
		void SetRuntimePreCollisionListenerExists( bool value );
		void RegisterPhysicsBody( b2BodyId bodyId );
		void DestroyPhysicsBody( b2BodyId bodyId );
		void InvalidateCompoundInternalEdges( b2BodyId bodyId );
		void RefreshCompoundInternalEdges( b2BodyId bodyId );
		void ReleaseCompoundInternalEdgePreSolveOwnership( b2ShapeId shapeId );

		void SetCompoundInternalEdgeSuppressionEnabled( bool enabled );
		bool GetCompoundInternalEdgeSuppressionEnabled() const { return fCompoundInternalEdgeSuppressionEnabled; }
		bool ShouldSuppressCompoundInternalEdge( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Vec2 point,
										 b2Vec2 normal ) const;

		// Default is 30 (content) pixels per meter so the range [3, 300] pixels
		// maps to [0.1, 10] meters (the optimal length scale range for Box2D)
		Real GetPixelsPerMeter() const { return fPixelsPerMeter; }
		Real GetMetersPerPixel() const { return ( 1.0f / fPixelsPerMeter ); }
		void SetPixelsPerMeter( Real newValue ) { fPixelsPerMeter = newValue; }
		S32 GetVelocityIterations() const { return fVelocityIterations; }
		void SetVelocityIterations( S32 newValue ) { fVelocityIterations = newValue; }
		S32 GetPositionIterations() const { return fPositionIterations; }
		void SetPositionIterations( S32 newValue ) { fPositionIterations = newValue; }

		float GetTimeStep() const { return fTimeStep; }
		void SetTimeStep( float newValue );

		float GetTimeScale() const { return fTimeScale; }
		void SetTimeScale( float newValue ) { fTimeScale = newValue; }

		int GetNumSteps() const { return fNumSteps; }
		void SetNumSteps( S32 newValue ) { fNumSteps = newValue; }

		int GetSubSteps() const { return fSubStepCount; }
		void SetSubSteps( S32 newValue ) { fSubStepCount = newValue; }

		int GetWorkerCount() const { return fWorkerCount; }
		void SetWorkerCount( int newValue );

		int GetNumHardwareThreads() const;

	private:
		void StepEvents();
		void FlushDeferredBodyDestructions();
		void UnregisterPhysicsBody( b2BodyId bodyId );
		void RemoveCompoundInternalEdges( b2BodyId bodyId, bool disablePreSolveEvents );
		void BuildCompoundInternalEdges( b2BodyId bodyId );
		void EnableCompoundInternalEdgePreSolve( b2BodyId bodyId, b2ShapeId shapeId );

		struct CompoundInternalEdgePreSolveShape
		{
			b2BodyId bodyId;
			b2ShapeId shapeId;
		};

		struct CompoundInternalEdge
		{
			b2BodyId bodyId;
			b2ShapeId shapeId;
			b2Vec2 point1;
			b2Vec2 point2;
			b2Vec2 normal;
		};

	public:
		void SetReportCollisionsInContentCoordinates( bool enabled );
		bool GetReportCollisionsInContentCoordinates() const;

		void SetLuaAssertEnabled( bool enabled );
		bool GetLuaAssertEnabled() const;

		void SetAverageCollisionPositions( bool enabled );
		bool GetAverageCollisionPositions() const;

	public:
		void DebugDraw( Renderer &renderer ) const;

	public:
		void StepWorld( double elapsedMS );

	private:
		Rtt_Allocator& fAllocator;
		b2GLESDebugDraw *fWorldDebugDraw;
		// b2DestructionListener *fWorldDestructionListener;
		PhysicsContactListener *fWorldContactListener;

		U32 fProperties;
		// b2WorldId fWorldId;
		b2LiquidWorld *fWorld;
		Real fPixelsPerMeter;
		// b2Body *fGroundBody;
		S32 fSubStepCount;
		S32 fVelocityIterations;
		S32 fPositionIterations;
		float fFrameInterval;
		float fTimeStep;
		float fTimeScale;
		float fTimePrevious;
		float fTimeRemainder;

		S32 fNumSteps;
		bool fCompoundInternalEdgeSuppressionEnabled;
		std::vector<b2BodyId> fPhysicsBodies;
		std::vector<b2BodyId> fDeferredBodyDestructions;
		std::vector<CompoundInternalEdgePreSolveShape> fCompoundInternalEdgePreSolveShapes;
		std::vector<CompoundInternalEdge> fCompoundInternalEdges;

		//! false: Contact points are reported in local-space.
		//! true: Contact points are reported in content-space.
		bool fReportCollisionsInContentCoordinates;

		//! false: Rtt_LuaAssert are disabled.
		//! true: Rtt_LuaAssert are enabled.
		bool fLuaAssertEnabled;

		//! It's common for Box2D to report multiple contact points during
		//! a single iteration of the simulation.
		//!
		//! How a set of contact points should be handled is game-specific.
		//! Therefore, we have to provide the ability to either get the first
		//! point from the set, or the average of the set.
		//!
		//! By default, we return only the first point.
		//!
		//! false: The point of contact reported is the first one reported by Box2D. The order is arbitrary.
		//! true: The point of contact reported is the average of all contact points.
		bool fAverageCollisionPositions;

		std::vector<b2BodyId> fMouseBodies;

	public:
		int fWorkerCount;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_PhysicsWorld_H__
