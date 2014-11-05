#ifdef HAVE_CONFIG
# include "config.h"
#endif

#include <Elementary.h>

#include "elm_code_widget.h"

#include "elm_code_private.h"

EAPI void elm_code_widget_fill(Evas_Object *o, Elm_Code *code)
{
   Elm_Code_Line *line;
   Evas_Textgrid_Cell *cells;
   const char *content, *chr;
   unsigned int length;
   int w, h, cw, ch;
   unsigned int x, y;

   evas_object_geometry_get(o, NULL, NULL, &w, &h);
   evas_object_textgrid_cell_size_get(o, &cw, &ch);
   evas_object_textgrid_size_set(o, ceil(((double) w) / cw),
         ceil(((double) h) / ch));
   evas_object_textgrid_size_get(o, &w, &h);

   for (y = 1; y <= elm_code_file_lines_get(code->file); y++)
     {
        line = elm_code_file_line_get(code->file, y);
        content = elm_code_file_line_content_get(code->file, y);
        chr = content;

        cells = evas_object_textgrid_cellrow_get(o, y - 1);
        length = strlen(content);

        for (x = 0; x < (unsigned int) w && x < length; x++)
          {
             cells[x].codepoint = *chr;
             cells[x].bg = line->status;
             cells[x].fg = ELM_CODE_TOKEN_TYPE_DEFAULT;

             chr++;
          }
     }
}

static void
_elm_code_widget_resize_cb(void *data, EINA_UNUSED Evas *e, Evas_Object *obj,
                           EINA_UNUSED void *event_info)
{
   Elm_Code *code;

   code = (Elm_Code *)data;
   elm_code_widget_fill(obj, code);
}

EAPI Evas_Object *elm_code_widget_add(Evas_Object *parent, Elm_Code *code)
{
   Evas_Object *o;

   o = evas_object_textgrid_add(parent);

   evas_object_textgrid_font_set(o, "Mono", 10 * elm_config_scale_get());

   // setup status colors
   evas_object_textgrid_palette_set(o, EVAS_TEXTGRID_PALETTE_STANDARD, ELM_CODE_STATUS_TYPE_DEFAULT,
                                    54, 54, 54, 255); 
   evas_object_textgrid_palette_set(o, EVAS_TEXTGRID_PALETTE_STANDARD, ELM_CODE_STATUS_TYPE_ERROR,
                                    205, 54, 54, 255); 

   // setup token colors
   evas_object_textgrid_palette_set(o, EVAS_TEXTGRID_PALETTE_STANDARD, ELM_CODE_TOKEN_TYPE_DEFAULT,
                                    205, 205, 205, 255);

   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE, _elm_code_widget_resize_cb, code);
   return o;
}

