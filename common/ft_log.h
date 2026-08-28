#ifndef FT_LOG_H
#define FT_LOG_H

#ifndef BLACKFIN

// Can't include svc_logger.h
void svc_log_printf(const char *format, ...);

#define LOG(...)              svc_log_printf(__VA_ARGS__)

#ifdef DEBUG
  #define DLOG(...)             LOG(__VA_ARGS__)
#else
  #define DLOG(...)             ((void)0)
#endif

#endif /* BLACKFIN */

/// @TODO: DSP should also get a logging interface that just pipes thru to CPU



#endif