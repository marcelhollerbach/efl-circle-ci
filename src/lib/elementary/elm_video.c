#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EO_BASE_BETA
#include <Emotion.h>

#define ELM_INTERFACE_ATSPI_ACCESSIBLE_PROTECTED
#define ELM_INTERFACE_ATSPI_WIDGET_ACTION_PROTECTED

#include <Elementary.h>

#include "elm_priv.h"
#include "elm_widget_layout.h"
#include "elm_widget_video.h"

/* TODO: add buffering support to Emotion and display buffering
 * progress in the theme when needed */

#define MY_CLASS ELM_VIDEO_CLASS

#define MY_CLASS_NAME "Elm_Video"
#define MY_CLASS_NAME_LEGACY "elm_video"

static const Evas_Smart_Cb_Description _smart_callbacks[] = {
   {SIG_LAYOUT_FOCUSED, ""}, /**< handled by elm_layout */
   {SIG_LAYOUT_UNFOCUSED, ""}, /**< handled by elm_layout */
   {NULL, NULL}
};


static Eina_Bool
_on_open_done(void *data, const Eo_Event *event);
static Eina_Bool
_on_playback_started(void *data, const Eo_Event *event);
static Eina_Bool
_on_playback_finished(void *data, const Eo_Event *event);
static Eina_Bool
_on_aspect_ratio_updated(void *data, const Eo_Event *event);
static Eina_Bool
_on_title_changed(void *data, const Eo_Event *event);
static Eina_Bool
_on_audio_level_changed(void *data, const Eo_Event *event);

static Eina_Bool _key_action_move(Evas_Object *obj, const char *params);
static Eina_Bool _key_action_play(Evas_Object *obj, const char *params);

static const Elm_Action key_actions[] = {
   {"move", _key_action_move},
   {"play", _key_action_play},
   {NULL, NULL}
};

EO_CALLBACKS_ARRAY_DEFINE(_video_cb,
   { EMOTION_OBJECT_EVENT_OPEN_DONE, _on_open_done },
   { EMOTION_OBJECT_EVENT_PLAYBACK_STARTED, _on_playback_started },
   { EMOTION_OBJECT_EVENT_PLAYBACK_FINISHED, _on_playback_finished },
   { EMOTION_OBJECT_EVENT_FRAME_RESIZE, _on_aspect_ratio_updated },
   { EMOTION_OBJECT_EVENT_TITLE_CHANGE, _on_title_changed },
   { EMOTION_OBJECT_EVENT_AUDIO_LEVEL_CHANGE, _on_audio_level_changed }
);

static Eina_Bool
_key_action_move(Evas_Object *obj, const char *params)
{
   const char *dir = params;

   _elm_widget_focus_auto_show(obj);
   if (!strcmp(dir, "left"))
     {
        double current, last;

        current = elm_video_play_position_get(obj);
        last = elm_video_play_length_get(obj);

        if (current < last)
          {
             current += last / 100;
             elm_video_play_position_set(obj, current);
          }
     }
   else if (!strcmp(dir, "right"))
     {
        double current, last;

        current = elm_video_play_position_get(obj);
        last = elm_video_play_length_get(obj);

        if (current > 0)
          {
             current -= last / 100;
             if (current < 0) current = 0;
             elm_video_play_position_set(obj, current);
          }
     }
   else return EINA_FALSE;

   return EINA_TRUE;
}

