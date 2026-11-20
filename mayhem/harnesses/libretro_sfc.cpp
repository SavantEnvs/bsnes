// bsnes whole-emulator target through its libretro API: load a Super Famicom ROM image
// (retro_load_game: Program::loadSuperFamicom, heuristics + manifest, cartridge/coprocessor
// setup) and emulate a few frames (CPU/SMP/PPU/DSP and the board's coprocessors).
//

#include "libretro_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if(!write_rom(data, size)) return 0;
  retro_game_info game = {};
  game.path = rom_path;
  if(retro_load_game(&game)) {
    for(int frame = 0; frame < 2; frame++) retro_run();
    retro_unload_game();
  }
  empty_scratch();
  return 0;
}
