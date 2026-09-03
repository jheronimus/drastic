#include "drastic_libretro.h"
#include "bionic_shim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <linux/input.h>

#define DRASTIC_LIB_NAME "libdrastic_arm64.so"

/* DraStic JNI Button Bitmask Order */
#define NDS_KEY_UP     (1 << 0)
#define NDS_KEY_DOWN   (1 << 1)
#define NDS_KEY_LEFT   (1 << 2)
#define NDS_KEY_RIGHT  (1 << 3)
#define NDS_KEY_A      (1 << 4)
#define NDS_KEY_B      (1 << 5)
#define NDS_KEY_X      (1 << 6)
#define NDS_KEY_Y      (1 << 7)
#define NDS_KEY_L      (1 << 8)
#define NDS_KEY_R      (1 << 9)
#define NDS_KEY_START  (1 << 10)
#define NDS_KEY_SELECT (1 << 11)

/* Screen Layout Modes */
enum layout_mode {
   LAYOUT_SINGLE = 0,
   LAYOUT_VERTICAL,
   LAYOUT_HORIZONTAL
};

enum touch_mode {
   TOUCH_MODE_TOUCHSCREEN = 0,
   TOUCH_MODE_ANALOG
};

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static void *g_drastic_handle = NULL;
static fn_JNI_OnLoad p_JNI_OnLoad = NULL;
static fn_onInit p_onInit = NULL;
static fn_startGame p_startGame = NULL;
static fn_applyConfig p_applyConfig = NULL;
static fn_updateFrame p_updateFrame = NULL;

static uint32_t g_opt_frameskip = 0;
static uint32_t g_opt_frameskip_type = 0;
static uint32_t g_opt_fastforward_speed = 3;
static uint32_t g_opt_cpu_threads = 3;
static bool g_opt_hires_3d = false;
static bool g_opt_threaded_3d = true;
static bool g_opt_initial_bottom = false;
static uint32_t g_opt_slot2 = 2;          /* 2 = Rumble Pak (default) */
static bool g_opt_direct_boot = true;      /* Direct Game Boot (default) */
static bool g_opt_preload_rom = true;      /* Preload ROM to RAM (default) */
static bool g_opt_rtc = true;              /* Sync System Clock (default) */
static bool g_opt_edge_marking = true;     /* 3D Edge Marking (default) */
static bool g_opt_cheats = true;           /* Action Replay Cheats (default) */
static bool g_opt_mic = false;             /* Microphone Emulation (default Disabled) */
static bool g_opt_audio_filter = true;     /* Audio Interpolation (default High Quality) */
static int g_opt_cursor_speed = 4000;      /* Normal cursor sensitivity (default) */
static uint32_t g_opt_autofire = 0;        /* Autofire Off (default) */
static char g_rom_basename[512] = "";

volatile int g_fast_forward_active = 0;
static struct retro_rumble_interface g_rumble = {0};

typedef jint (*fn_getRumbleState)(JNIEnv *env, jobject obj);
static fn_getRumbleState p_getRumbleState = NULL;

static uint64_t build_drastic_config(void) {
   uint64_t value = 0;
   if (g_fast_forward_active) {
      value |= (uint64_t)3;         /* frameskip = 3 */
      value |= (uint64_t)1 << 5;    /* frameskip_type = 1 (Manual 3 frameskip) */
      value |= (uint64_t)6 << 12;   /* fastforward_speed = 6 (0 us delay / Unlimited) */
      value |= (UINT64_C(1) << 29); /* FastForwardEnabled = 1 */
   } else {
      value |= (uint64_t)(g_opt_frameskip & 0x1f);
      value |= (uint64_t)(g_opt_frameskip_type & 0x7) << 5;
      value |= (uint64_t)(g_opt_fastforward_speed & 0xf) << 12;
   }
   value |= (uint64_t)2 << 8;    /* audio_latency = 2 (Medium) */
   value |= (uint64_t)(g_opt_cpu_threads & 0x7) << 16;
   value |= (uint64_t)(g_opt_autofire & 0x7) << 32;
   if (g_opt_mic) {
      value |= (uint64_t)1 << 37; /* mic_level = 1 (white noise sample) */
      value |= (UINT64_C(1) << 26); /* MicEnabled = 1 */
   }
   value |= (uint64_t)(g_opt_slot2 & 0xf) << 43; /* slot2 device */

   value |= (UINT64_C(1) << 31); /* SoundEnabled = 1 */
   if (g_opt_threaded_3d)
      value |= (UINT64_C(1) << 28); /* Threaded3D = 1 */
   if (g_opt_hires_3d)
      value |= (UINT64_C(1) << 41); /* Hires3D = 1 */
   if (g_opt_cheats)
      value |= (UINT64_C(1) << 27); /* CheatsEnabled = 1 */
   if (g_opt_audio_filter)
      value |= (UINT64_C(1) << 24); /* SoundVolumeInterpolation = 1 */
   if (!g_opt_edge_marking)
      value |= (UINT64_C(1) << 40); /* DisableEdgeMarking = 1 */
   if (g_opt_rtc)
      value |= (UINT64_C(1) << 39); /* RtcSystemTime = 1 */
   if (g_opt_direct_boot)
      value |= (UINT64_C(1) << 50); /* DirectBoot = 1 */
   if (g_opt_preload_rom)
      value |= (UINT64_C(1) << 48); /* PreloadRoms = 1 */

   value |= (UINT64_C(1) << 25); /* BackupInSavestates = 1 */
   value |= (UINT64_C(1) << 23); /* Use16BitColor = 1 */
   value |= (UINT64_C(1) << 42); /* LuaEnabled = 1 */
   return value;
}
static fn_getScreenBuffers p_getScreenBuffers = NULL;
static fn_getSnapshots16 p_getSnapshots16 = NULL;
static fn_renderFrame p_renderFrame = NULL;
static fn_updateInput p_updateInput = NULL;
static fn_saveState p_saveState = NULL;
static fn_loadState p_loadState = NULL;
static fn_setAutosaveInterval p_setAutosaveInterval = NULL;
static fn_setAudioVolume p_setAudioVolume = NULL;
static fn_resetDS p_resetDS = NULL;
static fn_quitSystem p_quitSystem = NULL;
static fn_signalScreen p_signalScreen = NULL;
static fn_waitScreen p_waitScreen = NULL;

