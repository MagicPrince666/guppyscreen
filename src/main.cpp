#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include "lv_tc.h"
#include "lv_tc_screen.h"

#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <fstream>
#if defined(__APPLE__)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif


#ifdef SIMULATOR
#define SDL_MAIN_HANDLED /*To fix SDL's "undefined reference to WinMain" issue*/
#include <SDL2/SDL.h>
#include "lv_drivers/sdl/sdl.h"

static int tick_thread(void *data);

#endif // SIMUALTOR

static void hal_init(lv_color_t p, lv_color_t s);

#include "guppyscreen.h"
#include "hlog.h"
#include "config.h"
#if defined(__linux__)
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#include <algorithm>

using namespace hv;

#define DISP_BUF_SIZE (128 * 1024)

#if defined(__unix__) || defined(__APPLE__)
#ifdef __GLIBC__
#include <execinfo.h>
#endif
#include <csignal>
#include <cstdlib>
#include <cstdio>

void Execute(std::string cmdline, std::string &recv)
{
#if defined(__unix__) || defined(__APPLE__)
    FILE *stream = NULL;
    char buff[1024];
    char recv_buff[256]      = {0};

    memset(recv_buff, 0, sizeof(recv_buff));

    if ((stream = popen(cmdline.c_str(), "r")) != NULL) {
        while (fgets(buff, 1024, stream)) {
            strcat(recv_buff, buff);
        }
    }
    recv = recv_buff;
    pclose(stream);
#endif
}

void Addr2Line(std::string exe, std::vector<std::string>& strs)
{
#if defined(__unix__) || defined(__APPLE__)
    char str[1024] = {0};
    for (uint32_t i = 0; i < strs.size(); i++) {
        std::string line = strs[i];
        std::string::size_type index = line.find("(+"); // found start stuck
        line = line.substr(index + 1, line.size() - index - 1);
        if (index != std::string::npos) {
            index = line.find(")"); // foud end
            if (index != std::string::npos) {
                line = line.substr(0, index);
                int len = snprintf(str, sizeof(str), "addr2line -e %s %s", exe.c_str(), line.c_str());
                str[len] = 0;
                // std::cout << "Run " << str << std::endl;
                std::string recv;
                Execute(str, recv);
                std::ofstream outfile;
                if (recv.find("??") == std::string::npos) {
                    outfile.open("coredump.log", std::ios::out | std::ios::app);
                    if (outfile.is_open()) {
                        outfile << recv;
                        outfile.close();
                    }
                }
            }
        }
    }
#endif
}

#define PRINT_SIZE_ 100
const char *g_exe_name;

static void _signal_handler(int signum)
{
#ifdef __GLIBC__
    void *array[PRINT_SIZE_];
    char **strings;

    size_t size = backtrace(array, PRINT_SIZE_);
    strings     = backtrace_symbols(array, size);

    if (strings == nullptr) {
        fprintf(stderr, "backtrace_symbols");
        exit(EXIT_FAILURE);
    }
#endif
    switch (signum) {
    case SIGSEGV:
        fprintf(stderr, "widebright received SIGSEGV! Stack trace:\n");
        break;

    case SIGPIPE:
        fprintf(stderr, "widebright received SIGPIPE! Stack trace:\n");
        break;

    case SIGFPE:
        fprintf(stderr, "widebright received SIGFPE! Stack trace:\n");
        break;

    case SIGABRT:
        fprintf(stderr, "widebright received SIGABRT! Stack trace:\n");
        break;

    default:
        break;
    }
#ifdef __GLIBC__
#ifdef BACKTRACE_DEBUG
    std::vector<std::string> strs;
    for (size_t i = 0; i < size; i++) {
        fprintf(stderr, "%ld %s \n", i, strings[i]);
        strs.push_back(strings[i]);
    }
    Addr2Line(g_exe_name, strs);
#else
    std::string path = std::string(g_exe_name) + ".log";
    std::ofstream outfile(path, std::ios::out | std::ios::app);
    if (outfile.is_open()) {
        outfile << "Commit ID: " << GIT_VERSION << std::endl;
        outfile << "Git path: " << GIT_PATH << std::endl;
        outfile << "Compile time: " << __TIME__ << " " << __DATE__ << std::endl;
    }
    for (size_t i = 0; i < size; i++) {
        fprintf(stderr, "%ld %s \n", i, strings[i]);
        if (outfile.is_open()) {
            outfile << strings[i] << std::endl;
        }
    }
    if (outfile.is_open()) {
        outfile.close();
    }
#endif
    free(strings);
#endif
    signal(signum, SIG_DFL); /* 还原默认的信号处理handler */
    fprintf(stderr, "%s quit execute now\n", g_exe_name);
    fflush(stderr);
    exit(-1);
}
#endif

