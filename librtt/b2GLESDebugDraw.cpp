/*
* Copyright (c) 2006-2007 Erin Catto http://www.gphysics.com
*
* iPhone port by Simon Oliver - http://www.simonoliver.com - http://www.handcircus.com
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "Core/Rtt_Build.h"

#include "b2GLESDebugDraw.h"

// #include "Box2D/Box2D.h"
#include "box2d/box2d.h"
#include "Core/Rtt_Geometry.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_DisplayObject.h"
#include "Display/Rtt_GroupObject.h"
#include "Display/Rtt_Shader.h"
#include "Display/Rtt_ShaderFactory.h"
#include "Renderer/Rtt_Geometry_Renderer.h"
#include "Renderer/Rtt_Renderer.h"
#include "Rtt_LuaLibPhysics.h"
#include "Rtt_ParticleSystemObject.h"
#include "Rtt_PhysicsJoint.h"
#include "Rtt_PhysicsWorld.h"

// ----------------------------------------------------------------------------

# define ENABLE_DEBUG_PRINT	( 0 )
#
# if ENABLE_DEBUG_PRINT
#
#	define DEBUG_PRINT( ... )	Rtt_Log( __VA_ARGS__ )
#
# else // Not ENABLE_DEBUG_PRINT
#
#	define DEBUG_PRINT( ... )
#
# endif // ENABLE_DEBUG_PRINT

// ----------------------------------------------------------------------------

namespace Rtt
{

static float invColorBase = 1.0f / 255.0f;
static inline Box2dDebugColor MakeRGBA( b2HexColor c )
{
	return { ((c >> 16) & 0xFF) * invColorBase, ((c >> 8) & 0xFF) * invColorBase, uint8_t(c & 0xFF) * invColorBase };
}

// The physics world lives in the work-group's local space (meters), so the
// visual position of a world-space point is: p' = origin + p * scale.
static inline b2Vec2
ApplyParentTransform( const b2GLESDebugDraw *debugDraw, b2Vec2 p )
{
	b2Vec2 scale = debugDraw->GetParentScale();
	b2Vec2 origin = debugDraw->GetParentOrigin();
	return { origin.x + p.x * scale.x, origin.y + p.y * scale.y };
}

// Precomputed unit-circle directions (16 segments, i.e. a 2*PI/16 step).
// Circles and capsule arcs index into this table instead of calling
// cosf/sinf per vertex.
static inline const b2Vec2 *
UnitCircle()
{
	static b2Vec2 s[ 16 ];
	static bool sInitialized = false;

	if ( ! sInitialized )
	{
		for( int i = 0; i < 16; ++i )
		{
			float theta = ( 2.0f * B2_PI * (float)i ) / 16.0f;
			s[ i ] = { cosf( theta ), sinf( theta ) };
		}
		sInitialized = true;
	}

	return s;
}

static inline void
SetVertex( Geometry::Vertex &v, float x, float y, const Box2dDebugColor &color, float alpha )
{
	v.Zero();
	v.SetPos( x, y );
	v.rs = (U8)( color.r * 255.0f );
	v.gs = (U8)( color.g * 255.0f );
	v.bs = (U8)( color.b * 255.0f );
	v.as = (U8)( alpha * 255.0f );
}

void DrawPolygonFcn(const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawPolygon( vertices, vertexCount, MakeRGBA(color) );
}

void DrawSolidPolygonFcn(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color,
						 void* context)
{
	b2GLESDebugDraw *debugDraw = static_cast<b2GLESDebugDraw*>(context);
	b2Vec2 scale = debugDraw->GetParentScale();

	// Transform the body-local vertices to world space, then apply the shared
	// display transform.
	b2Vec2 scaled[ B2_MAX_POLYGON_VERTICES ];
	for( int i = 0; i < vertexCount; ++i )
	{
		b2Vec2 p = b2TransformPoint( transform, vertices[ i ] );
		scaled[ i ] = ApplyParentTransform( debugDraw, p );
	}

	float s = 0.5f * ( scale.x + scale.y );
	debugDraw->DrawSolidPolygon( b2Transform_identity, scaled, vertexCount, radius * s, MakeRGBA(color) );
}

void DrawCircleFcn(b2Vec2 center, float radius, b2HexColor color, void* context)
{
	b2GLESDebugDraw *debugDraw = static_cast<b2GLESDebugDraw*>(context);
	b2Vec2 scale = debugDraw->GetParentScale();

	b2Vec2 c = ApplyParentTransform( debugDraw, center );
	float s = 0.5f * ( scale.x + scale.y );

	debugDraw->DrawCircle( c, radius * s, MakeRGBA(color) );
}

void DrawSolidCircleFcn(b2Transform transform, float radius, b2HexColor color, void* context)
{
	b2GLESDebugDraw *debugDraw = static_cast<b2GLESDebugDraw*>(context);
	b2Vec2 scale = debugDraw->GetParentScale();

	// transform.p is the circle center in world space; apply the shared
	// display transform. A circle under non-uniform scale is visually an
	// ellipse; approximate it with the average scale.
	b2Vec2 center = ApplyParentTransform( debugDraw, transform.p );

	float s = 0.5f * ( scale.x + scale.y );
	debugDraw->DrawSolidCircle( transform, center, radius * s, MakeRGBA(color) );
}

void DrawSolidCapsuleFcn(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context)
{
	b2GLESDebugDraw *debugDraw = static_cast<b2GLESDebugDraw*>(context);
	b2Vec2 scale = debugDraw->GetParentScale();

	// p1/p2 are in world space; apply the shared display transform.
	b2Vec2 v1 = ApplyParentTransform( debugDraw, p1 );
	b2Vec2 v2 = ApplyParentTransform( debugDraw, p2 );

	float s = 0.5f * ( scale.x + scale.y );
	debugDraw->DrawSolidCapsule( v1, v2, radius * s, MakeRGBA(color) );
}

void DrawSegmentFcn(b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context)
{
	b2GLESDebugDraw *debugDraw = static_cast<b2GLESDebugDraw*>(context);

	// The endpoints are in world space; apply the shared display transform.
	b2Vec2 v1 = ApplyParentTransform( debugDraw, p1 );
	b2Vec2 v2 = ApplyParentTransform( debugDraw, p2 );

	debugDraw->DrawSegment( v1, v2, MakeRGBA(color) );
}

void DrawTransformFcn(b2Transform transform, void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawTransform(transform);
}

void DrawPointFcn(b2Vec2 p, float size, b2HexColor color, void* context)
{
	b2GLESDebugDraw *debugDraw = static_cast<b2GLESDebugDraw*>(context);
	b2Vec2 scale = debugDraw->GetParentScale();

	// Apply the shared display transform to the point's position and size.
	b2Vec2 v = ApplyParentTransform( debugDraw, p );
	float s = 0.5f * ( scale.x + scale.y );

	debugDraw->DrawPoint( v, size * s, MakeRGBA(color) );
}

void DrawStringFcn(b2Vec2 p, const char* s, b2HexColor color, void* context)
{
	// static_cast<b2GLESDebugDraw*>(context)->DrawString(p, s);
}

void GetBodyTransformFcn( b2Transform* transform, void* bodyUserData, void* context )
{
	DisplayObject *o = static_cast<DisplayObject*>(bodyUserData);

	b2GLESDebugDraw *debugDraw = static_cast<b2GLESDebugDraw*>(context);
	float metersPerPixel = debugDraw->GetMetersPerPixel();

	// b2World_Draw calls this once per shape of a body; the cached parent
	// transform is still valid for repeated calls on the same body.
	if ( bodyUserData == debugDraw->GetLastBodyUserData() )
	{
		return;
	}
	debugDraw->SetLastBodyUserData( bodyUserData );

	// Bodies without user data (or the ground body) draw in physical
	// coordinates.
	debugDraw->SetParentScale( { 1.0f, 1.0f } );
	debugDraw->SetParentOrigin( { 0.0f, 0.0f } );

	if ( o && LuaLibPhysics::GetGroundBodyUserdata() != o )
	{
		// All bodies live in one display group chain (subgroups stay at the
		// origin, unscaled), so the parent chain's transform is the single
		// display transform shared by every body. The physics world lives in
		// the work-group's local space, so cache the chain's translation and
		// scale and apply them to every draw callback: p' = origin + p * scale.
		// The transform passed in is left untouched.
		//
		// The chain's matrix is composed explicitly instead of transforming
		// unit axes through LocalToContent: subtracting large translations
		// (e.g. ~8000px) from ~1px axis differences in float loses too much
		// precision.
		DisplayObject *chain[ 8 ];
		int chainCount = 0;
		for ( GroupObject *node = o->GetParent();
			  node && chainCount < (int)B2_ARRAY_COUNT( chain );
			  node = node->GetParent() )
		{
			chain[ chainCount++ ] = node;
		}

		// LocalToContent applies the node's own matrix first, then each
		// ancestor's, i.e. M = M_ancestor * ... * M_parent.
		Matrix m;
		m.SetIdentity();
		for ( int i = chainCount - 1; i >= 0; --i )
		{
			m.Concat( chain[ i ]->GetMatrix() );
		}

		if ( ! m.IsIdentity() )
		{
			Real a = m.Row0()[ 0 ];
			Real b = m.Row0()[ 1 ];
			Real c = m.Row1()[ 0 ];
			Real d = m.Row1()[ 1 ];

			float scaleX = sqrtf( Rtt_RealToFloat( a*a + c*c ) );
			float scaleY = sqrtf( Rtt_RealToFloat( b*b + d*d ) );

			// DEBUG_PRINT( "scaleX=%f, scaleY=%f, origin.x=%f, origin.y=%f", scaleX, scaleY, m.Tx(), m.Ty() );
			debugDraw->SetParentScale( { scaleX, scaleY } );
			debugDraw->SetParentOrigin( { Rtt_RealToFloat( m.Tx() ) * metersPerPixel,
										  Rtt_RealToFloat( m.Ty() ) * metersPerPixel } );

			// TODO: Disabled for now. The physical rotation is kept; the
			// parent-chain rotation is not applied to the shapes.
			// transform->q = b2MakeRot( b2Atan2( Rtt_RealToFloat( c ), Rtt_RealToFloat( a ) ) );
		}
	}
}

// ----------------------------------------------------------------------------

b2GLESDebugDraw::b2GLESDebugDraw( Display &display )
:	fRenderer( NULL ),
	fPixelsPerMeter( Rtt_REAL_1 ),
	fMetersPerPixel( Rtt_REAL_1 ),
	fFillData(),
	fLineData(),
	fParentScale({ 1.0f, 1.0f }),
	fParentOrigin({ 0.0f, 0.0f }),
	fLastBodyUserData( NULL ),
	fDebugDraw({})
{
	// Init the two accumulation buffers: one for filled triangles, one for
	// line segments. Both are flushed once per frame.
	{
		/**/ //BAD: THIS MEMORY IS LEAKED!!!!!!!!
		fFillData.fGeometry = Rtt_NEW( display.GetAllocator(),
										Rtt::Geometry( display.GetAllocator(),
														Geometry::kTriangles,
														256, // Vertex count.
														0, // Index count.
														false ) ); // Store on GPU.

		fLineData.fGeometry = Rtt_NEW( display.GetAllocator(),
										Rtt::Geometry( display.GetAllocator(),
														Geometry::kLines,
														512, // Vertex count.
														0, // Index count.
														false ) ); // Store on GPU.

		ShaderFactory &factory = display.GetShaderFactory();
		fShader = factory.FindOrLoad( ShaderTypes::kCategoryFilter, "color" );
		fShader->Prepare( fFillData, 0, 0, ShaderResource::kDefault );
		fShader->Prepare( fLineData, 0, 0, ShaderResource::kDefault );

		RenderData *datas[ 2 ] = { &fFillData, &fLineData };
		for( int i = 0; i < 2; ++i )
		{
			RenderData &data = *datas[ i ];

			data.fFillTexture0 = NULL;
			data.fFillTexture1 = NULL;
			data.fMaskTexture = NULL;
			data.fMaskUniform = NULL;

			data.fUserUniform0 = NULL;
			data.fUserUniform1 = NULL;
			data.fUserUniform2 = NULL;
			data.fUserUniform3 = NULL;
		}

		b2AABB bounds = {{-FLT_MAX, -FLT_MAX}, {FLT_MAX, FLT_MAX}};
		fDebugDraw = {};
		fDebugDraw.DrawPolygonFcn = DrawPolygonFcn;
		fDebugDraw.DrawSolidPolygonFcn = DrawSolidPolygonFcn;
		fDebugDraw.DrawCircleFcn = DrawCircleFcn;
		fDebugDraw.DrawSolidCircleFcn = DrawSolidCircleFcn;
		fDebugDraw.DrawSolidCapsuleFcn = DrawSolidCapsuleFcn;
		fDebugDraw.DrawLineFcn = DrawSegmentFcn;
		fDebugDraw.DrawTransformFcn = DrawTransformFcn;
		fDebugDraw.DrawPointFcn = DrawPointFcn;
		fDebugDraw.DrawStringFcn = DrawStringFcn;
		fDebugDraw.GetBodyTransformFcn = GetBodyTransformFcn;
		fDebugDraw.drawingBounds = bounds;

		fDebugDraw.forceScale = 1.0f;
		fDebugDraw.jointScale = 1.0f;
		fDebugDraw.drawShapes = true;
		fDebugDraw.drawJoints = true;
		fDebugDraw.drawJointExtras = false;
		fDebugDraw.drawBounds = false;
		fDebugDraw.drawMass = true;
		fDebugDraw.drawBodyNames = false;
		fDebugDraw.drawGraphColors = false;
		fDebugDraw.drawContactNormals = false;

		fDebugDraw.context = this;
	}
}

