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
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

public class FFmpegActivity extends Activity {

	private static final String TAG = "main";
	private static final int CONFIG_AUDIO_DEPTH = 16; // 8bit or 16bit
	private static final int CONFIG_AUDIO_CHANNEL = 2; // 1 or 2 channels
	private static final int CONFIG_AUDIO_SAMPLE = 115200;

	private Button playBtn = null;
	private Button stopBtn = null;
	private SurfaceView mSv = null;
	private SurfaceHolder mSh = null;
	private SurfaceView mSv1 = null;
	private SurfaceHolder mSh1 = null;
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
		
		
		int minisize = AudioTrack.getMinBufferSize(115200, AudioFormat.CHANNEL_OUT_STEREO,  AudioFormat.ENCODING_PCM_16BIT);

		mSv = (SurfaceView) findViewById(R.id.videoView);
		mSh = mSv.getHolder();
		mSh.setKeepScreenOn(true);
		mSh.addCallback(new SurfaceCallback());

		((Button) findViewById(R.id.bPlay)).setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {

			if (mSurfaceCreated) {
				if (mPlayer == null) {
					mPlayer = new DragonPlayer();
					mPlayer.setSurface(mSh.getSurface());
					mPlayer.setDataSource("file:///mnt/sdcard/screen_mic_main_r30_g30_ultrafast_c2_44k_6000K_128K_yuv444.mp4");
					mPlayer.setOnPreparedListener(new DragonPlayer.OnPreparedListener(){
						@Override
						public void onPrepared(DragonPlayer mp, int what) {
							mp.play();
						}
					});
					mPlayer.setOnCompletionListener(new DragonPlayer.OnCompletionListener() {
						@Override
						public void onCompletion(DragonPlayer mp) {
							String msg = "play complete";
							toastMessage(msg);
							Log.w(TAG, msg);
							mPlayer.stop();
							mPlayer.release();
							mPlayer = null;
						}
					});
					mPlayer.setOnErrorListener(new DragonPlayer.OnErrorListener(){

						@Override
						public boolean onError(DragonPlayer mp, int what, int extra) {
							String msg = "play Error what = " + what  + " extra " + extra;
							toastMessage( msg);
							Log.e(TAG,msg);
							mPlayer.stop();
							mPlayer.release();
							mPlayer = null;
							return true ;
						}
					});


				} else {
					toastMessage("please stop befor play again");
				}

			} else {
				toastMessage("surface not ready now !");
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
			}
			}
		});

	}

	private class SurfaceCallback implements SurfaceHolder.Callback {
		@Override
		public void surfaceChanged(SurfaceHolder arg0, int arg1, int arg2, int arg3) {
			Log.d(TAG, "surfaceChanged " + " arg1 = " + arg1 + " arg2 = " + arg2 + " arg3 = " + arg3);
		}
		@Override
		public void surfaceCreated(SurfaceHolder arg0) {
			Log.d(TAG, "surfaceCreated created");
			mSurfaceCreated = true;
		}
		@Override
		public void surfaceDestroyed(SurfaceHolder arg0) {
			Log.d(TAG, "surfaceCreated destroyed");
		}
	}

}
