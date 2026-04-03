#!/usr/bin/env python3
"""Modify ratatouille.xcodeproj to integrate bgfx."""
import re
import hashlib
import time

PROJ = "/Users/yee/data/dev/app/labo/corona/platform/mac/ratatouille.xcodeproj/project.pbxproj"

def gen_uuid(name):
    """Generate a 24-char hex UUID deterministically from name."""
    h = hashlib.md5(name.encode()).hexdigest()[:24].upper()
    return h

with open(PROJ, 'r') as f:
    content = f.read()

# --- Define UUIDs for new entries ---
# File references for bgfx static libraries
FR_BGFX   = gen_uuid("libbgfxRelease.a_fileref")
FR_BX     = gen_uuid("libbxRelease.a_fileref")
FR_BIMG   = gen_uuid("libbimgRelease.a_fileref")
FR_BIMG_D = gen_uuid("libbimg_decodeRelease.a_fileref")
FR_METAL  = gen_uuid("Metal.framework_fileref")

# File references for bgfx source files
bgfx_sources = [
    "Rtt_BgfxCommandBuffer.cpp",
    "Rtt_BgfxExports.cpp",
    "Rtt_BgfxFrameBufferObject.cpp",
    "Rtt_BgfxGeometry.cpp",
    "Rtt_BgfxProgram.cpp",
    "Rtt_BgfxRenderer.cpp",
    "Rtt_BgfxTexture.cpp",
]
bgfx_headers = [
    "Rtt_BgfxCommandBuffer.h",
    "Rtt_BgfxExports.h",
    "Rtt_BgfxFrameBufferObject.h",
    "Rtt_BgfxGeometry.h",
    "Rtt_BgfxProgram.h",
    "Rtt_BgfxRenderer.h",
    "Rtt_BgfxTexture.h",
]

src_filerefs = {}
src_buildfiles = {}
for s in bgfx_sources:
    src_filerefs[s] = gen_uuid(f"{s}_fileref")
    src_buildfiles[s] = gen_uuid(f"{s}_buildfile")

hdr_filerefs = {}
for h in bgfx_headers:
    hdr_filerefs[h] = gen_uuid(f"{h}_fileref")

# Build file UUIDs for linking
BF_BGFX   = gen_uuid("libbgfxRelease.a_buildfile_rtt")
BF_BX     = gen_uuid("libbxRelease.a_buildfile_rtt")
BF_BIMG   = gen_uuid("libbimgRelease.a_buildfile_rtt")
BF_BIMG_D = gen_uuid("libbimg_decodeRelease.a_buildfile_rtt")

BF_BGFX_P   = gen_uuid("libbgfxRelease.a_buildfile_player")
BF_BX_P     = gen_uuid("libbxRelease.a_buildfile_player")
BF_BIMG_P   = gen_uuid("libbimgRelease.a_buildfile_player")
BF_BIMG_D_P = gen_uuid("libbimg_decodeRelease.a_buildfile_player")
BF_METAL_P  = gen_uuid("Metal.framework_buildfile_player")

# Group UUID for bgfx files
GRP_BGFX = gen_uuid("bgfx_group")

# 1. Add PBXBuildFile entries
buildfile_entries = []
# Linking entries for librtt
buildfile_entries.append(f'\t\t{BF_BGFX} /* libbgfxRelease.a in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_BGFX} /* libbgfxRelease.a */; }};')
buildfile_entries.append(f'\t\t{BF_BX} /* libbxRelease.a in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_BX} /* libbxRelease.a */; }};')
buildfile_entries.append(f'\t\t{BF_BIMG} /* libbimgRelease.a in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_BIMG} /* libbimgRelease.a */; }};')
buildfile_entries.append(f'\t\t{BF_BIMG_D} /* libbimg_decodeRelease.a in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_BIMG_D} /* libbimg_decodeRelease.a */; }};')
# Linking entries for rttplayer
buildfile_entries.append(f'\t\t{BF_BGFX_P} /* libbgfxRelease.a in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_BGFX} /* libbgfxRelease.a */; }};')
buildfile_entries.append(f'\t\t{BF_BX_P} /* libbxRelease.a in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_BX} /* libbxRelease.a */; }};')
buildfile_entries.append(f'\t\t{BF_BIMG_P} /* libbimgRelease.a in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_BIMG} /* libbimgRelease.a */; }};')
buildfile_entries.append(f'\t\t{BF_BIMG_D_P} /* libbimg_decodeRelease.a in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_BIMG_D} /* libbimg_decodeRelease.a */; }};')
buildfile_entries.append(f'\t\t{BF_METAL_P} /* Metal.framework in Frameworks */ = {{isa = PBXBuildFile; fileRef = {FR_METAL} /* Metal.framework */; }};')
# Source compilation entries for librtt
for s in bgfx_sources:
    buildfile_entries.append(f'\t\t{src_buildfiles[s]} /* {s} in Sources */ = {{isa = PBXBuildFile; fileRef = {src_filerefs[s]} /* {s} */; }};')

