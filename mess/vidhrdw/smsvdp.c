/*
		For more information, please see:
		http://www.emucamp.com/cgfm2/smsvdp.txt
*/

#include "driver.h"
#include "includes/sms.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

UINT8 reg[NUM_OF_REGISTER];
UINT8 ggCRAM[GG_CRAM_SIZE];
UINT8 smsCRAM[SMS_CRAM_SIZE];
UINT8 VRAM[VRAM_SIZE];
UINT8 *lineCollisionBuffer;
UINT8 *spriteCache;

int addr;
int code;
int pending;
int latch;
int buffer;
int statusReg;

int isCRAMDirty;
UINT8 isGGCRAMDirty[GG_CRAM_SIZE];
UINT8 isSMSCRAMDirty[SMS_CRAM_SIZE];

int currentLine;
int lineCountDownCounter;
int irqState;			/* The status of the IRQ line, as seen by the VDP */

struct mame_bitmap *prevBitMap;
int prevBitMapSaved;

/* NTSC 192 lines precalculated return values from the V counter */
static UINT8 vcnt_ntsc_192[NTSC_Y_PIXELS] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
																0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* NTSC 224 lines precalculated return values from the V counter */
static UINT8 vcnt_ntsc_224[NTSC_Y_PIXELS] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
																0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* NTSC 240 lines precalculated return values from the V counter */
static UINT8 vcnt_ntsc_240[NTSC_Y_PIXELS] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05
};

/* PAL 192 lines precalculated return values from the V counter */
static UINT8 vcnt_pal_192[PAL_Y_PIXELS] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2,																						0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* PAL 224 lines precalculated return values from the V counter */
static UINT8 vcnt_pal_224[PAL_Y_PIXELS] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
	0x00, 0x01, 0x02,																						0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* PAL 240 lines precalculated return values from the V counter */
static UINT8 vcnt_pal_240[PAL_Y_PIXELS] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
							0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

READ_HANDLER(sms_vdp_curline_r) {
	/* Is it NTSC */
	if (IS_NTSC) {
		/* must be mode 4 */
		if (reg[0x00] & 0x04) {
			if (IS_GG_ANY) {
				if (reg[0x00] & 0x02) {
					/* Is it 224-line display */
					if ((reg[0x01] & 0x10) && !(reg[0x01] & 0x08)) {
						return vcnt_ntsc_224[currentLine];
					} else if (!(reg[0x01] & 0x10) && (reg[0x01] & 0x08)) {
						/* 240-line display */
						return vcnt_ntsc_240[currentLine];
					}
				}
			}
		}
		/* 192-line display */
		return vcnt_ntsc_192[currentLine];
	} else {
		/* It must be PAL */
		if (reg[0x00] & 0x04) {
			if (IS_GG_ANY) {
				if (reg[0x00] & 0x02) {
					/* Is it 224-line display */
					if ((reg[0x01] & 0x10) && !(reg[0x01] & 0x08)) {
						return vcnt_pal_224[currentLine];
					} else if (!(reg[0x01] & 0x10) && (reg[0x01] & 0x08)) {
						/* 240-line display */
						return vcnt_pal_240[currentLine];
					}
				}
			}
		}
		/* 192-line display */
		return vcnt_pal_192[currentLine];
	}
}

VIDEO_START(sms) {
	/* Clear RAM */
	memset(reg, 0, NUM_OF_REGISTER);
	isCRAMDirty = 1;
	if (IS_GG_ANY) {
		memset(ggCRAM, 0, GG_CRAM_SIZE);
		memset(isGGCRAMDirty, 1, GG_CRAM_SIZE);
	} else {
		memset(smsCRAM, 0, SMS_CRAM_SIZE);
		memset(isSMSCRAMDirty, 1, SMS_CRAM_SIZE);
	}
	memset(VRAM, 0, VRAM_SIZE);
	reg[0x02] = 0x0E;			/* power up default */

	/* Initialize VDP state variables */
	addr = code = pending = latch = buffer = statusReg = \
	currentLine = lineCountDownCounter = irqState = 0;

	if (IS_NTSC) {
		lineCollisionBuffer = auto_malloc(NTSC_X_PIXELS);
		spriteCache = auto_malloc(NTSC_X_PIXELS * 16);
	} else {
		lineCollisionBuffer = auto_malloc(PAL_X_PIXELS);
		spriteCache = auto_malloc(PAL_X_PIXELS * 16);
	}
	if (!lineCollisionBuffer) {
		return (1);
	}
	if (!spriteCache) {
		return (1);
	}

	/* Make temp bitmap for rendering */
	tmpbitmap = auto_bitmap_alloc(Machine->drv->screen_width, Machine->drv->screen_height);
	if (!tmpbitmap) {
		return (1);
	}

	prevBitMapSaved = 0;
	prevBitMap = auto_bitmap_alloc(Machine->drv->screen_width, Machine->drv->screen_height);
	if (!prevBitMap) {
		return (1);
	}

	return (0);
}

