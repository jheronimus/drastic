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

#define DRASTIC_LIB_NAME "libdrastic_arm64.so"

/* DS Keymap constants */
#define NDS_KEY_A      (1 << 0)
#define NDS_KEY_B      (1 << 1)
#define NDS_KEY_SELECT (1 << 2)
#define NDS_KEY_START  (1 << 3)
#define NDS_KEY_RIGHT  (1 << 4)
#define NDS_KEY_LEFT   (1 << 5)
#define NDS_KEY_UP     (1 << 6)
#define NDS_KEY_DOWN   (1 << 7)
#define NDS_KEY_R      (1 << 8)
#define NDS_KEY_L      (1 << 9)
#define NDS_KEY_X      (1 << 10)
#define NDS_KEY_Y      (1 << 11)

/* Screen Layout Modes */
enum layout_mode {
   LAYOUT_VERTICAL = 0,
   LAYOUT_SINGLE,
   LAYOUT_HORIZONTAL
};

enum touch_mode {
   TOUCH_MODE_ANALOG = 0,
   TOUCH_MODE_TOUCHSCREEN
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
static fn_updateFrame p_updateFrame = NULL;
static fn_getScreenBuffers p_getScreenBuffers = NULL;
static fn_renderFrame p_renderFrame = NULL;
static fn_updateInput p_updateInput = NULL;
static fn_saveState p_saveState = NULL;
static fn_loadState p_loadState = NULL;
static fn_setAutosaveInterval p_setAutosaveInterval = NULL;
static fn_resetDS p_resetDS = NULL;
static fn_quitSystem p_quitSystem = NULL;

static bool g_initialized = false;
static bool g_game_loaded = false;
static pthread_t g_start_thread;
static char g_rom_path[PATH_MAX];

#define NDS_SCREEN_W 256
#define NDS_SCREEN_H 192
static uint16_t g_raw_screen_buffer[NDS_SCREEN_W * NDS_SCREEN_H * 2];
static uint16_t g_output_buffer[512 * 384];
static jintArray g_jni_screen_top = NULL;
static jintArray g_jni_screen_bottom = NULL;

/* SAVE RAM: a memfd-backed file that doubles as the core's battery save
 * (User/backup/dseins.dsv). The core reads/writes it via its own fd; minarch
 * reads/writes the same pages via retro_get_memory_data. */
#define SAVE_RAM_SIZE (1 * 1024 * 1024)
static uint8_t *g_save_ram = NULL;
static size_t g_save_ram_size = 0;
static char g_battery_path[1024];
static char g_savestates_dir[1024];

static enum layout_mode g_layout_mode = LAYOUT_VERTICAL;
static enum touch_mode g_touch_mode = TOUCH_MODE_TOUCHSCREEN;
static bool g_active_screen = false; /* false = top, true = bottom */

static int g_cursor_x = 128;
static int g_cursor_y = 96;

/* System Paths */
char g_system_dir[512] = "./bios";
char g_save_dir[512] = "./saves";

void retro_set_environment(retro_environment_t cb) {
   environ_cb = cb;

   static const struct retro_variable vars[] = {
      { "drastic_screen_layout", "Screen Layout; Vertical (256x384)|Single Screen|Side by Side (512x192)" },
      { "drastic_touch_mode", "Touch Mode; Physical Touchscreen|Analog Cursor" },
      { NULL, NULL }
   };
   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
static void audio_batch_wrap(const void *data, size_t frames) {
   if (audio_batch_cb) {
      audio_batch_cb((const int16_t*)data, frames);
   }
}

void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
   audio_batch_cb = cb;
   g_drastic_audio_batch = (void (*)(const void *, size_t))audio_batch_wrap;
}
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

static void update_variables(void) {
   struct retro_variable var = {0};

   var.key = "drastic_screen_layout";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "Single Screen") == 0)
         g_layout_mode = LAYOUT_SINGLE;
      else if (strcmp(var.value, "Side by Side (512x192)") == 0)
         g_layout_mode = LAYOUT_HORIZONTAL;
      else
         g_layout_mode = LAYOUT_VERTICAL;
   }

   var.key = "drastic_touch_mode";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      if (strcmp(var.value, "Analog Cursor") == 0)
         g_touch_mode = TOUCH_MODE_ANALOG;
      else
         g_touch_mode = TOUCH_MODE_TOUCHSCREEN;
   }
}

