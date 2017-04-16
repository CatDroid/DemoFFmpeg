package com.catdroid.core;

/**
 * Created by Hanloong Ho on 17-4-3.
 */

import android.media.AudioTrack;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.Surface;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

public class DragonPlayer {

	private static final String TAG = "DragonPlayer";

	// 事件回调接口
	private EventHandler mEventHandler;
	// Native层对象指针
	private long mPlayerCtx = 0 ;

	public DragonPlayer(){
		Looper looper;
		if ((looper = Looper.myLooper()) != null) {
			mEventHandler = new EventHandler(this, looper);
		} else if ((looper = Looper.getMainLooper()) != null) {
			mEventHandler = new EventHandler(this, looper);
		} else {
			mEventHandler = null;
		}
		mPlayerCtx = native_setup(new WeakReference<DragonPlayer>(this));
	}

	@Override
	protected void finalize() throws Throwable {
		super.finalize();
		if( mPlayerCtx != 0 ) { // Note: 避免用户没有释放 导致可能的内存泄漏
			Log.w(TAG, "DragonPlayer is NOT called release");
			native_free(mPlayerCtx);
			mPlayerCtx = 0L;
			mEventHandler = null;
		}
	}

	static{
		System.loadLibrary("demo_ffmpeg");
	}


	// Part.1 初始化和反初始化接口
	/*
	 * 设置数据源
	 * @return false:路径不合法
     * @param path 文件路径 本地文件 file:///mnt/sdcard/test.mp4  file://+绝对路径
     * 					   rtsp直播 rtsp://192.168.1.187:8086
     * 					   rtsp回放 rtsp://192.168.1.187:8086/test.mp4
	 * @throws IllegalArgumentException 参数路径为空
	 * @throws IllegalStateException 已经调用release释放
     * @see android.media.MediaPlayer
     */
	public boolean setDataSource(String path) throws IllegalArgumentException,IllegalStateException{
		checkStatus();
		if(path == null || path.length() == 0 ){
			throw new IllegalArgumentException("file path is null or length is zero");
		}
		return native_setDataSource( mPlayerCtx,  path);
	}

	public void prepareAsync() {
		checkStatus();
		native_prepareAsync(mPlayerCtx);
	}

	public void release() {
		if( mPlayerCtx != 0 ) {
			native_free(mPlayerCtx);
			mPlayerCtx = 0L;
		}
		mEventHandler = null;
	}

	//  Part.2 控制接口
	public boolean setSurface(Surface surface)throws IllegalStateException {
		checkStatus();
		return native_setDisplay(mPlayerCtx, surface);
	}
	public void play() throws IllegalStateException {
		checkStatus();
		native_play(mPlayerCtx);
	}
	public void pause() throws IllegalStateException {
		checkStatus();
		native_pause(mPlayerCtx);
	}
	public void seekTo( int msec ) throws IllegalStateException{
		checkStatus();
		native_seekTo(mPlayerCtx, msec);
	}
	public void stop() throws IllegalStateException{
		checkStatus();
		native_stop(mPlayerCtx);
	}
	public boolean setVolume(float left , float right )throws IllegalStateException{
		checkStatus();
		return native_setVolume(mPlayerCtx, left , right ) ;
	}
	public boolean setMute(boolean on)throws IllegalStateException{
		checkStatus();
		return native_setMute(mPlayerCtx, on );
	}


	//  Part.3 文件长度和进度信息(非直播,本地回放和远端回放)
	public int getDuration() {
		return native_getDuration(mPlayerCtx);
	}
	public int getCurrentPosition() {
		return native_getCurrentPosition(mPlayerCtx);
	}

	//  Part.4 获取视频信息
	public int getVideoWidth() {
		return native_getVideoWidth(mPlayerCtx);
	}
	public int getVideoHeight() {
		return native_getVideoHeight(mPlayerCtx);
	}

	//  Part.5 获取音频信息
	public int getAudioChannel() {
		return native_getAudioChannel(mPlayerCtx);
	}
	public int getAudioDepth() {
		return native_getAudioDepth(mPlayerCtx);
	}
	public int getAudioSampleRate() {
		return native_getAudioSampleRate(mPlayerCtx);
	}