INTERRUPT_GEN(sms) {
	static UINT8 irqAtLines[3] = { 0xC1, 0xE1, 0xF1 };
	int irqAt;
	int maxLine;

	/* Is it NTSC */
	if (IS_NTSC) {
		/* Bump scanline counter */
		currentLine = (currentLine + 1) % NTSC_Y_PIXELS;

		/* We start a new frame, so reset line count down counter */
		if (currentLine == 0x00) {
			lineCountDownCounter = reg[0x0A];
		}

		/* must be mode 4 */
		if (reg[0x00] & 0x04) {
			if (IS_GG_ANY) {
				if (reg[0x00] & 0x02) {
					/* Is it 224-line display */
					if ((reg[0x01] & 0x10) && !(reg[0x01] & 0x08)) {
						irqAt = 0x01;
						logerror("mode 4\n");
					} else if (!(reg[0x01] & 0x10) && (reg[0x01] & 0x08)) {
						/* 240-line display */
						irqAt = 0x02;
					}
				}
			}
		}

		/* 192-line display */
		irqAt = 0x00;
	} else {
		/* It must be PAL */
		/* Bump scanline counter */
		currentLine = (currentLine + 1) % PAL_Y_PIXELS;

		/* We start a new frame, so reset line count down counter */
		if (currentLine == 0x00) {
			lineCountDownCounter = reg[0x0A];
		}

		/* must be mode 4 */
		if (reg[0x00] & 0x04) {
			if (IS_GG_ANY) {
				if (reg[0x00] & 0x02) {
					/* Is it 224-line display */
					if ((reg[0x01] & 0x10) && !(reg[0x01] & 0x08)) {
						irqAt = 0x01;
					} else if (!(reg[0x01] & 0x10) && (reg[0x01] & 0x08)) {
						/* 240-line display */
						irqAt = 0x02;
					}
				}
			}
		}

		/* 192-line display */
		irqAt = 0x00;
	}

	if (currentLine <= irqAtLines[irqAt]) {
		if (currentLine == irqAtLines[irqAt]) {
			statusReg |= STATUS_VINT;
		}

		if (currentLine == 0x00) {
			lineCountDownCounter = reg[0x0A];
		}

		if (lineCountDownCounter == 0x00) {
			lineCountDownCounter = reg[0x0A];
			statusReg |= STATUS_HINT;
		} else {
			lineCountDownCounter -= 1;
		}

		if ((statusReg & STATUS_HINT) && (reg[0x00] & 0x10)) {
			irqState = 1;
			cpu_set_irq_line(0, 0, ASSERT_LINE);
		}
	} else {
		lineCountDownCounter = reg[0x0A];

		if ((statusReg & STATUS_VINT) && (reg[0x01] & 0x20)) {
			irqState = 1;
			cpu_set_irq_line(0, 0, ASSERT_LINE);
		}
	}

	if (!osd_skip_this_frame()) {
#ifdef LOG_CURLINE
		logerror("l %04x, pc: %04x\n", currentLine, activecpu_get_pc());
#endif
		if (IS_GG_ANY) {
			if ((currentLine >= Machine->visible_area.min_y) && (currentLine <= Machine->visible_area.max_y)) {
				sms_update_palette();
#ifdef MAME_DEBUG
				if (code_pressed(KEYCODE_T)) {
					sms_show_tile_line(tmpbitmap, currentLine, 0);
				} else if (code_pressed(KEYCODE_Y)) {
					sms_show_tile_line(tmpbitmap, currentLine, 1);
				} else {
#endif
					sms_refresh_line(tmpbitmap, currentLine);
#ifdef MAME_DEBUG
				}
#endif
			}
		} else {
			maxLine = ((IS_NTSC) ? NTSC_Y_PIXELS : PAL_Y_PIXELS);

			if (currentLine < maxLine) {
				sms_update_palette();
#ifdef MAME_DEBUG
				if (code_pressed(KEYCODE_T)) {
					sms_show_tile_line(tmpbitmap, currentLine, 0);
				} else if (code_pressed(KEYCODE_Y)) {
					sms_show_tile_line(tmpbitmap, currentLine, 1);
				} else {
#endif
					sms_refresh_line(tmpbitmap, currentLine);
#ifdef MAME_DEBUG
				}
#endif
			}
		}
	}
}

