#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define ELM_LAYOUT_PROTECTED

#include <Elementary.h>
#include "elm_priv.h"

#include "elm_widget_layout.h"
#include "efl_ui_textpath_part.eo.h"
#include "elm_part_helper.h"


#define MY_CLASS EFL_UI_TEXTPATH_CLASS
#define MY_CLASS_NAME "Efl.Ui.Textpath"

#define SLICE_DEFAULT_NO 99

typedef struct _Efl_Ui_Textpath_Point Efl_Ui_Textpath_Point;
typedef struct _Efl_Ui_Textpath_Line Efl_Ui_Textpath_Line;
typedef struct _Efl_Ui_Textpath_Segment Efl_Ui_Textpath_Segment;
typedef struct _Efl_Ui_Textpath_Data Efl_Ui_Textpath_Data;

struct _Efl_Ui_Textpath_Point
{
   double x;
   double y;
};

struct _Efl_Ui_Textpath_Line
{
   Efl_Ui_Textpath_Point start;
   Efl_Ui_Textpath_Point end;
};

struct _Efl_Ui_Textpath_Segment
{
   EINA_INLIST;
   int length;
   Efl_Gfx_Path_Command_Type type;
   union
     {
        Eina_Bezier bezier;
        Efl_Ui_Textpath_Line line;
     };
};

struct _Efl_Ui_Textpath_Data
{
   Evas_Object *text_obj;
   char *text;
   Efl_Gfx_Path *path;
   struct {
        double x, y;
        double radius;
        double start_angle;
   } circle;
   Efl_Ui_Textpath_Direction direction;
   int slice_no;
   Eina_Bool autofit;
   Eina_Bool ellipsis;

   Eina_Inlist *segments;
   int total_length;
};

#define EFL_UI_TEXTPATH_DATA_GET(o, sd) \
   Efl_Ui_Textpath_Data *sd = efl_data_scope_get(o, EFL_UI_TEXTPATH_CLASS)

static inline double
_deg_to_rad(double angle)
{
   return angle / 180 * M_PI;
}

static void
_segment_draw(Efl_Ui_Textpath_Data *pd, int slice_no, int w1, int w2, int cmp, Evas_Map *map, Eina_Bezier bezier)
{
   int i, len, seg_len;
   double u0, u1, v0, v1;
   double dist, t, dt;
   double px, py, px2, py2;
   double rad;
   Eina_Rect r;
   Eina_Vector2 vec, nvec, vec0, vec1, vec2, vec3;
   Eina_Matrix2 mat;

   len = w2 - w1;
   r = efl_gfx_geometry_get(pd->text_obj);

   seg_len = eina_bezier_length_get(&bezier);
   if (pd->autofit)
     dt = len / (seg_len * (double) slice_no);
   else
     dt = 1.0 / (double) slice_no;
   dist = len / (double)slice_no;
   rad = _deg_to_rad(90);
   eina_matrix2_values_set(&mat, cos(rad), -sin(rad), sin(rad), cos(rad));

   eina_bezier_values_get(&bezier, NULL, NULL, NULL, NULL, NULL, NULL, &px2, &py2);
   t = 0;
   eina_bezier_point_at(&bezier, t, &px, &py);
   eina_bezier_point_at(&bezier, t + dt, &px2, &py2);

   vec.x = (px2 - px);
   vec.y = (py2 - py);
   eina_vector2_normalize(&nvec, &vec);

   eina_vector2_transform(&vec, &mat, &nvec);
   eina_vector2_normalize(&nvec, &vec);
   eina_vector2_scale(&vec, &nvec, ((double) r.h) * 0.5);

   vec1.x = (vec.x + px);
   vec1.y = (vec.y + py);
   vec2.x = (-vec.x + px);
   vec2.y = (-vec.y + py);

   //add points to map
   for (i = 0; i < slice_no; i++)
     {
        //v0, v3
        vec0.x = vec1.x;
        vec0.y = vec1.y;
        vec3.x = vec2.x;
        vec3.y = vec2.y;

        //v1, v2
        t = ((double) (i + 1) * dt);
        eina_bezier_point_at(&bezier, t, &px, &py);
        eina_bezier_point_at(&bezier, t + dt, &px2, &py2);

        vec.x = (px2 - px);
        vec.y = (py2 - py);
        eina_vector2_normalize(&nvec, &vec);
        eina_vector2_transform(&vec, &mat, &nvec);
        eina_vector2_normalize(&nvec, &vec);
        eina_vector2_scale(&vec, &nvec, ((double) r.h) * 0.5);

        vec1.x = (vec.x + px);
        vec1.y = (vec.y + py);
        vec2.x = (-vec.x + px);
        vec2.y = (-vec.y + py);

        evas_map_point_coord_set(map, cmp + i * 4, (int) vec0.x + r.x, (int) vec0.y + r.y, 0);
        evas_map_point_coord_set(map, cmp + i * 4 + 1, (int) vec1.x + r.x, (int) vec1.y + r.y, 0);
        evas_map_point_coord_set(map, cmp + i * 4 + 2, (int) vec2.x + r.x, (int) vec2.y + r.y, 0);
        evas_map_point_coord_set(map, cmp + i * 4 + 3, (int) vec3.x + r.x, (int) vec3.y + r.y, 0);

        //UV
        u0 = w1 + i * dist;
        u1 = u0 + dist;
        v0 = (double) 0;
        v1 = (double) r.h;

        evas_map_point_image_uv_set(map, cmp + i * 4, u0, v0);
        evas_map_point_image_uv_set(map, cmp + i * 4 + 1, u1, v0);
        evas_map_point_image_uv_set(map, cmp + i * 4 + 2, u1, v1);
        evas_map_point_image_uv_set(map, cmp + i * 4 + 3, u0, v1);
     }
}