b2GLESDebugDraw::~b2GLESDebugDraw()
{
	Rtt_DELETE( fFillData.fGeometry );
	fFillData.fGeometry = NULL;

	Rtt_DELETE( fLineData.fGeometry );
	fLineData.fGeometry = NULL;
}

static b2Transform
GetTransform( const b2BodyId b, Real metersPerPixel )
{
	b2Transform xf;

	DisplayObject *o = (DisplayObject *)b2Body_GetUserData(b);
	if ( ! o || LuaLibPhysics::GetGroundBodyUserdata() == o )
	{
		xf = b2Body_GetTransform(b);
	}
	else
	{
		Vertex2 v = { 0.0f, 0.0f };
		if( o->ShouldOffsetWithAnchor() )
		{
			Vertex2 offset = o->GetAnchorOffset();
			v.x -= offset.x;
			v.y -= offset.y;
		}
		o->LocalToContent( v );

		b2Vec2 p = { v.x, v.y };
		p *= metersPerPixel;

		// xf.Set( p, b.GetTransform().q.GetAngle() );
		xf.p = p;
		xf.q = b2MakeRot( b2Rot_GetAngle(b2Body_GetTransform(b).q) );
	}

	return xf;
}

float b2GLESDebugDraw::GetMetersPerPixel()
{
	return fMetersPerPixel;
}

