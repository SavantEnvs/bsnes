// bsnes cartridge heuristics: what bsnes derives from a ROM image with no manifest
// (board/memory mapping, coprocessor and firmware detection, region, title, manifest),
// following Program::loadSuperFamicom/loadGameBoy/loadBSMemory in
// bsnes/target-libretro/program.cpp step by step (including its size preconditions and
// the slicing of the ROM into program/data/expansion/firmware by the detected sizes),
// then parsing the generated manifest as the cartridge loader does.
// Byte 0 picks the media type, the rest is the ROM image.
#include "nall_common.hpp"

static auto walk(const Markup::Node& node, uint depth) -> void {
  if(depth > 16) return;
  for(auto leaf : node) { (void)leaf.text(); walk(leaf, depth + 1); }
}

static auto parseManifest(const string& manifest) -> void {
  auto document = BML::unserialize(manifest);
  walk(document, 0);
  for(auto memory : document.find("game/board/memory")) {
    (void)memory["type"].text(); (void)memory["size"].natural(); (void)memory["content"].text();
  }
  (void)document["game/board"].text();
}

static auto slice(vector<uint8_t>& rom, uint& offset, uint size) -> void {
  if(!size) return;
  vector<uint8_t> part;
  part.resize(size);
  memory::copy(&part[0], &rom[offset], size);  // as Program::loadSuperFamicom does
  offset += size;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if(size < 1) return -1;
  vector<uint8_t> rom;
  rom.resize(size - 1);
  if(size > 1) memory::copy(rom.data(), data + 1, size - 1);

  switch(data[0] & 3) {
  case 0: {
    if(rom.size() < 0x8000) return -1;  // Program::loadSuperFamicom rejects these
    if((rom.size() & 0x7fff) == 512) {
      memory::move(&rom[0], &rom[512], rom.size() - 512);
      rom.resize(rom.size() - 512);
    }
    auto heuristics = Heuristics::SuperFamicom(rom, "fuzz.sfc");
    (void)heuristics.title();
    (void)heuristics.videoRegion();
    parseManifest(heuristics.manifest());
    uint offset = 0;
    slice(rom, offset, heuristics.programRomSize());
    slice(rom, offset, heuristics.dataRomSize());
    slice(rom, offset, heuristics.expansionRomSize());
    slice(rom, offset, heuristics.firmwareRomSize());
    break;
  }
  case 1:
    if(rom.size() < 0x4000) return -1;  // Program::loadGameBoy
    parseManifest(Heuristics::GameBoy(rom, "fuzz.gb").manifest());
    break;
  case 2:
    if(rom.size() < 0x8000) return -1;  // Program::loadBSMemory
    parseManifest(Heuristics::BSMemory(rom, "fuzz.bs").manifest());
    break;
  case 3:  // target-bsnes loadSufamiTurbo; the heuristic checks its own minimum size
    parseManifest(Heuristics::SufamiTurbo(rom, "fuzz.st").manifest());
    break;
  }
  return 0;
}
