/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Headless SameBoy test harness (optional test tool, not part of the ROM).
 * Reads one command per line on stdin, answers on stdout:
 *   run N                 run N frames
 *   key NAME 0|1          set button state (right left up down a b select start)
 *   peek ADDR N           print N bytes (hex) from the CPU address space
 *   poke ADDR VAL         write a byte
 *   shot FILE             write the current frame as a binary PPM
 *   wav FILE              start recording audio to FILE (48 kHz stereo 16-bit)
 *   wavstop               finish the WAV file
 *   savebat FILE          write cartridge battery RAM
 *   cycles                print total emulated cycles (8 MHz units)
 *   prof on|off           per-PC cycle profiling (attributes each GB_run step)
 *   profdump FILE         write "pc cycles" lines
 *   quit
 * argv: harness BOOTROM ROM [BATTERYFILE] [model:dmg|cgb]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "gb.h"

static uint32_t pixels[160 * 144];
static FILE *wavf;
static uint32_t wav_frames;
static uint64_t vblanks;
static uint64_t prof[65536];
static int profiling;

static uint32_t rgb_encode(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void)gb;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void on_vblank(GB_gameboy_t *gb, GB_vblank_type_t type)
{
    (void)gb; (void)type;
    vblanks++;
}

static void on_sample(GB_gameboy_t *gb, GB_sample_t *s)
{
    (void)gb;
    if (wavf) {
        fwrite(&s->left, 2, 1, wavf);
        fwrite(&s->right, 2, 1, wavf);
        wav_frames++;
    }
}

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

static void wav_header(FILE *f, uint32_t frames)
{
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); put32(f, 36 + frames * 4);
    fwrite("WAVEfmt ", 1, 8, f); put32(f, 16); put16(f, 1); put16(f, 2);
    put32(f, 48000); put32(f, 48000 * 4); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4);
}

int main(int argc, char **argv)
{
    char line[512], a1[256], a2[64];
    GB_gameboy_t *gb;
    GB_model_t model = GB_MODEL_DMG_B;
    uint64_t cycles = 0;
    if (argc < 3) { fprintf(stderr, "usage\n"); return 1; }
    if (argc > 4 && strcmp(argv[4], "cgb") == 0) model = GB_MODEL_CGB_E;
    gb = GB_init(GB_alloc(), model);
    if (GB_load_boot_rom(gb, argv[1])) { fprintf(stderr, "bootrom\n"); return 1; }
    if (GB_load_rom(gb, argv[2])) { fprintf(stderr, "rom\n"); return 1; }
    if (argc > 3 && strcmp(argv[3], "-") != 0) GB_load_battery(gb, argv[3]);
    GB_set_rgb_encode_callback(gb, rgb_encode);
    GB_set_pixels_output(gb, pixels);
    GB_set_vblank_callback(gb, on_vblank);
    GB_set_sample_rate(gb, 48000);
    GB_apu_set_sample_callback(gb, on_sample);
    GB_set_emulate_joypad_bouncing(gb, false);
    GB_set_highpass_filter_mode(gb, GB_HIGHPASS_ACCURATE);   /* the console's output capacitor */
    setvbuf(stdout, NULL, _IOLBF, 0);
    while (fgets(line, sizeof line, stdin)) {
        int n = 0, v = 0;
        unsigned addr = 0;
        if (sscanf(line, "run %d", &n) == 1) {
            uint64_t target = vblanks + (uint64_t)n;
            while (vblanks < target) {
                if (profiling) {
                    uint16_t pc = GB_get_registers(gb)->pc;
                    unsigned c = GB_run(gb);
                    prof[pc] += c;
                    cycles += c;
                } else {
                    cycles += GB_run(gb);
                }
            }
            printf("ok\n");
        } else if (sscanf(line, "key %63s %d", a2, &v) == 2) {
            static const char *names[] = {"right","left","up","down","a","b","select","start"};
            int k;
            for (k = 0; k < 8; k++) if (!strcmp(names[k], a2)) GB_set_key_state(gb, (GB_key_t)k, v != 0);
            printf("ok\n");
        } else if (sscanf(line, "peek %x %d", &addr, &n) == 2) {
            int i;
            for (i = 0; i < n; i++) printf("%02x", GB_safe_read_memory(gb, (uint16_t)(addr + i)));
            printf("\n");
        } else if (sscanf(line, "poke %x %x", &addr, &v) == 2) {
            GB_write_memory(gb, (uint16_t)addr, (uint8_t)v);
            printf("ok\n");
        } else if (sscanf(line, "shot %255s", a1) == 1) {
            FILE *f = fopen(a1, "wb");
            int i;
            fprintf(f, "P6 160 144 255\n");
            for (i = 0; i < 160 * 144; i++) {
                fputc((pixels[i] >> 16) & 255, f);
                fputc((pixels[i] >> 8) & 255, f);
                fputc(pixels[i] & 255, f);
            }
            fclose(f);
            printf("ok\n");
        } else if (!strncmp(line, "wavstop", 7)) {
            if (wavf) { wav_header(wavf, wav_frames); fclose(wavf); wavf = NULL; }
            printf("ok %u\n", wav_frames);
        } else if (sscanf(line, "wav %255s", a1) == 1) {
            wavf = fopen(a1, "wb");
            wav_frames = 0;
            wav_header(wavf, 0);
            printf("ok\n");
        } else if (sscanf(line, "savebat %255s", a1) == 1) {
            GB_save_battery(gb, a1);
            printf("ok\n");
        } else if (!strncmp(line, "prof on", 7)) {
            memset(prof, 0, sizeof prof);
            profiling = 1;
            printf("ok\n");
        } else if (!strncmp(line, "prof off", 8)) {
            profiling = 0;
            printf("ok\n");
        } else if (sscanf(line, "profdump %255s", a1) == 1) {
            FILE *f = fopen(a1, "w");
            unsigned i;
            for (i = 0; i < 65536; i++) if (prof[i]) fprintf(f, "%u %llu\n", i, (unsigned long long)prof[i]);
            fclose(f);
            printf("ok\n");
        } else if (!strncmp(line, "cycles", 6)) {
            printf("%llu\n", (unsigned long long)cycles);
        } else if (!strncmp(line, "quit", 4)) {
            break;
        } else {
            printf("err\n");
        }
    }
    if (wavf) { wav_header(wavf, wav_frames); fclose(wavf); }
    return 0;
}
