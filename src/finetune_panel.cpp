#include "finetune_panel.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <algorithm>

LV_IMG_DECLARE(home_z);
LV_IMG_DECLARE(z_closer);
LV_IMG_DECLARE(z_farther);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(speed_up_img);
LV_IMG_DECLARE(speed_down_img);
LV_IMG_DECLARE(flow_up_img);
LV_IMG_DECLARE(flow_down_img);
LV_IMG_DECLARE(back);

FineTunePanel::FineTunePanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , zreset_btn(panel_cont, &refresh_img, "Reset Z", &FineTunePanel::_handle_zoffset, this)
  , zup_btn(panel_cont, &z_closer, "Z+", &FineTunePanel::_handle_zoffset, this)
  , zdown_btn(panel_cont, &z_farther, "Z-", &FineTunePanel::_handle_zoffset, this)
  , speed_reset_btn(panel_cont, &refresh_img, "Speed Reset", &FineTunePanel::_handle_speed, this)    
  , speed_up_btn(panel_cont, &speed_up_img, "Speed+", &FineTunePanel::_handle_speed, this)
  , speed_down_btn(panel_cont, &speed_down_img, "Speed-", &FineTunePanel::_handle_speed, this)
  , flow_reset_btn(panel_cont, &refresh_img, "Flow Reset", &FineTunePanel::_handle_flow, this)
  , flow_up_btn(panel_cont, &flow_up_img, "Flow+", &FineTunePanel::_handle_flow, this)
  , flow_down_btn(panel_cont, &flow_down_img, "Flow-", &FineTunePanel::_handle_flow, this)
  , back_btn(panel_cont, &back, "Back", &FineTunePanel::_handle_callback, this)
  , zoffset_selector(panel_cont, "Baby Step (mm)",
		     {"0.01", "0.05", ""}, 0, 20, 15, &FineTunePanel::_handle_callback, this)
  , multipler_selector(panel_cont, "Multipler Step (%)",
		       {"1", "5", "10", "25", ""}, 0, 40, 15, &FineTunePanel::_handle_callback, this)
  , z_offset(panel_cont, &home_z, 150, 20, 15, "0.0 mm")
  , speed_factor(panel_cont, &speed_up_img, 150, 20, 15 ,"100%")
  , flow_factor(panel_cont, &flow_up_img, 150, 20, 15, "100%")
{
  lv_obj_set_size(panel_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);

  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(panel_cont, grid_main_col_dsc, grid_main_row_dsc);

  // col 1
  lv_obj_set_grid_cell(zreset_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(zup_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_set_grid_cell(zdown_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  lv_obj_set_grid_cell(zoffset_selector.get_label(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 3, 1);
  lv_obj_set_grid_cell(zoffset_selector.get_selector(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1); 

  // col 2
  lv_obj_set_grid_cell(speed_reset_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(speed_up_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_set_grid_cell(speed_down_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  lv_obj_set_grid_cell(multipler_selector.get_label(), LV_GRID_ALIGN_CENTER, 1, 2, LV_GRID_ALIGN_START, 3, 1);
  lv_obj_set_grid_cell(multipler_selector.get_selector(), LV_GRID_ALIGN_CENTER, 1, 2, LV_GRID_ALIGN_CENTER, 3, 1);  

  // col 3
  lv_obj_set_grid_cell(flow_reset_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(flow_up_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_set_grid_cell(flow_down_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 2, 1);

  // col 4
  lv_obj_set_grid_cell(z_offset.get_container(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(speed_factor.get_container(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_set_grid_cell(flow_factor.get_container(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  lv_obj_set_grid_cell(back_btn.get_container(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 3, 1);

  ws.register_notify_update(this);
}

FineTunePanel::~FineTunePanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

void FineTunePanel::foreground() {
  // XXX: read zoffset, speedfactor, extrude factor from state.
  lv_obj_move_foreground(panel_cont);
}

void FineTunePanel::consume(json &j) {
  auto v = j["/params/0/gcode_move/homing_origin/2"_json_pointer];
  if (!v.is_null()) {
    
    z_offset.update_label(fmt::format("{:.5} mm", v.template get<double>()).c_str());
  }

  v = j["/params/0/gcode_move/speed_factor"_json_pointer];
  if (!v.is_null()) {
    speed_factor.update_label(fmt::format("{}%",
	   static_cast<int>(v.template get<double>() * 100)).c_str());
  }

  v = j["/params/0/gcode_move/extrude_factor"_json_pointer];
  if (!v.is_null()) {
    flow_factor.update_label(fmt::format("{}%",
	   static_cast<int>(v.template get<double>() * 100)).c_str());
  }
}

void FineTunePanel::handle_callback(lv_event_t *e) {
  spdlog::trace("fine tune btn callback");
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *selector = lv_event_get_target(e);
    uint32_t idx = lv_btnmatrix_get_selected_btn(selector);

    if (selector == zoffset_selector.get_selector()) {
      zoffset_selector.set_selected_idx(idx);
    }

    if (selector == multipler_selector.get_selector()) {
      multipler_selector.set_selected_idx(idx);
    }
  }
  
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == back_btn.get_button()) {
      lv_obj_move_background(panel_cont);
    }
  }
}

void FineTunePanel::handle_zoffset(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_target(e);

    if (btn == zreset_btn.get_button()) {
      spdlog::trace("clicked zoffset reset");
      ws.gcode_script("SET_GCODE_OFFSET Z=0 MOVE=1");
    } else {
      const char * step = lv_btnmatrix_get_btn_text(zoffset_selector.get_selector(),
						    zoffset_selector.get_selected_idx());
      spdlog::trace("clicked z {}", step);
      ws.gcode_script(fmt::format("SET_GCODE_OFFSET Z_ADJUST={}{} MOVE=1",
				  btn == zup_btn.get_button() ? "+" : "-",
				  step));
    }
  }
}

void FineTunePanel::handle_speed(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == speed_reset_btn.get_button()) {
      spdlog::trace("speed reset");
      ws.gcode_script("M220 S100");
    } else {
      auto spd_factor = State::get_instance()
	->get_data("/printer_state/gcode_move/speed_factor"_json_pointer);
      if (!spd_factor.is_null()) {
	const char * step = lv_btnmatrix_get_btn_text(multipler_selector.get_selector(),
						      multipler_selector.get_selected_idx());

	int32_t direction = btn == speed_up_btn.get_button() ? std::stoi(step) : -std::stoi(step);
	int32_t new_speed = static_cast<int32_t>(spd_factor.template get<double>() * 100 + direction);
	new_speed = std::max(new_speed, 1);
	spdlog::trace("speed step {}, {}", direction, new_speed);
	ws.gcode_script(fmt::format("M220 S{}", new_speed));
      }
    }
  }
}

void FineTunePanel::handle_flow(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == flow_reset_btn.get_button()) {
      spdlog::trace("flow reset");
      ws.gcode_script("M221 S100");
    } else {
      auto extrude_factor = State::get_instance()
	->get_data("/printer_state/gcode_move/extrude_factor"_json_pointer);
      if (!extrude_factor.is_null()) {
	const char * step = lv_btnmatrix_get_btn_text(multipler_selector.get_selector(),
						      multipler_selector.get_selected_idx());

	int32_t direction = btn == flow_up_btn.get_button() ? std::stoi(step) : -std::stoi(step);
	
	int32_t new_flow = static_cast<int32_t>(extrude_factor.template get<double>() * 100 + direction);
	new_flow = std::max(new_flow, 1);
	spdlog::trace("flow step {}, {}", direction, new_flow);
	ws.gcode_script(fmt::format("M221 S{}", new_flow));
      }
    }
  }
}