static Eina_Bool
_key_action_play(Evas_Object *obj, const char *params EINA_UNUSED)
{
   if (elm_video_is_playing_get(obj))
     elm_video_pause(obj);
   else
     elm_video_play(obj);

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_elm_video_elm_widget_event(Eo *obj, Elm_Video_Data *_pd EINA_UNUSED, Evas_Object *src, Evas_Callback_Type type, void *event_info)
{
   (void) src;
   Evas_Event_Key_Down *ev = event_info;

   if (type != EVAS_CALLBACK_KEY_DOWN) return EINA_FALSE;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return EINA_FALSE;

   if (!_elm_config_key_binding_call(obj, MY_CLASS_NAME, ev, key_actions))
     {
        INF("keyname: '%s' not handled", ev->key);
        return EINA_FALSE;
     }

   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   return EINA_TRUE;
}

EOLIAN static void
_elm_video_elm_layout_sizing_eval(Eo *obj, Elm_Video_Data *sd)
{
   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd);

   Evas_Coord minw = 0, minh = 0;
   Evas_Coord w = 0, h = 0;

   evas_object_size_hint_request_get(sd->emotion, &minw, &minh);
   evas_object_size_hint_aspect_set
     (sd->emotion, EVAS_ASPECT_CONTROL_BOTH, minw, minh);
   edje_object_size_min_calc(wd->resize_obj, &w, &h);

   if (w != 0 && h != 0)
     {
        minw = w;
        minh = h;
     }

   evas_object_size_hint_aspect_set(obj, EVAS_ASPECT_CONTROL_BOTH, minw, minh);
}

static void
_on_size_hints_changed(void *data EINA_UNUSED,
                       Evas *e EINA_UNUSED,
                       Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   elm_layout_sizing_eval(obj);
}

static Eina_Bool
_on_open_done(void *data, const Eo_Event *event EINA_UNUSED)
{
   elm_layout_signal_emit(data, "elm,video,open", "elm");

   return EINA_TRUE;
}

static Eina_Bool
_on_playback_started(void *data, const Eo_Event *event EINA_UNUSED)
{
   elm_layout_signal_emit(data, "elm,video,play", "elm");

   return EINA_TRUE;

}

static Eina_Bool
_on_playback_finished(void *data, const Eo_Event *event EINA_UNUSED)
{
   ELM_VIDEO_DATA_GET(data, sd);
   emotion_object_play_set(sd->emotion, EINA_FALSE);
   elm_layout_signal_emit(data, "elm,video,end", "elm");

   return EINA_TRUE;
}

static Eina_Bool
_on_aspect_ratio_updated(void *data, const Eo_Event *event EINA_UNUSED)
{
   elm_layout_sizing_eval(data);

   return EINA_TRUE;
}

static Eina_Bool
_on_title_changed(void *data, const Eo_Event *event EINA_UNUSED)
{
   const char *title;

   ELM_VIDEO_DATA_GET(data, sd);

   title = emotion_object_title_get(sd->emotion);
   elm_layout_text_set(data, "elm,title", title);
   elm_layout_signal_emit(data, "elm,video,title", "elm");

   return EINA_TRUE;
}

static Eina_Bool
_on_audio_level_changed(void *data, const Eo_Event *event EINA_UNUSED)
{
   (void)data;

   return EINA_TRUE;
}

static Eina_Bool
_suspend_cb(void *data)
{
   double interval;

   ELM_VIDEO_DATA_GET(data, sd);

   interval = ecore_timer_interval_get(sd->timer);
   if (interval <= 20)
     emotion_object_suspend_set(sd->emotion, EMOTION_SLEEP);
   else if (interval <= 30)
     emotion_object_suspend_set(sd->emotion, EMOTION_DEEP_SLEEP);
   else
     {
        emotion_object_suspend_set(sd->emotion, EMOTION_HIBERNATE);
        sd->timer = NULL;

        return ECORE_CALLBACK_CANCEL;
     }

   ecore_timer_interval_set(sd->timer, interval + 10);
   return ECORE_CALLBACK_RENEW;
}

Eina_Bool
_elm_video_check(Evas_Object *video)
{
   ELM_VIDEO_CHECK(video) EINA_FALSE;
   return EINA_TRUE;
}