void retro_init(void) {
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
      fprintf(stderr, "[DraStic-Libretro] RGB565 pixel format rejected by frontend\n");
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
      fprintf(stderr, "[DraStic-Libretro] Unable to load %s: %s\n", DRASTIC_LIB_NAME, dlerror());
      return;
   }

   fprintf(stderr, "[DraStic-Trace] STEP 1: retro_init start\n");
   fflush(stderr);
   p_JNI_OnLoad = (fn_JNI_OnLoad)dlsym(g_drastic_handle, "JNI_OnLoad");
   p_onInit = (fn_onInit)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_onInit");
   p_startGame = (fn_startGame)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_startGame");
   p_updateFrame = (fn_updateFrame)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_updateFrame");
   p_getScreenBuffers = (fn_getScreenBuffers)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_getScreenBuffers");
   p_renderFrame = (fn_renderFrame)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_renderFrame");
   p_updateInput = (fn_updateInput)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_updateInput");
   p_saveState = (fn_saveState)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_saveState");
   p_loadState = (fn_loadState)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_loadState");
   p_setAutosaveInterval = (fn_setAutosaveInterval)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_setAutosaveInterval");
   p_resetDS = (fn_resetDS)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_resetDS");
   p_quitSystem = (fn_quitSystem)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_quitSystem");

   if (p_JNI_OnLoad) {
      fprintf(stderr, "[DraStic-Trace] STEP 2: Calling JNI_OnLoad\n");
      fflush(stderr);
      p_JNI_OnLoad(get_mock_java_vm(), NULL);
   }

   JNIEnv *env = get_mock_jni_env();
   if (p_onInit && env) {
      fprintf(stderr, "[DraStic-Trace] STEP 3: Calling onInit sys_dir=%s save_dir=%s\n", g_system_dir, g_save_dir);
      fflush(stderr);
      jstring path = (*env)->NewStringUTF(env, g_system_dir);
      jstring savePath = (*env)->NewStringUTF(env, g_save_dir);
      p_onInit(env, NULL, path, savePath);
   }

   fprintf(stderr, "[DraStic-Trace] STEP 4: Creating screen arrays\n");
   fflush(stderr);
   g_jni_screen_top = (*env)->NewIntArray(env, NDS_SCREEN_W * NDS_SCREEN_H);
   g_jni_screen_bottom = (*env)->NewIntArray(env, NDS_SCREEN_W * NDS_SCREEN_H);

   if (p_setAutosaveInterval) {
      p_setAutosaveInterval(env, NULL, 30);
   }

   g_initialized = true;
   fprintf(stderr, "[DraStic-Trace] STEP 5: retro_init complete\n");
   fflush(stderr);
}

