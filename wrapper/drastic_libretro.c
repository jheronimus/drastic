#include "drastic_libretro.h"
#include "bionic_shim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

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
static fn_renderFrame p_renderFrame = NULL;
static fn_updateInput p_updateInput = NULL;
static fn_saveState p_saveState = NULL;
static fn_loadState p_loadState = NULL;
static fn_resetDS p_resetDS = NULL;
static fn_quitSystem p_quitSystem = NULL;

static bool g_initialized = false;
static bool g_game_loaded = false;

static uint16_t g_raw_screen_buffer[256 * 384];
static uint16_t g_output_buffer[512 * 384];
static jbyteArray g_jni_screen_array = NULL;

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
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
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
   p_renderFrame = (fn_renderFrame)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_renderFrame");
   p_updateInput = (fn_updateInput)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_updateInput");
   p_saveState = (fn_saveState)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_saveState");
   p_loadState = (fn_loadState)dlsym(g_drastic_handle, "Java_com_dsemu_drastic_DraSticJNI_loadState");
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

   fprintf(stderr, "[DraStic-Trace] STEP 4: Creating screen array\n");
   fflush(stderr);
   g_jni_screen_array = (*env)->NewByteArray(env, sizeof(g_raw_screen_buffer));
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

bool retro_load_game(const struct retro_game_info *game) {
   if (!game || !game->path) return false;
   update_variables();
   fprintf(stderr, "[DraStic-Trace] STEP 6: retro_load_game path=%s\n", game->path);
   fflush(stderr);

   if (p_startGame) {
      JNIEnv *env = get_mock_jni_env();
      jstring rom = (*env)->NewStringUTF(env, game->path);
      fprintf(stderr, "[DraStic-Trace] STEP 7: Calling startGame rom=%s\n", game->path);
      fflush(stderr);
      g_game_loaded = p_startGame(env, NULL, rom);
      fprintf(stderr, "[DraStic-Trace] STEP 8: startGame result=%d\n", g_game_loaded);
      fflush(stderr);
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
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }

size_t retro_serialize_size(void) {
   return 10 * 1024 * 1024;
}

bool retro_serialize(void *data, size_t size) {
   (void)data;
   (void)size;
   if (!g_game_loaded || !p_saveState) return false;
   JNIEnv *env = get_mock_jni_env();
   char state_path[512];
   snprintf(state_path, sizeof(state_path), "%s/drastic_savestate.tmp", g_save_dir);
   jstring path = (*env)->NewStringUTF(env, state_path);
   return p_saveState(env, NULL, path);
}

bool retro_unserialize(const void *data, size_t size) {
   (void)data;
   (void)size;
   if (!g_game_loaded || !p_loadState) return false;
   JNIEnv *env = get_mock_jni_env();
   char state_path[512];
   snprintf(state_path, sizeof(state_path), "%s/drastic_savestate.tmp", g_save_dir);
   jstring path = (*env)->NewStringUTF(env, state_path);
   return p_loadState(env, NULL, path);
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {
   (void)index;
   (void)enabled;
   (void)code;
}

void retro_run(void) {
   if (!g_game_loaded) return;

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
   if (p_updateInput) {
      p_updateInput(env, NULL, keys, touch_x, touch_y, touched);
   }
   if (p_updateFrame) {
      p_updateFrame(env, NULL);
   }
   if (p_renderFrame && g_jni_screen_array) {
      p_renderFrame(env, NULL, g_jni_screen_array);
      jbyte *raw = (*env)->GetByteArrayElements(env, g_jni_screen_array, NULL);
      if (raw) {
         memcpy(g_raw_screen_buffer, raw, sizeof(g_raw_screen_buffer));
         (*env)->ReleaseByteArrayElements(env, g_jni_screen_array, raw, 0);
      }
   }

   /* Render Output Framebuffer according to g_layout_mode */
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
}