static void
_text_on_line_draw(Efl_Ui_Textpath_Data *pd, int w1, int w2, int cmp, Evas_Map *map, Efl_Ui_Textpath_Line line)
{
   double x1, x2, y1, y2;
   double line_len, len, sina, cosa;
   Eina_Rect r;

   x1 = line.start.x;
   y1 = line.start.y;
   x2 = line.end.x;
   y2 = line.end.y;

   line_len = sqrt((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1));
   len = w2 - w1;
   if (line_len > len)
     {
        x2 = x1 + len * (x2 - x1) / line_len;
        y2 = y1 + len * (y2 - y1) / line_len;
     }

   len = sqrt((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1));
   sina = (y2 - y1) / len;
   cosa = (x2 - x1) / len;

   r = efl_gfx_geometry_get(pd->text_obj);
   r.h /= 2;
   evas_map_point_coord_set(map, cmp + 3, x1 - r.h * sina, y1 + r.h * cosa, 0);
   evas_map_point_coord_set(map, cmp + 2, x2 - r.h * sina, y2 + r.h * cosa, 0);
   evas_map_point_coord_set(map, cmp + 1, x2 + r.h * sina, y2 - r.h * cosa, 0);
   evas_map_point_coord_set(map, cmp + 0, x1 + r.h * sina, y1 - r.h * cosa, 0);

   r.h *= 2;
   evas_map_point_image_uv_set(map, cmp + 0, w1, 0);
   evas_map_point_image_uv_set(map, cmp + 1, w2, 0);
   evas_map_point_image_uv_set(map, cmp + 2, w2, r.h);
   evas_map_point_image_uv_set(map, cmp + 3, w1, r.h);
}

static int
_map_point_calc(Efl_Ui_Textpath_Data *pd)
{
   int map_no = 0;
   Efl_Ui_Textpath_Segment *seg;

   EINA_INLIST_FOREACH(pd->segments, seg)
     {
        if (seg->type == EFL_GFX_PATH_COMMAND_TYPE_LINE_TO)
          {
             map_no++;
          }
        else if (seg->type == EFL_GFX_PATH_COMMAND_TYPE_CUBIC_TO)
          {
             int no = pd->slice_no * seg->length / (double)pd->total_length;
             if (no == 0) no = 1;
             map_no += no;
          }
     }
   map_no *= 4;

   return map_no;
}

