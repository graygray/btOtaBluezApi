package com.example.pmxota;

import java.io.File;
import java.util.List;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

public class FileArrayAdapter extends ArrayAdapter<Option> {
	private Context c;
	private int id;
	private List<Option>items;
	private ImageView ivImg;
	private TextView tvName;
	//private TextView tvAbsolutePath;
	private TextView tvSize;
	
	public FileArrayAdapter(Context context, int textViewResourceId, List<Option> objects) {
		super(context, textViewResourceId, objects);
		c = context;
		id = textViewResourceId;
		items = objects;
	}
	
	public Option getItem(int i) {
		return items.get(i);
	}
	
	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		View v = convertView;
		if (v == null) {
			LayoutInflater vi = (LayoutInflater)c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			v = vi.inflate(id, null);
		}
		final Option o = items.get(position);
		if (o != null) {
			ivImg = (ImageView) v.findViewById(R.id.ivImg);
			tvName = (TextView) v.findViewById(R.id.tvName);
			//tvAbsolutePath = (TextView) v.findViewById(R.id.tvAbsolutePath);
			tvSize = (TextView) v.findViewById(R.id.tvSize);
			
			File file = new File(o.getPath());
			//Bitmap bitmap = getBitmapFromFile(file);
			
			if (file.isDirectory()) {
				ivImg.setImageResource(R.drawable.folder);
				
				if(tvName != null)
					tvName.setText(o.getName());
				/*if(tvAbsolutePath != null) {
					tvAbsolutePath.setVisibility(View.GONE);
					tvAbsolutePath.setText("");
				}*/
				if(tvSize != null) {
					tvSize.setText("");
				}
			} else {
				ivImg.setImageResource(R.drawable.img);
				
				if(tvName != null)
					tvName.setText(o.getName());
				/*if(tvAbsolutePath != null) {
					tvAbsolutePath.setVisibility(View.VISIBLE);
					tvAbsolutePath.setText("Path: " + o.getPath());
				}*/
				if(tvSize != null) {
					tvSize.setText("File Size: " + o.getSize() + " bytes");
				}
			}
		}
		return v;
	}
}
