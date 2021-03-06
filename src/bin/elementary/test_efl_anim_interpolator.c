#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif
#include <Elementary.h>

#define INTERP_NUM 7
#define BTN_NUM (INTERP_NUM + 1)
#define BTN_W 50
#define BTN_H 50
#define WIN_H (BTN_NUM * BTN_H)
#define WIN_W WIN_H

typedef struct _App_Data
{
   Efl_Animation        *anim[INTERP_NUM];
   Efl_Animation_Object *anim_obj[INTERP_NUM];

   Evas_Object          *btn[INTERP_NUM];
   Evas_Object          *start_all_btn;

   Eina_Bool             running_anim_cnt;
} App_Data;

static Efl_Interpolator *
_interpolator_create(int index)
{
   Efl_Interpolator *interp = NULL;

   if (index == 0)
     {
        interp = efl_add(EFL_INTERPOLATOR_LINEAR_CLASS, NULL);
     }
   else if (index == 1)
     {
        interp = efl_add(EFL_INTERPOLATOR_SINUSOIDAL_CLASS, NULL);
        efl_interpolator_sinusoidal_factor_set(interp, 1.0);
     }
   else if (index == 2)
     {
        interp = efl_add(EFL_INTERPOLATOR_DECELERATE_CLASS, NULL);
        efl_interpolator_decelerate_factor_set(interp, 1.0);
     }
   else if (index == 3)
     {
        interp = efl_add(EFL_INTERPOLATOR_ACCELERATE_CLASS, NULL);
        efl_interpolator_accelerate_factor_set(interp, 1.0);
     }
   else if (index == 4)
     {
        interp = efl_add(EFL_INTERPOLATOR_DIVISOR_CLASS, NULL);
        efl_interpolator_divisor_factors_set(interp, 1.0, 1.0);
     }
   else if (index == 5)
     {
        interp = efl_add(EFL_INTERPOLATOR_BOUNCE_CLASS, NULL);
        efl_interpolator_bounce_factors_set(interp, 1.0, 1.0);
     }
   else if (index == 6)
     {
        interp = efl_add(EFL_INTERPOLATOR_SPRING_CLASS, NULL);
        efl_interpolator_spring_factors_set(interp, 1.0, 1.0);
     }

   return interp;
}

static void
_anim_started_cb(void *data, const Efl_Event *event EINA_UNUSED)
{
   App_Data *ad = data;

   printf("Animation has been started!\n");

   ad->running_anim_cnt++;
}

static void
_anim_ended_cb(void *data, const Efl_Event *event)
{
   App_Data *ad = data;

   printf("Animation has been ended!\n");

   ad->running_anim_cnt--;

   int i;
   for (i = 0; i < INTERP_NUM; i++)
     {
        if (ad->anim_obj[i] == event->object)
          {
             ad->anim_obj[i] = NULL;
             elm_object_disabled_set(ad->btn[i], EINA_FALSE);
             break;
          }
     }

   if (ad->running_anim_cnt == 0)
     elm_object_disabled_set(ad->start_all_btn, EINA_FALSE);
}

static void
_anim_running_cb(void *data EINA_UNUSED, const Efl_Event *event)
{
   Efl_Animation_Object_Running_Event_Info *event_info = event->info;
   double progress = event_info->progress;
   printf("Animation is running! Current progress(%lf)\n", progress);
}

static void
_anim_start(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   App_Data *ad = data;

   int index = (uintptr_t)evas_object_data_get(obj, "index");

   //Create Animation Object from Animation
   Efl_Animation_Object *anim_obj = efl_animation_object_create(ad->anim[index]);
   ad->anim_obj[index] = anim_obj;

   //Register callback called when animation starts
   efl_event_callback_add(anim_obj, EFL_ANIMATION_OBJECT_EVENT_STARTED, _anim_started_cb, ad);

   //Register callback called when animation ends
   efl_event_callback_add(anim_obj, EFL_ANIMATION_OBJECT_EVENT_ENDED, _anim_ended_cb, ad);

   //Register callback called while animation is executed
   efl_event_callback_add(anim_obj, EFL_ANIMATION_OBJECT_EVENT_RUNNING, _anim_running_cb, NULL);

   //Let Animation Object start animation
   efl_animation_object_start(anim_obj);

   elm_object_disabled_set(obj, EINA_TRUE);
   elm_object_disabled_set(ad->start_all_btn, EINA_TRUE);
}

