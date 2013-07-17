package com.example.pmxota;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

public class OTAActivity extends Activity {
	public static OtaLoadJNI  otaLoadJni;
	private static boolean isOTARun = false;
	public static String fwFilePath = "";
	public static String deviceName = "";
	public static String deviceMac = "";
	public static boolean isVerboseLog;
	public static final String PREFS_NAME = "OTA_PrefsFile";
	public static final String KEY_UPGRADED_FILE = "KeyOTAUpgradeFile";
	public static final String KEY_DEVICE_MAC = "KeyDeviceMac";
	public static final String KEY_DEVICE_NAME = "KeyDeviceName";
	public static final int MEG_UPDATEEDITTEXT = 9527;
	public static final int MEG_UPDATETEXTVIEW_RED = 9528;
	public static final int MEG_UPDATETEXTVIEW_BLUE = 9529;
	private static final int SELECT_UPDADED_FILE = 0;
	private static final int PICK_DEVICE = 1;
	private static String resultTmp = "";
	private String TAG = "===OTAActivity===";
	
	TextView tvUpgradedFilePath;
	TextView tvRCMac;
	TextView tvResult;
	Button btnSelectUpgradedFile;
	Button button_start;
	Button button_clean;
	Button btnDevicePicker;
	Button btnConnect;
	Button btnDisconnect;
	ToggleButton tbVerboseLog;
	EditText editText_result;
	

