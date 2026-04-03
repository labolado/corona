//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Rtt_LuaProxyVTable.h"

#include "Display/Rtt_CompositeObject.h"
#include "Display/Rtt_Display.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "CoronaLua.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

const LuaCompositeObjectProxyVTable&
LuaCompositeObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaCompositeObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    return 0;
}

const LuaProxyVTable&
LuaCompositeObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
