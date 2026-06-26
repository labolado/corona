////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// Author: Labo Lado, laboladoapp@gmail.com
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_SDFGroupObject_H__
#define _Rtt_SDFGroupObject_H__

#include "Core/Rtt_Array.h"
#include "Core/Rtt_Real.h"
#include "Display/Rtt_DisplayObject.h"
#include "Display/Rtt_SDFInstanceData.h"
#include "Renderer/Rtt_RenderData.h"

namespace Rtt
{

class Display;
class Geometry;
class Renderer;
class Shader;

struct SDFInstanceSlot
{
	float x, y, w, h;
	float rotation;
	U8 shapeId;
	U8 nVerts;
	float r, g, b, a;
	bool active;
};

class SDFGroupObject : public DisplayObject
{
	Rtt_CLASS_NO_COPIES( SDFGroupObject )

	public:
		typedef DisplayObject Super;
		typedef SDFGroupObject Self;

	public:
		static SDFGroupObject* New( Rtt_Allocator* allocator, Display& display, int capacity );
		virtual ~SDFGroupObject();

	public:
		int AddShape( const SDFInstanceSlot& params );
		bool UpdateShape( int slotId, const SDFInstanceSlot& params, U32 fieldMask );
		bool RemoveShape( int slotId );
		void ClearShapes();
		int GetShapeCount() const { return fActiveCount; }
		int GetCapacity() const { return fSlots.Length(); }

	public:
		virtual void Draw( Renderer& renderer ) const;
		virtual void GetSelfBounds( Rect& rect ) const;
		virtual void Prepare( const Display& display );
		virtual bool HitTest( Real contentX, Real contentY );
		virtual const LuaProxyVTable& ProxyVTable() const;

	private:
		SDFGroupObject( Rtt_Allocator* allocator, Display& display );

		void FillInstanceData() const;
		void EnsurePlaceholderGeometry() const;
		static U8 ShapeVertexCount( U8 shapeId );

	private:
		Array< SDFInstanceSlot > fSlots;
		int fActiveCount;

		mutable Geometry* fGeometry;
		RenderData fData;
		Shader* fShader;
		mutable SDFInstanceDrawData fDrawData;
		mutable bool fUseInstancing;
};

} // namespace Rtt

#endif // _Rtt_SDFGroupObject_H__
