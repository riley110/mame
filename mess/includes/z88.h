#include "driver.h"
#include "osdepend.h"
#define Z88_NUM_COLOURS 2

#define Z88_SCREEN_WIDTH        640
#define Z88_SCREEN_HEIGHT       64

int z88_vh_start(void);
void z88_vh_stop(void);
void z88_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
void z88_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable, const unsigned char *color_prom);

struct blink_hw
{
	int pb[4];
	int sbr;

        /* screen */
        int sbf;
        int lores0;
        int lores1;
        int hires0;
        int hires1;

	int com;
	int ints;
	int sta;
	int ack;
	int tack;
	int tmk;
	int mem[4];
	int tim[5];
	int tsta;
};


#define Z88_SCR_HW_REV  (1<<4)
#define Z88_SCR_HW_HRS  (1<<5)
#define Z88_SCR_HW_UND  (1<<1)
#define Z88_SCR_HW_FLS  (1<<3)
#define Z88_SCR_HW_GRY  (1<<2)
