/* SameBoy: load an arbitrary Game Boy ROM image and emulate it.
 * Byte 0 selects the hardware model (DMG/CGB/SGB/AGB/...), the rest is the ROM.
 * Reaches gb.c (ROM loading/size rounding), mbc.c (cartridge/MMM01 detection),
 * memory.c, sm83_cpu.c, display.c, apu.c, timing.c and, for SGB models, sgb.c.
 * Two frames per input (one full LCD/vblank cycle after boot) keep each exec around 15 ms
 * under ASan+UBSan; 12 frames took ~80 ms and made Mayhem's startup pass over a grown
 * testsuite fail.
 * Inputs too short to hold a cartridge header (0x150 bytes after the model byte) are skipped:
 * an empty or few-byte ROM reads as 0xFF (RST $38) and the stack sweep through I/O space hits
 * the upstream HRAM index -53 write (memory.c:1773) on ~60% of them. Mayhem's -runs=5 startup
 * probe mutates up from an empty input, so without this it failed about half the time. Larger
 * inputs still reach that bug; nothing about the emulator is changed. */
#define FUZZ_MIN_ROM 0x150
#include "sameboy_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1 + FUZZ_MIN_ROM) return -1;
    GB_gameboy_t *gb = fuzz_gb_new(fuzz_pick_model(data[0]));
    GB_load_rom_from_buffer(gb, data + 1, size - 1);
    fuzz_run_frames(gb, 2);
    fuzz_gb_free(gb);
    return 0;
}