static bool g_initialized = false;
static bool g_game_loaded = false;
static bool g_running = false;
extern int g_drastic_audio_started;
static pthread_t g_start_thread;
static char g_rom_path[PATH_MAX];

#define NDS_SCREEN_W 256
#define NDS_SCREEN_H 192
static uint16_t g_raw_screen_buffer[NDS_SCREEN_W * NDS_SCREEN_H * 2];
static uint16_t g_output_buffer[512 * 384];
static jintArray g_jni_screen_top = NULL;
static jintArray g_jni_screen_bottom = NULL;

typedef struct {
   int16_t l;
   int16_t r;
} audio_frame_t;

#define AUDIO_RING_FRAMES 16384
static audio_frame_t g_audio_ring[AUDIO_RING_FRAMES];
static size_t g_audio_ring_read = 0;
static size_t g_audio_ring_write = 0;
static pthread_mutex_t g_audio_ring_mutex = PTHREAD_MUTEX_INITIALIZER;

static int g_ts_fd = -1;
static int g_ts_raw_x = 0;
static int g_ts_raw_y = 0;
static bool g_ts_down = false;

static void ts_poll_events(void) {
   if (g_ts_fd < 0) {
      g_ts_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
      if (g_ts_fd < 0) return;
   }
   struct input_event ev[32];
   ssize_t rd;
   while ((rd = read(g_ts_fd, ev, sizeof(ev))) > 0) {
      size_t count = rd / sizeof(struct input_event);
      for (size_t i = 0; i < count; i++) {
         if (ev[i].type == EV_ABS) {
            if (ev[i].code == ABS_MT_POSITION_X || ev[i].code == ABS_X) {
               g_ts_raw_x = ev[i].value;
            } else if (ev[i].code == ABS_MT_POSITION_Y || ev[i].code == ABS_Y) {
               g_ts_raw_y = ev[i].value;
            }
         } else if (ev[i].type == EV_KEY) {
            if (ev[i].code == BTN_TOUCH) {
               g_ts_down = (ev[i].value != 0);
            }
         }
      }
   }
}

/* SAVE RAM: a memfd-backed file that doubles as the core's battery save
 * (User/backup/dseins.dsv). The core reads/writes it via its own fd; minarch
 * reads/writes the same pages via retro_get_memory_data. */
#define SAVE_RAM_SIZE (1 * 1024 * 1024)
static uint8_t *g_save_ram = NULL;
static size_t g_save_ram_size = 0;
static char g_battery_path[1024];
static char g_savestates_dir[1024];

static enum layout_mode g_layout_mode = LAYOUT_SINGLE;
static enum touch_mode g_touch_mode = TOUCH_MODE_TOUCHSCREEN;
static bool g_active_screen = true; /* true = bottom (interactive/menus), false = top */

/* Frontend-provided logger; falls back to stderr. */
static retro_log_printf_t g_log = NULL;

#define LOGI(...) do { if (g_log) g_log(RETRO_LOG_INFO, __VA_ARGS__); else fprintf(stderr, __VA_ARGS__); } while (0)
#define LOGW(...) do { if (g_log) g_log(RETRO_LOG_WARN, __VA_ARGS__); else fprintf(stderr, __VA_ARGS__); } while (0)
#define LOGE(...) do { if (g_log) g_log(RETRO_LOG_ERROR, __VA_ARGS__); else fprintf(stderr, __VA_ARGS__); } while (0)

static int g_cursor_x = 128;
static int g_cursor_y = 96;

/* System Paths */
char g_system_dir[512] = "./bios";
char g_save_dir[512] = "./saves";

void retro_set_environment(retro_environment_t cb) {
   environ_cb = cb;

   static const struct retro_variable vars[] = {
      { "drastic_screen_layout", "Screen Layout; Single Screen|Vertical (256x384)|Side by Side (512x192)" },
      { "drastic_initial_screen", "Default Screen (Single); Top Screen|Bottom Screen" },
      { "drastic_hires_3d", "High-Resolution 3D (2x); Disabled|Enabled" },
      { "drastic_threaded_3d", "Threaded 3D Rasterizer; Enabled|Disabled" },
      { "drastic_frameskip", "Frameskip; None|Auto 1|Auto 2|Manual 1|Manual 2" },
      { "drastic_fastforward_speed", "Fast Forward Speed; 3x|2x|4x|5x|Unlimited" },
      { "drastic_cpu_threads", "CPU JIT Threads; 3|2|1" },
      { "drastic_slot2_device", "Slot 2 Device; Rumble Pak|GBA Cartridge|RAM Expansion (8MB)|None" },
      { "drastic_direct_boot", "Boot Mode; Direct Game Boot|NDS Firmware GUI" },
      { "drastic_preload_rom", "Preload ROM to RAM; Enabled|Disabled" },
      { "drastic_rtc", "Real-Time Clock; Sync System Clock|Fixed Time" },
      { "drastic_edge_marking", "3D Edge Marking; Enabled|Disabled" },
      { "drastic_cheats", "Action Replay Cheats; Enabled|Disabled" },
      { "drastic_mic_mode", "Microphone Emulation; Disabled|Sample White Noise" },
      { "drastic_audio_filter", "Sound Quality; Interpolated (High Quality)|Raw (Fast)" },
      { "drastic_touch_mode", "Touch Input Mode; Physical Touchscreen|Analog Cursor" },
      { "drastic_cursor_speed", "Stylus Cursor Speed; Normal|Fast|Slow" },
      { "drastic_autofire", "Autofire Rate; Off|Fast (10 Hz)|Slow (5 Hz)" },
      { NULL, NULL }
   };
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

   static const struct retro_controller_description port1[] = {
      { "Nintendo DS", RETRO_DEVICE_JOYPAD },
      { NULL, 0 },
   };
   static const struct retro_controller_info ports[] = {
      { port1, 1 },
      { NULL, 0 },
   };
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

   struct retro_log_callback log;
   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
      g_log = log.log;
   }

   cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &g_rumble);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