buildfile_block = '\n'.join(buildfile_entries)

# Insert after "/* Begin PBXBuildFile section */"
content = content.replace(
    '/* Begin PBXBuildFile section */\n',
    '/* Begin PBXBuildFile section */\n' + buildfile_block + '\n',
    1
)

# 2. Add PBXFileReference entries
fileref_entries = []
fileref_entries.append(f'\t\t{FR_BGFX} /* libbgfxRelease.a */ = {{isa = PBXFileReference; lastKnownFileType = archive.ar; name = libbgfxRelease.a; path = "../../external/bgfx/.build/projects/xcode15/libbgfxRelease.a"; sourceTree = "<group>"; }};')
fileref_entries.append(f'\t\t{FR_BX} /* libbxRelease.a */ = {{isa = PBXFileReference; lastKnownFileType = archive.ar; name = libbxRelease.a; path = "../../external/bgfx/.build/projects/xcode15/libbxRelease.a"; sourceTree = "<group>"; }};')
fileref_entries.append(f'\t\t{FR_BIMG} /* libbimgRelease.a */ = {{isa = PBXFileReference; lastKnownFileType = archive.ar; name = libbimgRelease.a; path = "../../external/bgfx/.build/projects/xcode15/libbimgRelease.a"; sourceTree = "<group>"; }};')
fileref_entries.append(f'\t\t{FR_BIMG_D} /* libbimg_decodeRelease.a */ = {{isa = PBXFileReference; lastKnownFileType = archive.ar; name = libbimg_decodeRelease.a; path = "../../external/bgfx/.build/projects/xcode15/libbimg_decodeRelease.a"; sourceTree = "<group>"; }};')
fileref_entries.append(f'\t\t{FR_METAL} /* Metal.framework */ = {{isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = Metal.framework; path = System/Library/Frameworks/Metal.framework; sourceTree = SDKROOT; }};')
for s in bgfx_sources:
    fileref_entries.append(f'\t\t{src_filerefs[s]} /* {s} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.cpp.cpp; name = {s}; path = "../../librtt/Renderer/{s}"; sourceTree = "<group>"; }};')
for h in bgfx_headers:
    fileref_entries.append(f'\t\t{hdr_filerefs[h]} /* {h} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.c.h; name = {h}; path = "../../librtt/Renderer/{h}"; sourceTree = "<group>"; }};')

fileref_block = '\n'.join(fileref_entries)
content = content.replace(
    '/* Begin PBXFileReference section */\n',
    '/* Begin PBXFileReference section */\n' + fileref_block + '\n',
    1
)

# 3. Add bgfx source files to librtt Sources build phase (00B733F012B6F6740057F594)
src_entries = []
for s in bgfx_sources:
    src_entries.append(f'\t\t\t\t{src_buildfiles[s]} /* {s} in Sources */,')
src_block = '\n'.join(src_entries)

# Find librtt sources phase and add before the closing );
# Pattern: 00B733F012B6F6740057F594 /* Sources */ = { ... files = ( ... ); }
# Add at the end of the files list
content = content.replace(
    '00B733F012B6F6740057F594 /* Sources */ = {\n\t\t\tisa = PBXSourcesBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n',
    '00B733F012B6F6740057F594 /* Sources */ = {\n\t\t\tisa = PBXSourcesBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n' + src_block + '\n',
    1
)

# 4. Add bgfx libs to librtt Frameworks build phase
lib_entries_rtt = f"""\t\t\t\t{BF_BGFX} /* libbgfxRelease.a in Frameworks */,
\t\t\t\t{BF_BX} /* libbxRelease.a in Frameworks */,
\t\t\t\t{BF_BIMG} /* libbimgRelease.a in Frameworks */,
\t\t\t\t{BF_BIMG_D} /* libbimg_decodeRelease.a in Frameworks */,"""

content = content.replace(
    '00B733F112B6F6740057F594 /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n',
    '00B733F112B6F6740057F594 /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n' + lib_entries_rtt + '\n',
    1
)