	Handler handler = new Handler() {
		public void handleMessage(Message msg) {
			switch (msg.what) {
				case MEG_UPDATEEDITTEXT:
					if (isVerboseLog) {
						editText_result.append((String)msg.obj);
					} else {
						editText_result.setText((String)msg.obj);
						editText_result.setSelection(editText_result.getText().length());
					}
					break;
				case MEG_UPDATETEXTVIEW_RED:	// update "tvResult" TextView info (RED color)
					tvResult.setTextColor(Color.RED);
					tvResult.setText((String)msg.obj);
					break;
				case MEG_UPDATETEXTVIEW_BLUE:	// update "tvResult" TextView info (BLUE color)
					tvResult.setTextColor(Color.BLUE);
					tvResult.setText((String)msg.obj);
					break;
			}
			super.handleMessage(msg);
		}
	};
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_ota);
		
		otaLoadJni = new OtaLoadJNI();
		isOTARun = false;	
				
		editText_result = (EditText) findViewById(R.id.editText_result);
		tvUpgradedFilePath = (TextView) findViewById(R.id.tvUpgradedFilePath);
		tvRCMac = (TextView) findViewById(R.id.tvRCMac);
		tvResult = (TextView) findViewById(R.id.tvResult);
		
		//select an OTA file
		btnSelectUpgradedFile = (Button) findViewById(R.id.btnSelectUpgradedFile);
		btnSelectUpgradedFile.setOnClickListener(new View.OnClickListener() {
	        public void onClick(View v) {
				Intent intent = new Intent(OTAActivity.this, SelectUpgradedFileActivity.class);
				startActivityForResult(intent, SELECT_UPDADED_FILE);
			}
	    });

		// start OTA
		button_start = (Button) findViewById(R.id.button_start);
		button_start.setOnClickListener(new View.OnClickListener() {
	        public void onClick(View v) {
				if (fwFilePath.equals("")) {
					Toast.makeText(getApplicationContext(), R.string.err_msg_no_select_upgraded_file, Toast.LENGTH_SHORT).show();
					return;
				}
				
				if(isOTARun == false){
					isOTARun = true;
				} else {
					Toast.makeText(getApplicationContext(), R.string.msg_ota_is_run, Toast.LENGTH_SHORT).show();
					return;
				}
				
	        	tvResult.setText("");
	    		editText_result.setText("");
	    		resultTmp = "";
	        	new Thread(new Runnable() {
	        		public void run() {
	        			try {
							try {
								//clear (flush) the entire log and exit 
								Runtime.getRuntime().exec("logcat -c -v raw ====OTAUpdate====:I *:S");
								Thread.sleep(100);
							} catch (InterruptedException e) {
								throw new RuntimeException(e);
							}
							
							new Thread(new Runnable() {
								public void run() {
									String isVerboseLogS = isVerboseLog ? "Y" : "N";
									otaLoadJni.startOTAUpdate(fwFilePath, isVerboseLogS);
								}
							}).start();
							
							String temp;
	        				Process process = Runtime.getRuntime().exec("logcat -v raw ====OTAUpdate====:I *:S");
	            			BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
							try {
								while (true) {
									temp = reader.readLine();
									if (temp == null) {
										break;
									}
									temp += "\n";
									
									if (temp.contains("OTA STOP @@======================================")) {
										process.destroy();
										break;
									}
									//temp += "\n";
									
									if (temp.contains("--------- beginning of /dev/log/main") || temp.contains("--------- beginning of /dev/log/system"))
										continue;
									
									Message m = new Message();
									m.what = MEG_UPDATEEDITTEXT;
									if (isVerboseLog) {
										m.obj = temp;
									} else {
										m.obj = resultTmp+temp;
									}
									handler.sendMessage(m);
									
									if (temp.contains("write data ... 100%") || !temp.contains("write data ... ")) {
										if (!isVerboseLog) {
											resultTmp = resultTmp + temp;
										}
									}
									if (temp.contains("OTA fail ~~======================================")) {
										m = new Message();
										m.what = MEG_UPDATETEXTVIEW_RED;
										m.obj =  getString(R.string.msg_result_fail);
										handler.sendMessage(m);
									} else if (temp.contains("OTA end  !! ======================================")) {
										m = new Message();
										m.what = MEG_UPDATETEXTVIEW_BLUE;
										m.obj =  getString(R.string.msg_result_success);
										handler.sendMessage(m);
									}
									Thread.sleep(100);
								}
							} catch (InterruptedException e) {
								throw new RuntimeException(e);
							} catch (IOException e1) {
								e1.printStackTrace();
							}
							temp = "OTA stop!!\n";
        					Message m = new Message();
							m.what = MEG_UPDATEEDITTEXT;
							if (isVerboseLog) {
								m.obj = temp;
							} else {
								m.obj = resultTmp+temp;
							}
							handler.sendMessage(m);
							
							isOTARun = false;

						} catch (IOException e) {
							Log.e(TAG, "SimpleAndroidApp.java: "	+ "IOException:" + e);
							throw new RuntimeException(e);
						} 

             		 }
 				}).start();
	        }
	    });
		
		// clean log
		button_clean = (Button) findViewById(R.id.button_clean);
		button_clean.setOnClickListener(new View.OnClickListener() {
	        public void onClick(View v) {
				tvResult.setText("");
	    		editText_result.setText("");
	    		resultTmp = "";
	        }
	    });
	    
	    tbVerboseLog = (ToggleButton) findViewById(R.id.tbVerboseLog);
		tbVerboseLog.setOnCheckedChangeListener(new OnCheckedChangeListener() {
			@Override
			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				if (isChecked) {
					isVerboseLog = true;	// Verbose log Open
				} else {
					isVerboseLog = false;	// Verbose log Close
				}
			}
		});
		
		// pick a device to update firmware 
		btnDevicePicker = (Button) findViewById(R.id.btnDevicePicker);
		btnDevicePicker.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				Intent intent = new Intent(OTAActivity.this, BtDevicePicker.class);
				startActivityForResult(intent, PICK_DEVICE);
			}
		});
		// connect the picked device
		btnConnect = (Button) findViewById(R.id.btnConnect);
		btnConnect.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				new Thread(new Runnable() {
	        		public void run() {
	        			try {
							int connectionStatus = otaLoadJni.connectToHost(deviceMac);
							Log.d(TAG, "connectionStatus="+connectionStatus);
						} catch (Exception connectionFailed){
							Log.d(TAG, "connectionFailed="+connectionFailed.getMessage());
						}
             		 }
 				}).start();
				
			}
		});
		// disconnect the picked device 
		btnDisconnect = (Button) findViewById(R.id.btnDisconnect);
		btnDisconnect.setOnClickListener(new View.OnClickListener () {
			public void onClick(View v) {
				otaLoadJni.deconnectClient();
			}
		});
	}
	
	/*@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.activity_ota, menu);
		return true;
	}*/

	/*
	 * When OTA firmware upgrade in process, application will not be closed.
	 */
	@Override
	public void onBackPressed() {
		if(isOTARun == false){
			super.onBackPressed();
		} else {
			Toast.makeText(getApplicationContext(), R.string.msg_ota_is_writing, Toast.LENGTH_SHORT).show();
			return;
		}
	}
	
	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		switch(requestCode){
			case SELECT_UPDADED_FILE:
				if (resultCode == RESULT_OK) {
					tvUpgradedFilePath.setText(data.getExtras().getString(KEY_UPGRADED_FILE));
					fwFilePath = data.getExtras().getString(KEY_UPGRADED_FILE);
				} else if (resultCode == RESULT_CANCELED) {
					tvUpgradedFilePath.setText("");
					fwFilePath = "";
				}
				break;
			case PICK_DEVICE:
				if (resultCode == RESULT_OK) {
					tvRCMac.setText(data.getExtras().getString(KEY_DEVICE_MAC));
					deviceName = data.getExtras().getString(KEY_DEVICE_NAME);
					deviceMac = data.getExtras().getString(KEY_DEVICE_MAC);
				} else if (resultCode == RESULT_CANCELED) {
					tvRCMac.setText("");
					deviceName = "";
					deviceMac = "";
				}
				break;
		}
	}
}
