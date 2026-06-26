# Issue #081 - SDF Shape GPU Instancing Analysis

## Summary

GPU instancing is feasible for SDF shapes, but the current bgfx default instance attribute budget is the primary constraint. `bgfx::allocInstanceDataBuffer()` accepts any 16-byte-aligned stride at the allocation API level, but bgfx exposes only `i_data0` through `i_data4` by default, so a 96-byte `6 * vec4` instance payload is not directly addressable in shaders without raising `BGFX_CONFIG_MAX_INSTANCE_DATA_COUNT` and updating backend attribute-name tables.

Recommended route: route B, a new explicit instanced API such as `display.newInstancedMesh(count)` or an SDF-specific variant, reusing the existing `RenderData::fInstanceDraw -> DeferredCmd::instanceDraw -> ExecuteDraw()` fast path. Automatic route A is possible, but it fights current batching rules, custom-effect uniform semantics, and SDF's dedicated `kDrawSDF` command path.

## Q1: Can `allocInstanceDataBuffer` stride exceed 80 bytes?

Allocation: yes in the narrow API sense. The public bgfx declaration says the stride only "must be multiple of 16" (`external/bgfx/include/bgfx/bgfx.h:2804`-`external/bgfx/include/bgfx/bgfx.h:2818`). The implementation enforces instancing support, 16-byte alignment, and non-zero instance count, then delegates allocation (`external/bgfx/src/bgfx.cpp:4832`-`external/bgfx/src/bgfx.cpp:4838`). Internally the stride is aligned to 16 bytes and copied into the `InstanceDataBuffer` fields (`external/bgfx/src/bgfx_p.h:4924`-`external/bgfx/src/bgfx_p.h:4937`).

Shader access: no, not with the current default config. bgfx's configured maximum is 5 instance vec4 attributes, 80 bytes total (`external/bgfx/src/config.h:463`-`external/bgfx/src/config.h:468`). The GL backend exposes exactly five names, `i_data0`..`i_data4`, and static-asserts the array size against `BGFX_CONFIG_MAX_INSTANCE_DATA_COUNT` (`external/bgfx/src/renderer_gl.cpp:67`-`external/bgfx/src/renderer_gl.cpp:75`). The stock example follows the same contract: `varying.def.sc` declares `i_data0`..`i_data4` (`external/bgfx/examples/05-instancing/varying.def.sc:5`-`external/bgfx/examples/05-instancing/varying.def.sc:9`), and the vertex shader inputs use those five attributes (`external/bgfx/examples/05-instancing/vs_instancing.sc:1`-`external/bgfx/examples/05-instancing/vs_instancing.sc:17`).

Therefore, a 6th vec4 would require increasing `BGFX_CONFIG_MAX_INSTANCE_DATA_COUNT` to 6 and adding `i_data5` support consistently across renderer backends, shader varying definitions, and compiled shader variants. For this project, avoiding a bgfx fork/config divergence is preferable. The existing local instancing prototype already packs 2D affine transform, UV rect, and color into 5 vec4s: 80 bytes (`librtt/Display/Rtt_InstancedBatchRenderer.h:61`-`librtt/Display/Rtt_InstancedBatchRenderer.h:63`; `librtt/Display/Rtt_BatchObject.cpp:415`-`librtt/Display/Rtt_BatchObject.cpp:449`). SDF should use the same 80-byte discipline by packing or deriving fields.

## Q2: How would the Solar2D deferred path change?

Current SDF path:

- `ShapeObject::Draw()` builds one `SDFIssueData` per eligible shape and calls `renderer.InsertSDFDraw(issueData)` (`librtt/Display/Rtt_ShapeObject.cpp:209`-`librtt/Display/Rtt_ShapeObject.cpp:314`).
- `Renderer::InsertSDFDraw()` flushes pending geometry to preserve z-order, records one SDF draw, and increments draw stats (`librtt/Renderer/Rtt_Renderer.cpp:1909`-`librtt/Renderer/Rtt_Renderer.cpp:1919`).
- `DeferredCmd` has a separate `kDrawSDF` type plus embedded SDF payload fields (`librtt/Renderer/Rtt_BgfxCommandBuffer.h:43`-`librtt/Renderer/Rtt_BgfxCommandBuffer.h:45`; `librtt/Renderer/Rtt_BgfxCommandBuffer.h:109`-`librtt/Renderer/Rtt_BgfxCommandBuffer.h:116`).
- `DrawSDF()` snapshots the SDF payload and uniforms into a `DeferredCmd` (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2375`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2402`).
- `ExecuteDrawSDF()` allocates a transient quad VB/IB, sets uniforms, and submits once per command (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2287`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2357`).
- The replay loop treats `kDrawSDF` as a single submit and does not attempt batching (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2113`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2117`).