static void audio_batch_wrap(const void *data, size_t frames) {
   if (!data || frames == 0) return;
   pthread_mutex_lock(&g_audio_ring_mutex);
   const audio_frame_t *src = (const audio_frame_t*)data;
   for (size_t i = 0; i < frames; i++) {
      size_t next = (g_audio_ring_write + 1) % AUDIO_RING_FRAMES;
      if (next == g_audio_ring_read) break; /* ring full */
      g_audio_ring[g_audio_ring_write] = src[i];
      g_audio_ring_write = next;
   }
   pthread_mutex_unlock(&g_audio_ring_mutex);
}

void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }

static volatile int *g_p_minarch_ff = NULL;

static void init_minarch_ff_pointer(void) {
   if (!audio_batch_cb) return;
   const uint32_t *code = (const uint32_t*)audio_batch_cb;
   /* Match audio_sample_batch_callback instructions in minarch:
    * +0x20: adrp x21, <page>
    * +0x38: add  x0, x21, #0xc48 */
   uint32_t w_adrp = code[8];   /* +0x20 */
   uint32_t w_add  = code[14];  /* +0x38 */

   if ((w_adrp & 0x9f000000) == 0x90000000 && (w_add & 0xffc00000) == 0x91000000) {
      int32_t immlo = (w_adrp >> 29) & 3;
      int32_t immhi = (w_adrp >> 5) & 0x7ffff;
      int32_t imm = (immhi << 2) | immlo;
      if (imm & (1 << 20)) imm -= (1 << 21);

      uintptr_t pc = (uintptr_t)&code[8];
      uintptr_t page = (pc & ~0xfffULL) + ((int64_t)imm << 12);
      uint32_t imm12 = (w_add >> 10) & 0xfff;

      /* fast_forward is at struct_base + 4 */
      g_p_minarch_ff = (volatile int*)(page + imm12 + 4);
      /* rewind_enable is at struct_base + 0x138: disable rewind snapshots */
      volatile int *p_minarch_rewind = (volatile int*)(page + imm12 + 0x138);
      *p_minarch_rewind = 0;
      LOGI("[DraStic] Hooked minarch fast_forward at %p (val=%d), disabled rewind at %p\n",
           (void*)g_p_minarch_ff, *g_p_minarch_ff, (void*)p_minarch_rewind);
   } else {
      LOGW("[DraStic] Could not resolve minarch fast_forward from audio_batch_cb\n");
   }
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
   audio_batch_cb = cb;
   g_drastic_audio_batch = (void (*)(const void *, size_t))audio_batch_wrap;
   init_minarch_ff_pointer();
}
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

static void update_variables(void) {
   struct retro_variable var = {0};
   enum layout_mode prev_layout = g_layout_mode;
   uint64_t prev_config = build_drastic_config();

   var.key = "drastic_screen_layout";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "Single Screen") == 0)
         g_layout_mode = LAYOUT_SINGLE;
      else if (strcmp(var.value, "Side by Side (512x192)") == 0)
         g_layout_mode = LAYOUT_HORIZONTAL;
      else
         g_layout_mode = LAYOUT_VERTICAL;
   }

   var.key = "drastic_initial_screen";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_initial_bottom = (strcmp(var.value, "Bottom Screen") == 0);
   }

   var.key = "drastic_hires_3d";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_hires_3d = (strcmp(var.value, "Enabled") == 0);
   }

   var.key = "drastic_threaded_3d";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_threaded_3d = (strcmp(var.value, "Enabled") == 0);
   }

   var.key = "drastic_frameskip";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "Auto 1") == 0) {
         g_opt_frameskip = 1; g_opt_frameskip_type = 2;
      } else if (strcmp(var.value, "Auto 2") == 0) {
         g_opt_frameskip = 2; g_opt_frameskip_type = 2;
      } else if (strcmp(var.value, "Manual 1") == 0) {
         g_opt_frameskip = 1; g_opt_frameskip_type = 1;
      } else if (strcmp(var.value, "Manual 2") == 0) {
         g_opt_frameskip = 2; g_opt_frameskip_type = 1;
      } else {
         g_opt_frameskip = 0; g_opt_frameskip_type = 0;
      }
   }

   var.key = "drastic_fastforward_speed";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "2x") == 0) g_opt_fastforward_speed = 2;
      else if (strcmp(var.value, "4x") == 0) g_opt_fastforward_speed = 4;
      else if (strcmp(var.value, "5x") == 0) g_opt_fastforward_speed = 5;
      else if (strcmp(var.value, "Unlimited") == 0) g_opt_fastforward_speed = 0;
      else g_opt_fastforward_speed = 3;
   }

   var.key = "drastic_cpu_threads";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "1") == 0) g_opt_cpu_threads = 1;
      else if (strcmp(var.value, "2") == 0) g_opt_cpu_threads = 2;
      else g_opt_cpu_threads = 3;
   }

   var.key = "drastic_slot2_device";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "GBA Cartridge") == 0) g_opt_slot2 = 1;
      else if (strcmp(var.value, "RAM Expansion (8MB)") == 0) g_opt_slot2 = 3;
      else if (strcmp(var.value, "None") == 0) g_opt_slot2 = 0;
      else g_opt_slot2 = 2; /* Rumble Pak */
   }

   var.key = "drastic_direct_boot";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_direct_boot = (strcmp(var.value, "NDS Firmware GUI") != 0);
   }

   var.key = "drastic_preload_rom";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_preload_rom = (strcmp(var.value, "Disabled") != 0);
   }

   var.key = "drastic_rtc";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_rtc = (strcmp(var.value, "Fixed Time") != 0);
   }

   var.key = "drastic_edge_marking";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_edge_marking = (strcmp(var.value, "Disabled") != 0);
   }

   var.key = "drastic_cheats";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_cheats = (strcmp(var.value, "Disabled") != 0);
   }

   var.key = "drastic_mic_mode";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_mic = (strcmp(var.value, "Sample White Noise") == 0);
   }

   var.key = "drastic_audio_filter";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      g_opt_audio_filter = (strcmp(var.value, "Raw (Fast)") != 0);
   }

   var.key = "drastic_touch_mode";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "Analog Cursor") == 0)
         g_touch_mode = TOUCH_MODE_ANALOG;
      else
         g_touch_mode = TOUCH_MODE_TOUCHSCREEN;
   }

   var.key = "drastic_cursor_speed";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "Fast") == 0) g_opt_cursor_speed = 2000;
      else if (strcmp(var.value, "Slow") == 0) g_opt_cursor_speed = 8000;
      else g_opt_cursor_speed = 4000;
   }

   var.key = "drastic_autofire";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "Fast (10 Hz)") == 0) g_opt_autofire = 2;
      else if (strcmp(var.value, "Slow (5 Hz)") == 0) g_opt_autofire = 1;
      else g_opt_autofire = 0;
   }

   /* Layout changes reshape the output; tell the frontend so scalers and
    * the aspect ratio follow without a reload. */
   if (g_layout_mode != prev_layout && environ_cb) {
      struct retro_system_av_info av;
      retro_get_system_av_info(&av);
      environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &av.geometry);
   }

   /* Apply live core config updates if any DraStic bitfield changed */
   uint64_t cur_config = build_drastic_config();
   if (cur_config != prev_config && p_applyConfig && g_game_loaded) {
      JNIEnv *env = get_mock_jni_env();
      p_applyConfig(env, NULL, (jlong)cur_config);
      LOGI("[DraStic] Applied live core config: 0x%llx\n", (unsigned long long)cur_config);
   }
}

