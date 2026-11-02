//////////////////////////////////////////////////////////////////////////////
// Private implementation included inside Rtt_LuaPhysicsWorld.cpp's namespace.
// Display coordinates only. Nothing here changes a native physics transform.
// This file is part of the Solar2D engine. See the repository LICENSE.
//////////////////////////////////////////////////////////////////////////////
LuaProxy *ViewProxy( lua_State *L, int index )
{
    luaL_checktype( L, index, LUA_TTABLE );
    LuaProxy *proxy = LuaProxy::GetProxy( L, index );
    const LuaProxyVTable *v = proxy ? &proxy->Delegate() : NULL;
    while ( v && v != &LuaDisplayObjectProxyVTable::Constant() )
    {
        const LuaProxyVTable *parent = &v->Parent();
        v = parent == v ? NULL : parent;
    }
    if ( !v || !proxy->Object() )
        luaL_error( L, "a live display object is required" );
    return proxy;
}
bool ViewClaimed( lua_State *L, const DisplayObject &o )
{
    LuaProxy *proxy = o.GetProxy();
    if ( !proxy )
        return false;
    proxy->PushTable( L );
    lua_pushstring( L, kBinding );
    lua_rawget( L, -2 );
    bool claimed = !lua_isnil( L, -1 );
    lua_pop( L, 2 );
    return claimed;
}
bool ClaimedDescendant( lua_State *L, const DisplayObject &o )
{
    const GroupObject *group = o.AsGroupObject();
    if ( group )
        for ( int i = 0; i < group->NumChildren(); ++i )
        {
            const DisplayObject &child = group->ChildAt( i );
            if ( ViewClaimed( L, child ) || ClaimedDescendant( L, child ) )
                return true;
        }
    return false;
}
bool ViewOnStage( lua_State *L, const DisplayObject &view )
{
    // Removed groups live in an orphanage StageObject, and descendants can
    // retain an old cached stage. Follow the actual parents instead.
    const DisplayObject *top = &view;
    while ( top->GetParent() )
        top = top->GetParent();
    return top == LuaContext::GetRuntime( L )->GetDisplay().GetStage();
}
int WorldViewRoot( lua_State *L )
{
    World &w = GetWorld( L );
    Mutable( L, w );
    if ( w.busy || w.bindingCount )
        return luaL_error( L, "setViewRoot requires an idle world with no bound rigid views" );
    LuaProxy *proxy = NULL;
    if ( !lua_isnoneornil( L, 2 ) )
    {
        proxy = ViewProxy( L, 2 );
        DisplayObject *root = static_cast<DisplayObject *>( proxy->Object() );
        if ( !root->AsGroupObject() || !ViewOnStage( L, *root ) || root->GetExtensions() ||
             ViewClaimed( L, *root ) )
            return luaL_error(
                L, "view root must be an on-stage, unbound display group without a physics body" );
    }
    lua_settop( L, 2 );
    lua_getfenv( L, 1 );
    lua_pushvalue( L, 2 );
    lua_setfield( L, -2, "viewRoot" );
    if ( proxy )
    {
        lua_pushliteral( L, "_proxy" );
        lua_rawget( L, 2 );
    }
    else
        lua_pushnil( L );
    lua_setfield( L, -2, "viewRootProxy" );
    w.viewRoot = proxy;
    return 0;
}

// Stop at the physical display root. Its own transform (and every camera above
// it) cancels out; do not make physics depend on content/stage coordinates.
const char *ParentToViewRoot( lua_State *L, World &w, DisplayObject &view, Matrix &matrix )
{
    DisplayObject *root = static_cast<DisplayObject *>( w.viewRoot->Object() );
    if ( !root || !ViewOnStage( L, *root ) )
        return "view root was removed; unbind views before selecting a new root";
    if ( root->GetExtensions() || ViewClaimed( L, *root ) )
        return "view root must remain unbound and without a physics body";
    if ( static_cast<const DisplayObject &>( view ).GetTransform().GetUserMatrix() )
        return "rooted bindView does not support a view user matrix";
    DisplayObject *parent = view.GetParent();
    while ( parent && parent != root )
    {
        if ( parent->GetExtensions() || ViewClaimed( L, *parent ) )
            return "rooted bindView does not support a physics-driven ancestor";
        if ( parent->ShouldOffsetWithAnchor() )
            return "rooted bindView requires anchorChildren=false on intermediate groups";
        matrix.Prepend( parent->GetMatrix() );
        parent = parent->GetParent();
    }
    return parent == root ? NULL : "bound view must remain a descendant of its world view root";
}

