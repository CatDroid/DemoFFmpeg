//
// Created by Hanloong Ho on 17-4-13.
//

#ifndef DEMO_FFMPEG_AS_JEVENT_H
#define DEMO_FFMPEG_AS_JEVENT_H


enum NATIVE_POST_MESSAGE {
    MEDIA_STATUS					= 0 ,
    MEDIA_PREPARED 					,
    MEDIA_SEEK_COMPLETED 			,
    MEDIA_PLAY_COMPLETED 			,


    MEDIA_DATA						= 100 ,

    MEDIA_INFO						= 200 ,
    MEDIA_INFO_PAUSE_COMPLETED 		,

    MEDIA_ERR						= 400 , // like 404 ..
    MEDIA_ERR_SEEK					,
    MEDIA_ERR_PREPARE				,
    MEDIA_ERR_PAUSE					,
    MEDIA_ERR_PLAY					,
};




#endif //DEMO_FFMPEG_AS_JEVENT_H
