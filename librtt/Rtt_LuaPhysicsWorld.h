// Internal Lua entry point for independent, display-optional Box2D worlds.
#ifndef _Rtt_LuaPhysicsWorld_H__
#define _Rtt_LuaPhysicsWorld_H__
struct lua_State;
namespace Rtt
{
class LuaPhysicsWorld
{
  public:
    static int New( lua_State *L );
};
} // namespace Rtt
#endif
