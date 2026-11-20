// nall::Decode::ZIP: the ZIP reader bsnes uses for zipped ROMs/patches/states
// (central-directory parser + inflate). Opens the input as an archive and extracts every member.
#include "nall_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  Decode::ZIP archive;
  if(archive.open(data, (uint)size)) {
    for(auto& file : archive.file) {
      auto content = archive.extract(file);
      (void)content;
    }
  }
  archive.close();
  return 0;
}
