#!/usr/bin/env python3
"""Revert bgfx source file additions from iOS ratatouille.xcodeproj (they're already in libcorona)."""
import hashlib

PROJ = "/Users/yee/data/dev/app/labo/corona/platform/iphone/ratatouille.xcodeproj/project.pbxproj"

def gen_uuid(name):
    h = hashlib.md5(("ios_" + name).encode()).hexdigest()[:24].upper()
    return h

with open(PROJ, 'r') as f:
    lines = f.readlines()

bgfx_sources = [
    "Rtt_BgfxCommandBuffer.cpp",
    "Rtt_BgfxExports.cpp",
    "Rtt_BgfxFrameBufferObject.cpp",
    "Rtt_BgfxGeometry.cpp",
    "Rtt_BgfxProgram.cpp",
    "Rtt_BgfxShaderCompiler.cpp",
    "Rtt_BgfxRenderer.cpp",
    "Rtt_BgfxTexture.cpp",
    "Rtt_BgfxMetalReadback.mm",
]
bgfx_headers = [
    "Rtt_BgfxCommandBuffer.h",
    "Rtt_BgfxExports.h",
    "Rtt_BgfxFrameBufferObject.h",
    "Rtt_BgfxGeometry.h",
    "Rtt_BgfxProgram.h",
    "Rtt_BgfxShaderCompiler.h",
    "Rtt_BgfxRenderer.h",
    "Rtt_BgfxTexture.h",
    "Rtt_BgfxMetalReadback.h",
]

# Generate all UUIDs that were added
uuids_to_remove = set()
for s in bgfx_sources:
    uuids_to_remove.add(gen_uuid(f"{s}_fileref"))
    uuids_to_remove.add(gen_uuid(f"{s}_buildfile_core"))
for h in bgfx_headers:
    uuids_to_remove.add(gen_uuid(f"{h}_fileref"))
GRP_BGFX = gen_uuid("bgfx_source_group")
uuids_to_remove.add(GRP_BGFX)

# Remove lines containing any of these UUIDs
filtered = []
in_group = False
for line in lines:
    # Check if any UUID is in this line
    has_uuid = any(uuid in line for uuid in uuids_to_remove)
    if has_uuid:
        continue
    filtered.append(line)

with open(PROJ, 'w') as f:
    f.writelines(filtered)

print(f"Removed {len(lines) - len(filtered)} lines containing bgfx source UUIDs.")
