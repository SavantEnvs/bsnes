# Wraps upstream's bsnes/GNUmakefile (run from bsnes/) to build its libretro core objects only.
include GNUmakefile
mayhem-objects: $(objects)
mayhem-print-objects:
	@echo $(objects)
# The vendored LZMA SDK reads little-endian header fields through unaligned UInt64/UInt32 pointers by
# design (CpuArch.h GetUi64/GetUi32 under MY_CPU_LE_UNALIGN on x86), so UBSan's alignment check fires
# on every 7z archive. Only that check is relaxed, only for obj/lzma.o; the rest of UBSan stays halting.
obj/lzma.o: flags += -fno-sanitize=alignment
