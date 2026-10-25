#pragma once
// Private opt-in Lua adapter. Solver is isolated in the Box2D extern tree.
#include "../../external/box2d_v3/extern/softbody/soft_box2d_static.h"
#include "../../external/box2d_v3/extern/softbody/soft_box2d_box.h"
#include "../../external/box2d_v3/extern/softbody/soft_game_world.h"
#include "../../external/box2d_v3/extern/softbody/soft_game_assembly.h"
#include "Rtt_FixedStepScheduler.h"
#include <new>
#include <cstring>

namespace Rtt { namespace ExperimentalSoftWorld {
using Core=SoftBody::World<float>;
static const char kWorld[]="physics.experimentalSoftWorld";
static const char kBody[]="physics.experimentalSoftBody";
static const char kTouch[]="physics.experimentalSoftTouchJoint";
static const char kPin[]="physics.experimentalSoftPin";
static const char kRigidPin[]="physics.experimentalSoftRigidPin";
static const char kRigid[]="physics.experimentalSoftStaticBody";
static const char kBoundary[]="physics.experimentalSoftBoundary";
static int sLive=0;
struct Instance
{
    struct Attachment { b2BodyId body;size_t first,count;bool active=true; };
    Core world;SoftBody::Box2DStaticContacts rigid;
    SoftBody::Box2DBoxContacts boxes;
    SoftBody::Box2DBoxContacts::AttachmentOwner attachmentOwner;
    SoftBody::Game::Solver* game=nullptr;
    SoftBody::Game::Assembly* assembly=nullptr;
    bool assemblyMode=false,gameMode=false,wholeStep=false,removePending=false;
    unsigned activeSteps=0;
    PhysicsWorld* physics=nullptr;
    b2WorldId physicsId=b2_nullWorldId;
    bool coupled=false,invalidated=false;
    unsigned long rigidSteps=0;
    std::vector<Attachment> attachments;
    float rigidScale=0;
    unsigned long frames=0;double timeStep=1./60;
    Instance(){++sLive;}
    ~Instance(){if(physics)FixedStepScheduler::Detach(*physics,this);delete game;delete assembly;--sLive;}
};
#include "Rtt_SoftWorldCoupling.h"
struct Handle { Instance* instance; };
struct Entity { Handle* owner;int id;unsigned long long generation; };
static void CheckActiveStep(lua_State* L,Instance* s)
{
    if(s && s->wholeStep && s->activeSteps)luaL_error(L,"attachment world is executing a native step");
}
static Handle* WorldHandle(lua_State* L,int index=1)
{ return static_cast<Handle*>(luaL_checkudata(L,index,kWorld)); }
static Instance* World(lua_State* L,int index=1)
{ Handle* h=WorldHandle(L,index);CheckActiveStep(L,h->instance);if(!h->instance)luaL_error(L,"soft world was removed");if(h->instance->invalidated)luaL_error(L,"coupled soft world was invalidated by physics.stop");return h->instance; }
static Entity* Object(lua_State* L,const char* type,int index=1)
{
    Entity* e=static_cast<Entity*>(luaL_checkudata(L,index,type));
    CheckActiveStep(L,e->owner->instance);
    if(!e->owner->instance)luaL_error(L,"soft world was removed");
    if(e->owner->instance->invalidated)luaL_error(L,"coupled soft world was invalidated by physics.stop");
    auto& w=e->owner->instance->world;
    bool active=false;
    if(type==kBody)active=w.bodies[e->id].active;
    else if(type==kPin)active=w.pins[e->id].active;
    else if(type==kTouch)active=w.touchJoints[e->id].active && w.touchJoints[e->id].generation==e->generation;
    else if(type==kRigidPin)
    {
        const auto& pin=e->owner->instance->boxes.pins[e->id];
        active=pin.active && w.bodies[pin.softBody].active && b2Body_IsValid(pin.rigid);
    }
    else if(type==kRigid)
    {
        const auto& attachment=e->owner->instance->attachments[e->id];
        active=attachment.active && b2Body_IsValid(attachment.body);
    }
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
{
    Handle* h=WorldHandle(L);Instance* s=h->instance;h->instance=NULL;
    if(s)
    {
        if(s->physics)FixedStepScheduler::Detach(*s->physics,s);
        if(s->activeSteps)s->removePending=true;else delete s;
    }
    return 0;
}
static int RemoveEntity(lua_State* L,const char* type)
{
    Entity* e=static_cast<Entity*>(luaL_checkudata(L,1,type));
    CheckActiveStep(L,e->owner->instance);
    if(e->owner->instance)
    {
        auto& w=e->owner->instance->world;
        if(type==kRigidPin)e->owner->instance->boxes.pins[e->id].active=false;
        else if(type==kBody)w.removeBody(e->id);else if(type==kPin)w.pins[e->id].active=false;else if(type==kTouch){if(w.touchJoints[e->id].generation==e->generation)w.touchJoints[e->id].active=false;}else w.boundaries[e->id].active=false;
    }
    return 0;
}
static int RemoveBody(lua_State* L){return RemoveEntity(L,kBody);}
static int RemoveTouch(lua_State* L){return RemoveEntity(L,kTouch);}
static int RemovePin(lua_State* L){return RemoveEntity(L,kPin);}
static int RemoveRigidPin(lua_State* L){return RemoveEntity(L,kRigidPin);}
static int RemoveBoundary(lua_State* L){return RemoveEntity(L,kBoundary);}
static int NewBody(lua_State* L)
{
    auto& w=World(L)->world;luaL_checktype(L,2,LUA_TTABLE);
    if(World(L)->gameMode && !w.bodies.empty())return luaL_error(L,"game mode supports one soft body");
    if(World(L)->assemblyMode && w.bodies.size()>=3)return luaL_error(L,"assembly mode supports three soft bodies");
    if(w.bodies.size()>=32)return luaL_error(L,"soft body allocation limit reached");
    Field(L,2,"material");const char* material=luaL_checkstring(L,-1);
    int kind=-1;
    if(!strcmp(material,"localFrame"))kind=0;
    else if(!strcmp(material,"pressure"))kind=2;
    else if(!strcmp(material,"recoverable"))kind=3;
    lua_pop(L,1);
    if((World(L)->gameMode || World(L)->assemblyMode) && kind!=3)return luaL_error(L,"game mode requires recoverable material");
    if(kind<0)return luaL_error(L,"unsupported soft material");
    float mass=Value(L,2,"mass",1,.001,1000),rx=Value(L,2,"halfWidth",.35,.01,100),ry=Value(L,2,"halfHeight",.35,.01,100);
    float stiffness=Value(L,2,"edgeStiffness",300,.001,1e6),hz=Value(L,2,"frequency",6,0,1000);
    float damping=Value(L,2,"damping",40,0,1e5),preload=Value(L,2,"preload",0,0,1e6);
    float scale=Value(L,2,"materialScale",1,.001,1e4),overlap=Value(L,2,"frameOverlap",.2,0,100);
    float friction=Value(L,2,"friction",0,0,10);
    float bulk=Value(L,2,"bulkStiffness",300,.001,1e6);
    float x=Value(L,2,"x",0,-1e4,1e4),y=Value(L,2,"y",0,-1e4,1e4);
    if((kind==2 && scale!=1) || (kind==0 && preload!=0))return luaL_error(L,"materialScale requires localFrame or recoverable; preload is pressure-only");
    if(kind==3 && 2*3.14159265358979323846*hz*std::sqrt(scale)*w.dt>1)
        return luaL_error(L,"recoverable frequency is too high for the world substep; reduce frequency or increase substeps");
    if(kind==3 && (preload!=0 || hz==0))return luaL_error(L,"recoverable material requires positive frequency and zero preload");
    if(kind==2 && preload>0 && rx!=ry)return luaL_error(L,"pressure preload currently requires a circle");
    SoftBody::Body<float> body(kind,mass,rx,ry,stiffness,hz,damping,preload,scale,overlap);body.bulk=bulk;body.friction=friction;
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
    if(World(L)->gameMode)return luaL_error(L,"game mode has no soft-soft pins yet");
    Handle* owner=WorldHandle(L);auto& w=World(L)->world;luaL_checktype(L,2,LUA_TTABLE);
    if(owner->instance->assemblyMode && w.pins.size()>=2)return luaL_error(L,"assembly mode supports two connections");
    if(w.pins.size()>=64)return luaL_error(L,"soft joint allocation limit reached");
    Field(L,2,"bodyA");Entity* a=Object(L,kBody,-1);lua_pop(L,1);
    Field(L,2,"bodyB");Entity* b=Object(L,kBody,-1);lua_pop(L,1);
    if(a->owner!=owner||b->owner!=owner||a->id==b->id)return luaL_error(L,"pin requires distinct bodies in this world");
    Core::Pin pin;pin.a=a->id;pin.b=b->id;Weights(L,"weightsA",pin.wa);Weights(L,"weightsB",pin.wb);
    pin.gain=Value(L,2,"motorGain",8,0,1e6);
    int id=int(w.pins.size());
    if(owner->instance->assemblyMode)
    {
        auto& link=owner->instance->assembly->links[id];
        link.frequency=Value(L,2,"frequency",12,.01,60);
        link.dampingRatio=Value(L,2,"dampingRatio",1,0,10);
    }
    w.pins.push_back(pin);PushEntity(L,kPin,id);return 1;
}
static int NewWorldPin(lua_State* L)
{
    if(World(L)->gameMode || World(L)->assemblyMode)return luaL_error(L,"this mode does not support world pins");
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
    if(owner->instance->wholeStep)
        for(const auto& touch:world.touchJoints)if(touch.active)
            return luaL_error(L,"attachment mode supports one active touch joint");
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
static int SetBoundaryCollision(lua_State* L)
{
    Entity* boundary=Object(L,kBoundary);
    Entity* body=Object(L,kBody,2);
    if(boundary->owner!=body->owner)return luaL_error(L,"body belongs to another world");
    luaL_checktype(L,3,LUA_TBOOLEAN);
    auto& exclusions=boundary->owner->instance->world.boundaries[boundary->id].excludedBodies;
    bool enabled=lua_toboolean(L,3);
    for(auto i=exclusions.begin();i!=exclusions.end();++i)
    {
        if(*i==body->id)
        {
            if(enabled)exclusions.erase(i);
            return 0;
        }
    }
    if(!enabled)exclusions.push_back(body->id);
    return 0;
}
static int NewBoundary(lua_State* L)
{
    if(World(L)->gameMode || World(L)->assemblyMode)return luaL_error(L,"this mode uses attached native boxes");
    auto& w=World(L)->world;luaL_checktype(L,2,LUA_TTABLE);
    if(w.boundaries.size()>=64)return luaL_error(L,"soft boundary allocation limit reached");
    Field(L,2,"type");const char* type=luaL_checkstring(L,-1);
    int kind=!strcmp(type,"plane") ? 0 : (!strcmp(type,"corner") ? 1 : (!strcmp(type,"box") ? 2 : -1));lua_pop(L,1);
    if(kind<0)return luaL_error(L,"unsupported soft boundary");
    Core::Boundary b;b.kind=kind;b.friction=Value(L,2,"friction",.8,0,10);
    if(kind==0)
    {
        b.normal={float(Value(L,2,"normalX",0,-1,1)),float(Value(L,2,"normalY",1,-1,1))};
        if(std::abs(std::hypot(b.normal[0],b.normal[1])-1)>1e-6)return luaL_error(L,"plane normal must be unit length");
        b.target=b.offset=Value(L,2,"offset",0,-1e4,1e4);
    }
    else if(kind==2)
    {
        b.center={float(Value(L,2,"x",0,-1e4,1e4)),float(Value(L,2,"y",0,-1e4,1e4))};
        b.halfSize={float(Value(L,2,"halfWidth",1,.001,1e4)),float(Value(L,2,"halfHeight",.1,.001,1e4))};
        double angle=Value(L,2,"angle",0,-360,360)*3.14159265358979323846/180;
        b.axis={float(std::cos(angle)),float(std::sin(angle))};
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
    if(Object(L,kBody)->owner->instance->gameMode || Object(L,kBody)->owner->instance->assemblyMode)return luaL_error(L,"game mode supports touch loads only");
    Entity* e=Object(L,kBody);auto& b=e->owner->instance->world.bodies[e->id];
    double x=Argument(L,2,-1e6,1e6)+b.forceInput[0],y=Argument(L,3,-1e6,1e6)+b.forceInput[1];
    if(std::abs(x)>1e6||std::abs(y)>1e6)return luaL_error(L,"accumulated force outside supported range");
    b.forceInput={float(x),float(y)};return 0;
}
static int ApplyTorque(lua_State* L)
{
    if(Object(L,kBody)->owner->instance->gameMode || Object(L,kBody)->owner->instance->assemblyMode)return luaL_error(L,"game mode supports touch loads only");
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
    Instance* s=WorldHandle(L)->instance;CheckActiveStep(L,s);if(!s)return luaL_error(L,"soft world was removed");
    auto& w=s->world;auto m=w.currentMetrics();lua_createtable(L,0,20);
    Bool(L,"coupled",s->coupled);Bool(L,"invalidated",s->invalidated);
    Number(L,"rigidSteps",s->rigidSteps);Number(L,"softSteps",s->frames);
    Number(L,"boxContacts",s->gameMode?s->game->rowSolves:(s->wholeStep?s->attachmentOwner.contacts:s->boxes.contacts));
    Bool(L,"gameMode",s->gameMode);Bool(L,"assemblyMode",s->assemblyMode);
    if(s->assemblyMode)
    {
        unsigned long long entries=0,exits=0,steps=0,warm=0,rows=0;double depth=0;bool recovering=false;
        for(const auto& part:s->assembly->parts)
        {
            const auto& c=part.contact;entries+=c.recovery.entries;exits+=c.recovery.exits;
            steps+=c.recovery.activeSteps;warm+=c.warmHits;rows+=c.rowSolves;
            depth=std::max(depth,c.lastDepth);recovering=recovering||c.recovery.active;
        }
        Bool(L,"recovering",recovering);Number(L,"recoveryEntries",entries);Number(L,"recoveryExits",exits);
        Number(L,"recoverySteps",steps);Number(L,"contactDepthM",depth);Number(L,"warmHits",warm);
        Number(L,"boxContacts",rows);Number(L,"assemblyPinErrorM",s->assembly->peakGap);
    }
    if(s->gameMode)
    {
        Bool(L,"recovering",s->game->recovery.active);
        Number(L,"recoveryEntries",s->game->recovery.entries);
        Number(L,"recoveryExits",s->game->recovery.exits);
        Number(L,"recoverySteps",s->game->recovery.activeSteps);
        Number(L,"contactDepthM",s->game->lastDepth);
        Number(L,"warmHits",s->game->warmHits);
    }
    Bool(L,"wholeStep",s->wholeStep);
    Number(L,"time",s->frames*s->timeStep);Number(L,"substep",double(w.completedSubsteps));Number(L,"liveInstances",sLive);
    Number(L,"motorWork",w.motorWork);Number(L,"boundaryWork",w.boundaryWork);Number(L,"appliedWork",w.appliedWork);Number(L,"energy",m.energy);
    Number(L,"penetration",w.peak.penetration);Number(L,"pinError",w.peak.pin);Number(L,"nodeSpeed",w.peak.nodeSpeed);
    Number(L,"centerSpeed",w.peak.centerSpeed);Number(L,"minAreaRatio",w.peak.minArea);Bool(L,"selfCross",w.peak.selfCross);
    Number(L,"strictEvents",w.strictEvents);
    Number(L,"staticPenetration",s->rigid.penetration(w));
    Number(L,"staticQueries",double(s->rigid.queryCount));
    Number(L,"staticCandidates",double(s->rigid.candidateCount));
    Number(L,"maxUnforcedEnergy",w.completedSubsteps ? w.maxUnforcedEnergy : m.energy);
    lua_createtable(L,int(w.minimumY.size()),0);
    for(size_t i=0;i<w.minimumY.size();++i)if(w.bodies[i].active){lua_pushnumber(L,w.minimumY[i]);lua_rawseti(L,-2,int(i)+1);}
    lua_setfield(L,-2,"minimumY");
    if(w.failure){lua_pushstring(L,w.failure);lua_setfield(L,-2,"failure");}
    lua_newtable(L);int n=0;
    for(const auto& b:w.bodies)if(b.active)for(auto p:b.x)for(float v:p){lua_pushnumber(L,v);lua_rawseti(L,-2,++n);}
    lua_setfield(L,-2,"vertices");return 1;
}
// Static-only bridge into the existing experimental, metre/up-positive world.
// The application retains ownership of the registered display/physics body.
static int AttachStatic(lua_State* L)
{
    Instance* instance=World(L);
    if(instance->coupled)return luaL_error(L,"use attachPhysicsBody for a coupled soft world");
    auto& physics=LuaContext::GetRuntime(L)->GetPhysicsWorld();
    DisplayObject* object=static_cast<DisplayObject*>(LuaProxy::GetProxyableObject(L,2));
    if(!object || !object->GetExtensions())return luaL_error(L,"attachStaticBody requires a physics display object");
    b2BodyId body=object->GetExtensions()->GetBody();
    if(!b2Body_IsValid(body) || b2Body_GetType(body)!=b2_staticBody)
        return luaL_error(L,"attachStaticBody requires a live static body");
    if(instance->attachments.size()>=64)return luaL_error(L,"static attachment limit reached");
    for(const auto& attachment:instance->attachments)
        if(attachment.active && b2StoreBodyId(attachment.body)==b2StoreBodyId(body))
            return luaL_error(L,"static body already attached");
    float scale=physics.GetPixelsPerMeter();
    if(instance->rigidScale!=0 && instance->rigidScale!=scale)
        return luaL_error(L,"physics scale changed; recreate the experimental soft world");
    int count=b2Body_GetShapeCount(body);
    if(count==0)return luaL_error(L,"static body has no shapes");
    // Validate with a temporary bridge, and destroy C++ temporaries before luaL_error.
    const char* error=nullptr;
    {
        SoftBody::Box2DStaticContacts candidate;
        std::vector<b2ShapeId> shapes(count);b2Body_GetShapes(body,shapes.data(),count);
        for(auto shape:shapes)candidate.shapes.push_back({shape,{}});
        error=candidate.validate();
        if(!error)
        {
            Instance::Attachment attachment;attachment.body=body;
            attachment.first=instance->rigid.shapes.size();attachment.count=shapes.size();
            instance->rigid.shapes.insert(instance->rigid.shapes.end(),candidate.shapes.begin(),candidate.shapes.end());
            instance->attachments.push_back(attachment);
        }
    }
    if(error)return luaL_error(L,"%s",error);
    instance->rigidScale=scale;instance->rigid.flipY=true;
    PushEntity(L,kRigid,int(instance->attachments.size())-1);return 1;
}
static int AttachPhysics(lua_State* L)
{
    Instance* instance=World(L);
    if(!instance->coupled)return luaL_error(L,"attachPhysicsBody requires a coupled soft world");
    DisplayObject* object=static_cast<DisplayObject*>(LuaProxy::GetProxyableObject(L,2));
    if(!object || !object->GetExtensions())return luaL_error(L,"expected a physics display object");
    b2BodyId body=object->GetExtensions()->GetBody();
    if(!b2Body_IsValid(body) || b2StoreWorldId(b2Body_GetWorld(body))!=b2StoreWorldId(instance->physicsId))
        return luaL_error(L,"attachment must belong to the coupled physics world");
    if(instance->attachments.size()>=64)return luaL_error(L,"attachment limit reached");
    if(b2Body_GetShapeCount(body)!=1)return luaL_error(L,"attachment requires one centred sharp box");
    b2ShapeId shape;b2Body_GetShapes(body,&shape,1);
    instance->boxes.shapes.push_back({shape,{}});
    const char* error=nullptr;
    if(instance->gameMode || instance->assemblyMode)
    {
        // Validate the new fixture independently; a pin may be added afterward.
        SoftBody::Game::Solver candidate(instance->world);candidate.world=instance->physicsId;
        candidate.add(shape);if(!candidate.prepare())error=candidate.error;
        for(const auto& a:instance->attachments)if(a.active && B2_ID_EQUALS(a.body,body))error="body already attached";
    }
    else error=instance->boxes.validate(instance->world);
    if(error){instance->boxes.shapes.pop_back();return luaL_error(L,"%s",error);}
    Instance::Attachment attachment;attachment.body=body;attachment.first=instance->boxes.shapes.size()-1;attachment.count=1;
    instance->attachments.push_back(attachment);
    PushEntity(L,kRigid,int(instance->attachments.size())-1);return 1;
}
static int NewRigidConnection(lua_State* L,bool hard)
{
    Instance* instance=World(L);Handle* owner=WorldHandle(L);luaL_checktype(L,2,LUA_TTABLE);
    if(!instance->coupled)return luaL_error(L,"rigid pin requires a coupled soft world");
    if(instance->assemblyMode)return luaL_error(L,"assembly mode does not support rigid pins yet");
    if(hard!=instance->wholeStep)return luaL_error(L,"connection type does not match couplingMode");
    if(hard && !instance->boxes.pins.empty())return luaL_error(L,"attachment mode supports one connection");
    if(hard)
    {
        Field(L,2,"frequency");bool frequency=!lua_isnil(L,-1);lua_pop(L,1);
        Field(L,2,"dampingRatio");bool damping=!lua_isnil(L,-1);lua_pop(L,1);
        if(frequency || damping)return luaL_error(L,"rigid attachment has no spring parameters");
    }
    if(instance->boxes.pins.size()>=64)return luaL_error(L,"rigid pin allocation limit reached");
    Field(L,2,"body");Entity* soft=Object(L,kBody,-1);lua_pop(L,1);
    Field(L,2,"rigid");Entity* rigid=Object(L,kRigid,-1);lua_pop(L,1);
    if(soft->owner!=owner || rigid->owner!=owner)return luaL_error(L,"rigid pin entities must belong to this world");
    SoftBody::Box2DBoxContacts::Pin pin;
    pin.softBody=soft->id;pin.rigid=instance->attachments[rigid->id].body;
    if(b2Body_GetType(pin.rigid)!=b2_dynamicBody)return luaL_error(L,"rigid pin requires a dynamic body");
    if(!b2Body_IsEnabled(pin.rigid))return luaL_error(L,"rigid pin requires an enabled body");
    Weights(L,"weights",pin.weights);
    pin.localAnchor={float(Value(L,2,"localAnchorX",0,-100,100)),-float(Value(L,2,"localAnchorY",0,-100,100))};
    pin.frequency=Value(L,2,"frequency",30,1,60);
    pin.dampingRatio=Value(L,2,"dampingRatio",1,0,10);
    if(instance->boxes.pinError(instance->world,pin)>.001)return luaL_error(L,"rigid pin anchors must initially coincide within 1mm");
    instance->boxes.pins.push_back(pin);
    const char* error=instance->gameMode?nullptr:instance->boxes.validate(instance->world);
    if(error){instance->boxes.pins.pop_back();return luaL_error(L,"%s",error);}
    PushEntity(L,kRigidPin,int(instance->boxes.pins.size())-1);return 1;
}
static int NewRigidPin(lua_State* L){return NewRigidConnection(L,false);}
static int NewRigidAttachment(lua_State* L){return NewRigidConnection(L,true);}
static int RigidPinState(lua_State* L)
{
    Entity* entity=Object(L,kRigidPin);auto& instance=*entity->owner->instance;
    const auto& pin=instance.boxes.pins[entity->id];lua_createtable(L,0,3);
    Bool(L,"enabled",instance.boxes.pinActive(instance.world,pin));
    Number(L,"errorM",instance.boxes.pinError(instance.world,pin));
    Number(L,"impulseX",pin.impulse.x);Number(L,"impulseY",-pin.impulse.y);
    return 1;
}
static int RemoveStatic(lua_State* L)
{
    Entity* entity=static_cast<Entity*>(luaL_checkudata(L,1,kRigid));
    CheckActiveStep(L,entity->owner->instance);
    if(entity->owner->instance)
    {
        auto& instance=*entity->owner->instance;
        auto& attachment=instance.attachments[entity->id];attachment.active=false;
        for(auto& pin:instance.boxes.pins)if(B2_ID_EQUALS(pin.rigid,attachment.body))pin.active=false;
        for(size_t i=0;i<attachment.count;++i)
            if(instance.coupled)instance.boxes.shapes[attachment.first+i].id=b2_nullShapeId;
            else instance.rigid.shapes[attachment.first+i].id=b2_nullShapeId;
    }
    return 0;
}
static int StaticCollision(lua_State* L)
{
    Entity* boundary=Object(L,kRigid);Entity* body=Object(L,kBody,2);
    if(boundary->owner!=body->owner)return luaL_error(L,"body belongs to another world");
    luaL_checktype(L,3,LUA_TBOOLEAN);bool enabled=lua_toboolean(L,3);
    auto& instance=*boundary->owner->instance;
    const auto& attachment=instance.attachments[boundary->id];
    for(size_t i=0;i<attachment.count;++i)
    {
        auto& excluded=instance.coupled ? instance.boxes.shapes[attachment.first+i].excludedBodies : instance.rigid.shapes[attachment.first+i].excludedBodies;
        auto found=std::find(excluded.begin(),excluded.end(),body->id);
        if(enabled && found!=excluded.end())excluded.erase(found);
        else if(!enabled && found==excluded.end())excluded.push_back(body->id);
    }
    return 0;
}
static int RigidFilter(lua_State* L)
{
    Entity* entity=Object(L,kBody);luaL_checktype(L,2,LUA_TTABLE);
    double category=Value(L,2,"categoryBits",1,0,65535),mask=Value(L,2,"maskBits",65535,0,65535);
    double group=Value(L,2,"groupIndex",0,-32768,32767);
    if(category!=std::floor(category)||mask!=std::floor(mask)||group!=std::floor(group))
        return luaL_error(L,"filter fields must be integers");
    auto& instance=*entity->owner->instance;
    auto& filters=instance.coupled ? instance.boxes.filters : instance.rigid.filters;
    if(filters.size()<=size_t(entity->id))filters.resize(entity->id+1,b2DefaultFilter());
    filters[entity->id]={uint64_t(category),uint64_t(mask),int(group)};return 0;
}
static int Advance(lua_State* L)
{
    Instance* s=World(L);if(lua_gettop(L)!=1)return luaL_error(L,"world:step takes no arguments");
    if(s->coupled)return luaL_error(L,"coupled soft world is advanced by physics; manual step is forbidden");
    if(s->rigidScale!=0)
    {
        if(s->rigidScale!=LuaContext::GetRuntime(L)->GetPhysicsWorld().GetPixelsPerMeter())
            return luaL_error(L,"physics scale changed; recreate the experimental soft world");
        for(const auto& attachment:s->attachments)
        {
            if(!attachment.active || !b2Body_IsValid(attachment.body))continue;
            if(size_t(b2Body_GetShapeCount(attachment.body))!=attachment.count)
                return luaL_error(L,"static fixtures changed; detach and reattach the body");
            for(size_t i=0;i<attachment.count;++i)
                if(!b2Shape_IsValid(s->rigid.shapes[attachment.first+i].id))
                    return luaL_error(L,"static fixtures changed; detach and reattach the body");
        }
        s->rigid.step(s->world);
    }
    else s->world.step();
    if(!s->world.failure)++s->frames;
    lua_pushboolean(L,!s->world.failure);if(s->world.failure){lua_pushstring(L,s->world.failure);return 2;}return 1;
}
static int New(lua_State* L)
{
    luaL_checktype(L,1,LUA_TTABLE);
    Field(L,1,"coupled");bool coupled=lua_toboolean(L,-1);lua_pop(L,1);
    Field(L,1,"couplingMode");bool wholeStep=false,gameMode=false,assemblyMode=false;
    if(!lua_isnil(L,-1))
    {
        if(lua_type(L,-1)!=LUA_TSTRING)return luaL_error(L,"couplingMode must be attachment, game, assembly or omitted");
        gameMode=!std::strcmp(lua_tostring(L,-1),"game");
        assemblyMode=!std::strcmp(lua_tostring(L,-1),"assembly");
        if(!gameMode && !assemblyMode && std::strcmp(lua_tostring(L,-1),"attachment"))
            return luaL_error(L,"couplingMode must be attachment, game, assembly or omitted");
        wholeStep=true;
    }
    lua_pop(L,1);
    if(wholeStep && !coupled)return luaL_error(L,"attachment mode requires coupled=true");
    double step=Value(L,1,"timeStep",coupled ? 1./480 : 1./60,1./1000,1./30);
    double subs=Value(L,1,"substeps",coupled ? 1 : 8,1,32),iterations=Value(L,1,"iterations",8,1,32);
    if(subs!=std::floor(subs)||iterations!=std::floor(iterations))return luaL_error(L,"substeps and iterations must be integers");
    if(coupled && (float(step)!=1.f/480 || subs!=1))return luaL_error(L,"coupled world requires timeStep=1/480 and substeps=1");
    if((gameMode || assemblyMode) && iterations!=8)return luaL_error(L,"game mode requires iterations=8");
    float gx=Value(L,1,"gravityX",0,-1000,1000),gy=Value(L,1,"gravityY",-20,-1000,1000);
    Handle* h=static_cast<Handle*>(lua_newuserdata(L,sizeof(Handle)));h->instance=NULL;
    luaL_getmetatable(L,kWorld);lua_setmetatable(L,-2);h->instance=new(std::nothrow) Instance;
    if(!h->instance)return luaL_error(L,"cannot allocate soft world");
    auto& s=*h->instance;s.assemblyMode=assemblyMode;s.gameMode=gameMode;s.wholeStep=wholeStep;s.attachmentOwner.flipY=true;s.timeStep=step;s.world.dt=float(step/subs);s.world.substeps=int(subs);s.world.iterations=int(iterations);s.world.gravity={gx,gy};
    if(gameMode)
    {
        s.game=new(std::nothrow) SoftBody::Game::Solver(s.world);
        if(!s.game){delete h->instance;h->instance=nullptr;return luaL_error(L,"cannot allocate game solver");}
        s.game->flipY=true;
    }
    if(assemblyMode)
    {
        s.assembly=new(std::nothrow) SoftBody::Game::Assembly(s.world);
        if(!s.assembly){delete h->instance;h->instance=nullptr;return luaL_error(L,"cannot allocate assembly solver");}
        s.assembly->flipY=true;
    }
    if(coupled)
    {
        auto& physics=LuaContext::GetRuntime(L)->GetPhysicsWorld();
        if(!physics.IsWorldValid()){delete h->instance;h->instance=nullptr;return luaL_error(L,"coupled soft world requires physics.start");}
        s.coupled=true;s.timeStep=s.world.dt;s.physicsId=physics.GetWorldId();s.rigidScale=physics.GetPixelsPerMeter();s.boxes.flipY=true;
        FixedStepScheduler::Client client={&s,PrepareCoupled,AdvanceCoupled,InvalidateCoupled,
            assemblyMode?AdvanceAssembly:(gameMode?AdvanceGame:(wholeStep?AdvanceAttachment:nullptr)),wholeStep?RetainCoupled:nullptr,wholeStep?ReleaseCoupled:nullptr};
        const char* error=FixedStepScheduler::Attach(physics,client,s.world.dt);
        if(error){delete h->instance;h->instance=nullptr;return luaL_error(L,"%s",error);}
        s.physics=&physics;
    }
    return 1;
}
static void Metatable(lua_State* L,const char* name,const luaL_Reg* methods)
{ luaL_newmetatable(L,name);luaL_register(L,NULL,methods);lua_pushvalue(L,-1);lua_setfield(L,-2,"__index");lua_pop(L,1); }
static void Register(lua_State* L)
{
    const luaL_Reg world[]={{"createRigidAttachment",NewRigidAttachment},{"createRigidPinJoint",NewRigidPin},{"attachPhysicsBody",AttachPhysics},{"attachStaticBody",AttachStatic},{"createTouchJoint",NewTouchJoint},{"createBody",NewBody},{"createPinJoint",NewPin},{"createWorldPinJoint",NewWorldPin},{"createBoundary",NewBoundary},{"setCollisionEnabled",SetCollision},{"step",Advance},{"getState",State},{"removeSelf",RemoveWorld},{"__gc",RemoveWorld},{NULL,NULL}};
    const luaL_Reg body[]={{"setRigidFilter",RigidFilter},{"getState",BodyState},{"applyForce",ApplyForce},{"applyTorque",ApplyTorque},{"removeSelf",RemoveBody},{NULL,NULL}};
    const luaL_Reg pin[]={{"setMotor",SetMotor},{"removeSelf",RemovePin},{NULL,NULL}};
    const luaL_Reg boundary[]={{"setCollisionEnabled",SetBoundaryCollision},{"setTarget",Target},{"setEnabled",Enable},{"removeSelf",RemoveBoundary},{NULL,NULL}};
    const luaL_Reg touch[]={
        {"setTarget",TouchTarget},
        {"getState",TouchState},
        {"removeSelf",RemoveTouch},
        {NULL,NULL}
    };
    const luaL_Reg rigid[]={{"setCollisionEnabled",StaticCollision},{"removeSelf",RemoveStatic},{NULL,NULL}};
    const luaL_Reg rigidPin[]={{"getState",RigidPinState},{"removeSelf",RemoveRigidPin},{NULL,NULL}};
    Metatable(L,kRigidPin,rigidPin);
    Metatable(L,kRigid,rigid);
    Metatable(L,kTouch,touch);
    Metatable(L,kWorld,world);Metatable(L,kBody,body);Metatable(L,kPin,pin);Metatable(L,kBoundary,boundary);
    lua_pushcfunction(L,New);lua_setfield(L,-2,"newExperimentalSoftWorld");
}
} }