static void
_text_draw(Efl_Ui_Textpath_Data *pd)
{
   Efl_Ui_Textpath_Segment *seg;
   Evas_Map *map;
   double slice_unit;
   int w1, w2;
   int remained_w;
   int drawn_slice = 0;
   int cur_map_point = 0, map_point_no;
   Eina_Size2D sz;

   sz = efl_gfx_size_get(pd->text_obj);
   if (pd->autofit)
     remained_w = sz.w;
   else
     remained_w = pd->total_length;
   slice_unit = (double)pd->slice_no / pd->total_length;

   map_point_no = _map_point_calc(pd);
   if (map_point_no == 0)
     {
        evas_object_map_enable_set(pd->text_obj, EINA_FALSE);
        return;
     }
   map = evas_map_new(map_point_no);

   w1 = w2 = 0;
   EINA_INLIST_FOREACH(pd->segments, seg)
     {
        int len = seg->length;
        if (!pd->autofit)
          len = (double)seg->length * sz.w / (double)pd->total_length;
        if (remained_w <= 0)
          break;
        w2 = w1 + len;
        if (w2 > sz.w)
          w2 = sz.w;
        if (seg->type == EFL_GFX_PATH_COMMAND_TYPE_LINE_TO)
          {
             drawn_slice += 1;
             _text_on_line_draw(pd, w1, w2, cur_map_point, map, seg->line);
             cur_map_point += 4;
          }
        else
          {
             int slice_no;
             slice_no = pd->slice_no * seg->length / (double)pd->total_length;
             if (slice_no == 0)
               slice_no = len * slice_unit + 1;
             drawn_slice += slice_no;
             _segment_draw(pd, slice_no, w1, w2, cur_map_point, map, seg->bezier);
             cur_map_point += slice_no * 4;
          }
        w1 = w2;
        remained_w -= len;
     }
   evas_object_map_enable_set(pd->text_obj, EINA_TRUE);
   evas_object_map_set(pd->text_obj, map);
   evas_map_free(map);
}