# 5. Add bgfx libs + Metal.framework to rttplayer Frameworks build phase
lib_entries_player = f"""\t\t\t\t{BF_BGFX_P} /* libbgfxRelease.a in Frameworks */,
\t\t\t\t{BF_BX_P} /* libbxRelease.a in Frameworks */,
\t\t\t\t{BF_BIMG_P} /* libbimgRelease.a in Frameworks */,
\t\t\t\t{BF_BIMG_D_P} /* libbimg_decodeRelease.a in Frameworks */,
\t\t\t\t{BF_METAL_P} /* Metal.framework in Frameworks */,"""

content = content.replace(
    '8D11072E0486CEB800E47090 /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n',
    '8D11072E0486CEB800E47090 /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n' + lib_entries_player + '\n',
    1
)

# 6. Add PBXGroup for bgfx files - insert into existing groups
# Add a bgfx group
group_children = []
for s in bgfx_sources:
    group_children.append(f'\t\t\t\t{src_filerefs[s]} /* {s} */,')
for h in bgfx_headers:
    group_children.append(f'\t\t\t\t{hdr_filerefs[h]} /* {h} */,')
group_children.append(f'\t\t\t\t{FR_BGFX} /* libbgfxRelease.a */,')
group_children.append(f'\t\t\t\t{FR_BX} /* libbxRelease.a */,')
group_children.append(f'\t\t\t\t{FR_BIMG} /* libbimgRelease.a */,')
group_children.append(f'\t\t\t\t{FR_BIMG_D} /* libbimg_decodeRelease.a */,')
group_children.append(f'\t\t\t\t{FR_METAL} /* Metal.framework */,')

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

# Add bgfx group reference to the main project group (29B97314)
# Find "29B97314" group's children list and add our group
content = content.replace(
    '29B97314FDCFA39411CA2CEA /* ratatouille */ = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n',
    '29B97314FDCFA39411CA2CEA /* ratatouille */ = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n\t\t\t\t' + GRP_BGFX + ' /* bgfx */,\n',
    1
)

# 7. Modify project-level build configurations to add bgfx headers and Rtt_BGFX macro

# Project-level Debug (C01FCF4F08A954540054247B)
# Add to HEADER_SEARCH_PATHS
content = content.replace(
    'C01FCF4F08A954540054247B /* Debug */ = {\n\t\t\tisa = XCBuildConfiguration;\n\t\t\tbuildSettings = {\n\t\t\t\tALWAYS_SEARCH_USER_PATHS = YES;\n\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = "c++0x";\n\t\t\t\tCLANG_CXX_LIBRARY = "libc++";\n',
    'C01FCF4F08A954540054247B /* Debug */ = {\n\t\t\tisa = XCBuildConfiguration;\n\t\t\tbuildSettings = {\n\t\t\t\tALWAYS_SEARCH_USER_PATHS = YES;\n\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = "c++17";\n\t\t\t\tCLANG_CXX_LIBRARY = "libc++";\n',
    1
)

# Add Rtt_BGFX to project Debug preprocessor defs
content = content.replace(
    'GCC_PREPROCESSOR_DEFINITIONS = (\n\t\t\t\t\tRtt_MAC_ENV,\n\t\t\t\t\tRtt_DEBUG,\n\t\t\t\t\tRtt_LUA_COMPILER,\n',
    'GCC_PREPROCESSOR_DEFINITIONS = (\n\t\t\t\t\tRtt_MAC_ENV,\n\t\t\t\t\tRtt_DEBUG,\n\t\t\t\t\tRtt_LUA_COMPILER,\n\t\t\t\t\tRtt_BGFX,\n',
    1
)

# Add bgfx header search paths to project Debug
content = content.replace(
    'HEADER_SEARCH_PATHS = (\n\t\t\t\t\t"../../external/b2Separator-cpp",\n\t\t\t\t\t../../external/Box2D,\n\t\t\t\t\t../../external/smoothpolygon,\n\t\t\t\t\t"../../external/tiny-aes128-c",\n\t\t\t\t\t/System/Library/Frameworks/OpenAL.framework/Headers,\n\t\t\t\t\t"$(SRCROOT)/../../external/SDL_sound.framework/Headers",\n\t\t\t\t\t"$(SRCROOT)/../../external/SDL.framework/Headers",\n\t\t\t\t);',
    'HEADER_SEARCH_PATHS = (\n\t\t\t\t\t"../../external/b2Separator-cpp",\n\t\t\t\t\t../../external/Box2D,\n\t\t\t\t\t../../external/smoothpolygon,\n\t\t\t\t\t"../../external/tiny-aes128-c",\n\t\t\t\t\t/System/Library/Frameworks/OpenAL.framework/Headers,\n\t\t\t\t\t"$(SRCROOT)/../../external/SDL_sound.framework/Headers",\n\t\t\t\t\t"$(SRCROOT)/../../external/SDL.framework/Headers",\n\t\t\t\t\t"../../external/bgfx/include",\n\t\t\t\t\t"../../external/bx/include",\n\t\t\t\t\t"../../external/bimg/include",\n\t\t\t\t);',
    1
)

