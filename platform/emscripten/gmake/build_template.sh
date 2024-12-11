#!/bin/sh -e

path=`dirname $0`

CLEAN=true
config=Release
PARALLEL=true

while test $# != 0
do
    case "$1" in
    -nc) CLEAN=false ;;
    -np) PARALLEL=false ;;
    -d) config=Debug ;;
    esac
    shift
done

if [ "$CLEAN" = true ] ; then
    make clean config=$config
fi

# Use RAM disk for temporary files if available
if [ -d "/dev/shm" ]; then
    export TMPDIR="/dev/shm"
fi

rm -rf "$path/webtemplate_build"

# Create assets folder in memory if possible
if [ -d "/dev/shm" ]; then
    ASSETS_DIR="/dev/shm/assetsFolder-$$"
else
    ASSETS_DIR="$path/assetsFolder"
fi

rm -rf "$ASSETS_DIR"
mkdir -p "$ASSETS_DIR"
touch "$ASSETS_DIR/CORONA_FILE_PLACEHOLDER"
mkdir -p "$ASSETS_DIR/CORONA_FOLDER_PLACEHOLDER"
touch  "$ASSETS_DIR/CORONA_FOLDER_PLACEHOLDER/zzz"

# Set optimal compilation environment
if [ "$PARALLEL" = true ] && [ -z "$EMCC_CORES" ]; then
    export EMCC_CORES=$(getconf _NPROCESSORS_ONLN)
fi

"$path"/build_app.sh "$ASSETS_DIR" "$path/webtemplate_build/coronaHtml5App.html" $config

rm -rf "$ASSETS_DIR"


pushd "$path/webtemplate_build" > /dev/null

# html
rm coronaHtml5App.html
mkdir html
mv coronaHtml5App.* html/
rm html/coronaHtml5App.data
cp ../template.webapp/emscripten.html html/index.html
cp ../template.webapp/emscripten-debug.html html/index-debug.html
cp ../template.webapp/emscripten-nosplash.html html/index-nosplash.html

cp -r ../template.webapp/fbinstant fbinstant/

# mkdir res_font
# cp ../../mac/OpenSans-Regular.ttf res_font

mkdir res_widget
cp ../../../../subrepos/widget/widget_theme_*.png res_widget/

# mkdir bin
# cp ../obj/Release/libcorona.o bin/
# cp ../../*.js bin/
# echo "return { plugin = { exportedFunctions = {'_main'}, }, }" > bin/metadata.lua

rm -f ../../webtemplate.zip
zip -r ../../webtemplate.zip ./

pushd ../../ > /dev/null
echo "Template zip:"
echo "$(pwd)/webtemplate.zip"
popd > /dev/null

popd > /dev/null