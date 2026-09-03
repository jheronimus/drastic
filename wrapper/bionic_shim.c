#include "bionic_shim.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

typedef int32_t SLresult;
typedef uint32_t SLuint32;
typedef uint16_t SLuint16;
typedef uint8_t SLboolean;
typedef int32_t SLint32;
typedef SLuint32 SLmillisecond;
typedef SLint32 SLmillibel;

#define SL_OBJECT_STATE_REALIZED 0x00000001u
#define SL_PLAYSTATE_PLAYING     0x00000003u
#define SL_RESULT_SUCCESS        0u

struct SLObjectItf_;
typedef const struct SLObjectItf_ * const * SLObjectItf;
struct SLEngineItf_;
typedef const struct SLEngineItf_ * const * SLEngineItf;
struct SLPlayItf_;
typedef const struct SLPlayItf_ * const * SLPlayItf;
struct SLVolumeItf_;
typedef const struct SLVolumeItf_ * const * SLVolumeItf;
struct SLBufferQueueItf_;
typedef const struct SLBufferQueueItf_ * const * SLBufferQueueItf;
struct SLAndroidSimpleBufferQueueItf_;
typedef const struct SLAndroidSimpleBufferQueueItf_ * const * SLAndroidSimpleBufferQueueItf;
struct SLOutputMixItf_;
typedef const struct SLOutputMixItf_ * const * SLOutputMixItf;

typedef struct SLBufferQueueState_ {
    SLuint32 count;
    SLuint32 playIndex;
} SLBufferQueueState;

typedef struct SLAndroidSimpleBufferQueueState_ {
    SLuint32 count;
    SLuint32 index;
} SLAndroidSimpleBufferQueueState;

struct SLObjectItf_ {
    SLresult (*Realize)(SLObjectItf self, SLboolean async);
    SLresult (*Resume)(SLObjectItf self, SLboolean async);
    SLresult (*GetState)(SLObjectItf self, SLuint32 *pState);
    SLresult (*GetInterface)(SLObjectItf self, const SLInterfaceID iid, void *pInterface);
    SLresult (*RegisterCallback)(SLObjectItf self, void *callback, void *pContext);
    void (*AbortAsyncOperation)(SLObjectItf self);
    void (*Destroy)(SLObjectItf self);
    SLresult (*SetPriority)(SLObjectItf self, SLuint32 priority);
    SLresult (*GetPriority)(SLObjectItf self, SLuint32 *pPriority);
    SLresult (*SetLossOfControlInterfaces)(SLObjectItf self, SLuint16 numInterfaces, SLInterfaceID *pInterfaceIDs, SLboolean enabled);
};