void retro_init(void) {
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
      LOGW("[DraStic] RGB565 pixel format rejected by frontend");
   }

   const char *dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir && dir[0] == '/') {
      snprintf(g_system_dir, sizeof(g_system_dir), "%s", dir);
   } else {
      snprintf(g_system_dir, sizeof(g_system_dir), "/mnt/sdcard/Bios/NDS");
   }
   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir && dir[0] == '/') {
      snprintf(g_save_dir, sizeof(g_save_dir), "%s", dir);
   } else {
      snprintf(g_save_dir, sizeof(g_save_dir), "/mnt/sdcard/Saves/NDS");
   }

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Screen Swap" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "Touch Tap" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,"Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 0, 0, 0, 0, NULL }
   };
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   /* Load libdrastic_arm64.so dynamically if available */
   g_drastic_handle = dlopen(DRASTIC_LIB_NAME, RTLD_LAZY | RTLD_GLOBAL);
   if (!g_drastic_handle) {
      LOGW("[DraStic] unable to load %s: %s (proprietary core not installed)", DRASTIC_LIB_NAME, dlerror());
      return;
   }
   p_JNI_OnLoad = (fn_JNI_OnLoad)dlsym(g_drastic_handle, "JNI_OnLoad");
   p_onInit = (fn_onInit)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_onInit");
   p_startGame = (fn_startGame)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_startGame");
   p_applyConfig = (fn_applyConfig)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_applyConfig");
   p_updateFrame = (fn_updateFrame)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_updateFrame");
   p_getScreenBuffers = (fn_getScreenBuffers)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_getScreenBuffers");
   p_getSnapshots16 = (fn_getSnapshots16)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_getSnapshots16");
   p_renderFrame = (fn_renderFrame)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_renderFrame");
   p_updateInput = (fn_updateInput)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_updateInput");
   p_saveState = (fn_saveState)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_saveState");
   p_loadState = (fn_loadState)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_loadState");
   p_setAutosaveInterval = (fn_setAutosaveInterval)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_setAutosaveInterval");
   p_setAudioVolume = (fn_setAudioVolume)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_setAudioVolume");
   p_resetDS = (fn_resetDS)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_resetDS");
   p_quitSystem = (fn_quitSystem)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_quitSystem");
   p_signalScreen = (fn_signalScreen)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_signalScreen");
   p_waitScreen = (fn_waitScreen)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_waitScreen");
   p_getRumbleState = (fn_getRumbleState)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_getRumbleState");

   if (p_JNI_OnLoad) {
      p_JNI_OnLoad(get_mock_java_vm(), NULL);
   }

   JNIEnv *env = get_mock_jni_env();
   if (p_onInit && env) {
      jstring path = (*env)->NewStringUTF(env, g_system_dir);
      jstring savePath = (*env)->NewStringUTF(env, g_save_dir);
      LOGI("[DraStic] calling onInit: sys=%s save=%s\n", g_system_dir, g_save_dir);
      p_onInit(env, NULL, path, savePath, 28);
      LOGI("[DraStic] onInit finished\n");
   }
   g_jni_screen_top = (*env)->NewIntArray(env, NDS_SCREEN_W * NDS_SCREEN_H);
   g_jni_screen_bottom = (*env)->NewIntArray(env, NDS_SCREEN_W * NDS_SCREEN_H);

   if (p_setAutosaveInterval) {
      p_setAutosaveInterval(env, NULL, 30);
   }
   if (p_setAudioVolume) {
      p_setAudioVolume(env, NULL, 100);
      LOGI("[DraStic] setAudioVolume(100) called\n");
   }

   g_initialized = true;}