EOLIAN static void
_elm_video_evas_object_smart_add(Eo *obj, Elm_Video_Data *priv)
{
   _elm_emotion_init();

   evas_obj_smart_add(eo_super(obj, MY_CLASS));
   elm_widget_sub_object_parent_add(obj);
   elm_widget_can_focus_set(obj, EINA_TRUE);

   priv->emotion = emotion_object_add(evas_object_evas_get(obj));
   emotion_object_init(priv->emotion, NULL);

   if (!elm_layout_theme_set(obj, "video", "base", elm_widget_style_get(obj)))
     CRI("Failed to set layout!");

   elm_layout_content_set(obj, "elm.swallow.video", priv->emotion);

   eo_event_callback_array_add(priv->emotion, _video_cb(), obj);

   evas_object_event_callback_add
     (obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _on_size_hints_changed, NULL);
   eo_composite_attach(obj, priv->emotion);

   priv->timer = ecore_timer_add(20.0, _suspend_cb, obj);
}

EOLIAN static void
_elm_video_evas_object_smart_del(Eo *obj, Elm_Video_Data *sd)
{
   ecore_timer_del(sd->timer);
   if (sd->remember) emotion_object_last_position_save(sd->emotion);

   evas_obj_smart_del(eo_super(obj, MY_CLASS));
}

EAPI Evas_Object *
elm_video_add(Evas_Object *parent)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);
   Evas_Object *obj = eo_add(MY_CLASS, parent);
   return obj;
}

EOLIAN static Eo *
_elm_video_eo_base_constructor(Eo *obj, Elm_Video_Data *_pd EINA_UNUSED)
{
   obj = eo_constructor(eo_super(obj, MY_CLASS));
   evas_obj_type_set(obj, MY_CLASS_NAME_LEGACY);
   evas_object_smart_callbacks_descriptions_set(obj, _smart_callbacks);
   elm_interface_atspi_accessible_role_set(obj, ELM_ATSPI_ROLE_ANIMATION);

   return obj;
}

EOLIAN static Eina_Bool
_elm_video_efl_file_file_set(Eo *obj, Elm_Video_Data *sd, const char *filename, const char *key EINA_UNUSED)
{
   if (sd->remember) emotion_object_last_position_save(sd->emotion);
   sd->stop = EINA_FALSE;
   if (!emotion_object_file_set(sd->emotion, filename)) return EINA_FALSE;

   if (filename && ((!strncmp(filename, "file://", 7)) || (!strstr(filename, "://"))))
     emotion_object_last_position_load(sd->emotion);

   elm_layout_signal_emit(obj, "elm,video,load", "elm");

   return EINA_TRUE;
}

EOLIAN static void
_elm_video_efl_file_file_get(Eo *obj EINA_UNUSED, Elm_Video_Data *sd EINA_UNUSED, const char **filename, const char **key EINA_UNUSED)
{
   if (filename)
     *filename = emotion_object_file_get(sd->emotion);
}

EOLIAN static Evas_Object*
_elm_video_emotion_get(Eo *obj EINA_UNUSED, Elm_Video_Data *sd)
{
   return sd->emotion;
}

EOLIAN static void
_elm_video_play(Eo *obj EINA_UNUSED, Elm_Video_Data *sd)
{
   if (emotion_object_play_get(sd->emotion)) return;

   ELM_SAFE_FREE(sd->timer, ecore_timer_del);
   sd->stop = EINA_FALSE;
   emotion_object_play_set(sd->emotion, EINA_TRUE);
   elm_layout_signal_emit(obj, "elm,video,play", "elm");
}

/* FIXME: pause will setup timer and go into sleep or
 * hibernate after a while without activity.
 */
EOLIAN static void
_elm_video_pause(Eo *obj, Elm_Video_Data *sd)
{
   if (!emotion_object_play_get(sd->emotion)) return;

   if (!sd->timer) sd->timer = ecore_timer_add(20.0, _suspend_cb, obj);
   emotion_object_play_set(sd->emotion, EINA_FALSE);
   elm_layout_signal_emit(obj, "elm,video,pause", "elm");
}

/* FIXME: stop should go into hibernate state directly.
 */
