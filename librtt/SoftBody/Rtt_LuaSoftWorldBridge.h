// Internal ownership/step bridge. No solver storage or public plugin API.
#ifndef _Rtt_LuaSoftWorldBridge_H__
#define _Rtt_LuaSoftWorldBridge_H__
#include "box2d/id.h"
struct lua_State;
namespace Rtt
{
namespace SoftSimulationLua
{
struct IndependentHost
{
    void *owner;
    b2WorldId id;
    float scale, timeStep;
    int subSteps;
    const char *( *validate )( void * );
    void ( *detach )( void *, void * );
};
struct IndependentClient
{
    void *owner;
    const char *( *prepare )( void * );
    const char *( *advance )( void *, bool &entered );
    void ( *invalidate )( void * );
    void ( *retain )( void * );
    void ( *release )( void * );
    void ( *syncViews )( lua_State *, void *, int systemIndex, double alpha );
    void ( *clearViews )( lua_State *, void *, int systemIndex );
    void ( *setInterpolation )( void *, bool enabled );
};
// L[1] is the owning rigid world, L[2] the validated options table. Pushes one
// soft userdata, retaining L[1]. Implemented with the existing Lua adapter in
// Rtt_LuaLibPhysics.cpp; it must have only one instance registry per Runtime path.
int NewIndependent( lua_State *L, const IndependentHost &host, IndependentClient &client );
} // namespace SoftSimulationLua
} // namespace Rtt
#endif
