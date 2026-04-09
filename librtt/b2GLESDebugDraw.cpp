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

void DrawPolygonFcn(const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawPolygon( vertices, vertexCount, MakeRGBA(color) );
}

void DrawSolidPolygonFcn(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color,
						 void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawSolidPolygon( transform, vertices, vertexCount, radius, MakeRGBA(color) );
}

void DrawCircleFcn(b2Vec2 center, float radius, b2HexColor color, void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawCircle( center, radius, MakeRGBA(color) );
}

void DrawSolidCircleFcn(b2Transform transform, float radius, b2HexColor color, void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawSolidCircle( transform, transform.p, radius, MakeRGBA(color) );
}

void DrawCapsuleFcn(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context)
{
	// static_cast<b2GLESDebugDraw*>(context)->DrawCapsule(p1, p2, radius, color);
}

void DrawSolidCapsuleFcn(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context)
{
	// static_cast<b2GLESDebugDraw*>(context)->DrawSolidCapsule(p1, p2, radius, color);
}

void DrawSegmentFcn(b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawSegment( p1, p2, MakeRGBA(color) );
}

void DrawTransformFcn(b2Transform transform, void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawTransform(transform);
}

void DrawPointFcn(b2Vec2 p, float size, b2HexColor color, void* context)
{
	static_cast<b2GLESDebugDraw*>(context)->DrawPoint( p, size, MakeRGBA(color) );
}

void DrawStringFcn(b2Vec2 p, const char* s, b2HexColor color, void* context)
{
	// static_cast<b2GLESDebugDraw*>(context)->DrawString(p, s);
}

void GetBodyTransformFcn( b2Transform* transform, void* bodyUserData, void* context )
{
	DisplayObject *o = static_cast<DisplayObject*>(bodyUserData);

	float metersPerPixel = static_cast<b2GLESDebugDraw*>(context)->GetMetersPerPixel();
	if ( o && LuaLibPhysics::GetGroundBodyUserdata() != o )
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

		transform->p = p;
	}
}

// ----------------------------------------------------------------------------

