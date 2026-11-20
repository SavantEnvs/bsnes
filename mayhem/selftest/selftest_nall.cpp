// Known-answer functional tests for nall (BML, ZIP, BPS) and the bsnes cartridge heuristics,
// built with the project's normal flags. Prints one "PASS <name>" / "FAIL <name>" line per check.
#include "../harnesses/nall_common.hpp"

static int failures;
static auto check(const char* name, bool ok) -> void {
  printf("%s %s\n", ok ? "PASS" : "FAIL", name);
  if(!ok) failures++;
}

int main() {
  // BML
  auto document = BML::unserialize(
    "game\n"
    "  label: Mayhem Test\n"
    "  board: LOROM-RAM\n"
    "    memory type=ROM content=Program\n"
    "      size: 0x80000\n"
    "    memory type=RAM content=Save\n"
    "      size: 0x2000\n");
  check("bml_text", document["game/label"].text() == "Mayhem Test");
  check("bml_board", document["game/board"].text() == "LOROM-RAM");
  check("bml_query", document["game/board/memory(type=RAM)/size"].natural() == 0x2000);
  check("bml_find", document.find("game/board/memory").size() == 2);

  // BPS round trip
  vector<uint8_t> source, target;
  for(uint n : range(4096)) source.append(uint8_t(n * 7));
  target = source;
  for(uint n : range(64)) target[n * 31] ^= 0x5a;
  target.append(0x99);
  auto patch = Beat::Single::create(source, target, "mayhem");
  string manifest, error;
  auto applied = Beat::Single::apply(source, patch, manifest, error);
  check("bps_apply", applied && applied() == target && manifest == "mayhem");
  vector<uint8_t> wrong = source; wrong[0] ^= 1;
  Beat::Single::apply(wrong, patch, manifest, error);
  check("bps_source_crc", error == "warning: source hash mismatch");

  // ZIP round trip (stored member written by nall's encoder, read by the decoder)
  string zipPath = {Path::temporary(), "mayhem-selftest-", (uint)getpid(), ".zip"};
  {
    Encode::ZIP writer(zipPath);
    string text = "the quick brown fox jumps over the lazy dog";
    writer.append("dir/fox.txt", (const uint8_t*)text.data(), text.size());
  }
  Decode::ZIP reader;
  bool zipOk = reader.open(zipPath) && reader.file.size() == 1 && reader.file[0].name == "dir/fox.txt";
  if(zipOk) zipOk = string{reader.extract(reader.file[0])} == "the quick brown fox jumps over the lazy dog";
  reader.close();
  file::remove(zipPath);
  check("zip_roundtrip", zipOk);

  // Super Famicom heuristics: 512 KiB LoROM with a battery-backed 8 KiB SRAM
  vector<uint8_t> rom;
  rom.resize(0x80000);
  memory::fill<uint8_t>(rom.data(), rom.size(), 0x00);
  memory::copy(&rom[0x7fc0], "MAYHEM TEST          ", 21);
  rom[0x7fd5] = 0x20;  // LoROM
  rom[0x7fd6] = 0x02;  // ROM + RAM + battery
  rom[0x7fd7] = 0x09;  // 512 KiB
  rom[0x7fd8] = 0x03;  // 8 KiB SRAM
  rom[0x7fd9] = 0x01;  // North America
  rom[0x7fdc] = 0xff; rom[0x7fdd] = 0xff; rom[0x7fde] = 0x00; rom[0x7fdf] = 0x00;
  rom[0x7ffc] = 0x00; rom[0x7ffd] = 0x80;  // reset vector
  auto sfc = Heuristics::SuperFamicom(rom, "mayhem.sfc");
  check("sfc_title", sfc.title() == "MAYHEM TEST");
  check("sfc_region", sfc.videoRegion() == "NTSC");
  auto sfcManifest = BML::unserialize(sfc.manifest());
  check("sfc_board", sfcManifest["game/board"].text() == "LOROM-RAM#A");
  check("sfc_rom", sfcManifest["game/board/memory(type=ROM)/size"].natural() == 0x80000 && sfc.programRomSize() == 0x80000);
  check("sfc_ram", sfcManifest["game/board/memory(type=RAM)/size"].natural() == 0x2000);

  // Game Boy heuristics: MBC3+RAM+BATTERY
  vector<uint8_t> gb;
  gb.resize(0x8000);
  memory::fill<uint8_t>(gb.data(), gb.size(), 0x00);
  gb[0x147] = 0x13; gb[0x148] = 0x00; gb[0x149] = 0x03;
  auto gbManifest = BML::unserialize(Heuristics::GameBoy(gb, "mayhem.gb").manifest());
  check("gb_board", gbManifest["game/board"].text() == "MBC3");

  return failures ? 1 : 0;
}
