#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EFL_ACCESS_BETA
#include <Elementary.h>
#include "elm_suite.h"

START_TEST (elm_atspi_role_get)
{
   Evas_Object *win, *dayselector;
   Efl_Access_Role role;

   elm_init(1, NULL);
   win = elm_win_add(NULL, "dayselector", ELM_WIN_BASIC);

   dayselector = elm_dayselector_add(win);
   role = efl_access_role_get(dayselector);

   ck_assert(role == EFL_ACCESS_ROLE_PANEL);

   elm_shutdown();
}
END_TEST

void elm_test_dayselector(TCase *tc)
{
 tcase_add_test(tc, elm_atspi_role_get);
}