static void
_path_data_get(Eo *obj, Efl_Ui_Textpath_Data *pd, Eina_Bool set_min)
{
   const Efl_Gfx_Path_Command_Type *cmd;
   const double *points;
   Efl_Ui_Textpath_Segment *seg;
   Eina_Position2D opos;

   EINA_INLIST_FREE(pd->segments, seg)
     {
        pd->segments = eina_inlist_remove(pd->segments, EINA_INLIST_GET(seg));
        free(seg);
     }

   opos = efl_gfx_position_get(obj);

   pd->total_length = 0;
   efl_gfx_path_get(obj, &cmd, &points);
   if (cmd)
     {
        int pos = -1;
        double px0 = 0.0, py0 = 0.0, ctrl_x0, ctrl_y0, ctrl_x1, ctrl_y1, px1, py1;
        Eina_Rect rect = EINA_RECT_ZERO();

        while (*cmd != EFL_GFX_PATH_COMMAND_TYPE_END)
          {
             if (*cmd == EFL_GFX_PATH_COMMAND_TYPE_MOVE_TO)
               {
                  pos++;
                  px0 = points[pos] + opos.x;
                  pos++;
                  py0 = points[pos] + opos.y;
               }
             else if (*cmd == EFL_GFX_PATH_COMMAND_TYPE_CUBIC_TO)
               {
                  Eina_Bezier bz;
                  double bx, by, bw, bh;
                  Eina_Rect brect;

                  pos++;
                  ctrl_x0 = points[pos] + opos.x;
                  pos++;
                  ctrl_y0 = points[pos] + opos.y;
                  pos++;
                  ctrl_x1 = points[pos] + opos.x;
                  pos++;
                  ctrl_y1 = points[pos] + opos.y;
                  pos++;
                  px1 = points[pos] + opos.x;
                  pos++;
                  py1 = points[pos] + opos.y;

                  eina_bezier_values_set(&bz, px0, py0, ctrl_x0, ctrl_y0, ctrl_x1, ctrl_y1, px1, py1);
                  seg = malloc(sizeof(Efl_Ui_Textpath_Segment));
                  if (!seg)
                    {
                       ERR("Failed to allocate segment");
                       px0 = px1;
                       py0 = py1;
                       continue;
                    }
                  seg->length = eina_bezier_length_get(&bz);
                  seg->bezier = bz;
                  seg->type = EFL_GFX_PATH_COMMAND_TYPE_CUBIC_TO;
                  pd->segments = eina_inlist_append(pd->segments, EINA_INLIST_GET(seg));
                  pd->total_length += seg->length;

                  //move points
                  px0 = px1;
                  py0 = py1;

                  eina_bezier_bounds_get(&bz, &bx, &by, &bw, &bh);
                  brect = EINA_RECT(bx, by, bw, bh);
                  eina_rectangle_union(&rect.rect, &brect.rect);
               }
             else if (*cmd == EFL_GFX_PATH_COMMAND_TYPE_LINE_TO)
               {
                  Eina_Rect lrect;

                  pos++;
                  px1 = points[pos] + opos.x;
                  pos++;
                  py1 = points[pos] + opos.y;

                  seg = malloc(sizeof(Efl_Ui_Textpath_Segment));
                  if (!seg)
                    {
                       ERR("Failed to allocate segment");
                       px0 = px1;
                       py0 = py1;
                       continue;
                    }
                  seg->type = EFL_GFX_PATH_COMMAND_TYPE_LINE_TO;
                  seg->line.start.x = px0;
                  seg->line.start.y = py0;
                  seg->line.end.x = px1;
                  seg->line.end.y = py1;
                  seg->length = sqrt((px1 - px0)*(px1 - px0) + (py1 - py0)*(py1 - py0));
                  pd->segments = eina_inlist_append(pd->segments, EINA_INLIST_GET(seg));
                  pd->total_length += seg->length;

                  lrect = EINA_RECT(px0, py0, px1 - px0, py1 - py0);
                  eina_rectangle_union(&rect.rect, &lrect.rect);
               }
             cmd++;
          }
        if (set_min)
          {
             efl_gfx_size_hint_min_set(obj, rect.size);
          }
     }
}

static void
_sizing_eval(Efl_Ui_Textpath_Data *pd)
{
   _text_draw(pd);
}

static void
_textpath_ellipsis_set(Efl_Ui_Textpath_Data *pd, Eina_Bool enabled)
{
   edje_object_part_text_style_user_pop(pd->text_obj, "elm.text");

   if (enabled)
     edje_object_part_text_style_user_push(pd->text_obj, "elm.text",
                                           "DEFAULT='ellipsis=1.0'");
}

static void
_ellipsis_set(Efl_Ui_Textpath_Data *pd)
{
   if (!pd->text_obj) return;

   Evas_Coord w = 0, h = 0;
   Eina_Bool is_ellipsis = EINA_FALSE;
   const Evas_Object *tb;

   tb = edje_object_part_object_get(pd->text_obj, "elm.text");
   evas_object_textblock_size_native_get(tb, &w, &h);
   evas_object_size_hint_min_set(pd->text_obj, w, h);
   if (pd->ellipsis)
     {
        if (w > pd->total_length)
          {
             is_ellipsis = EINA_TRUE;
             w = pd->total_length;
          }
     }
   efl_gfx_size_set(pd->text_obj, EINA_SIZE2D(w,  h));
   _textpath_ellipsis_set(pd, is_ellipsis);
}

static void
_path_changed_cb(void *data, const Efl_Event *event EINA_UNUSED)
{
   EFL_UI_TEXTPATH_DATA_GET(data, sd);

   _path_data_get(data, sd, EINA_TRUE);
   _sizing_eval(sd);
}