float b2GLESDebugDraw::GetPixelsPerMeter()
{
	return fPixelsPerMeter;
}

void b2GLESDebugDraw::Begin( const PhysicsWorld& physics, Renderer &renderer )
{
	fRenderer = & renderer;
	fPixelsPerMeter = Rtt_RealToFloat( physics.GetPixelsPerMeter() );
	fMetersPerPixel = Rtt_RealToFloat( physics.GetMetersPerPixel() );
	fParentScale = { 1.0f, 1.0f };
	fParentOrigin = { 0.0f, 0.0f };
	fLastBodyUserData = NULL;

	// Reset the accumulation buffers. Their storage is reused across frames.
	fFillData.fGeometry->SetVerticesUsed( 0 );
	fLineData.fGeometry->SetVerticesUsed( 0 );
}

void b2GLESDebugDraw::End()
{
	// Flush the accumulation buffers: two draw calls for the whole frame.
	if ( fFillData.fGeometry->GetVerticesUsed() > 0 )
	{
		fFillData.fGeometry->SetPrimitiveType( Geometry::kTriangles );
		fRenderer->Insert( &fFillData );
	}

	if ( fLineData.fGeometry->GetVerticesUsed() > 0 )
	{
		fLineData.fGeometry->SetPrimitiveType( Geometry::kLines );
		fRenderer->Insert( &fLineData );
	}

	fRenderer = NULL;
	fPixelsPerMeter = Rtt_REAL_1;
	fMetersPerPixel = Rtt_REAL_1;
}

