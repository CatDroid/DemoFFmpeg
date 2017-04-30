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
import android.os.Message;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.Toast;

public class FFmpegActivity extends Activity {

	private static final String TAG = "FFmpegActivity";

	private SeekBar mSb = null;

	private SurfaceView mSv = null;
	private SurfaceHolder mSh = null;
	private boolean mSurfaceCreated = false;

	private Button mBtnPlay = null;
	private Button mBtnStop = null;
	private Button mBtnPause = null;

	private DragonPlayer mPlayer = null;
	private boolean mPaused = false ;


	private Handler mUIhandler = new UIEventHandler();
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

	public class UIEventHandler extends Handler
	{
		public final static int EVENT_INIT 			= 0 ;
		public final static int EVENT_START_VOD  	= 1 ;
		public final static int EVENT_ING_VOD		= 2 ;
		public final static int EVENT_UPDATE_VOD 	= 3 ;
		public final static int EVENT_START_PREVIEW = 4 ;
		public final static int EVENT_ING_PREVIEW   = 5 ;

		@Override
		public void handleMessage(Message msg) {
			switch(msg.what){
				case EVENT_UPDATE_VOD:
					if( mPlayer == null ) return ;
					int position = mPlayer.getCurrentPosition() ;
					Log.d(TAG , " position = " + position);
					if(mSb != null) mSb.setProgress(position);
					sendEmptyMessageDelayed(EVENT_UPDATE_VOD, 1000);
					break;
				default:
					break;
			}
			super.handleMessage(msg);
		}
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		mSb = (SeekBar) findViewById(R.id.sbSeek);
		mSb.setOnSeekBarChangeListener( new MySeekBarChangeListener() );

		mSv = (SurfaceView) findViewById(R.id.videoView);
		mSh = mSv.getHolder();
		mSh.addCallback(new SurfaceCallback());

		mBtnPlay = (Button)findViewById(R.id.bPlay);
		mBtnPause = (Button)findViewById(R.id.bPause);
		mBtnStop = (Button)findViewById(R.id.bStop);

		mBtnPlay.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {

			if (mSurfaceCreated) {
				if (mPlayer == null) {
					mPlayer = new DragonPlayer();
					mPlayer.setSurface(mSh.getSurface());
					//mPlayer.setDataSource("file:///mnt/sdcard/screen_mic_main_r30_g30_ultrafast_c2_44k_6000K_128K_yuv444.mp4");
					//mPlayer.setDataSource("file:///mnt/sdcard/1080p60fps.mp4");
					mPlayer.setDataSource("file:///mnt/sdcard/test.3gp");
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
							mSb.setMax( mp.getDuration() > 0 ? mp.getDuration() : 0  );
							mUIhandler.sendEmptyMessageDelayed(UIEventHandler.EVENT_UPDATE_VOD,1000);
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

							if(what == DragonPlayer.MEDIA_ERR_SEEK){
								String msg = "定位失败 " + what ;
								toastMessage( msg); Log.e(TAG,msg);
								mSb.setProgress(mPlayer.getCurrentPosition());
								// 如果seek失败 返回原来的位置
							}else{
								String msg = "播放错误 : " + what ;
								toastMessage( msg); Log.e(TAG,msg);
								mPlayer.stop(); mPlayer.release(); mPlayer = null;
							}

							return true ;
						}
					});
					mPlayer.setOnInfoListener(new DragonPlayer.OnInfoListener(){
						@Override
						public boolean onInfo(DragonPlayer mp, int what, int extra) {

							if(what == DragonPlayer.MEDIA_INFO_PAUSE_COMPLETED){
								String msg = "已经暂停" ;
								toastMessage( msg); Log.w(TAG,msg);
							}else{
								String msg = "播放信息 : " + what ;
							}

							return false;
						}
					});
					mPlayer.setOnSeekCompleteListener(
							new DragonPlayer.OnSeekCompleteListener() {
							  	@Override
								public void onSeekComplete(DragonPlayer mp) {
									String msg = "定位成功";
									toastMessage( msg); Log.w(TAG,msg);
							  	}
						  	}
					);
					mPlayer.prepareAsync();

				} else {
					toastMessage("请先关闭再重新启动");
				}
			} else {
				toastMessage("界面还没有准备好");
			}
			}
		});

		mBtnStop.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				if (mPlayer != null) {
					mPlayer.stop();
					mPlayer.release();
					mPlayer = null;
					mPaused = false ;
				}else{
					toastMessage("播放器没有启动");
				}
			}
		});

		mBtnPause.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				if (mPlayer != null) {
					if(mPaused ){
						mPlayer.play();
						mPaused = false ;
					}else{
						mPlayer.pause();
						mPaused = true ;
					}

				}else{
					toastMessage("播放器没有启动");
				}
			}
		});

	}


	class MySeekBarChangeListener implements SeekBar.OnSeekBarChangeListener
	{
		public int last_change = -1 ;
		@Override
		public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
			if(fromUser){
				last_change = progress ;
			}
		}

		@Override
		public void onStartTrackingTouch(SeekBar seekBar) {

		}

		@Override
		public void onStopTrackingTouch(SeekBar seekBar) {
			if( last_change != -1){
				// default progress is 0 ~ 100
				// after mSb.setMax( mDuration ); progress is 0 ~ mDuration
				if(mPlayer != null ){
					mPlayer.seekTo(last_change);
					Log.d(TAG, "seekTo " + last_change );
				}
				last_change = -1 ;
			}
		}
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
			if (mPlayer != null) {
				mPlayer.stop();
				mPlayer.release();
				mPlayer = null;
			}
		}
	}

}
