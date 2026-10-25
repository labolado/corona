// Private implementation included inside ExperimentalSoftWorld after Instance.
// Runs only on the Runtime owner thread. No Lua API calls from solver hooks.
static const char* PrepareGame(Instance& s)
{
    auto& solver=*s.game;
    // Scheduler registration happens before Lua can create the first body.
    // Every actual step still requires the complete configuration below.
    if(!s.physics && s.world.bodies.empty())return nullptr;
    if(solver.error)return solver.error;
    solver.world=s.physicsId;solver.pin.active=false;
    for(const auto& pin:s.boxes.pins)if(pin.active)solver.pin=pin;
    solver.filter=s.boxes.filters.empty()?b2DefaultFilter():s.boxes.filters[0];
    if(s.world.bodies.size()==1)solver.friction=s.world.bodies[0].friction;
    b2ShapeId targets[SoftBody::Game::MaxBoxes];int count=0;bool support=!solver.pin.active;
    for(const auto& attachment:s.attachments)if(attachment.active)
    {
        const auto& shape=s.boxes.shapes[attachment.first];
        if(!b2Shape_IsValid(shape.id))return "game attachment shape was removed; detach it before deletion";
        bool excluded=std::find(shape.excludedBodies.begin(),shape.excludedBodies.end(),0)!=shape.excludedBodies.end();
        if(solver.pin.active && B2_ID_EQUALS(attachment.body,solver.pin.rigid))
        {
            if(!excluded)return "disable soft collision with the attachment support";
            support=true;
        }
        else if(!excluded)
        {
            if(count==SoftBody::Game::MaxBoxes)return "game mode supports four contact boxes";
            targets[count++]=shape.id;
        }
    }
    if(!support)return "game attachment support was removed";
    bool changed=count!=solver.boxCount;
    for(int i=0;i<count && !changed;++i)changed=!B2_ID_EQUALS(targets[i],solver.boxes[i].shape);
    if(changed)
    {
        solver.clearCache();solver.boxes={};solver.boxCount=0;
        for(int i=0;i<count;++i)solver.add(targets[i]);
    }
    return solver.validate() && solver.prepare()?nullptr:solver.error;
}
static const char* PrepareAssembly(Instance& s)
{
    auto& solver=*s.assembly;
    if(!s.physics && s.world.bodies.empty())return nullptr;
    if(solver.error)return solver.error;
    solver.world=s.physicsId;
    for(int body=0;body<SoftBody::Game::Assembly::PartCount;++body)
    {
        auto& contact=solver.parts[body].contact;
        contact.filter=body<int(s.boxes.filters.size())?s.boxes.filters[body]:b2DefaultFilter();
        b2ShapeId targets[SoftBody::Game::MaxBoxes];int count=0;
        for(const auto& attachment:s.attachments)if(attachment.active)
        {
            const auto& shape=s.boxes.shapes[attachment.first];
            if(!b2Shape_IsValid(shape.id))return "assembly attachment was removed; detach it before deletion";
            if(std::find(shape.excludedBodies.begin(),shape.excludedBodies.end(),body)!=shape.excludedBodies.end())continue;
            if(count==SoftBody::Game::MaxBoxes)return "assembly mode supports four contact boxes per body";
            targets[count++]=shape.id;
        }
        bool changed=count!=contact.boxCount;
        for(int i=0;i<count && !changed;++i)changed=!B2_ID_EQUALS(targets[i],contact.boxes[i].shape);
        if(changed)
        {
            contact.clearCache();contact.boxes={};contact.boxCount=0;
            for(int i=0;i<count;++i)contact.add(targets[i]);
        }
    }
    return solver.prepare()?nullptr:solver.error;
}
static const char* PrepareCoupled(void* owner,PhysicsWorld& physics)
{
    auto& s=*static_cast<Instance*>(owner);
    if(s.invalidated || !physics.IsWorldValid() || b2StoreWorldId(s.physicsId)!=b2StoreWorldId(physics.GetWorldId()))
        return "coupled physics world is no longer valid";
    if(s.rigidScale!=physics.GetPixelsPerMeter())return "physics scale changed; rebuild coupled world";
    if(physics.GetWorld()->GetParticleSystemList())return "coupled soft world does not support LiquidFun in the same world";
    if(s.world.failure)return s.world.failure;
    for(const auto& attachment:s.attachments)
    {
        if(!attachment.active || !b2Body_IsValid(attachment.body))continue;
        if(b2Body_GetShapeCount(attachment.body)!=1 || !b2Shape_IsValid(s.boxes.shapes[attachment.first].id))
            return "coupled fixtures changed; detach and reattach the body";
    }
    b2Vec2 gravity=b2World_GetGravity(s.physicsId);
    s.world.gravity={gravity.x,-gravity.y};
    return s.assemblyMode?PrepareAssembly(s):(s.gameMode?PrepareGame(s):s.boxes.validate(s.world));
}
static const char* AdvanceCoupled(void* owner,PhysicsWorld& physics)
{
    auto& s=*static_cast<Instance*>(owner);
    ++s.rigidSteps;
    // Existing rigid preCollision callbacks can mutate inputs after preflight.
    // Count the completed rigid step, then reject an incompatible mutation.
    if(const char* error=PrepareCoupled(owner,physics)){s.world.failure=error;return error;}
    if(!s.boxes.step(s.world))return s.world.failure ? s.world.failure : "coupled soft step failed";
    ++s.frames;
    return nullptr;
}
static void InvalidateCoupled(void* owner)
{
    auto& s=*static_cast<Instance*>(owner);
    s.physics=nullptr;s.invalidated=true;
}