// NOTE:
// This is a replacement for b2World::DrawDebugData() b/c we need to account for
// the display object's object-to-world-space transform.
void b2GLESDebugDraw::DrawDebugData( const PhysicsWorld& physics, Renderer &renderer )
{
	b2WorldId worldId = physics.GetWorldId();
	if ( ! b2World_IsValid( worldId ) )
	{
		return;
	}

	Begin( physics, renderer );

	b2World_Draw( worldId, &fDebugDraw );

	for (b2ParticleSystem *p = physics.GetWorld()->GetParticleSystemList(); p; p = p->GetNext())
	{
		DrawParticleSystem( *p );
	}

	// uint32 flags = GetFlags();

	// // Flags for items we'll draw, overriding the drawing of b2World::DrawDebugData()
	// const uint32 kOverrideFlags =
	// 	b2Draw::e_shapeBit | b2Draw::e_centerOfMassBit | b2Draw::e_particleBit | b2Draw::e_jointBit;

	// // Our version of drawing Shapes
	// if( flags & kOverrideFlags )
	// {
	// 	// Draw all bodies.
	// 	for (b2Body* body = world->GetBodyList(); body; body = body->GetNext())
	// 	{
	// 		DisplayObject *o = (DisplayObject *)body->GetUserData();
	// 		if( !o || body->GetUserData() == LuaLibPhysics::GetGroundBodyUserdata() )
	// 		{
	// 			continue;
	// 		}

	// 		b2Transform xf = GetTransform( * body, fMetersPerPixel );
	// 		if( flags & b2Draw::e_shapeBit )
	// 		{
	// 			for (b2Fixture* f = body->GetFixtureList(); f; f = f->GetNext())
	// 			{
	// 				float r = 0.95f, g = 0.75f, b = 0.5f;

	// 				if (body->IsActive() == false)
	// 				{
	// 					r = 0.5f; g = 0.5f; b = 0.3f;
	// 				}
	// 				else if (body->GetType() == b2_staticBody)
	// 				{
	// 					r = 0.5f; g = 0.9f; b = 0.5f;
	// 				}
	// 				else if (body->GetType() == b2_kinematicBody)
	// 				{
	// 					r = 0.5f; g = 0.5f; b = 0.9f;
	// 				}
	// 				else if (body->IsAwake() == false)
	// 				{
	// 					r = 0.55f; g = 0.55f; b = 0.55f;
	// 				}

	// 				// Draw.
	// 				b2HexColor c( r, g, b );
	// 				DrawShape( f, xf, c );
	// 			}
	// 		}

	// 		if( flags & b2Draw::e_centerOfMassBit )
	// 		{
	// 			DrawTransform( xf );
	// 		}
	// 	}

	// 	// Draw all particle systems.
	// 	if( flags & b2Draw::e_particleBit )
	// 	{
	// 		for (b2ParticleSystem *p = world->GetParticleSystemList(); p; p = p->GetNext())
	// 		{
	// 			DrawParticleSystem( *p );
	// 		}
	// 	}

	// 	if (flags & b2Draw::e_jointBit)
	// 	{
	// 		for (b2Joint* j = world->GetJointList(); j; j = j->GetNext())
	// 		{
	// 			DrawJoint(j);
	// 		}
	// 	}
	// }

	// // Temporarily modify flags
	// // Clear out shapeBit, since we want to override drawing of shapes
	// uint32 tmpFlags = flags & ~(kOverrideFlags);
	// SetFlags( tmpFlags );
	// {
	// 	// Draw everything else
	// 	world->DrawDebugData();
	// }
	// // Restore flags
	// SetFlags( flags );

	End();
}