static Eina_Bool
_textpath_text_set_internal(Eo *obj, Efl_Ui_Textpath_Data *pd, const char *part, const char *text)
{
   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd, EINA_FALSE);
   Eina_Bool ret = EINA_TRUE;

   if (!text) text = "";
   ret = edje_object_part_text_set(pd->text_obj, part, text);
   _ellipsis_set(pd);

   return ret;
}

EOLIAN static void
_efl_ui_textpath_efl_canvas_group_group_calculate(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd)
{
   _sizing_eval(pd);
}

EOLIAN static void
_efl_ui_textpath_efl_canvas_group_group_add(Eo *obj, Efl_Ui_Textpath_Data *priv)
{
   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd);

   efl_canvas_group_add(efl_super(obj, MY_CLASS));
   elm_widget_sub_object_parent_add(obj);

   priv->text_obj = edje_object_add(evas_object_evas_get(obj));
   elm_widget_theme_object_set(obj, priv->text_obj, "textpath", "base",
                               elm_widget_style_get(obj));
   efl_gfx_size_hint_weight_set(priv->text_obj, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   efl_gfx_size_hint_align_set(priv->text_obj, EVAS_HINT_FILL, EVAS_HINT_FILL);
   efl_gfx_visible_set(priv->text_obj, EINA_TRUE);

   evas_object_smart_member_add(priv->text_obj, obj);
   elm_widget_sub_object_add(obj, priv->text_obj);

   efl_event_callback_add(obj, EFL_GFX_PATH_EVENT_CHANGED, _path_changed_cb, obj);
}

EOLIAN static Efl_Object *
_efl_ui_textpath_efl_object_constructor(Eo *obj, Efl_Ui_Textpath_Data *pd)
{
   obj = efl_constructor(efl_super(obj, MY_CLASS));
   pd->autofit = EINA_TRUE;
   pd->slice_no = SLICE_DEFAULT_NO;
   pd->direction = EFL_UI_TEXTPATH_DIRECTION_CW;

   return obj;
}

EOLIAN static void
_efl_ui_textpath_efl_object_destructor(Eo *obj, Efl_Ui_Textpath_Data *pd)
{
   Efl_Ui_Textpath_Segment *seg;

   if (pd->text) free(pd->text);
   if (pd->text_obj) evas_object_del(pd->text_obj);
   EINA_INLIST_FREE(pd->segments, seg)
     {
        pd->segments = eina_inlist_remove(pd->segments, EINA_INLIST_GET(seg));
        free(seg);
     }

   efl_destructor(efl_super(obj, MY_CLASS));
}

EOLIAN static Eina_Bool
_efl_ui_textpath_text_set(Eo *obj, Efl_Ui_Textpath_Data *pd, const char *part, const char *text)
{
   return _textpath_text_set_internal(obj, pd, part, text);
}

EOLIAN static const char *
_efl_ui_textpath_text_get(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd, const char *part)
{
   return edje_object_part_text_get(pd->text_obj, part);
}

EOLIAN static void
_efl_ui_textpath_efl_text_text_set(Eo *obj, Efl_Ui_Textpath_Data *pd, const char *text)
{
   _textpath_text_set_internal(obj, pd, "elm.text", text);
}

EOLIAN static const char *
_efl_ui_textpath_efl_text_text_get(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd)
{
   return edje_object_part_text_get(pd->text_obj, "elm.text");
}

EOLIAN static Efl_Ui_Theme_Apply
_efl_ui_textpath_elm_widget_theme_apply(Eo *obj, Efl_Ui_Textpath_Data *pd)
{
   Efl_Ui_Theme_Apply ret = EFL_UI_THEME_APPLY_FAILED;

   ret = efl_ui_widget_theme_apply(efl_super(obj, MY_CLASS));
   if (!ret) return EFL_UI_THEME_APPLY_FAILED;

   elm_widget_theme_object_set(obj, pd->text_obj, "textpath", "base",
                               elm_widget_style_get(obj));
   _ellipsis_set(pd);

   return ret;
}

