#pragma once
// Lua/lifetime adapter for the native simulation API. No solver storage or
// contact-cache access belongs here. Historical modes use their old adapter.
#include "../../external/box2d_v3/extern/softbody/soft_simulation.h"
#include "../../external/box2d_v3/extern/softbody/soft_box2d_simulation.h"
#include "Rtt_FixedStepScheduler.h"
#include <cmath>
#include <cstring>
#include <new>

namespace Rtt { namespace SoftSimulationLua {
static const char kWorld[]="physics.softSimulationWorld";
static const char kBody[]="physics.softSimulationBody";
static const char kJoint[]="physics.softSimulationJoint";
static const char kTouch[]="physics.softSimulationTouch";
static const char kAttachment[]="physics.softSimulationAttachment";
static int& LiveCount() { static int count=0;return count; }
struct Instance
{
    SoftBody::Simulation simulation;
    SoftBody::Box2DCoupling coupling;
    PhysicsWorld* physics=nullptr;
    b2WorldId physicsId=b2_nullWorldId;
    float scale=0;
    unsigned activeSteps=0;
    bool invalidated=false,removePending=false;
    Instance(const SoftBody::SimulationDef& def,const SoftBody::Box2DCouplingDef& box):simulation(def),coupling(simulation,box)
    {++LiveCount();physicsId=box.world;}
    ~Instance(){if(physics)FixedStepScheduler::Detach(*physics,this);--LiveCount();}
};
struct Handle { Instance* instance; };
struct Entity { Handle* owner;SoftBody::ObjectId id; };
static void CheckStep(lua_State* L,Instance* s)
{if(s && s->activeSteps)luaL_error(L,"attachment world is executing a native step");}
static Handle* WorldHandle(lua_State* L)
{return static_cast<Handle*>(luaL_checkudata(L,1,kWorld));}
static Instance* World(lua_State* L)
{
    auto* s=WorldHandle(L)->instance;CheckStep(L,s);
    if(!s)luaL_error(L,"soft world was removed");
    if(s->invalidated)luaL_error(L,"coupled soft world was invalidated by physics.stop");
    return s;
}
static Entity* Object(lua_State* L,const char* type,int index=1)
{
    auto* e=static_cast<Entity*>(luaL_checkudata(L,index,type));auto* s=e->owner->instance;CheckStep(L,s);
    if(!s)luaL_error(L,"soft world was removed");
    if(s->invalidated)luaL_error(L,"coupled soft world was invalidated by physics.stop");
    bool valid=type==kAttachment?s->coupling.isValid(e->id):s->simulation.isValid(e->id);
    if(!valid)luaL_error(L,"soft object was removed");
    return e;
}
static void Field(lua_State* L,int table,const char* name)
{lua_pushstring(L,name);lua_rawget(L,table);}
static double NumberArg(lua_State* L,int index,double low,double high)
{
    if(lua_type(L,index)!=LUA_TNUMBER)luaL_error(L,"expected a number");
    double value=lua_tonumber(L,index);
    if(!std::isfinite(value)||value<low||value>high)luaL_error(L,"number outside supported range");
    return value;
}
static double Value(lua_State* L,int table,const char* name,double fallback,double low,double high)
{
    Field(L,table,name);double value=lua_isnil(L,-1)?fallback:NumberArg(L,-1,low,high);lua_pop(L,1);return value;
}
static void Number(lua_State* L,const char* key,double value)
{lua_pushnumber(L,value);lua_setfield(L,-2,key);}
static void Bool(lua_State* L,const char* key,bool value)
{lua_pushboolean(L,value);lua_setfield(L,-2,key);}
static void Check(lua_State* L,bool ok,const char* error)
{if(!ok)luaL_error(L,"%s",error?error:"invalid soft operation");}
static SoftBody::Weights ReadWeights(lua_State* L,const char* name)
{
    SoftBody::Weights result{};Field(L,2,name);luaL_checktype(L,-1,LUA_TTABLE);
    if(lua_objlen(L,-1)!=16)luaL_error(L,"%s requires 16 weights",name);
    for(int i=0;i<16;++i){lua_rawgeti(L,-1,i+1);result[i]=NumberArg(L,-1,0,1);lua_pop(L,1);}
    lua_pop(L,1);return result;
}
static SoftBody::ObjectId BodyField(lua_State* L,const char* field)
{
    Field(L,2,field);auto* e=Object(L,kBody,-1);
    if(e->owner!=WorldHandle(L))luaL_error(L,"body belongs to another world");
    auto id=e->id;lua_pop(L,1);return id;
}
static int Push(lua_State* L,const char* type,SoftBody::ObjectId id)
{
    auto* s=World(L);
    Check(L,id.index>=0,type==kAttachment?s->coupling.lastError():s->simulation.lastError());
    auto* e=new(lua_newuserdata(L,sizeof(Entity))) Entity{};e->owner=WorldHandle(L);e->id=id;
    luaL_getmetatable(L,type);lua_setmetatable(L,-2);
    lua_newtable(L);lua_pushvalue(L,1);lua_setfield(L,-2,"world");lua_setfenv(L,-2);return 1;
}
static int RemoveWorld(lua_State* L)
{
    auto* handle=WorldHandle(L);auto* s=handle->instance;handle->instance=nullptr;
    if(s)
    {
        if(s->physics)FixedStepScheduler::Detach(*s->physics,s);
        if(s->activeSteps)s->removePending=true;else delete s;
    }
    return 0;
}
static int Remove(lua_State* L,const char* type)
{
    auto* e=static_cast<Entity*>(luaL_checkudata(L,1,type));auto* s=e->owner->instance;CheckStep(L,s);
    if(s)
    {
        if(type==kAttachment)s->coupling.detach(e->id);
        else s->simulation.remove(e->id);
    }
    return 0;
}
static int RemoveBody(lua_State* L){return Remove(L,kBody);}
static int RemoveJoint(lua_State* L){return Remove(L,kJoint);}
static int RemoveTouch(lua_State* L){return Remove(L,kTouch);}
static int RemoveAttachment(lua_State* L){return Remove(L,kAttachment);}
static int NewBody(lua_State* L)
{
    auto* s=World(L);luaL_checktype(L,2,LUA_TTABLE);Field(L,2,"material");
    if(std::strcmp(luaL_checkstring(L,-1),"recoverable"))return luaL_error(L,"assembly mode requires recoverable material");
    lua_pop(L,1);SoftBody::SoftBodyDef def;
    def.mass=Value(L,2,"mass",1,.001,1000);def.halfWidth=Value(L,2,"halfWidth",.35,.01,100);
    def.halfHeight=Value(L,2,"halfHeight",.35,.01,100);
    def.position={float(Value(L,2,"x",0,-1e4,1e4)),float(Value(L,2,"y",0,-1e4,1e4))};
    def.frequency=Value(L,2,"frequency",6,0,1000);def.damping=Value(L,2,"damping",40,0,1e5);
    def.materialScale=Value(L,2,"materialScale",1,.001,1e4);def.friction=Value(L,2,"friction",0,0,10);
    if(Value(L,2,"preload",0,0,1e6)!=0)return luaL_error(L,"recoverable material requires zero preload");
    return Push(L,kBody,s->simulation.createBody(def));
}
static int NewJoint(lua_State* L)
{
    auto* s=World(L);luaL_checktype(L,2,LUA_TTABLE);SoftBody::SoftJointDef def;
    def.bodyA=BodyField(L,"bodyA");def.bodyB=BodyField(L,"bodyB");
    def.weightsA=ReadWeights(L,"weightsA");def.weightsB=ReadWeights(L,"weightsB");
    def.frequency=Value(L,2,"frequency",12,.01,60);def.dampingRatio=Value(L,2,"dampingRatio",1,0,10);
    def.motorGain=Value(L,2,"motorGain",8,0,1e6);
    return Push(L,kJoint,s->simulation.createJoint(def));
}
static int NewTouch(lua_State* L)
{
    auto* s=World(L);luaL_checktype(L,2,LUA_TTABLE);SoftBody::SoftTouchDef def;
    def.body=BodyField(L,"body");def.weights=ReadWeights(L,"weights");
    def.target={float(Value(L,2,"targetX",0,-1e4,1e4)),float(Value(L,2,"targetY",0,-1e4,1e4))};
    def.frequency=Value(L,2,"frequency",3,.01,60);def.dampingRatio=Value(L,2,"dampingRatio",1,0,10);
    def.maxForce=Value(L,2,"maxForce",50,0,1e5);
    return Push(L,kTouch,s->simulation.createTouch(def));
}
static int SetMotor(lua_State* L)
{
    auto* e=Object(L,kJoint);auto& api=e->owner->instance->simulation;
    bool ok=api.setMotor(e->id,NumberArg(L,2,-1e4,1e4),NumberArg(L,3,0,1e6));Check(L,ok,api.lastError());return 0;
}
static int TouchTarget(lua_State* L)
{
    auto* e=Object(L,kTouch);auto& api=e->owner->instance->simulation;
    bool ok=api.setTouchTarget(e->id,{float(NumberArg(L,2,-1e4,1e4)),float(NumberArg(L,3,-1e4,1e4))});Check(L,ok,api.lastError());return 0;
}
static int TouchState(lua_State* L)
{
    auto* e=Object(L,kTouch);SoftBody::SoftTouchState state;
    Check(L,e->owner->instance->simulation.getTouchState(e->id,state),"touch state unavailable");
    lua_createtable(L,0,5);Number(L,"x",state.position[0]);Number(L,"y",state.position[1]);
    Number(L,"targetX",state.target[0]);Number(L,"targetY",state.target[1]);Number(L,"force",state.force);return 1;
}
static int BodyState(lua_State* L)
{
    auto* e=Object(L,kBody);SoftBody::SoftBodyState state;
    Check(L,e->owner->instance->simulation.getBodyState(e->id,state),"body state unavailable");
    lua_createtable(L,0,2);Number(L,"id",e->id.index+1);lua_createtable(L,32,0);
    int n=0;for(auto p:state.vertices)for(float v:p){lua_pushnumber(L,v);lua_rawseti(L,-2,++n);}
    lua_setfield(L,-2,"vertices");return 1;
}
static int SetCollision(lua_State* L)
{
    auto* s=World(L);auto* a=Object(L,kBody,2);auto* b=Object(L,kBody,3);luaL_checktype(L,4,LUA_TBOOLEAN);
    bool ok=s->simulation.setCollisionEnabled(a->id,b->id,lua_toboolean(L,4));Check(L,ok,s->simulation.lastError());return 0;
}
static int AttachBody(lua_State* L)
{
    auto* s=World(L);auto* object=static_cast<DisplayObject*>(LuaProxy::GetProxyableObject(L,2));
    if(!object || !object->GetExtensions())return luaL_error(L,"expected a physics display object");
    return Push(L,kAttachment,s->coupling.attachBody(object->GetExtensions()->GetBody()));
}
static int AttachmentCollision(lua_State* L)
{
    auto* a=Object(L,kAttachment);auto* b=Object(L,kBody,2);luaL_checktype(L,3,LUA_TBOOLEAN);
    auto& api=a->owner->instance->coupling;
    bool ok=api.setCollisionEnabled(a->id,b->id,lua_toboolean(L,3));Check(L,ok,api.lastError());return 0;
}
static int RigidFilter(lua_State* L)
{
    auto* e=Object(L,kBody);luaL_checktype(L,2,LUA_TTABLE);
    double category=Value(L,2,"categoryBits",1,0,65535),mask=Value(L,2,"maskBits",65535,0,65535);
    double group=Value(L,2,"groupIndex",0,-32768,32767);
    if(category!=std::floor(category)||mask!=std::floor(mask)||group!=std::floor(group))return luaL_error(L,"filter fields must be integers");
    SoftBody::RigidFilter filter;filter.categoryBits=std::uint64_t(category);filter.maskBits=std::uint64_t(mask);filter.groupIndex=int(group);
    auto& api=e->owner->instance->coupling;bool ok=api.setFilter(e->id,filter);Check(L,ok,api.lastError());return 0;
}
static int State(lua_State* L)
{
    auto* s=WorldHandle(L)->instance;CheckStep(L,s);if(!s)return luaL_error(L,"soft world was removed");
    auto state=s->simulation.getState();auto coupled=s->coupling.getState();lua_createtable(L,0,32);
    Bool(L,"coupled",true);Bool(L,"invalidated",s->invalidated);Bool(L,"gameMode",false);Bool(L,"assemblyMode",true);Bool(L,"wholeStep",true);
    Number(L,"rigidSteps",coupled.entered);Number(L,"softSteps",coupled.completed);Number(L,"substep",state.completedSteps);
    Number(L,"time",coupled.completed*(1./480));Number(L,"liveInstances",LiveCount());
    Bool(L,"recovering",coupled.recovering);Number(L,"recoveryEntries",coupled.recoveryEntries);Number(L,"recoveryExits",coupled.recoveryExits);
    Number(L,"recoverySteps",coupled.recoverySteps);Number(L,"contactDepthM",coupled.contactDepth);Number(L,"warmHits",coupled.warmHits);
    Number(L,"boxContacts",coupled.contactRows);Number(L,"assemblyPinErrorM",coupled.peakGap);
    Number(L,"motorWork",state.motorWork);Number(L,"appliedWork",state.appliedWork);Number(L,"boundaryWork",0);Number(L,"energy",state.energy);
    Number(L,"penetration",state.penetration);Number(L,"pinError",state.pinError);Number(L,"nodeSpeed",state.nodeSpeed);
    Number(L,"centerSpeed",state.centerSpeed);Number(L,"minAreaRatio",state.minAreaRatio);Bool(L,"selfCross",state.selfCross);
    Number(L,"strictEvents",0);Number(L,"staticPenetration",0);Number(L,"staticQueries",0);Number(L,"staticCandidates",0);
    Number(L,"maxUnforcedEnergy",state.completedSteps?0:state.energy);lua_newtable(L);lua_setfield(L,-2,"minimumY");
    if(state.failure){lua_pushstring(L,state.failure);lua_setfield(L,-2,"failure");}
    lua_createtable(L,int(state.vertices.size()),0);int n=0;
    for(float value:state.vertices){lua_pushnumber(L,value);lua_rawseti(L,-2,++n);}lua_setfield(L,-2,"vertices");return 1;
}
static int ProfileState(lua_State* L)
{
    auto profile=World(L)->coupling.getProfile();lua_createtable(L,0,12);
    Bool(L,"enabled",profile.enabled);Bool(L,"clockFailed",profile.clockFailed);
    const char* names[]={"prepare","materialMotor","initialLinks","contactGeometry","contactSolve","observe","finalize","nativeWorld","advance","adapter"};
    for(int i=0;i<SoftBody::CouplingProfile::Count;++i)
    {
        const auto& sample=profile.samples[i];lua_createtable(L,0,3);
        Number(L,"wallMs",sample.wallMs);Number(L,"cpuMs",sample.cpuMs);Number(L,"calls",sample.calls);lua_setfield(L,-2,names[i]);
    }
    return 1;
}
static int Unsupported(lua_State* L){World(L);return luaL_error(L,"unsupported operation in assembly mode");}
static int UnsupportedLoad(lua_State* L){Object(L,kBody);return luaL_error(L,"game mode supports touch loads only");}
static int ManualStep(lua_State* L){World(L);return luaL_error(L,"coupled soft world is advanced by physics; manual step is forbidden");}
static const char* Validate(Instance& s,PhysicsWorld& physics)
{
    if(s.invalidated || !physics.IsWorldValid() || b2StoreWorldId(s.physicsId)!=b2StoreWorldId(physics.GetWorldId()))return "coupled physics world is no longer valid";
    if(s.scale!=physics.GetPixelsPerMeter())return "physics scale changed; rebuild coupled world";
    if(physics.GetWorld()->GetParticleSystemList())return "coupled soft world does not support LiquidFun in the same world";
    return nullptr;
}
static const char* Prepare(void* owner,PhysicsWorld& physics)
{
    auto& s=*static_cast<Instance*>(owner);if(const char* error=Validate(s,physics))return error;
    return s.coupling.prepare()?nullptr:s.coupling.lastError();
}
static const char* Advance(void* owner,PhysicsWorld& physics,bool& entered)
{
    auto& s=*static_cast<Instance*>(owner);entered=false;
    if(const char* error=Validate(s,physics))return error;
    bool success=s.coupling.advance(&entered);
    return success?nullptr:s.coupling.lastError();
}
static void Invalidate(void* owner){auto* s=static_cast<Instance*>(owner);s->physics=nullptr;s->invalidated=true;}
static void Retain(void* owner){++static_cast<Instance*>(owner)->activeSteps;}
static void Release(void* owner){auto* s=static_cast<Instance*>(owner);if(--s->activeSteps==0 && s->removePending)delete s;}
static int New(lua_State* L)
{
    luaL_checktype(L,1,LUA_TTABLE);Field(L,1,"coupled");bool coupled=lua_toboolean(L,-1);lua_pop(L,1);
    if(!coupled)return luaL_error(L,"assembly mode requires coupled=true");
    Field(L,1,"profile");if(!lua_isnil(L,-1) && !lua_isboolean(L,-1))return luaL_error(L,"profile must be boolean");
    bool profile=lua_toboolean(L,-1);lua_pop(L,1);
    if(profile && !SoftBody::Box2DCoupling::profilingSupported())return luaL_error(L,"profiling requires thread CPU clock support");
    SoftBody::SimulationDef def;
    double dt=Value(L,1,"timeStep",1./480,1./1000,1./30),subs=Value(L,1,"substeps",1,1,32),iterations=Value(L,1,"iterations",8,1,32);
    if(float(dt)!=1.f/480 || subs!=1 || iterations!=8)return luaL_error(L,"coupled world requires timeStep=1/480, substeps=1 and iterations=8");
    double bodies=Value(L,1,"maxBodies",32,1,32),joints=Value(L,1,"maxJoints",64,0,64),touches=Value(L,1,"maxTouches",1,0,64);
    if(bodies!=std::floor(bodies)||joints!=std::floor(joints)||touches!=std::floor(touches))return luaL_error(L,"soft capacities must be integers");
    def.maxBodies=int(bodies);def.maxJoints=int(joints);def.maxTouches=int(touches);
    auto& physics=LuaContext::GetRuntime(L)->GetPhysicsWorld();if(!physics.IsWorldValid())return luaL_error(L,"coupled soft world requires physics.start");
    SoftBody::Box2DCouplingDef box;box.world=physics.GetWorldId();box.flipY=true;box.profile=profile;
    auto* h=static_cast<Handle*>(lua_newuserdata(L,sizeof(Handle)));h->instance=nullptr;luaL_getmetatable(L,kWorld);lua_setmetatable(L,-2);
    h->instance=new(std::nothrow) Instance(def,box);if(!h->instance)return luaL_error(L,"cannot allocate soft simulation");
    auto& s=*h->instance;s.scale=physics.GetPixelsPerMeter();
    if(!s.coupling.ready()){const char* error=s.coupling.lastError();delete h->instance;h->instance=nullptr;return luaL_error(L,"%s",error);}
    FixedStepScheduler::Client client={&s,Prepare,nullptr,Invalidate,Advance,Retain,Release};
    if(const char* error=FixedStepScheduler::Attach(physics,client,def.timeStep))
    {delete h->instance;h->instance=nullptr;return luaL_error(L,"%s",error);}
    s.physics=&physics;return 1;
}
static void Register(lua_State* L)
{
    const luaL_Reg world[]={{"createBody",NewBody},{"createPinJoint",NewJoint},{"createTouchJoint",NewTouch},
        {"attachPhysicsBody",AttachBody},{"attachStaticBody",Unsupported},{"setCollisionEnabled",SetCollision},
        {"createWorldPinJoint",Unsupported},{"createRigidPinJoint",Unsupported},{"createRigidAttachment",Unsupported},{"createBoundary",Unsupported},
        {"step",ManualStep},{"getState",State},{"getProfile",ProfileState},{"removeSelf",RemoveWorld},{"__gc",RemoveWorld},{NULL,NULL}};
    const luaL_Reg body[]={{"getState",BodyState},{"setRigidFilter",RigidFilter},{"applyForce",UnsupportedLoad},{"applyTorque",UnsupportedLoad},{"removeSelf",RemoveBody},{NULL,NULL}};
    const luaL_Reg joint[]={{"setMotor",SetMotor},{"removeSelf",RemoveJoint},{NULL,NULL}};
    const luaL_Reg touch[]={{"setTarget",TouchTarget},{"getState",TouchState},{"removeSelf",RemoveTouch},{NULL,NULL}};
    const luaL_Reg attachment[]={{"setCollisionEnabled",AttachmentCollision},{"removeSelf",RemoveAttachment},{NULL,NULL}};
    const char* names[]={kWorld,kBody,kJoint,kTouch,kAttachment};const luaL_Reg* methods[]={world,body,joint,touch,attachment};
    for(int i=0;i<5;++i){luaL_newmetatable(L,names[i]);luaL_register(L,nullptr,methods[i]);lua_pushvalue(L,-1);lua_setfield(L,-2,"__index");lua_pop(L,1);}
}
}}
