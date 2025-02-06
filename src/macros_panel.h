#ifndef __MACROS_PANEL_H__
#define __MACROS_PANEL_H__

#include "lvgl/lvgl.h"
#include "macro_item.h"
#include "websocket_client.h"

#include <memory>
#include <mutex>
#include <vector>

class MacrosPanel
{
public:
    MacrosPanel(GcodeTransmitClient &c, std::mutex &l, lv_obj_t *parent);
    ~MacrosPanel();

    void populate();
    void handle_hide_show(lv_event_t *e);

    static void _handle_hide_show(lv_event_t *e)
    {
        MacrosPanel *panel = (MacrosPanel *)e->user_data;
        panel->handle_hide_show(e);
    };

private:
    GcodeTransmitClient &gcode_transmit_;
    std::mutex &lv_lock;
    lv_obj_t *cont;
    lv_obj_t *top_controls;
    lv_obj_t *show_hide_switch;
    lv_obj_t *top_cont;
    lv_obj_t *kb;
    std::vector<std::shared_ptr<MacroItem>> macro_items;
};

#endif // __MACROS_PANEL_H__
