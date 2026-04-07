//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

package com.ansca.corona.graphics.opengl;

/**
 * Common interface for Corona surface views (GL and bgfx backends).
 * Both CoronaGLSurfaceView and CoronaBgfxSurfaceView implement this interface,
 * allowing the rest of the engine to work with either backend interchangeably.
 */
public interface CoronaSurfaceViewInterface {
	/** Set the owning activity. */
	void setActivity(android.app.Activity activity);

	/** Called when the Corona runtime has been started or resumed. */
	void onResumeCoronaRuntime();

	/** Called when the Corona runtime has been suspended or exited. */
	void onSuspendCoronaRuntime();

	/** Informs the rendering system to clear the first rendered frame. */
	void clearFirstSurface();

	/** Determines if this surface can currently render. */
	boolean canRender();

	/** Request a render frame. */
	void requestRender();

	/** Mark that a swap/redraw is needed. */
	void setNeedsSwap();

	/** Queue a runnable to execute on the render thread. */
	void queueEvent(Runnable r);

	/** Pause rendering. */
	void onPause();

	/** Resume rendering. */
	void onResume();

	/** Request the render thread to exit and wait for it. */
	void requestExitAndWait();

	/** Get the underlying Android View. */
	android.view.View asView();

	/** Get the SurfaceHolder. */
	android.view.SurfaceHolder getHolder();

	/** Get system UI visibility flags. */
	int getSystemUiVisibility();

	/** Set system UI visibility flags. */
	void setSystemUiVisibility(int visibility);

	/** Get parent view. */
	android.view.ViewParent getParent();
}
