#!/usr/bin/env python3
"""Modify iOS ratatouille.xcodeproj to integrate bgfx source files."""
import hashlib

PROJ = "/Users/yee/data/dev/app/labo/corona/platform/iphone/ratatouille.xcodeproj/project.pbxproj"

def gen_uuid(name):
    """Generate a 24-char hex UUID deterministically from name."""
    h = hashlib.md5(("ios_" + name).encode()).hexdigest()[:24].upper()
    return h

with open(PROJ, 'r') as f:
    content = f.read()

# bgfx source files to compile
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

# Generate UUIDs
src_filerefs = {}
src_buildfiles = {}
for s in bgfx_sources:
    src_filerefs[s] = gen_uuid(f"{s}_fileref")
    src_buildfiles[s] = gen_uuid(f"{s}_buildfile_core")

hdr_filerefs = {}
for h in bgfx_headers:
    hdr_filerefs[h] = gen_uuid(f"{h}_fileref")

GRP_BGFX = gen_uuid("bgfx_source_group")

# 1. Add PBXBuildFile entries (source compilation for libplayer-core)
buildfile_entries = []
for s in bgfx_sources:
    buildfile_entries.append(f'\t\t{src_buildfiles[s]} /* {s} in Sources */ = {{isa = PBXBuildFile; fileRef = {src_filerefs[s]} /* {s} */; }};')

buildfile_block = '\n'.join(buildfile_entries)
content = content.replace(
    '/* Begin PBXBuildFile section */\n',
    '/* Begin PBXBuildFile section */\n' + buildfile_block + '\n',
    1
)

# 2. Add PBXFileReference entries
fileref_entries = []
for s in bgfx_sources:
    ftype = "sourcecode.cpp.objcpp" if s.endswith(".mm") else "sourcecode.cpp.cpp"
    fileref_entries.append(f'\t\t{src_filerefs[s]} /* {s} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = {ftype}; name = {s}; path = ../../librtt/Renderer/{s}; sourceTree = "<group>"; }};')
for h in bgfx_headers:
    fileref_entries.append(f'\t\t{hdr_filerefs[h]} /* {h} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.c.h; name = {h}; path = ../../librtt/Renderer/{h}; sourceTree = "<group>"; }};')

fileref_block = '\n'.join(fileref_entries)
content = content.replace(
    '/* Begin PBXFileReference section */\n',
    '/* Begin PBXFileReference section */\n' + fileref_block + '\n',
    1
)

# 3. Add bgfx source files to libplayer-core Sources build phase (A4841091151147BE0074BD57)
src_entries = []
for s in bgfx_sources:
    src_entries.append(f'\t\t\t\t{src_buildfiles[s]} /* {s} in Sources */,')
src_block = '\n'.join(src_entries)

content = content.replace(
    'A4841091151147BE0074BD57 /* Sources */ = {\n\t\t\tisa = PBXSourcesBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n',
    'A4841091151147BE0074BD57 /* Sources */ = {\n\t\t\tisa = PBXSourcesBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n' + src_block + '\n',
    1
)

# 4. Add PBXGroup for bgfx files
group_children = []
for s in bgfx_sources:
    group_children.append(f'\t\t\t\t{src_filerefs[s]} /* {s} */,')
for h in bgfx_headers:
    group_children.append(f'\t\t\t\t{hdr_filerefs[h]} /* {h} */,')

group_block = f"""\t\t{GRP_BGFX} /* bgfx */ = {{
\t\t\tisa = PBXGroup;
\t\t\tchildren = (
{chr(10).join(group_children)}
\t\t\t);
\t\t\tname = bgfx;
\t\t\tsourceTree = "<group>";
\t\t}};"""

content = content.replace(
    '/* Begin PBXGroup section */\n',
    '/* Begin PBXGroup section */\n' + group_block + '\n',
    1
)

# 5. Add bgfx group to the main project group
# Find the root group and add our group reference
content = content.replace(
    '29B97314FDCFA39411CA2CEA /* ratatouille */ = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n',
    '29B97314FDCFA39411CA2CEA /* ratatouille */ = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n\t\t\t\t' + GRP_BGFX + ' /* bgfx */,\n',
    1
)

with open(PROJ, 'w') as f:
    f.write(content)

print("Done! Modified iOS project.pbxproj successfully.")
print(f"Added {len(bgfx_sources)} source files and {len(bgfx_headers)} header files to libplayer-core.")