# Project-level Release (C01FCF5008A954540054247B)
content = content.replace(
    'C01FCF5008A954540054247B /* Release */ = {\n\t\t\tisa = XCBuildConfiguration;\n\t\t\tbuildSettings = {\n\t\t\t\tALWAYS_SEARCH_USER_PATHS = YES;\n\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = "c++0x";\n\t\t\t\tCLANG_CXX_LIBRARY = "libc++";\n',
    'C01FCF5008A954540054247B /* Release */ = {\n\t\t\tisa = XCBuildConfiguration;\n\t\t\tbuildSettings = {\n\t\t\t\tALWAYS_SEARCH_USER_PATHS = YES;\n\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = "c++17";\n\t\t\t\tCLANG_CXX_LIBRARY = "libc++";\n',
    1
)

# Add Rtt_BGFX to project Release preprocessor defs
content = content.replace(
    'GCC_PREPROCESSOR_DEFINITIONS = (\n\t\t\t\t\tRtt_MAC_ENV,\n\t\t\t\t\tRtt_LUA_COMPILER,\n\t\t\t\t\tLUA_USE_MODERN_MACOSX,\n',
    'GCC_PREPROCESSOR_DEFINITIONS = (\n\t\t\t\t\tRtt_MAC_ENV,\n\t\t\t\t\tRtt_LUA_COMPILER,\n\t\t\t\t\tRtt_BGFX,\n\t\t\t\t\tLUA_USE_MODERN_MACOSX,\n',
    1
)

# Add bgfx header search paths to project Release
content = content.replace(
    'HEADER_SEARCH_PATHS = (\n\t\t\t\t\t"../../external/b2Separator-cpp",\n\t\t\t\t\t../../external/Box2D,\n\t\t\t\t\t../../external/smoothpolygon,\n\t\t\t\t\t"../../external/tiny-aes128-c",\n\t\t\t\t\t/System/Library/Frameworks/OpenAL.framework/Headers,\n\t\t\t\t\t"$(SRCROOT)/../../external/SDL_sound.framework/Headers",\n\t\t\t\t\t"$(SRCROOT)/../../external/SDL.framework/Headers",\n\t\t\t\t);',
    'HEADER_SEARCH_PATHS = (\n\t\t\t\t\t"../../external/b2Separator-cpp",\n\t\t\t\t\t../../external/Box2D,\n\t\t\t\t\t../../external/smoothpolygon,\n\t\t\t\t\t"../../external/tiny-aes128-c",\n\t\t\t\t\t/System/Library/Frameworks/OpenAL.framework/Headers,\n\t\t\t\t\t"$(SRCROOT)/../../external/SDL_sound.framework/Headers",\n\t\t\t\t\t"$(SRCROOT)/../../external/SDL.framework/Headers",\n\t\t\t\t\t"../../external/bgfx/include",\n\t\t\t\t\t"../../external/bx/include",\n\t\t\t\t\t"../../external/bimg/include",\n\t\t\t\t);',
    1
)

