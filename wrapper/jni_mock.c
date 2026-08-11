#include "jni_mock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef JNICALL
#define JNICALL
#endif

static jstring JNICALL mock_NewStringUTF(JNIEnv *env, const char *utf) {
    (void)env;
    if (!utf) return NULL;
    return (jstring)strdup(utf);
}

static const char* JNICALL mock_GetStringUTFChars(JNIEnv *env, jstring str, jboolean *isCopy) {
    (void)env;
    if (isCopy) *isCopy = JNI_FALSE;
    return (const char*)str;
}

static void JNICALL mock_ReleaseStringUTFChars(JNIEnv *env, jstring str, const char *chars) {
    (void)env;
    (void)str;
    (void)chars;
}

static jbyteArray JNICALL mock_NewByteArray(JNIEnv *env, jsize len) {
    (void)env;
    void *mem = malloc(sizeof(jsize) + len);
    if (!mem) return NULL;
    *(jsize*)mem = len;
    return (jbyteArray)((char*)mem + sizeof(jsize));
}

static jbyte* JNICALL mock_GetByteArrayElements(JNIEnv *env, jbyteArray array, jboolean *isCopy) {
    (void)env;
    if (isCopy) *isCopy = JNI_FALSE;
    return (jbyte*)array;
}

static void JNICALL mock_ReleaseByteArrayElements(JNIEnv *env, jbyteArray array, jbyte *elems, jint mode) {
    (void)env;
    (void)array;
    (void)elems;
    (void)mode;
}

static jsize JNICALL mock_GetArrayLength(JNIEnv *env, jarray array) {
    (void)env;
    if (!array) return 0;
    return *(jsize*)((char*)array - sizeof(jsize));
}

static jint JNICALL mock_GetVersion(JNIEnv *env) {
    (void)env;
    return 0x00010006; /* JNI 1.6 */
}

static jclass JNICALL mock_FindClass(JNIEnv *env, const char *name) {
    (void)env;
    (void)name;
    return (jclass)1;
}

static jmethodID JNICALL mock_GetMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    (void)env;
    (void)clazz;
    (void)name;
    (void)sig;
    return (jmethodID)1;
}

static const struct JNINativeInterface g_jni_functions = {
    .reserved0 = NULL,
    .reserved1 = NULL,
    .reserved2 = NULL,
    .reserved3 = NULL,
    .GetVersion = mock_GetVersion,
    .FindClass = mock_FindClass,
    .GetMethodID = mock_GetMethodID,
    .NewStringUTF = mock_NewStringUTF,
    .GetStringUTFChars = mock_GetStringUTFChars,
    .ReleaseStringUTFChars = mock_ReleaseStringUTFChars,
    .GetArrayLength = mock_GetArrayLength,
    .NewByteArray = mock_NewByteArray,
    .GetByteArrayElements = mock_GetByteArrayElements,
    .ReleaseByteArrayElements = mock_ReleaseByteArrayElements,
};

static const struct JNINativeInterface* g_jni_env_ptr = &g_jni_functions;

static jint JNICALL mock_GetEnv(JavaVM *vm, void **env, jint version) {
    (void)vm;
    (void)version;
    if (env) *env = (void*)&g_jni_env_ptr;
    return JNI_OK;
}

static const struct JNIInvokeInterface g_jvm_functions = {
    .reserved0 = NULL,
    .reserved1 = NULL,
    .reserved2 = NULL,
    .GetEnv = mock_GetEnv,
};

static const struct JNIInvokeInterface* g_jvm_ptr = &g_jvm_functions;

JNIEnv* get_mock_jni_env(void) {
    return (JNIEnv*)&g_jni_env_ptr;
}

JavaVM* get_mock_java_vm(void) {
    return (JavaVM*)&g_jvm_ptr;
}
