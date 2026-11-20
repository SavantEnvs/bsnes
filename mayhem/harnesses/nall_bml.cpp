// nall BML (and XML) markup: the parser behind bsnes game manifests, cheat databases and
// settings. Parses the input, queries it the way the cartridge loader does, and re-serializes it.
#include "nall_common.hpp"

static auto walk(const Markup::Node& node, uint depth) -> void {
  if(depth > 32) return;
  for(auto leaf : node) {
    (void)leaf.name(); (void)leaf.text(); (void)leaf.natural(); (void)leaf.boolean();
    walk(leaf, depth + 1);
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if(size < 1) return -1;
  auto text = fuzz_string(data + 1, size - 1);
  if(data[0] & 1) {
    auto document = XML::unserialize(text);
    walk(document, 0);
    return 0;
  }
  auto document = BML::unserialize(text);
  walk(document, 0);
  (void)document["game/board"].text();
  (void)document["game/label"].text();
  for(auto node : document.find("game/board/memory(type=ROM,content=Program)")) (void)node["size"].natural();
  for(auto node : document.find("cartridge/memory")) (void)node["name"].text();
  for(auto node : document.find("cheat")) (void)node["code"].text();
  (void)BML::serialize(document);
  return 0;
}
