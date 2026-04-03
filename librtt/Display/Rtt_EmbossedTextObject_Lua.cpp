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
#include "Rtt_FilePath.h"
#include "Display/Rtt_BitmapMask.h"
#include "Display/Rtt_BitmapPaint.h"
#include "Display/Rtt_ClosedPath.h"
#include "Display/Rtt_ContainerObject.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_DisplayDefaults.h"
#include "Display/Rtt_DisplayObject.h"
#include "Display/Rtt_EmbossedTextObject.h"
#include "Display/Rtt_GradientPaint.h"
#include "Display/Rtt_LineObject.h"
#include "Display/Rtt_LuaLibDisplay.h"
#include "Display/Rtt_Paint.h"
#include "Display/Rtt_RectPath.h"
#include "Display/Rtt_Shader.h"
#include "Display/Rtt_ShaderFactory.h"
#include "Display/Rtt_ShapeObject.h"
#include "Display/Rtt_SnapshotObject.h"
#include "Display/Rtt_SpriteObject.h"
#include "Display/Rtt_StageObject.h"
#include "Display/Rtt_TextObject.h"
#include "Display/Rtt_TextureFactory.h"
#include "Rtt_LuaContext.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_MPlatformDevice.h"
#include "Rtt_PlatformDisplayObject.h"
#include "Rtt_Runtime.h"
#include "Rtt_PhysicsWorld.h"
#include "CoronaLua.h"
#include "Rtt_ParticleSystemObject.h"
#include "Display/Rtt_EmitterObject.h"
#include "Core/Rtt_StringHash.h"
#include "Renderer/Rtt_Geometry_Renderer.h"
#include "Rtt_Profiling.h"
#include <string.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

const LuaEmbossedTextObjectProxyVTable&
LuaEmbossedTextObjectProxyVTable::Constant()
{
    static const Self kVTable;
    return kVTable;
}

int
LuaEmbossedTextObjectProxyVTable::ValueForKey( lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction /* = false */ ) const
{
    if ( ! key ) { return 0; }
    
    int result = 1;

    static const char * keys[] =
    {
        "setText",            // 0
        "setSize",            // 1
        "setEmbossColor",    // 2
        "setTextColor",        // 3
        "setFillColor",        // 4
    };
    static const int numKeys = sizeof( keys ) / sizeof( const char * );
    static StringHash sHash( *LuaContext::GetAllocator( L ), keys, numKeys, 5, 4, 9, __FILE__, __LINE__ );
    StringHash *hash = &sHash;

    EmbossedTextObject& textObject = (EmbossedTextObject&)const_cast<MLuaProxyable&>(object);

    int index = hash->Lookup( key );
    switch ( index )
    {
        case 0:
            lua_pushcfunction( L, OnSetText );
            break;
        case 1:
            lua_pushcfunction( L, OnSetSize );
            break;
        case 2:
            lua_pushcfunction( L, OnSetEmbossColor );
            break;
        case 3:            // setTextColor
        case 4:            // setFillColor
            // Resetting the foreground text color will revert the embossed colors back to their defaults.
            textObject.UseDefaultHighlightColor();
            textObject.UseDefaultShadowColor();
            result = Super::ValueForKey( L, object, key, overrideRestriction );
            break;
        default:
            result = Super::ValueForKey( L, object, key, overrideRestriction );
            break;
    }

    // If we retrieved the "_properties" key from the super, merge it with the local properties
    if ( result == 1 && strcmp( key, "_properties" ) == 0 )
    {
        String embossedTextProperties(LuaContext::GetRuntime( L )->Allocator());

        DumpObjectProperties( L, object, keys, numKeys, embossedTextProperties );

        // Currently embossedTextProperties is empty but this might change in the future, make the JSON right
        const char *comma = "";
        if (embossedTextProperties.IsEmpty())
        {
            comma = "";
        }
        else
        {
            comma = ", ";
        }

        lua_pushfstring( L, "{ %s%s%s }", embossedTextProperties.GetString(), comma, lua_tostring( L, -1 ) );
        lua_remove( L, -2 ); // pop super properties
        result = 1;
    }

    return result;
}

bool
LuaEmbossedTextObjectProxyVTable::SetValueForKey( lua_State *L, MLuaProxyable& object, const char key[], int valueIndex ) const
{
    return Super::SetValueForKey( L, object, key, valueIndex );
}