Existing non-SDF instancing path:

- `RenderData` already carries an opaque backend pointer, `fInstanceDraw` (`librtt/Renderer/Rtt_RenderData.h:54`-`librtt/Renderer/Rtt_RenderData.h:55`).
- `Renderer::Insert()` forwards it into the command buffer (`librtt/Renderer/Rtt_Renderer.cpp:719`-`librtt/Renderer/Rtt_Renderer.cpp:723`).
- `DeferredCmd` stores it as `instanceDraw` (`librtt/Renderer/Rtt_BgfxCommandBuffer.h:106`-`librtt/Renderer/Rtt_BgfxCommandBuffer.h:107`).
- `Draw()` consumes the pending pointer into the deferred command (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:710`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:716`).
- `ExecuteDraw()` has an instancing branch that binds base quad VB/IB, calls `bgfx::setInstanceDataBuffer()`, and submits once (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1003`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1065`).

For SDF instancing, the clean change is to introduce an SDF instanced command or generalized instance payload, not to force `kDrawSDF` through `ExecuteBatchedDraws()`. `CanBatchDraws()` currently rejects commands with `instanceDraw` (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1623`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1625`), rejects named uniforms (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1679`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1680`), and requires per-object user uniforms to match (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1682`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1691`). `ExecuteBatchedDraws()` also skips instanced commands (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1711`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1713`). Those rules are correct for mesh concatenation, but they are the opposite of what SDF instancing needs: per-instance params must vary while the program and fixed base quad remain shared.

## Q3: CPU vertex rotation + instance buffer vs current uniform writes

Instancing should be better for the target workload because it changes the scaling factor from "N draw submissions and N uniform sets" to "one or a few submissions plus one contiguous transient buffer fill." Current SDF execution does per object:

- transient vertex allocation for four expanded vertices (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2298`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2315`);
- transient index allocation for six indices (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2317`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2322`);
- transform, shape, polygon, and color uniform updates (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2324`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2349`);
- one `submit()` (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2351`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2357`).

Uniform snapshotting also copies built-in uniforms into every deferred draw (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:599`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:621`), and project memory already notes that `DeferredCmd` uniform storage was a known high-object-count CPU cost.

The existing instanced batch path fills an 80-byte record per active slot (`librtt/Display/Rtt_BatchObject.cpp:329`-`librtt/Display/Rtt_BatchObject.cpp:335`; `librtt/Display/Rtt_BatchObject.cpp:341`-`librtt/Display/Rtt_BatchObject.cpp:449`) and submits one draw through the instancing branch. For 713 shapes, 80 bytes is about 57 KB per frame; 96 bytes would be about 68 KB per frame. That memory bandwidth is small compared with 713 bgfx submissions, 713 uniform update groups, and repeated transient VB/IB allocations.

Caveat: if route A tries to auto-merge existing display objects, the engine still has to inspect and copy many `DeferredCmd` payloads. Route B can build the instance buffer directly from owned instance state and avoid most deferred-command fan-out.

## Q4: Route A vs Route B

### Route A: automatic merge adjacent draws with the same program

Pros:

- Preserves existing Lua object model (`display.newRect` plus effects).
- Can improve existing scenes without API migration.

Cons:

- Hard to make correct for custom generator/filter effects. Current batching deliberately rejects named uniforms and differing user-data uniforms because merging them would render all objects with the first draw's params (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1679`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1691`).
- SDF currently bypasses normal `kDraw`/`kDrawIndexed` and uses `kDrawSDF`, which replay never batches (`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2113`-`librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:2117`).
- `InsertSDFDraw()` intentionally flushes pending geometry before every SDF command for z-order (`librtt/Renderer/Rtt_Renderer.cpp:1909`-`librtt/Renderer/Rtt_Renderer.cpp:1914`), so automatic adjacent aggregation needs new ordering rules.
- Needs shape/program bucketing. Polygon SDF can carry up to 16 contour vertices (`librtt/Display/Rtt_SDFRenderer.h:64`-`librtt/Display/Rtt_SDFRenderer.h:65`), while the benchmark custom effect uses several user-data vec4s plus per-frame rotation (`tests/bgfx-demo/test_sdfpoly_bench.lua:20`-`tests/bgfx-demo/test_sdfpoly_bench.lua:30`; `tests/bgfx-demo/test_sdfpoly_bench.lua:188`-`tests/bgfx-demo/test_sdfpoly_bench.lua:192`).

