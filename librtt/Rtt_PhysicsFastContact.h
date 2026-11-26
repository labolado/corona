//////////////////////////////////////////////////////////////////////////////
// Internal thread-safe fast-track contact policies shared by the default
// PhysicsWorld and independent LuaPhysicsWorld instances:
//   1. Per-world material-pair friction/restitution override table wired to
//      Box2D's native b2World_SetFrictionCallback / b2World_SetRestitutionCallback.
//   2. Declarative one-way surface suppression evaluated in worker threads via
//      b2World_SetParallelPreSolveCallback alongside compound internal edges.
// This file is part of the Solar2D engine. See the repository LICENSE.
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_PhysicsFastContact_H__
#define _Rtt_PhysicsFastContact_H__

#include "box2d/box2d.h"
#include "Rtt_PhysicsCompoundEdges.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Rtt
{
namespace PhysicsFastContact
{

struct MaterialPairEntry
{
	uint32_t idA; // canonical: idA <= idB
	uint32_t idB;
	bool hasFriction;
	float friction;
	bool hasRestitution;
	float restitution;
};

struct OneWaySurface
{
	b2BodyId bodyId;
	b2ShapeId shapeId;
	b2Vec2 localNormal; // unit vector pointing toward blocking side in body-local space
	float minDot;       // minimum dot(outwardNormal, worldBlockingNormal) required to block
	uint64_t maskBits;  // category mask of colliding shapes subject to one-way pass-through
	bool requireFullExit; // when true, stay latched in pass-through until geometric separation
};

// Encode a 32-bit user material id with the owning world's 1-based slot index
// so Box2D's contextless friction/restitution callbacks can look up per-world
// material pair overrides lock-free from worker threads.
inline uint64_t EncodeUserMaterialId( b2WorldId worldId, uint32_t userId )
{
	if ( userId == 0 || ! b2World_IsValid( worldId ) )
	{
		return 0ULL;
	}
	return ( (uint64_t)worldId.index1 << 32 ) | (uint64_t)userId;
}

inline uint32_t DecodeUserMaterialId( uint64_t rawMaterialId )
{
	return (uint32_t)( rawMaterialId & 0xFFFFFFFFULL );
}

// Per-world material pair registry (defined in Rtt_PhysicsWorld.cpp to avoid
// ODR duplication across translation units). Mutate only while world is unlocked.
void ClearMaterialPairs( b2WorldId worldId );
void SetMaterialPair( b2WorldId worldId, uint32_t idA, uint32_t idB,
					  bool hasFriction, float friction,
					  bool hasRestitution, float restitution );
void RemoveMaterialPair( b2WorldId worldId, uint32_t idA, uint32_t idB );
bool GetMaterialPair( b2WorldId worldId, uint32_t idA, uint32_t idB, MaterialPairEntry& outEntry );
size_t GetMaterialPairCount( b2WorldId worldId );

// Declarative one-way surface helper (read-only during worker callbacks).
inline bool CheckOneWayPenetrationVelocity( const OneWaySurface& rule, b2ShapeId otherShapeId,
											b2Vec2 worldNormal, float deepestSeparation )
{
	float unit = b2GetLengthUnitsPerMeter();
	if ( deepestSeparation < -0.01f * unit )
	{
		b2BodyId otherBodyId = b2Shape_GetBody( otherShapeId );
		b2Vec2 vOther = b2Body_GetLinearVelocity( otherBodyId );
		b2Vec2 vPlatform = b2Body_GetLinearVelocity( rule.bodyId );
		float relNormalVel = b2Dot( b2Sub( vOther, vPlatform ), worldNormal );
		if ( relNormalVel > 0.05f * unit )
		{
			return true;
		}
	}
	return false;
}

inline void EvaluateOneWayRule( const OneWaySurface& rule, b2ShapeId otherShapeId,
								b2Vec2 outwardNormal, float deepestSeparation,
								bool checkPenetrationVelocity, const bool* inLatched,
								bool& outSuppress, bool& outAnyRequireFullExit, bool& outKeepLatch )
{
	if ( ! b2Body_IsValid( rule.bodyId ) || ! b2Shape_IsValid( otherShapeId ) )
	{
		return;
	}

	b2Filter otherFilter = b2Shape_GetFilter( otherShapeId );
	if ( ( otherFilter.categoryBits & rule.maskBits ) == 0 )
	{
		return;
	}

	b2Vec2 worldNormal = b2RotateVector( b2Body_GetRotation( rule.bodyId ), rule.localNormal );
	float alignment = b2Dot( outwardNormal, worldNormal );

	if ( rule.requireFullExit && inLatched != NULL )
	{
		outAnyRequireFullExit = true;
		if ( deepestSeparation > 0.0f )
		{
			// Speculative contact (geometrically separated).
			if ( alignment < rule.minDot )
			{
				// Approaching from non-blocking/side direction: suppress and pre-latch.
				outSuppress = true;
				outKeepLatch = true;
			}
			// Else: separated on blocking side -> clear latch and allow normal landing.
			return;
		}

		// True geometric contact / overlap (deepestSeparation <= 0.0f).
		if ( *inLatched )
		{
			// Stay suppressed unconditionally until full geometric exit.
			outSuppress = true;
			outKeepLatch = true;
			return;
		}

		if ( alignment < rule.minDot ||
			 ( checkPenetrationVelocity &&
			   CheckOneWayPenetrationVelocity( rule, otherShapeId, worldNormal, deepestSeparation ) ) )
		{
			outSuppress = true;
			outKeepLatch = true;
		}
		return;
	}

	// Stateless mode (requireFullExit == false) or CCD (inLatched == NULL).
	if ( alignment < rule.minDot ||
		 ( checkPenetrationVelocity &&
		   CheckOneWayPenetrationVelocity( rule, otherShapeId, worldNormal, deepestSeparation ) ) )
	{
		outSuppress = true;
	}
}

inline bool ShouldSuppressOneWay( const std::vector<OneWaySurface>& rules,
								  b2ShapeId shapeIdA, b2ShapeId shapeIdB,
								  b2Vec2 normal, float deepestSeparation,
								  bool checkPenetrationVelocity,
								  bool* latched = NULL )
{
	bool suppress = false;
	bool anyRequireFullExit = false;
	bool keepLatch = false;

	for ( size_t i = 0; i < rules.size(); ++i )
	{
		const OneWaySurface& rule = rules[i];
		if ( B2_ID_EQUALS( rule.shapeId, shapeIdA ) )
		{
			EvaluateOneWayRule( rule, shapeIdB, normal, deepestSeparation,
								checkPenetrationVelocity, latched,
								suppress, anyRequireFullExit, keepLatch );
		}
		else if ( B2_ID_EQUALS( rule.shapeId, shapeIdB ) )
		{
			b2Vec2 flipped = { -normal.x, -normal.y };
			EvaluateOneWayRule( rule, shapeIdA, flipped, deepestSeparation,
								checkPenetrationVelocity, latched,
								suppress, anyRequireFullExit, keepLatch );
		}
	}

	if ( latched != NULL )
	{
		*latched = anyRequireFullExit ? keepLatch : false;
	}
	return suppress;
}

template <typename Edges>
inline bool EvaluateParallelPreContinuous( bool compoundEnabled, const Edges& compoundEdges,
										   const std::vector<OneWaySurface>& oneWaySurfaces,
										   b2ShapeId shapeIdA, b2ShapeId shapeIdB,
										   b2Vec2 point, b2Vec2 normal )
{
	if ( compoundEnabled && ! compoundEdges.empty() &&
		 PhysicsCompoundEdges::Suppress( compoundEdges, shapeIdA, shapeIdB, point, normal ) )
	{
		return false;
	}
	if ( ! oneWaySurfaces.empty() &&
		 ShouldSuppressOneWay( oneWaySurfaces, shapeIdA, shapeIdB, normal, 0.0f, false, NULL ) )
	{
		return false;
	}
	return true;
}

template <typename Edges>
inline void EvaluateParallelPreSolve( bool compoundEnabled, const Edges& compoundEdges,
									  const std::vector<OneWaySurface>& oneWaySurfaces,
									  b2ShapeId shapeIdA, b2ShapeId shapeIdB,
									  b2Manifold* manifold, bool* latched )
{
	if ( manifold->pointCount == 0 )
	{
		if ( latched != NULL )
		{
			*latched = false;
		}
		return;
	}

	int deepest = 0;
	for ( int i = 1; i < manifold->pointCount; ++i )
	{
		if ( manifold->points[i].separation < manifold->points[deepest].separation )
		{
			deepest = i;
		}
	}

	const b2ManifoldPoint& mp = manifold->points[deepest];
	if ( compoundEnabled && ! compoundEdges.empty() )
	{
		b2Vec2 originA = b2Body_GetPosition( b2Shape_GetBody( shapeIdA ) );
		if ( PhysicsCompoundEdges::Suppress( compoundEdges, shapeIdA, shapeIdB,
											 b2Add( originA, mp.anchorA ), manifold->normal ) )
		{
			manifold->pointCount = 0;
			return;
		}
	}

	if ( ! oneWaySurfaces.empty() &&
		 ShouldSuppressOneWay( oneWaySurfaces, shapeIdA, shapeIdB, manifold->normal, mp.separation, true, latched ) )
	{
		manifold->pointCount = 0;
	}
	else if ( oneWaySurfaces.empty() && latched != NULL )
	{
		*latched = false;
	}
}

inline void SetOrUpdateOneWay( std::vector<OneWaySurface>& rules, const OneWaySurface& rule )
{
	for ( size_t i = 0; i < rules.size(); ++i )
	{
		if ( B2_ID_EQUALS( rules[i].shapeId, rule.shapeId ) )
		{
			rules[i] = rule;
			return;
		}
	}
	rules.push_back( rule );
}

inline bool RemoveOneWayForShape( std::vector<OneWaySurface>& rules, b2ShapeId shapeId )
{
	bool removed = false;
	size_t writeIndex = 0;
	for ( size_t i = 0; i < rules.size(); ++i )
	{
		if ( B2_ID_EQUALS( rules[i].shapeId, shapeId ) )
		{
			removed = true;
		}
		else
		{
			rules[writeIndex++] = rules[i];
		}
	}
	rules.resize( writeIndex );
	return removed;
}

inline bool RemoveOneWayForBody( std::vector<OneWaySurface>& rules, b2BodyId bodyId )
{
	bool removed = false;
	size_t writeIndex = 0;
	for ( size_t i = 0; i < rules.size(); ++i )
	{
		if ( B2_ID_EQUALS( rules[i].bodyId, bodyId ) )
		{
			removed = true;
		}
		else
		{
			rules[writeIndex++] = rules[i];
		}
	}
	rules.resize( writeIndex );
	return removed;
}

inline const OneWaySurface* FindOneWayForShape( const std::vector<OneWaySurface>& rules, b2ShapeId shapeId )
{
	for ( size_t i = 0; i < rules.size(); ++i )
	{
		if ( B2_ID_EQUALS( rules[i].shapeId, shapeId ) )
		{
			return &rules[i];
		}
	}
	return NULL;
}

template <typename Edges>
inline bool HasCompoundEdgeForShape( const Edges& edges, b2ShapeId shapeId )
{
	for ( size_t i = 0; i < edges.size(); ++i )
	{
		if ( B2_ID_EQUALS( edges[i].shapeId, shapeId ) )
		{
			return true;
		}
	}
	return false;
}

} // namespace PhysicsFastContact
} // namespace Rtt

#endif // _Rtt_PhysicsFastContact_H__
