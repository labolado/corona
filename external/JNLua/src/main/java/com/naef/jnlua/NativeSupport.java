/*
 * $Id: NativeSupport.java 121 2012-01-22 01:40:14Z andre@naef.com $
 * See LICENSE.txt for license terms.
 */

package com.naef.jnlua;

/**
 * Loads the JNLua native library.
 * 
 * The class provides and configures a default loader implementation that loads
 * the JNLua native library by means of the <code>System.loadLibrary</code>
 * method. In some situations, you may want to override this behavior. For
 * example, when using JNLua as an OSGi bundle, the native library is loaded by
 * the OSGi runtime. Therefore, the OSGi bundle activator replaces the loader by
 * a no-op implementaion. Note that the loader must be configured before
 * LuaState is accessed.
 */
public final class NativeSupport {
	// -- Static
	private static final NativeSupport INSTANCE = new NativeSupport();

	// Names of native libraries already loaded by loadLibraryOnce(), so a second
	// loadLibrary call for the same library is skipped. This avoids the Bionic
	// linker "recursive attempt to load library" info-level warning that appears
	// when both the SDK bootstrap and JNLua's DefaultLoader request the same .so.
	// Class-load ordering between the two paths is not guaranteed, so the guard
	// lives here -- the lowest common loader both of them can reach.
	private static final java.util.HashSet<String> sLoaded = new java.util.HashSet<String>();

	/**
	 * Loads a native library exactly once per process. Subsequent calls for the
	 * same name are no-ops. The name is recorded only after a successful load, so
	 * a failed load can be retried.
	 *
	 * @param libraryName
	 *            the System.loadLibrary name (without the "lib" prefix / ".so")
	 */
	public static synchronized void loadLibraryOnce(String libraryName) {
		if (sLoaded.contains(libraryName)) {
			return;
		}
		// Mark BEFORE loading. Loading libjnlua5.1.so triggers LuaState's static
		// initializer, which calls the loader again on the SAME thread while the
		// first load is still in progress -- a re-entrant call that the linker
		// flags as "recursive attempt to load library". Recording the name first
		// short-circuits that nested call (this method is synchronized and Java
		// monitors are reentrant, so the same-thread nested call proceeds and
		// returns immediately). Roll back if the load genuinely fails.
		sLoaded.add(libraryName);
		try {
			System.loadLibrary(libraryName);
		} catch (Throwable t) {
			sLoaded.remove(libraryName);
			throw t;
		}
	}

	// -- State
	private Loader loader = new DefaultLoader();

	/**
	 * Returns the instance.
	 * 
	 * @return the instance
	 */
	public static NativeSupport getInstance() {
		return INSTANCE;
	}

	// -- Construction
	/**
	 * Private constructor to prevent external instantiation.
	 */
	private NativeSupport() {
	}

	// -- Properties
	/**
	 * Return the native library loader.
	 * 
	 * @return the loader
	 */
	public Loader getLoader() {
		return loader;
	}

	/**
	 * Sets the native library loader.
	 * 
	 * @param loader
	 *            the loader
	 */
	public void setLoader(Loader loader) {
		if (loader == null) {
			throw new NullPointerException("loader must not be null");
		}
		this.loader = loader;
	}

	// -- Member types
	/**
	 * Loads the library.
	 */
	public interface Loader {
		public void load();
	}

	private class DefaultLoader implements Loader {
		@Override
		public void load() {
			loadLibraryOnce("jnlua5.1");
		}
	}
}