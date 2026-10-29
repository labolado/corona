//////////////////////////////////////////////////////////////////////////////
// Internal shared geometry policy for default and independent physics worlds.
// This file is part of the Solar2D engine. See the repository LICENSE.
//////////////////////////////////////////////////////////////////////////////
#ifndef _Rtt_PhysicsCompoundEdges_H__
#define _Rtt_PhysicsCompoundEdges_H__
#include "box2d/box2d.h"
#include <vector>
namespace Rtt
{
namespace PhysicsCompoundEdges
{
// Templates retain the legacy world's existing edge storage and object layout.
// Call only while unlocked to rebuild; suppression only reads during worker callbacks.
template <typename Edges, typename Enable>
void Build( b2BodyId bodyId, Edges& edges, Enable enable )
{
	if ( ! b2Body_IsValid( bodyId ) )
	{
		return;
	}

	int shapeCount = b2Body_GetShapeCount( bodyId );
	if ( shapeCount < 2 )
	{
		return;
	}

	struct PolygonShape
	{
		b2ShapeId shapeId;
		b2Polygon polygon;
	};

	std::vector<b2ShapeId> shapeIds( shapeCount );
	b2Body_GetShapes( bodyId, shapeIds.data(), shapeCount );

	std::vector<PolygonShape> polygons;
	polygons.reserve( shapeCount );
	for ( int i = 0; i < shapeCount; ++i )
	{
		if ( b2Shape_GetType( shapeIds[i] ) == b2_polygonShape )
		{
			PolygonShape entry = { shapeIds[i], b2Shape_GetPolygon( shapeIds[i] ) };
			polygons.push_back( entry );
		}
	}

	if ( polygons.size() < 2 )
	{
		return;
	}

	const float edgeTolerance = 0.5f * 0.005f * b2GetLengthUnitsPerMeter();
	const float edgeToleranceSquared = edgeTolerance * edgeTolerance;
	const float opposingNormalLimit = -0.995f;

	for ( size_t polygonIndexA = 0; polygonIndexA + 1 < polygons.size(); ++polygonIndexA )
	{
		const PolygonShape& polygonShapeA = polygons[polygonIndexA];
		const b2Polygon& polygonA = polygonShapeA.polygon;

		for ( size_t polygonIndexB = polygonIndexA + 1; polygonIndexB < polygons.size(); ++polygonIndexB )
		{
			const PolygonShape& polygonShapeB = polygons[polygonIndexB];
			const b2Polygon& polygonB = polygonShapeB.polygon;

			for ( int edgeIndexA = 0; edgeIndexA < polygonA.count; ++edgeIndexA )
			{
				int nextIndexA = edgeIndexA + 1 < polygonA.count ? edgeIndexA + 1 : 0;
				b2Vec2 pointA1 = polygonA.vertices[edgeIndexA];
				b2Vec2 pointA2 = polygonA.vertices[nextIndexA];

				for ( int edgeIndexB = 0; edgeIndexB < polygonB.count; ++edgeIndexB )
				{
					int nextIndexB = edgeIndexB + 1 < polygonB.count ? edgeIndexB + 1 : 0;
					b2Vec2 pointB1 = polygonB.vertices[edgeIndexB];
					b2Vec2 pointB2 = polygonB.vertices[nextIndexB];

					bool reversedEndpoints = b2DistanceSquared( pointA1, pointB2 ) <= edgeToleranceSquared &&
						b2DistanceSquared( pointA2, pointB1 ) <= edgeToleranceSquared;
					bool opposingNormals = b2Dot( polygonA.normals[edgeIndexA], polygonB.normals[edgeIndexB] ) <= opposingNormalLimit;
					if ( reversedEndpoints == false || opposingNormals == false )
					{
						continue;
					}

					typename Edges::value_type edgeA = {
						bodyId, polygonShapeA.shapeId, pointA1, pointA2, polygonA.normals[edgeIndexA]
					};
					typename Edges::value_type edgeB = {
						bodyId, polygonShapeB.shapeId, pointB1, pointB2, polygonB.normals[edgeIndexB]
					};
					edges.push_back( edgeA );
					edges.push_back( edgeB );
					enable( polygonShapeA.shapeId );
					enable( polygonShapeB.shapeId );
				}
			}
		}
	}
}

template <typename Edges>
bool Suppress( const Edges& edges, b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Vec2 point, b2Vec2 normal )
{
	if ( ! b2Shape_IsValid( shapeIdA ) || ! b2Shape_IsValid( shapeIdB ) )
	{
		return false;
	}

	b2ShapeType shapeTypeA = b2Shape_GetType( shapeIdA );
	b2ShapeType shapeTypeB = b2Shape_GetType( shapeIdB );
	bool polygonIsA = shapeTypeA == b2_polygonShape && shapeTypeB == b2_circleShape;
	bool polygonIsB = shapeTypeA == b2_circleShape && shapeTypeB == b2_polygonShape;
	if ( polygonIsA == false && polygonIsB == false )
	{
		return false;
	}

	b2ShapeId polygonShapeId = polygonIsA ? shapeIdA : shapeIdB;
	b2Vec2 outwardNormal = polygonIsA ? normal : b2Vec2{ -normal.x, -normal.y };
	b2BodyId polygonBodyId = b2Shape_GetBody( polygonShapeId );
	b2Vec2 localPoint = b2Body_GetLocalPoint( polygonBodyId, point );
	b2Vec2 localNormal = b2Body_GetLocalVector( polygonBodyId, outwardNormal );

	const float normalAlignmentLimit = 0.995f;
	const float edgeTolerance = 0.5f * 0.005f * b2GetLengthUnitsPerMeter();
	for ( size_t i = 0; i < edges.size(); ++i )
	{
		const typename Edges::value_type& edge = edges[i];
		if ( B2_ID_EQUALS( edge.shapeId, polygonShapeId ) == false ||
			 b2Dot( edge.normal, localNormal ) < normalAlignmentLimit )
		{
			continue;
		}

		b2Vec2 edgeVector = b2Sub( edge.point2, edge.point1 );
		float edgeLength = b2Length( edgeVector );
		if ( edgeLength <= edgeTolerance )
		{
			continue;
		}

		float projection = b2Dot( b2Sub( localPoint, edge.point1 ), edgeVector );
		float projectionTolerance = edgeTolerance * edgeLength;
		if ( projection >= -projectionTolerance &&
			 projection <= edgeLength * edgeLength + projectionTolerance )
		{
			return true;
		}
	}

	return false;
}

} // namespace PhysicsCompoundEdges
} // namespace Rtt
#endif
