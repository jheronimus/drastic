/* Frontend smoke harness: loads the built libretro core and verifies the
 * API contract a real frontend relies on, without the proprietary DraStic
 * binary (all proprietary entry points stay NULL; graceful paths only).
 *
 * Build+run via `make check`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <libretro.h>

static int g_checks;
static int g_env_set_variables;
static int g_env_pixel_format;
static int g_env_input_descriptors;
static int g_env_controller_info;
static int g_log_lines;

static const char *g_layout_answer = NULL;

static void log_capture(enum retro_log_level level, const char *fmt, ...) {
    (void)level;
    (void)fmt;
    g_log_lines++;
}

static bool env_cb(unsigned cmd, void *data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        g_env_set_variables++;
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        g_env_pixel_format = *(enum retro_pixel_format *)data;
        return true;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        g_env_input_descriptors++;
        return true;
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        g_env_controller_info++;
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = log_capture;
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = "/tmp";
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = "/tmp";
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *var = (struct retro_variable *)data;
        if (!var->key) return false;
        if (strcmp(var->key, "drastic_screen_layout") == 0 && g_layout_answer) {
            var->value = g_layout_answer;
            return true;
        }
        var->value = NULL;
        return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
        *(bool *)data = false;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool *)data = true;
        return true;
    default:
        return false;
    }
}

static unsigned g_frames;
static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch) {
    (void)data; (void)w; (void)h; (void)pitch;
    g_frames++;
}
static void audio_cb(const int16_t *data, size_t frames) { (void)data; (void)frames; }
static void poll_cb(void) {}
static int16_t state_cb(unsigned port, unsigned dev, unsigned idx, unsigned id) {
    (void)port; (void)dev; (void)idx; (void)id;
    return 0;
}

#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL(%d): %s\n", g_checks, msg); \
        exit(1); \
    } \
} while (0)

typedef void (*fn_void)(void);
typedef unsigned (*fn_uint)(void);
typedef void (*fn_sysinfo)(struct retro_system_info *);
typedef void (*fn_avinfo)(struct retro_system_av_info *);
typedef void (*fn_env)(retro_environment_t);
typedef void (*fn_vrefresh)(retro_video_refresh_t);
typedef void (*fn_asample)(retro_audio_sample_t);
typedef void (*fn_asample_batch)(retro_audio_sample_batch_t);
typedef void (*fn_ipoll)(retro_input_poll_t);
typedef void (*fn_istate)(retro_input_state_t);
typedef bool (*fn_load)(const struct retro_game_info *);
typedef void *(*fn_memdata)(unsigned);
typedef size_t (*fn_ssize)(unsigned);

int main(void) {
    void *h = dlopen("./drastic_libretro.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        fprintf(stderr, "FAIL: dlopen ./drastic_libretro.so: %s\n", dlerror());
        return 1;
    }

    static const char *mandatory[] = {
        "retro_set_environment", "retro_set_video_refresh", "retro_set_audio_sample",
        "retro_set_audio_sample_batch", "retro_set_input_poll", "retro_set_input_state",
        "retro_init", "retro_deinit", "retro_api_version", "retro_get_system_info",
        "retro_get_system_av_info", "retro_set_controller_port_device", "retro_reset",
        "retro_run", "retro_serialize_size", "retro_serialize", "retro_unserialize",
        "retro_cheat_reset", "retro_cheat_set", "retro_load_game",
        "retro_load_game_special", "retro_unload_game", "retro_get_region",
        "retro_get_memory_data", "retro_get_memory_size", NULL,
    };
    static fn_void sym[sizeof(mandatory) / sizeof(*mandatory)];
    for (int i = 0; mandatory[i]; i++) {
        sym[i] = (fn_void)dlsym(h, mandatory[i]);
        CHECK(sym[i], mandatory[i]);
    }

    CHECK(((fn_uint)sym[9 - 9]) && ((fn_uint)dlsym(h, "retro_api_version"))() == RETRO_API_VERSION,
          "api version");

    fn_env set_env = (fn_env)dlsym(h, "retro_set_environment");
    fn_vrefresh set_video = (fn_vrefresh)dlsym(h, "retro_set_video_refresh");
    fn_asample set_audio = (fn_asample)dlsym(h, "retro_set_audio_sample");
    fn_asample_batch set_audio_batch = (fn_asample_batch)dlsym(h, "retro_set_audio_sample_batch");
    fn_ipoll set_poll = (fn_ipoll)dlsym(h, "retro_set_input_poll");
    fn_istate set_state = (fn_istate)dlsym(h, "retro_set_input_state");

    set_env(env_cb);
    set_video(video_cb);
    set_audio(NULL);
    set_audio_batch(audio_cb);
    set_poll(poll_cb);
    set_state(state_cb);

    CHECK(g_env_set_variables == 1, "SET_VARIABLES announced once");
    CHECK(g_env_controller_info == 1, "SET_CONTROLLER_INFO announced");
    CHECK(g_log_lines == 0, "no log lines before init");

    ((fn_void)dlsym(h, "retro_init"))();
    CHECK(g_env_pixel_format == RETRO_PIXEL_FORMAT_RGB565, "RGB565 requested");
    CHECK(g_env_input_descriptors == 1, "input descriptors declared");
    CHECK(g_log_lines > 0, "missing proprietary core reported via log interface");

    fn_sysinfo sysinfo = (fn_sysinfo)dlsym(h, "retro_get_system_info");
    struct retro_system_info si;
    memset(&si, 0, sizeof(si));
    sysinfo(&si);
    CHECK(si.library_name && strcmp(si.library_name, "DraStic") == 0, "library_name");
    CHECK(si.valid_extensions && strcmp(si.valid_extensions, "nds|zip") == 0, "valid_extensions");
    CHECK(si.need_fullpath, "need_fullpath");

    fn_avinfo avinfo = (fn_avinfo)dlsym(h, "retro_get_system_av_info");
    struct retro_system_av_info av;
    avinfo(&av);
    CHECK(av.geometry.base_width == 256 && av.geometry.base_height == 384,
          "default vertical geometry");
    CHECK(av.geometry.max_width == 512 && av.geometry.max_height == 384, "max geometry");
    CHECK(av.timing.fps > 55.0 && av.timing.fps < 65.0, "fps sane");
    CHECK(av.timing.sample_rate == 44100.0, "sample rate");

    fn_ssize mem_size = (fn_ssize)dlsym(h, "retro_get_memory_size");
    fn_memdata mem_data = (fn_memdata)dlsym(h, "retro_get_memory_data");
    CHECK(mem_data(RETRO_MEMORY_SAVE_RAM) == NULL, "SRAM null before load");
    CHECK(mem_size(RETRO_MEMORY_SAVE_RAM) == 0, "SRAM size 0 before load");

    /* Without the proprietary core the load must fail cleanly and leave
     * every entry point callable. */
    fn_load load_game = (fn_load)dlsym(h, "retro_load_game");
    struct retro_game_info game = { .path = "/nonexistent/game.nds" };
    CHECK(!load_game(&game), "load fails cleanly without core");

    /* Layout option must reshape av_info even without the proprietary core
     * (update_variables runs during load_game). */
    g_layout_answer = "Single Screen";
    load_game(&game);
    avinfo(&av);
    CHECK(av.geometry.base_width == 256 && av.geometry.base_height == 192,
          "single-screen geometry applied");
    g_layout_answer = "Side by Side (512x192)";
    load_game(&game);
    avinfo(&av);
    CHECK(av.geometry.base_width == 512 && av.geometry.base_height == 192,
          "horizontal geometry applied");
    g_layout_answer = NULL;

    ((fn_void)dlsym(h, "retro_run"))();               /* safe pre-load */
    ((fn_void)dlsym(h, "retro_reset"))();             /* safe without core */
    CHECK(((fn_uint)dlsym(h, "retro_get_region"))() == RETRO_REGION_NTSC, "region");
    CHECK(g_frames == 0, "no frames claimed while booting nothing");

    ((fn_void)dlsym(h, "retro_unload_game"))();
    ((fn_void)dlsym(h, "retro_deinit"))();

    printf("frontend smoke: %d checks passed\n", g_checks);
    return 0;
}