void retro_deinit(void) {
    if (g_initialized && p_quitSystem) {
       JNIEnv *env = get_mock_jni_env();
       p_quitSystem(env, NULL);
    }
    /* startGame blocks until game exit; quitSystem is what makes it return.
     * Join before dlclose or the still-running thread executes unmapped code. */
    if (g_start_thread) {
       pthread_join(g_start_thread, NULL);
       g_start_thread = 0;
    }
    if (g_drastic_handle) {
       dlclose(g_drastic_handle);
       g_drastic_handle = NULL;
    }
    g_initialized = false;
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info) {
   memset(info, 0, sizeof(*info));
   info->library_name     = "DraStic";
   info->library_version  = "2.6.0.4a";
   info->valid_extensions = "nds|zip";
   info->need_fullpath    = true;
   info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *av) {
   memset(av, 0, sizeof(*av));
   switch (g_layout_mode) {
   case LAYOUT_SINGLE:
      av->geometry.base_width   = 256;
      av->geometry.base_height  = 192;
      av->geometry.aspect_ratio = 256.0f / 192.0f;
      break;
   case LAYOUT_HORIZONTAL:
      av->geometry.base_width   = 512;
      av->geometry.base_height  = 192;
      av->geometry.aspect_ratio = 512.0f / 192.0f;
      break;
   case LAYOUT_VERTICAL:
   default:
      av->geometry.base_width   = 256;
      av->geometry.base_height  = 384;
      av->geometry.aspect_ratio = 256.0f / 384.0f;
      break;
   }
   av->geometry.max_width    = 512;
   av->geometry.max_height   = 384;
   av->timing.fps            = 60.0;
   av->timing.sample_rate    = 44100.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
   (void)port;
   (void)device;
}

void retro_reset(void) {
   if (p_resetDS) {
      JNIEnv *env = get_mock_jni_env();
      p_resetDS(env, NULL);
   }
}

static void *startgame_thread(void *arg) {
   (void)arg;
   JNIEnv *env = get_mock_jni_env();
   jstring rom = (*env)->NewStringUTF(env, g_rom_path);
   uint64_t config = build_drastic_config();
   int result = p_startGame(env, NULL, rom, -1, (jlong)config, 0, JNI_FALSE, 0);
   LOGI("[DraStic] startGame returned %d for %s (config=0x%llx)\n", result, g_rom_path, (unsigned long long)config);
   if (p_applyConfig) {
      p_applyConfig(env, NULL, (jlong)config);
   }
   if (p_setAudioVolume) {
      p_setAudioVolume(env, NULL, 100);
   }
   if (!result) {
      LOGW("[DraStic] startGame failed for %s", g_rom_path);
      g_game_loaded = false;
   }
   return NULL;
}

