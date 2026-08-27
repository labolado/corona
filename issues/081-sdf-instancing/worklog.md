# Issue #081 - Worklog

## 2026-06-25

- Read task brief from `/tmp/worker-task-w-inst-081.md`.
- Queried worker memory for `SDF instancing instanceDataBuffer allocInstanceDataBuffer DeferredCmd`.
- Confirmed current repository had no `issues/` directory, so created `issues/081-sdf-instancing/`.
- Read the requested bgfx/Solar2D files:
  - `librtt/Renderer/Rtt_BgfxCommandBuffer.cpp`
  - `librtt/Renderer/Rtt_BgfxCommandBuffer.h`
  - `librtt/Renderer/Rtt_Renderer.cpp`
  - `external/bgfx/include/bgfx/bgfx.h`
- Also read supporting implementation files for line-backed conclusions:
  - `external/bgfx/src/config.h`
  - `external/bgfx/src/bgfx.cpp`
  - `external/bgfx/src/bgfx_p.h`
  - `external/bgfx/src/renderer_gl.cpp`
  - `external/bgfx/examples/05-instancing/*`
  - `librtt/Display/Rtt_BatchObject.cpp`
  - `librtt/Display/Rtt_InstancedBatchRenderer.h`
  - `librtt/Display/Rtt_ShapeObject.cpp`
  - `librtt/Display/Rtt_SDFRenderer.h`
  - `tests/bgfx-demo/test_sdfpoly_bench.lua`
- The requested `issues/064-sdf-bevel-bench/worklog.md` was not present in this repo. Found the matching prior worklog at `/Users/yee/data/dev/app/labo/game_engine/issues/064-sdf-bevel-bench/worklog.md` and cited it explicitly.
- Wrote `analysis.md` with Q1-Q5 answers, route recommendation, and working-day estimates.

Result: route B is recommended; keep bgfx instance payload at 80 bytes unless the project is willing to raise `BGFX_CONFIG_MAX_INSTANCE_DATA_COUNT` and update backend/shader support for `i_data5`.

## 2026-06-27

- Read task brief from `/tmp/worker-task-w-diag.md` and queried worker memory for SDF instanced/bgfx shader diagnostics.
- Added Android diagnostic logs in `SDFInstanceRenderer::CreateProgram()` around shader binary sizes, `bgfx::createShader()`, and `bgfx::createProgram()`.
- Added Android diagnostic log in `SDFGroupObject::Prepare()` for `inst.IsAvailable()`, `inst.IsSupported()`, and `fActiveCount`.
- Built `:Corona:assembleRelease` successfully after full `assembleRelease` repeatedly crashed in the App packaging phase.
- Repacked `Corona-release.aar` with the unstripped `.cxx/RelWithDebInfo/.../obj/arm64-v8a/libcorona.so` and installed it into `/Applications/Corona-b3`.
- Built `tests/bgfx-demo` with CoronaBuilder, then manually replaced the APK's stripped `libcorona.so` with the unstripped one and re-signed with the Corona-b3 debug keystore.
- Verified with `nm -C` that the APK's `libcorona.so` contains `SDFInstanceRenderer::CreateProgram()` and `SDFGroupObject::Prepare()`.
- Installed and launched on device `KXU0221225000888`.

Result: device logs show `SDFInst: VS valid=1`, `FS valid=1`, `program valid=1`, and `SDFGroup: inst available=1, supported=1, activeCount=6`; program creation failure is ruled out.

- Added Android logs in `SDFGroupObject::FillInstanceData()` for parent matrix, first world matrix, and first two instance payloads.
- Rebuilt `assembleRelease`, repacked Corona-b3 AAR with unstripped arm64 `libcorona.so`, built `tests/bgfx-demo`, replaced APK `libcorona.so` with stored/unstripped copy, signed, installed, and launched on device `KXU0221225000888`.
- Device logs showed sane instance data: `activeCount=6`, identity parent matrix, finite 90x90-derived world matrix, expected positions/colors.
- User confirmed original build still showed only text/no shapes.
- Patched `vs_sdf_instanced.sc` to force all quads to screen center, compiled SPIR-V (`s_vs_sdf_instanced_spirv_size=1263`), regenerated `Rtt_BgfxShaderData_sdf_instanced_spirv.h`, rebuilt/repacked/reinstalled.
- Screenshot `/tmp/w-diag-forced-vs.png` shows a large red quad visible on device.

Result: render pipeline/draw submission works; failure is isolated to original SDF instanced vertex shader path reading/applying instance attributes (`i_data*`) or their GPU binding/layout, not CPU-side instance values nor fragment shader output.

### Final Android build and device verification

- Recompiled the SDF instanced Vulkan shaders with `shadercRelease`: VS 1,785 bytes and FS 36,426 bytes.
- Regenerated `Rtt_BgfxShaderData_sdf_instanced_spirv.h` from the compiled binaries.
- Fixed two stale `s_vs_batch_instanced_spirv*` references in `SDFInstanceRenderer::CreateProgram()` to use the SDF vertex shader symbols.
- Built `:Corona:assembleRelease` successfully and repacked the AAR with the 248,915,296-byte unstripped arm64 `libcorona.so`.
- Installed the repacked AAR into `/Applications/Corona-b3`, built `/tmp/android-build-v2/bgfx-demo.apk`, and installed/launched it on device.
- Captured `/tmp/sc_final.png`; bright-pixel check returned `10188/2527200 (0.4%)`, below the required 5% visibility threshold.
- Fresh initialization logs contain no SDF shader error: `VS valid=1`, `FS valid=1`, and `program valid=1`; six active instances are submitted.

Result: build/install pipeline succeeded, but the requested visual threshold failed despite valid shader/program creation.

### Root-cause fix after visual failure

- Kept the new base-quad layout (`Position3 + TexCoord0`) and `TEXCOORD7..3` instance semantics; a GPU diagnostic proved `i_data2.w` arrived as the expected value `1`, so the new vertex/instance binding was not the regression.
- Confirmed the captured Solar2D VP matrix was valid for the 360x780 surface (`sx=0.00556`, `sy=-0.00256`, `tx=-1`, `ty=1`).
- Found that the instanced draw branch did not submit this matrix as a bgfx draw transform. Pixel-space instance coordinates were therefore outside clip space.
- Added `bgfx::setTransform()` for instanced draws and changed the SDF instanced VS to apply the submitted full VP through `u_model[0]`, avoiding an additional bgfx view/projection composition.
- Recompiled SPIR-V, rebuilt/repacked the AAR with the unstripped arm64 library, rebuilt and installed the APK.
- Device screenshot `/tmp/sc_final_fixed.png` shows all six beveled shapes. Bright-pixel verification improved from `0.40%` to `5.88%`.

Result: SDF instanced beveled shapes render correctly with the new vertex layout and instance semantics.
