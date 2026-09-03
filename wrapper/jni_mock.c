#include "jni_mock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef JNICALL
#define JNICALL
#endif

extern char g_system_dir[512];
extern char g_save_dir[512];

/* NativePathHandle layout matching libdrastic_arm64.so struct offsets
 * (verified by disassembly of the file-open helper at 0x1b4d4: it reads the
 * fd with `ldr w0, [x0, #8]` and falls back to `fopen([x0])` on a negative fd):
 * offset  0: char *filePath
 * offset  8: int fileFd
 * offset 16: char *fileName
 */
typedef struct {
    char *filePath;
    int fileFd;
    char *fileName;
} MockPathHandle;

static jfieldID g_field_filePath = (jfieldID)0;
static jfieldID g_field_fileName = (jfieldID)16;
static jfieldID g_field_fileFd   = (jfieldID)8;

static jmethodID g_method_open = (jmethodID)0x401;
static jmethodID g_method_rename = (jmethodID)0x402;
static jmethodID g_method_remove = (jmethodID)0x403;

static void ensure_dir(const char *path) {
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", path);
    char *p = strrchr(temp, '/');
    if (p) {
        *p = '\0';
        for (char *q = temp + 1; *q; q++) {
            if (*q == '/') {
                *q = '\0';
                mkdir(temp, 0777);
                *q = '/';
            }
        }
        mkdir(temp, 0777);
    }
}

static jstring JNICALL mock_NewString(JNIEnv *env, const jchar *unicode, jsize len) {
    (void)env;
    (void)unicode;
    (void)len;
    return (jstring)strdup("mock");
}

static jsize JNICALL mock_GetStringLength(JNIEnv *env, jstring str) {
    (void)env;
    if (!str) return 0;
    return (jsize)strlen((const char*)str);
}

static const jchar* JNICALL mock_GetStringChars(JNIEnv *env, jstring str, jboolean *isCopy) {
    (void)env;
    if (isCopy) *isCopy = JNI_FALSE;
    return (const jchar*)str;
}

static void JNICALL mock_ReleaseStringChars(JNIEnv *env, jstring str, const jchar *chars) {
    (void)env;
    (void)str;
    (void)chars;
}

static jstring JNICALL mock_NewStringUTF(JNIEnv *env, const char *utf) {
    (void)env;
    if (!utf) return NULL;
    return (jstring)strdup(utf);
}

static jsize JNICALL mock_GetStringUTFLength(JNIEnv *env, jstring str) {
    (void)env;
    if (!str) return 0;
    return (jsize)strlen((const char*)str);
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
    fprintf(stderr, "[JNI-Mock] NewByteArray len=%d\n", len);
    fflush(stderr);
    void *mem = malloc(sizeof(jsize) + len);
    if (!mem) return NULL;
    *(jsize*)mem = len;
    return (jbyteArray)((char*)mem + sizeof(jsize));
}

static jintArray JNICALL mock_NewIntArray(JNIEnv *env, jsize len) {
    (void)env;
    fprintf(stderr, "[JNI-Mock] NewIntArray len=%d\n", len);
    fflush(stderr);
    void *mem = malloc(sizeof(jsize) + len * sizeof(jint));
    if (!mem) return NULL;
    *(jsize*)mem = len;
    return (jintArray)((char*)mem + sizeof(jsize));
}

static jshortArray JNICALL mock_NewShortArray(JNIEnv *env, jsize len) {
    (void)env;
    fprintf(stderr, "[JNI-Mock] NewShortArray len=%d\n", len);
    fflush(stderr);
    void *mem = malloc(sizeof(jsize) + len * sizeof(jshort));
    if (!mem) return NULL;
    *(jsize*)mem = len;
    return (jshortArray)((char*)mem + sizeof(jsize));
}

static jbyte* JNICALL mock_GetByteArrayElements(JNIEnv *env, jbyteArray array, jboolean *isCopy) {
    (void)env;
    if (isCopy) *isCopy = JNI_FALSE;
    return (jbyte*)array;
}

static jshort* JNICALL mock_GetShortArrayElements(JNIEnv *env, jshortArray array, jboolean *isCopy) {
    (void)env;
    if (isCopy) *isCopy = JNI_FALSE;
    return (jshort*)array;
}