b2GLESDebugDraw::b2GLESDebugDraw( Display &display )
:	fRenderer( NULL ),
	fPixelsPerMeter( Rtt_REAL_1 ),
	fMetersPerPixel( Rtt_REAL_1 ),
	fData(),
	fDebugDraw({})
{
	// Init fData.
	{
		/**/ //BAD: THIS MEMORY IS LEAKED!!!!!!!!
		fData.fGeometry = Rtt_NEW( display.GetAllocator(),
									Rtt::Geometry( display.GetAllocator(),
													Geometry::kTriangleFan,
													0, // Vertex count.
													0, // Index count.
													false ) ); // Store on GPU.

		ShaderFactory &factory = display.GetShaderFactory();
		fShader = factory.FindOrLoad( ShaderTypes::kCategoryFilter, "color" );
		fShader->Prepare( fData, 0, 0, ShaderResource::kDefault );

		fData.fFillTexture0 = NULL;
		fData.fFillTexture1 = NULL;
		fData.fMaskTexture = NULL;
		fData.fMaskUniform = NULL;

		fData.fUserUniform0 = NULL;
		fData.fUserUniform1 = NULL;
		fData.fUserUniform2 = NULL;
		fData.fUserUniform3 = NULL;

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
	Rtt_DELETE( fData.fGeometry );
	fData.fGeometry = NULL;
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
}

void b2GLESDebugDraw::End()
{
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

void b2GLESDebugDraw::_SetVerticesUsed( int vertexCount )
{
	if( vertexCount > (int)fData.fGeometry->GetVerticesAllocated() )
	{
		fData.fGeometry->Resize( vertexCount, false );
	}

	fData.fGeometry->SetVerticesUsed( vertexCount );
}

void b2GLESDebugDraw::_DrawPolygon( bool fill_body,
									b2Transform transform,
									const b2Vec2* vertices,
									int vertexCount,
									Box2dDebugColor color )
{
	_SetVerticesUsed( vertexCount );

	Rtt::Geometry::Vertex *output_vertices = fData.fGeometry->GetVertexData();

	// Copy the vertices from Box2D to our own rendering format.
	for( int i = 0;
			i < vertexCount;
			++i )
	{
		const b2Vec2 &input_vert = vertices[ i ];
		b2Vec2 p = b2TransformPoint( transform, input_vert );
		Rtt::Geometry::Vertex &output_vert = output_vertices[ i ];

		output_vert.Zero();
		output_vert.SetPos( ( p.x * fPixelsPerMeter ),
							( p.y * fPixelsPerMeter ) );
	}

	// We're iterating multiple times over the input and output arrays.
	// From a cache point of view, this is only ok with small arrays.
	// With large arrays, it's more efficient to set all the fields of
	// each entries in a single pass.

	// Draw the body of the polygon.
	if( fill_body )
	{
		// Set the color.
		Rtt::Geometry::Vertex::SetColor( vertexCount,
											output_vertices,
											0.5*color.r,
											0.5*color.g,
											0.5*color.b,
											0.5 );

		fData.fGeometry->SetPrimitiveType( Geometry::kTriangleFan );
		fRenderer->Insert( &fData );
	}

	// Set the color.
	Rtt::Geometry::Vertex::SetColor( vertexCount,
										output_vertices,
										color.r,
										color.g,
										color.b,
										1.f );

	// Draw the outline of the polygon.
	fData.fGeometry->SetPrimitiveType( Geometry::kLineLoop );
	fRenderer->Insert( &fData );
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

	const int vertexCount = 16;

	_SetVerticesUsed( vertexCount );

	Rtt::Geometry::Vertex *output_vertices = fData.fGeometry->GetVertexData();

	float theta = 0.0f;

	for( int i = 0;
			i < vertexCount;
			++i,
			theta += ( ( 2.0f * B2_PI ) / (float)vertexCount ) )
	{
		Rtt::Geometry::Vertex &output_vert = output_vertices[ i ];

		b2Vec2 pos = { cosf( theta ), sinf( theta ) };
		pos *= radius;
		pos += circleOrigin;
		pos *= fPixelsPerMeter;

		output_vert.Zero();
		output_vert.SetPos( pos.x, pos.y );
	}

	// Draw the body of the circle.
	if( fill_body )
	{
		// Set the color.
		Rtt::Geometry::Vertex::SetColor( vertexCount,
											output_vertices,
											0.5*color.r,
											0.5*color.g,
											0.5*color.b,
											0.5 );

		fData.fGeometry->SetPrimitiveType( Geometry::kTriangleFan );
		fRenderer->Insert( &fData );
	}

	Rtt::Geometry::Vertex::SetColor( vertexCount,
										output_vertices,
										color.r,
										color.g,
										color.b,
										1.f );

	// Draw the outline of the circle.
	fData.fGeometry->SetPrimitiveType( Geometry::kLineLoop );
	fRenderer->Insert( &fData );

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
	// DrawCircle( true, center, radius, NULL, color, NULL );
}

void b2GLESDebugDraw::DrawSolidCircle( b2Transform transform,
										b2Vec2 center,
										float radius,
										Box2dDebugColor color )
{
	b2Vec2 axis = b2Rot_GetXAxis(transform.q);
	DrawCircle( true, center, radius, &axis, color, NULL );
}

void b2GLESDebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, Box2dDebugColor color)
{
	const int vertexCount = 2;

	_SetVerticesUsed( vertexCount );

	Rtt::Geometry::Vertex *output_vertices = fData.fGeometry->GetVertexData();

	// Start point.
	{
		Rtt::Geometry::Vertex &output_vert = output_vertices[ 0 ];

		output_vert.Zero();

		output_vert.SetPos( ( p1.x * fPixelsPerMeter ),
							( p1.y * fPixelsPerMeter ) );
	}

	// End point.
	{
		Rtt::Geometry::Vertex &output_vert = output_vertices[ 1 ];

		output_vert.Zero();

		output_vert.SetPos( ( p2.x * fPixelsPerMeter ),
							( p2.y * fPixelsPerMeter ) );
	}

	// Set the color.
	Rtt::Geometry::Vertex::SetColor( vertexCount,
										output_vertices,
										color.r,
										color.g,
										color.b,
										1.0f );

	// Draw.
	fData.fGeometry->SetPrimitiveType( Geometry::kLines );
	fRenderer->Insert( &fData );
}

void b2GLESDebugDraw::DrawTransform(const b2Transform& xf)
{
	b2Vec2 p1 = xf.p, p2;
	// const float k_axisScale = 0.4f;
	const float k_axisScale = 24.0f * fMetersPerPixel;

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
	const int vertexCount = 4;

	_SetVerticesUsed( vertexCount );

	Rtt::Geometry::Vertex *output_vertices = fData.fGeometry->GetVertexData();

	// Upper left.
	{
		Rtt::Geometry::Vertex &output_vert = output_vertices[ 0 ];

		output_vert.Zero();

		output_vert.SetPos( ( aabb->lowerBound.x * fPixelsPerMeter ),
							( aabb->lowerBound.y * fPixelsPerMeter ) );
	}

	// Upper right.
	{
		Rtt::Geometry::Vertex &output_vert = output_vertices[ 1 ];

		output_vert.Zero();

		output_vert.SetPos( ( aabb->upperBound.x * fPixelsPerMeter ),
							( aabb->lowerBound.y * fPixelsPerMeter ) );
	}

	// Lower right.
	{
		Rtt::Geometry::Vertex &output_vert = output_vertices[ 2 ];

		output_vert.Zero();

		output_vert.SetPos( ( aabb->upperBound.x * fPixelsPerMeter ),
							( aabb->upperBound.y * fPixelsPerMeter ) );
	}

	// Lower left.
	{
		Rtt::Geometry::Vertex &output_vert = output_vertices[ 3 ];

		output_vert.Zero();

		output_vert.SetPos( ( aabb->lowerBound.x * fPixelsPerMeter ),
							( aabb->upperBound.y * fPixelsPerMeter ) );
	}

	// Set the color.
	Rtt::Geometry::Vertex::SetColor( vertexCount,
										output_vertices,
										c.r,
										c.g,
										c.b,
										1.0f );

	// Draw.
	fData.fGeometry->SetPrimitiveType( Geometry::kLineLoop );
	fRenderer->Insert( &fData );
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
