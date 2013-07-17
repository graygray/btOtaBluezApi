package com.example.pmxota;

public class BtDevice {
	public String humanReadableName;
	public String btAddress;
	
	public BtDevice( String btAddr ) {
		this.btAddress = btAddr;
		this.humanReadableName = "[unknown]";
	}
	
	public BtDevice( String btAddr, String humanName ) {
		this.humanReadableName = humanName;
		this.btAddress = btAddr;
	}
}