static jint* JNICALL mock_GetIntArrayElements(JNIEnv *env, jintArray array, jboolean *isCopy) {
    (void)env;
    if (isCopy) *isCopy = JNI_FALSE;
    return (jint*)array;
}

static void* JNICALL mock_GetPrimitiveArrayCritical(JNIEnv *env, jarray array, jboolean *isCopy) {
    (void)env;
    if (isCopy) *isCopy = JNI_FALSE;
    return (void*)array;
}

static void JNICALL mock_ReleaseByteArrayElements(JNIEnv *env, jbyteArray array, jbyte *elems, jint mode) {
    (void)env;
    (void)array;
    (void)elems;
    (void)mode;
}

static void JNICALL mock_ReleaseShortArrayElements(JNIEnv *env, jshortArray array, jshort *elems, jint mode) {
    (void)env;
    (void)array;
    (void)elems;
    (void)mode;
}

static void JNICALL mock_ReleaseIntArrayElements(JNIEnv *env, jintArray array, jint *elems, jint mode) {
    (void)env;
    (void)array;
    (void)elems;
    (void)mode;
}

static void JNICALL mock_GetIntArrayRegion(JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf) {
    (void)env;
    if (array && buf && len > 0) {
        memcpy(buf, (jint*)array + start, len * sizeof(jint));
    }
}

static void JNICALL mock_SetIntArrayRegion(JNIEnv *env, jintArray array, jsize start, jsize len, const jint *buf) {
    (void)env;
    if (array && buf && len > 0) {
        memcpy((jint*)array + start, buf, len * sizeof(jint));
    }
}

static void JNICALL mock_ReleasePrimitiveArrayCritical(JNIEnv *env, jarray array, void *carray, jint mode) {
    (void)env;
    (void)array;
    (void)carray;
    (void)mode;
}

static jsize JNICALL mock_GetArrayLength(JNIEnv *env, jarray array) {
    (void)env;
    if (!array) return 0;
    return *(jsize*)((char*)array - sizeof(jsize));
}

static jint JNICALL mock_GetVersion(JNIEnv *env) {
    (void)env;
    fprintf(stderr, "[JNI-Mock] GetVersion called\n");
    fflush(stderr);
    return 0x00010006; /* JNI 1.6 */
}

static jclass JNICALL mock_FindClass(JNIEnv *env, const char *name) {
    (void)env;
    fprintf(stderr, "[JNI-Mock] FindClass: %s\n", name ? name : "NULL");
    fflush(stderr);
    return (jclass)0x100;
}

static char g_method_name[128] = "";

static jmethodID JNICALL mock_GetMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    (void)env;
    (void)clazz;
    fprintf(stderr, "[JNI-Mock] GetMethodID: name=%s sig=%s\n", name ? name : "", sig ? sig : "");
    fflush(stderr);
    if (name) {
        snprintf(g_method_name, sizeof(g_method_name), "%s", name);
    }
    return (jmethodID)0x200;
}

static jfieldID JNICALL mock_GetFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    (void)env;
    (void)clazz;
    fprintf(stderr, "[JNI-Mock] GetFieldID: name=%s sig=%s\n", name ? name : "", sig ? sig : "");
    fflush(stderr);
    if (name && strcmp(name, "fileFd") == 0) return g_field_fileFd;
    if (name && strcmp(name, "filePath") == 0) return g_field_filePath;
    if (name && strcmp(name, "fileName") == 0) return g_field_fileName;
    return (jfieldID)0x300;
}

static jmethodID JNICALL mock_GetStaticMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    (void)env;
    (void)clazz;
    fprintf(stderr, "[JNI-Mock] GetStaticMethodID: name=%s sig=%s\n", name ? name : "", sig ? sig : "");
    fflush(stderr);
    if (name && strcmp(name, "open") == 0) return g_method_open;
    if (name && strcmp(name, "rename") == 0) return g_method_rename;
    if (name && strcmp(name, "remove") == 0) return g_method_remove;
    return (jmethodID)0x400;
}

static jfieldID JNICALL mock_GetStaticFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    (void)env;
    (void)clazz;
    fprintf(stderr, "[JNI-Mock] GetStaticFieldID: name=%s sig=%s\n", name ? name : "", sig ? sig : "");
    fflush(stderr);
    return (jfieldID)0x500;
}