// Keep the native context alive when preCollision removes/collects its Lua handle.
static void RetainCoupled(void* owner)
{++static_cast<Instance*>(owner)->activeSteps;}
static void ReleaseCoupled(void* owner)
{
    auto* s=static_cast<Instance*>(owner);
    if(--s->activeSteps==0 && s->removePending)delete s;
}
static const char* AdvanceAttachment(void* owner,PhysicsWorld& physics,bool& entered)
{
    auto& s=*static_cast<Instance*>(owner);auto& bridge=s.boxes;auto& solver=s.attachmentOwner;
    entered=false;
    if(bridge.pins.size()!=1 || !bridge.pins[0].active)return "attachment mode requires one active connection";
    solver.attachment=bridge.pins[0];solver.contact=b2_nullShapeId;
    solver.filter=bridge.filters.empty()?b2DefaultFilter():bridge.filters[0];
    bool support=false;
    for(const auto& shape:bridge.shapes)
    {
        if(!b2Shape_IsValid(shape.id))return "attachment shape was removed";
        bool excluded=std::find(shape.excludedBodies.begin(),shape.excludedBodies.end(),0)!=shape.excludedBodies.end();
        if(B2_ID_EQUALS(b2Shape_GetBody(shape.id),solver.attachment.rigid))
        {
            if(!excluded)return "disable soft collision with the attachment support";
            support=true;
        }
        else
        {
            if(B2_IS_NON_NULL(solver.contact))return "attachment mode supports one contact box";
            if(excluded)return "attachment contact box must be enabled";
            solver.contact=shape.id;
        }
    }
    if(!support)return "attachment support was removed";
    const auto before=solver.guard.entered;
    bool success=solver.advance(physics.GetWorldId(),s.world);
    entered=solver.guard.entered!=before;
    if(entered)++s.rigidSteps;
    if(!success)return solver.guard.error?solver.guard.error:"attachment step failed";
    bridge.pins[0].impulse=solver.impulse;
    ++s.frames;
    return nullptr;
}

static const char* AdvanceGame(void* owner,PhysicsWorld& physics,bool& entered)
{
    auto& s=*static_cast<Instance*>(owner);auto& solver=*s.game;
    entered=false;
    if(const char* error=PrepareCoupled(owner,physics))return error;
    auto before=solver.entered;
    bool success=solver.advance();
    entered=solver.entered!=before;
    if(entered)++s.rigidSteps;
    if(!success)return solver.error?solver.error:"game step failed";
    for(auto& pin:s.boxes.pins)if(pin.active)pin.impulse={solver.impulse.x,-solver.impulse.y};
    ++s.frames;
    return nullptr;
}

static const char* AdvanceAssembly(void* owner,PhysicsWorld& physics,bool& entered)
{
    auto& s=*static_cast<Instance*>(owner);auto& solver=*s.assembly;
    entered=false;
    if(const char* error=PrepareCoupled(owner,physics))return error;
    auto before=solver.entered;
    bool success=solver.advance();
    entered=solver.entered!=before;
    if(entered)++s.rigidSteps;
    if(!success)return solver.error?solver.error:"assembly step failed";
    ++s.frames;
    return nullptr;
}
