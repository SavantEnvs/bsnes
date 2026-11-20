#!/usr/bin/env bash
# mayhem/build.sh — build the bsnes fuzz harnesses, their standalone reproducers and the selftests.
#
# bsnes is built by upstream's GNU makefiles as one big frontend; the parsers the harnesses reach
# are compiled here directly from the same sources with the fuzz flags:
#   * SameBoy (bsnes/gb/Core/*.c) -> libsameboy.a, with the debugger and cheats enabled (upstream's
#     bsnes build defines GB_DISABLE_DEBUGGER/GB_DISABLE_CHEATS; SameBoy's own frontends do not).
#   * nall (header-only) + bsnes/heuristics/*.cpp, compiled into each C++ harness as one TU, the way
#     bsnes/target-libretro/program.cpp includes them, with upstream's -fno-strict-aliasing -fwrapv.
# Outputs go to mayhem/out/ (fuzzers, *-standalone, selftests); objects to mayhem/build/ (both gitignored).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=$(cd "$(dirname "$0")/.." && pwd)}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"

cd "$SRC"
M="$SRC/mayhem"
B="$M/build"
OUT="$M/out"
mkdir -p "$B/fuzz" "$B/sa" "$B/test" "$OUT"

GB_DEFS=(-std=gnu11 -DGB_INTERNAL -D_GNU_SOURCE '-DGB_VERSION="bsnes"' -Wno-multichar -I"$SRC/bsnes/gb")
CXX_DEFS=(-std=c++17 -fno-strict-aliasing -fwrapv -Wno-narrowing -Wno-multichar -Wno-everything
          -I"$SRC" -I"$SRC/bsnes")

