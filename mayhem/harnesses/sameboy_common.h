/* Shared SameBoy (bsnes/gb/Core) setup for the mayhem harnesses.
 * Every input gets a fresh GB_gameboy_t, freed at the end of the input, so no
 * emulator state carries over between inputs. */
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <Core/gb.h>

static uint32_t fuzz_pixels[256 * 224]; /* large enough for SGB border output */

static uint32_t fuzz_rgb(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void)gb;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void fuzz_log(GB_gameboy_t *gb, const char *string, GB_log_attributes_t attributes)
{
    (void)gb; (void)string; (void)attributes;
}

/* No interactive debugger: returning NULL makes a debugger stop resume running. */
static char *fuzz_input(GB_gameboy_t *gb)
{
    (void)gb;
    return NULL;
}

static const GB_model_t fuzz_models[] = {
    GB_MODEL_DMG_B, GB_MODEL_CGB_E, GB_MODEL_SGB, GB_MODEL_AGB_A,
    GB_MODEL_MGB, GB_MODEL_SGB2, GB_MODEL_CGB_C, GB_MODEL_SGB_PAL,
};

static GB_model_t fuzz_pick_model(uint8_t selector)
{
    return fuzz_models[selector % (sizeof(fuzz_models) / sizeof(fuzz_models[0]))];
}

/* A zero-filled boot ROM is a NOP slide into the cartridge entry point at 0x100. */
static GB_gameboy_t *fuzz_gb_new(GB_model_t model)
{
    static uint8_t boot[0x900];
    GB_gameboy_t *gb = GB_init(GB_alloc(), model);
    GB_set_log_callback(gb, fuzz_log);
    GB_set_input_callback(gb, fuzz_input);
    GB_set_async_input_callback(gb, NULL);
    GB_set_rgb_encode_callback(gb, fuzz_rgb);
    GB_set_pixels_output(gb, fuzz_pixels);
    GB_set_sample_rate(gb, 0);
    GB_load_boot_rom_from_buffer(gb, boot, (model & GB_MODEL_FAMILY_MASK) == GB_MODEL_CGB_FAMILY ? 0x900 : 0x100);
    return gb;
}

static void fuzz_gb_free(GB_gameboy_t *gb)
{
    GB_free(gb);
    GB_dealloc(gb);
}

static void fuzz_run_frames(GB_gameboy_t *gb, unsigned frames)
{
    for (unsigned i = 0; i < frames; i++) GB_run_frame(gb);
}

/* A fixed, valid 32 KiB ROM-only cartridge: NOP slide + an infinite loop at 0x150. */
static void fuzz_load_fixed_rom(GB_gameboy_t *gb)
{
    static uint8_t rom[0x8000];
    memset(rom, 0, sizeof(rom));
    rom[0x100] = 0x00; rom[0x101] = 0xC3; rom[0x102] = 0x50; rom[0x103] = 0x01; /* jp $150 */
    memcpy(rom + 0x134, "MAYHEMFUZZ", 10);
    rom[0x150] = 0x18; rom[0x151] = 0xFE; /* jr -2 */
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
}

/* For APIs that only take a path: write the input to a fresh scratch file under $TMPDIR. */
#include <stdio.h>
#include <unistd.h>
static int fuzz_write_tmp(const uint8_t *data, size_t size, const char *suffix, char *path, size_t path_size)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";
    snprintf(path, path_size, "%s/sameboy-fuzz-XXXXXX%s", dir, suffix);
    int fd = mkstemps(path, (int)strlen(suffix));
    if (fd < 0) return -1;
    size_t off = 0;
    while (off < size) {
        ssize_t w = write(fd, data + off, size - off);
        if (w <= 0) { close(fd); unlink(path); return -1; }
        off += (size_t)w;
    }
    close(fd);
    return 0;
}