	//  Part.6 事件回调
	public static final int MEDIA_STATUS					= 0 ;
	public static final int MEDIA_PREPARED 					= MEDIA_STATUS + 1 ;
	public static final int MEDIA_SEEK_COMPLETED 			= MEDIA_STATUS + 2 ;
	public static final int MEDIA_PLAY_COMPLETED 			= MEDIA_STATUS + 3 ;


	public static final int MEDIA_DATA						= 100 ;

	public static final int MEDIA_INFO						= 200 ;
	public static final int MEDIA_INFO_PAUSE_COMPLETED 		= MEDIA_INFO + 1 ;

	public static final int MEDIA_ERR						= 400 ; // like 404 ..
	public static final int MEDIA_ERR_SEEK					= MEDIA_ERR + 1 ;
	public static final int MEDIA_ERR_PREPARE				= MEDIA_ERR + 2 ;
	public static final int MEDIA_ERR_PAUSE					= MEDIA_ERR + 3 ;
	public static final int MEDIA_ERR_PLAY					= MEDIA_ERR + 4 ;


	/*
     * what					extra
     * MEDIA_ERR_PREPARE 	-
     * MEDIA_ERR_PLAY		-
     * MEDIA_ERR_PAUSE		-
     * MEDIA_ERR_SEEK		-
     */
	public interface OnErrorListener {
		boolean onError(DragonPlayer mp, int what, int extra);
	}
	public void setOnErrorListener(OnErrorListener listener) {
		mOnErrorListener = listener;
	}
	private OnErrorListener mOnErrorListener;

	public interface OnCompletionListener {
		void onCompletion(DragonPlayer mp);
	}
	public void setOnCompletionListener(OnCompletionListener listener) {
		mOnCompletionListener = listener;
	}
	private OnCompletionListener mOnCompletionListener;

	public interface OnPreparedListener {
		void onPrepared(DragonPlayer mp, int what);
	}
	public void setOnPreparedListener(OnPreparedListener listener) {
		mOnPreparedListener = listener;
	}
	private OnPreparedListener mOnPreparedListener;

	public interface OnSeekCompleteListener {
		public void onSeekComplete(DragonPlayer mp);
	}
	public void setOnSeekCompleteListener(OnSeekCompleteListener listener) {
		mOnSeekCompleteListener = listener;
	}
	private OnSeekCompleteListener mOnSeekCompleteListener ;


	public interface OnInfoListener {
		boolean onInfo(DragonPlayer mp, int what, int extra);
	}
	public void setOnInfoListener(OnInfoListener listener) {
		mOnInfoListener = listener;
	}
	private OnInfoListener mOnInfoListener;

	/* 内部分类数据 给到使用者给定的Listene实例 当然可以是同一实例 通过DragonBuffer.type来判定  */
	public void subscribeData( DragonBuffer.MEDIA_DATA_TYPE data_type , OnDataAvailableListener listener ){
		boolean isON = false ;
		int index = data_type.ordinal();
		if(listener == null){
			isON = false;
			boolean has = mDataListener.containsKey(index);
			if(!has) {
				Log.w(TAG,"did NOT subscribe this kind of data before ! data_type =" + data_type  );
				return ;
			}
			mDataListener.remove(index);
		}else{
			isON = true ;
			boolean has = mDataListener.containsKey(index);
			if(has){
				Log.w(TAG,"subscribe this kind of data before yet, only support one type data one listener, replace the older one ! data_type =" + data_type  );
			}
			mDataListener.put(index , listener);
		}
		native_subscribeData(mPlayerCtx ,index , isON);
	}
	public interface OnDataAvailableListener {
		void onData(DragonPlayer mp ,DragonBuffer data);
	}
	Map<Integer , OnDataAvailableListener> mDataListener = new HashMap<Integer , OnDataAvailableListener>();

	private class EventHandler extends Handler
	{
		private DragonPlayer mPlayer;
		public EventHandler(DragonPlayer mp, Looper looper) {
			super(looper);
			mPlayer = mp;
		}