static jint JNICALL mock_GetIntField(JNIEnv *env, jobject obj, jfieldID fieldID) {
    (void)env;
    if (obj) {
        return *(jint*)((char*)obj + (uintptr_t)fieldID);
    }
    return 0;
}

static jobject JNICALL mock_GetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID) {
    if (obj) {
        char *str = *(char**)((char*)obj + (uintptr_t)fieldID);
        return mock_NewStringUTF(env, str);
    }
    return NULL;
}

static jobject JNICALL mock_CallStaticObjectMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args) {
    (void)clazz;
    if (methodID == g_method_open) {
        jstring jpath = va_arg(args, jstring);
        jstring jmode = va_arg(args, jstring);
        const char *path = mock_GetStringUTFChars(env, jpath, NULL);
        const char *mode = mock_GetStringUTFChars(env, jmode, NULL);

        char fullpath[1024];
        if (path && strncmp(path, "User/savestates", 15) == 0) {
            ensure_dir("/tmp/drastic_savestates");
            snprintf(fullpath, sizeof(fullpath), "/tmp/drastic_savestates/%s", path + 15);
        } else if (path && (path[0] == '/' || path[0] == '.')) {
            snprintf(fullpath, sizeof(fullpath), "%s", path);
        } else {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", g_system_dir, path ? path : "");
        }

        bool is_read_only = mode && (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0);
        int fd = -1;

        if (!is_read_only) {
            ensure_dir(fullpath);
            fd = open(fullpath, O_RDWR | O_CREAT, 0666);
        } else if (access(fullpath, F_OK) == 0) {
            fd = open(fullpath, O_RDONLY);
        } else if (path) {
            /* Try fallback paths */
            char altpath[1024];
            snprintf(altpath, sizeof(altpath), "/mnt/sdcard/Bios/NDS/%s", path);
            if (access(altpath, F_OK) == 0) {
                fd = open(altpath, O_RDONLY);
                snprintf(fullpath, sizeof(fullpath), "%s", altpath);
            } else {
                snprintf(altpath, sizeof(altpath), "/mnt/sdcard/Emus/minime/NDS.pak/%s", path);
                if (access(altpath, F_OK) == 0) {
                    fd = open(altpath, O_RDONLY);
                    snprintf(fullpath, sizeof(fullpath), "%s", altpath);
                }
            }
        }

        /* DraStic allocates the ROM buffer from the NDS header's declared size
         * (offset 0x80) but CRCs the file's actual size. Padded ROMs (file
         * larger than declared) therefore overrun the buffer. Match DraStic's
         * "trim roms" behavior: patch the header size field to the file size. */
        if (fd >= 0 && is_read_only) {
            struct stat st;
            if (fstat(fd, &st) == 0 && st.st_size >= 0x84) {
                unsigned int hdr_size = 0;
                ssize_t got = pread(fd, &hdr_size, 4, 0x80);
                if (got == 4 && hdr_size != 0 && hdr_size < (unsigned int)st.st_size) {
                    int wfd = open(fullpath, O_RDWR);
                    if (wfd >= 0) {
                        unsigned int real = (unsigned int)st.st_size;
                        pwrite(wfd, &real, 4, 0x80);
                        close(wfd);
                        fprintf(stderr, "[JNI-Mock] ROM header size patched 0x%x -> 0x%x (%s)\n",
                                hdr_size, real, path ? path : "");
                        fflush(stderr);
                    }
                }
            }
        }

        fprintf(stderr, "[JNI-Mock] DraSticPathCache.open('%s', '%s') -> fd=%d (fullpath=%s)\n",
                path ? path : "", mode ? mode : "", fd, fullpath);
        fflush(stderr);

        if (fd < 0) {
            /* DraStic's Android open() returns null when a file cannot be
             * opened; the core checks the handle, not the fd, so a handle
             * with fd=-1 makes it read from an invalid descriptor (EBADF). */
            return NULL;
        }

        MockPathHandle *handle = (MockPathHandle*)calloc(1, sizeof(MockPathHandle));
        if (handle) {
            handle->filePath = strdup(fullpath);
            handle->fileName = strdup(path ? path : "");
            handle->fileFd = fd;
        }
        return (jobject)handle;
    }
    return NULL;
}

static jobject JNICALL mock_CallStaticObjectMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    jobject res = mock_CallStaticObjectMethodV(env, clazz, methodID, args);
    va_end(args);
    return res;
}

