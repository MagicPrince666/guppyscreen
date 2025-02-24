#ifndef __TMC_STATUS_PANEL_H__
#define __TMC_STATUS_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "notify_consumer.h"
#include "tmc_status_container.h"
#include "websocket_client.h"

#include <map>
#include <memory>

class TmcStatusPanel : public NotifyConsumer
{
public:
    TmcStatusPanel(GcodeTransmitClient &ws,
                   std::mutex &lv_lock);
    ~TmcStatusPanel();

    void foreground();
    void background();

    void init(json &);
    void consume(json &j);

private:
    GcodeTransmitClient &gcode_transmit_;
    lv_obj_t *cont;
    lv_obj_t *top;
    lv_obj_t *toggle;
    ButtonContainer back_btn;
    std::map<std::string, std::shared_ptr<TmcStatusContainer>> metrics;
};

#endif // __TMC_STATUS_PANEL_H__
