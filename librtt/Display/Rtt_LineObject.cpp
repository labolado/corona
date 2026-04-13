//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Rtt_LineObject.h"

#include "Display/Rtt_Display.h"
#include "Display/Rtt_OpenPath.h"
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )
#include "Display/Rtt_SDFRenderer.h"
#endif
#include "Display/Rtt_Shader.h"
#include "Display/Rtt_ShaderFactory.h"
#include "Renderer/Rtt_Renderer.h"
#include "Rtt_LuaProxyVTable.h"

#include "Rtt_Profiling.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

LineObject*
LineObject::NewLine( Rtt_Allocator* pAllocator, Real xStart, Real yStart, Real xEnd, Real yEnd )
{
	OpenPath* path = Rtt_NEW( pAllocator, OpenPath( pAllocator ) );
	Vertex2 vStart = { xStart, yStart };
	path->Append( vStart );

	Vertex2 vEnd = { xEnd, yEnd };
	path->Append( vEnd );

	return Rtt_NEW( pAllocator, LineObject( path ) );
}

// ----------------------------------------------------------------------------

LineObject::LineObject( OpenPath* path )
:	Super(),
	fStrokeData(),
	fShaderColor( ColorZero() ),
	fPath( path ),
	fStrokeShader( NULL ),
	fAnchorSegments( false )
{
	Rtt_ASSERT( fPath );
	fPath->SetObserver( this );

    SetObjectDesc("LineObject"); // for introspection
}

LineObject::~LineObject()
{
	Rtt_DELETE( fPath );
}

bool
LineObject::UpdateTransform( const Matrix& parentToDstSpace )
{
	bool shouldUpdate = Super::UpdateTransform( parentToDstSpace );

	if ( shouldUpdate )
	{
		fPath->Invalidate( OpenPath::kStroke );
	}

	return shouldUpdate;
}

void
LineObject::Prepare( const Display& display )
{
	Rtt_ASSERT( fPath );

	Super::Prepare( display );

	SUMMED_TIMING( lp, "Line: post-Super::Prepare" );

	if ( ShouldPrepare() )
	{
		// NOTE: We need to update paint *prior* to geometry
		// b/c in the case of image sheets, the paint needs to be updated
		// in order for the texture coordinates to be updated.
		if ( ! IsValid( kPaintFlag ) )
		{
			fPath->GetStroke()->UpdatePaint( fStrokeData );
			SetValid( kPaintFlag );
		}

		if ( ! IsValid( kGeometryFlag ) )
		{
			const Matrix& xform = GetSrcToDstMatrix();
			fPath->Update( fStrokeData, xform );
			SetValid( kGeometryFlag );
		}

		if ( ! IsValid( kColorFlag ) )
		{
			fPath->GetStroke()->UpdateColor( fStrokeData, AlphaCumulative() );
			SetValid( kColorFlag );
		}

		if ( ! IsValid( kProgramDataFlag ) )
		{
			SetValid( kProgramDataFlag );
		}

		// Program
		if ( ! IsValid( kProgramFlag ) )
		{
			Rect bounds;
			fPath->GetSelfBounds( bounds );
			int w = Rtt_RealToInt( bounds.Width() );
			int h = Rtt_RealToInt( bounds.Height() );

			ShaderFactory& factory = display.GetShaderFactory();

			Paint *stroke = fPath->GetStroke();
			if ( stroke )
			{
				Shader *shader = stroke->GetShader(factory);
				shader->Prepare( fStrokeData, w, h, ShaderResource::kDefault );
				fStrokeShader = shader;
			}

			SetValid( kProgramFlag );
		}
	}
}

/*
void
LineObject::Translate( Real dx, Real dy )
{
	Rtt_ASSERT( fPath );

	Super::Translate( dx, dy );

	if ( IsValid() && IsNotHidden() )
	{
//		if ( ! IsProperty( kIsTransformLocked ) )
		{
			fPath->Translate( dx, dy );
		}
	}
}
*/

