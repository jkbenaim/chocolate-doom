#pragma once

#include <stdarg.h>
#include <stdnoreturn.h>
#include <ulfius.h>

void vwarnulfius(int u_rc, const char *fmt, va_list args);
void noreturn verrulfius(int eval, int u_rc, const char *fmt, va_list args);
void warnulfius(int u_rc, const char *fmt, ...);
void noreturn errulfius(int eval, int u_rc, const char *fmt, ...);

