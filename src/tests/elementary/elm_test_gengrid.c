#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EFL_ACCESS_BETA
#include <Elementary.h>
#include "elm_suite.h"
#include "elm_test_helper.h"


START_TEST (elm_atspi_role_get)
{
   Evas_Object *win, *gengrid;
   Efl_Access_Role role;

   elm_init(1, NULL);
   win = elm_win_add(NULL, "gengrid", ELM_WIN_BASIC);

   gengrid = elm_gengrid_add(win);
   role = efl_access_role_get(gengrid);

   ck_assert(role == EFL_ACCESS_ROLE_TREE_TABLE);

   elm_shutdown();
}
END_TEST

// Temporary commnted since gengrid fields_update function do not call content callbacks
// (different behaviour then genlist - which calls)
#if 0
static Evas_Object *content;

static Evas_Object *
gl_content_get(void *data EINA_UNUSED, Evas_Object *obj, const char *part EINA_UNUSED)
{
   content = elm_button_add(obj);
   evas_object_show(content);
   return content;
}

/**
 * Validate if gengrid implementation properly reset AT-SPI parent to Elm_Gengrid_Item
 * from Elm_Gengrid
 */
START_TEST(elm_atspi_children_parent)
{
   elm_init(1, NULL);
   elm_config_atspi_mode_set(EINA_TRUE);
   static Elm_Gengrid_Item_Class itc;

   Evas_Object *win = elm_win_add(NULL, "gengrid", ELM_WIN_BASIC);
   evas_object_resize(win, 100, 100);
   Evas_Object *gengrid = elm_gengrid_add(win);
   evas_object_resize(gengrid, 100, 100);

   Efl_Access *parent;
   content = NULL;

   itc.item_style = "default";
   itc.func.content_get = gl_content_get;

   evas_object_show(win);
   evas_object_show(gengrid);

   Elm_Object_Item *it = elm_gengrid_item_append(gengrid, &itc, NULL, NULL, NULL);
   elm_gengrid_item_fields_update(it, "*.", ELM_GENGRID_ITEM_FIELD_CONTENT);

   ck_assert(content != NULL);
   parent = efl_access_parent_get(content);
   ck_assert(it == parent);

   elm_shutdown();
}
END_TEST
#endif

void elm_test_gengrid(TCase *tc)
{
   tcase_add_test(tc, elm_atspi_role_get);
#if 0
   tcase_add_test(tc, elm_atspi_children_parent);
#endif
}
