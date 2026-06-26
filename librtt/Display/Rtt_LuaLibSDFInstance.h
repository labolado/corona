////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// Author: Labo Lado, laboladoapp@gmail.com
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_LuaLibSDFInstance_H__
#define _Rtt_LuaLibSDFInstance_H__

#include "Rtt_LuaProxyVTable.h"

namespace Rtt
{

class LuaSDFGroupObjectProxyVTable : public LuaDisplayObjectProxyVTable
{
	public:
		typedef LuaSDFGroupObjectProxyVTable Self;
		typedef LuaDisplayObjectProxyVTable Super;

	public:
		static const Self& Constant();

	public:
		static int addShape( lua_State* L );
		static int updateShape( lua_State* L );
		static int removeShape( lua_State* L );
		static int clearShapes( lua_State* L );
		static int numShapes( lua_State* L );

	protected:
		LuaSDFGroupObjectProxyVTable() {}

	public:
		virtual int ValueForKey( lua_State* L, const MLuaProxyable& object, const char key[], bool overrideRestriction = false ) const;
		virtual bool SetValueForKey( lua_State* L, MLuaProxyable& object, const char key[], int valueIndex ) const;
		virtual const LuaProxyVTable& Parent() const;
};

class LuaLibSDFInstance
{
	public:
		static void Initialize( lua_State* L );
		static void RegisterDisplayFunctions( lua_State* L );
};

int SDFInstance_newSDFGroup( lua_State* L );

} // namespace Rtt

#endif // _Rtt_LuaLibSDFInstance_H__
