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

#ifndef RENDER_H
#define RENDER_H

// #include "Box2D/Box2D.h"
#include "box2d/box2d.h"
#include "particle_system.h"

#include "Renderer/Rtt_RenderData.h"

// struct b2AABB;

// ----------------------------------------------------------------------------

namespace Rtt
{
// ----------------------------------------------------------------------------

struct Box2dDebugColor
{
	float r, g, b;
};

class Display;
class PhysicsWorld;
class Renderer;
class Shader;

// ----------------------------------------------------------------------------

// This class implements debug drawing callbacks that are invoked
// inside b2World::Step.
// class b2GLESDebugDraw : public b2Draw
class b2GLESDebugDraw
{
	public:
		b2GLESDebugDraw( Display &display );
		virtual ~b2GLESDebugDraw();

	protected:
		void Begin( const PhysicsWorld& physics, Renderer &renderer );
		void End();

	public:
		void DrawDebugData( const PhysicsWorld& physics, Renderer &renderer );
		float GetMetersPerPixel();
		float GetPixelsPerMeter();

		// The transform of the display group chain shared by all bodies. Set by
		// GetBodyTransformFcn, applied to every draw callback as
		// p' = fParentOrigin + p * fParentScale (the physics world lives in the
		// work-group's local space).
		void SetParentScale( b2Vec2 scale ) { fParentScale = scale; }
		b2Vec2 GetParentScale() const { return fParentScale; }

		void SetParentOrigin( b2Vec2 origin ) { fParentOrigin = origin; }
		b2Vec2 GetParentOrigin() const { return fParentOrigin; }

		// Lets GetBodyTransformFcn reuse the cached parent transform for
		// repeated calls on the same body (one call per shape).
		void SetLastBodyUserData( const void *userData ) { fLastBodyUserData = userData; }
		const void *GetLastBodyUserData() const { return fLastBodyUserData; }

	protected:
		void DrawShape( b2ShapeId fixture, const b2Transform& xf, Box2dDebugColor color);
		void DrawJoint( b2JointId joint );
		void DrawParticleSystem( const b2ParticleSystem& system );

	public:
		// b2Draw.

		virtual void DrawPolygon(const b2Vec2* vertices, int vertexCount, Box2dDebugColor color);

		virtual void DrawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, Box2dDebugColor color);

		virtual void DrawCircle(const b2Vec2& center, float radius, Box2dDebugColor color);

		virtual void DrawParticles(const b2Vec2 *centers, float radius, const b2ParticleColor *colors, int count);

		void DrawParticlesOffset( const b2Vec2 *centers, float radius, const b2ParticleColor *colors, int count, const b2Vec2 *offset );

		virtual void DrawSolidCircle(b2Transform transform, b2Vec2 center, float radius, Box2dDebugColor color);

		virtual void DrawSolidCapsule(b2Vec2 p1, b2Vec2 p2, float radius, Box2dDebugColor color);

		virtual void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, Box2dDebugColor color);

		virtual void DrawTransform(const b2Transform& xf);

		virtual void DrawPoint(const b2Vec2& p, float size, Box2dDebugColor color);

		virtual void DrawString(int x, int y, const char* string, ...);

		virtual void DrawAABB(b2AABB* aabb, Box2dDebugColor color);

	public:

		void DrawCircle( bool fill_body,
							const b2Vec2& center,
							float radius,
							const b2Vec2 *optionalAxis,
							Box2dDebugColor color,
							const b2Vec2 *optionalOffset );

	private:

		void _SetVerticesUsed( int vertexCount );

		void _DrawPolygon( bool fill_body,
							b2Transform transform,
							const b2Vec2* vertices,
							int vertexCount,
							Box2dDebugColor color );

		//! fRenderer and fScale are only valid between Begin() and End().
		Renderer *fRenderer;
		float fPixelsPerMeter;
		float fMetersPerPixel;

		RenderData fData;
		Shader *fShader;

		b2Vec2 fParentScale;
		b2Vec2 fParentOrigin;

		const void *fLastBodyUserData;

		b2DebugDraw fDebugDraw;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif
