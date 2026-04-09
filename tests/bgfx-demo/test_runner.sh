#!/bin/bash
cd /Users/yee/data/dev/app/labo/corona/tests/bgfx-demo
export SOLAR2D_TEST=regression
export SOLAR2D_BACKEND=bgfx
"/Users/yee/data/dev/app/labo/corona/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator" . > /tmp/solar2d_test.log 2>&1 &
pid=$!
sleep 25
kill $pid 2>/dev/null
cat /tmp/solar2d_test.log
