#pragma once
// Private opt-in Lua adapter. Solver is isolated in the Box2D extern tree.
#include "../../external/box2d_v3/extern/softbody/sb_world.h"
#include <new>
#include <cstring>

namespace Rtt { namespace ExperimentalSoftWorld {
using Core=SoftBody::World<float>;
static const char kWorld[]="physics.experimentalSoftWorld";
static const char kBody[]="physics.experimentalSoftBody";
static const char kTouch[]="physics.experimentalSoftTouchJoint";
static const char kPin[]="physics.experimentalSoftPin";
static const char kBoundary[]="physics.experimentalSoftBoundary";
static int sLive=0;
struct Instance
{
    Core world;unsigned long frames=0;double timeStep=1./60;
    Instance(){++sLive;}~Instance(){--sLive;}
};
struct Handle { Instance* instance; };
struct Entity { Handle* owner;int id;unsigned long long generation; };
static Handle* WorldHandle(lua_State* L,int index=1)
{ return static_cast<Handle*>(luaL_checkudata(L,index,kWorld)); }
static Instance* World(lua_State* L,int index=1)
{ Handle* h=WorldHandle(L,index);if(!h->instance)luaL_error(L,"soft world was removed");return h->instance; }
static Entity* Object(lua_State* L,const char* type,int index=1)
{
    Entity* e=static_cast<Entity*>(luaL_checkudata(L,index,type));
    if(!e->owner->instance)luaL_error(L,"soft world was removed");
    auto& w=e->owner->instance->world;
    bool active=false;
    if(type==kBody)active=w.bodies[e->id].active;
    else if(type==kPin)active=w.pins[e->id].active;
    else if(type==kTouch)active=w.touchJoints[e->id].active && w.touchJoints[e->id].generation==e->generation;
    else active=w.boundaries[e->id].active;
    if(!active)luaL_error(L,"soft object was removed");return e;
}
static void Field(lua_State* L,int table,const char* name)
{ lua_pushstring(L,name);lua_rawget(L,table); }
static double Value(lua_State* L,int table,const char* name,double fallback,double low,double high)
{
    Field(L,table,name);double value=fallback;
    if(!lua_isnil(L,-1))
    {
        if(lua_type(L,-1)!=LUA_TNUMBER)luaL_error(L,"%s must be a number",name);
        value=lua_tonumber(L,-1);
    }
    lua_pop(L,1);
    if(!std::isfinite(value)||value<low||value>high)luaL_error(L,"%s is outside its supported range",name);
    return value;
}
static double Argument(lua_State* L,int index,double low,double high)
{
    if(lua_type(L,index)!=LUA_TNUMBER)luaL_error(L,"expected a number");double v=lua_tonumber(L,index);
    if(!std::isfinite(v)||v<low||v>high)luaL_error(L,"number outside supported range");return v;
}
static void Number(lua_State* L,const char* key,double value)
{ lua_pushnumber(L,value);lua_setfield(L,-2,key); }
static void Bool(lua_State* L,const char* key,bool value)
{ lua_pushboolean(L,value);lua_setfield(L,-2,key); }
static void PushEntity(lua_State* L,const char* type,int id)
{
    Handle* owner=WorldHandle(L);
    Entity* e=static_cast<Entity*>(lua_newuserdata(L,sizeof(Entity)));e->owner=owner;e->id=id;
    e->generation=type==kTouch ? owner->instance->world.touchJoints[id].generation : 0;
    luaL_getmetatable(L,type);lua_setmetatable(L,-2);
    // Child owns a Lua reference to the world userdata, never a bare native lifetime.
    lua_newtable(L);lua_pushvalue(L,1);lua_setfield(L,-2,"world");lua_setfenv(L,-2);
}
static int RemoveWorld(lua_State* L)
{ Handle* h=WorldHandle(L);delete h->instance;h->instance=NULL;return 0; }
static int RemoveEntity(lua_State* L,const char* type)
{
    Entity* e=static_cast<Entity*>(luaL_checkudata(L,1,type));
    if(e->owner->instance)
    {
        auto& w=e->owner->instance->world;
        if(type==kBody)w.removeBody(e->id);else if(type==kPin)w.pins[e->id].active=false;else if(type==kTouch){if(w.touchJoints[e->id].generation==e->generation)w.touchJoints[e->id].active=false;}else w.boundaries[e->id].active=false;
    }
    return 0;
}
static int RemoveBody(lua_State* L){return RemoveEntity(L,kBody);}
static int RemoveTouch(lua_State* L){return RemoveEntity(L,kTouch);}
static int RemovePin(lua_State* L){return RemoveEntity(L,kPin);}
static int RemoveBoundary(lua_State* L){return RemoveEntity(L,kBoundary);}
static int NewBody(lua_State* L)
{
    auto& w=World(L)->world;luaL_checktype(L,2,LUA_TTABLE);
    if(w.bodies.size()>=32)return luaL_error(L,"soft body allocation limit reached");
    Field(L,2,"material");const char* material=luaL_checkstring(L,-1);
    int kind=-1;
    if(!strcmp(material,"localFrame"))kind=0;
    else if(!strcmp(material,"pressure"))kind=2;
    else if(!strcmp(material,"recoverable"))kind=3;
    lua_pop(L,1);
    if(kind<0)return luaL_error(L,"unsupported soft material");
    float mass=Value(L,2,"mass",1,.001,1000),rx=Value(L,2,"halfWidth",.35,.01,100),ry=Value(L,2,"halfHeight",.35,.01,100);
    float stiffness=Value(L,2,"edgeStiffness",300,.001,1e6),hz=Value(L,2,"frequency",6,0,1000);
    float damping=Value(L,2,"damping",40,0,1e5),preload=Value(L,2,"preload",0,0,1e6);
    float scale=Value(L,2,"materialScale",1,.001,1e4),overlap=Value(L,2,"frameOverlap",.2,0,100);
    float bulk=Value(L,2,"bulkStiffness",300,.001,1e6);
    float x=Value(L,2,"x",0,-1e4,1e4),y=Value(L,2,"y",0,-1e4,1e4);
    if((kind==2 && scale!=1) || (kind==0 && preload!=0))return luaL_error(L,"materialScale requires localFrame or recoverable; preload is pressure-only");
    if(kind==3 && 2*3.14159265358979323846*hz*std::sqrt(scale)*w.dt>1)
        return luaL_error(L,"recoverable frequency is too high for the world substep; reduce frequency or increase substeps");
    if(kind==3 && (preload!=0 || hz==0))return luaL_error(L,"recoverable material requires positive frequency and zero preload");
    if(kind==2 && preload>0 && rx!=ry)return luaL_error(L,"pressure preload currently requires a circle");
    SoftBody::Body<float> body(kind,mass,rx,ry,stiffness,hz,damping,preload,scale,overlap);body.bulk=bulk;
    for(float length:body.lengths)if(!(length>1e-6))return luaL_error(L,"preload exceeds edge rest length");
    for(auto& p:body.x){p[0]+=x;p[1]+=y;}
    int id=int(w.bodies.size());w.bodies.push_back(body);PushEntity(L,kBody,id);return 1;
}
static void Weights(lua_State* L,const char* field,std::array<float,16>& result)
{
    Field(L,2,field);luaL_checktype(L,-1,LUA_TTABLE);double total=0;
    if(lua_objlen(L,-1)!=16)luaL_error(L,"%s requires 16 weights",field);
    for(int i=0;i<16;++i){lua_rawgeti(L,-1,i+1);result[i]=float(Argument(L,-1,0,1));total+=result[i];lua_pop(L,1);}
    lua_pop(L,1);if(std::abs(total-1)>1e-6)luaL_error(L,"anchor weights must sum to one");
}
static int NewPin(lua_State* L)
{
    Handle* owner=WorldHandle(L);auto& w=World(L)->world;luaL_checktype(L,2,LUA_TTABLE);
    if(w.pins.size()>=64)return luaL_error(L,"soft joint allocation limit reached");
    Field(L,2,"bodyA");Entity* a=Object(L,kBody,-1);lua_pop(L,1);
    Field(L,2,"bodyB");Entity* b=Object(L,kBody,-1);lua_pop(L,1);
    if(a->owner!=owner||b->owner!=owner||a->id==b->id)return luaL_error(L,"pin requires distinct bodies in this world");
    Core::Pin pin;pin.a=a->id;pin.b=b->id;Weights(L,"weightsA",pin.wa);Weights(L,"weightsB",pin.wb);
    pin.gain=Value(L,2,"motorGain",8,0,1e6);
    int id=int(w.pins.size());w.pins.push_back(pin);PushEntity(L,kPin,id);return 1;
}
static int NewWorldPin(lua_State* L)
{
    Handle* owner=WorldHandle(L);
    auto& world=World(L)->world;
    luaL_checktype(L,2,LUA_TTABLE);
    if(world.pins.size()>=64)return luaL_error(L,"soft joint allocation limit reached");
    Field(L,2,"body");Entity* body=Object(L,kBody,-1);lua_pop(L,1);
    if(body->owner!=owner)return luaL_error(L,"pin body belongs to another world");
    Core::Pin pin;pin.a=body->id;pin.b=-1;
    Weights(L,"weights",pin.wa);
    pin.worldAnchor={float(Value(L,2,"x",0,-1e4,1e4)),float(Value(L,2,"y",0,-1e4,1e4))};
    pin.gain=Value(L,2,"motorGain",8,0,1e6);
    int id=int(world.pins.size());world.pins.push_back(pin);PushEntity(L,kPin,id);
    return 1;
}
static int NewTouchJoint(lua_State* L)
{
    Handle* owner=WorldHandle(L);
    Core& world=World(L)->world;
    luaL_checktype(L,2,LUA_TTABLE);
    int id=0;
    while(id<int(world.touchJoints.size()) && world.touchJoints[id].active)++id;
    if(id>=64)return luaL_error(L,"active touch joint limit reached");
    Field(L,2,"body");
    Entity* body=Object(L,kBody,-1);
    lua_pop(L,1);
    if(body->owner!=owner)return luaL_error(L,"touch body must belong to this world");

    Core::TouchJoint joint;
    joint.body=body->id;
    Weights(L,"weights",joint.weights);
    joint.target={float(Value(L,2,"targetX",0,-1e4,1e4)),
                  float(Value(L,2,"targetY",0,-1e4,1e4))};
    joint.frequency=Value(L,2,"frequency",3,.01,60);
    joint.dampingRatio=Value(L,2,"dampingRatio",1,0,10);
    joint.maxForce=Value(L,2,"maxForce",50,0,1e5);
    if(id==int(world.touchJoints.size()))
    {
        world.touchJoints.push_back(joint);
    }
    else
    {
        joint.generation=world.touchJoints[id].generation+1;
        world.touchJoints[id]=joint;
    }
    PushEntity(L,kTouch,id);
    return 1;
}
static int TouchTarget(lua_State* L)
{
    Entity* entity=Object(L,kTouch);
    float x=Argument(L,2,-1e4,1e4);
    float y=Argument(L,3,-1e4,1e4);
    entity->owner->instance->world.touchJoints[entity->id].target={x,y};
    return 0;
}
static int TouchState(lua_State* L)
{
    Entity* entity=Object(L,kTouch);
    Core& world=entity->owner->instance->world;
    const auto& joint=world.touchJoints[entity->id];
    auto point=world.anchor(joint.body,joint.weights);
    lua_createtable(L,0,5);
    Number(L,"x",point[0]);
    Number(L,"y",point[1]);
    Number(L,"targetX",joint.target[0]);
    Number(L,"targetY",joint.target[1]);
    Number(L,"force",joint.lastForce);
    return 1;
}
static int SetMotor(lua_State* L)
{
    Entity* e=Object(L,kPin);float speed=Argument(L,2,-1e4,1e4),limit=Argument(L,3,0,1e6);
    auto& pin=e->owner->instance->world.pins[e->id];pin.speed=speed;pin.maxTorque=limit;return 0;
}
static int SetCollision(lua_State* L)
{
    Handle* owner=WorldHandle(L);auto& w=World(L)->world;Entity* a=Object(L,kBody,2);Entity* b=Object(L,kBody,3);
    luaL_checktype(L,4,LUA_TBOOLEAN);
    if(a->owner!=owner||b->owner!=owner||a->id==b->id)return luaL_error(L,"collision pair must belong to this world");
    for(auto i=w.exclusions.begin();i!=w.exclusions.end();++i)
        if(((*i)[0]==a->id&&(*i)[1]==b->id)||((*i)[0]==b->id&&(*i)[1]==a->id))
        { if(lua_toboolean(L,4))w.exclusions.erase(i);return 0; }
    if(!lua_toboolean(L,4))w.exclusions.push_back({a->id,b->id});return 0;
}
static int NewBoundary(lua_State* L)
{
    auto& w=World(L)->world;luaL_checktype(L,2,LUA_TTABLE);
    if(w.boundaries.size()>=64)return luaL_error(L,"soft boundary allocation limit reached");
    Field(L,2,"type");const char* type=luaL_checkstring(L,-1);
    int kind=!strcmp(type,"plane") ? 0 : (!strcmp(type,"corner") ? 1 : -1);lua_pop(L,1);
    if(kind<0)return luaL_error(L,"unsupported soft boundary");
    Core::Boundary b;b.kind=kind;b.friction=Value(L,2,"friction",.8,0,10);
    if(kind==0)
    {
        b.normal={float(Value(L,2,"normalX",0,-1,1)),float(Value(L,2,"normalY",1,-1,1))};
        if(std::abs(std::hypot(b.normal[0],b.normal[1])-1)>1e-6)return luaL_error(L,"plane normal must be unit length");
        b.target=b.offset=Value(L,2,"offset",0,-1e4,1e4);
    }
    else { b.left=Value(L,2,"left",0,-1e4,1e4);b.top=Value(L,2,"top",0,-1e4,1e4); }
    int id=int(w.boundaries.size());w.boundaries.push_back(b);PushEntity(L,kBoundary,id);return 1;
}
static int Target(lua_State* L)
{
    Entity* e=Object(L,kBoundary);auto& b=e->owner->instance->world.boundaries[e->id];
    if(b.kind!=0)return luaL_error(L,"only planes support prescribed motion");b.target=Argument(L,2,-1e4,1e4);return 0;
}
static int Enable(lua_State* L)
{ Entity* e=Object(L,kBoundary);luaL_checktype(L,2,LUA_TBOOLEAN);e->owner->instance->world.boundaries[e->id].enabled=lua_toboolean(L,2);return 0; }
static int ApplyForce(lua_State* L)
{
    Entity* e=Object(L,kBody);auto& b=e->owner->instance->world.bodies[e->id];
    double x=Argument(L,2,-1e6,1e6)+b.forceInput[0],y=Argument(L,3,-1e6,1e6)+b.forceInput[1];
    if(std::abs(x)>1e6||std::abs(y)>1e6)return luaL_error(L,"accumulated force outside supported range");
    b.forceInput={float(x),float(y)};return 0;
}
static int ApplyTorque(lua_State* L)
{
    Entity* e=Object(L,kBody);auto& b=e->owner->instance->world.bodies[e->id];
    double torque=Argument(L,2,-1e6,1e6)+b.torqueInput;
    if(std::abs(torque)>1e6)return luaL_error(L,"accumulated torque outside supported range");
    b.torqueInput=float(torque);return 0;
}
static int BodyState(lua_State* L)
{
    Entity* e=Object(L,kBody);auto& b=e->owner->instance->world.bodies[e->id];lua_createtable(L,0,2);Number(L,"id",e->id+1);
    lua_createtable(L,32,0);int n=0;for(auto p:b.x)for(float v:p){lua_pushnumber(L,v);lua_rawseti(L,-2,++n);}lua_setfield(L,-2,"vertices");return 1;
}
static int State(lua_State* L)
{
    Instance* s=World(L);auto& w=s->world;auto m=w.currentMetrics();lua_createtable(L,0,16);
    Number(L,"time",s->frames*s->timeStep);Number(L,"substep",double(w.completedSubsteps));Number(L,"liveInstances",sLive);
    Number(L,"motorWork",w.motorWork);Number(L,"boundaryWork",w.boundaryWork);Number(L,"appliedWork",w.appliedWork);Number(L,"energy",m.energy);
    Number(L,"penetration",w.peak.penetration);Number(L,"pinError",w.peak.pin);Number(L,"nodeSpeed",w.peak.nodeSpeed);
    Number(L,"centerSpeed",w.peak.centerSpeed);Number(L,"minAreaRatio",w.peak.minArea);Bool(L,"selfCross",w.peak.selfCross);
    Number(L,"strictEvents",w.strictEvents);
    Number(L,"maxUnforcedEnergy",w.completedSubsteps ? w.maxUnforcedEnergy : m.energy);
    lua_createtable(L,int(w.minimumY.size()),0);
    for(size_t i=0;i<w.minimumY.size();++i)if(w.bodies[i].active){lua_pushnumber(L,w.minimumY[i]);lua_rawseti(L,-2,int(i)+1);}
    lua_setfield(L,-2,"minimumY");
    if(w.failure){lua_pushstring(L,w.failure);lua_setfield(L,-2,"failure");}
    lua_newtable(L);int n=0;
    for(const auto& b:w.bodies)if(b.active)for(auto p:b.x)for(float v:p){lua_pushnumber(L,v);lua_rawseti(L,-2,++n);}
    lua_setfield(L,-2,"vertices");return 1;
}
static int Advance(lua_State* L)
{
    Instance* s=World(L);if(lua_gettop(L)!=1)return luaL_error(L,"world:step takes no arguments");
    s->world.step();if(!s->world.failure)++s->frames;
    lua_pushboolean(L,!s->world.failure);if(s->world.failure){lua_pushstring(L,s->world.failure);return 2;}return 1;
}
static int New(lua_State* L)
{
    luaL_checktype(L,1,LUA_TTABLE);
    double step=Value(L,1,"timeStep",1./60,1./1000,1./30);
    double subs=Value(L,1,"substeps",8,1,32),iterations=Value(L,1,"iterations",8,1,32);
    if(subs!=std::floor(subs)||iterations!=std::floor(iterations))return luaL_error(L,"substeps and iterations must be integers");
    float gx=Value(L,1,"gravityX",0,-1000,1000),gy=Value(L,1,"gravityY",-20,-1000,1000);
    Handle* h=static_cast<Handle*>(lua_newuserdata(L,sizeof(Handle)));h->instance=NULL;
    luaL_getmetatable(L,kWorld);lua_setmetatable(L,-2);h->instance=new(std::nothrow) Instance;
    if(!h->instance)return luaL_error(L,"cannot allocate soft world");
    auto& s=*h->instance;s.timeStep=step;s.world.dt=float(step/subs);s.world.substeps=int(subs);s.world.iterations=int(iterations);s.world.gravity={gx,gy};return 1;
}
static void Metatable(lua_State* L,const char* name,const luaL_Reg* methods)
{ luaL_newmetatable(L,name);luaL_register(L,NULL,methods);lua_pushvalue(L,-1);lua_setfield(L,-2,"__index");lua_pop(L,1); }
static void Register(lua_State* L)
{
    const luaL_Reg world[]={{"createTouchJoint",NewTouchJoint},{"createBody",NewBody},{"createPinJoint",NewPin},{"createWorldPinJoint",NewWorldPin},{"createBoundary",NewBoundary},{"setCollisionEnabled",SetCollision},{"step",Advance},{"getState",State},{"removeSelf",RemoveWorld},{"__gc",RemoveWorld},{NULL,NULL}};
    const luaL_Reg body[]={{"getState",BodyState},{"applyForce",ApplyForce},{"applyTorque",ApplyTorque},{"removeSelf",RemoveBody},{NULL,NULL}};
    const luaL_Reg pin[]={{"setMotor",SetMotor},{"removeSelf",RemovePin},{NULL,NULL}};
    const luaL_Reg boundary[]={{"setTarget",Target},{"setEnabled",Enable},{"removeSelf",RemoveBoundary},{NULL,NULL}};
    const luaL_Reg touch[]={
        {"setTarget",TouchTarget},
        {"getState",TouchState},
        {"removeSelf",RemoveTouch},
        {NULL,NULL}
    };
    Metatable(L,kTouch,touch);
    Metatable(L,kWorld,world);Metatable(L,kBody,body);Metatable(L,kPin,pin);Metatable(L,kBoundary,boundary);
    lua_pushcfunction(L,New);lua_setfield(L,-2,"newExperimentalSoftWorld");
}
} }
