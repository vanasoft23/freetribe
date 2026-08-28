#ifndef SVC_LOGGER_H
#define SVC_LOGGER_H

#include "ft.h"

void svc_log_init(void);
void svc_log_printf(const char *format, ...);
void svc_log_printf_deferred(const char *format, ...);
bool svc_log_process(void);

#endif