void b2GLESDebugDraw::DrawShape( b2ShapeId fixture, const b2Transform& xf, Box2dDebugColor color)
{
	// switch (fixture->GetType())
	// {
	// 	case b2Shape::e_circle:
	// 	{
	// 		b2CircleShape* circle = (b2CircleShape*)fixture->GetShape();

	// 		b2Vec2 center = b2Mul(xf, circle->m_p);
	// 		float radius = circle->m_radius;
	// 		b2Vec2 axis = xf.q.GetXAxis();

	// 		DrawSolidCircle(center, radius, axis, color);
	// 	}
	// 	break;

	// 	case b2Shape::e_polygon:
	// 	{
	// 		b2PolygonShape* poly = (b2PolygonShape*)fixture->GetShape();
	// 		int vertexCount = poly->m_count;
	// 		b2Assert(vertexCount <= b2_maxPolygonVertices);
	// 		b2Vec2 vertices[b2_maxPolygonVertices];

	// 		for (int i = 0; i < vertexCount; ++i)
	// 		{
	// 			vertices[i] = b2Mul(xf, poly->m_vertices[i]);
	// 		}

	// 		DrawSolidPolygon(vertices, vertexCount, color);
	// 	}
	// 	break;

	// 	case b2Shape::e_edge:
	// 	{
	// 		b2EdgeShape* edge = (b2EdgeShape*)fixture->GetShape();
	// 		b2Vec2 v1 = b2Mul(xf, edge->m_vertex1);
	// 		b2Vec2 v2 = b2Mul(xf, edge->m_vertex2);
	// 		DrawSegment(v1, v2, color);
	// 	}
	// 	break;

	// 	case b2Shape::e_chain:
	// 	{
	// 		b2ChainShape* chain = (b2ChainShape*)fixture->GetShape();
	// 		int count = chain->m_count;
	// 		const b2Vec2* vertices = chain->m_vertices;

	// 		b2Vec2 v1 = b2Mul(xf, vertices[0]);
	// 		for (int i = 1; i < count; ++i)
	// 		{
	// 			b2Vec2 v2 = b2Mul(xf, vertices[i]);
	// 			DrawSegment(v1, v2, color);
	// 			DrawCircle(v1, 3.0f * fMetersPerPixel, color);
	// 			v1 = v2;
	// 		}

	// 		// Draw the "end cap" circle.
	// 		DrawCircle(v1, 3.0f * fMetersPerPixel, color);
	// 	}
	// 	break;

	// 	default:
	// 		Rtt_ASSERT_NOT_REACHED();
	// 		break;
	// }
}

