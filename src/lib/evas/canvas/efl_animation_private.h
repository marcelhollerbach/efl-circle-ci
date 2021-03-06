#define EFL_ANIMATION_PROTECTED

#include "evas_common_private.h"
#include <Ecore.h>

#define MY_CLASS EFL_ANIMATION_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

typedef struct _Efl_Animation_Data
{
   Efl_Canvas_Object        *target;

   double                    duration;

   double                    start_delay_time;

   Efl_Animation_Repeat_Mode repeat_mode;
   int                       repeat_count;

   Efl_Interpolator         *interpolator;

   Eina_Bool                 keep_final_state : 1;
} Efl_Animation_Data;

#define EFL_ANIMATION_DATA_GET(o, pd) \
   Efl_Animation_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_CLASS)
