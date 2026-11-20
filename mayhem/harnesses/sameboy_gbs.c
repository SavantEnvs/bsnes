/* SameBoy: load a GBS (Game Boy Sound) file, switch tracks and play.
 * Byte 0: model selector + track number, rest: the GBS file. */
#include "sameboy_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    GB_gameboy_t *gb = fuzz_gb_new(data[0] & 1 ? GB_MODEL_CGB_E : GB_MODEL_DMG_B);
    GB_gbs_info_t info;
    if (GB_load_gbs_from_buffer(gb, data + 1, size - 1, &info) == 0) {
        fuzz_run_frames(gb, 4);
        if (info.track_count) {
            GB_gbs_switch_track(gb, (uint8_t)((data[0] >> 1) % info.track_count));
            fuzz_run_frames(gb, 4);
        }
    }
    fuzz_gb_free(gb);
    return 0;
}
