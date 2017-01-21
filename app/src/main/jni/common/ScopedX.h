//
// Created by hanlon on 17-1-8.
//

#ifndef DEMO_FFMPEG_AS_SCOPEDX_H
#define DEMO_FFMPEG_AS_SCOPEDX_H

#include "string.h"

template <class T>
class ScopedX {
public:
    ScopedX(T& target ){ p = &target ; obtain() ;}
    ScopedX(T* target = 0) : p(target) { if(p) obtain() ; }
    ~ScopedX() {  if(p) { release();  p = NULL; } }

    T* operator->() {return p; }
    //T& operator*()  {return *p;}

    void attach(T* target) { if(p) release(); p = target; if(p) obtain();}
    T* detach(){ T* ret = p; p = NULL; return ret;  }

protected:
    virtual void release(){ // 默认直接析构   override 释放引用
        delete p;
    }
    virtual void obtain(){ //  默认无操作    override 增加引用

    }
    T* p;

};

#endif //DEMO_FFMPEG_AS_SCOPEDX_H
