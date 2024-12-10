//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

package com.ansca.corona.events;


/** Provides the result of the image picker window to be passed over to the C++ side of Corona. */
public class ImagePickerTask extends MediaPickerTask {
	protected int fMultipleFilesCount;

	public ImagePickerTask() {
		super();
		fMultipleFilesCount = 0;
	}

	public ImagePickerTask(String selectedFileName) {
		super(selectedFileName);
		fMultipleFilesCount = 0;
	}

	public ImagePickerTask(String selectedFileName, int multipleFilesCount) {
		super(selectedFileName);
		fMultipleFilesCount = multipleFilesCount;
	}

	/**
	 * Sends this event's data to Corona.
	 */
	@Override
	public void executeUsing(com.ansca.corona.CoronaRuntime runtime) {
		com.ansca.corona.JavaToNativeShim.imagePickerEvent(runtime, fSelectedMediaFileName, fMultipleFilesCount);
	}

}
