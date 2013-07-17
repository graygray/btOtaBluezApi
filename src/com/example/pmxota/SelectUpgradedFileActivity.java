package com.example.pmxota;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ListView;
import android.widget.TextView;

public class SelectUpgradedFileActivity  extends ListActivity {
	
	
	private String TAG = "SelectUpgradedFileActivity";
	private File currentDir;
	private FileArrayAdapter adapter;
	List<Option>fls = new ArrayList<Option>();
	private TextView tvPath;
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);
	    setContentView(R.layout.activity_select_file);
	    
	    currentDir = new File("/mnt/");
	    ListView listView = getListView();
	    tvPath = (TextView) findViewById(R.id.tvPath);
	    
	    adapter = new FileArrayAdapter(SelectUpgradedFileActivity.this, R.layout.file_view, fls);
		//this.setListAdapter(adapter);
		listView.setAdapter(adapter);
	    searchBinFiles(currentDir);
		//Collections.sort(fls);

	}
	
	private void searchBinFiles(File f) {
		tvPath.setText(f.getAbsolutePath());
		fls.clear();
		if (!f.getAbsolutePath().equals("/mnt"))
			fls.add(new Option("..", f.getParentFile().getAbsolutePath(), f.length()));
		File[]dirs = f.listFiles();
		try {
			for(File ff: dirs) {
				//fls.add(new Option(ff.getName(), ff.getAbsolutePath(), ff.length()));
				if(ff.isDirectory()) {
					fls.add(new Option(ff.getName(), ff.getAbsolutePath(), ff.length()));
					//searchBinFiles(new File(ff.getAbsolutePath()));
				} else {
					if (ff.getName().contains(".img")) {
						fls.add(new Option(ff.getName(), ff.getAbsolutePath(), ff.length()));
					}
				}
			}
		} catch(Exception e) {
		}
		Collections.sort(fls);
		adapter.notifyDataSetChanged();
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		super.onListItemClick(l, v, position, id);
		
		Option o = adapter.getItem(position);
		onFileClick(o);
	}
	
	private void onFileClick(Option o) {
		File file = new File(o.getPath());
		if (file.isDirectory()) {
			searchBinFiles(file);
			return;
		}
		
		Intent intent = new Intent(SelectUpgradedFileActivity.this, OTAActivity.class);
		Bundle bundle = new Bundle();
		bundle.putString(OTAActivity.KEY_UPGRADED_FILE, o.getPath());
		intent.putExtras(bundle);
		setResult(RESULT_OK, intent);
		finish();
	}

	@Override
	public void onBackPressed() {
		super.onBackPressed();
		Intent intent = new Intent(SelectUpgradedFileActivity.this, OTAActivity.class);
		setResult(RESULT_CANCELED, intent);
		finish();
	}
	
	
	
	
	/*private String TAG = "SelectUpgradedFileActivity";
	private File currentDir;
	private FileArrayAdapter adapter;
	List<Option>fls = new ArrayList<Option>();
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);
	    currentDir = new File("/mnt/");
	    searchBinFiles(currentDir);
		Collections.sort(fls);
		adapter = new FileArrayAdapter(SelectUpgradedFileActivity.this, R.layout.file_view, fls);
		this.setListAdapter(adapter);
	}
	
	private void searchBinFiles(File f) {
		File[]dirs = f.listFiles();
		try {
			for(File ff: dirs) {
				if(ff.isDirectory()) {
					searchBinFiles(new File(ff.getAbsolutePath()));
				} else {
					if (ff.getName().contains(".img")) {
						fls.add(new Option(ff.getName(), ff.getAbsolutePath(), ff.length()));
					}
				}
			}
		} catch(Exception e) {
		}
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		super.onListItemClick(l, v, position, id);
		
		Option o = adapter.getItem(position);
		onFileClick(o);
	}
	
	private void onFileClick(Option o) {
		Intent intent = new Intent(SelectUpgradedFileActivity.this, OTAActivity.class);
		Bundle bundle = new Bundle();
		bundle.putString(OTAActivity.KEY_UPGRADED_FILE, o.getPath());
		intent.putExtras(bundle);
		setResult(RESULT_OK, intent);
		finish();
	}

	@Override
	public void onBackPressed() {
		super.onBackPressed();
		Intent intent = new Intent(SelectUpgradedFileActivity.this, OTAActivity.class);
		setResult(RESULT_CANCELED, intent);
		finish();
	}*/
	
}
