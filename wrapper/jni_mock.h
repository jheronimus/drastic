#ifndef JNI_MOCK_H
#define JNI_MOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifndef JNICALL
#define JNICALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  jboolean;
typedef int8_t   jbyte;
typedef uint16_t jchar;
typedef int16_t  jshort;
typedef int32_t  jint;
typedef int64_t  jlong;
typedef float    jfloat;
typedef double   jdouble;
typedef jint     jsize;

typedef void*    jobject;
typedef jobject  jclass;
typedef jobject  jstring;
typedef jobject  jarray;
typedef jobject  jbyteArray;
typedef jobject  jshortArray;
typedef jobject  jintArray;
typedef jobject  jlongArray;
typedef jobject  jfloatArray;
typedef jobject  jdoubleArray;
typedef jobject  jobjectArray;
typedef jobject  jthrowable;
typedef void*    jfieldID;
typedef void*    jmethodID;

#define JNI_FALSE 0
#define JNI_TRUE  1
#define JNI_OK    0
#define JNI_ERR   (-1)

struct JNINativeInterface;
struct JNIInvokeInterface;

typedef const struct JNINativeInterface* JNIEnv;
typedef const struct JNIInvokeInterface* JavaVM;

struct JNINativeInterface {
    void* reserved0;
    void* reserved1;
    void* reserved2;
    void* reserved3;

    jint (JNICALL *GetVersion)(JNIEnv *);

    jclass (JNICALL *DefineClass)(JNIEnv *, const char *, jobject, const jbyte *, jsize);
    jclass (JNICALL *FindClass)(JNIEnv *, const char *);

    jmethodID (JNICALL *FromReflectedMethod)(JNIEnv *, jobject);
    jfieldID (JNICALL *FromReflectedField)(JNIEnv *, jobject);

    jobject (JNICALL *ToReflectedMethod)(JNIEnv *, jclass, jmethodID, jboolean);

    jclass (JNICALL *GetSuperclass)(JNIEnv *, jclass);
    jboolean (JNICALL *IsAssignableFrom)(JNIEnv *, jclass, jclass);

    jobject (JNICALL *ToReflectedField)(JNIEnv *, jclass, jfieldID, jboolean);

    jint (JNICALL *Throw)(JNIEnv *, jthrowable);
    jint (JNICALL *ThrowNew)(JNIEnv *, jclass, const char *);
    jthrowable (JNICALL *ExceptionOccurred)(JNIEnv *);
    void (JNICALL *ExceptionDescribe)(JNIEnv *);
    void (JNICALL *ExceptionClear)(JNIEnv *);
    void (JNICALL *FatalError)(JNIEnv *, const char *);

    jint (JNICALL *PushLocalFrame)(JNIEnv *, jint);
    jobject (JNICALL *PopLocalFrame)(JNIEnv *, jobject);

    jobject (JNICALL *NewGlobalRef)(JNIEnv *, jobject);
    void (JNICALL *DeleteGlobalRef)(JNIEnv *, jobject);
    void (JNICALL *DeleteLocalRef)(JNIEnv *, jobject);
    jboolean (JNICALL *IsSameObject)(JNIEnv *, jobject, jobject);
    jobject (JNICALL *NewLocalRef)(JNIEnv *, jobject);
    jint (JNICALL *EnsureLocalCapacity)(JNIEnv *, jint);

    jobject (JNICALL *AllocObject)(JNIEnv *, jclass);
    jobject (JNICALL *NewObject)(JNIEnv *, jclass, jmethodID, ...);
    jobject (JNICALL *NewObjectV)(JNIEnv *, jclass, jmethodID, va_list);
    jobject (JNICALL *NewObjectA)(JNIEnv *, jclass, jmethodID, const void *);

    jclass (JNICALL *GetObjectClass)(JNIEnv *, jobject);
    jboolean (JNICALL *IsInstanceOf)(JNIEnv *, jobject, jclass);

    jmethodID (JNICALL *GetMethodID)(JNIEnv *, jclass, const char *, const char *);

    /* String functions */
    jstring (JNICALL *NewStringUTF)(JNIEnv *, const char *);
    jsize (JNICALL *GetStringUTFLength)(JNIEnv *, jstring);
    const char* (JNICALL *GetStringUTFChars)(JNIEnv *, jstring, jboolean *);
    void (JNICALL *ReleaseStringUTFChars)(JNIEnv *, jstring, const char *);

    /* Array functions */
    jsize (JNICALL *GetArrayLength)(JNIEnv *, jarray);
    jbyteArray (JNICALL *NewByteArray)(JNIEnv *, jsize);
    jbyte* (JNICALL *GetByteArrayElements)(JNIEnv *, jbyteArray, jboolean *);
    void (JNICALL *ReleaseByteArrayElements)(JNIEnv *, jbyteArray, jbyte *, jint);
    void (JNICALL *GetByteArrayRegion)(JNIEnv *, jbyteArray, jsize, jsize, jbyte *);
    void (JNICALL *SetByteArrayRegion)(JNIEnv *, jbyteArray, jsize, jsize, const jbyte *);

    jintArray (JNICALL *NewIntArray)(JNIEnv *, jsize);
    jint* (JNICALL *GetIntArrayElements)(JNIEnv *, jintArray, jboolean *);
    void (JNICALL *ReleaseIntArrayElements)(JNIEnv *, jintArray, jint *, jint);

    void* (JNICALL *GetPrimitiveArrayCritical)(JNIEnv *, jarray, jboolean *);
    void (JNICALL *ReleasePrimitiveArrayCritical)(JNIEnv *, jarray, void *, jint);
};

struct JNIInvokeInterface {
    void* reserved0;
    void* reserved1;
    void* reserved2;

    jint (JNICALL *DestroyJavaVM)(JavaVM *);
    jint (JNICALL *AttachCurrentThread)(JavaVM *, void **, void *);
    jint (JNICALL *DetachCurrentThread)(JavaVM *);
    jint (JNICALL *GetEnv)(JavaVM *, void **, jint);
    jint (JNICALL *AttachCurrentThreadAsDaemon)(JavaVM *, void **, void *);
};

JNIEnv* get_mock_jni_env(void);
JavaVM* get_mock_java_vm(void);

#ifdef __cplusplus
}
#endif

#endif /* JNI_MOCK_H */
