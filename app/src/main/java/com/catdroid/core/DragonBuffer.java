package com.catdroid.core;

import java.nio.ByteBuffer;

/**
 * Created by Hanloong Ho on 17-4-3.
 */

public class DragonBuffer {

    static private final String TAG = "DragonBuffer" ;

    public enum MEDIA_DATA_TYPE{
        H264,
        AAC,
        PCM,
        RGB888,
        RGB565,
        RGBA8888,
        RGBX8888,
        JPEG,
        NV21,
        NV12,
        I420,
        YV12
    }

    public long mNativeCtx ;
    public int  mDataType = 0 ;
    public ByteBuffer mData = null ;
    public long mArg1 = 0;
    public long mArg2 = 0;
    public long mArg3 = 0;
    public long mArg4 = 0;

    public void releaseBuffer() {
        if( mNativeCtx != 0 ) {
            //native_releaseBuffer(mNativeCtx);
            mNativeCtx = 0 ;
        }
    }

}