struct SLEngineItf_ {
    SLresult (*CreateLEDDevice)(SLEngineItf self, SLObjectItf *pLedDevice, SLuint32 deviceID, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*CreateVibraDevice)(SLEngineItf self, SLObjectItf *pVibraDevice, SLuint32 deviceID, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*CreateAudioPlayer)(SLEngineItf self, SLObjectItf *pPlayer, void *pAudioSrc, void *pAudioSnk, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*CreateAudioRecorder)(SLEngineItf self, SLObjectItf *pRecorder, void *pAudioSrc, void *pAudioSnk, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*CreateMidiPlayer)(SLEngineItf self, SLObjectItf *pPlayer, void *pMIDISrc, void *pBankSrc, void *pAudioOutput, void *pVibra, void *pLEDArray, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*CreateListener)(SLEngineItf self, SLObjectItf *pListener, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*Create3DGroup)(SLEngineItf self, SLObjectItf *pGroup, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*CreateOutputMix)(SLEngineItf self, SLObjectItf *pMix, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*CreateMetadataExtractor)(SLEngineItf self, SLObjectItf *pMetadataExtractor, void *pDataSource, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*CreateExtensionObject)(SLEngineItf self, SLObjectItf *pObject, void *pParameters, SLuint32 objectID, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
    SLresult (*QueryNumSupportedInterfaces)(SLEngineItf self, SLuint32 *pNumSupportedInterfaces);
    SLresult (*QuerySupportedInterfaces)(SLEngineItf self, SLuint32 index, SLInterfaceID *pInterfaceId);
    SLresult (*QueryNumSupportedExtensions)(SLEngineItf self, SLuint32 *pNumExtensions);
    SLresult (*QuerySupportedExtension)(SLEngineItf self, SLuint32 index, char *pName, SLuint32 *pNameLength);
    SLresult (*IsExtensionSupported)(SLEngineItf self, const char *pExtensionName, SLboolean *pSupported);
};

struct SLPlayItf_ {
    SLresult (*SetPlayState)(SLPlayItf self, SLuint32 state);
    SLresult (*GetPlayState)(SLPlayItf self, SLuint32 *pState);
    SLresult (*GetDuration)(SLPlayItf self, SLmillisecond *pMsec);
    SLresult (*GetPosition)(SLPlayItf self, SLmillisecond *pMsec);
    SLresult (*RegisterCallback)(SLPlayItf self, void *callback, void *pContext);
    SLresult (*SetCallbackEventsMask)(SLPlayItf self, SLuint32 eventFlags);
    SLresult (*GetCallbackEventsMask)(SLPlayItf self, SLuint32 *pEventFlags);
    SLresult (*SetMarkerPosition)(SLPlayItf self, SLmillisecond mSec);
    SLresult (*ClearMarkerPosition)(SLPlayItf self);
    SLresult (*GetMarkerPosition)(SLPlayItf self, SLmillisecond *pMsec);
    SLresult (*SetPositionUpdatePeriod)(SLPlayItf self, SLmillisecond mSec);
    SLresult (*GetPositionUpdatePeriod)(SLPlayItf self, SLmillisecond *pMsec);
};

struct SLVolumeItf_ {
    SLresult (*SetVolumeLevel)(SLVolumeItf self, SLmillibel millibels);
    SLresult (*GetVolumeLevel)(SLVolumeItf self, SLmillibel *pMillibels);
    SLresult (*GetMaxVolumeLevel)(SLVolumeItf self, SLmillibel *pMaxMillibels);
    SLresult (*SetMute)(SLVolumeItf self, SLboolean mute);
    SLresult (*GetMute)(SLVolumeItf self, SLboolean *pMute);
    SLresult (*EnableStereoPosition)(SLVolumeItf self, SLboolean enable);
    SLresult (*IsEnabledStereoPosition)(SLVolumeItf self, SLboolean *pEnable);
    SLresult (*SetStereoPosition)(SLVolumeItf self, SLmillibel pPosition);
    SLresult (*GetStereoPosition)(SLVolumeItf self, SLmillibel *pPosition);
};

struct SLBufferQueueItf_ {
    SLresult (*Enqueue)(SLBufferQueueItf self, const void *pBuffer, SLuint32 size);
    SLresult (*Clear)(SLBufferQueueItf self);
    SLresult (*GetState)(SLBufferQueueItf self, SLBufferQueueState *pState);
    SLresult (*RegisterCallback)(SLBufferQueueItf self, void *callback, void *pContext);
};

struct SLAndroidSimpleBufferQueueItf_ {
    SLresult (*Enqueue)(SLAndroidSimpleBufferQueueItf self, const void *pBuffer, SLuint32 size);
    SLresult (*Clear)(SLAndroidSimpleBufferQueueItf self);
    SLresult (*GetState)(SLAndroidSimpleBufferQueueItf self, SLAndroidSimpleBufferQueueState *pState);
    SLresult (*RegisterCallback)(SLAndroidSimpleBufferQueueItf self, void *callback, void *pContext);
};

struct SLOutputMixItf_ {
    SLresult (*GetDestinationOutputDevice)(SLOutputMixItf self, void *pDeviceVolume);
    SLresult (*RegisterDeviceChangeCallback)(SLOutputMixItf self, void *callback, void *pContext);
    SLresult (*ReRoute)(SLOutputMixItf self, SLint32 numOutputDevices, void *pOutputDeviceIds);
};

static SLuint32 g_play_state = SL_PLAYSTATE_PLAYING;

#define BQ_MAX_PENDING 64
static pthread_mutex_t g_bq_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_bq_cond = PTHREAD_COND_INITIALIZER;
static unsigned int g_bq_sizes[BQ_MAX_PENDING];
static const void *g_bq_bufs[BQ_MAX_PENDING];
static int g_bq_n = 0;
static int g_bq_head = 0;
static void *g_bq_cb = NULL;
static void *g_bq_ctx = NULL;
static int g_bq_worker_started = 0;
static pthread_t g_bq_thread;
static void *bq_worker(void *arg);
static const struct SLBufferQueueItf_ * const g_bufqueue_itf_ref;

/* Routed to the libretro audio_sample_batch callback by drastic_libretro.c */
void (*g_drastic_audio_batch)(const void *data, size_t frames) = NULL;

/* Set on the first audio buffer: the core has finished booting. */
int g_drastic_audio_started = 0;
static volatile int s_shim_ff_active = 0;

void bionic_set_fast_forward(int active) {
    s_shim_ff_active = active;
}

static void bq_set_callback(void *cb, void *ctx) {
    pthread_mutex_lock(&g_bq_mutex);
    g_bq_cb = cb;
    g_bq_ctx = ctx;
    if (!g_bq_worker_started) {
        g_bq_worker_started = 1;
        pthread_mutex_unlock(&g_bq_mutex);
        pthread_create(&g_bq_thread, NULL, bq_worker, NULL);
        return;
    }
    pthread_mutex_unlock(&g_bq_mutex);
    pthread_cond_signal(&g_bq_cond);
}

static void bq_clear_played(void) {
    pthread_mutex_lock(&g_bq_mutex);
    g_bq_n = 0;
    g_bq_head = 0;
    pthread_mutex_unlock(&g_bq_mutex);
}

static void bq_push_played(const void *buf, unsigned int size) {
    pthread_mutex_lock(&g_bq_mutex);
    if (g_bq_n < BQ_MAX_PENDING) {
        g_bq_bufs[(g_bq_head + g_bq_n) % BQ_MAX_PENDING] = buf;
        g_bq_sizes[(g_bq_head + g_bq_n) % BQ_MAX_PENDING] = size;
        g_bq_n++;
    }
    pthread_mutex_unlock(&g_bq_mutex);
    pthread_cond_signal(&g_bq_cond);
}

static void *bq_worker(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&g_bq_mutex);
        while (g_bq_n == 0) {
            pthread_cond_wait(&g_bq_cond, &g_bq_mutex);
        }
        const void *buf = g_bq_bufs[g_bq_head];
        unsigned int size = g_bq_sizes[g_bq_head];
        g_bq_head = (g_bq_head + 1) % BQ_MAX_PENDING;
        g_bq_n--;
        void *cb = g_bq_cb;
        void *ctx = g_bq_ctx;
        void (*audio)(const void *, size_t) = g_drastic_audio_batch;
        pthread_mutex_unlock(&g_bq_mutex);