int main(int argc, char* argv[])
{
#ifdef BACKTRACE_DEBUG
    signal(SIGPIPE, _signal_handler); // SIGPIPE，管道破裂。
    signal(SIGSEGV, _signal_handler); // SIGSEGV，非法内存访问
    signal(SIGFPE, _signal_handler);  // SIGFPE，数学相关的异常，如被0除，浮点溢出，等等
    signal(SIGABRT, _signal_handler); // SIGABRT，由调用abort函数产生，进程非正常退出
#endif
    g_exe_name = argv[0];

    Config *conf = Config::get_instance();
#if defined(__APPLE__)
    spdlog::info("current path {}", PROJECT_PATH);
    auto config_path = std::string(PROJECT_PATH) + "/build/guppyconfig.json";
    conf->init(config_path, "/tmp/thumbnails");
#else
    // config
    spdlog::info("current path {}", std::string(fs::canonical("/proc/self/exe").parent_path()));
    auto config_path = fs::canonical("/proc/self/exe").parent_path() / "guppyconfig.json";
    conf->init(config_path.string(), "/tmp/thumbnails");
#endif
#if defined(__linux__)
    struct stat buffer;
    // disable blinking cursor
    if (stat("/dev/tty0", &buffer) == 0) {
      int console_fd = open("/dev/tty0", O_RDWR);
      if (console_fd) {
        ioctl(console_fd, KDSETMODE, KD_GRAPHICS);
      }
    }
#endif
    GuppyScreen::init(hal_init);
    GuppyScreen::loop();
    return 0;
}

#ifndef SIMULATOR

static void hal_init(lv_color_t primary, lv_color_t secondary) {
    /*A small buffer for LittlevGL to draw the screen's content*/
    static lv_color_t buf[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];

    /*Initialize a descriptor for the buffer*/
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, buf2, DISP_BUF_SIZE);

    /*Initialize and register a display driver*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf   = &disp_buf;
    disp_drv.flush_cb   = fbdev_flush;

    uint32_t width;
    uint32_t height;
    uint32_t dpi;
    fbdev_get_sizes(&width, &height, &dpi);
    if (width < height) { // 横屏显示
        disp_drv.sw_rotate = 1;              // add for rotation
        disp_drv.rotated   = LV_DISP_ROT_90; // add for rotation
    }
    
    disp_drv.hor_res    = width;
    disp_drv.ver_res    = height;
    Config *conf = Config::get_instance();
    auto rotate = conf->get_json("/display_rotate");
    if (!rotate.is_null()) {
      auto rotate_value = rotate.template get<uint32_t>();
      if (rotate_value > 0 && rotate_value < 4) {
        disp_drv.sw_rotate = 1;
        disp_drv.rotated = rotate_value;
      }
    }

    spdlog::debug("resolution {} x {}", width, height);
    lv_disp_t * disp = lv_disp_drv_register(&disp_drv);
    lv_theme_t * th = height <= 480
      ? lv_theme_default_init(NULL, primary, secondary, true, &lv_font_montserrat_12)
      : lv_theme_default_init(NULL, primary, secondary, true, &lv_font_montserrat_20);
    lv_disp_set_theme(disp, th);

    evdev_init();
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1);
    indev_drv_1.read_cb = evdev_read; // no calibration
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;

    auto touch_calibrated = conf->get_json("/touch_calibrated");
    if (!touch_calibrated.is_null()) {
      auto is_calibrated = touch_calibrated.template get<bool>();
      if (is_calibrated) {
        spdlog::info("using touch calibration");
        lv_tc_indev_drv_init(&indev_drv_1, evdev_read);
      }
    }
      
    lv_indev_drv_register(&indev_drv_1);
}

#else // SIMULATOR

/**
 * Initialize the Hardware Abstraction Layer (HAL) for the LVGL graphics
 * library
 */