static void resolve_mock_path(const char *in, char *out, size_t outsz) {
    if (!in || !in[0]) {
        out[0] = '\0';
        return;
    }
    if (strncmp(in, "User/savestates", 15) == 0) {
        snprintf(out, outsz, "/tmp/drastic_savestates/%s", in + 15);
        return;
    }
    if (in[0] == '/' || in[0] == '.') {
        snprintf(out, outsz, "%s", in);
    } else {
        snprintf(out, outsz, "%s/%s", g_system_dir, in);
    }
}

static jboolean JNICALL mock_CallStaticBooleanMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args) {
    (void)clazz;
    if (methodID == g_method_rename) {
        jstring jold = va_arg(args, jstring);
        jstring jnew = va_arg(args, jstring);
        const char *oldp = mock_GetStringUTFChars(env, jold, NULL);
        const char *newp = mock_GetStringUTFChars(env, jnew, NULL);
        char fold[1024], fnew[1024];
        resolve_mock_path(oldp, fold, sizeof(fold));
        resolve_mock_path(newp, fnew, sizeof(fnew));
        int r = rename(fold, fnew);
        fprintf(stderr, "[JNI-Mock] rename('%s' -> '%s') -> %d\n", fold, fnew, r);
        fflush(stderr);
        return r == 0 ? JNI_TRUE : JNI_FALSE;
    }
    if (methodID == g_method_remove) {
        jstring jpath = va_arg(args, jstring);
        const char *path = mock_GetStringUTFChars(env, jpath, NULL);
        char fpath[1024];
        resolve_mock_path(path, fpath, sizeof(fpath));
        int r = unlink(fpath);
        fprintf(stderr, "[JNI-Mock] remove('%s') -> %d\n", fpath, r);
        fflush(stderr);
        return r == 0 ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

static jboolean JNICALL mock_CallStaticBooleanMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    jboolean res = mock_CallStaticBooleanMethodV(env, clazz, methodID, args);
    va_end(args);
    return res;
}

static jobject JNICALL mock_NewGlobalRef(JNIEnv *env, jobject obj) {
    (void)env;
    return obj ? obj : (jobject)0x600;
}

static void JNICALL mock_DeleteGlobalRef(JNIEnv *env, jobject obj) {
    (void)env;
    (void)obj;
}

static void JNICALL mock_DeleteLocalRef(JNIEnv *env, jobject obj) {
    (void)env;
    (void)obj;
}

static jobject JNICALL mock_NewLocalRef(JNIEnv *env, jobject obj) {
    (void)env;
    return obj ? obj : (jobject)0x700;
}

static jclass JNICALL mock_GetObjectClass(JNIEnv *env, jobject obj) {
    (void)env;
    (void)obj;
    return (jclass)0x800;
}

static jint JNICALL mock_RegisterNatives(JNIEnv *env, jclass clazz, const void *methods, jint nMethods) {
    (void)env;
    (void)clazz;
    (void)methods;
    fprintf(stderr, "[JNI-Mock] RegisterNatives nMethods=%d\n", nMethods);
    fflush(stderr);
    return JNI_OK;
}

static jint JNICALL mock_UnregisterNatives(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;
    return JNI_OK;
}

static jthrowable JNICALL mock_ExceptionOccurred(JNIEnv *env) {
    (void)env;
    return NULL;
}

static void JNICALL mock_ExceptionClear(JNIEnv *env) {
    (void)env;
}

static void JNICALL mock_ExceptionDescribe(JNIEnv *env) {
    (void)env;
}

static intptr_t JNICALL mock_CallLogged(const char *what) {
    fprintf(stderr, "[JNI-Mock] %s: %s\n", what, g_method_name[0] ? g_method_name : "?");
    fflush(stderr);
    return 0;
}
static intptr_t JNICALL mock_CallObjectMethod(void) { return mock_CallLogged("CallObjectMethod"); }
static intptr_t JNICALL mock_CallBooleanMethod(void) { return mock_CallLogged("CallBooleanMethod"); }
static intptr_t JNICALL mock_CallIntMethod(void) { return mock_CallLogged("CallIntMethod"); }
static intptr_t JNICALL mock_CallLongMethod(void) { return mock_CallLogged("CallLongMethod"); }
static intptr_t JNICALL mock_CallVoidMethod(void) { return mock_CallLogged("CallVoidMethod"); }

static const struct JNINativeInterface g_jni_functions = {
    .GetVersion = mock_GetVersion,
    .FindClass = mock_FindClass,
    .GetMethodID = mock_GetMethodID,
    .GetFieldID = mock_GetFieldID,
    .GetStaticMethodID = mock_GetStaticMethodID,
    .GetStaticFieldID = mock_GetStaticFieldID,

    .GetIntField = mock_GetIntField,
    .GetObjectField = mock_GetObjectField,

    .CallStaticObjectMethod = mock_CallStaticObjectMethod,
    .CallStaticObjectMethodV = mock_CallStaticObjectMethodV,

    .CallStaticBooleanMethod = mock_CallStaticBooleanMethod,
    .CallStaticBooleanMethodV = mock_CallStaticBooleanMethodV,

    .GetObjectClass = mock_GetObjectClass,
    .NewGlobalRef = mock_NewGlobalRef,
    .DeleteGlobalRef = mock_DeleteGlobalRef,
    .DeleteLocalRef = mock_DeleteLocalRef,
    .NewLocalRef = mock_NewLocalRef,

    .ExceptionOccurred = mock_ExceptionOccurred,
    .ExceptionDescribe = mock_ExceptionDescribe,
    .ExceptionClear = mock_ExceptionClear,

    .NewString = mock_NewString,
    .GetStringLength = mock_GetStringLength,
    .GetStringChars = mock_GetStringChars,
    .ReleaseStringChars = mock_ReleaseStringChars,

    .NewStringUTF = mock_NewStringUTF,
    .GetStringUTFLength = mock_GetStringUTFLength,
    .GetStringUTFChars = mock_GetStringUTFChars,
    .ReleaseStringUTFChars = mock_ReleaseStringUTFChars,

    .GetArrayLength = mock_GetArrayLength,
    .NewByteArray = mock_NewByteArray,
    .NewShortArray = mock_NewShortArray,
    .NewIntArray = mock_NewIntArray,

    .GetByteArrayElements = mock_GetByteArrayElements,
    .GetShortArrayElements = mock_GetShortArrayElements,
    .GetIntArrayElements = mock_GetIntArrayElements,

    .ReleaseByteArrayElements = mock_ReleaseByteArrayElements,
    .ReleaseShortArrayElements = mock_ReleaseShortArrayElements,
    .ReleaseIntArrayElements = mock_ReleaseIntArrayElements,

    .GetIntArrayRegion = mock_GetIntArrayRegion,
    .SetIntArrayRegion = mock_SetIntArrayRegion,

    .GetPrimitiveArrayCritical = mock_GetPrimitiveArrayCritical,
    .ReleasePrimitiveArrayCritical = mock_ReleasePrimitiveArrayCritical,

    .RegisterNatives = mock_RegisterNatives,
    .UnregisterNatives = mock_UnregisterNatives,

    .CallObjectMethod = (void*)mock_CallObjectMethod,
    .CallBooleanMethod = (void*)mock_CallBooleanMethod,
    .CallByteMethod = (void*)mock_CallLogged,
    .CallCharMethod = (void*)mock_CallLogged,
    .CallShortMethod = (void*)mock_CallLogged,
    .CallIntMethod = (void*)mock_CallIntMethod,
    .CallLongMethod = (void*)mock_CallLongMethod,
    .CallFloatMethod = (void*)mock_CallLogged,
    .CallDoubleMethod = (void*)mock_CallLogged,
    .CallVoidMethod = (void*)mock_CallVoidMethod,

    .CallStaticByteMethod = (void*)mock_CallLogged,
    .CallStaticCharMethod = (void*)mock_CallLogged,
    .CallStaticShortMethod = (void*)mock_CallLogged,
    .CallStaticIntMethod = (void*)mock_CallLogged,
    .CallStaticLongMethod = (void*)mock_CallLogged,
    .CallStaticFloatMethod = (void*)mock_CallLogged,
    .CallStaticDoubleMethod = (void*)mock_CallLogged,
    .CallStaticVoidMethod = (void*)mock_CallLogged,
};

static const struct JNINativeInterface* g_jni_env_ptr = &g_jni_functions;

static jint JNICALL mock_GetEnv(JavaVM *vm, void **env, jint version) {
    (void)vm;
    fprintf(stderr, "[JNI-Mock] mock_GetEnv version=0x%x\n", version);
    fflush(stderr);
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