static void save_ram_setup(const struct retro_game_info *game) {
   /* Battery save lives at <system_dir>/User/backup/dseins.dsv. Pre-size it
    * and mmap so the core's reads/writes and minarch's retro_get_memory_data
    * share the same pages. Seed from minarch's .sav if one exists. */
   char dir[1024];
   snprintf(dir, sizeof(dir), "%s/User/backup", g_system_dir);
   for (char *p = dir + 1; *p; p++) {
      if (*p == '/') { *p = '\0'; mkdir(dir, 0777); *p = '/'; }
   }
   mkdir(dir, 0777);

   snprintf(g_battery_path, sizeof(g_battery_path), "%s/User/backup/dseins.dsv", g_system_dir);
   snprintf(g_savestates_dir, sizeof(g_savestates_dir), "/tmp/drastic_savestates");
   mkdir(g_savestates_dir, 0777);

   int fd = open(g_battery_path, O_RDWR | O_CREAT, 0666);
   if (fd < 0) {
      LOGW("[DraStic] SAVE_RAM: cannot open %s", g_battery_path);
      return;
   }
   struct stat st;
   size_t want = SAVE_RAM_SIZE;
   if (fstat(fd, &st) == 0 && st.st_size > 0) {
      want = (size_t)st.st_size;
   }
   if (want < 8) want = SAVE_RAM_SIZE;
   if (ftruncate(fd, (off_t)want) != 0) {
      LOGW("[DraStic] SAVE_RAM: ftruncate failed");
      close(fd);
      return;
   }
   void *map = mmap(NULL, want, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (map == MAP_FAILED) {
      LOGW("[DraStic] SAVE_RAM: mmap failed");
      close(fd);
      return;
   }
   g_save_ram = (uint8_t*)map;
   g_save_ram_size = want;

   /* Seed from minarch's <saves_dir>/<game>.sav if present. */
   const char *base = game && game->path ? strrchr(game->path, '/') : NULL;
   base = base ? base + 1 : (game && game->path ? game->path : "game");
   snprintf(g_rom_basename, sizeof(g_rom_basename), "%s", base);
   char sav[1024];
   snprintf(sav, sizeof(sav), "%s/%s.sav", g_save_dir, base);
   FILE *f = fopen(sav, "rb");
   if (f) {
      size_t got = fread(g_save_ram, 1, g_save_ram_size, f);
      fclose(f);
      LOGI("[DraStic] SAVE_RAM: seeded %zu bytes from %s", got, sav);
   } else {
      memset(g_save_ram, 0xff, g_save_ram_size);
   }
   close(fd);
   LOGI("[DraStic] SAVE_RAM: %zu bytes at %s", g_save_ram_size, g_battery_path);
}

static char g_start_stack[1024 * 1024] __attribute__((aligned(4096)));

bool retro_load_game(const struct retro_game_info *game) {
   if (!game || !game->path) return false;
   update_variables();
   g_active_screen = g_opt_initial_bottom ? true : false;

   if (g_save_ram) {
      munmap(g_save_ram, g_save_ram_size);
      g_save_ram = NULL;
      g_save_ram_size = 0;
   }
   save_ram_setup(game);

   if (p_startGame) {
      snprintf(g_rom_path, sizeof(g_rom_path), "%s", game->path);
      g_game_loaded = true;
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setstack(&attr, g_start_stack, sizeof(g_start_stack));
      pthread_create(&g_start_thread, &attr, startgame_thread, NULL);
      pthread_attr_destroy(&attr);

      /* Wait for startGame to finish all memory mappings and BIOS loading */
      int wait_ms = 0;
      while (!g_drastic_audio_started && g_game_loaded && wait_ms < 3000) {
         usleep(10000);
         wait_ms += 10;
      }
      LOGI("[DraStic] startGame ready after %d ms\n", wait_ms);
      return g_game_loaded;
   }
   return false;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
   (void)type;
   (void)info;
   (void)num;
   return false;
}

void retro_unload_game(void) {
   g_game_loaded = false;
   g_running = false;
   /* quitSystem is what unblocks the boot/emulation thread; join it so a
    * following load_game starts from a quiet state and dlclose stays safe. */
   if (g_initialized && p_quitSystem) {
      JNIEnv *env = get_mock_jni_env();
      p_quitSystem(env, NULL);
   }
   if (g_start_thread) {
      pthread_join(g_start_thread, NULL);
      g_start_thread = 0;
   }
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void *retro_get_memory_data(unsigned id) {
   if (id == RETRO_MEMORY_SAVE_RAM) return g_save_ram;
   return NULL;
}
size_t retro_get_memory_size(unsigned id) {
   if (id == RETRO_MEMORY_SAVE_RAM) return g_save_ram_size;
   return 0;
}

#define SAVE_SLOT 0
#define RETRO_SERIALIZE_BUFFER_SIZE (4 * 1024 * 1024)

size_t retro_serialize_size(void) {
   return RETRO_SERIALIZE_BUFFER_SIZE;
}

static bool state_candidate_path(char *out, size_t outsz) {
   /* 1. Try _savestate_temp.dss */
   snprintf(out, outsz, "%s/_savestate_temp.dss", g_savestates_dir);
   if (access(out, F_OK) == 0) return true;

   /* 2. Try <rom_basename>_0.dss */
   if (g_rom_basename[0]) {
      snprintf(out, outsz, "%s/%s_0.dss", g_savestates_dir, g_rom_basename);
      if (access(out, F_OK) == 0) return true;

      char noext[512];
      snprintf(noext, sizeof(noext), "%s", g_rom_basename);
      char *dot = strrchr(noext, '.');
      if (dot) *dot = '\0';
      snprintf(out, outsz, "%s/%s_0.dss", g_savestates_dir, noext);
      if (access(out, F_OK) == 0) return true;
   }
   return false;
}

bool retro_serialize(void *data, size_t size) {
   if (!g_game_loaded || !g_drastic_audio_started || !p_saveState || !data) return false;
   if (size < sizeof(uint32_t)) return false;

   /* If fast forward is active, skip serialization to avoid interrupting speed */
   if (g_fast_forward_active) {
      return false;
   }

   /* Remove any existing temp state before triggering synchronous save */
   char path[1100];
   snprintf(path, sizeof(path), "%s/_savestate_temp.dss", g_savestates_dir);
   unlink(path);

   JNIEnv *env = get_mock_jni_env();
   p_saveState(env, NULL, SAVE_SLOT, JNI_TRUE);

   if (!state_candidate_path(path, sizeof(path))) {
      LOGW("[DraStic-State] retro_serialize: save state file not found\n");
      return false;
   }

   FILE *f = fopen(path, "rb");
   if (!f) return false;
   struct stat st;
   if (fstat(fileno(f), &st) != 0 || st.st_size <= 0 || (size_t)st.st_size > (size - sizeof(uint32_t))) {
      fclose(f);
      return false;
   }

   uint32_t payload_sz = (uint32_t)st.st_size;
   memcpy(data, &payload_sz, sizeof(uint32_t));

   size_t got = fread((char*)data + sizeof(uint32_t), 1, payload_sz, f);
   fclose(f);

   LOGI("[DraStic-State] retro_serialize: saved %zu bytes from %s\n", got, path);
   return got == payload_sz;
}

bool retro_unserialize(const void *data, size_t size) {
   if (!g_game_loaded || !g_drastic_audio_started || !p_loadState || !data) return false;
   if (size < sizeof(uint32_t)) return false;

   uint32_t payload_sz = 0;
   memcpy(&payload_sz, data, sizeof(uint32_t));

   const void *payload_data = (const char*)data + sizeof(uint32_t);
   size_t write_sz = payload_sz;

   /* If data does not have 4-byte header (e.g. raw DSS dump), use full buffer */
   if (payload_sz == 0 || payload_sz > size - sizeof(uint32_t)) {
      payload_data = data;
      write_sz = size;
   }

   /* Write to both candidates so loadState finds it whether looking for _temp or slot 0 */
   char path1[1100], path2[1100];
   snprintf(path1, sizeof(path1), "%s/_savestate_temp.dss", g_savestates_dir);
   FILE *f1 = fopen(path1, "wb");
   if (f1) {
      fwrite(payload_data, 1, write_sz, f1);
      fclose(f1);
   }

   if (g_rom_basename[0]) {
      snprintf(path2, sizeof(path2), "%s/%s_0.dss", g_savestates_dir, g_rom_basename);
      FILE *f2 = fopen(path2, "wb");
      if (f2) {
         fwrite(payload_data, 1, write_sz, f2);
         fclose(f2);
      }
      char noext[512];
      snprintf(noext, sizeof(noext), "%s", g_rom_basename);
      char *dot = strrchr(noext, '.');
      if (dot) *dot = '\0';
      snprintf(path2, sizeof(path2), "%s/%s_0.dss", g_savestates_dir, noext);
      FILE *f3 = fopen(path2, "wb");
      if (f3) {
         fwrite(payload_data, 1, write_sz, f3);
         fclose(f3);
      }
   }

   JNIEnv *env = get_mock_jni_env();
   bool ok = p_loadState(env, NULL, SAVE_SLOT);
   LOGI("[DraStic-State] retro_unserialize: wrote %zu bytes, loadState -> %s\n", write_sz, ok ? "ok" : "failed");
   return ok;
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {
   (void)index;
   (void)enabled;
   (void)code;
}

void retro_run(void) {
   if (!g_game_loaded) return;
   g_running = true;

   /* Apply option changes live (screen layout, touch mode). */
   bool vars_changed = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &vars_changed) && vars_changed) {
      update_variables();
   }

   /* Allow 600ms for background startgame_thread to finish ROM & DLDI setup */
   static struct timespec s_start_time = {0};
   if (s_start_time.tv_sec == 0) {
      clock_gettime(CLOCK_MONOTONIC, &s_start_time);
   }
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   long elapsed_ms = (now.tv_sec - s_start_time.tv_sec) * 1000 +
                     (now.tv_nsec - s_start_time.tv_nsec) / 1000000;
   if (elapsed_ms < 600) {
      video_cb(NULL, 0, 0, 0);
      return;
   }

   static unsigned frame_count = 0;
   frame_count++;
   if ((frame_count % 60) == 0) {
      LOGI("[DraStic] frame=%u (elapsed=%ld ms)\n", frame_count, elapsed_ms);
   }

   input_poll_cb();

   uint32_t keys = 0;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))  keys |= NDS_KEY_RIGHT;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))   keys |= NDS_KEY_LEFT;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))     keys |= NDS_KEY_UP;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))   keys |= NDS_KEY_DOWN;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))      keys |= NDS_KEY_A;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))      keys |= NDS_KEY_B;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))      keys |= NDS_KEY_X;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))      keys |= NDS_KEY_Y;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L))      keys |= NDS_KEY_L;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))      keys |= NDS_KEY_R;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))  keys |= NDS_KEY_START;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT)) keys |= NDS_KEY_SELECT;

   static uint32_t prev_keys = 0;
   if (keys != prev_keys) {
      LOGI("[DraStic-Key] keys: 0x%04x (prev 0x%04x)\n", keys, prev_keys);
      prev_keys = keys;
   }

   /* Screen Swap Toggle (L2) */
   static bool l2_pressed = false;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2)) {
      if (!l2_pressed) {
         g_active_screen = !g_active_screen;
         LOGI("[DraStic-Screen] swapped screen to: %s\n", g_active_screen ? "BOTTOM" : "TOP");
         l2_pressed = true;
      }
   } else {
      l2_pressed = false;
   }

   /* Handle Touch Cursor & Pointer */
   int touch_x = 0, touch_y = 0;
   bool touched = false;

   /* Physical Touchscreen (Goodix capacitive touch on RG Arc D) */
   ts_poll_events();
   if (g_ts_down) {
      /* RG ARC-D panel: 480x640 portrait rotated 90 deg clockwise.
       * raw_x in 0..479, raw_y in 0..639.
       * Landscape screen X = raw_y (0..639).
       * Landscape screen Y = 479 - raw_x (0..479). */
      int disp_x = g_ts_raw_y;
      int disp_y = 479 - g_ts_raw_x;
      if (disp_x < 0) disp_x = 0;
      if (disp_x > 639) disp_x = 639;
      if (disp_y < 0) disp_y = 0;
      if (disp_y > 479) disp_y = 479;

      if (g_layout_mode == LAYOUT_SINGLE) {
         if (g_active_screen) {
            touch_x = (disp_x * NDS_SCREEN_W) / 640;
            touch_y = (disp_y * NDS_SCREEN_H) / 480;
            touched = true;
         }
      } else if (g_layout_mode == LAYOUT_VERTICAL) {
         if (disp_y >= 240) {
            touch_x = (disp_x * NDS_SCREEN_W) / 640;
            touch_y = ((disp_y - 240) * NDS_SCREEN_H) / 240;
            touched = true;
         }
      }
   } else if (input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED)) {
      int16_t px = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
      int16_t py = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);

      if (g_layout_mode == LAYOUT_SINGLE && g_active_screen) {
         int tx = (int)(((int32_t)(px + 0x7fff) * NDS_SCREEN_W) / 0xffff);
         int ty = (int)(((int32_t)(py + 0x7fff) * NDS_SCREEN_H) / 0xffff);
         if (tx < 0) tx = 0;
         if (tx >= NDS_SCREEN_W) tx = NDS_SCREEN_W - 1;
         if (ty < 0) ty = 0;
         if (ty >= NDS_SCREEN_H) ty = NDS_SCREEN_H - 1;
         touch_x = tx;
         touch_y = ty;
         touched = true;
      } else if (g_layout_mode == LAYOUT_VERTICAL) {
         int tx = (int)(((int32_t)(px + 0x7fff) * NDS_SCREEN_W) / 0xffff);
         int ty = (int)(((int32_t)(py + 0x7fff) * (NDS_SCREEN_H * 2)) / 0xffff);
         if (tx < 0) tx = 0;
         if (tx >= NDS_SCREEN_W) tx = NDS_SCREEN_W - 1;
         if (ty >= NDS_SCREEN_H) {
            touch_x = tx;
            touch_y = ty - NDS_SCREEN_H;
            if (touch_y >= NDS_SCREEN_H) touch_y = NDS_SCREEN_H - 1;
            touched = true;
         }
      }
   }

   /* Fallback: Analog Cursor & R2 Tap */
   if (!touched) {
      if (g_touch_mode == TOUCH_MODE_ANALOG) {
         int16_t rx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
         int16_t ry = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
         if (abs(rx) > 4000) g_cursor_x += rx / g_opt_cursor_speed;
         if (abs(ry) > 4000) g_cursor_y += ry / g_opt_cursor_speed;
      }

      if (g_cursor_x < 0) g_cursor_x = 0;
      if (g_cursor_x > 255) g_cursor_x = 255;
      if (g_cursor_y < 0) g_cursor_y = 0;
      if (g_cursor_y > 191) g_cursor_y = 191;

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2)) {
         touched = true;
         touch_x = g_cursor_x;
         touch_y = g_cursor_y;
      }
   }

   /* Fast Forward detection: directly read minarch fast_forward state */
   int is_ff = 0;
   if (g_p_minarch_ff) {
      is_ff = *g_p_minarch_ff ? 1 : 0;
   } else {
      bool r1_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R);
      bool sel_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
      if (r1_pressed && sel_pressed) is_ff = 1;
   }

   JNIEnv *env = get_mock_jni_env();
   extern void bionic_set_fast_forward(int active);
   if (is_ff != g_fast_forward_active) {
      g_fast_forward_active = is_ff;
      bionic_set_fast_forward(is_ff);
      if (p_applyConfig && g_game_loaded) {
         p_applyConfig(env, NULL, (jlong)build_drastic_config());
         LOGI("[DraStic] Fast Forward live state -> %s\n", is_ff ? "ON" : "OFF");
      }
   }

   /* Hardware Rumble Feedback */
   if (p_getRumbleState && g_rumble.set_rumble_state) {
      bool rumble_on = p_getRumbleState(env, NULL) != 0;
      static bool s_last_rumble = false;
      if (rumble_on != s_last_rumble) {
         s_last_rumble = rumble_on;
         g_rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, rumble_on ? 0xffff : 0);
         g_rumble.set_rumble_state(0, RETRO_RUMBLE_WEAK, rumble_on ? 0x8000 : 0);
      }
   }

   int touchXY = ((touch_x & 0xffff) << 16) | (touch_y & 0xffff);

   /* Inject input + read frame readiness in one call. */
   jint frame_info = 0;
   if (p_updateFrame) {
      frame_info = p_updateFrame(env, NULL, (jint)keys, touchXY, touched ? JNI_TRUE : JNI_FALSE);
   } else if (p_updateInput) {
      p_updateInput(env, NULL, (jint)keys, touchXY, touched ? JNI_TRUE : JNI_FALSE);
   }
   (void)frame_info;

   bool video_ready = true;

   /* Wait for emulation to produce the next frame */
   if (p_waitScreen) {
      p_waitScreen(env, NULL);
   }

   /* Fetch screen buffers */
   if (p_getScreenBuffers && g_jni_screen_top && g_jni_screen_bottom) {
      p_getScreenBuffers(env, NULL, g_jni_screen_top, g_jni_screen_bottom);

      jint *top = (jint*)(*env)->GetPrimitiveArrayCritical(env, g_jni_screen_top, NULL);
      jint *bot = (jint*)(*env)->GetPrimitiveArrayCritical(env, g_jni_screen_bottom, NULL);
      if (top && bot) {
         int nb_top = 0, nb_bot = 0;
         uint32_t s_top = 0, s_bot = 0;
         uint16_t *dst = g_raw_screen_buffer;
         for (int i = 0; i < NDS_SCREEN_W * NDS_SCREEN_H; i++) {
            uint32_t px = (uint32_t)top[i];
            if ((px & 0x00ffffff) != 0) {
               nb_top++;
               if (!s_top) s_top = px;
            }
            dst[i] = (uint16_t)(((px >> 8) & 0xf800) | ((px >> 5) & 0x07e0) | ((px >> 3) & 0x001f));
         }
         dst = g_raw_screen_buffer + NDS_SCREEN_W * NDS_SCREEN_H;
         for (int i = 0; i < NDS_SCREEN_W * NDS_SCREEN_H; i++) {
            uint32_t px = (uint32_t)bot[i];
            if ((px & 0x00ffffff) != 0) {
               nb_bot++;
               if (!s_bot) s_bot = px;
            }
            dst[i] = (uint16_t)(((px >> 8) & 0xf800) | ((px >> 5) & 0x07e0) | ((px >> 3) & 0x001f));
         }
         if ((frame_count % 60) == 0) {
            LOGI("[DraStic] f=%u nb_top=%d nb_bot=%d s_top=0x%08x s_bot=0x%08x act=%d\n",
                 frame_count, nb_top, nb_bot, s_top, s_bot, (int)g_active_screen);
         }
      }
      (*env)->ReleasePrimitiveArrayCritical(env, g_jni_screen_bottom, bot, JNI_ABORT);
      (*env)->ReleasePrimitiveArrayCritical(env, g_jni_screen_top, top, JNI_ABORT);
   }

   /* Signal emulation thread that frame was consumed */
   if (p_signalScreen) {
      p_signalScreen(env, NULL);
   }

   /* Render Output Framebuffer according to g_layout_mode */
   if (video_ready) {
      unsigned out_width = 256, out_height = 384, pitch = 256 * 2;
      if (g_layout_mode == LAYOUT_VERTICAL) {
         out_width = 256;
         out_height = 384;
         pitch = 256 * 2;
         memcpy(g_output_buffer, g_raw_screen_buffer, sizeof(g_raw_screen_buffer));
      } else if (g_layout_mode == LAYOUT_SINGLE) {
         out_width = 256;
         out_height = 192;
         pitch = 256 * 2;
         uint16_t *src = g_raw_screen_buffer + (g_active_screen ? (256 * 192) : 0);
         memcpy(g_output_buffer, src, 256 * 192 * 2);
      } else if (g_layout_mode == LAYOUT_HORIZONTAL) {
         out_width = 512;
         out_height = 192;
         pitch = 512 * 2;
         for (int y = 0; y < 192; y++) {
            memcpy(g_output_buffer + (y * 512), g_raw_screen_buffer + (y * 256), 256 * 2);
            memcpy(g_output_buffer + (y * 512 + 256), g_raw_screen_buffer + (192 * 256 + y * 256), 256 * 2);
         }
      }
      video_cb(g_output_buffer, out_width, out_height, pitch);
   } else {
      /* Frontend supports dupe (GET_CAN_DUPE); push nothing while booting. */
      video_cb(NULL, 0, 0, 0);
   }

   /* Synchronously drain audio ring buffer and feed frontend on main thread */
   if (audio_batch_cb && g_running) {
      audio_frame_t drain_buf[735];
      size_t drained = 0;
      pthread_mutex_lock(&g_audio_ring_mutex);
      if (g_fast_forward_active) {
         /* Flush audio ring during fast forward so it never backs up or blocks */
         g_audio_ring_read = g_audio_ring_write;
      } else {
         while (g_audio_ring_read != g_audio_ring_write && drained < 735) {
            drain_buf[drained++] = g_audio_ring[g_audio_ring_read];
            g_audio_ring_read = (g_audio_ring_read + 1) % AUDIO_RING_FRAMES;
         }
      }
      pthread_mutex_unlock(&g_audio_ring_mutex);

      if (drained > 0) {
         static int s_audio_report = 0;
         if (++s_audio_report % 60 == 0) {
            int nz = 0;
            int16_t pk = 0;
            const int16_t *s16 = (const int16_t*)drain_buf;
            for (size_t s = 0; s < drained * 2; s++) {
               if (s16[s] != 0) nz++;
               int16_t v = abs(s16[s]);
               if (v > pk) pk = v;
            }
            LOGI("[DraStic-Audio] drained=%zu non_zero=%d peak=%d\n", drained, nz, (int)pk);
         }
         audio_batch_cb((const int16_t*)drain_buf, drained);
      }
   }
}
