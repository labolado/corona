#!/usr/bin/env bash
set -e
if [ "${REPLACE_CODE_SIGN_QUIET:-0}" != "1" ]; then
    set -x
fi

# platform mac
sign1_a='CODE_SIGN_IDENTITY = "Developer ID Application: Corona Labs Inc"'
sign1_b='CODE_SIGN_IDENTITY = "Developer ID Application: Labo Lado Inc"'
# sign2_a='DEVELOPMENT_TEAM = BG2J43EA88'
# sign2_b='DEVELOPMENT_TEAM = V9E9E7HUEW'
sign2_a='= BG2J43EA88'
sign2_b='= V9E9E7HUEW'
sign3_a='com\.(coronalabs\.Corona_Simulator|labolado\.solar2d(\.b3)?)'
if [ -n "${SIMULATOR_BUNDLE_ID:-}" ]; then
    sign3_b="$SIMULATOR_BUNDLE_ID"
elif [[ "${BUILD:-${BUILD_NUMBER:-}}" =~ (^|\.)b3(\.|$) ]]; then
    sign3_b='com.labolado.solar2d.b3'
else
    sign3_b='com.labolado.solar2d'
fi
if [[ ! "$sign3_b" =~ ^[[:alnum:].-]+$ ]]; then
    echo "Error: invalid SIMULATOR_BUNDLE_ID '$sign3_b'" >&2
    exit 1
fi
sign4_a='Developer ID Application: Corona Labs Inc'
sign4_b='Developer ID Application: Labo Lado Inc'

sed -i.bak "s/${sign1_a}/${sign1_b}/g" platform/mac/car.xcodeproj/project.pbxproj
sed -i.bak "s/${sign1_a}/${sign1_b}/g" platform/mac/CoronaBuilder.xcodeproj/project.pbxproj
sed -i.bak "s/${sign1_a}/${sign1_b}/g" platform/mac/CoronaShell/CoronaShell.xcodeproj/project.pbxproj
sed -i.bak "s/${sign1_a}/${sign1_b}/g" platform/mac/car.xcodeproj/project.pbxproj
sed -i.bak "s/${sign1_a}/${sign1_b}/g" platform/mac/lua.xcodeproj/project.pbxproj
sed -i.bak "s/${sign4_a}/${sign4_b}/g" bin/mac/build_dmg.sh
sed -i.bak -E "s/${sign3_a}/${sign3_b}/g" platform/mac/CoronaConsole/CoronaConsole/AppDelegate.m
sed -i.bak "s/${sign1_a}/${sign1_b}/g" platform/mac/ratatouille.xcodeproj/project.pbxproj
sed -i.bak "s/${sign2_a}/${sign2_b}/g" platform/mac/ratatouille.xcodeproj/project.pbxproj
sed -i.bak -E "s/${sign3_a}/${sign3_b}/g" platform/mac/ratatouille.xcodeproj/project.pbxproj
sed -i.bak "s/${sign4_a}/${sign4_b}/g" platform/mac/ratatouille.xcodeproj/project.pbxproj
rm -f platform/mac/car.xcodeproj/project.pbxproj.bak
rm -f platform/mac/CoronaBuilder.xcodeproj/project.pbxproj.bak
rm -f platform/mac/CoronaShell/CoronaShell.xcodeproj/project.pbxproj.bak
rm -f platform/mac/car.xcodeproj/project.pbxproj.bak
rm -f platform/mac/lua.xcodeproj/project.pbxproj.bak
rm -f bin/mac/build_dmg.sh.bak
rm -f platform/mac/CoronaConsole/CoronaConsole/AppDelegate.m.bak
rm -f platform/mac/ratatouille.xcodeproj/project.pbxproj.bak

if [ "${REPLACE_CODE_SIGN_MAC_ONLY:-0}" != "1" ]; then
    # platform iphone
    sign5_a='PROVISIONING_PROFILE_SPECIFIER = ios'
    sign5_b='PROVISIONING_PROFILE_SPECIFIER = "dev ios solar2d";\nPRODUCT_BUNDLE_IDENTIFIER = com.labolado.solar2d'
    sed -i.bak "s/${sign2_a}/${sign2_b}/g" platform/iphone/ratatouille.xcodeproj/project.pbxproj
    sed -i.bak "s/${sign5_a}/${sign5_b}/g" platform/iphone/ratatouille.xcodeproj/project.pbxproj
    rm -f platform/iphone/ratatouille.xcodeproj/project.pbxproj.bak

    # platform tvos
    sign6_a='PROVISIONING_PROFILE_SPECIFIER = tvos'
    sign6_b='PROVISIONING_PROFILE_SPECIFIER = "com.labolado.* Development"'
    sign7_a='com.coronalabs.'
    sign7_b='com.labolado.'
    sed -i.bak "s/${sign2_a}/${sign2_b}/g" platform/tvos/ratatouille.xcodeproj/project.pbxproj
    sed -i.bak "s/${sign6_a}/${sign6_b}/g" platform/tvos/ratatouille.xcodeproj/project.pbxproj
    sed -i.bak "s/${sign7_a}/${sign7_b}/g" platform/tvos/ratatouille.xcodeproj/project.pbxproj
    rm -f platform/tvos/ratatouille.xcodeproj/project.pbxproj.bak
fi

# "plugins/gameNetwork/ios/CoronaEnterprise/Project Template/App/ios/App.xcodeproj/project.pbxproj"
# "plugins/gameNetwork/mac/CoronaEnterprise/Project Template/App/ios/App.xcodeproj/project.pbxproj"
# "plugins/licensing/ios/CoronaEnterprise/Project Template/App/ios/App.xcodeproj/project.pbxproj"
# "plugins/licensing/mac/CoronaEnterprise/Project Template/App/ios/App.xcodeproj/project.pbxproj"
# "plugins/network/ios/CoronaEnterprise/Project Template/App/ios/App.xcodeproj/project.pbxproj"
# "plugins/network/mac/CoronaEnterprise/Project Template/App/ios/App.xcodeproj/project.pbxproj"
# "subrepos/enterprise/contents/Project Template/App/ios/App.xcodeproj/project.pbxproj"
