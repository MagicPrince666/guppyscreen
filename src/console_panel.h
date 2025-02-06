#ifndef __CONSOLE_PANEL_H__
#define __CONSOLE_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <list>
#include <mutex>

class ConsolePanel
{
public:
    ConsolePanel(GcodeTransmitClient &ws, std::mutex &lock, lv_obj_t *parent);
    ~ConsolePanel();

    lv_obj_t *get_container();
    void foreground();
    void handle_kb_input(lv_event_t *e);
    void handle_select_macro(lv_event_t *e);
    void handle_macros(json &d);
    void handle_macro_response(json &d);
    void handle_send_macro(lv_event_t *e);
    void handle_clear_input(lv_event_t *e);

    static void _handle_kb_input(lv_event_t *e)
    {
        ConsolePanel *panel = (ConsolePanel *)e->user_data;
        panel->handle_kb_input(e);
    };

    static void _handle_select_macro(lv_event_t *e)
    {
        ConsolePanel *panel = (ConsolePanel *)e->user_data;
        panel->handle_select_macro(e);
    };

    static void _handle_send_macro(lv_event_t *e)
    {
        ConsolePanel *panel = (ConsolePanel *)e->user_data;
        panel->handle_send_macro(e);
    };

    static void _handle_clear_input(lv_event_t *e)
    {
        ConsolePanel *panel = (ConsolePanel *)e->user_data;
        panel->handle_clear_input(e);
    };

private:
    GcodeTransmitClient &gcode_transmit_;
    std::mutex &lv_lock;
    lv_obj_t *console_cont;
    lv_obj_t *top_cont;
    lv_obj_t *output;
    lv_obj_t *macro_list;
    lv_obj_t *input_cont;
    lv_obj_t *input;
    lv_obj_t *kb;
    std::list<std::string> all_macros;
    std::list<std::string> history;
};

#endif // __CONSOLE_PANEL_H__
