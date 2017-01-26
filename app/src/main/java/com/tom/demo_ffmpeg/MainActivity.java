package com.tom.demo_ffmpeg;


import com.tom.ffmpegAPI.Mp4player;

import android.app.Activity;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

public class MainActivity extends Activity {

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
	private Mp4player mPlayer = null;

	private void toastMessage(final String msg) {
		mUIhandler.post(new Runnable() {
			@Override
			public void run() {
				Toast.makeText(MainActivity.this, msg, Toast.LENGTH_LONG).show();
			}
		});
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

//						int sampleRate = CONFIG_AUDIO_SAMPLE;
//						int channel = AudioFormat.CHANNEL_OUT_STEREO;
//						int depth = AudioFormat.ENCODING_PCM_16BIT;
//
//						if (CONFIG_AUDIO_CHANNEL == 1) {
//							channel = AudioFormat.CHANNEL_OUT_MONO;
//						}
//
//						if (CONFIG_AUDIO_DEPTH == 8) {
//							depth = AudioFormat.ENCODING_PCM_8BIT;
//						}

//						int bufferSize = AudioTrack.getMinBufferSize(sampleRate, channel, depth);
//						AudioTrack at = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate, channel, depth,
//								bufferSize, AudioTrack.MODE_STREAM);

//						at.setVolume(1.0f);
//						at.play();
						mPlayer = new Mp4player();

						//mPlayer.playmp4("/mnt/sdcard/wushun.3gp", mSh.getSurface(), null);
						//mPlayer.playmp4("/mnt/sdcard/1080p60fps.mp4", mSh.getSurface(), null);
						mPlayer.playmp4("/mnt/sdcard/screen_mic_main_r30_g30_ultrafast_c2_44k_6000K_128K_yuv444.mp4", mSh.getSurface(), null);

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