const char *PrepareViewPose( lua_State *L, World &w, Body &b, Binding &binding, DisplayObject &view,
                             double t )
{
    const Pose &z = binding.current;
    const Pose &a = w.interpolating ? binding.previous : z;
    ViewPose pose;
    // Exact endpoints avoid subtract/add restoration drift.
    pose.x = t == 1 ? z.x : a.x + ( (double)z.x - a.x ) * t;
    pose.y = t == 1 ? z.y : a.y + ( (double)z.y - a.y ) * t;
    pose.rotation = ( t == 1 ? z.angle : a.angle + b2UnwindAngle( z.angle - a.angle ) * t ) / kRad;
    if ( binding.authoredStatic && b2Body_GetType( b.id ) == b2_staticBody )
        pose = b.authoredPose;
    if ( binding.offsetX != 0 || binding.offsetY != 0 )
    {
        double c = std::cos( pose.rotation * kRad ), s = std::sin( pose.rotation * kRad );
        pose.x += c * binding.offsetX - s * binding.offsetY;
        pose.y += s * binding.offsetX + c * binding.offsetY;
    }
    pose.rotation += binding.offsetRotation;
    if ( !w.viewRoot )
    {
        binding.output = pose;
        return NULL;
    }
    Matrix parent;
    if ( const char *error = ParentToViewRoot( L, w, view, parent ) )
        return error;
    double a00 = 1, a01 = 0, a10 = 0, a11 = 1, tx = 0, ty = 0;
    if ( !parent.IsIdentity() )
    {
        const Real *r0 = parent.Row0(), *r1 = parent.Row1();
        a00 = r0[0];
        a01 = r0[1];
        tx = r0[2];
        a10 = r1[0];
        a11 = r1[1];
        ty = r1[2];
    }
    double det = a00 * a11 - a01 * a10;
    double norm = std::hypot( a00, a10 ) * std::hypot( a01, a11 );
    if ( !std::isfinite( norm ) || norm == 0 || std::abs( det ) <= norm * 1e-12 )
        return "view parent transform is singular or ill-conditioned";
    // inverse(parent) * rotation(body + offset) * captured visual scale.
    double c = std::cos( pose.rotation * kRad ), s = std::sin( pose.rotation * kRad );
    double x0 = ( a11 * c - a01 * s ) * binding.scaleX / det;
    double y0 = ( -a10 * c + a00 * s ) * binding.scaleX / det;
    double x1 = ( -a11 * s - a01 * c ) * binding.scaleY / det;
    double y1 = ( a10 * s + a00 * c ) * binding.scaleY / det;
    double n0 = std::hypot( x0, y0 ), n1 = std::hypot( x1, y1 );
    if ( n0 == 0 || n1 == 0 || !std::isfinite( n0 * n1 ) )
        return "rooted bindView requires finite nonzero visual scales";
    // A TRS display object cannot express shear. Allow only float matrix noise.
    if ( std::abs( x0 * x1 + y0 * y1 ) > 1e-5 * n0 * n1 )
        return "view parent transform requires unrepresentable shear";
    binding.outputScaleX = std::copysign( n0, binding.scaleX );
    binding.outputScaleY = ( x0 * y1 - y0 * x1 ) / binding.outputScaleX;
    binding.output.rotation = std::atan2( y0 / binding.outputScaleX, x0 / binding.outputScaleX ) / kRad;
    binding.output.x = ( a11 * ( pose.x - tx ) - a01 * ( pose.y - ty ) ) / det;
    binding.output.y = ( -a10 * ( pose.x - tx ) + a00 * ( pose.y - ty ) ) / det;
    for ( double value : { binding.output.x, binding.output.y, binding.output.rotation, binding.outputScaleX,
                           binding.outputScaleY } )
        if ( !std::isfinite( value ) || !std::isfinite( (float)value ) )
            return "view transform exceeds display coordinate range";
    if ( (float)binding.outputScaleX == 0 || (float)binding.outputScaleY == 0 )
        return "view scale underflows display coordinate range";
    return NULL;
}

