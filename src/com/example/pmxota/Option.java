package com.example.pmxota;

public class Option implements Comparable<Option> {
	private String name;
	private String path;
	private long size;
	
	public Option(String n, String p, long s) {
		name = n;
		path = p;
		size = s;
	}
	
	public String getName() {
		return name;
	}
	
	public String getPath() {
		return path;
	}
	
	public long getSize() {
		return size;
	}
	
	@Override
	public int compareTo(Option o) {
		if(this.name != null)
			return this.name.toLowerCase().compareTo(o.getName().toLowerCase()); 
		else 
			throw new IllegalArgumentException();
	}
}
