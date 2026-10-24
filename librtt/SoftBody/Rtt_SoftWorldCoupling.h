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
