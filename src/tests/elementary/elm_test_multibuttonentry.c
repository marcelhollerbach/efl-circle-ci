#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EFL_ACCESS_BETA
#include <Elementary.h>
#include "elm_suite.h"


START_TEST (elm_atspi_role_get)
{
   Evas_Object *win, *multibuttonentry;
   Efl_Access_Role role;

   elm_init(1, NULL);
   win = elm_win_add(NULL, "multibuttonentry", ELM_WIN_BASIC);

   multibuttonentry = elm_multibuttonentry_add(win);
   role = efl_access_role_get(multibuttonentry);

   ck_assert_int_eq(role, EFL_ACCESS_ROLE_PANEL);

   elm_shutdown();
}
END_TEST

void elm_test_multibuttonentry(TCase *tc)
{
 tcase_add_test(tc, elm_atspi_role_get);
}
