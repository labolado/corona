//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_DisplayObjectExtensions_H__
#define _Rtt_DisplayObjectExtensions_H__

#include "Rtt_LuaProxyVTable.h"

// ----------------------------------------------------------------------------

#ifdef Rtt_PHYSICS
	// class b2Body;
	// class b2World;
	#include "box2d/box2d.h"
#endif

struct lua_State;

namespace Rtt
{

class DisplayObject;

// ----------------------------------------------------------------------------

class DisplayObjectExtensions : public LuaProxyVTable
{
	Rtt_CLASS_NO_COPIES( DisplayObjectExtensions )

	public:
		typedef DisplayObjectExtensions Self;

	public:
		DisplayObjectExtensions( DisplayObject& owner );
		~DisplayObjectExtensions();

	public:
		DisplayObject& GetOwner() { return fOwner; }
		const DisplayObject& GetOwner() const { return fOwner; }

#ifdef Rtt_PHYSICS
	public:
		static int setLinearVelocity( lua_State *L );
		static int getLinearVelocity( lua_State *L );
		static int applyForce( lua_State *L );
		static int applyTorque( lua_State *L );
		static int applyLinearImpulse( lua_State *L );
		static int applyAngularImpulse( lua_State *L );
		static int resetMassData( lua_State *L );
		static int getMassWorldCenter( lua_State *L );
		static int getMassLocalCenter( lua_State *L );
		static int getWorldVector( lua_State *L);
		static int getInertia( lua_State *L );
		static int getLinearVelocityFromWorldPoint(lua_State *L);
		static int getLinearVelocityFromLocalPoint(lua_State* L);
		static int setHitEventsEnabled( lua_State* L );
		static int setContactEventsEnabled( lua_State* L );
		static int setSensorEventsEnabled( lua_State* L );
		static int setPreSolveEventsEnabled( lua_State* L );
		static int setFilter( lua_State* L );

#endif // Rtt_PHYSICS

	public:
		// LuaProxyVTable
		virtual int ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction = false ) const;
		virtual bool SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const;

#ifdef Rtt_PHYSICS
	public:
		void SetBody( b2BodyId bodyId, b2WorldId worldId );
		b2BodyId GetBody() const { return fBodyId; }
#endif // Rtt_PHYSICS

	private:
		DisplayObject& fOwner;

	private:
#ifdef Rtt_PHYSICS
		b2BodyId fBodyId;
#endif // Rtt_PHYSICS
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_DisplayObjectExtensions_H__
