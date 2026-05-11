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
	private static final String[] SCENARIO_TEST_NAMES = { null, null, "bench" };

	private Handler fFinishHandler;
	private Runnable fFinishRunnable;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		Intent intent = getIntent();
		long durationMs = DEFAULT_DURATION_MS;
		if (intent != null && ACTION_TEST_LOOP.equals(intent.getAction())) {
			int scenarioNumber = intent.getIntExtra("scenarioNumber", 1);
			Log.i(TAG, "TEST_LOOP launched scenario=" + scenarioNumber);

			// Inject SOLAR2D_TEST for non-default scenarios
			if (scenarioNumber >= 2 && scenarioNumber < SCENARIO_TEST_NAMES.length
					&& SCENARIO_TEST_NAMES[scenarioNumber] != null) {
				String testName = SCENARIO_TEST_NAMES[scenarioNumber];
				Log.i(TAG, "Setting SOLAR2D_TEST=" + testName + " for scenario " + scenarioNumber);
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
					try {
						android.system.Os.setenv("SOLAR2D_TEST", testName, true);
					} catch (Exception e) {
						Log.w(TAG, "setenv failed: " + e.getMessage());
					}
				}
				// Bench needs more time: 5 levels × ~300 frames + overhead ≈ 8 min
				durationMs = 540_000L;
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