        if (audio) {
            audio(buf, size / 4);
        }
        /* The emulation thread is paced by the buffer-consumption callback.
         * Skip sleep during fast forward so the SPU advances at full speed. */
        if (!s_shim_ff_active) {
            double sec = (double)size / (4.0 * 44100.0);
            struct timespec ts;
            ts.tv_sec = (time_t)sec;
            ts.tv_nsec = (long)((sec - (double)ts.tv_sec) * 1e9);
            nanosleep(&ts, NULL);
        }

        if (cb) {
            ((void (*)(SLBufferQueueItf, void *))cb)(&g_bufqueue_itf_ref, ctx);
        }
    }
    return NULL;
}

static SLresult obj_GetInterface(SLObjectItf self, const SLInterfaceID iid, void *pInterface);

static SLresult obj_Realize(SLObjectItf self, SLboolean async) { (void)self; (void)async; fprintf(stderr, "[DraStic-OpenSLES] obj_Realize\n"); fflush(stderr); return SL_RESULT_SUCCESS; }
static SLresult obj_Resume(SLObjectItf self, SLboolean async) { (void)self; (void)async; return SL_RESULT_SUCCESS; }
static SLresult obj_GetState(SLObjectItf self, SLuint32 *pState) { (void)self; if (pState) *pState = SL_OBJECT_STATE_REALIZED; return SL_RESULT_SUCCESS; }
static SLresult obj_RegisterCallback(SLObjectItf self, void *cb, void *ctx) { (void)self; (void)cb; (void)ctx; return SL_RESULT_SUCCESS; }
static void obj_AbortAsyncOperation(SLObjectItf self) { (void)self; }
static void obj_Destroy(SLObjectItf self) { (void)self; }
static SLresult obj_SetPriority(SLObjectItf self, SLuint32 prio) { (void)self; (void)prio; return SL_RESULT_SUCCESS; }
static SLresult obj_GetPriority(SLObjectItf self, SLuint32 *pPrio) { (void)self; if (pPrio) *pPrio = 0; return SL_RESULT_SUCCESS; }
static SLresult obj_SetLossOfControlInterfaces(SLObjectItf self, SLuint16 n, SLInterfaceID *ids, SLboolean enabled) { (void)self; (void)n; (void)ids; (void)enabled; return SL_RESULT_SUCCESS; }