# Compile every SameBoy core file into $1 with the given flags, in parallel.
build_sameboy() {
  local dir="$1"; shift
  local pids=() f
  for f in bsnes/gb/Core/*.c; do
    "$CC" "${GB_DEFS[@]}" "$@" -c "$f" -o "$dir/$(basename "${f%.c}").o" &
    pids+=($!)
    if [ "${#pids[@]}" -ge "$MAYHEM_JOBS" ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
  done
  for f in "${pids[@]}"; do wait "$f"; done
  rm -f "$dir/libsameboy.a"
  ar rcs "$dir/libsameboy.a" "$dir"/*.o
}

# 1) Fuzz build: project + harnesses with ASan/UBSan (halting), libFuzzer coverage and DWARF 3.
#    SameBoy only: UBSan's nonnull-attribute check is off. GB_reset (gb.c) runs
#    memset(gb->mbc_ram, 0xFF, gb->mbc_ram_size) with a NULL, zero-sized mbc_ram before any ROM is
#    loaded, so it fires inside GB_init on every input; every other UBSan check stays on and halting.
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1"
COV_FLAGS="-fsanitize=fuzzer-no-link"
GB_RELAX="-fno-sanitize=nonnull-attribute"
build_sameboy "$B/fuzz" $FUZZ_FLAGS $COV_FLAGS $GB_RELAX
# The standalone reproducers get their own objects without SanCov, so they link with no libFuzzer
# runtime (and with no runtime at all when SANITIZER_FLAGS is explicitly empty).
build_sameboy "$B/sa" $FUZZ_FLAGS $GB_RELAX
"$CC" $FUZZ_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"
"$CXX" $FUZZ_FLAGS -c "$M/lsan_off.cc" -o "$B/lsan_off.o"

# The whole emulator, through upstream's own make (target-libretro, objects only: upstream links a
# version-scripted .so with -Wl,--no-undefined, which cannot carry the sanitizer runtimes). Flags are
# passed through the environment so upstream's `flags +=` lines append to them. OpenMP (only used by
# video filters) is off. Objects land in bsnes/obj/ (gitignored upstream); make rebuilds what changed.
LR_COV="${SANITIZER_FLAGS:+$COV_FLAGS}"
LR_MAKE=(make -f "$M/libretro.mk" target=libretro binary=library local=false openmp=false build=stable compiler="$CXX")
LR_OBJS="$(cd bsnes && "${LR_MAKE[@]}" -s mayhem-print-objects)"
(cd bsnes && flags="$FUZZ_FLAGS $LR_COV" "${LR_MAKE[@]}" -j"$MAYHEM_JOBS" mayhem-objects)
LR_OBJS="$(for o in $LR_OBJS; do printf '%s ' "$SRC/bsnes/$o"; done)"

GB_HARNESSES="sameboy_rom sameboy_state sameboy_battery sameboy_gbs sameboy_isx sameboy_debugger sameboy_symbols sameboy_cheats"
NALL_HARNESSES="sfc_heuristics nall_zip nall_bml nall_bps"
# Linked against the libretro core objects (lzma_7z only needs obj/lzma.o, which pulls in emulator code).
LIBRETRO_HARNESSES="libretro_sfc libretro_cheats lzma_7z"

pids=()
for h in $GB_HARNESSES; do
  (
    "$CC" "${GB_DEFS[@]}" $FUZZ_FLAGS $COV_FLAGS $GB_RELAX -c "$M/harnesses/$h.c" -o "$B/$h.o"
    "$CC" $FUZZ_FLAGS $LIB_FUZZING_ENGINE "$B/$h.o" "$B/lsan_off.o" "$B/fuzz/libsameboy.a" -lm -o "$OUT/$h"
    "$CC" "${GB_DEFS[@]}" $FUZZ_FLAGS $GB_RELAX -c "$M/harnesses/$h.c" -o "$B/$h.sa.o"
    "$CXX" $FUZZ_FLAGS "$B/$h.sa.o" "$B/standalone_main.o" "$B/lsan_off.o" "$B/sa/libsameboy.a" -lm -o "$OUT/$h-standalone"
  ) &
  pids+=($!)
done
for h in $NALL_HARNESSES; do
  (
    "$CXX" "${CXX_DEFS[@]}" $FUZZ_FLAGS $COV_FLAGS -c "$M/harnesses/$h.cpp" -o "$B/$h.o"
    "$CXX" $FUZZ_FLAGS $LIB_FUZZING_ENGINE "$B/$h.o" "$B/lsan_off.o" -ldl -o "$OUT/$h"
    "$CXX" "${CXX_DEFS[@]}" $FUZZ_FLAGS -c "$M/harnesses/$h.cpp" -o "$B/$h.sa.o"
    "$CXX" $FUZZ_FLAGS "$B/$h.sa.o" "$B/standalone_main.o" "$B/lsan_off.o" -ldl -o "$OUT/$h-standalone"
  ) &
  pids+=($!)
done
for h in $LIBRETRO_HARNESSES; do
  (
    "$CXX" "${CXX_DEFS[@]}" $FUZZ_FLAGS $LR_COV -c "$M/harnesses/$h.cpp" -o "$B/$h.o"
    "$CXX" $FUZZ_FLAGS $LIB_FUZZING_ENGINE "$B/$h.o" "$B/lsan_off.o" $LR_OBJS -ldl -lpthread -o "$OUT/$h"
    "$CXX" $FUZZ_FLAGS "$B/$h.o" "$B/standalone_main.o" "$B/lsan_off.o" $LR_OBJS -ldl -lpthread -o "$OUT/$h-standalone"
  ) &
  pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done

# 2) Selftests (the functional oracle run by mayhem/test.sh), with normal flags.
TEST_FLAGS="-O1 $COVERAGE_FLAGS"
build_sameboy "$B/test" $TEST_FLAGS
"$CC" "${GB_DEFS[@]}" $TEST_FLAGS "$M/selftest/selftest_gb.c" "$B/test/libsameboy.a" -lm -o "$OUT/selftest_gb"
"$CXX" "${CXX_DEFS[@]}" $TEST_FLAGS "$M/selftest/selftest_nall.cpp" -ldl -o "$OUT/selftest_nall"

echo "built: $(ls "$OUT" | tr '\n' ' ')"
