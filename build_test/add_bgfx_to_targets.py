#!/usr/bin/env python3
"""Add bgfx libs to CoronaCards and CoronaCore framework phases."""

PROJ = "/Users/yee/data/dev/app/labo/corona/platform/mac/ratatouille.xcodeproj/project.pbxproj"

with open(PROJ, 'r') as f:
    content = f.read()

# Build file entries to add to PBXBuildFile section
new_buildfiles = """		E707EA34B233C77BB9023074 /* libbgfxRelease.a in Frameworks */ = {isa = PBXBuildFile; fileRef = 62F53053CDC9ACE4FAD588E4 /* libbgfxRelease.a */; };
		FF81CD90243D480D18AC59E3 /* libbxRelease.a in Frameworks */ = {isa = PBXBuildFile; fileRef = 6BDF997774391BA9C031D638 /* libbxRelease.a */; };
		37CD1BDC89083A1E1AA8E704 /* libbimgRelease.a in Frameworks */ = {isa = PBXBuildFile; fileRef = 6F39D5C684AA098023A5A409 /* libbimgRelease.a */; };
		A19881EA5CF3C88271B4BAB3 /* libbimg_decodeRelease.a in Frameworks */ = {isa = PBXBuildFile; fileRef = 81D7C01BD247D477ED064E86 /* libbimg_decodeRelease.a */; };
		2B371B81E9AFFA5E0B6FB4A1 /* Metal.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = B5C34561E307CC8B13FF96D9 /* Metal.framework */; };
		6249922BC099C1D94B8DD529 /* libbgfxRelease.a in Frameworks */ = {isa = PBXBuildFile; fileRef = 62F53053CDC9ACE4FAD588E4 /* libbgfxRelease.a */; };
		EC3BBE729205476ED2BB0B20 /* libbxRelease.a in Frameworks */ = {isa = PBXBuildFile; fileRef = 6BDF997774391BA9C031D638 /* libbxRelease.a */; };
		2B7F54680296C729DB295298 /* libbimgRelease.a in Frameworks */ = {isa = PBXBuildFile; fileRef = 6F39D5C684AA098023A5A409 /* libbimgRelease.a */; };
		22EA586B206094C270D49A1F /* libbimg_decodeRelease.a in Frameworks */ = {isa = PBXBuildFile; fileRef = 81D7C01BD247D477ED064E86 /* libbimg_decodeRelease.a */; };
		3A549BB22B167806E935C406 /* Metal.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = B5C34561E307CC8B13FF96D9 /* Metal.framework */; };"""

content = content.replace(
    '/* Begin PBXBuildFile section */\n',
    '/* Begin PBXBuildFile section */\n' + new_buildfiles + '\n',
    1
)

# Add to CoronaCards framework phase (A4FBC4F41A2D5479004D9A01)
cc_entries = """				E707EA34B233C77BB9023074 /* libbgfxRelease.a in Frameworks */,
				FF81CD90243D480D18AC59E3 /* libbxRelease.a in Frameworks */,
				37CD1BDC89083A1E1AA8E704 /* libbimgRelease.a in Frameworks */,
				A19881EA5CF3C88271B4BAB3 /* libbimg_decodeRelease.a in Frameworks */,
				2B371B81E9AFFA5E0B6FB4A1 /* Metal.framework in Frameworks */,"""

content = content.replace(
    'A4FBC4F41A2D5479004D9A01 /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n',
    'A4FBC4F41A2D5479004D9A01 /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n' + cc_entries + '\n',
    1
)

# Add to CoronaCore framework phase (C26E845A1A5DE0C0008950FB)
core_entries = """				6249922BC099C1D94B8DD529 /* libbgfxRelease.a in Frameworks */,
				EC3BBE729205476ED2BB0B20 /* libbxRelease.a in Frameworks */,
				2B7F54680296C729DB295298 /* libbimgRelease.a in Frameworks */,
				22EA586B206094C270D49A1F /* libbimg_decodeRelease.a in Frameworks */,
				3A549BB22B167806E935C406 /* Metal.framework in Frameworks */,"""

content = content.replace(
    'C26E845A1A5DE0C0008950FB /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n',
    'C26E845A1A5DE0C0008950FB /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n' + core_entries + '\n',
    1
)

with open(PROJ, 'w') as f:
    f.write(content)

print("Done! Added bgfx libs to CoronaCards and CoronaCore.")
