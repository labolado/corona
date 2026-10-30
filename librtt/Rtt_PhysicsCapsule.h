//////////////////////////////////////////////////////////////////////////////
// Shared Lua-adapter capsule creation. See the repository LICENSE.
//////////////////////////////////////////////////////////////////////////////
#ifndef _Rtt_PhysicsCapsule_H__
#define _Rtt_PhysicsCapsule_H__

#include "box2d/box2d.h"
#include "box2d/constants.h"
#include <cmath>

namespace Rtt
{
namespace PhysicsCapsule
{
// Apply this rule after each adapter's existing pixel-to-meter conversion.
// Box2D rejects short capsules; a midpoint disk is an explicit approximation,
// not a shape with identical area to a nonzero-length capsule.
inline b2ShapeId Create( b2BodyId body, const b2ShapeDef *definition, const b2Capsule &capsule )
{
    if ( !b2IsValidVec2( capsule.center1 ) || !b2IsValidVec2( capsule.center2 ) ||
         !std::isfinite( capsule.radius ) || capsule.radius <= 0 )
        return b2_nullShapeId;

    float lengthSquared = b2DistanceSquared( capsule.center1, capsule.center2 );
    if ( !std::isfinite( lengthSquared ) )
        return b2_nullShapeId;

    if ( lengthSquared <= B2_LINEAR_SLOP * B2_LINEAR_SLOP )
    {
        b2Circle circle = { b2Add( b2MulSV( 0.5f, capsule.center1 ),
                                 b2MulSV( 0.5f, capsule.center2 ) ), capsule.radius };
        return b2CreateCircleShape( body, definition, &circle );
    }
    return b2CreateCapsuleShape( body, definition, &capsule );
}
}
}

#endif // _Rtt_PhysicsCapsule_H__
