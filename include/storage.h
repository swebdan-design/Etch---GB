/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Battery-backed SRAM project storage (MBC5, 32 KB = 4 x 8 KB banks).
 */
#ifndef STORAGE_H
#define STORAGE_H
#include "etch.h"

#define SLOT_EMPTY 0
#define SLOT_OK    1
#define SLOT_BAD   2          /* header present but checksum/version/size wrong */

#define SAVE_VERSION 1
#define SAVE_NONE    0xFF

uint8_t storage_slot_status(uint8_t slot, char *name_out) ETCH_BANKED;  /* name_out: 10 chars */
uint8_t storage_save(uint8_t slot) ETCH_BANKED;    /* 1 = written and verified */
uint8_t storage_load(uint8_t slot) ETCH_BANKED;    /* 1 = loaded; 0 = empty/corrupt, proj untouched */
uint8_t storage_last_slot(void) ETCH_BANKED;       /* SAVE_NONE if unknown */
void    storage_erase(uint8_t slot) ETCH_BANKED;
#endif