void retro_deinit(void) {
   if (g_initialized && p_quitSystem) {
      JNIEnv *env = get_mock_jni_env();
      p_quitSystem(env, NULL);
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
   av->geometry.base_width   = 256;
   av->geometry.base_height  = 384;
   av->geometry.max_width    = 512;
   av->geometry.max_height   = 384;
   av->geometry.aspect_ratio = 256.0f / 384.0f;
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
   fprintf(stderr, "[DraStic-Trace] STEP 7: Calling startGame rom=%s (async thread)\n", g_rom_path);
   fflush(stderr);
   int result = p_startGame(env, NULL, rom);
   fprintf(stderr, "[DraStic-Trace] STEP 8: startGame returned result=%d\n", result);
   fflush(stderr);
   if (!result) {
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
   snprintf(g_savestates_dir, sizeof(g_savestates_dir), "%s/User/savestates", g_system_dir);
   mkdir(g_savestates_dir, 0777);

   int fd = open(g_battery_path, O_RDWR | O_CREAT, 0666);
   if (fd < 0) {
      fprintf(stderr, "[DraStic-Trace] SAVE_RAM: cannot open %s\n", g_battery_path);
      return;
   }
   struct stat st;
   size_t want = SAVE_RAM_SIZE;
   if (fstat(fd, &st) == 0 && st.st_size > 0) {
      want = (size_t)st.st_size;
   }
   if (want < 8) want = SAVE_RAM_SIZE;
   if (ftruncate(fd, (off_t)want) != 0) {
      fprintf(stderr, "[DraStic-Trace] SAVE_RAM: ftruncate failed\n");
      close(fd);
      return;
   }
   void *map = mmap(NULL, want, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (map == MAP_FAILED) {
      fprintf(stderr, "[DraStic-Trace] SAVE_RAM: mmap failed\n");
      close(fd);
      return;
   }
   g_save_ram = (uint8_t*)map;
   g_save_ram_size = want;

   /* Seed from minarch's <saves_dir>/<game>.sav if present. */
   const char *base = game && game->path ? strrchr(game->path, '/') : NULL;
   base = base ? base + 1 : (game && game->path ? game->path : "game");
   char sav[1024];
   snprintf(sav, sizeof(sav), "%s/%s.sav", g_save_dir, base);
   FILE *f = fopen(sav, "rb");
   if (f) {
      size_t got = fread(g_save_ram, 1, g_save_ram_size, f);
      fclose(f);
      fprintf(stderr, "[DraStic-Trace] SAVE_RAM: seeded %zu bytes from %s\n", got, sav);
   } else {
      memset(g_save_ram, 0xff, g_save_ram_size);
   }
   close(fd);
   fprintf(stderr, "[DraStic-Trace] SAVE_RAM: %zu bytes at %s\n", g_save_ram_size, g_battery_path);
   fflush(stderr);
}

bool retro_load_game(const struct retro_game_info *game) {
   if (!game || !game->path) return false;
   update_variables();

   if (g_save_ram) {
      munmap(g_save_ram, g_save_ram_size);
      g_save_ram = NULL;
      g_save_ram_size = 0;
   }
   save_ram_setup(game);

   fprintf(stderr, "[DraStic-Trace] STEP 6: retro_load_game path=%s\n", game->path);
   fflush(stderr);

   if (p_startGame) {
      snprintf(g_rom_path, sizeof(g_rom_path), "%s", game->path);
      g_game_loaded = true;
      pthread_create(&g_start_thread, NULL, startgame_thread, NULL);
      pthread_detach(g_start_thread);
      return true;
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

size_t retro_serialize_size(void) {
   /* DraStic .dss savestates are zlib-compressed and game-size dependent;
    * report a generous fixed cap. */
   return 8 * 1024 * 1024;
}

static void state_temp_path(char *out, size_t outsz) {
   snprintf(out, outsz, "%s/_savestate_temp.dss", g_savestates_dir);
}

bool retro_serialize(void *data, size_t size) {
   if (!g_game_loaded || !p_saveState || !data) return false;
   JNIEnv *env = get_mock_jni_env();
   /* Blocking save (async=1); the core writes <savestates>/_savestate_temp.dss. */
   p_saveState(env, NULL, SAVE_SLOT, JNI_TRUE);

   char path[1100];
   state_temp_path(path, sizeof(path));
   FILE *f = fopen(path, "rb");
   if (!f) return false;
   size_t got = fread(data, 1, size, f);
   fclose(f);
   return got > 0;
}

bool retro_unserialize(const void *data, size_t size) {
   if (!g_game_loaded || !p_loadState || !data) return false;
   char path[1100];
   state_temp_path(path, sizeof(path));
   FILE *f = fopen(path, "wb");
   if (!f) return false;
   size_t wrote = fwrite(data, 1, size, f);
   fclose(f);
   if (wrote != size) return false;

   JNIEnv *env = get_mock_jni_env();
   return p_loadState(env, NULL, SAVE_SLOT);
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {
   (void)index;
   (void)enabled;
   (void)code;
}

void retro_run(void) {
   if (!g_game_loaded) return;

   /* Do not touch the core until it has finished booting (first audio buffer);
    * getScreenBuffers/updateFrame race the boot thread and crash. */
   if (!g_drastic_audio_started) {
      video_cb(NULL, 0, 0, 0);
      return;
   }

   static unsigned frame_count = 0;
   frame_count++;
   if (frame_count <= 10 || frame_count % 300 == 0) {
      fprintf(stderr, "[DraStic-Trace] retro_run frame #%u\n", frame_count);
      fflush(stderr);
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

   /* Screen Swap Toggle (L2) */
   static bool l2_pressed = false;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2)) {
      if (!l2_pressed) {
         g_active_screen = !g_active_screen;
         l2_pressed = true;
      }
   } else {
      l2_pressed = false;
   }

   /* Handle Touch Cursor & Pointer */
   int touch_x = 0, touch_y = 0;
   bool touched = false;

   if (g_touch_mode == TOUCH_MODE_ANALOG) {
      int16_t rx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
      int16_t ry = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
      if (abs(rx) > 4000) g_cursor_x += rx / 4000;
      if (abs(ry) > 4000) g_cursor_y += ry / 4000;
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

   JNIEnv *env = get_mock_jni_env();
   int touchXY = ((touch_x & 0xffff) << 16) | (touch_y & 0xffff);

   /* Inject input + read frame readiness in one call. */
   jint frame_info = 0;
   if (p_updateFrame) {
      frame_info = p_updateFrame(env, NULL, (jint)keys, touchXY, touched ? JNI_TRUE : JNI_FALSE);
   } else if (p_updateInput) {
      p_updateInput(env, NULL, (jint)keys, touchXY, touched ? JNI_TRUE : JNI_FALSE);
   }

   /* The renderer is live once the frame counter advances past 0. */
   bool video_ready = ((unsigned)frame_info & 0xffff) > 0;

   /* Copy both 256x192 ARGB8888 screens via getScreenBuffers, convert to
    * RGB565 for minarch. */
   if (video_ready && p_getScreenBuffers && g_jni_screen_top && g_jni_screen_bottom) {
      if (p_renderFrame) {
         p_renderFrame(env, NULL, 0, 0, 0);
      }
      p_getScreenBuffers(env, NULL, g_jni_screen_top, g_jni_screen_bottom);

      jint *top = (jint*)(*env)->GetPrimitiveArrayCritical(env, g_jni_screen_top, NULL);
      jint *bot = (jint*)(*env)->GetPrimitiveArrayCritical(env, g_jni_screen_bottom, NULL);
      if (top && bot) {
         uint16_t *dst = g_raw_screen_buffer;
         for (int i = 0; i < NDS_SCREEN_W * NDS_SCREEN_H; i++) {
            uint32_t px = (uint32_t)top[i];
            dst[i] = (uint16_t)(((px >> 8) & 0xf800) | ((px >> 5) & 0x07e0) | ((px >> 3) & 0x001f));
         }
         dst = g_raw_screen_buffer + NDS_SCREEN_W * NDS_SCREEN_H;
         for (int i = 0; i < NDS_SCREEN_W * NDS_SCREEN_H; i++) {
            uint32_t px = (uint32_t)bot[i];
            dst[i] = (uint16_t)(((px >> 8) & 0xf800) | ((px >> 5) & 0x07e0) | ((px >> 3) & 0x001f));
         }
      }
      (*env)->ReleasePrimitiveArrayCritical(env, g_jni_screen_bottom, bot, JNI_ABORT);
      (*env)->ReleasePrimitiveArrayCritical(env, g_jni_screen_top, top, JNI_ABORT);
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
}