static void
_anim_start_all(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   App_Data *ad = data;

   int i;
   for (i = 0; i < INTERP_NUM; i++)
     {
        //Create Animation Object from Animation
        Efl_Animation_Object *anim_obj = efl_animation_object_create(ad->anim[i]);
        ad->anim_obj[i] = anim_obj;

        //Register callback called when animation starts
        efl_event_callback_add(anim_obj, EFL_ANIMATION_OBJECT_EVENT_STARTED, _anim_started_cb, ad);

        //Register callback called when animation ends
        efl_event_callback_add(anim_obj, EFL_ANIMATION_OBJECT_EVENT_ENDED, _anim_ended_cb, ad);

        //Register callback called while animation is executed
        efl_event_callback_add(anim_obj, EFL_ANIMATION_OBJECT_EVENT_RUNNING, _anim_running_cb, NULL);

        //Let Animation Object start animation
        efl_animation_object_start(anim_obj);

        elm_object_disabled_set(ad->btn[i], EINA_TRUE);
     }

   elm_object_disabled_set(obj, EINA_TRUE);
}

static void
_win_del_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   App_Data *ad = data;
   free(ad);
}

void
test_efl_anim_interpolator(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   App_Data *ad = calloc(1, sizeof(App_Data));
   if (!ad) return;

   const char *modes[] = {"LINEAR", "SINUSOIDAL", "DECELERATE", "ACCELERATE",
                          "DIVISOR_INTERP", "BOUNCE", "SPRING"};

   Evas_Object *win = elm_win_add(NULL, "Efl Animation Interpolator", ELM_WIN_BASIC);
   elm_win_title_set(win, "Efl Animation Interpolator");
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ad);

   //Button to start all animations
   Evas_Object *start_all_btn = elm_button_add(win);
   elm_object_text_set(start_all_btn, "Start All");
   evas_object_resize(start_all_btn, WIN_W, BTN_H);
   evas_object_move(start_all_btn, 0, (WIN_H - BTN_H));
   evas_object_show(start_all_btn);
   evas_object_smart_callback_add(start_all_btn, "clicked", _anim_start_all, ad);
   ad->start_all_btn = start_all_btn;

   int i;
   for (i = 0; i < INTERP_NUM; i++)
     {
        Evas_Object *label = elm_label_add(win);
        elm_object_text_set(label, modes[i]);
        evas_object_resize(label, WIN_W, BTN_H);
        evas_object_move(label, 0, (i * BTN_H));
        evas_object_show(label);

        //Button to be animated
        Evas_Object *btn = elm_button_add(win);
        evas_object_data_set(btn, "index", (void *)(uintptr_t)i);
        elm_object_text_set(btn, "Start");
        evas_object_resize(btn, BTN_W, BTN_H);
        evas_object_move(btn, 0, (i * BTN_H));
        evas_object_show(btn);
        evas_object_smart_callback_add(btn, "clicked", _anim_start, ad);
        ad->btn[i] = btn;

        Efl_Animation *anim = efl_add(EFL_ANIMATION_TRANSLATE_CLASS, NULL);
        efl_animation_translate_set(anim, 0, 0, (WIN_W - BTN_W), 0);
        efl_animation_duration_set(anim, 2.0);
        efl_animation_target_set(anim, btn);

        Efl_Interpolator *interp = _interpolator_create(i);
        efl_animation_interpolator_set(anim, interp);
        ad->anim[i] = anim;
     }

   ad->running_anim_cnt = 0;

   evas_object_resize(win, WIN_W, WIN_H);
   evas_object_show(win);
}
