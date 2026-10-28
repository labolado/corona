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
    // Private experimental client. Native stepping may dispatch existing collision Lua.
    // A whole-step client must retain its owner until the callback returns.
    struct Client
    {
        void *owner;
        const char *(*prepare)(void *, PhysicsWorld&);
        const char *(*advance)(void *, PhysicsWorld&);
        void (*invalidate)(void *);
        // Optional alternative to rigid Step + advance. entered is true even
        // when a failure follows native entry; such a step must not be retried.
        const char *(*wholeStep)(void *, PhysicsWorld&, bool& entered);
        void (*retain)(void *);
        void (*release)(void *);
    };
    static bool GetTiming(PhysicsWorld& physics, float& dt, int& subSteps);
    static const char *Attach(PhysicsWorld& physics, const Client& client, float dt, int subSteps);
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
