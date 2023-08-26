#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define LCD_PT035TN01

#define LCD_BASE	0x13050000
#define LCD_SIZE    0x00010000

#define BIT(n)	(1ul << (n))

#include "drm_mode.h"
#include "drm_connector.h"
#include "media-bus-format.h"

#define _I	volatile
#define _O	volatile
#define _IO	volatile

typedef struct hw_lcd_t {
    _IO uint32_t LCDCFG;
    _IO uint32_t LCDVSYNC;
    _IO uint32_t LCDHSYNC;
    _IO uint32_t LCDVAT;
    _IO uint32_t LCDDAH;
    _IO uint32_t LCDDAV;

    _IO uint32_t LCDPS;
    _IO uint32_t LCDCLS;
    _IO uint32_t LCDSPL;
    _IO uint32_t LCDREV;
    _IO uint32_t _RESERVED0[2];

    _IO uint32_t LCDCTRL;
    _IO uint32_t LCDSTATE;
    _I  uint32_t LCDIID;
    _IO uint32_t _RESERVED1[1];

    _IO uint32_t LCDDA0;
    _I  uint32_t LCDSA0;
    _I  uint32_t LCDFID0;
    _I  uint32_t LCDCMD0;

    _IO uint32_t LCDDA1;
    _I  uint32_t LCDSA1;
    _I  uint32_t LCDFID1;
    _I  uint32_t LCDCMD1;
} hw_lcd_t;

struct lcd_config_t {
    unsigned clock;
    unsigned hdisplay;
    unsigned hsync_start;
    unsigned hsync_end;
    unsigned htotal;
    unsigned vdisplay;
    unsigned vsync_start;
    unsigned vsync_end;
    unsigned vtotal;
    unsigned flags;
    unsigned bus_format;
    unsigned bus_flags;
};

static const struct lcd_config_t lcd_config = {
#ifdef LCD_PT035TN01
    6035,
    320,
    320 + 10,
    320 + 10 + 1,
    320 + 10 + 1 + 50,
    240,
    240 + 10,
    240 + 10 + 1,
    240 + 10 + 1 + 14,
    DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
    MEDIA_BUS_FMT_RGB888_3X8,
    DRM_BUS_FLAG_DE_HIGH | DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE,
#elif defined(LCD_AT043TN24)
    9000,
    480,
    480 + 2,
    480 + 2 + 41,
    480 + 2 + 41 + 2,
    272,
    272 + 2,
    272 + 2 + 10,
    272 + 2 + 10 + 2,
    DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
    MEDIA_BUS_FMT_RGB666_1X18,
    DRM_BUS_FLAG_DE_HIGH | DRM_BUS_FLAG_PIXDATA_DRIVE_POSEDGE,
#endif
};

int np1000_lcd_fix(void)
{
    // Open /dev/mem
    int mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem == -1) {
        perror("open");
        return 1;
    }

    // Map APB block from LCD_BASE
    void *map = mmap(0, LCD_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem, LCD_BASE);
    if (map == (void *)-1) {
        perror("mmap");
        return 1;
    }

    struct hw_lcd_t *lcd = (struct hw_lcd_t *)map;
	lcd->LCDVSYNC = lcd_config.vsync_end - lcd_config.vsync_start;
	lcd->LCDHSYNC = lcd_config.hsync_end - lcd_config.hsync_start;
	lcd->LCDVAT = (lcd_config.htotal << 16) | lcd_config.vtotal;
	lcd->LCDDAH = ((lcd_config.htotal - lcd_config.hsync_start) << 16) |
		      (lcd_config.htotal - (lcd_config.hsync_start - lcd_config.hdisplay));
	lcd->LCDDAV = ((lcd_config.vtotal - lcd_config.vsync_start) << 16) |
		      (lcd_config.vtotal - (lcd_config.vsync_start - lcd_config.vdisplay));

    return 0;
}