void b2GLESDebugDraw::DrawJoint(b2JointId joint)
{
	// b2Body* bodyA = joint->GetBodyA();
	// b2Body* bodyB = joint->GetBodyB();
	// b2Transform xf1 = GetTransform( * bodyA, fMetersPerPixel );
	// b2Transform xf2 = GetTransform( * bodyB, fMetersPerPixel );
	// b2Vec2 x1 = xf1.p;
	// b2Vec2 x2 = xf2.p;
	// b2Vec2 p1 = PhysicsJoint::HasLocalAnchor( * joint ) ? x1 + PhysicsJoint::GetLocalAnchorA( * joint ) : joint->GetAnchorA();
	// b2Vec2 p2 = PhysicsJoint::HasLocalAnchor( * joint ) ? x2 + PhysicsJoint::GetLocalAnchorB( * joint ) : joint->GetAnchorB();

	// b2HexColor color(0.5f, 0.8f, 0.8f);

	// switch (joint->GetType())
	// {
	// 	case e_distanceJoint:
	// 		DrawSegment(p1, p2, color);
	// 		break;

	// 	case e_pulleyJoint:
	// 	{
	// 		b2PulleyJoint* pulley = (b2PulleyJoint*)joint;
	// 		b2Vec2 s1 = pulley->GetGroundAnchorA();
	// 		b2Vec2 s2 = pulley->GetGroundAnchorB();
	// 		DrawSegment(s1, p1, color);
	// 		DrawSegment(s2, p2, color);
	// 		DrawSegment(s1, s2, color);
	// 	}
	// 	break;

	// 	case e_mouseJoint:
	// 	{
	// 		// Drawing code adapted from Box2D 2.0.1 testbed, updated for 2.3.x
	// 		DrawSegment( p1, p2, color );
			
	// 		float size = fMetersPerPixel * 3;
	// 		DrawPoint( p1, size, b2HexColor(0,1,0) );
	// 		DrawPoint( p2, size, b2HexColor(0,1,0) );
	// 	}
	// 	break;

	// 	default:
	// 		DrawSegment(x1, p1, color);
	// 		DrawSegment(p1, p2, color);
	// 		DrawSegment(x2, p2, color);
	// 		break;
	// }
}

void b2GLESDebugDraw::DrawParticleSystem( const b2ParticleSystem& system )
{
	int particleCount = system.GetParticleCount();
	if ( particleCount )
	{
		// This is safe to do because there's at least ONE particle,
		// and all particles have the same userdata.
		const ParticleSystemObject *pso = static_cast< const ParticleSystemObject * >( system.GetUserDataBuffer()[ 0 ] );
		if ( pso )
		{
			// Calculate offset. Convert to Box2D coords (meters)
			Vertex2 offsetInPixels = { 0.0f, 0.0f };
			pso->GetSrcToDstMatrix().Apply( offsetInPixels );
			b2Vec2 offsetInMeters = { offsetInPixels.x, offsetInPixels.y };
			offsetInMeters *= fMetersPerPixel;

			// Draw all particles.
			float radius = system.GetRadius();
			const b2Vec2* positionBuffer = system.GetPositionBuffer();
			const b2ParticleColor* colorBuffer = NULL;
			// TODO: We can't easily determine if m_colorBuffer.data is NULL
			// w/o accidentally forcing an allocation of m_colorBuffer.data
			/*
			if (system.m_colorBuffer.data)
			{
				colorBuffer = system.GetColorBuffer();
			}
			*/

			DrawParticlesOffset( positionBuffer, radius, colorBuffer, particleCount, &offsetInMeters );
		}
	}
}

void b2GLESDebugDraw::_AppendFillFan( const b2Vec2 *vertices, int vertexCount, Box2dDebugColor color )
{
	// Expand the triangle fan into an explicit triangle list. vertices[0] is
	// the fan's common vertex.
	const int triangleCount = vertexCount - 2;
	const int extra = 3 * triangleCount;

	// Match the legacy look: the fill is the outline color darkened by half,
	// at 0.5 alpha.
	Box2dDebugColor fillColor = { 0.5f * color.r, 0.5f * color.g, 0.5f * color.b };

	Geometry *geometry = fFillData.fGeometry;
	U32 used = geometry->GetVerticesUsed();
	if ( used + extra > geometry->GetVerticesAllocated() )
	{
		U32 newSize = Max<U32>( used + extra, 2 * geometry->GetVerticesAllocated() );
		geometry->Resize( newSize, 0, true );
	}

	Geometry::Vertex *out = geometry->GetVertexData() + used;
	for( int i = 1; i < vertexCount - 1; ++i )
	{
		Geometry::Vertex *tri = out + 3 * ( i - 1 );

		SetVertex( tri[ 0 ], vertices[ 0 ].x * fPixelsPerMeter, vertices[ 0 ].y * fPixelsPerMeter, fillColor, 0.5f );
		SetVertex( tri[ 1 ], vertices[ i ].x * fPixelsPerMeter, vertices[ i ].y * fPixelsPerMeter, fillColor, 0.5f );
		SetVertex( tri[ 2 ], vertices[ i + 1 ].x * fPixelsPerMeter, vertices[ i + 1 ].y * fPixelsPerMeter, fillColor, 0.5f );
	}

	geometry->SetVerticesUsed( used + extra );
}

void b2GLESDebugDraw::_AppendLineSegment( const b2Vec2 &p1, const b2Vec2 &p2, Box2dDebugColor color )
{
	Geometry *geometry = fLineData.fGeometry;
	U32 used = geometry->GetVerticesUsed();
	if ( used + 2 > geometry->GetVerticesAllocated() )
	{
		U32 newSize = Max<U32>( used + 2, 2 * geometry->GetVerticesAllocated() );
		geometry->Resize( newSize, 0, true );
	}

	Geometry::Vertex *out = geometry->GetVertexData() + used;
	SetVertex( out[ 0 ], p1.x * fPixelsPerMeter, p1.y * fPixelsPerMeter, color, 1.0f );
	SetVertex( out[ 1 ], p2.x * fPixelsPerMeter, p2.y * fPixelsPerMeter, color, 1.0f );

	geometry->SetVerticesUsed( used + 2 );
}

