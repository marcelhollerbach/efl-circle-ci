#ifndef EOLIAN_PRIV_H
#define EOLIAN_PRIV_H

#include <Eina.h>
#include <stdio.h>
#include <stdarg.h>

static inline void _eolian_log_line(const char *file, int line, int column, const char *fmt, ...) EINA_ARG_NONNULL(1, 4) EINA_PRINTF(4, 5);
static inline void _eolian_log(const char *fmt, ...) EINA_ARG_NONNULL(1) EINA_PRINTF(1, 2);

static inline void
_eolian_log_line(const char *file, int line, int column, const char *fmt, ...)
{
   Eina_Strbuf *sb = eina_strbuf_new();
   va_list args;

   va_start(args, fmt);
   eina_strbuf_append_vprintf(sb, fmt, args);
   va_end(args);

   if (!eina_log_color_disable_get())
     {
        fprintf(stderr, EINA_COLOR_RED "eolian" EINA_COLOR_RESET ": "
                EINA_COLOR_WHITE "%s" EINA_COLOR_RESET ":%d:%d: "
                EINA_COLOR_ORANGE "%s\n" EINA_COLOR_RESET,
                file, line, column, eina_strbuf_string_get(sb));
     }
   else
     {
        fprintf(stderr, "eolian: %s:%d:%d: %s\n", file, line, column,
                eina_strbuf_string_get(sb));
     }
   eina_strbuf_free(sb);
}

static inline void
_eolian_log(const char *fmt, ...)
{
   Eina_Strbuf *sb = eina_strbuf_new();
   va_list args;

   va_start(args, fmt);
   eina_strbuf_append_vprintf(sb, fmt, args);
   va_end(args);

   if (!eina_log_color_disable_get())
     {
        fprintf(stderr, EINA_COLOR_RED "eolian" EINA_COLOR_RESET ": "
                EINA_COLOR_ORANGE "%s\n" EINA_COLOR_RESET,
                eina_strbuf_string_get(sb));
     }
   else
     {
        fprintf(stderr, "eolian: %s\n", eina_strbuf_string_get(sb));
     }
   eina_strbuf_free(sb);
}

#endif

