package com.example.pmxota;

import java.io.IOException;
import android.util.Log;

public class OtaLoadJNI {
	private static final String TAG = "===OtaLoadJNI===";
	//public static native String otaUpdateFromJni(String fwFilePath, String isVerboseLogS);
	static {
		try {
			Log.e(TAG, "start load library");
			System.loadLibrary("hidota");
			Log.e(TAG, "end load library");
		} catch (UnsatisfiedLinkError ule) {
			Log.e(TAG, "Error:" + ule);
			Log.e(TAG, "can not load library");
		}
	}
  
	/** 
	 * Setup the bluetooth HID socket to listen on incoming host connection
	 * should be called on construction 
	 * Takes as argument the bluetooth Address of the host
	 * 
	 * @return	0 if successful 
	 */
	public native int setupClientSocket();

	/**
	* Connect to the host
	* 
	* @return	0 if successful
	* @return	1 if not successful 
	*/
	public native int connectToHost(String hostAddress);

	/**
	 * Deinitialize the HID client and disconnects
	 * Should be called on destruction
	 * 
	 * @return	0 if successful
	 */
	public native int deconnectClient();

	/**
	 * Start OTA update
	 * 
	 * @return	0 if successful
	 * @param	fwFilePath: the path of OTA file
	 * @param	isVerboseLogS: 1 is show the verbose log; 0 is not show
	 */
	public native int startOTAUpdate(String fwFilePath, String isVerboseLogS);

	/**
	 * Check status of bluetooth adapter
	 * 
	 * @return true if Bluetooth is currently enabled and ready for use.
	 */
	public native boolean isEnabled();

	/**
	 * Detect remote bt devices in visible state
	 * 
	 * @return Array of Strings with detected bt device addresses
	 */
	public native String[] btDeviceScan();

	/**
	 * Get the human readable name of a bt device
	 * 
	 * @return	Human readable name of bt device
	 * @param		bluetooth mac address
	 */
	public native String btDeviceGetName(String btAddress);
}