READ_HANDLER(sms_vdp_data_r) {
	int temp;

	/* Clear pending write flag */
	pending = 0;

	/* Return read buffer contents */
	temp = buffer;
#ifdef LOG_REG
	logerror("VRAM[%x] = %x read\n", addr & 0x3FFF, temp);
#endif

	/* Load read buffer */
	buffer = VRAM[(addr & 0x3FFF)];

	/* Bump internal address register */
	addr += 1;
	return (temp);
}

READ_HANDLER(sms_vdp_ctrl_r) {
	int temp = statusReg;

	/* Clear pending write flag */
	pending = 0;
#ifdef LOG_REG
	logerror("CTRL read\n");
#endif

	statusReg &= ~(STATUS_VINT | STATUS_HINT | STATUS_SPRCOL);

	if (irqState == 1) {
		irqState = 0;
		cpu_set_irq_line(0, 0, CLEAR_LINE);
	}

	return (temp);
}

WRITE_HANDLER(sms_vdp_data_w) {
	/* Clear pending write flag */
	pending = 0;

	switch(code) {
		case 0x00:
		case 0x01:
		case 0x02: {
			int address = (addr & 0x3FFF);

			if (data != VRAM[address]) {
				VRAM[address] = data;
#ifdef LOG_REG
				logerror("VRAM[%x] = %x\n", address, data);
#endif
			}
		}
		break;
		case 0x03: {
			int address = IS_GG_ANY ? (addr & 0x3F) : (addr & 0x1F);
			int _index	= IS_GG_ANY ? ((addr & 0x3E) >> 1) : (addr & 0x1F);

#ifdef LOG_REG
			logerror("CRAM[%x] = %x\n", address, data);
#endif

			if (IS_GG_ANY) {
				if (data != ggCRAM[address]) {
					ggCRAM[address] = data;
					isGGCRAMDirty[_index] = isCRAMDirty = 1;
				}
			} else {
				if (data != smsCRAM[address]) {
					smsCRAM[address] = data;
					isSMSCRAMDirty[_index] = isCRAMDirty = 1;
				}
			}
		}
		break;
	}

	//buffer = data;
	addr += 1;
}

WRITE_HANDLER(sms_vdp_ctrl_w) {
	int regNum;

	if (pending == 0) {
		latch = data;
		pending = 1;
	} else {
		/* Clear pending write flag */
		pending = 0;

		code = (data >> 6) & 0x03;
		addr = ((data & 0x3F) << 8) | latch;
#ifdef LOG_REG
		logerror("code = %x, addr = %x\n", code, addr);
#endif

		/* Is it VDP register write - code 0x02 */
		if (code == 0x02) {
			regNum = data & 0x0F;
			reg[regNum] = latch;
			if (regNum == 0 && latch & 0x02) {
				logerror("overscan enabled.\n");
			}
#ifdef LOG_REG
			logerror("r%x = %x\n", regNum, latch);
#endif
			addr = code = 0;
		} else if (code == 0x00) {
			buffer = VRAM[(addr & 0x3FFF)];
			addr += 1;
		}
	}
}

