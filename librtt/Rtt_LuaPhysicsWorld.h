// Internal Lua entry point for independent, display-optional Box2D worlds.
#ifndef _Rtt_LuaPhysicsWorld_H__
#define _Rtt_LuaPhysicsWorld_H__
#include "box2d/id.h"
struct lua_State;
namespace Rtt
{
class LuaPhysicsWorld
{
  public:
    static int New( lua_State *L );
    static b2BodyId GetBodyId( lua_State *L, int index, b2WorldId owner );
};
} // namespace Rtt
#endif
