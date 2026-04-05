////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BatchObject_Lua_H__
#define _Rtt_BatchObject_Lua_H__

#include "Core/Rtt_Macros.h"

struct lua_State;

// ----------------------------------------------------------------------------

namespace Rtt
{

class BatchObject;

// ----------------------------------------------------------------------------

// SlotProxy userdata: lightweight handle referencing a batch + slot ID
class BatchSlotProxy
{
	public:
		static const char kMetatableName[];

		static void Initialize( lua_State* L );

		// Push a new SlotProxy userdata onto the stack
		static void PushProxy( lua_State* L, BatchObject* batch, int slotId );

	public:
		BatchObject* fBatch;
		int fSlotId;
};

// display.newBatch() implementation
int BatchObject_newBatch( lua_State* L );

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BatchObject_Lua_H__
