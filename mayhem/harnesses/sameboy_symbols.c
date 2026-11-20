/* SameBoy: load a debugger symbol file (.sym) and resolve symbols/addresses with it. */
#include "sameboy_common.h"
#include <Core/debugger.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char path[4096];
    if (fuzz_write_tmp(data, size, ".sym", path, sizeof(path)) != 0) return 0;
    GB_gameboy_t *gb = fuzz_gb_new(GB_MODEL_DMG_B);
    fuzz_load_fixed_rom(gb);
    GB_debugger_load_symbol_file(gb, path);
    unlink(path);
    static const uint16_t addrs[] = {0x0000, 0x0100, 0x0150, 0x4000, 0x8000, 0xC000, 0xFF40, 0xFFFF};
    for (unsigned i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        GB_debugger_name_for_address(gb, addrs[i]);
        GB_debugger_describe_address(gb, addrs[i], (uint16_t)-1, false, true);
        GB_debugger_describe_address(gb, addrs[i], 1, true, false);
    }
    /* Look every loaded symbol name up again through the expression evaluator:
     * the first token of each line is the bank:address, the second the name. */
    size_t start = 0;
    unsigned lookups = 0;
    while (start < size && lookups < 32) {
        size_t end = start;
        while (end < size && data[end] != '\n') end++;
        const uint8_t *sp = memchr(data + start, ' ', end - start);
        if (sp) {
            size_t n = (size_t)(data + end - (sp + 1));
            char *name = malloc(n + 1);
            memcpy(name, sp + 1, n);
            name[n] = 0;
            uint16_t result, bank;
            GB_debugger_evaluate(gb, name, &result, &bank);
            free(name);
            lookups++;
        }
        start = end + 1;
    }
    fuzz_run_frames(gb, 1);
    fuzz_gb_free(gb);
    return 0;
}
