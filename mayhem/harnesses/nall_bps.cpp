// nall::Beat::Single::apply: the BPS patcher bsnes applies to ROMs (target-bsnes program/patch.cpp).
// Byte 0..1 (little endian) is the source length; the next bytes are the source ROM,
// the rest is the BPS patch.
#include "nall_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if(size < 2) return -1;
  size_t sourceSize = data[0] | (data[1] << 8);
  data += 2; size -= 2;
  if(sourceSize > size) sourceSize = size;
  array_view<uint8_t> source{data, sourceSize};
  array_view<uint8_t> patch{data + sourceSize, size - sourceSize};
  string manifest, error;
  auto result = Beat::Single::apply(source, patch, manifest, error);
  (void)result;
  return 0;
}
