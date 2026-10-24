// Internal fixed-step scheduling. Definitions live in Rtt_PhysicsWorld.cpp.
#ifndef _Rtt_FixedStepScheduler_H__
#define _Rtt_FixedStepScheduler_H__
struct lua_State;
namespace Rtt
{
class PhysicsWorld;
class FixedStepScheduler
{
public:
    // Private experimental client. Callbacks never call Lua; owner unregisters on GC.
    struct Client
    {
        void *owner;
        const char *(*prepare)(void *, PhysicsWorld&);
        const char *(*advance)(void *, PhysicsWorld&);
        void (*invalidate)(void *);
    };
    static const char *Attach(PhysicsWorld& physics, const Client& client, float dt);
    static void Detach(PhysicsWorld& physics, void *owner);
    static void Invalidate(PhysicsWorld& physics);
    static int Configure( lua_State *L );
    static int SetSpeed( lua_State *L );
    static int SetListener( lua_State *L );
    static int GetState( lua_State *L );
    static bool Step( PhysicsWorld& physics, float frameInterval );
    static void Reset( PhysicsWorld& physics );
    static void Interrupt( PhysicsWorld& physics );
    static void Forget( PhysicsWorld& physics );
private:
    static void SyncDisplay( PhysicsWorld& physics );
    static bool DispatchEvents( PhysicsWorld& physics, unsigned long long generation );
};
}
#endif
