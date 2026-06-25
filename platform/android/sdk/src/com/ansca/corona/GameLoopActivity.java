// ----------------------------------------------------------------------------
//
// GameLoopActivity.java
//
// FTL game-loop entry: extends CoronaActivity to handle
// android.intent.action.TEST_LOOP (Firebase Test Lab Game Loop).
// The base activity drives the Solar2D runtime as usual; this subclass
// just tags the launch as a non-interactive test session and auto-
// finishes after a fixed duration so the FTL crawler does not tap the
// screen and break our timer-driven regression harness.
//
// ----------------------------------------------------------------------------

package com.ansca.corona;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

public class GameLoopActivity extends CoronaActivity {
	private static final String TAG = "GameLoop";
	private static final String ACTION_TEST_LOOP = "com.google.intent.action.TEST_LOOP";
	private static final long DEFAULT_DURATION_MS = 900_000L;

	// Scenario mapping:
	//   1 = regression (20-scene walkthrough, default)
	//   2 = bench (performance benchmark, 5 stress levels, ~8 min)
	//   3 = all_scenes (walk all nav scenes, measure FPS per scene)
	//   4 = shapes (vector graphics benchmark: rects/circles/roundedRects)
	//   5 = perf (7-scene throughput benchmark, ~3 min)
	//   6 = sdf_perf (SDF OFF vs ON head-to-head: FPS + draw calls + tris, ~30s)
	//   7 = realrepro079 (#079 extended-geometry shared-pool regression: static grid of marbles, auto backend → Vulkan)
	//   8 = realrepro079 with SOLAR2D_VULKAN=0 (force GLES backend for cross-backend #079 coverage)
	//   9 = sdf_ftl (SDF quality-tier stress test: HIGH/MID/LOW 25s each, 200 shapes, auto FPS logging)
	private static final String[] SCENARIO_TEST_NAMES = { null, null, "bench", "all_scenes", "shapes", "perf", "sdf_perf", "realrepro079", "realrepro079", "sdf_ftl" };

	private Handler fFinishHandler;
	private Runnable fFinishRunnable;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		Intent intent = getIntent();
		long durationMs = DEFAULT_DURATION_MS;
		if (intent != null && ACTION_TEST_LOOP.equals(intent.getAction())) {
			int scenarioNumber = intent.getIntExtra("scenario", 1);
			Log.i(TAG, "TEST_LOOP launched scenario=" + scenarioNumber);

			// Inject SOLAR2D_TEST for non-default scenarios
			if (scenarioNumber >= 2 && scenarioNumber < SCENARIO_TEST_NAMES.length
					&& SCENARIO_TEST_NAMES[scenarioNumber] != null) {
				String testName = SCENARIO_TEST_NAMES[scenarioNumber];
				Log.i(TAG, "Setting SOLAR2D_TEST=" + testName + " for scenario " + scenarioNumber);
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
					try {
						android.system.Os.setenv("SOLAR2D_TEST", testName, true);
						// Scenario 8 forces the GLES backend so #079 is verified on both
						// bgfx renderers (scenario 7 = auto/Vulkan, scenario 8 = GLES).
						if (scenarioNumber == 8) {
							android.system.Os.setenv("SOLAR2D_VULKAN", "0", true);
							Log.i(TAG, "Setting SOLAR2D_VULKAN=0 (force GLES) for scenario 8");
						}
					} catch (Exception e) {
						Log.w(TAG, "setenv failed: " + e.getMessage());
					}
				}
				// Per-scenario duration
				if (scenarioNumber == 2) {
					durationMs = 540_000L; // bench: 5 levels × ~300 frames + overhead ≈ 8 min
				} else if (scenarioNumber == 3) {
					durationMs = 180_000L; // all_scenes: 11 scenes × ~8s + transitions ≈ 2 min
				} else if (scenarioNumber == 4) {
					durationMs = 180_000L; // shapes: 5 levels × ~20s + warmup ≈ 2 min
				} else if (scenarioNumber == 5) {
					durationMs = 660_000L; // perf: 10 scenes × ~4 levels × ~7s ≈ 11 min
				} else if (scenarioNumber == 6) {
					durationMs = 90_000L;  // sdf_perf: OFF(10s)+ON(10s)+overhead ≈ 1.5 min
				} else if (scenarioNumber == 7 || scenarioNumber == 8) {
					durationMs = 30_000L;  // realrepro079: static render, 30s to settle + record video
				} else if (scenarioNumber == 9) {
					durationMs = 120_000L; // sdf_ftl: 3 tiers × 25s + warmup + overhead ≈ 2 min
				}
			}

			long override = intent.getLongExtra("durationMs", 0L);
			if (override > 0L) {
				durationMs = override;
			}
		}

		super.onCreate(savedInstanceState);

		final long finalDurationMs = durationMs;
		fFinishHandler = new Handler(Looper.getMainLooper());
		fFinishRunnable = new Runnable() {
			@Override
			public void run() {
				Log.i(TAG, "Auto-finishing TEST_LOOP after " + finalDurationMs + " ms");
				setResult(RESULT_OK);
				finish();
			}
		};
		fFinishHandler.postDelayed(fFinishRunnable, durationMs);
	}

	@Override
	protected void onDestroy() {
		if (fFinishHandler != null && fFinishRunnable != null) {
			fFinishHandler.removeCallbacks(fFinishRunnable);
		}
		super.onDestroy();
	}
}
