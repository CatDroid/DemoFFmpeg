package com.tom.ffmpegAPI;

import android.media.AudioTrack;
import android.view.Surface;

public class Mp4player {

	private static final String TAG = "java_mp4player";
	private long mNativeContext = 0 ;
	public Mp4player(){
		
	}
	
	public void playmp4(String file , Surface surface , AudioTrack at)
	{
		native_playmp4(mNativeContext, file , surface , at);
	}
	
	public void stop(){
		native_stop(mNativeContext);
	}
	
	native public void native_playmp4(long ctx , String file , Surface surface , AudioTrack at);
	
	native public void native_stop(long ctx);
	
	
	static{
		System.loadLibrary("demo_ffmpeg");
	}
}
