#include "efl_animation_alpha_private.h"

EOLIAN static void
_efl_animation_alpha_alpha_set(Eo *eo_obj EINA_UNUSED,
                               Efl_Animation_Alpha_Data *pd,
                               double from_alpha,
                               double to_alpha)
{
   pd->from.alpha = from_alpha;
   pd->to.alpha = to_alpha;
}

EOLIAN static void
_efl_animation_alpha_alpha_get(Eo *eo_obj EINA_UNUSED,
                               Efl_Animation_Alpha_Data *pd,
                               double *from_alpha,
                               double *to_alpha)
{
   if (from_alpha)
     *from_alpha = pd->from.alpha;
   if (to_alpha)
     *to_alpha = pd->to.alpha;
}

EOLIAN static Efl_Animation_Object *
_efl_animation_alpha_efl_animation_object_create(Eo *eo_obj,
                                                 Efl_Animation_Alpha_Data *pd)
{
   Efl_Animation_Object_Alpha *anim_obj
      = efl_add(EFL_ANIMATION_OBJECT_ALPHA_CLASS, NULL);

   Efl_Canvas_Object *target = efl_animation_target_get(eo_obj);
   efl_animation_object_target_set(anim_obj, target);

   Eina_Bool state_keep = efl_animation_final_state_keep_get(eo_obj);
   efl_animation_object_final_state_keep_set(anim_obj, state_keep);

   double duration = efl_animation_duration_get(eo_obj);
   efl_animation_object_duration_set(anim_obj, duration);

   double start_delay_time = efl_animation_start_delay_get(eo_obj);
   efl_animation_object_start_delay_set(anim_obj, start_delay_time);

   Efl_Animation_Object_Repeat_Mode repeat_mode =
      (Efl_Animation_Object_Repeat_Mode)efl_animation_repeat_mode_get(eo_obj);
   efl_animation_object_repeat_mode_set(anim_obj, repeat_mode);

   int repeat_count = efl_animation_repeat_count_get(eo_obj);
   efl_animation_object_repeat_count_set(anim_obj, repeat_count);

   Efl_Interpolator *interpolator = efl_animation_interpolator_get(eo_obj);
   efl_animation_object_interpolator_set(anim_obj, interpolator);

   efl_animation_object_alpha_set(anim_obj, pd->from.alpha, pd->to.alpha);

   return anim_obj;
}

EOLIAN static Efl_Object *
_efl_animation_alpha_efl_object_constructor(Eo *eo_obj,
                                            Efl_Animation_Alpha_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->from.alpha = 1.0;
   pd->to.alpha = 1.0;

   return eo_obj;
}

#include "efl_animation_alpha.eo.c"
