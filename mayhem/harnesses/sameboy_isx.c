/* SameBoy: load an ISX (Intelligent Systems eXecutable) file and run it.
 * GB_load_isx only takes a path, so the input is written to a scratch file. */
#include "sameboy_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char path[4096];
    if (fuzz_write_tmp(data, size, ".isx", path, sizeof(path)) != 0) return 0;
    GB_gameboy_t *gb = fuzz_gb_new(GB_MODEL_CGB_E);
    if (GB_load_isx(gb, path) == 0) {
        fuzz_run_frames(gb, 4);
    }
    unlink(path);
    fuzz_gb_free(gb);
    return 0;
}
