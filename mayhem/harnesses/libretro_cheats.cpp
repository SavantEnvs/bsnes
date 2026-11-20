// bsnes cheat codes through the libretro API: each input line goes to retro_cheat_set
// (Game Genie / Pro Action Replay / raw address=value decoding in target-libretro/libretro.cpp,
// then the emulator's cheat table), and the fixed ROM runs with the cheats applied.
#include "libretro_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if(load_fixed_rom()) {
    size_t start = 0;
    unsigned index = 0;
    while(start < size && index < 64) {
      size_t end = start;
      while(end < size && data[end] != '\n') end++;
      char* line = (char*)malloc(end - start + 1);
      memcpy(line, data + start, end - start);
      line[end - start] = 0;
      retro_cheat_set(index++, true, line);
      free(line);
      start = end + 1;
    }
    retro_run();
    retro_cheat_reset();
    retro_unload_game();
  }
  empty_scratch();
  return 0;
}
