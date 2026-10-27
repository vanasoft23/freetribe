#ifndef FT_LOG_H
#define FT_LOG_H

#ifndef BLACKFIN

extern void (print_semihosting)(const char *format, ...);

// One argument is literal text; additional arguments enable formatting.
#define LOG(str, ...)                                      \
    (sizeof(#__VA_ARGS__) == 1                              \
        ? (print_semihosting)("%s", (str))                   \
        : (print_semihosting)((str), ##__VA_ARGS__))

/* Keep existing direct calls using the same argument handling as LOG. */
#define print_semihosting(...) LOG(__VA_ARGS__)

#ifdef DEBUG
  #define DLOG(...)             LOG(__VA_ARGS__)
#else
  #define DLOG(...)             ((void)0)
#endif

#endif /* BLACKFIN */

/// @TODO: DSP should also get a logging interface that just pipes thru to CPU

#endif