int
LuaEmbossedTextObjectProxyVTable::OnSetText( lua_State *L )
{
    TextObject *textObjectPointer = (TextObject*)LuaProxy::GetProxyableObject( L, 1 );
    if (textObjectPointer)
    {
        textObjectPointer->SetText( lua_tostring( L, 2 ) );
    }
    return 0;
}

int
LuaEmbossedTextObjectProxyVTable::OnSetSize( lua_State *L )
{
    TextObject *textObjectPointer = (TextObject*)LuaProxy::GetProxyableObject( L, 1 );
    if (textObjectPointer)
    {
        Runtime& runtime = * LuaContext::GetRuntime( L );
        const Display& display = runtime.GetDisplay();
        Real fontSize = Rtt_RealDiv( luaL_toreal( L, 2 ), display.GetSx() );
        textObjectPointer->SetSize( fontSize );
    }
    return 0;
}

static U8
GetEmbossedColorValueFromField(
    lua_State *L, int tableIndex, const char *fieldName, U8 defaultValue, bool isByteValue)
{
    U8 value = defaultValue;
    if (L && tableIndex && fieldName)
    {
        lua_getfield(L, tableIndex, fieldName);
        if (lua_type(L, -1) == LUA_TNUMBER)
        {
            if (isByteValue)
            {

                value = (U8)Clamp((int)lua_tointeger(L, -1), 0, 255);
            }
            else
            {
                double decimalValue = Clamp(lua_tonumber(L, -1), 0.0, 1.0) * 255.0;
                value = (U8)(decimalValue + 0.5);
            }
        }
        lua_pop(L, 1);
    }
    return value;
}

int
LuaEmbossedTextObjectProxyVTable::OnSetEmbossColor( lua_State *L )
{
    // Validate.
    if (NULL == L)
    {
        return 0;
    }
    
    // Fetch the text object.
    EmbossedTextObject *textObjectPointer = (EmbossedTextObject*)LuaProxy::GetProxyableObject(L, 1);
    if (NULL == textObjectPointer)
    {
        return 0;
    }

    // Default the highlight and shadow colors to white.
    // Note: It doesn't make sense to make the shadow color default to white, but that was the old behavior.
    RGBA highlightColor = { 255, 255, 255, 255 };
    RGBA shadowColor = highlightColor;
    
    // Fetch the emboss colors from the table argument.
    if (lua_istable(L, 2))
    {
        // Fetch the highlight colors.
        lua_getfield(L, 2, "highlight");
        if (lua_istable(L, -1))
        {
            highlightColor.r = GetEmbossedColorValueFromField(
                                    L, -1, "r", highlightColor.r, textObjectPointer->IsByteColorRange());
            highlightColor.g = GetEmbossedColorValueFromField(
                                    L, -1, "g", highlightColor.g, textObjectPointer->IsByteColorRange());
            highlightColor.b = GetEmbossedColorValueFromField(
                                    L, -1, "b", highlightColor.b, textObjectPointer->IsByteColorRange());
            highlightColor.a = GetEmbossedColorValueFromField(
                                    L, -1, "a", highlightColor.a, textObjectPointer->IsByteColorRange());
        }
        lua_pop(L, 1);

        // Fetch the shadow colors.
        lua_getfield(L, 2, "shadow");
        if (lua_istable(L, -1))
        {
            shadowColor.r = GetEmbossedColorValueFromField(
                                    L, -1, "r", shadowColor.r, textObjectPointer->IsByteColorRange());
            shadowColor.g = GetEmbossedColorValueFromField(
                                    L, -1, "g", shadowColor.g, textObjectPointer->IsByteColorRange());
            shadowColor.b = GetEmbossedColorValueFromField(
                                    L, -1, "b", shadowColor.b, textObjectPointer->IsByteColorRange());
            shadowColor.a = GetEmbossedColorValueFromField(
                                    L, -1, "a", shadowColor.a, textObjectPointer->IsByteColorRange());
        }
        lua_pop(L, 1);
    }
    
    // Apply the emboss colors to the text object.
    textObjectPointer->SetHighlightColor(highlightColor);
    textObjectPointer->SetShadowColor(shadowColor);
    return 0;
}

const LuaProxyVTable&
LuaEmbossedTextObjectProxyVTable::Parent() const
{
    return Super::Constant();
}


} // namespace Rtt

// ----------------------------------------------------------------------------