int BodyBind( lua_State *L )
{
    Body &b = GetBody( L );
    World &w = Owner( L, 1, kBody );
    Mutable( L, w );
    LuaProxy *proxy = ViewProxy( L, 2 );
    DisplayObject *o = static_cast<DisplayObject *>( proxy->Object() );
    if ( o->GetExtensions() || o->IsOrphan() || ( w.viewRoot && !ViewOnStage( L, *o ) ) )
        return luaL_error( L, "bindView requires an on-stage view without a physics body" );
    if ( ViewClaimed( L, *o ) )
        return luaL_error( L, "view is already bound; unbind it first" );
    Binding candidate;
    candidate.proxy = proxy;
    candidate.scaleX = o->GetGeometricProperty( kScaleX );
    candidate.scaleY = o->GetGeometricProperty( kScaleY );
    candidate.current = candidate.previous = ReadPose( w, b );
    if ( !lua_isnoneornil( L, 3 ) )
    {
        luaL_checktype( L, 3, LUA_TTABLE );
        lua_getfield( L, 3, "interpolation" );
        if ( !lua_isnil( L, -1 ) )
            return luaL_error( L, "configure interpolation on the physics world, not bindView" );
        lua_pop( L, 1 );
        Options( L, 3, { "offsetX", "offsetY", "offsetRotation", "staticPose" } );
        candidate.offsetX = Option( L, 3, "offsetX", 0 );
        candidate.offsetY = Option( L, 3, "offsetY", 0 );
        candidate.offsetRotation = Option( L, 3, "offsetRotation", 0 );
        lua_getfield( L, 3, "staticPose" );
        if ( !lua_isnil( L, -1 ) )
        {
            if ( lua_type( L, -1 ) != LUA_TSTRING )
                return luaL_error( L, "staticPose must be 'physics' or 'authored'" );
            const char *policy = lua_tostring( L, -1 );
            if ( strcmp( policy, "physics" ) && strcmp( policy, "authored" ) )
                return luaL_error( L, "staticPose must be 'physics' or 'authored'" );
            candidate.authoredStatic = !strcmp( policy, "authored" );
        }
        lua_pop( L, 1 );
    }
    if ( w.viewRoot && ClaimedDescendant( L, *o ) )
        return luaL_error( L, "rooted bindView does not support nested bound views" );
    if ( const char *error = PrepareViewPose( L, w, b, candidate, *o, 1 ) )
        return luaL_error( L, "%s", error );
    // All checks precede releasing the previous binding. Binding does not write
    // the display until the next syncViews/advanceFrame.
    ClearBinding( L, w, b, 1 );
    lua_getfenv( L, 1 );
    lua_pushvalue( L, 2 );
    lua_setfield( L, -2, "view" );
    lua_pushliteral( L, "_proxy" );
    lua_rawget( L, 2 );
    lua_setfield( L, -2, "proxy" );
    lua_pop( L, 1 );
    lua_pushstring( L, kBinding );
    lua_pushlightuserdata( L, &b );
    lua_rawset( L, 2 );
    b.binding.reset( new Binding( candidate ) );
    ++w.bindingCount;
    Activate( w, b );
    return 0;
}

const char *SyncViews( lua_State *L, World &w, int wi, double alpha )
{
    if ( !w.interpolating )
        alpha = 1;
    const double target = w.index ? (double)w.index - 1 + alpha : 0;
    double renderTime = std::max( w.renderTime, target );
    alpha = w.index ? std::max( 0.0, std::min( 1.0, renderTime - ( (double)w.index - 1 ) ) ) : 1;
    w.lastViewUpdates = 0;
    // Preflight every active rigid view before changing any display property.
    // Return errors to the caller; never longjmp across its Busy/Runtime guards.
    for ( size_t i = 0; i < w.active.size(); )
    {
        Body &b = *w.active[i];
        Binding &binding = *b.binding;
        DisplayObject *o = static_cast<DisplayObject *>( binding.proxy->Object() );
        if ( !o || o->IsOrphan() || o->GetExtensions() || ( w.viewRoot && !ViewOnStage( L, *o ) ) )
        {
            PushHandle( L, wi, "bodies", b.serial );
            ClearBinding( L, w, b, -1 );
            lua_pop( L, 1 );
            continue;
        }
        if ( const char *error = PrepareViewPose( L, w, b, binding, *o, alpha ) )
            return error;
        ++i;
    }
    w.renderTime = renderTime;
    w.viewAlpha = alpha;
    w.syncing = true;
    for ( size_t i = 0; i < w.active.size(); )
    {
        Body &b = *w.active[i];
        Binding &binding = *b.binding;
        DisplayObject *o = static_cast<DisplayObject *>( binding.proxy->Object() );
        o->SetGeometricProperty( kOriginX, (Real)binding.output.x );
        o->SetGeometricProperty( kOriginY, (Real)binding.output.y );
        o->SetGeometricProperty( kRotation, (Real)binding.output.rotation );
        if ( w.viewRoot )
        {
            o->SetGeometricProperty( kScaleX, (Real)binding.outputScaleX );
            o->SetGeometricProperty( kScaleY, (Real)binding.outputScaleY );
        }
        ++w.lastViewUpdates;
        const Pose &z = binding.current;
        const Pose &a = w.interpolating ? binding.previous : z;
        bool same = a.x == z.x && a.y == z.y && a.angle == z.angle;
        // Rooted views must observe parent edits even while the body sleeps.
        if ( !w.viewRoot && !b2Body_IsAwake( b.id ) && ( alpha == 1 || same ) )
            Deactivate( w, b );
        else
            ++i;
    }
    if ( w.soft.owner )
    {
        lua_getfenv( L, wi );
        lua_getfield( L, -1, "softSystem" );
        w.soft.syncViews( L, w.soft.owner, lua_gettop( L ), alpha );
        lua_pop( L, 2 );
    }
    w.syncing = false;
    return NULL;
}