#ifdef MAME_DEBUG
void sms_show_tile_line(struct mame_bitmap *bitmap, int line, int palletteSelected) {
	int tileColumn, tileRow;
	int pixelX;
	int bitPlane0, bitPlane1, bitPlane2, bitPlane3;

	/* Draw background layer */
	tileRow = line / 8;
	for (tileColumn = 0; tileColumn < 32; tileColumn++) {
		if ((tileColumn + (32 * tileRow)) > 448) {
			for (pixelX = 0; pixelX < 8 ; pixelX++) {
				plot_pixel(bitmap, (tileColumn << 3) + pixelX, line, Machine->pens[BACKDROP_COLOR]);
			}
		} else {
			bitPlane0 = VRAM[(((tileColumn + (32 * tileRow)) << 5) + ((line & 0x07) << 2)) + 0x00];
			bitPlane1 = VRAM[(((tileColumn + (32 * tileRow)) << 5) + ((line & 0x07) << 2)) + 0x01];
			bitPlane2 = VRAM[(((tileColumn + (32 * tileRow)) << 5) + ((line & 0x07) << 2)) + 0x02];
			bitPlane3 = VRAM[(((tileColumn + (32 * tileRow)) << 5) + ((line & 0x07) << 2)) + 0x03];

			for (pixelX = 0; pixelX < 8 ; pixelX++) {
				UINT8 penBit0, penBit1, penBit2, penBit3;
				UINT8 penSelected;

				penBit0 = (bitPlane0 >> (7 - pixelX)) & 0x01;
				penBit1 = (bitPlane1 >> (7 - pixelX)) & 0x01;
				penBit2 = (bitPlane2 >> (7 - pixelX)) & 0x01;
				penBit3 = (bitPlane3 >> (7 - pixelX)) & 0x01;

				penSelected = (penBit3 << 3 | penBit2 << 2 | penBit1 << 1 | penBit0);
				if (palletteSelected) {
					penSelected |= 0x10;
				}

				plot_pixel(bitmap, (tileColumn << 3) + pixelX, line, Machine->pens[penSelected]);
			}
		}
	}
}
#endif

