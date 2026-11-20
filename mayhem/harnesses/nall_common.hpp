// Shared include set for the nall/heuristics harnesses: the same single-TU pattern
// bsnes/target-libretro/program.cpp uses (nall headers, then heuristics/*.cpp).
#pragma once
#include <nall/nall.hpp>
#include <nall/decode/zip.hpp>
#include <nall/beat/single/apply.hpp>
#include <nall/beat/single/create.hpp>
using namespace nall;

#include <heuristics/heuristics.hpp>
#include <heuristics/heuristics.cpp>
#include <heuristics/super-famicom.cpp>
#include <heuristics/game-boy.cpp>
#include <heuristics/bs-memory.cpp>
#include <heuristics/sufami-turbo.cpp>

static inline auto fuzz_string(const uint8_t* data, size_t size) -> string {
  return string{string_view{(const char*)data, (uint)size}};
}
