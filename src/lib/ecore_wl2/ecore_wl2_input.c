#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ecore_wl2_private.h"

static const struct wl_data_device_listener _data_listener =
{
   NULL, // data offer
   NULL, // data enter
   NULL, // data leave
   NULL, // data motion
   NULL, // data drop
   NULL, // data selection
};

static void
_seat_cb_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
   Ecore_Wl2_Input *input;

   input = data;
   if (!input) return;

   if ((caps & WL_SEAT_CAPABILITY_POINTER) && (!input->wl.pointer))
     {
        input->wl.pointer = wl_seat_get_pointer(seat);
        wl_pointer_set_user_data(input->wl.pointer, input);
        /* TODO: pointer listener */
        /* TODO: pointer surface */
     }
   else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && (input->wl.pointer))
     {
        if (input->cursor.surface) wl_surface_destroy(input->cursor.surface);
        input->cursor.surface = NULL;

        wl_pointer_destroy(input->wl.pointer);
        input->wl.pointer = NULL;
     }
   else if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && (!input->wl.keyboard))
     {
        input->wl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_set_user_data(input->wl.keyboard, input);
        /* TODO: keyboard listener */
     }
   else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && (input->wl.keyboard))
     {
        wl_keyboard_destroy(input->wl.keyboard);
        input->wl.keyboard = NULL;
     }
   else if ((caps & WL_SEAT_CAPABILITY_TOUCH) && (!input->wl.touch))
     {
        input->wl.touch = wl_seat_get_touch(seat);
        wl_touch_set_user_data(input->wl.touch, input);
        /* TODO: touch listener */
     }
   else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && (input->wl.touch))
     {
        wl_touch_destroy(input->wl.touch);
        input->wl.touch = NULL;
     }
}

static const struct wl_seat_listener _seat_listener =
{
   _seat_cb_capabilities,
   NULL
};

void
_ecore_wl2_input_add(Ecore_Wl2_Display *display, unsigned int id)
{
   Ecore_Wl2_Input *input;

   input = calloc(1, sizeof(Ecore_Wl2_Input));
   if (!input) return;

   input->display = display;

   input->repeat.rate = 0.025;
   input->repeat.delay = 0.4;
   input->repeat.enabled = EINA_TRUE;

   wl_array_init(&input->data.types);

   /* TODO: setup cursor size and theme */

   input->wl.seat =
     wl_registry_bind(wl_display_get_registry(display->wl.display),
                      id, &wl_seat_interface, 1);

   display->inputs =
     eina_inlist_append(display->inputs, EINA_INLIST_GET(input));

   wl_seat_add_listener(input->wl.seat, &_seat_listener, input);
   wl_seat_set_user_data(input->wl.seat, input);

   if (!display->wl.data_device_manager) return;

   input->data.device =
     wl_data_device_manager_get_data_device(display->wl.data_device_manager,
                                            input->wl.seat);
   wl_data_device_add_listener(input->data.device, &_data_listener, input);
}

void
_ecore_wl2_input_del(Ecore_Wl2_Input *input)
{
   Ecore_Wl2_Display *display;

   if (!input) return;

   display = input->display;

   if (input->repeat.timer) ecore_timer_del(input->repeat.timer);

   if (input->cursor.surface) wl_surface_destroy(input->cursor.surface);
   if (input->cursor.name) eina_stringshare_del(input->cursor.name);
   if (input->cursor.theme) eina_stringshare_del(input->cursor.theme);

   if (input->data.types.data)
     {
        char **t;

        wl_array_for_each(t, &input->data.types)
          free(*t);

        wl_array_release(&input->data.types);
     }

   if (input->data.source) wl_data_source_destroy(input->data.source);
   /* TODO: cleanup dnd */
   /* TODO: cleanup selection */
   if (input->data.device) wl_data_device_destroy(input->data.device);

   if (input->xkb.state) xkb_state_unref(input->xkb.state);
   if (input->xkb.keymap) xkb_map_unref(input->xkb.keymap);

   if (input->wl.seat) wl_seat_destroy(input->wl.seat);

   display->inputs =
     eina_inlist_remove(display->inputs, EINA_INLIST_GET(input));

   free(input);
}