Difficulty: high. Estimate 10-15 working days for a narrow SDF-only adjacent-run prototype, 20+ working days if it must work for arbitrary custom effects and maintain all batching invariants.

### Route B: explicit Lua API, e.g. `display.newInstancedMesh(count)`

Pros:

- Matches the engine's existing instancing architecture. `BatchObject` already allocates instance data, passes `fInstanceDraw`, and uses a single submit path (`librtt/Display/Rtt_BatchObject.cpp:325`-`librtt/Display/Rtt_BatchObject.cpp:357`; `librtt/Display/Rtt_BatchObject.cpp:453`-`librtt/Display/Rtt_BatchObject.cpp:477`).
- Avoids pretending independent display objects can be merged while preserving all per-object Lua semantics.
- Gives API-level constraints: same shader/program, same texture set, fixed max shape payload, explicit capacity, predictable update cost.
- Easier to keep within 5 vec4s by designing the per-instance schema from the start.

Cons:

- Requires a new user-facing API and proxy/state management.
- Existing scenes must migrate to the new object for best performance.
- Needs explicit decisions for hit testing, per-instance properties, z-order granularity, and dirty updates.

Difficulty: medium. Estimate 6-9 working days for an SDF-shape-only prototype using the existing instance draw bridge; 10-14 working days for a production Lua API with tests, docs, fallback behavior, and basic hit testing.

## Q5: Overall difficulty estimate

- Feasibility spike: 1-2 working days. Prove 80-byte packed SDF instance schema, compile shader, draw N rect/polygon SDF instances.
- Route B MVP: 6-9 working days. New instanced SDF display object, transient instance buffer fill, one submit per shape/program bucket, Lua create/update API, bgfx-only fallback handling.
- Route B production hardening: 10-14 working days. Add validation, dirty-region updates, lifecycle cleanup, tests, stats, and platform fallback.
- Route A narrow SDF auto-merge: 10-15 working days. Add `kDrawSDF` adjacent-run collection, instance buffer creation in replay, and ordering safeguards.
- Route A generic custom-effect auto-instancing: 20+ working days and high regression risk.

## Recommendation

Use route B first. Keep the instance stride at 80 bytes and pack SDF fields instead of expanding bgfx to a 6th instance vec4. The existing `BatchObject` path demonstrates the engine shape: build one transient instance buffer, pass `fInstanceDraw`, bind a base quad, call `bgfx::setInstanceDataBuffer()`, and submit once. For SDF polygons, constrain the first implementation to a small schema, for example: affine 2D transform in 3 vec4s, packed color/shape params in 1 vec4, and either packed polygon selector/rotation or a limited contour representation in 1 vec4. If full arbitrary 16-vertex polygons are required per instance, use route B with shape-template IDs or a texture/storage-buffer-like lookup later; do not try to fit all contour vertices into bgfx default instance attributes.

Route A can be revisited after route B defines the canonical per-instance SDF payload. Automatic merging should consume that same payload format; otherwise it risks duplicating a complex second instancing system inside deferred replay.

## Previous Experiment Notes

The requested prior worklog is not present under the current repository's `issues/` directory because `issues/` did not exist here before this analysis. The matching file was found at `/Users/yee/data/dev/app/labo/game_engine/issues/064-sdf-bevel-bench/worklog.md`. Its relevant conclusion was that SDF reduced triangles but increased draw calls, and listed batching/instancing as the follow-up direction (`/Users/yee/data/dev/app/labo/game_engine/issues/064-sdf-bevel-bench/worklog.md:93`-`/Users/yee/data/dev/app/labo/game_engine/issues/064-sdf-bevel-bench/worklog.md:97`). The same worklog's Method A benchmark described N custom-effect display objects as approximately N draw calls (`/Users/yee/data/dev/app/labo/game_engine/issues/064-sdf-bevel-bench/worklog.md:116`-`/Users/yee/data/dev/app/labo/game_engine/issues/064-sdf-bevel-bench/worklog.md:127`) and recorded bgfx draw counts growing as N+4 in that test (`/Users/yee/data/dev/app/labo/game_engine/issues/064-sdf-bevel-bench/worklog.md:151`-`/Users/yee/data/dev/app/labo/game_engine/issues/064-sdf-bevel-bench/worklog.md:158`).
