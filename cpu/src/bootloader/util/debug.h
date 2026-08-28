#ifndef DEBUG_H
#define DEBUG_H

///@TODO: debug.h/.c is a vague name and should probably be merged with something else

#define SH_SYS_WRITE0  0x04  // write null-terminated string
#define SH_SYS_WRITEC  0x03  // write single character

/**
 * @brief   Print debug string via semihosting.
 *          Will show up in your GDB console.
 */
void debug_print_sh(const char *str);


#endif /* DEBUG_H */