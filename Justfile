default:
    @just --list

# === Build ===

# macOS build (BACKEND: gl|bgfx, CONFIG: Debug|Release)
build-mac BACKEND="bgfx" CONFIG="Debug":
    xcodebuild -project platform/mac/ratatouille.xcodeproj -target rttplayer \
      -configuration {{CONFIG}} build \
      CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
      ARCHS=arm64 ONLY_ACTIVE_ARCH=NO

# macOS ASAN build (catches memory errors)
build-mac-asan BACKEND="bgfx" CONFIG="Debug":
    ASAN_OPTIONS='detect_leaks=0:halt_on_error=1' \
    xcodebuild -project platform/mac/ratatouille.xcodeproj -target rttplayer \
      -configuration {{CONFIG}} build \
      CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
      ARCHS=arm64 ONLY_ACTIVE_ARCH=NO ENABLE_ADDRESS_SANITIZER=YES

# Android: compile AAR → install to Corona-b3 → CoronaBuilder package → adb install
build-android PROJECT="tests/bgfx-demo" BUNDLE_ID="com.labolado.bgfxdemo":
    bash tests/build_android.sh {{PROJECT}} {{BUNDLE_ID}}

# Android force rebuild (ignore caches)
build-android-force PROJECT="tests/bgfx-demo" BUNDLE_ID="com.labolado.bgfxdemo":
    FORCE_BUILD=1 bash tests/build_android.sh {{PROJECT}} {{BUNDLE_ID}}

# Compile bgfx shaders (.sc → binary headers)
build-shaders TYPE="default":
    bash tests/compile_shaders.sh {{TYPE}}

# === Run ===

# Launch macOS simulator (BACKEND: gl|bgfx)
run PROJECT="tests/bgfx-demo" BACKEND="bgfx":
    SOLAR2D_BACKEND={{BACKEND}} \
    "platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator" \
      -no-console YES {{PROJECT}}

# Launch with specific test entry
run-test TEST PROJECT="tests/bgfx-demo" BACKEND="bgfx":
    SOLAR2D_TEST={{TEST}} SOLAR2D_BACKEND={{BACKEND}} \
    "platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator" \
      -no-console YES {{PROJECT}}

# === Test ===

# Full regression (debug|release|all|screenshots)
test-all MODE="debug":
    bash tests/run_all_tests.sh {{MODE}}

# GL vs bgfx screenshot comparison for a project
test-compare PROJECT="tests/bgfx-demo":
    bash tests/test_compare.sh {{PROJECT}}

# Render comparison + gemma4 analysis
compare-render PROJECT="tests/bgfx-demo":
    bash tests/compare_render.sh {{PROJECT}}

# === Android Test ===

# Flash (install + run) Android project
flash-android PROJECT="tests/bgfx-demo" BUNDLE_ID="com.labolado.bgfxdemo":
    bash tests/test_flash.sh {{PROJECT}} {{BUNDLE_ID}}

# Android replay recorded session
replay-android REPLAY:
    bash tests/android_replay.sh {{REPLAY}}

# Android full test cycle: build → install → replay → screenshot → log
android-cycle PROJECT="tests/bgfx-demo" BUNDLE_ID="com.labolado.bgfxdemo":
    bash tests/android_test_cycle.sh --project {{PROJECT}} --bundle-id {{BUNDLE_ID}} --build

# === Utility ===

# Kill stale Corona/lua processes
kill-stale:
    pkill -f 'Corona Simulator' 2>/dev/null || true
    pkill -f 'corona/platform.*lua' 2>/dev/null || true

# Clean build artifacts
clean:
    rm -rf platform/mac/build
    cd platform/android && ./gradlew :Corona:clean --no-daemon

# Check shader binaries are in sync
shaders-check:
    bash tests/compile_shaders.sh --check

# Show available test entries
list-tests:
    @echo "Available SOLAR2D_TEST entries:"
    @echo "  bench       - Performance benchmark (500-5000 objects FPS)"
    @echo "  regression  - Full regression (10 scenes)"
    @echo "  realworld   - Static UI + dynamic + particles"
    @echo "  scene       - Single scene (use SOLAR2D_SCENE=xxx)"
    @echo "  capture     - CaptureRect test"
    @echo "  atlas       - Atlas functionality"
    @echo "  batch       - Batch functionality + performance"
    @echo "  sdf         - SDF rendering comparison"
    @echo "  leak        - Memory leak detection"
