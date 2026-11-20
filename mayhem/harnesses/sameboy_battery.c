/* SameBoy: load a battery save (.sav: cartridge RAM followed by an optional RTC block in the
 * SameBoy-legacy / VBA 32-bit / VBA 64-bit / TPP1 / HuC3 layouts) with GB_load_battery_from_buffer,
 * the entry bsnes's Super Game Boy (sfc/coprocessor/icd/icd.cpp) and SameBoy's frontends use, then
 * emulate a frame on the restored RAM/RTC.
 *
 * The save is only meaningful against a cartridge, so the first 5 input bytes describe the fixed
 * cartridge it is loaded into (they are not part of the save):
 *   data[0] -> header 0x147 (cartridge type: MBC1/2/3+RTC/5/7, HuC1/HuC3, TPP1 = 0xBC, ...)
 *   data[1] -> header 0x149 (RAM size; TPP1 needs 0xC1)
 *   data[2] -> header 0x14A (TPP1 needs 0x65)
 *   data[3] -> header 0x152 (TPP1 RAM size)
 *   data[4] -> hardware model
 * and data[5..] is the battery file, passed with its exact size. */
#include "sameboy_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 5) return -1;
    static uint8_t rom[0x8000];
    memset(rom, 0, sizeof(rom));
    rom[0x100] = 0x00; rom[0x101] = 0xC3; rom[0x102] = 0x50; rom[0x103] = 0x01; /* jp $150 */
    memcpy(rom + 0x134, "MAYHEMFUZZ", 10);
    rom[0x147] = data[0];
    rom[0x149] = data[1];
    rom[0x14A] = data[2];
    rom[0x152] = data[3];
    rom[0x150] = 0x18; rom[0x151] = 0xFE; /* jr -2 */

    GB_gameboy_t *gb = fuzz_gb_new(fuzz_pick_model(data[4]));
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    GB_load_battery_from_buffer(gb, data + 5, size - 5);
    fuzz_run_frames(gb, 1);
    fuzz_gb_free(gb);
    return 0;
}