void b2GLESDebugDraw::_AppendLineLoop( const b2Vec2 *vertices, int vertexCount, Box2dDebugColor color )
{
	_AppendLineSegment( vertices[ vertexCount - 1 ], vertices[ 0 ], color );

	for( int i = 0; i < vertexCount - 1; ++i )
	{
		_AppendLineSegment( vertices[ i ], vertices[ i + 1 ], color );
	}
}

void b2GLESDebugDraw::_DrawPolygon( bool fill_body,
									b2Transform transform,
									const b2Vec2* vertices,
									int vertexCount,
									Box2dDebugColor color )
{
	// Transform the vertices to world space, then accumulate them.
	b2Vec2 transformed[ B2_MAX_POLYGON_VERTICES ];
	for( int i = 0; i < vertexCount; ++i )
	{
		transformed[ i ] = b2TransformPoint( transform, vertices[ i ] );
	}

	if( fill_body )
	{
		_AppendFillFan( transformed, vertexCount, color );
	}

	_AppendLineLoop( transformed, vertexCount, color );
}

void b2GLESDebugDraw::DrawPolygon(const b2Vec2* vertices, int vertexCount, Box2dDebugColor color)
{
	_DrawPolygon( false, b2Transform_identity, vertices, vertexCount, color );
}

void b2GLESDebugDraw::DrawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, Box2dDebugColor color)
{
	_DrawPolygon( true, transform, vertices, vertexCount, color );
}

void b2GLESDebugDraw::DrawParticles( const b2Vec2 *centers,
										float radius,
										const b2ParticleColor *colors,
										int count )
{
	DrawParticlesOffset( centers, radius, colors, count, NULL );
}

void b2GLESDebugDraw::DrawParticlesOffset( const b2Vec2 *centers,
										float radius,
										const b2ParticleColor *colors,
										int count,
										const b2Vec2 *offset )
{
	// static b2HexColor kColorDefault = b2HexColor( 1.0f, 1.0f, 1.0f );
	static b2HexColor kColorDefault = b2_colorWhite;

	// b2HexColor color = kColorDefault;
	Box2dDebugColor color = MakeRGBA( kColorDefault );

	for( int i = 0; i < count; i++ )
	{
		DEBUG_PRINT( "index: %d/%d centers: %f, %f radius: %f colors: %d %d %d %d\n",
						i, count,
						centers->x, centers->y,
						radius,
						colors->r, colors->g, colors->b, colors->a );

		if ( colors )
		{
			color = { colors[ i ].r * invColorBase, colors[ i ].g * invColorBase, colors[ i ].b * invColorBase };
		}

		DrawCircle( true, centers[ i ], radius, NULL, color, offset );
	}
}

void b2GLESDebugDraw::DrawCircle( bool fill_body,
									const b2Vec2& center,
									float radius,
									const b2Vec2 *optionalAxis,
									Box2dDebugColor color,
									const b2Vec2 *optionalOffset )
{
	b2Vec2 circleOrigin( center + ( optionalOffset ? *optionalOffset : b2Vec2_zero ) );

	const int kSegments = 16;
	const b2Vec2 *unit = UnitCircle();

	// Arc points in world space.
	b2Vec2 arc[ kSegments ];
	for( int i = 0; i < kSegments; ++i )
	{
		arc[ i ] = { circleOrigin.x + unit[ i ].x * radius,
					 circleOrigin.y + unit[ i ].y * radius };
	}

	// Draw the body of the circle. The fan needs the circle center as its
	// common vertex; the closing arc vertex wraps back to the first one.
	if( fill_body )
	{
		const int vertexCount = 2 + kSegments;

		b2Vec2 fan[ vertexCount ];
		fan[ 0 ] = circleOrigin;
		for( int i = 0; i <= kSegments; ++i )
		{
			fan[ 1 + i ] = arc[ i % kSegments ];
		}

		_AppendFillFan( fan, vertexCount, color );
	}

	// Draw the outline of the circle.
	_AppendLineLoop( arc, kSegments, color );

	if( optionalAxis )
	{
		// Draw the axis line
		DrawSegment( circleOrigin,
						( circleOrigin + ( radius * *optionalAxis ) ),
						color );
	}
}

void b2GLESDebugDraw::DrawCircle(const b2Vec2& center, float radius, Box2dDebugColor color)
{
	DrawCircle( false, center, radius, NULL, color, NULL );
}