EOLIAN static void
_elm_video_stop(Eo *obj, Elm_Video_Data *sd)
{
   if (!emotion_object_play_get(sd->emotion) && sd->stop) return;

   ELM_SAFE_FREE(sd->timer, ecore_timer_del);

   sd->stop = EINA_TRUE;
   emotion_object_play_set(sd->emotion, EINA_FALSE);
   elm_layout_signal_emit(obj, "elm,video,stop", "elm");
   emotion_object_suspend_set(sd->emotion, EMOTION_HIBERNATE);
}

EOLIAN static Eina_Bool
_elm_video_is_playing_get(Eo *obj EINA_UNUSED, Elm_Video_Data *sd)
{
   return emotion_object_play_get(sd->emotion);
}

EOLIAN static const char*
_elm_video_title_get(Eo *obj EINA_UNUSED, Elm_Video_Data *sd)
{
   return emotion_object_title_get(sd->emotion);
}

EOLIAN static void
_elm_video_remember_position_set(Eo *obj EINA_UNUSED, Elm_Video_Data *sd, Eina_Bool remember)
{
   sd->remember = remember;
}

EOLIAN static Eina_Bool
_elm_video_remember_position_get(Eo *obj EINA_UNUSED, Elm_Video_Data *sd)
{
   return sd->remember;
}

EOLIAN static Eina_Bool
_elm_video_elm_widget_focus_next_manager_is(Eo *obj EINA_UNUSED, Elm_Video_Data *_pd EINA_UNUSED)
{
   return EINA_FALSE;
}

EOLIAN static Eina_Bool
_elm_video_elm_widget_focus_direction_manager_is(Eo *obj EINA_UNUSED, Elm_Video_Data *_pd EINA_UNUSED)
{
   return EINA_FALSE;
}

EOLIAN static void
_elm_video_class_constructor(Eo_Class *klass)
{
   evas_smart_legacy_type_register(MY_CLASS_NAME_LEGACY, klass);
}

EOLIAN const Elm_Atspi_Action *
_elm_video_elm_interface_atspi_widget_action_elm_actions_get(Eo *obj EINA_UNUSED, Elm_Video_Data *pd EINA_UNUSED)
{
   static Elm_Atspi_Action atspi_actions[] = {
          { "move,left", "move", "left", _key_action_move},
          { "move,right", "move", "right", _key_action_move},
          { "play", "play", NULL, _key_action_play},
          { NULL, NULL, NULL, NULL}
   };
   return &atspi_actions[0];
}

EAPI Eina_Bool
elm_video_file_set(Eo *obj, const char *filename)
{
   return efl_file_set((Eo *) obj, filename, NULL);
}

EAPI void
elm_video_file_get(Eo *obj, const char **filename)
{
   efl_file_get((Eo *) obj, filename, NULL);
}

EAPI void
elm_video_audio_level_set(Evas_Object *obj, double volume)
{
   efl_player_audio_volume_set(obj, volume);
}

EAPI double
elm_video_audio_level_get(const Evas_Object *obj)
{
   return efl_player_audio_volume_get(obj);
}

EAPI void
elm_video_audio_mute_set(Evas_Object *obj, Eina_Bool mute)
{
   efl_player_audio_mute_set(obj, mute);
}

EAPI Eina_Bool
elm_video_audio_mute_get(const Evas_Object *obj)
{
   return efl_player_audio_mute_get(obj);
}

EAPI double
elm_video_play_length_get(const Evas_Object *obj)
{
   return efl_player_length_get(obj);
}

EAPI Eina_Bool
elm_video_is_seekable_get(const Evas_Object *obj)
{
   return efl_player_seekable_get(obj);
}

EAPI void
elm_video_play_position_set(Evas_Object *obj, double position)
{
   efl_player_position_set(obj, position);
}

EAPI double
elm_video_play_position_get(const Evas_Object *obj)
{
   return efl_player_position_get(obj);
}

#include "elm_video.eo.c"
