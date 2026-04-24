////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_BatchObject_H__
#define _Rtt_BatchObject_H__

#include "Display/Rtt_DisplayObject.h"
#include "Display/Rtt_InstancedBatchRenderer.h"
#include "Core/Rtt_Real.h"
#include "Core/Rtt_Array.h"
#include "Core/Rtt_SharedPtr.h"
#include "Renderer/Rtt_RenderData.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

class Display;
class Geometry;
class Shader;
class TextureAtlas;
class TextureResource;

// ----------------------------------------------------------------------------

// BatchObject: renders many atlas-based sprites in a single draw call.
// Each "slot" references a frame in the atlas and has its own transform.
class BatchObject : public DisplayObject
{
	Rtt_CLASS_NO_COPIES( BatchObject )

	public:
		typedef DisplayObject Super;
		typedef BatchObject Self;

		struct Slot
		{
			S32 frameIndex;     // index into atlas frames array
			Real x, y;          // position
			Real scaleX, scaleY;
			Real rotation;      // degrees
			Real alpha;
			bool isVisible;
			bool isDirty;
			bool isActive;      // false = removed (slot reuse)
		};

	public:
		static BatchObject* New(
			Rtt_Allocator* allocator,
			Display& display,
			TextureAtlas* atlas,
			int initialCapacity
		);

		virtual ~BatchObject();

	public:
		// Slot management
		int AddSlot( int frameIndex, Real x, Real y );
		void RemoveSlot( int slotId );
		Slot* GetSlot( int slotId );
		const Slot* GetSlot( int slotId ) const;
		int GetCount() const { return fActiveCount; }
		void Clear();

		TextureAtlas* GetAtlas() const { return fAtlas; }

	public:
		// MDrawable
		virtual void Draw( Renderer& renderer ) const;
		virtual void GetSelfBounds( Rect& rect ) const;
		virtual void Prepare( const Display& display );
		virtual bool HitTest( Real contentX, Real contentY );

	public:
		virtual const LuaProxyVTable& ProxyVTable() const;
		virtual void RemovedFromParent( lua_State * L, GroupObject * parent );
		bool IsRemoved() const { return fRemoved; }

	private:
		BatchObject( Rtt_Allocator* allocator, Display& display );

		void RebuildVertices() const;
		void FillInstanceData() const;

	private:
		TextureAtlas* fAtlas;
		SharedPtr< TextureResource > fTextureResource; // keep texture alive even if atlas is GC'd
		Array< Slot > fSlots;
		int fActiveCount;
		bool fRemoved;

		// Rendering
		mutable Geometry* fGeometry;
		RenderData fData;
		Shader* fShader;
		mutable bool fVerticesDirty;

		// GPU instancing
		mutable bool fUseInstancing;
		mutable InstanceDrawData fInstanceDrawData;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_BatchObject_H__
