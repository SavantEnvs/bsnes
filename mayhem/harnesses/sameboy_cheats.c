/* SameBoy: cheat codes. Byte 0 picks the mode:
 *   even: every following line is a GameShark/Game Genie code for GB_import_cheat;
 *   odd:  the rest is a SameBoy binary cheat file for GB_load_cheats.
 * The emulator then runs so the imported cheats are applied (GB_apply_cheat). */
#include "sameboy_common.h"
#include <Core/cheats.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    GB_gameboy_t *gb = fuzz_gb_new(GB_MODEL_CGB_E);
    fuzz_load_fixed_rom(gb);
    if ((data[0] & 1) == 0) {
        size_t start = 1;
        unsigned count = 0;
        while (start < size && count < 64) {
            size_t end = start;
            while (end < size && data[end] != '\n') end++;
            char *line = malloc(end - start + 1);
            memcpy(line, data + start, end - start);
            line[end - start] = 0;
            GB_import_cheat(gb, line, "fuzz", true);
            free(line);
            count++;
            start = end + 1;
        }
    }
    else {
        char path[4096];
        if (fuzz_write_tmp(data + 1, size - 1, ".cht", path, sizeof(path)) == 0) {
            GB_load_cheats(gb, path, true);
            unlink(path);
        }
    }
    fuzz_run_frames(gb, 2);
    fuzz_gb_free(gb);
    return 0;
}
