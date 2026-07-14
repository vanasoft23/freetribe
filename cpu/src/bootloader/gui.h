#ifndef GUI_H
#define GUI_H

#include <stdint.h>

void gui_init(void);
void gui_tick(void);
void gui_show_message_box(const char *title, const char *message);
void gui_boot_effect_begin(void);
void gui_boot_effect_step(uint32_t done, uint32_t total);

#endif