void b2GLESDebugDraw::DrawSolidCircle( b2Transform transform,
										b2Vec2 center,
										float radius,
										Box2dDebugColor color )
{
	b2Vec2 axis = b2Rot_GetXAxis(transform.q);
	DrawCircle( true, center, radius, &axis, color, NULL );
}

void b2GLESDebugDraw::DrawSolidCapsule(b2Vec2 p1, b2Vec2 p2, float radius, Box2dDebugColor color)
{
	b2Vec2 axis = b2Normalize( p2 - p1 );
	float angle = b2Atan2( axis.y, axis.x );
	b2Rot rot = b2MakeRot( angle );

	const int kArcSegments = 8;
	const int vertexCount = 2 + 2 * kArcSegments; // 4 corners + (kArcSegments - 1) arc points per cap.

	// Build the outline as one continuous closed path around the capsule, so the
	// fill and the outline have no internal seam lines. The path starts on the
	// angle - 90° side of p1, sweeps around the outside of the p1 cap to the
	// angle + 90° side, follows that side to p2, sweeps around the outside of
	// the p2 cap back to the angle - 90° side, and closes on the start.
	// Unit-circle table indices: -90° = 12, +90° = 4 (16 segments, PI/8 steps).
	const b2Vec2 *unit = UnitCircle();

	b2Vec2 outline[ vertexCount ];
	int index = 0;

	outline[ index++ ] = p1 + radius * b2RotateVector( rot, unit[ 12 ] );

	for( int i = 1; i < kArcSegments; ++i )
	{
		outline[ index++ ] = p1 + radius * b2RotateVector( rot, unit[ ( 12 - i + 16 ) % 16 ] );
	}

	outline[ index++ ] = p1 + radius * b2RotateVector( rot, unit[ 4 ] );
	outline[ index++ ] = p2 + radius * b2RotateVector( rot, unit[ 4 ] );

	for( int i = 1; i < kArcSegments; ++i )
	{
		outline[ index++ ] = p2 + radius * b2RotateVector( rot, unit[ ( 4 - i + 16 ) % 16 ] );
	}

	outline[ index++ ] = p2 + radius * b2RotateVector( rot, unit[ 12 ] );

	// Draw the body: one triangle fan whose common vertex is the capsule
	// center. The closing vertex wraps back to the start of the outline.
	{
		const int fillVertexCount = 1 + vertexCount + 1;

		b2Vec2 fan[ fillVertexCount ];

		b2Vec2 center = p1 + p2;
		center *= 0.5f;
		fan[ 0 ] = center;

		for( int i = 0; i < vertexCount; ++i )
		{
			fan[ 1 + i ] = outline[ i ];
		}
		fan[ 1 + vertexCount ] = outline[ 0 ];

		_AppendFillFan( fan, fillVertexCount, color );
	}

	// Draw the outline as one continuous line.
	_AppendLineLoop( outline, vertexCount, color );
}

void b2GLESDebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, Box2dDebugColor color)
{
	_AppendLineSegment( p1, p2, color );
}

void b2GLESDebugDraw::DrawTransform(const b2Transform& xf)
{
	b2Vec2 scale = GetParentScale();

	// Apply the shared display transform to the cross position and size.
	b2Vec2 p1 = ApplyParentTransform( this, xf.p );
	b2Vec2 p2;
	// const float k_axisScale = 0.4f;
	const float k_axisScale = 24.0f * fMetersPerPixel * 0.5f * ( scale.x + scale.y );

	// p2 = p1 + k_axisScale * xf.q.GetXAxis();
	p2 = p1 + k_axisScale * b2Rot_GetXAxis( xf.q );
	DrawSegment( p1, p2, MakeRGBA( b2_colorRed ) );

	p2 = p1 + k_axisScale * b2Rot_GetYAxis( xf.q );
	DrawSegment( p1, p2, MakeRGBA( b2_colorGreen ) );
}

void b2GLESDebugDraw::DrawPoint(const b2Vec2& p, float size, Box2dDebugColor color)
{
	// We're aware that this isn't the most efficient way to draw a point.
	// We'll make this more efficient if necessary.
	DrawCircle( true, p, size * fMetersPerPixel, NULL, color, NULL );
}

void b2GLESDebugDraw::DrawString(int x, int y, const char *string, ...)
{
	/* Unsupported as yet. Could replace with bitmap font renderer at a later date */
}

void b2GLESDebugDraw::DrawAABB(b2AABB* aabb, Box2dDebugColor c)
{
	b2Vec2 vertices[ 4 ] =
	{
		{ aabb->lowerBound.x, aabb->lowerBound.y },
		{ aabb->upperBound.x, aabb->lowerBound.y },
		{ aabb->upperBound.x, aabb->upperBound.y },
		{ aabb->lowerBound.x, aabb->upperBound.y },
	};

	_AppendLineLoop( vertices, 4, c );
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
