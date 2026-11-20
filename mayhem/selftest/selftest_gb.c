/* Known-answer functional tests for the SameBoy core (bsnes/gb/Core), built with the
 * project's normal flags. Prints one "PASS <name>" / "FAIL <name>" line per check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Core/gb.h>

static uint32_t pixels[256 * 224];
static int failures;
static uint32_t rgb(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b) { (void)gb; return (r << 16) | (g << 8) | b; }
static void quiet(GB_gameboy_t *gb, const char *s, GB_log_attributes_t a) { (void)gb; (void)s; (void)a; }
static char *no_input(GB_gameboy_t *gb) { (void)gb; return NULL; }

static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) failures++;
}

static GB_gameboy_t *make(GB_model_t model)
{
    static uint8_t boot[0x900];
    GB_gameboy_t *gb = GB_init(GB_alloc(), model);
    GB_set_log_callback(gb, quiet);
    GB_set_input_callback(gb, no_input);
    GB_set_async_input_callback(gb, NULL);
    GB_set_rgb_encode_callback(gb, rgb);
    GB_set_pixels_output(gb, pixels);
    GB_load_boot_rom_from_buffer(gb, boot, (model & GB_MODEL_FAMILY_MASK) == GB_MODEL_CGB_FAMILY ? 0x900 : 0x100);
    return gb;
}

static void destroy(GB_gameboy_t *gb) { GB_free(gb); GB_dealloc(gb); }

/* ROM: ld a,$42 ; ld ($C000),a ; ld a,$37 ; ld b,a ; add a,b ; ld ($C001),a ; jr -2 */
static void build_rom(uint8_t *rom, size_t size, uint8_t cart_type, uint8_t ram_code)
{
    memset(rom, 0, size);
    const uint8_t entry[] = {0x00, 0xC3, 0x50, 0x01};
    memcpy(rom + 0x100, entry, sizeof(entry));
    memcpy(rom + 0x134, "MAYHEMTEST", 10);
    rom[0x147] = cart_type;
    rom[0x148] = 0x00;
    rom[0x149] = ram_code;
    const uint8_t code[] = {0x3E, 0x42, 0xEA, 0x00, 0xC0, 0x3E, 0x37, 0x47, 0x80, 0xEA, 0x01, 0xC0, 0x18, 0xFE};
    memcpy(rom + 0x150, code, sizeof(code));
}

int main(void)
{
    static uint8_t rom[0x8000];
    build_rom(rom, sizeof(rom), 0x00, 0x00);

    GB_gameboy_t *gb = make(GB_MODEL_DMG_B);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    char title[17] = {0};
    GB_get_rom_title(gb, title);
    check("gb_rom_title", strcmp(title, "MAYHEMTEST") == 0);
    for (int i = 0; i < 5; i++) GB_run_frame(gb);
    check("gb_cpu_store", GB_read_memory(gb, 0xC000) == 0x42);
    check("gb_cpu_add", GB_read_memory(gb, 0xC001) == 0x6E);
    check("gb_battery_none", GB_save_battery_size(gb) == 0);

    size_t state_size = GB_get_save_state_size(gb);
    uint8_t *state = malloc(state_size);
    GB_save_state_to_buffer(gb, state);
    GB_model_t model = 0;
    check("gb_state_model", GB_get_state_model_from_buffer(state, state_size, &model) == 0 && model == GB_MODEL_DMG_B);
    destroy(gb);

    gb = make(GB_MODEL_DMG_B);
    build_rom(rom, sizeof(rom), 0x00, 0x00);
    rom[0x150] = 0x18; rom[0x151] = 0xFE; /* a ROM that never stores anything */
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    check("gb_state_load", GB_load_state_from_buffer(gb, state, state_size) == 0);
    check("gb_state_restored", GB_read_memory(gb, 0xC000) == 0x42 && GB_read_memory(gb, 0xC001) == 0x6E);
    uint16_t value = 0, bank = 0;
    check("gb_debugger_eval", GB_debugger_evaluate(gb, "$10 + 2 * 3", &value, &bank) == false && value == 0x16);
    check("gb_debugger_mem", GB_debugger_evaluate(gb, "[$c000]", &value, &bank) == false && value == 0x42);
    free(state);
    destroy(gb);

    /* MBC1+RAM+BATTERY with 32 KiB RAM: cartridge detection + battery size */
    gb = make(GB_MODEL_CGB_E);
    build_rom(rom, sizeof(rom), 0x03, 0x03);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    check("gb_mbc1_battery", GB_save_battery_size(gb) == 0x8000);
    destroy(gb);

    gb = make(GB_MODEL_DMG_B);
    build_rom(rom, sizeof(rom), 0x00, 0x00);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    const GB_cheat_t *cheat = GB_import_cheat(gb, "01FF00C0", "gameshark", true);
    check("gb_cheat_import", cheat != NULL);
    check("gb_cheat_reject", GB_import_cheat(gb, "not a cheat", "bad", true) == NULL);
    destroy(gb);

    return failures ? 1 : 0;
}