static void hal_init(lv_color_t primary, lv_color_t secondary)
{
  /* Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/
  sdl_init();
  /* Tick init.
   * You have to call 'lv_tick_inc()' in periodically to inform LittelvGL about
   * how much time were elapsed Create an SDL thread to do this*/
  SDL_CreateThread(tick_thread, "tick", NULL);

  /*Create a display buffer*/
  static lv_disp_draw_buf_t disp_buf1;
  static lv_color_t buf1_1[MONITOR_HOR_RES * 100];
  static lv_color_t buf1_2[MONITOR_HOR_RES * 100];
  lv_disp_draw_buf_init(&disp_buf1, buf1_1, buf1_2, MONITOR_HOR_RES * 100);

  /*Create a display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv); /*Basic initialization*/
  disp_drv.draw_buf = &disp_buf1;
  disp_drv.flush_cb = sdl_display_flush;
  spdlog::debug("resolution {} x {}", MONITOR_VER_RES, MONITOR_HOR_RES);
  disp_drv.hor_res = MONITOR_HOR_RES;
  disp_drv.ver_res = MONITOR_VER_RES;
  // disp_drv.sw_rotate = 1;
  // disp_drv.rotated = LV_DISP_ROT_270;
  disp_drv.antialiasing = 1;

  lv_disp_t * disp = lv_disp_drv_register(&disp_drv);
  lv_theme_t * th = MONITOR_HOR_RES <= 480
    ? lv_theme_default_init(NULL, primary, secondary, true, &lv_font_montserrat_12)
    : lv_theme_default_init(NULL, primary, secondary, true, &lv_font_montserrat_16);
  lv_disp_set_theme(disp, th);
 
  lv_group_t * g = lv_group_create();
  lv_group_set_default(g);

  /* Add the mouse as input device
   * Use the 'mouse' driver which reads the PC's mouse*/
  // mouse_init();
  static lv_indev_drv_t indev_drv_1;
  lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
  indev_drv_1.type = LV_INDEV_TYPE_POINTER;

  /*This function will be called periodically (by the library) to get the mouse position and state*/
  indev_drv_1.read_cb = sdl_mouse_read;
  lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv_1);

  // keyboard_init();
  static lv_indev_drv_t indev_drv_2;
  lv_indev_drv_init(&indev_drv_2); /*Basic initialization*/
  indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;
  indev_drv_2.read_cb = sdl_keyboard_read;
  lv_indev_t *kb_indev = lv_indev_drv_register(&indev_drv_2);
  lv_indev_set_group(kb_indev, g);
  // mousewheel_init();
  static lv_indev_drv_t indev_drv_3;
  lv_indev_drv_init(&indev_drv_3); /*Basic initialization*/
  indev_drv_3.type = LV_INDEV_TYPE_ENCODER;
  indev_drv_3.read_cb = sdl_mousewheel_read;

  lv_indev_t * enc_indev = lv_indev_drv_register(&indev_drv_3);
  lv_indev_set_group(enc_indev, g);

  // /*Set a cursor for the mouse*/
  // LV_IMG_DECLARE(mouse_cursor_icon); /*Declare the image file.*/
  // lv_obj_t * cursor_obj = lv_img_create(lv_scr_act()); /*Create an image object for the cursor */
  // lv_img_set_src(cursor_obj, &mouse_cursor_icon);           /*Set the image source*/
  // lv_indev_set_cursor(mouse_indev, cursor_obj);             /*Connect the image  object to the driver*/
}

/**
 * A task to measure the elapsed time for LVGL
 * @param data unused
 * @return never return
 */
static int tick_thread(void *data) {
  (void)data;

  while(1) {
    SDL_Delay(5);
    lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
  }

  return 0;
}

#endif // SIMULATOR