void sms_refresh_line(struct mame_bitmap *bitmap, int line) {
	int tileColumn;
	int xScroll, yScroll, xScrollStartColumn;
	int spriteIndex;
	int pixelX, pixelPlotX, pixelPlotY, pixelOffsetX, prioritySelected[33];
	int spriteX, spriteY, spriteLine, spriteTileSelected, spriteHeight;
	int spriteBuffer[8], spriteBufferCount, spriteBufferIndex;
	int bitPlane0, bitPlane1, bitPlane2, bitPlane3;
	UINT16 *nameTable = (UINT16 *) &(VRAM[(((reg[0x02] & 0x0E) << 10) & 0x3800) + ((((line + reg[0x09]) % 224) >> 3) << 6)]);
	UINT8 *spriteTable = (UINT8 *) &(VRAM[(reg[0x05] << 7) & 0x3F00]);
	struct rectangle rec;

	rec = Machine->visible_area;
	pixelPlotY = line;
	pixelOffsetX = 0;
	if (!(IS_GG_ANY)) {
		pixelPlotY += (rec.min_y + TOP_192_BORDER);
		pixelOffsetX = rec.min_x + LBORDER_X_PIXELS;
	}

	/* Check if display is disabled */
	if (!(reg[0x01] & 0x40)) {
		/* set whole line to reg[0x07] color */
		rec = Machine->visible_area;
		rec.min_y = rec.max_y = pixelPlotY;
		fillbitmap(bitmap, Machine->pens[BACKDROP_COLOR], &rec);

		return;
	}

	/* Only SMS have border */
	if (!(IS_GG_ANY)) {
		if (line >= 192 && line < (192 + BOTTOM_192_BORDER)) {
			/* Draw bottom border */
			rec = Machine->visible_area;
			rec.min_y = rec.max_y = pixelPlotY;
			fillbitmap(bitmap, Machine->pens[BACKDROP_COLOR], &rec);

			return;
		}
		if (line >= (192 + BOTTOM_192_BORDER) && line < (192 + BOTTOM_192_BORDER + 19)) {
			return;
		}
		if (line >= (192 + BOTTOM_192_BORDER + 19) && line < (192 + BOTTOM_192_BORDER + 19 + TOP_192_BORDER)) {
			/* Draw top border */
			rec = Machine->visible_area;
			rec.min_y = rec.max_y = line + rec.min_y - (192 + BOTTOM_192_BORDER + 19);
			fillbitmap(bitmap, Machine->pens[BACKDROP_COLOR], &rec);

			return;
		}
		/* Draw left border */
		rec = Machine->visible_area;
		rec.min_y = rec.max_y = pixelPlotY;
		rec.max_x = rec.min_x + LBORDER_X_PIXELS;
		fillbitmap(bitmap, Machine->pens[BACKDROP_COLOR], &rec);
		/* Draw right border */
		rec = Machine->visible_area;
		rec.min_y = rec.max_y = pixelPlotY;
		rec.min_x = rec.max_x - RBORDER_X_PIXELS;
		fillbitmap(bitmap, Machine->pens[BACKDROP_COLOR], &rec);
	}

	/* if top 2 rows of screen not affected by horizontal scrolling, then xScroll = 0 */
	/* else xScroll = reg[0x08] (SMS Only)                                            */
	if (IS_GG_ANY) {
		xScroll = 0x0100 - reg[0x08];
	} else {
		xScroll = (((reg[0x00] & 0x40) && (line < 16)) ? 0 : 0x0100 - reg[0x08]);
	}

	xScrollStartColumn = (xScroll >> 3);							 /* x starting column tile */

	/* Draw background layer */
	for (tileColumn = 0; tileColumn < 33; tileColumn++) {
		UINT16 tileData;
		int tileSelected, palletteSelected, horizSelected, vertSelected;
		int tileLine;

		/* Rightmost 8 columns for SMS (or 2 columns for GG) not affected by */
		/* vertical scrolling when bit 7 of reg[0x00] is set */
		if (IS_GG_ANY) {
			yScroll = (((reg[0x00] & 0x80) && (tileColumn > 29)) ? 0 : (reg[0x09]) % 224);
		} else {
			yScroll = (((reg[0x00] & 0x80) && (tileColumn > 23)) ? 0 : (reg[0x09]) % 224);
		}

		#ifndef LSB_FIRST
		tileData = ((nameTable[((tileColumn + xScrollStartColumn) & 0x1F)] & 0xFF) << 8) | (nameTable[((tileColumn + xScrollStartColumn) & 0x1F)] >> 8);
		#else
		tileData = nameTable[((tileColumn + xScrollStartColumn) & 0x1F)];
		#endif

		tileSelected = (tileData & 0x01FF);
		prioritySelected[tileColumn] = (tileData >> 12) & 0x01;
		palletteSelected = (tileData >> 11) & 0x01;
		vertSelected = (tileData >> 10) & 0x01;
		horizSelected = (tileData >> 9) & 0x01;

		tileLine = line - ((0x07 - (yScroll & 0x07)) + 1);
		if (vertSelected) {
			tileLine = 0x07 - tileLine;
		}
		bitPlane0 = VRAM[((tileSelected << 5) + ((tileLine & 0x07) << 2)) + 0x00];
		bitPlane1 = VRAM[((tileSelected << 5) + ((tileLine & 0x07) << 2)) + 0x01];
		bitPlane2 = VRAM[((tileSelected << 5) + ((tileLine & 0x07) << 2)) + 0x02];
		bitPlane3 = VRAM[((tileSelected << 5) + ((tileLine & 0x07) << 2)) + 0x03];

		for (pixelX = 0; pixelX < 8 ; pixelX++) {
			UINT8 penBit0, penBit1, penBit2, penBit3;
			UINT8 penSelected;

			penBit0 = (bitPlane0 >> (7 - pixelX)) & 0x01;
			penBit1 = (bitPlane1 >> (7 - pixelX)) & 0x01;
			penBit2 = (bitPlane2 >> (7 - pixelX)) & 0x01;
			penBit3 = (bitPlane3 >> (7 - pixelX)) & 0x01;

			penSelected = (penBit3 << 3 | penBit2 << 2 | penBit1 << 1 | penBit0);
			if (palletteSelected) {
				penSelected |= 0x10;
			}

			if (!horizSelected) {
				pixelPlotX = pixelX;
			} else {
				pixelPlotX = 7 - pixelX;
			}
			pixelPlotX = (0 - (xScroll & 0x07) + (tileColumn << 3) + pixelPlotX);
			if (pixelPlotX >= 0 && pixelPlotX < 256) {
//				logerror("%x %x\n", pixelPlotX + pixelOffsetX, pixelPlotY);
				plot_pixel(bitmap, pixelPlotX + pixelOffsetX, pixelPlotY, Machine->pens[penSelected]);
			}
		}
	}

	/* Draw sprite layer */
	spriteHeight = (reg[0x01] & 0x02 ? 16 : 8);
	if (reg[0x01] & 0x01) {
		/* sprite doubling */																/********* TODO: Need to emulate x pixel doubling bug	 (SMS Only) **********/
		spriteHeight <<= 1;
	}
	spriteBufferCount = 0;
	for (spriteIndex = 0; (spriteIndex < 64) && (spriteTable[spriteIndex] != 0xD0) && (spriteBufferCount < 8); spriteIndex++) {
		spriteY = spriteTable[spriteIndex] + 1; /* sprite y position starts at line 1 */
		if (spriteY > 240) {
			spriteY -= 256; /* wrap from top if y position is > 240 */
		}
		if ((line >= spriteY) && (line < (spriteY + spriteHeight))) {
			if (spriteBufferCount < 8) {
				spriteBuffer[spriteBufferCount] = spriteIndex;
				spriteBufferCount++;
			} else {
				/* too many sprites per line */
				statusReg |= STATUS_HINT;
			}
		}
	}
	/* Is it NTSC */
	memset(lineCollisionBuffer, 0, (IS_NTSC) ? NTSC_X_PIXELS : PAL_X_PIXELS);
	spriteBufferCount--;
	for (spriteBufferIndex = spriteBufferCount; spriteBufferIndex >= 0; spriteBufferIndex--) {
		spriteIndex = spriteBuffer[spriteBufferIndex];
		spriteY = spriteTable[spriteIndex] + 1; /* sprite y position starts at line 1 */
		if (spriteY > 240) {
			spriteY -= 256; /* wrap from top if y position is > 240 */
		}

		spriteX = spriteTable[0x80 + (spriteIndex << 1)];
		if (reg[0x00] & 0x08) {
			spriteX -= 0x08;	 /* sprite shift */
		}
		spriteTileSelected = spriteTable[0x81 + (spriteIndex << 1)];
		if (reg[0x06] & 0x04) {
			spriteTileSelected += 256; /* pattern table select */
		}
		if (reg[0x01] & 0x02) {
			spriteTileSelected &= 0x01FE; /* force even index */
		}
		spriteLine = line - spriteY;

		if (spriteLine > 0x07) {
			spriteTileSelected += 1;
		}

		bitPlane0 = VRAM[((spriteTileSelected << 5) + ((spriteLine & 0x07) << 2)) + 0x00];
		bitPlane1 = VRAM[((spriteTileSelected << 5) + ((spriteLine & 0x07) << 2)) + 0x01];
		bitPlane2 = VRAM[((spriteTileSelected << 5) + ((spriteLine & 0x07) << 2)) + 0x02];
		bitPlane3 = VRAM[((spriteTileSelected << 5) + ((spriteLine & 0x07) << 2)) + 0x03];

		for (pixelX = 0; pixelX < 8 ; pixelX++) {
			UINT8 penBit0, penBit1, penBit2, penBit3;
			int penSelected;

			penBit0 = (bitPlane0 >> (7 - pixelX)) & 0x01;
			penBit1 = (bitPlane1 >> (7 - pixelX)) & 0x01;
			penBit2 = (bitPlane2 >> (7 - pixelX)) & 0x01;
			penBit3 = (bitPlane3 >> (7 - pixelX)) & 0x01;

			penSelected = (penBit3 << 3 | penBit2 << 2 | penBit1 << 1 | penBit0) | 0x10;

			if (reg[0x01] & 0x01) {
				/* sprite doubling is enabled */
				pixelPlotX = spriteX + (pixelX << 1) + pixelOffsetX;
				if (spriteLine < (spriteHeight >> 1)) {
					spriteCache[pixelPlotX + (((IS_NTSC) ? NTSC_X_PIXELS : PAL_X_PIXELS) * spriteLine)] = penSelected;
				}

				penSelected = pixelPlotX + (((IS_NTSC) ? NTSC_X_PIXELS : PAL_X_PIXELS) * ((spriteLine & 0xFE) >> 1));
				if (spriteCache[penSelected] == 0x10) {		/* Transparent pallette so skip draw */
					continue;
				}
				if (!prioritySelected[(((xScroll & 0x07) + pixelPlotX - pixelOffsetX) / 8) & 0x1F]) {
					plot_pixel(bitmap, pixelPlotX, pixelPlotY, Machine->pens[spriteCache[penSelected]]);
					plot_pixel(bitmap, pixelPlotX + 1, pixelPlotY, Machine->pens[spriteCache[penSelected]]);
				} else {
					if (read_pixel(bitmap, pixelPlotX, pixelPlotY) == Machine->pens[0x00]) {
						plot_pixel(bitmap, pixelPlotX, pixelPlotY, Machine->pens[spriteCache[penSelected]]);
					}
					if (read_pixel(bitmap, pixelPlotX + 1, pixelPlotY) == Machine->pens[0x00]) {
						plot_pixel(bitmap, pixelPlotX + 1, pixelPlotY, Machine->pens[spriteCache[penSelected]]);
					}
				}
				if (lineCollisionBuffer[pixelPlotX] != 1) {
					lineCollisionBuffer[pixelPlotX] = 1;
				} else {
					/* sprite collision occurred */
					statusReg |= STATUS_SPRCOL;
				}
				if (lineCollisionBuffer[pixelPlotX + 1] != 1) {
					lineCollisionBuffer[pixelPlotX + 1] = 1;
				} else {
					/* sprite collision occurred */
					statusReg |= STATUS_SPRCOL;
				}
			} else {
				if (penSelected == 0x10) {		/* Transparent pallette so skip draw */
					continue;
				}

				pixelPlotX = spriteX + pixelX + pixelOffsetX;
				if (!prioritySelected[(((xScroll & 0x07) + pixelPlotX - pixelOffsetX) / 8) & 0x1F]) {
					plot_pixel(bitmap, pixelPlotX, pixelPlotY, Machine->pens[penSelected]);
				} else {
					if (read_pixel(bitmap, pixelPlotX, pixelPlotY) == Machine->pens[0x00]) {
						plot_pixel(bitmap, pixelPlotX, pixelPlotY, Machine->pens[penSelected]);
					}
				}
				if (lineCollisionBuffer[pixelPlotX] != 1) {
					lineCollisionBuffer[pixelPlotX] = 1;
				} else {
					/* sprite collision occurred */
					statusReg |= STATUS_SPRCOL;
				}
			}
		}
	}

	/* Fill column 0 with overscan color from reg[0x07]	 (SMS Only) */
	if (!IS_GG_ANY) {
		if (reg[0x00] & 0x20) {
			rec = Machine->visible_area;
			rec.min_y = rec.max_y = pixelPlotY;
			rec.min_x += LBORDER_X_PIXELS;
			rec.max_x = rec.min_x + 8;
			fillbitmap(bitmap, Machine->pens[BACKDROP_COLOR], &rec);
		}
	}
}

