package com.catdroid.demo;

/**
 * Created by Hanloong Ho on 17-4-3.
 */

import com.catdroid.core.DragonPlayer;

import android.app.Activity;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

public class FFmpegActivity extends Activity {

	private static final String TAG = "FFmpegActivity";

	private SurfaceView mSv = null;
	private SurfaceHolder mSh = null;
	private boolean mSurfaceCreated = false;

	private Handler mUIhandler = new Handler();
	private DragonPlayer mPlayer = null;

	private void toastMessage(final String msg) {
		mUIhandler.post(new Runnable() {
			@Override
			public void run() {
				Toast.makeText(FFmpegActivity.this, msg, Toast.LENGTH_LONG).show();
			}
		});
	}
	private Activity getActivity(){
		return this;
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		mSv = (SurfaceView) findViewById(R.id.videoView);
		mSh = mSv.getHolder();
		mSh.addCallback(new SurfaceCallback());

		((Button) findViewById(R.id.bPlay)).setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {

			if (mSurfaceCreated) {
				if (mPlayer == null) {
					mPlayer = new DragonPlayer();
					mPlayer.setSurface(mSh.getSurface());
					//mPlayer.setDataSource("file:///mnt/sdcard/screen_mic_main_r30_g30_ultrafast_c2_44k_6000K_128K_yuv444.mp4");
					mPlayer.setDataSource("file:///mnt/sdcard/1080p60fps.mp4");
					mPlayer.setOnPreparedListener(new DragonPlayer.OnPreparedListener(){
						@Override
						public void onPrepared(DragonPlayer mp, int what) {
							Log.d(TAG,String.format("w:%d h:%d d:%d c:%d",
									mp.getVideoWidth(),
									mp.getVideoHeight(),
									mp.getDuration(),
									mp.getCurrentPosition()
									));

							mp.play();
						}
					});
					mPlayer.setOnCompletionListener(new DragonPlayer.OnCompletionListener() {
						@Override
						public void onCompletion(DragonPlayer mp) {
							String msg = "播放完毕";
							toastMessage(msg); Log.w(TAG, msg);
							mPlayer.stop(); mPlayer.release(); mPlayer = null;
						}
					});
					mPlayer.setOnErrorListener(new DragonPlayer.OnErrorListener(){

						@Override
						public boolean onError(DragonPlayer mp, int what, int extra) {
							String msg = "播放错误 : " + what ;
							toastMessage( msg); Log.e(TAG,msg);
							mPlayer.stop(); mPlayer.release(); mPlayer = null;
							return true ;
						}
					});
					mPlayer.prepareAsync();
				} else {
					toastMessage("请先关闭再重新启动");
				}
			} else {
				toastMessage("界面还没有准备好");
			}
			}
		});

		((Button) findViewById(R.id.bStop)).setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
			if (mPlayer != null) {
				mPlayer.stop();
				mPlayer.release();
				mPlayer = null;
			}else{
				toastMessage("播放器没有启动");
			}
			}
		});

	}

	private class SurfaceCallback implements SurfaceHolder.Callback {
		@Override
		public void surfaceCreated(SurfaceHolder arg0) {
			Log.d(TAG, "surfaceCreated created");
			mSurfaceCreated = true;
		}

		@Override
		public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
			Log.d(TAG, "surfaceChanged " + " format = " + format + " width = " + width + " height = " + height);
		}

		@Override
		public void surfaceDestroyed(SurfaceHolder arg0) {
			Log.d(TAG, "surfaceCreated destroyed");
		}
	}

}