# 8. Add LIBRARY_SEARCH_PATHS for bgfx .a files to rttplayer Debug
content = content.replace(
    'LIBRARY_SEARCH_PATHS = (\n\t\t\t\t\t"$(inherited)",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../plugins/build-core/gameNetwork/mac\\\"",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../plugins/build-core/licensing/mac\\\"",\n\t\t\t\t\t/opt/X11/lib,\n\t\t\t\t\t"$(SYSTEM_APPS_DIR)/Steam.app/Contents/MacOS",\n\t\t\t\t);\n\t\t\t\tLIBRARY_SEARCH_PATHS_QUOTED_FOR_TARGET_1 = "\\\"$(SRCROOT)/../../modules/platform/mac/build/Release\\\"";\n\t\t\t\tOTHER_LDFLAGS = "-ObjC";\n\t\t\t\tPRODUCT_BUNDLE_IDENTIFIER = com.coronalabs.Corona_Simulator;\n\t\t\t\tPRODUCT_NAME = "Corona Simulator";\n\t\t\t\tPROVISIONING_PROFILE = "";\n\t\t\t\tPROVISIONING_PROFILE_SPECIFIER = "";\n\t\t\t\tSDKROOT = macosx;\n\t\t\t\tSTRIP_STYLE = "non-global";\n\t\t\t\tWARNING_CFLAGS',
    'LIBRARY_SEARCH_PATHS = (\n\t\t\t\t\t"$(inherited)",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../plugins/build-core/gameNetwork/mac\\\"",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../plugins/build-core/licensing/mac\\\"",\n\t\t\t\t\t/opt/X11/lib,\n\t\t\t\t\t"$(SYSTEM_APPS_DIR)/Steam.app/Contents/MacOS",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../external/bgfx/.build/projects/xcode15\\\"",\n\t\t\t\t);\n\t\t\t\tLIBRARY_SEARCH_PATHS_QUOTED_FOR_TARGET_1 = "\\\"$(SRCROOT)/../../modules/platform/mac/build/Release\\\"";\n\t\t\t\tOTHER_LDFLAGS = "-ObjC";\n\t\t\t\tPRODUCT_BUNDLE_IDENTIFIER = com.coronalabs.Corona_Simulator;\n\t\t\t\tPRODUCT_NAME = "Corona Simulator";\n\t\t\t\tPROVISIONING_PROFILE = "";\n\t\t\t\tPROVISIONING_PROFILE_SPECIFIER = "";\n\t\t\t\tSDKROOT = macosx;\n\t\t\t\tSTRIP_STYLE = "non-global";\n\t\t\t\tWARNING_CFLAGS',
    1
)

# rttplayer Release - add LIBRARY_SEARCH_PATHS
content = content.replace(
    'LIBRARY_SEARCH_PATHS = (\n\t\t\t\t\t"$(inherited)",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../plugins/build-core/gameNetwork/mac\\\"",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../plugins/build-core/licensing/mac\\\"",\n\t\t\t\t\t/opt/X11/lib,\n\t\t\t\t\t"$(SYSTEM_APPS_DIR)/Steam.app/Contents/MacOS",\n\t\t\t\t);\n\t\t\t\tLIBRARY_SEARCH_PATHS_QUOTED_FOR_TARGET_1 = "\\\"$(SRCROOT)/../../modules/platform/mac/build/Release\\\"";\n\t\t\t\tOTHER_LDFLAGS = "-ObjC";\n\t\t\t\tPRODUCT_BUNDLE_IDENTIFIER = com.coronalabs.Corona_Simulator;\n\t\t\t\tPRODUCT_NAME = "Corona Simulator";\n\t\t\t\tPROVISIONING_PROFILE = "";\n\t\t\t\tPROVISIONING_PROFILE_SPECIFIER = "";\n\t\t\t\tSDKROOT = macosx;\n\t\t\t\tSTRIP_STYLE = "non-global";\n\t\t\t};\n\t\t\tname = Release;',
    'LIBRARY_SEARCH_PATHS = (\n\t\t\t\t\t"$(inherited)",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../plugins/build-core/gameNetwork/mac\\\"",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../plugins/build-core/licensing/mac\\\"",\n\t\t\t\t\t/opt/X11/lib,\n\t\t\t\t\t"$(SYSTEM_APPS_DIR)/Steam.app/Contents/MacOS",\n\t\t\t\t\t"\\\"$(SRCROOT)/../../external/bgfx/.build/projects/xcode15\\\"",\n\t\t\t\t);\n\t\t\t\tLIBRARY_SEARCH_PATHS_QUOTED_FOR_TARGET_1 = "\\\"$(SRCROOT)/../../modules/platform/mac/build/Release\\\"";\n\t\t\t\tOTHER_LDFLAGS = "-ObjC";\n\t\t\t\tPRODUCT_BUNDLE_IDENTIFIER = com.coronalabs.Corona_Simulator;\n\t\t\t\tPRODUCT_NAME = "Corona Simulator";\n\t\t\t\tPROVISIONING_PROFILE = "";\n\t\t\t\tPROVISIONING_PROFILE_SPECIFIER = "";\n\t\t\t\tSDKROOT = macosx;\n\t\t\t\tSTRIP_STYLE = "non-global";\n\t\t\t};\n\t\t\tname = Release;',
    1
)

with open(PROJ, 'w') as f:
    f.write(content)

print("Done! Modified project.pbxproj successfully.")
print(f"Generated UUIDs:")
print(f"  bgfx group: {GRP_BGFX}")
print(f"  FR_BGFX: {FR_BGFX}")
print(f"  FR_METAL: {FR_METAL}")