		@Override
		public void handleMessage(Message msg) {
			switch(msg.what) {
				//case MEDIA_STATUS:
				case MEDIA_PREPARED:
					if (mOnPreparedListener != null){
						mOnPreparedListener.onPrepared(mPlayer, msg.what);
					}
					break;
				case MEDIA_SEEK_COMPLETED:
					if (mOnSeekCompleteListener != null) {
						mOnSeekCompleteListener.onSeekComplete(mPlayer);
					}
					break;
				case MEDIA_PLAY_COMPLETED:
					if (mOnCompletionListener != null) {
						mOnCompletionListener.onCompletion(mPlayer);
					}
					break;

				// case MEDIA_ERR:
				case MEDIA_ERR_SEEK:
				case MEDIA_ERR_PLAY:
				case MEDIA_ERR_PREPARE:
				case MEDIA_ERR_PAUSE:

				// case MEDIA_DATA:
				case MEDIA_DATA:
					DragonBuffer buffer = (DragonBuffer)msg.obj;
					boolean has = mDataListener.containsKey(buffer.mDataType);
					if(has){
						OnDataAvailableListener listener = mDataListener.get(buffer.mDataType);
						listener.onData(mPlayer, buffer);
						/*
							如果是同一个Listener实例的话 需要这样区分
						    switch ( MEDIA_DATA_TYPE.values()[mDataType] ){
								case AAC:
									// do something here
									break;
								case H264:
									// do something here
									break;
							}
						 */
					}else{
						Log.w(TAG,"native post data(type=" + buffer.mDataType + "), but no one want, release it now");
						buffer.releaseBuffer();
					}
					break;
				default:
					Log.e(TAG, "Unknown message type " + msg.what);
					return;
			}
		}
	}

	/*
	 *		native post message format as followed :
	 *
	 * 		what					arg1			arg2			obj
	 *
	 * 		MEDIA_PREPARED			-				-				-
	 * 		MEDIA_SEEK_COMPLETED	-				-				-
	 * 		MEDIA_PLAY_COMPLETED	-				-				-
	 *
	 * 		MEDIA_ERR_SEEK			int (if needed) -				-
	 * 		MEDIA_ERR_PREPARE		int (if needed) -				-
	 * 		MEDIA_ERR_PAUSE			int (if needed) -				-
	 *		MEDIA_ERR_PLAY			int (if needed) -				-
	 *
	 *		MEDIA_DATA 				-				-				DragonBuffer
	 *
	 * */
	private static void postEventFromNative(Object mediaplayer_ref, int what, int arg1, int arg2, Object obj)
	{
		DragonPlayer mp = (DragonPlayer)((WeakReference)mediaplayer_ref).get();
		if (mp == null) {
			Log.e(TAG, "(DragonPlayer is Finalize)postEventFromNative: what=" + what + ", arg1=" + arg1 + ", arg2=" + arg2);
			return;
		}
		if (mp.mEventHandler != null) {
			Message m = mp.mEventHandler.obtainMessage(what, arg1, arg2, obj);
			mp.mEventHandler.sendMessage(m);
		}
	}


	// Part.7 内部 函数和类
	private void checkStatus() throws IllegalStateException{
		if(mPlayerCtx == 0){throw new IllegalStateException("DragonPlayer is NOT initialized or IS released yet");}
	}


	// Part.8 本地接口
	private native long native_setup(Object player_this_ref );

	private native boolean native_setDataSource(long ctx ,  String path);
	private native void native_prepareAsync(long ctx);
	private native void native_free(long ctx );

	private native boolean native_setDisplay(long player_ctx, Surface surface );
	private native void native_play(long ctx );
	private native void native_pause(long ctx);
	private native void native_seekTo(long ctx , int msec);
	private native void  native_stop(long ctx);

	private native boolean native_setVolume(long ctx , float left , float right );
	private native boolean native_setMute(long ctx , boolean on);

	private native int  native_getDuration(long ctx );
	private native int  native_getCurrentPosition(long ctx );

	private native int  native_getAudioChannel(long ctx );
	private native int  native_getAudioDepth(long ctx );
	private native int  native_getAudioSampleRate(long ctx );
	private native int  native_getVideoWidth(long ctx );
	private native int  native_getVideoHeight(long ctx );

	private native void native_subscribeData(long ctx , int data_type , boolean isOn);

}