static SLresult engine_CreateOutputMix(SLEngineItf self, SLObjectItf *pMix, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
static SLresult engine_CreateAudioPlayer(SLEngineItf self, SLObjectItf *pPlayer, void *pAudioSrc, void *pAudioSnk, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
static SLresult engine_QueryNumSupportedInterfaces(SLEngineItf self, SLuint32 *pNum) { (void)self; if (pNum) *pNum = 0; return SL_RESULT_SUCCESS; }
static SLresult engine_QuerySupportedInterfaces(SLEngineItf self, SLuint32 index, SLInterfaceID *pInterfaceId) { (void)self; (void)index; (void)pInterfaceId; return SL_RESULT_SUCCESS; }
static SLresult engine_generic(SLEngineItf self) { (void)self; return SL_RESULT_SUCCESS; }

static SLresult play_SetPlayState(SLPlayItf self, SLuint32 state) { (void)self; g_play_state = state; return SL_RESULT_SUCCESS; }
static SLresult play_GetPlayState(SLPlayItf self, SLuint32 *pState) { (void)self; if (pState) *pState = g_play_state; return SL_RESULT_SUCCESS; }
static SLresult play_GetDuration(SLPlayItf self, SLmillisecond *pMsec) { (void)self; if (pMsec) *pMsec = 0; return SL_RESULT_SUCCESS; }
static SLresult play_GetPosition(SLPlayItf self, SLmillisecond *pMsec) { (void)self; if (pMsec) *pMsec = 0; return SL_RESULT_SUCCESS; }
static SLresult play_RegisterCallback(SLPlayItf self, void *cb, void *ctx) { (void)self; (void)cb; (void)ctx; return SL_RESULT_SUCCESS; }
static SLresult play_generic(SLPlayItf self) { (void)self; return SL_RESULT_SUCCESS; }

static SLresult bq_Enqueue(SLBufferQueueItf self, const void *pBuffer, SLuint32 size) {
    (void)self;
    if (!g_drastic_audio_started) {
        g_drastic_audio_started = 1;
        fprintf(stderr, "[DraStic-OpenSLES] audio started\n");
        fflush(stderr);
    }
    bq_push_played(pBuffer, size);
    return SL_RESULT_SUCCESS;
}
static SLresult bq_Clear(SLBufferQueueItf self) { (void)self; bq_clear_played(); return SL_RESULT_SUCCESS; }
static SLresult bq_GetState(SLBufferQueueItf self, SLBufferQueueState *pState) { (void)self; if (pState) { pState->count = 0; pState->playIndex = 0; } return SL_RESULT_SUCCESS; }
static SLresult bq_RegisterCallback(SLBufferQueueItf self, void *cb, void *ctx) {
    (void)self;
    fprintf(stderr, "[DraStic-OpenSLES] bq_RegisterCallback cb=%p ctx=%p\n", cb, ctx);
    fflush(stderr);
    bq_set_callback(cb, ctx);
    return SL_RESULT_SUCCESS;
}

static SLresult absq_Enqueue(SLAndroidSimpleBufferQueueItf self, const void *pBuffer, SLuint32 size) {
    (void)self;
    if (!g_drastic_audio_started) {
        g_drastic_audio_started = 1;
        fprintf(stderr, "[DraStic-OpenSLES] audio started (absq)\n");
        fflush(stderr);
    }
    bq_push_played(pBuffer, size);
    return SL_RESULT_SUCCESS;
}
static SLresult absq_Clear(SLAndroidSimpleBufferQueueItf self) { (void)self; bq_clear_played(); return SL_RESULT_SUCCESS; }
static SLresult absq_GetState(SLAndroidSimpleBufferQueueItf self, SLAndroidSimpleBufferQueueState *pState) { (void)self; if (pState) { pState->count = 0; pState->index = 0; } return SL_RESULT_SUCCESS; }
static SLresult absq_RegisterCallback(SLAndroidSimpleBufferQueueItf self, void *cb, void *ctx) {
    (void)self;
    fprintf(stderr, "[DraStic-OpenSLES] absq_RegisterCallback cb=%p ctx=%p\n", cb, ctx);
    fflush(stderr);
    bq_set_callback(cb, ctx);
    return SL_RESULT_SUCCESS;
}

static SLresult vol_SetVolumeLevel(SLVolumeItf self, SLmillibel mbs) { (void)self; (void)mbs; return SL_RESULT_SUCCESS; }
static SLresult vol_GetVolumeLevel(SLVolumeItf self, SLmillibel *pMbs) { (void)self; if (pMbs) *pMbs = 0; return SL_RESULT_SUCCESS; }
static SLresult vol_GetMaxVolumeLevel(SLVolumeItf self, SLmillibel *pMbs) { (void)self; if (pMbs) *pMbs = 0; return SL_RESULT_SUCCESS; }
static SLresult vol_SetMute(SLVolumeItf self, SLboolean mute) { (void)self; (void)mute; return SL_RESULT_SUCCESS; }
static SLresult vol_GetMute(SLVolumeItf self, SLboolean *pMute) { (void)self; if (pMute) *pMute = 0; return SL_RESULT_SUCCESS; }
static SLresult vol_EnableStereoPosition(SLVolumeItf self, SLboolean enable) { (void)self; (void)enable; return SL_RESULT_SUCCESS; }
static SLresult vol_SetStereoPosition(SLVolumeItf self, SLmillibel pos) { (void)self; (void)pos; return SL_RESULT_SUCCESS; }
static SLresult vol_GetStereoPosition(SLVolumeItf self, SLmillibel *pPos) { (void)self; if (pPos) *pPos = 0; return SL_RESULT_SUCCESS; }

static SLresult om_GetDestinationOutputDevice(SLOutputMixItf self, void *pDeviceVolume) { (void)self; if (pDeviceVolume) *(void **)pDeviceVolume = NULL; return SL_RESULT_SUCCESS; }
static SLresult om_RegisterDeviceChangeCallback(SLOutputMixItf self, void *cb, void *ctx) { (void)self; (void)cb; (void)ctx; return SL_RESULT_SUCCESS; }
static SLresult om_ReRoute(SLOutputMixItf self, SLint32 n, void *pIds) { (void)self; (void)n; (void)pIds; return SL_RESULT_SUCCESS; }

static const struct SLEngineItf_ g_engine_itf = {
    .CreateLEDDevice = (void *)engine_generic,
    .CreateVibraDevice = (void *)engine_generic,
    .CreateAudioPlayer = (void *)engine_CreateAudioPlayer,
    .CreateAudioRecorder = (void *)engine_generic,
    .CreateMidiPlayer = (void *)engine_generic,
    .CreateListener = (void *)engine_generic,
    .Create3DGroup = (void *)engine_generic,
    .CreateOutputMix = (void *)engine_CreateOutputMix,
    .CreateMetadataExtractor = (void *)engine_generic,
    .CreateExtensionObject = (void *)engine_generic,
    .QueryNumSupportedInterfaces = (void *)engine_QueryNumSupportedInterfaces,
    .QuerySupportedInterfaces = (void *)engine_QuerySupportedInterfaces,
    .QueryNumSupportedExtensions = (void *)engine_generic,
    .QuerySupportedExtension = (void *)engine_generic,
    .IsExtensionSupported = (void *)engine_generic,
};
static const struct SLEngineItf_ * const g_engine_itf_ref = &g_engine_itf;

static const struct SLPlayItf_ g_play_itf = {
    .SetPlayState = (void *)play_SetPlayState,
    .GetPlayState = (void *)play_GetPlayState,
    .GetDuration = (void *)play_GetDuration,
    .GetPosition = (void *)play_GetPosition,
    .RegisterCallback = (void *)play_RegisterCallback,
    .SetCallbackEventsMask = (void *)play_generic,
    .GetCallbackEventsMask = (void *)play_generic,
    .SetMarkerPosition = (void *)play_generic,
    .ClearMarkerPosition = (void *)play_generic,
    .GetMarkerPosition = (void *)play_generic,
    .SetPositionUpdatePeriod = (void *)play_generic,
    .GetPositionUpdatePeriod = (void *)play_generic,
};
static const struct SLPlayItf_ * const g_play_itf_ref = &g_play_itf;

static const struct SLVolumeItf_ g_volume_itf = {
    .SetVolumeLevel = (void *)vol_SetVolumeLevel,
    .GetVolumeLevel = (void *)vol_GetVolumeLevel,
    .GetMaxVolumeLevel = (void *)vol_GetMaxVolumeLevel,
    .SetMute = (void *)vol_SetMute,
    .GetMute = (void *)vol_GetMute,
    .EnableStereoPosition = (void *)vol_EnableStereoPosition,
    .IsEnabledStereoPosition = (void *)vol_GetMute,
    .SetStereoPosition = (void *)vol_SetStereoPosition,
    .GetStereoPosition = (void *)vol_GetStereoPosition,
};
static const struct SLVolumeItf_ * const g_volume_itf_ref = &g_volume_itf;

static const struct SLBufferQueueItf_ g_bufqueue_itf = {
    .Enqueue = (void *)bq_Enqueue,
    .Clear = (void *)bq_Clear,
    .GetState = (void *)bq_GetState,
    .RegisterCallback = (void *)bq_RegisterCallback,
};
static const struct SLBufferQueueItf_ * const g_bufqueue_itf_ref = &g_bufqueue_itf;

static const struct SLAndroidSimpleBufferQueueItf_ g_absbufqueue_itf = {
    .Enqueue = (void *)absq_Enqueue,
    .Clear = (void *)absq_Clear,
    .GetState = (void *)absq_GetState,
    .RegisterCallback = (void *)absq_RegisterCallback,
};
static const struct SLAndroidSimpleBufferQueueItf_ * const g_absbufqueue_itf_ref = &g_absbufqueue_itf;

static const struct SLOutputMixItf_ g_outmix_itf = {
    .GetDestinationOutputDevice = (void *)om_GetDestinationOutputDevice,
    .RegisterDeviceChangeCallback = (void *)om_RegisterDeviceChangeCallback,
    .ReRoute = (void *)om_ReRoute,
};
static const struct SLOutputMixItf_ * const g_outmix_itf_ref __attribute__((unused)) = &g_outmix_itf;

static const struct SLObjectItf_ g_engine_obj = {
    .Realize = obj_Realize,
    .Resume = obj_Resume,
    .GetState = obj_GetState,
    .GetInterface = obj_GetInterface,
    .RegisterCallback = obj_RegisterCallback,
    .AbortAsyncOperation = obj_AbortAsyncOperation,
    .Destroy = obj_Destroy,
    .SetPriority = obj_SetPriority,
    .GetPriority = obj_GetPriority,
    .SetLossOfControlInterfaces = obj_SetLossOfControlInterfaces,
};
static const struct SLObjectItf_ * const g_engine_obj_ref = &g_engine_obj;

static const struct SLObjectItf_ g_outmix_obj = {
    .Realize = obj_Realize,
    .Resume = obj_Resume,
    .GetState = obj_GetState,
    .GetInterface = obj_GetInterface,
    .RegisterCallback = obj_RegisterCallback,
    .AbortAsyncOperation = obj_AbortAsyncOperation,
    .Destroy = obj_Destroy,
    .SetPriority = obj_SetPriority,
    .GetPriority = obj_GetPriority,
    .SetLossOfControlInterfaces = obj_SetLossOfControlInterfaces,
};
static const struct SLObjectItf_ * const g_outmix_obj_ref = &g_outmix_obj;

static const struct SLObjectItf_ g_player_obj = {
    .Realize = obj_Realize,
    .Resume = obj_Resume,
    .GetState = obj_GetState,
    .GetInterface = obj_GetInterface,
    .RegisterCallback = obj_RegisterCallback,
    .AbortAsyncOperation = obj_AbortAsyncOperation,
    .Destroy = obj_Destroy,
    .SetPriority = obj_SetPriority,
    .GetPriority = obj_GetPriority,
    .SetLossOfControlInterfaces = obj_SetLossOfControlInterfaces,
};
static const struct SLObjectItf_ * const g_player_obj_ref = &g_player_obj;

static SLresult obj_GetInterface(SLObjectItf self, const SLInterfaceID iid, void *pInterface) {
    fprintf(stderr, "[DraStic-OpenSLES] obj_GetInterface self=%p iid=%p eng=%p outmix=%p player=%p\n",
            (void *)self, (const void *)iid, (void *)&g_engine_obj_ref, (void *)&g_outmix_obj_ref, (void *)&g_player_obj_ref);
    if (iid) {
        const unsigned char *b = (const unsigned char *)iid;
        fprintf(stderr, "[DraStic-OpenSLES]   iid bytes=%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
                b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    }
    fflush(stderr);
    if (iid && memcmp(iid, SL_IID_ENGINE, sizeof(SLInterfaceID_struct)) == 0 && self == &g_engine_obj_ref) {
        *(void **)pInterface = (void *)&g_engine_itf_ref;
        return SL_RESULT_SUCCESS;
    }
    if (self == &g_player_obj_ref) {
        if (iid && memcmp(iid, SL_IID_PLAY, sizeof(SLInterfaceID_struct)) == 0) { *(void **)pInterface = (void *)&g_play_itf_ref; return SL_RESULT_SUCCESS; }
        if (iid && memcmp(iid, SL_IID_BUFFERQUEUE, sizeof(SLInterfaceID_struct)) == 0) { *(void **)pInterface = (void *)&g_bufqueue_itf_ref; return SL_RESULT_SUCCESS; }
        if (iid && memcmp(iid, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, sizeof(SLInterfaceID_struct)) == 0) { *(void **)pInterface = (void *)&g_absbufqueue_itf_ref; return SL_RESULT_SUCCESS; }
        if (iid && memcmp(iid, SL_IID_VOLUME, sizeof(SLInterfaceID_struct)) == 0) { *(void **)pInterface = (void *)&g_volume_itf_ref; return SL_RESULT_SUCCESS; }
    }
    return SL_RESULT_SUCCESS;
}

static SLresult engine_CreateOutputMix(SLEngineItf self, SLObjectItf *pMix, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired) {
    (void)self; (void)numInterfaces; (void)pInterfaceIds; (void)pInterfaceRequired;
    fprintf(stderr, "[DraStic-OpenSLES] CreateOutputMix numIf=%u\n", numInterfaces); fflush(stderr);
    if (pMix) *pMix = &g_outmix_obj_ref;
    return SL_RESULT_SUCCESS;
}

static SLresult engine_CreateAudioPlayer(SLEngineItf self, SLObjectItf *pPlayer, void *pAudioSrc, void *pAudioSnk, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired) {
    (void)self; (void)pAudioSrc; (void)pAudioSnk; (void)numInterfaces; (void)pInterfaceIds; (void)pInterfaceRequired;
    fprintf(stderr, "[DraStic-OpenSLES] CreateAudioPlayer numIf=%u\n", numInterfaces); fflush(stderr);
    if (pPlayer) *pPlayer = &g_player_obj_ref;
    return SL_RESULT_SUCCESS;
}

int slCreateEngine(void *pEngine, uint32_t numOptions, const void *pEngineOptions, uint32_t numInterfaces, const void *pInterfaceIds, const void *pInterfaceRequired) {
    (void)numOptions;
    (void)pEngineOptions;
    (void)numInterfaces;
    (void)pInterfaceIds;
    (void)pInterfaceRequired;
    if (pEngine) *(void **)pEngine = (void *)&g_engine_obj_ref;
    fprintf(stderr, "[DraStic-OpenSLES] slCreateEngine ok\n");
    fflush(stderr);
    return 0; /* SL_RESULT_SUCCESS */
}

static const SLInterfaceID_struct g_SL_IID_ANDROIDSIMPLEBUFFERQUEUE = {0x19380d80, 0xf082, 0x11df, 0x8e, 0x92, {0x02, 0x42, 0xac, 0x11, 0x00, 0x02}};
static const SLInterfaceID_struct g_SL_IID_BUFFERQUEUE              = {0x05671000, 0xaa41, 0x11db, 0x9a, 0x94, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
static const SLInterfaceID_struct g_SL_IID_ENGINE                   = {0x8d0865f1, 0x2ec5, 0x11db, 0x89, 0x30, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
static const SLInterfaceID_struct g_SL_IID_PLAY                     = {0xef0cc080, 0x2ec5, 0x11db, 0x89, 0x30, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
static const SLInterfaceID_struct g_SL_IID_RECORD                   = {0xc035a900, 0x2ec5, 0x11db, 0x89, 0x30, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
static const SLInterfaceID_struct g_SL_IID_VOLUME                   = {0x09e8ed00, 0x2ec5, 0x11db, 0x85, 0x0a, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};

const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE = &g_SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
const SLInterfaceID SL_IID_BUFFERQUEUE              = &g_SL_IID_BUFFERQUEUE;
const SLInterfaceID SL_IID_ENGINE                   = &g_SL_IID_ENGINE;
const SLInterfaceID SL_IID_PLAY                     = &g_SL_IID_PLAY;
const SLInterfaceID SL_IID_RECORD                   = &g_SL_IID_RECORD;
const SLInterfaceID SL_IID_VOLUME                   = &g_SL_IID_VOLUME;

char *__strcpy_chk(char *dest, const char *src, size_t dest_len) {
    (void)dest_len;
    return strcpy(dest, src);
}

char *__strncpy_chk2(char *dest, const char *src, size_t n, size_t dest_len, size_t src_len) {
    (void)dest_len;
    (void)src_len;
    return strncpy(dest, src, n);
}

char *__strncpy_chk(char *dest, const char *src, size_t n, size_t dest_len) {
    (void)dest_len;
    return strncpy(dest, src, n);
}

void *__memcpy_chk(void *dest, const void *src, size_t n, size_t dest_len) {
    (void)dest_len;
    return memcpy(dest, src, n);
}

void *__memmove_chk(void *dest, const void *src, size_t n, size_t dest_len) {
    (void)dest_len;
    return memmove(dest, src, n);
}

void *__memset_chk(void *dest, int c, size_t n, size_t dest_len) {
    (void)dest_len;
    return memset(dest, c, n);
}

int __snprintf_chk(char *s, size_t maxlen, int flag, size_t slen, const char *format, ...) {
    (void)flag;
    (void)slen;
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(s, maxlen, format, ap);
    va_end(ap);
    return ret;
}

int __vsnprintf_chk(char *s, size_t maxlen, int flag, size_t slen, const char *format, va_list ap) {
    (void)flag;
    (void)slen;
    return vsnprintf(s, maxlen, format, ap);
}

int __vsprintf_chk(char *s, int flag, size_t slen, const char *format, va_list ap) {
    (void)flag;
    (void)slen;
    return vsprintf(s, format, ap);
}

int __sprintf_chk(char *s, int flag, size_t slen, const char *format, ...) {
    (void)flag;
    (void)slen;
    va_list ap;
    va_start(ap, format);
    int ret = vsprintf(s, format, ap);
    va_end(ap);
    return ret;
}

int __fprintf_chk(FILE *fp, int flag, const char *format, ...) {
    (void)flag;
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(fp, format, ap);
    va_end(ap);
    return ret;
}

int __printf_chk(int flag, const char *format, ...) {
    (void)flag;
    va_list ap;
    va_start(ap, format);
    int ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

int __vfprintf_chk(FILE *fp, int flag, const char *format, va_list ap) {
    (void)flag;
    return vfprintf(fp, format, ap);
}

char *__strchr_chk(const char *s, int c, size_t s_len) {
    (void)s_len;
    return strchr(s, c);
}

char *__strrchr_chk(const char *s, int c, size_t s_len) {
    (void)s_len;
    return strrchr(s, c);
}

size_t __strlen_chk(const char *s, size_t s_len) {
    (void)s_len;
    return strlen(s);
}

void *__memchr_chk(const void *s, int c, size_t n, size_t s_len) {
    (void)s_len;
    return memchr(s, c, n);
}

char *__stpcpy_chk(char *dest, const char *src, size_t dest_len) {
    (void)dest_len;
    return stpcpy(dest, src);
}

char *__stpncpy_chk(char *dest, const char *src, size_t n, size_t dest_len) {
    (void)dest_len;
    return stpncpy(dest, src, n);
}

int __open_2(const char *path, int flags) {
    int res = open(path, flags);
    fprintf(stderr, "[Bionic-Shim] __open_2('%s', 0x%x) -> %d\n", path ? path : "", flags, res);
    fflush(stderr);
    return res;
}

int __open64_2(const char *path, int flags) {
    int res = open(path, flags);
    fprintf(stderr, "[Bionic-Shim] __open64_2('%s', 0x%x) -> %d\n", path ? path : "", flags, res);
    fflush(stderr);
    return res;
}

ssize_t __read_chk(int fd, void *buf, size_t count, size_t buf_len) {
    (void)buf_len;
    ssize_t res = read(fd, buf, count);
    fprintf(stderr, "[Bionic-Shim] __read_chk(fd=%d, count=%zu) -> %zd\n", fd, count, res);
    fflush(stderr);
    return res;
}

ssize_t __write_chk(int fd, const void *buf, size_t count, size_t buf_len) {
    (void)buf_len;
    ssize_t res = write(fd, buf, count);
    fprintf(stderr, "[Bionic-Shim] __write_chk(fd=%d, count=%zu) -> %zd\n", fd, count, res);
    fflush(stderr);
    return res;
}

ssize_t __recvfrom_chk(int fd, void *buf, size_t len, size_t buf_len, int flags, void *src_addr, void *addrlen) {
    (void)buf_len;
    return recvfrom(fd, buf, len, flags, (struct sockaddr*)src_addr, (socklen_t*)addrlen);
}

mode_t __umask_chk(mode_t mask) {
    return umask(mask);
}
