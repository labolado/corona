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

	/** Queue of runnables to execute on the render thread. */
	private final java.util.ArrayList<Runnable> fEventQueue = new java.util.ArrayList<>();

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

		// Create and start the render thread.
		fRenderThread = new RenderThread();
		fRenderThread.start();
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
		Log.i(TAG, "surfaceCreated");
		fSurfaceValid = true;

		// Re-draw what was last rendered if the surface has been replaced.
		// This can happen when the user leaves and returns to the activity.
		if (!sFirstSurface) {
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

		queueEvent(new Runnable() {
			@Override
			public void run() {
				// Pass the Surface to native for bgfx before resize.
				com.ansca.corona.JavaToNativeShim.setSurface(fCoronaRuntime, surface);

				com.ansca.corona.WindowOrientation orientation = winOrientation;
				if (fIsCoronaKit) {
					orientation = com.ansca.corona.WindowOrientation.PORTRAIT_UPRIGHT;
				} else if ((orientation.isPortrait() && (w > h)) ||
						   (orientation.isLandscape() && (w < h))) {
					fCanRender = false;
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
			}
		});
	}

	@Override
	public void surfaceDestroyed(android.view.SurfaceHolder holder) {
		Log.i(TAG, "surfaceDestroyed");
		fSurfaceValid = false;
		fCanRender = false;
		fWatchdogTimer.stop();
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

	/** Request a render frame (equivalent to GLSurfaceView.requestRender). */
	public void requestRender() {
		fRenderRequested = true;
		synchronized (fRenderThread) {
			fRenderThread.notifyAll();
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
		synchronized (fRenderThread) {
			fRenderThread.notifyAll();
		}
	}

	/** Pause the render thread. */
	public void onPause() {
		fPaused = true;
	}

	/** Resume the render thread. */
	public void onResume() {
		fPaused = false;
		synchronized (fRenderThread) {
			fRenderThread.notifyAll();
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
			synchronized (fRenderThread) {
				fRenderThread.notifyAll();
			}
			try {
				fRenderThread.join(5000);
			} catch (InterruptedException e) {
				Log.w(TAG, "Interrupted waiting for render thread to exit");
			}
			fRenderThread = null;
		}
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

				// Render if not paused, render requested, and surface is ready.
				if (!fPaused && fCanRender && fRenderRequested) {
					fRenderRequested = false;
					com.ansca.corona.Controller.updateRuntimeState(fCoronaRuntime, true);
				}

				// Wait for next event or render request.
				synchronized (this) {
					try {
						// Don't wait if there are pending events or render requests.
						boolean hasPendingWork;
						synchronized (fEventQueue) {
							hasPendingWork = !fEventQueue.isEmpty();
						}
						if (!hasPendingWork && !fRenderRequested && !fExitRequested) {
							this.wait(16); // ~60fps max poll rate
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
