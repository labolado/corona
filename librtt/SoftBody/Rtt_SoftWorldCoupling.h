// Private implementation included inside ExperimentalSoftWorld after Instance.
// Runs only on the Runtime owner thread. No Lua API calls from solver hooks.
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
    return s.boxes.validate(s.world);
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
