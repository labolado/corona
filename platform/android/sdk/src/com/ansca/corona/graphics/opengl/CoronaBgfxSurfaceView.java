//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

package com.ansca.corona.graphics.opengl;

import android.util.Log;

/**
 * A plain SurfaceView for the bgfx rendering backend.
 * <p>
 * Unlike CoronaGLSurfaceView (which extends GLSurfaceView and auto-creates an EGL context),
 * this view does NOT create any EGL context. bgfx manages its own EGL context internally.
 * Using GLSurfaceView with bgfx causes "already connected to another API" crashes because
 * both GLSurfaceView and bgfx try to attach EGL surfaces to the same ANativeWindow.
 * <p>
 * This class provides the same external API as CoronaGLSurfaceView so that CoronaActivity,
 * Controller, and ViewManager can use it interchangeably.
 */
public class CoronaBgfxSurfaceView extends android.view.SurfaceView
		implements android.view.SurfaceHolder.Callback, CoronaSurfaceViewInterface {

	private static final String TAG = "CoronaBgfxSurfaceView";

	/** Reference to the Corona activity window that owns this view. */
	private android.app.Activity fActivity;

	/** The Corona runtime. */
	private com.ansca.corona.CoronaRuntime fCoronaRuntime;

	/** Whether this view is a CoronaKit view. */
	private boolean fIsCoronaKit;

	/** Flag indicating the render thread can render. */
	private volatile boolean fCanRender;

	/** Flag indicating the surface is valid. */
	private volatile boolean fSurfaceValid;

	/** The render thread (replaces GLThread). */
	private RenderThread fRenderThread;

	/** Shared lock for render-thread state changes and wakeups. */
	private final Object fRenderSignal = new Object();

	/** Main-thread handler used to schedule Choreographer callbacks safely. */
	private final android.os.Handler fUiHandler;

	/** Timer used to make sure that the surface is working. */
	private com.ansca.corona.MessageBasedTimer fWatchdogTimer;

	/**
	 * Receives the device's current display orientation/rotation in degrees.
	 */
	private android.view.OrientationEventListener fOrientationListener;

	private com.ansca.corona.WindowOrientation fCurrentDeviceOrientation;
	private com.ansca.corona.WindowOrientation fPreviousDeviceOrientation;
	private com.ansca.corona.WindowOrientation fCurrentWindowOrientation;
	private com.ansca.corona.WindowOrientation fPreviousWindowOrientation;

	private com.ansca.corona.CoronaActivityInfo fActivityInfo;

	/** Whether the view is paused. */
	private volatile boolean fPaused = true;

	/** Whether a render has been requested (for RENDERMODE_WHEN_DIRTY equivalent). */
	private volatile boolean fRenderRequested = false;

	/** Whether a swap is needed. */
	private volatile boolean fNeedsSwap = true;

	/**
	 * Flag set when the surface is restored after being destroyed (e.g. lock-screen resume).
	 * When true, the render thread will force-render one frame even if fPaused is still true,
	 * to ensure the new EGL surface gets content before the compositor shows it as black.
	 */
	private volatile boolean fSurfaceRestored = false;

	/** VSync trigger flag: set by Choreographer callback, consumed by RenderThread. */
	private volatile boolean fVSyncTriggered = false;

	/** Frame scheduling flag: prevents duplicate Choreographer posts. */
	private volatile boolean fFrameScheduled = false;

	/** Choreographer for VSync-bound rendering (replaces wait(16) polling). */
	private android.view.Choreographer fChoreographer;

	/** Frame callback posted to Choreographer to trigger one render frame. */
	private android.view.Choreographer.FrameCallback fFrameCallback;

	/** Posts the next Choreographer callback from the main thread. */
	private Runnable fPostFrameCallbackRunnable;

	/** Queue of runnables to execute on the render thread. */
	private final java.util.ArrayList<Runnable> fEventQueue = new java.util.ArrayList<>();

	/** ImageView that covers the surface during recreation to prevent black frames. */
	private android.widget.ImageView fCoverView;

	/** Bitmap captured from the surface before destruction. */
	private android.graphics.Bitmap fCoverBitmap;

	/** Background thread for PixelCopy callback (avoids main-thread deadlock). */
	private android.os.HandlerThread fPixelCopyThread;
	private android.os.Handler fPixelCopyHandler;

	/**
	 * Creates a new surface view for bgfx rendering.
	 * @param context Reference to the context. Cannot be null.
	 */
	public CoronaBgfxSurfaceView(android.content.Context context, com.ansca.corona.CoronaRuntime runtime,
			boolean isCoronaKit, boolean wantsDepthBuffer, boolean wantsStencilBuffer) {
		super(context);

		if (context == null) {
			throw new NullPointerException();
		}

		fCoronaRuntime = runtime;
		fIsCoronaKit = isCoronaKit;
		fCanRender = false;
		fSurfaceValid = false;
		fUiHandler = new android.os.Handler(android.os.Looper.getMainLooper());

		// Fetch the current orientation of the activity's window.
		fCurrentWindowOrientation = com.ansca.corona.WindowOrientation.fromCurrentWindowUsing(getContext());
		fPreviousWindowOrientation = fCurrentWindowOrientation;

		// Set up a watchdog timer.
		fWatchdogTimer = new com.ansca.corona.MessageBasedTimer();
		fWatchdogTimer.setHandler(new android.os.Handler());
		fWatchdogTimer.setInterval(com.ansca.corona.TimeSpan.fromSeconds(1));
		fWatchdogTimer.setListener(new com.ansca.corona.MessageBasedTimer.Listener() {
			@Override
			public void onTimerElapsed() {
				android.view.SurfaceHolder holder = getHolder();
				if ((holder == null) || (holder.getSurface() == null)) {
					return;
				}
				if (fSurfaceValid && fCanRender) {
					return;
				}
				// Surface should exist but rendering state is inconsistent; re-trigger surfaceChanged.
				surfaceChanged(holder, android.graphics.PixelFormat.RGBA_8888, getWidth(), getHeight());
			}
		});

		// Set up the orientation listener.
		fCurrentDeviceOrientation = fCurrentWindowOrientation;
		fPreviousDeviceOrientation = com.ansca.corona.WindowOrientation.UNKNOWN;
		fOrientationListener = new android.view.OrientationEventListener(getContext()) {
			@Override
			public void onOrientationChanged(int orientationInDegrees) {
				if (fActivityInfo == null) {
					return;
				}
				if (orientationInDegrees == android.view.OrientationEventListener.ORIENTATION_UNKNOWN) {
					return;
				}
				if ((fCoronaRuntime.isRunning() == false) || (fCanRender == false)) {
					return;
				}

				orientationInDegrees = (360 - orientationInDegrees) % 360;

				com.ansca.corona.WindowOrientation currentOrientation =
						com.ansca.corona.WindowOrientation.fromDegrees(getContext(), orientationInDegrees);
				if ((currentOrientation != fCurrentDeviceOrientation) ||
					(fPreviousDeviceOrientation == com.ansca.corona.WindowOrientation.UNKNOWN)) {
					fPreviousDeviceOrientation = fCurrentDeviceOrientation;
					fCurrentDeviceOrientation = currentOrientation;
					if (fActivityInfo.hasFixedOrientation() &&
						android.provider.Settings.System.getInt(fActivity.getContentResolver(),
							android.provider.Settings.System.ACCELEROMETER_ROTATION, 0) != 0) {
						sendOrientationChangedEvent();
					}
				}
			}
		};

		// Register for surface callbacks. NO EGL setup — bgfx handles that.
		getHolder().addCallback(this);
		getHolder().setFormat(android.graphics.PixelFormat.RGBA_8888);

		// Set up Choreographer for VSync-bound rendering.
		fFrameCallback = new android.view.Choreographer.FrameCallback() {
			@Override
			public void doFrame(long frameTimeNanos) {
				synchronized (fRenderSignal) {
					fFrameScheduled = false;
					fVSyncTriggered = true;
					fRenderSignal.notifyAll();
				}
			}
		};
		fPostFrameCallbackRunnable = new Runnable() {
			@Override
			public void run() {
				if (fChoreographer == null) {
					fChoreographer = android.view.Choreographer.getInstance();
				}
				synchronized (fRenderSignal) {
					if (!fFrameScheduled) {
						return;
					}
				}
				fChoreographer.postFrameCallback(fFrameCallback);
			}
		};

		// Create and start the render thread.
		fRenderThread = new RenderThread();
		fRenderThread.start();
	}

	@Override
	protected void onAttachedToWindow() {
		super.onAttachedToWindow();
		Log.i(TAG, "onAttachedToWindow: width=" + getWidth() + " height=" + getHeight() + " visibility=" + getVisibility());
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		super.onSizeChanged(w, h, oldw, oldh);
		Log.i(TAG, "onSizeChanged: " + w + "x" + h + " (was " + oldw + "x" + oldh + ")");
	}

	public void setActivity(android.app.Activity activity) {
		fActivity = activity;
		fActivityInfo = new com.ansca.corona.CoronaActivityInfo(fActivity);
	}

	@Override
	public void onConfigurationChanged(android.content.res.Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		if (fActivityInfo == null) {
			return;
		}

		if (fOrientationListener.canDetectOrientation() == false) {
			com.ansca.corona.WindowOrientation currentOrientation;
			if (newConfig.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE) {
				currentOrientation = com.ansca.corona.WindowOrientation.LANDSCAPE_RIGHT;
			} else {
				currentOrientation = com.ansca.corona.WindowOrientation.PORTRAIT_UPRIGHT;
			}
			if ((currentOrientation != fCurrentDeviceOrientation) ||
				(fPreviousDeviceOrientation == com.ansca.corona.WindowOrientation.UNKNOWN)) {
				fPreviousDeviceOrientation = fCurrentDeviceOrientation;
				fCurrentDeviceOrientation = currentOrientation;
				if (fActivityInfo.hasFixedOrientation() &&
					android.provider.Settings.System.getInt(fActivity.getContentResolver(),
						android.provider.Settings.System.ACCELEROMETER_ROTATION, 0) != 0) {
					sendOrientationChangedEvent();
				}
			}
		}
	}

	private void sendOrientationChangedEvent() {
		if (fCoronaRuntime != null && fCoronaRuntime.isRunning()) {
			fCoronaRuntime.getTaskDispatcher().send(
				new com.ansca.corona.events.OrientationTask(
					fCurrentDeviceOrientation.toCoronaIntegerId(),
					fPreviousDeviceOrientation.toCoronaIntegerId()));
		}
	}

	// --- SurfaceHolder.Callback implementation ---

	@Override
	public void surfaceCreated(android.view.SurfaceHolder holder) {
		Log.i(TAG, "surfaceCreated (firstSurface=" + sFirstSurface + ", restored=" + !sFirstSurface + ")");
		fSurfaceValid = true;

		// Re-draw what was last rendered if the surface has been replaced.
		// This can happen when the user leaves and returns to the activity.
		if (!sFirstSurface) {
			fSurfaceRestored = true;
			setNeedsSwap();
		}
		sFirstSurface = false;

		// Unload textures from the previous context (forces reload for the new surface).
		// Must run on the render thread, same as the GL version's onSurfaceCreated.
		queueEvent(new Runnable() {
			@Override
			public void run() {
				com.ansca.corona.JavaToNativeShim.unloadResources(fCoronaRuntime);
			}
		});
	}

	@Override
	public void surfaceChanged(android.view.SurfaceHolder holder, int format, int width, int height) {
		if ((holder == null) || (holder.getSurface() == null) || (holder.getSurface().isValid() == false)) {
			return;
		}

		Log.i(TAG, "surfaceChanged: " + width + "x" + height);

		// Track orientation.
		com.ansca.corona.WindowOrientation currentOrientation;
		currentOrientation = com.ansca.corona.WindowOrientation.fromCurrentWindowUsing(getContext());

		boolean isCurrentOrientationSupported = fActivity == null ? true : currentOrientation.isSupportedBy(fActivity);

		if (isCurrentOrientationSupported && (fCurrentWindowOrientation != currentOrientation)) {
			fPreviousWindowOrientation = fCurrentWindowOrientation;
			fCurrentWindowOrientation = currentOrientation;
		}

		// Queue the surface setup + resize on the render thread.
		final android.view.Surface surface = holder.getSurface();
		final int w = width;
		final int h = height;
		final com.ansca.corona.WindowOrientation winOrientation = fCurrentWindowOrientation;
		final com.ansca.corona.WindowOrientation prevOrientation = fPreviousWindowOrientation;

		// Latch used to block the UI thread until the render thread has committed the first frame
		// to the new surface. This prevents the Android compositor from displaying a black frame
		// when it acquires the surface before any content has been rendered into it.
		final java.util.concurrent.CountDownLatch latch = new java.util.concurrent.CountDownLatch(1);

		queueEvent(new Runnable() {
			@Override
			public void run() {
				// Guard: if surfaceDestroyed fired before this runnable ran, skip.
				// This can happen during the initial window visibility transition where
				// surfaceCreated+surfaceChanged fire, then surfaceDestroyed fires before
				// the render thread processes the queued event.
				if (!fSurfaceValid) {
					Log.w(TAG, "surfaceChanged event skipped: surface already destroyed");
					latch.countDown();
					return;
				}

				// Pass the Surface to native for bgfx before resize.
				com.ansca.corona.JavaToNativeShim.setSurface(fCoronaRuntime, surface);

				com.ansca.corona.WindowOrientation orientation = winOrientation;
				if (fIsCoronaKit) {
					orientation = com.ansca.corona.WindowOrientation.PORTRAIT_UPRIGHT;
				} else if ((orientation.isPortrait() && (w > h)) ||
						   (orientation.isLandscape() && (w < h))) {
					fCanRender = false;
					latch.countDown();
					return;
				}

				// Initialize/resize Corona.
				com.ansca.corona.JavaToNativeShim.resize(
					fCoronaRuntime, getContext(), w, h, orientation, fIsCoronaKit);

				fCanRender = true;

				// Handle orientation events (same logic as GL renderer).
				if (fLastReceivedWindowOrientation == com.ansca.corona.WindowOrientation.UNKNOWN) {
					fLastReceivedWindowOrientation = orientation;
				} else if (fLastReceivedWindowOrientation != orientation) {
					fLastReceivedWindowOrientation = orientation;
					if (fCoronaRuntime != null) {
						fCoronaRuntime.getTaskDispatcher().send(
							new com.ansca.corona.events.OrientationTask(
								orientation.toCoronaIntegerId(),
								prevOrientation.toCoronaIntegerId()));
					}
				}

				// Handle resize events.
				if ((fLastViewWidth >= 0) && (fLastViewHeight >= 0) &&
					((fLastViewWidth != w) || (fLastViewHeight != h))) {
					if (fCoronaRuntime != null) {
						fCoronaRuntime.getTaskDispatcher().send(
							new com.ansca.corona.events.ResizeTask());
					}
				}
				fLastViewWidth = w;
				fLastViewHeight = h;

				// Force-render one frame after surface restoration.
				// The cover ImageView hides the surface during recreation, so we only
				// need one render to get content onto the new surface. The cover is
				// removed once this frame is committed.
				if (fSurfaceRestored) {
					Log.i(TAG, "Surface restored: force-rendering one frame");
					fSurfaceRestored = false;
					fRenderRequested = false;
					com.ansca.corona.Controller.updateRuntimeState(fCoronaRuntime, true);
					hideCover();
				}

				latch.countDown();
			}
		});

		// Block the UI thread briefly until the render thread sets up the surface.
		// The cover ImageView prevents black frames, so a short timeout is sufficient.
		// 100 ms timeout prevents a deadlock if the render thread is stuck.
		try {
			latch.await(100, java.util.concurrent.TimeUnit.MILLISECONDS);
		} catch (InterruptedException e) {
			Log.w(TAG, "surfaceChanged latch interrupted");
		}
	}

	@Override
	public void surfaceDestroyed(android.view.SurfaceHolder holder) {
		Log.i(TAG, "surfaceDestroyed");
		showCover();
		fSurfaceValid = false;
		fCanRender = false;
		fSurfaceRestored = false;
		fWatchdogTimer.stop();

		// Do NOT clear the event queue here. Queued Runnables (e.g. from surfaceChanged)
		// already guard on fSurfaceValid and skip if the surface is gone. Clearing
		// the queue risks dropping the Controller.stop() Runnable, which causes a
		// 4-second deadlock → Process.killProcess → app restart instead of resume.

		// Notify native layer that the surface is gone so it can clear fNativeWindow.
		// This prevents bgfx from accessing an abandoned ANativeWindow.
		com.ansca.corona.JavaToNativeShim.setSurface(fCoronaRuntime, null);
	}

	// --- State tracking for orientation/resize (used in surfaceChanged runnable) ---
	private com.ansca.corona.WindowOrientation fLastReceivedWindowOrientation =
		com.ansca.corona.WindowOrientation.UNKNOWN;
	private int fLastViewWidth = -1;
	private int fLastViewHeight = -1;
	private static boolean sFirstSurface = true;

	// --- Public API matching CoronaGLSurfaceView ---

	public void onResumeCoronaRuntime() {
		fWatchdogTimer.start();
		if (fOrientationListener.canDetectOrientation()) {
			fOrientationListener.enable();
		}
	}

	public void onSuspendCoronaRuntime() {
		fWatchdogTimer.stop();
		if (fOrientationListener.canDetectOrientation()) {
			fOrientationListener.disable();
		}
	}

	public void clearFirstSurface() {
		sFirstSurface = true;
	}

	public boolean canRender() {
		return fCanRender && fSurfaceValid;
	}

	private boolean canRenderFrameNow() {
		return fCanRender && fRenderRequested && (fVSyncTriggered || fSurfaceRestored) && (!fPaused || fSurfaceRestored);
	}

	/** Request a render frame (equivalent to GLSurfaceView.requestRender). */
	public void requestRender() {
		boolean shouldScheduleFrame = false;
		synchronized (fRenderSignal) {
			fRenderRequested = true;
			if (!fFrameScheduled) {
				fFrameScheduled = true;
				shouldScheduleFrame = true;
			}
			fRenderSignal.notifyAll();
		}
		if (shouldScheduleFrame) {
			if (android.os.Looper.myLooper() == android.os.Looper.getMainLooper()) {
				fPostFrameCallbackRunnable.run();
			} else {
				fUiHandler.post(fPostFrameCallbackRunnable);
			}
		}
	}

	/** Mark that a swap is needed. */
	public void setNeedsSwap() {
		fNeedsSwap = true;
		requestRender();
	}

	public void clearNeedsSwap() {
		fNeedsSwap = false;
	}

	/** Queue a runnable to execute on the render thread. */
	public void queueEvent(Runnable r) {
		if (r == null) {
			throw new IllegalArgumentException("r must not be null");
		}
		synchronized (fEventQueue) {
			fEventQueue.add(r);
		}
		synchronized (fRenderSignal) {
			fRenderSignal.notifyAll();
		}
	}

	/**
	 * Captures the current surface content to a bitmap via PixelCopy (API 24+).
	 * Called from onPause while the surface is still valid. The bitmap is stored
	 * for later use as a cover during surface recreation.
	 */
	private void captureSurfaceCover() {
		if (!fSurfaceValid) return;
		int w = getWidth(), h = getHeight();
		if (w <= 0 || h <= 0) return;

		if (fPixelCopyThread == null) {
			fPixelCopyThread = new android.os.HandlerThread("PixelCopy");
			fPixelCopyThread.start();
			fPixelCopyHandler = new android.os.Handler(fPixelCopyThread.getLooper());
		}

		final android.graphics.Bitmap bitmap = android.graphics.Bitmap.createBitmap(
			w, h, android.graphics.Bitmap.Config.ARGB_8888);
		final java.util.concurrent.CountDownLatch latch = new java.util.concurrent.CountDownLatch(1);

		try {
			android.view.PixelCopy.request(this, bitmap, new android.view.PixelCopy.OnPixelCopyFinishedListener() {
				@Override
				public void onPixelCopyFinished(int result) {
					if (result == android.view.PixelCopy.SUCCESS) {
						recycleCoverBitmap();
						fCoverBitmap = bitmap;
					} else {
						bitmap.recycle();
						Log.w(TAG, "PixelCopy failed: result=" + result);
					}
					latch.countDown();
				}
			}, fPixelCopyHandler);
			latch.await(100, java.util.concurrent.TimeUnit.MILLISECONDS);
		} catch (Exception e) {
			Log.w(TAG, "PixelCopy exception: " + e);
			if (fCoverBitmap != bitmap) {
				bitmap.recycle();
			}
		}
	}

	/** Shows the cover ImageView over the surface. Must be called on the UI thread. */
	private void showCover() {
		if (fCoverBitmap == null) return;
		if (fCoverView == null) {
			fCoverView = new android.widget.ImageView(getContext());
			fCoverView.setScaleType(android.widget.ImageView.ScaleType.FIT_XY);
		}
		android.view.ViewGroup parent = (android.view.ViewGroup) getParent();
		if (parent == null) return;

		fCoverView.setImageBitmap(fCoverBitmap);
		if (fCoverView.getParent() == null) {
			int surfaceIndex = parent.indexOfChild(this);
			parent.addView(fCoverView, surfaceIndex + 1,
				new android.widget.FrameLayout.LayoutParams(
					android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
					android.widget.FrameLayout.LayoutParams.MATCH_PARENT));
		}
		fCoverView.setVisibility(android.view.View.VISIBLE);
		Log.i(TAG, "Cover shown");
	}

	/** Hides the cover and recycles the bitmap. Safe to call from any thread. */
	private void hideCover() {
		fUiHandler.post(new Runnable() {
			@Override
			public void run() {
				if (fCoverView != null) {
					fCoverView.setVisibility(android.view.View.GONE);
					fCoverView.setImageBitmap(null);
					Log.i(TAG, "Cover hidden");
				}
				recycleCoverBitmap();
			}
		});
	}

	private void recycleCoverBitmap() {
		if (fCoverBitmap != null && !fCoverBitmap.isRecycled()) {
			fCoverBitmap.recycle();
			fCoverBitmap = null;
		}
	}

	/** Pause the render thread. */
	public void onPause() {
		captureSurfaceCover();
		fPaused = true;
	}

	/** Resume the render thread. */
	public void onResume() {
		fPaused = false;
		synchronized (fRenderSignal) {
			fRenderSignal.notifyAll();
		}
	}

	@Override
	public android.view.View asView() {
		return this;
	}

	/** Request the render thread to exit and wait for it to finish. */
	public void requestExitAndWait() {
		if (fRenderThread != null) {
			fRenderThread.requestExit();
			synchronized (fRenderSignal) {
				fRenderSignal.notifyAll();
			}
			try {
				fRenderThread.join(5000);
			} catch (InterruptedException e) {
				Log.w(TAG, "Interrupted waiting for render thread to exit");
			}
			fRenderThread = null;
		}
		if (fPixelCopyThread != null) {
			fPixelCopyThread.quitSafely();
			fPixelCopyThread = null;
			fPixelCopyHandler = null;
		}
		recycleCoverBitmap();
	}

	/**
	 * Render thread for bgfx mode.
	 * This thread runs the Corona render loop (updateRuntimeState) without any EGL management.
	 * bgfx's internal thread handles all GPU operations.
	 */
	private class RenderThread extends Thread {
		private volatile boolean fExitRequested = false;

		RenderThread() {
			super("CoronaBgfxRenderThread");
		}

		void requestExit() {
			fExitRequested = true;
		}

		@Override
		public void run() {
			Log.i(TAG, "RenderThread started");

			while (!fExitRequested) {
				// Process queued events.
				java.util.ArrayList<Runnable> pendingEvents = null;
				synchronized (fEventQueue) {
					if (!fEventQueue.isEmpty()) {
						pendingEvents = new java.util.ArrayList<>(fEventQueue);
						fEventQueue.clear();
					}
				}
				if (pendingEvents != null) {
					for (Runnable r : pendingEvents) {
						r.run();
					}
				}

				// Render if surface is ready and render requested.
				// Normally requires !fPaused, but fSurfaceRestored bypasses the pause check
				// to force-render the first frame after lock-screen resume (onResume may lag).
				// VSync trigger ensures at most one frame per display refresh.
				if (canRenderFrameNow()) {
					if (fSurfaceRestored) {
						Log.i(TAG, "RenderThread: rendering with surface-restored bypass");
						fSurfaceRestored = false;
					}
					fVSyncTriggered = false;
					fRenderRequested = false;
					com.ansca.corona.Controller.updateRuntimeState(fCoronaRuntime, true);
				}

				// Wait for next event or VSync trigger.
				synchronized (fRenderSignal) {
					try {
						while (!fExitRequested) {
							boolean hasPendingWork;
							synchronized (fEventQueue) {
								hasPendingWork = !fEventQueue.isEmpty();
							}
							if (hasPendingWork || canRenderFrameNow()) {
								break;
							}
							fRenderSignal.wait();
						}
					} catch (InterruptedException e) {
						// Continue loop.
					}
				}
			}

			Log.i(TAG, "RenderThread exited");
		}
	}
}
