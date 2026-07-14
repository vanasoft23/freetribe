#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <stdbool.h>
#include <stdint.h>

#define FB_MAX_ITEMS 8
#define FB_NAME_MAX 256
#define FB_ERROR_MAX 32

typedef enum {
    FB_ITEM_FILE,
    FB_ITEM_DIR,
    FB_ITEM_ROOT_DIR,
    FB_ITEM_PARENT_DIR,
    FB_ITEM_BOOT_FLASH,
    FB_ITEM_INSTALL_BOOTLOADER,
    FB_ITEM_DUMP_FLASH
} fb_item_type_t;

typedef struct {
    char           name[FB_NAME_MAX];
    fb_item_type_t type;
    bool           is_dir;
    uint32_t       size;
} fb_item_t;

typedef struct {
    fb_item_t items[FB_MAX_ITEMS];
    uint16_t  count;
    uint16_t  selected;
    uint16_t  scroll;
    char      cdir[FB_NAME_MAX];
    bool      error_active;
    char      error_message[FB_ERROR_MAX];
    bool      sdcard_inserted;
} fb_view_t;

void file_browser_init(void);
void file_browser_tick(void);
const fb_view_t *file_browser_get_view(void);

#endif
