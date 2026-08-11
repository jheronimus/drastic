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
typedef jobject  jcharArray;
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
    void *reserved0;
    void *reserved1;
    void *reserved2;
    void *reserved3;

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

    /* CallNativeMethod placeholders */
    void *CallObjectMethod;
    void *CallObjectMethodV;
    void *CallObjectMethodA;
    void *CallBooleanMethod;
    void *CallBooleanMethodV;
    void *CallBooleanMethodA;
    void *CallByteMethod;
    void *CallByteMethodV;
    void *CallByteMethodA;
    void *CallCharMethod;
    void *CallCharMethodV;
    void *CallCharMethodA;
    void *CallShortMethod;
    void *CallShortMethodV;
    void *CallShortMethodA;
    void *CallIntMethod;
    void *CallIntMethodV;
    void *CallIntMethodA;
    void *CallLongMethod;
    void *CallLongMethodV;
    void *CallLongMethodA;
    void *CallFloatMethod;
    void *CallFloatMethodV;
    void *CallFloatMethodA;
    void *CallDoubleMethod;
    void *CallDoubleMethodV;
    void *CallDoubleMethodA;
    void *CallVoidMethod;
    void *CallVoidMethodV;
    void *CallVoidMethodA;

    void *CallNonvirtualObjectMethod;
    void *CallNonvirtualObjectMethodV;
    void *CallNonvirtualObjectMethodA;
    void *CallNonvirtualBooleanMethod;
    void *CallNonvirtualBooleanMethodV;
    void *CallNonvirtualBooleanMethodA;
    void *CallNonvirtualByteMethod;
    void *CallNonvirtualByteMethodV;
    void *CallNonvirtualByteMethodA;
    void *CallNonvirtualCharMethod;
    void *CallNonvirtualCharMethodV;
    void *CallNonvirtualCharMethodA;
    void *CallNonvirtualShortMethod;
    void *CallNonvirtualShortMethodV;
    void *CallNonvirtualShortMethodA;
    void *CallNonvirtualIntMethod;
    void *CallNonvirtualIntMethodV;
    void *CallNonvirtualIntMethodA;
    void *CallNonvirtualLongMethod;
    void *CallNonvirtualLongMethodV;
    void *CallNonvirtualLongMethodA;
    void *CallNonvirtualFloatMethod;
    void *CallNonvirtualFloatMethodV;
    void *CallNonvirtualFloatMethodA;
    void *CallNonvirtualDoubleMethod;
    void *CallNonvirtualDoubleMethodV;
    void *CallNonvirtualDoubleMethodA;
    void *CallNonvirtualVoidMethod;
    void *CallNonvirtualVoidMethodV;
    void *CallNonvirtualVoidMethodA;

    jfieldID (JNICALL *GetFieldID)(JNIEnv *, jclass, const char *, const char *);

    void *GetObjectField;
    void *GetBooleanField;
    void *GetByteField;
    void *GetCharField;
    void *GetShortField;
    jint (JNICALL *GetIntField)(JNIEnv *, jobject, jfieldID);
    void *GetLongField;
    void *GetFloatField;
    void *GetDoubleField;

    void *SetObjectField;
    void *SetBooleanField;
    void *SetByteField;
    void *SetCharField;
    void *SetShortField;
    void *SetIntField;
    void *SetLongField;
    void *SetFloatField;
    void *SetDoubleField;

    jmethodID (JNICALL *GetStaticMethodID)(JNIEnv *, jclass, const char *, const char *);

    jobject (JNICALL *CallStaticObjectMethod)(JNIEnv *, jclass, jmethodID, ...);
    jobject (JNICALL *CallStaticObjectMethodV)(JNIEnv *, jclass, jmethodID, va_list);
    void *CallStaticObjectMethodA;
    jboolean (JNICALL *CallStaticBooleanMethod)(JNIEnv *, jclass, jmethodID, ...);
    jboolean (JNICALL *CallStaticBooleanMethodV)(JNIEnv *, jclass, jmethodID, va_list);
    void *CallStaticBooleanMethodA;
    void *CallStaticByteMethod;
    void *CallStaticByteMethodV;
    void *CallStaticByteMethodA;
    void *CallStaticCharMethod;
    void *CallStaticCharMethodV;
    void *CallStaticCharMethodA;
    void *CallStaticShortMethod;
    void *CallStaticShortMethodV;
    void *CallStaticShortMethodA;
    void *CallStaticIntMethod;
    void *CallStaticIntMethodV;
    void *CallStaticIntMethodA;
    void *CallStaticLongMethod;
    void *CallStaticLongMethodV;
    void *CallStaticLongMethodA;
    void *CallStaticFloatMethod;
    void *CallStaticFloatMethodV;
    void *CallStaticFloatMethodA;
    void *CallStaticDoubleMethod;
    void *CallStaticDoubleMethodV;
    void *CallStaticDoubleMethodA;
    void *CallStaticVoidMethod;
    void *CallStaticVoidMethodV;
    void *CallStaticVoidMethodA;

    jfieldID (JNICALL *GetStaticFieldID)(JNIEnv *, jclass, const char *, const char *);

    void *GetStaticObjectField;
    void *GetStaticBooleanField;
    void *GetStaticByteField;
    void *GetStaticCharField;
    void *GetStaticShortField;
    void *GetStaticIntField;
    void *GetStaticLongField;
    void *GetStaticFloatField;
    void *GetStaticDoubleField;

    void *SetStaticObjectField;
    void *SetStaticBooleanField;
    void *SetStaticByteField;
    void *SetStaticCharField;
    void *SetStaticShortField;
    void *SetStaticIntField;
    void *SetStaticLongField;
    void *SetStaticFloatField;
    void *SetStaticDoubleField;

    jstring (JNICALL *NewString)(JNIEnv *, const jchar *, jsize);
    jsize (JNICALL *GetStringLength)(JNIEnv *, jstring);
    const jchar* (JNICALL *GetStringChars)(JNIEnv *, jstring, jboolean *);
    void (JNICALL *ReleaseStringChars)(JNIEnv *, jstring, const jchar *);

    jstring (JNICALL *NewStringUTF)(JNIEnv *, const char *);
    jsize (JNICALL *GetStringUTFLength)(JNIEnv *, jstring);
    const char* (JNICALL *GetStringUTFChars)(JNIEnv *, jstring, jboolean *);
    void (JNICALL *ReleaseStringUTFChars)(JNIEnv *, jstring, const char *);

    jsize (JNICALL *GetArrayLength)(JNIEnv *, jarray);

    jobjectArray (JNICALL *NewObjectArray)(JNIEnv *, jsize, jclass, jobject);
    jobject (JNICALL *GetObjectArrayElement)(JNIEnv *, jobjectArray, jsize);
    void (JNICALL *SetObjectArrayElement)(JNIEnv *, jobjectArray, jsize, jobject);

    jbyteArray (JNICALL *NewByteArray)(JNIEnv *, jsize);
    jbyteArray (JNICALL *NewBooleanArray)(JNIEnv *, jsize);
    jcharArray (JNICALL *NewCharArray)(JNIEnv *, jsize);
    jshortArray (JNICALL *NewShortArray)(JNIEnv *, jsize);
    jintArray (JNICALL *NewIntArray)(JNIEnv *, jsize);
    jlongArray (JNICALL *NewLongArray)(JNIEnv *, jsize);
    jfloatArray (JNICALL *NewFloatArray)(JNIEnv *, jsize);
    jdoubleArray (JNICALL *NewDoubleArray)(JNIEnv *, jsize);

    jbyte* (JNICALL *GetByteArrayElements)(JNIEnv *, jbyteArray, jboolean *);
    jboolean* (JNICALL *GetBooleanArrayElements)(JNIEnv *, jarray, jboolean *);
    jchar* (JNICALL *GetCharArrayElements)(JNIEnv *, jarray, jboolean *);
    jshort* (JNICALL *GetShortArrayElements)(JNIEnv *, jshortArray, jboolean *);
    jint* (JNICALL *GetIntArrayElements)(JNIEnv *, jintArray, jboolean *);
    jlong* (JNICALL *GetLongArrayElements)(JNIEnv *, jlongArray, jboolean *);
    jfloat* (JNICALL *GetFloatArrayElements)(JNIEnv *, jfloatArray, jboolean *);
    jdouble* (JNICALL *GetDoubleArrayElements)(JNIEnv *, jdoubleArray, jboolean *);

    void (JNICALL *ReleaseByteArrayElements)(JNIEnv *, jbyteArray, jbyte *, jint);
    void (JNICALL *ReleaseBooleanArrayElements)(JNIEnv *, jarray, jboolean *, jint);
    void (JNICALL *ReleaseCharArrayElements)(JNIEnv *, jarray, jchar *, jint);
    void (JNICALL *ReleaseShortArrayElements)(JNIEnv *, jshortArray, jshort *, jint);
    void (JNICALL *ReleaseIntArrayElements)(JNIEnv *, jintArray, jint *, jint);
    void (JNICALL *ReleaseLongArrayElements)(JNIEnv *, jlongArray, jlong *, jint);
    void (JNICALL *ReleaseFloatArrayElements)(JNIEnv *, jfloatArray, jfloat *, jint);
    void (JNICALL *ReleaseDoubleArrayElements)(JNIEnv *, jdoubleArray, jdouble *, jint);

    void (JNICALL *GetByteArrayRegion)(JNIEnv *, jbyteArray, jsize, jsize, jbyte *);
    void (JNICALL *GetBooleanArrayRegion)(JNIEnv *, jarray, jsize, jsize, jboolean *);
    void (JNICALL *GetCharArrayRegion)(JNIEnv *, jarray, jsize, jsize, jchar *);
    void (JNICALL *GetShortArrayRegion)(JNIEnv *, jshortArray, jsize, jsize, jshort *);
    void (JNICALL *GetIntArrayRegion)(JNIEnv *, jintArray, jsize, jsize, jint *);
    void (JNICALL *GetLongArrayRegion)(JNIEnv *, jlongArray, jsize, jsize, jlong *);
    void (JNICALL *GetFloatArrayRegion)(JNIEnv *, jfloatArray, jsize, jsize, jfloat *);
    void (JNICALL *GetDoubleArrayRegion)(JNIEnv *, jdoubleArray, jsize, jsize, jdouble *);

    void (JNICALL *SetByteArrayRegion)(JNIEnv *, jbyteArray, jsize, jsize, const jbyte *);
    void (JNICALL *SetBooleanArrayRegion)(JNIEnv *, jarray, jsize, jsize, const jboolean *);
    void (JNICALL *SetCharArrayRegion)(JNIEnv *, jarray, jsize, jsize, const jchar *);
    void (JNICALL *SetShortArrayRegion)(JNIEnv *, jshortArray, jsize, jsize, const jshort *);
    void (JNICALL *SetIntArrayRegion)(JNIEnv *, jintArray, jsize, jsize, const jint *);
    void (JNICALL *SetLongArrayRegion)(JNIEnv *, jlongArray, jsize, jsize, const jlong *);
    void (JNICALL *SetFloatArrayRegion)(JNIEnv *, jfloatArray, jsize, jsize, const jfloat *);
    void (JNICALL *SetDoubleArrayRegion)(JNIEnv *, jdoubleArray, jsize, jsize, const jdouble *);

    void* (JNICALL *GetPrimitiveArrayCritical)(JNIEnv *, jarray, jboolean *);
    void (JNICALL *ReleasePrimitiveArrayCritical)(JNIEnv *, jarray, void *, jint);

    jint (JNICALL *RegisterNatives)(JNIEnv *, jclass, const void *, jint);
    jint (JNICALL *UnregisterNatives)(JNIEnv *, jclass);
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
