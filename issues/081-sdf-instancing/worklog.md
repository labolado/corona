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
