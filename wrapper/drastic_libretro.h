#ifndef DRASTIC_LIBRETRO_H
#define DRASTIC_LIBRETRO_H

#include "libretro.h"
#include "jni_mock.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef jint (JNICALL *fn_JNI_OnLoad)(JavaVM *vm, void *reserved);
typedef void (JNICALL *fn_onInit)(JNIEnv *env, jobject obj, jstring path, jstring savePath);
typedef jboolean (JNICALL *fn_startGame)(JNIEnv *env, jobject obj, jstring romPath);
typedef jint (JNICALL *fn_updateFrame)(JNIEnv *env, jobject obj, jint keys, jint touchXY, jboolean touched);
typedef void (JNICALL *fn_updateInput)(JNIEnv *env, jobject obj, jint keys, jint touchXY, jboolean touched);
typedef void (JNICALL *fn_getScreenBuffers)(JNIEnv *env, jobject obj, jintArray top, jintArray bottom);
typedef void (JNICALL *fn_renderFrame)(JNIEnv *env, jobject obj, jint x, jint y, jbyte mode);
typedef void (JNICALL *fn_saveState)(JNIEnv *env, jobject obj, jint slot, jboolean async);
typedef jboolean (JNICALL *fn_loadState)(JNIEnv *env, jobject obj, jint slot);
typedef void (JNICALL *fn_setAutosaveInterval)(JNIEnv *env, jobject obj, jint seconds);
typedef void (JNICALL *fn_resetDS)(JNIEnv *env, jobject obj);
typedef void (JNICALL *fn_quitSystem)(JNIEnv *env, jobject obj);

#ifdef __cplusplus
}
#endif

#endif /* DRASTIC_LIBRETRO_H */
