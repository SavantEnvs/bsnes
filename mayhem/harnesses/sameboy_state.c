/* SameBoy: restore a save state (native SameBoy and BESS formats) and keep running.
 * Reaches save_state.c (section/BESS block readers, sanitize_state) and the emulator
 * running on the restored state. */
#include "sameboy_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    GB_model_t model = GB_MODEL_DMG_B;
    /* Pick the model the state was made with when it says so (as frontends do). */
    if (GB_get_state_model_from_buffer(data, size, &model) != 0) model = GB_MODEL_DMG_B;
    GB_gameboy_t *gb = fuzz_gb_new(model);
    fuzz_load_fixed_rom(gb);
    if (GB_load_state_from_buffer(gb, data, size) == 0) {
        fuzz_run_frames(gb, 3);
    }
    fuzz_gb_free(gb);
    return 0;
}
