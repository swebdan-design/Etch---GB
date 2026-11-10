/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Project persistence in cartridge SRAM.
 *
 * Layout: 8 slots of 4 KB; slot s lives in SRAM bank s/2 at offset
 * (s & 1) * 0x1000.  Each slot = 10-byte header + project_t image:
 *     "ETCH" | version | 0 | size (LE16) | Fletcher-16 (LE16) | data...
 * A save first invalidates the magic, writes the data, computes the checksum
 * from what is actually in SRAM, writes the header last and reads it back.
 * Bank 3 offset 0x1FF0 keeps a small metadata block (last used slot).
 * Missing, foreign or corrupted data is reported (EMPTY / BAD) and never
 * loaded; the working project is only replaced after full verification.
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "storage.h"
#include "engine.h"

#define HDR_SIZE 10
#define META_OFF 0x1FF0

static uint8_t *slot_base(uint8_t s)
{
    SWITCH_RAM((uint8_t)(s >> 1));
    return (uint8_t *)(0xA000 + ((uint16_t)(s & 1) << 12));
}

extern uint16_t fl_ptr, fl_n, fl_sum;
void fletcher_asm(void);

/* Fletcher-16 (assembly, src/storage/fletcher.s) */
static uint16_t fletcher(const uint8_t *p, uint16_t n)
{
    fl_ptr = (uint16_t)p;
    fl_n = n;
    fletcher_asm();
    return fl_sum;
}

static uint8_t check(uint8_t *base)
{
    uint16_t size, sum;
    if (base[0] != 'E' || base[1] != 'T' || base[2] != 'C' || base[3] != 'H') return SLOT_EMPTY;
    size = (uint16_t)(base[6] | (base[7] << 8));
    sum = (uint16_t)(base[8] | (base[9] << 8));
    if (base[4] != SAVE_VERSION || size != sizeof(project_t)) return SLOT_BAD;
    if (fletcher(base + HDR_SIZE, size) != sum) return SLOT_BAD;
    return SLOT_OK;
}

uint8_t storage_slot_status(uint8_t slot, char *name_out) ETCH_BANKED
{
    uint8_t r, i, *b;
    if (slot >= NUM_SLOTS) return SLOT_BAD;
    ENABLE_RAM;
    b = slot_base(slot);
    r = check(b);
    if (r == SLOT_OK && name_out)
        for (i = 0; i < PROJECT_NAME_LEN; i++) name_out[i] = (char)b[HDR_SIZE + i];
    DISABLE_RAM;
    return r;
}

uint8_t storage_save(uint8_t slot) ETCH_BANKED
{
    uint8_t *b, ok;
    uint16_t sum;
    if (slot >= NUM_SLOTS) return 0;
    ENABLE_RAM;
    b = slot_base(slot);
    b[0] = 0;                                   /* invalidate while writing */
    memcpy(b + HDR_SIZE, &proj, sizeof(project_t));
    sum = fletcher(b + HDR_SIZE, sizeof(project_t));
    b[4] = SAVE_VERSION;
    b[5] = 0;
    b[6] = (uint8_t)sizeof(project_t);
    b[7] = (uint8_t)(sizeof(project_t) >> 8);
    b[8] = (uint8_t)sum;
    b[9] = (uint8_t)(sum >> 8);
    b[1] = 'T'; b[2] = 'C'; b[3] = 'H';
    b[0] = 'E';
    ok = check(b) == SLOT_OK;
    if (ok) {
        uint8_t *m;
        SWITCH_RAM(3);
        m = (uint8_t *)(0xA000 + META_OFF);
        m[0] = 'E'; m[1] = 'M'; m[2] = slot; m[3] = (uint8_t)~slot;
    }
    DISABLE_RAM;
    return ok;
}

uint8_t storage_load(uint8_t slot) ETCH_BANKED
{
    uint8_t *b;
    uint16_t off;
    if (slot >= NUM_SLOTS) return 0;
    ENABLE_RAM;
    b = slot_base(slot);
    if (check(b) != SLOT_OK) { DISABLE_RAM; return 0; }
    /* copy in short critical chunks so playback keeps its timing */
    for (off = 0; off < sizeof(project_t); off += 64) {
        uint16_t n = sizeof(project_t) - off;
        if (n > 64) n = 64;
        CRITICAL {
            memcpy((uint8_t *)&proj + off, b + HDR_SIZE + off, n);
        }
    }
    DISABLE_RAM;
    return 1;
}

uint8_t storage_last_slot(void) ETCH_BANKED
{
    uint8_t *m, r = SAVE_NONE;
    ENABLE_RAM;
    SWITCH_RAM(3);
    m = (uint8_t *)(0xA000 + META_OFF);
    if (m[0] == 'E' && m[1] == 'M' && m[2] < NUM_SLOTS && m[3] == (uint8_t)~m[2]) r = m[2];
    DISABLE_RAM;
    return r;
}

void storage_erase(uint8_t slot) ETCH_BANKED
{
    uint8_t *b;
    if (slot >= NUM_SLOTS) return;
    ENABLE_RAM;
    b = slot_base(slot);
    b[0] = 0; b[1] = 0; b[2] = 0; b[3] = 0;
    DISABLE_RAM;
}
