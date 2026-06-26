#pragma once

#include <bgfx/bgfx.h>

#include "Core/Rtt_Types.h"

namespace Rtt {

struct SDFInstanceDrawData {
	bgfx::InstanceDataBuffer instanceBuffer;
	bgfx::ProgramHandle      programHandle;
	bgfx::VertexBufferHandle baseQuadVB;
	bgfx::IndexBufferHandle  baseQuadIB;
	U32                      instanceCount;
};

// 5 x vec4 = 80 bytes per-instance layout
static const U32 kSDFInstanceStride = 80;

} // namespace Rtt
