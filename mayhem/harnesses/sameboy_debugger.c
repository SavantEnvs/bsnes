/* SameBoy: the debugger's command language and expression evaluator.
 * Each input line is one debugger command (GB_debugger_execute_command), as typed into
 * a SameBoy frontend console; the emulator runs a frame after each command so
 * breakpoints/watchpoints set by earlier lines are exercised. None of the debugger's
 * commands touch files or stdin (the input callback returns NULL = resume), so all
 * of them are allowed. */
#include "sameboy_common.h"
#include <Core/debugger.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    GB_gameboy_t *gb = fuzz_gb_new(GB_MODEL_CGB_E);
    fuzz_load_fixed_rom(gb);
    fuzz_run_frames(gb, 1);

    size_t start = 0;
    unsigned commands = 0;
    while (start < size && commands < 64) {
        size_t end = start;
        while (end < size && data[end] != '\n') end++;
        char *line = malloc(end - start + 1); /* execute_command destroys its input */
        memcpy(line, data + start, end - start);
        line[end - start] = 0;
        GB_debugger_execute_command(gb, line);
        free(line);
        GB_run(gb);
        commands++;
        start = end + 1;
    }
    fuzz_run_frames(gb, 1);
    fuzz_gb_free(gb);
    return 0;
}
