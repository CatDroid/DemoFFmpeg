//
// Created by hanlon on 17-1-4.
//

#ifndef DEMO_FFMPEG_AS_LIGHTREFBASE_H
#define DEMO_FFMPEG_AS_LIGHTREFBASE_H

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/atomics.h> Android-21已经没有这个接口
//#include "stdatomic.h" // 这个在Android-21之后才有
#include "atomic"        // C++ 11

template <class T>
class LightRefBase
{
public:
    inline LightRefBase() : mCount(0) { }
    inline void incStrong(__attribute__((unused)) const void* id)  {
        mCount ++ ;
    }
    inline void decStrong(__attribute__((unused)) const void* id)  {
        if( --mCount == 0 ){  //  if (__atomic_dec(&mCount) == 1) {
            delete static_cast<const T*>(this);
        }
    }
    inline ~LightRefBase() { }

    // Debug Only
    inline int ref_count(){ return mCount ;}
    // Debug Only

    typedef LightRefBase<T> basetype;
private:
    std::atomic<int> mCount ;
};

template <class T>
class RecycleRefBase
{
public:
    inline RecycleRefBase() : mCount(0) { }
    inline void incStrong(__attribute__((unused)) const void* id) {
        mCount ++ ;
    }
    inline void decStrong(__attribute__((unused)) const void* id) {
        if( --mCount == 0 ){
            recycle(); // 与LightRefBase区别就是不delete 而是调用实现类的recycle
        }
    }
    virtual ~RecycleRefBase() { }

    typedef RecycleRefBase<T> basetype;
protected:
    virtual void recycle(); // implements / override
private:
    std::atomic<int> mCount ;
};

template<typename T> class sp ;

#define COMPARE(_op_)                                           \
inline bool operator _op_ (const sp<T>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
inline bool operator _op_ (const T* o) const {                  \
    return m_ptr _op_ o;                                        \
}                                                               \
template<typename U>                                            \
inline bool operator _op_ (const sp<U>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
template<typename U>                                            \
inline bool operator _op_ (const U* o) const {                  \
    return m_ptr _op_ o;                                        \
}

template<typename T>
class sp {
public:
    inline sp() : m_ptr(0) { }

    sp(T* other);
    sp(const sp<T>& other);
    template<typename U> sp(U* other);
    template<typename U> sp(const sp<U>& other);

    ~sp();

    // Assignment
    sp& operator = (T* other);
    sp& operator = (const sp<T>& other);

    template<typename U> sp& operator = (const sp<U>& other);
    template<typename U> sp& operator = (U* other);

    //! Special optimization for use by ProcessState (and nobody else).
    void force_set(T* other);

    // Reset
    void clear();

    // Accessors
    inline  T&      operator* () const  { return *m_ptr; }
    inline  T*      operator-> () const { return m_ptr;  }
    inline  T*      get() const         { return m_ptr; }

    // Operators
    COMPARE(==)
    COMPARE(!=)
    COMPARE(>)
    COMPARE(<)
    COMPARE(<=)
    COMPARE(>=)

private:
    template<typename Y> friend class sp;
    void set_pointer(T* ptr);
    T* m_ptr;
};

#undef COMPARE

template<typename T>
sp<T>::sp(T* other)
        : m_ptr(other) {
    if (other)
        other->incStrong(this);
}

template<typename T>
sp<T>::sp(const sp<T>& other)
        : m_ptr(other.m_ptr) {
    if (m_ptr)
        m_ptr->incStrong(this);
}

template<typename T> template<typename U>
sp<T>::sp(U* other)
        : m_ptr(other) {
    if (other)
        ((T*) other)->incStrong(this);
}

template<typename T> template<typename U>
sp<T>::sp(const sp<U>& other)
        : m_ptr(other.m_ptr) {
    if (m_ptr)
        m_ptr->incStrong(this);
}

template<typename T>
sp<T>::~sp() {
    if (m_ptr)
        m_ptr->decStrong(this);
}

template<typename T>
sp<T>& sp<T>::operator =(const sp<T>& other) {
    T* otherPtr(other.m_ptr);
    if (otherPtr)
        otherPtr->incStrong(this);
    if (m_ptr)
        m_ptr->decStrong(this);
    m_ptr = otherPtr;
    return *this;
}

template<typename T>
sp<T>& sp<T>::operator =(T* other) {
    if (other)
        other->incStrong(this);
    if (m_ptr)
        m_ptr->decStrong(this);
    m_ptr = other;
    return *this;
}

template<typename T> template<typename U>
sp<T>& sp<T>::operator =(const sp<U>& other) {
    T* otherPtr(other.m_ptr);
    if (otherPtr)
        otherPtr->incStrong(this);
    if (m_ptr)
        m_ptr->decStrong(this);
    m_ptr = otherPtr;
    return *this;
}

template<typename T> template<typename U>
sp<T>& sp<T>::operator =(U* other) {
    if (other)
        ((T*) other)->incStrong(this);
    if (m_ptr)
        m_ptr->decStrong(this);
    m_ptr = other;
    return *this;
}

template<typename T>
void sp<T>::force_set(T* other) {
    other->forceIncStrong(this);
    m_ptr = other;
}

template<typename T>
void sp<T>::clear() {
    if (m_ptr) {
        m_ptr->decStrong(this);
        m_ptr = 0;
    }
}

template<typename T>
void sp<T>::set_pointer(T* ptr) {
    m_ptr = ptr;
}

#endif //DEMO_FFMPEG_AS_LIGHTREFBASE_H