void
LineObject::Draw( Renderer& renderer ) const
{
	if ( ShouldDraw() )
	{
		SUMMED_TIMING( ld, "Line: Draw" );

		Rtt_ASSERT( fPath );

		fPath->UpdateResources( renderer );

		if ( fPath->HasStroke() && fPath->IsStrokeVisible() )
		{
#if !defined( Rtt_EMSCRIPTEN_ENV ) && !defined( Rtt_TVOS_ENV )
			SDFRenderer& sdf = SDFRenderer::Instance();

			// SDF path for simple 2-point lines
			if ( sdf.IsAvailable() && fPath->NumVertices() == 2 )
			{
				ArrayVertex2& pts = const_cast< OpenPath* >( fPath )->GetVertices();
				Vertex2 p0 = pts[0];
				Vertex2 p1 = pts[1];

				Real lineW = fPath->GetWidth();
				Real pad = lineW + Rtt_IntToReal( 2 );
				Real minX = ( p0.x < p1.x ? p0.x : p1.x ) - pad;
				Real minY = ( p0.y < p1.y ? p0.y : p1.y ) - pad;
				Real maxX = ( p0.x > p1.x ? p0.x : p1.x ) + pad;
				Real maxY = ( p0.y > p1.y ? p0.y : p1.y ) + pad;
				Real bboxW = maxX - minX;
				Real bboxH = maxY - minY;
				Real cx = (minX + maxX) * Rtt_REAL_HALF;
				Real cy = (minY + maxY) * Rtt_REAL_HALF;

				Real halfW = bboxW * Rtt_REAL_HALF;
				Real halfH = bboxH * Rtt_REAL_HALF;
				Real nx0 = (p0.x - cx) / halfW;
				Real ny0 = (p0.y - cy) / halfH;
				Real nx1 = (p1.x - cx) / halfW;
				Real ny1 = (p1.y - cy) / halfH;

				sdf.SetShapeUniforms( SDFRenderer::kLine, bboxW, bboxH, Rtt_REAL_0, lineW );
				sdf.SetLineUniforms( nx0, ny0, nx1, ny1 );
				sdf.SetColorUniforms(
					Rtt_REAL_1, Rtt_REAL_1, Rtt_REAL_1, Rtt_REAL_1,
					Rtt_REAL_0, Rtt_REAL_0, Rtt_REAL_0, Rtt_REAL_0 );
			}
#endif // !Rtt_EMSCRIPTEN_ENV && !Rtt_TVOS_ENV

			fStrokeShader->Draw( renderer, fStrokeData );
		}
	}
}

void
LineObject::GetSelfBounds( Rect& rect ) const
{
	fPath->GetSelfBounds( rect );
}

const LuaProxyVTable&
LineObject::ProxyVTable() const
{
	return LuaLineObjectProxyVTable::Constant();
}

void
LineObject::SetStroke( Paint* newValue )
{
	DirtyFlags flags = ( kPaintFlag | kProgramFlag );
	if ( Paint::ShouldInvalidateColor( fPath->GetStroke(), newValue ) )
	{
		flags |= kColorFlag;
	}
	if ( newValue && NULL == fPath->GetStroke() )
	{
		// When paint goes from NULL to non-NULL,
		// ensure geometry is prepared
		flags |= kGeometryFlag;
	}
	Invalidate( flags );

	fPath->SetStroke( newValue );

	DidChangePaint( fStrokeData );
}

void
LineObject::SetStrokeColor( Color newValue )
{
	Paint *paint = GetPath().GetStroke();
	if ( paint )
	{
		paint->SetColor( newValue );
		Invalidate( kGeometryFlag | kColorFlag );
	}	
}

void
LineObject::SetStrokeWidth( Real newValue )
{
	if ( GetStrokeWidth() != newValue )
	{
		fPath->SetWidth( newValue );
		Invalidate( kGeometryFlag | kStageBoundsFlag );
	}
}

void
LineObject::Append( const Vertex2& p )
{
	const Matrix& m = GetMatrix();
	Vertex2 v = { p.x - m.Tx(), p.y - m.Ty() };
	fPath->Append( v );
	Invalidate( kGeometryFlag | kStageBoundsFlag );
}
    
void
LineObject::SetBlend( RenderTypes::BlendType newValue )
{
    Paint *paint = fPath->GetStroke();
    paint->SetBlend( newValue );
}

RenderTypes::BlendType
LineObject::GetBlend() const
{
	const Paint *paint = fPath->GetStroke();
	return paint->GetBlend();
}

void
LineObject::SetAnchorSegments( bool should_anchor )
{
	if( fAnchorSegments != should_anchor )
	{
		Invalidate( kGeometryFlag | kTransformFlag | kMaskFlag | kStageBoundsFlag );
	}

	fAnchorSegments = should_anchor;
}

bool
LineObject::ShouldOffsetWithAnchor() const
{
	if( IsV1Compatibility() )
	{
		return Super::ShouldOffsetWithAnchor();
	}
	else
	{
		return fAnchorSegments;
	}
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------