EOLIAN static void
_efl_ui_textpath_efl_gfx_position_set(Eo *obj, Efl_Ui_Textpath_Data *pd, Eina_Position2D pos)
{
   efl_gfx_position_set(efl_super(obj, MY_CLASS), pos);
   _path_data_get(obj, pd, EINA_FALSE);
   _text_draw(pd);
}

EOLIAN static void
_efl_ui_textpath_efl_gfx_size_set(Eo *obj, Efl_Ui_Textpath_Data *pd EINA_UNUSED, Eina_Size2D sz)
{
   efl_gfx_size_set(efl_super(obj, MY_CLASS), sz);
}

EOLIAN static void
_efl_ui_textpath_circle_set(Eo *obj, Efl_Ui_Textpath_Data *pd, double x, double y, double radius, double start_angle, Efl_Ui_Textpath_Direction direction)
{
   if (pd->circle.x == x && pd->circle.y == y &&
       pd->circle.radius == radius &&
       pd->circle.start_angle == start_angle &&
       pd->direction == direction &&
       _map_point_calc(pd) > 0)
     {
        ERR("Same circle");
        return;
     }
   pd->circle.x = x;
   pd->circle.y = y;
   pd->circle.radius = radius;
   pd->circle.start_angle = start_angle;
   pd->direction = direction;

   efl_gfx_path_reset(obj);
   if (direction == EFL_UI_TEXTPATH_DIRECTION_CW)
     {
        efl_gfx_path_append_arc(obj, x - radius, y - radius, radius * 2,
                                radius * 2,  start_angle, -360);
     }
   else
     {
        efl_gfx_path_append_arc(obj, x - radius, y - radius, radius * 2,
                                radius * 2,  start_angle, 360);
     }

   _sizing_eval(pd);
}

EOLIAN static Eina_Bool
_efl_ui_textpath_autofit_get(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd)
{
   return pd->autofit;
}

EOLIAN static void
_efl_ui_textpath_autofit_set(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd, Eina_Bool autofit)
{
   if (pd->autofit == autofit) return;
   pd->autofit = autofit;
   _sizing_eval(pd);
}

EOLIAN static int
_efl_ui_textpath_slice_number_get(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd)
{
   return pd->slice_no;
}

EOLIAN static void
_efl_ui_textpath_slice_number_set(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd, int slice_no)
{
   if (pd->slice_no == slice_no) return;
   pd->slice_no = slice_no;
   _sizing_eval(pd);
}

EOLIAN static void
_efl_ui_textpath_ellipsis_set(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd, Eina_Bool ellipsis)
{
   if (pd->ellipsis == ellipsis) return;
   pd->ellipsis = ellipsis;

   _ellipsis_set(pd);
   _sizing_eval(pd);
}

EOLIAN static Eina_Bool
_efl_ui_textpath_ellipsis_get(Eo *obj EINA_UNUSED, Efl_Ui_Textpath_Data *pd)
{
   return pd->ellipsis;
}

/* Efl.Part begin */
ELM_PART_OVERRIDE(efl_ui_textpath, EFL_UI_TEXTPATH, Efl_Ui_Textpath_Data)
ELM_PART_OVERRIDE_TEXT_SET(efl_ui_textpath, EFL_UI_TEXTPATH, Efl_Ui_Textpath_Data)
ELM_PART_OVERRIDE_TEXT_GET(efl_ui_textpath, EFL_UI_TEXTPATH, Efl_Ui_Textpath_Data)
#include "efl_ui_textpath_part.eo.c"
/* Efl.Part end */

#define EFL_UI_TEXTPATH_EXTRA_OPS \
      EFL_CANVAS_GROUP_ADD_OPS(efl_ui_textpath)

#include "efl_ui_textpath.eo.c"
