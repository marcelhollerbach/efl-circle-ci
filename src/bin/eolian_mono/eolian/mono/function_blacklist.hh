#ifndef EOLIAN_MONO_FUNCTION_BLACKLIST_HH
#define EOLIAN_MONO_FUNCTION_BLACKLIST_HH

namespace eolian_mono {

inline bool is_function_blacklisted(std::string const& c_name)
{
  return
    c_name == "efl_event_callback_array_priority_add"
    || c_name == "efl_player_position_get"
    || c_name == "efl_image_load_error_get"
    || c_name == "efl_text_font_source_get"
    || c_name == "efl_text_font_source_set"
    || c_name == "efl_ui_focus_manager_focus_get"
    || c_name == "efl_ui_widget_focus_set"
    || c_name == "efl_ui_widget_focus_get"
    || c_name == "efl_ui_text_password_get"
    || c_name == "efl_ui_text_password_set"
    || c_name == "elm_interface_scrollable_repeat_events_get"
    || c_name == "elm_interface_scrollable_repeat_events_set"
    || c_name == "elm_wdg_item_del"
    || c_name == "elm_wdg_item_focus_get"
    || c_name == "elm_wdg_item_focus_set"
    || c_name == "elm_interface_scrollable_mirrored_set"
    || c_name == "evas_obj_table_mirrored_get"
    || c_name == "evas_obj_table_mirrored_set"
    || c_name == "edje_obj_load_error_get"
    || c_name == "efl_ui_focus_user_parent_get"
    || c_name == "efl_canvas_object_scale_get" // duplicated signature
    || c_name == "efl_canvas_object_scale_set" // duplicated signature
    || c_name == "efl_ui_format_cb_set"
    || c_name == "efl_access_parent_get"
    || c_name == "efl_access_name_get"
    || c_name == "efl_access_name_set"
    || c_name == "efl_access_root_get"
    || c_name == "efl_access_type_get"
    || c_name == "efl_access_role_get"
    || c_name == "efl_access_action_description_get"
    || c_name == "efl_access_action_description_set"
    || c_name == "efl_access_image_description_get"
    || c_name == "efl_access_image_description_set"
    || c_name == "efl_access_component_layer_get" // duplicated signature
    || c_name == "efl_access_component_alpha_get"
    || c_name == "efl_ui_spin_button_loop_get"
    ;
}

}

#endif