void sms_update_palette(void) {
	int i, r, g, b;

	/* Exit if palette is has no changes */
	if (isCRAMDirty == 0) {
		return;
	}
	isCRAMDirty = 0;

	if (IS_GG_ANY) {
		for (i = 0; i < (GG_CRAM_SIZE >> 1); i += 1) {
			if (isGGCRAMDirty[i] == 1) {
				isGGCRAMDirty[i] = 0;

				r = ((ggCRAM[i * 2 + 0] >> 0) & 0x0F) << 4;
				g = ((ggCRAM[i * 2 + 0] >> 4) & 0x0F) << 4;
				b = ((ggCRAM[i * 2 + 1] >> 0) & 0x0F) << 4;
				palette_set_color(i, r, g, b);
#ifdef LOG_COLOR
				logerror("pallette set for i %x r %x g %x b %x\n", i, r, g, b);
#endif
			}
		}
	} else {
		for (i = 0; i < SMS_CRAM_SIZE; i += 1) {
			if (isSMSCRAMDirty[i] == 1) {
				isSMSCRAMDirty[i] = 0;

				r = ((smsCRAM[i] >> 0) & 0x03) << 6;
				g = ((smsCRAM[i] >> 2) & 0x03) << 6;
				b = ((smsCRAM[i] >> 4) & 0x03) << 6;
				palette_set_color(i, r, g, b);
#ifdef LOG_COLOR
				logerror("pallette set for i %x r %x g %x b %x\n", i, r, g, b);
#endif
			}
		}
	}
}

VIDEO_UPDATE(sms) {
	int x, y;

	if (prevBitMapSaved) {
	for (y = 0; y < Machine->drv->screen_height; y++) {
		for (x = 0; x < Machine->drv->screen_width; x++) {
			plot_pixel(bitmap, x, y, (read_pixel(tmpbitmap, x, y) + read_pixel(prevBitMap, x, y)) >> 2);
			logerror("%x %x %x\n", read_pixel(tmpbitmap, x, y), read_pixel(prevBitMap, x, y), (read_pixel(tmpbitmap, x, y) + read_pixel(prevBitMap, x, y)) >> 2);
		}
	}
	} else {
		copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);
	}
	if (!prevBitMapSaved) {
		copybitmap(prevBitMap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);
	//prevBitMapSaved = 1;
	}
}


