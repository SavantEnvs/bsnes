// bsnes .7z ROM archives: the vendored LZMA SDK 7z reader and decoders (7zArcIn, 7zDec, LzmaDec,
// Lzma2Dec, Bcj2, Bra*, Delta) as compiled into bsnes' obj/lzma.o, which the frontends reach
// through LZMA::extract (bsnes/lzma/lzma.cpp) on any game path ending in .7z.
//
// This runs exactly the sequence LZMA::extract runs (file stream -> LookToRead2 -> SzArEx_Open ->
// SzArEx_Extract of the first non-directory entry -> SzArEx_Free), but releases what the upstream
// wrapper never does: it closes the archive file and frees the 256 KiB lookahead buffer. Calling
// LZMA::extract itself in-process leaks one FILE* and 256 KiB per input, so after ~1000 inputs
// every fopen fails with EMFILE and the fuzzer stops reaching the parser at all (0 edges on Mayhem).
// The input is written to a scratch file under $TMPDIR, as the frontends read a path.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lzma/7z.h>
#include <lzma/7zAlloc.h>
#include <lzma/7zCrc.h>
#include <lzma/7zFile.h>

static const size_t kInputBufSize = (size_t)1 << 18;  // as in bsnes/lzma/lzma.cpp

static void extract(const char* path) {
  static bool initialized = false;
  if(!initialized) {
    initialized = true;
    CrcGenerateTable();
  }
  static ISzAlloc allocate = {SzAlloc, SzFree};
  static ISzAlloc allocateTemporary = {SzAllocTemp, SzFreeTemp};

  CFileInStream archive;
  if(InFile_Open(&archive.file, path) != SZ_OK) return;
  FileInStream_CreateVTable(&archive);

  CLookToRead2 look;
  LookToRead2_CreateVTable(&look, False);
  look.buf = (Byte*)ISzAlloc_Alloc(&allocate, kInputBufSize);
  look.bufSize = kInputBufSize;
  look.realStream = &archive.vt;
  LookToRead2_Init(&look);

  CSzArEx db;
  SzArEx_Init(&db);
  if(SzArEx_Open(&db, &look.vt, &allocate, &allocateTemporary) == SZ_OK) {
    for(UInt32 index = 0; index < db.NumFiles; index++) {
      if(SzArEx_IsDir(&db, index)) continue;
      UInt32 blockIndex = (UInt32)-1;
      Byte* filedata = nullptr;
      size_t filesize = 0, offset = 0, count = 0;
      SRes res = SzArEx_Extract(&db, &look.vt, index, &blockIndex, &filedata, &filesize, &offset,
                                &count, &allocate, &allocateTemporary);
      if(res == SZ_OK) {
        // LZMA::extract copies [filedata, filedata + filesize) into its result: touch it the same way.
        volatile uint8_t sink = 0;
        for(size_t i = 0; i < filesize; i += 4096) sink ^= filedata[i];
        (void)sink;
      }
      ISzAlloc_Free(&allocate, filedata);
      if(res == SZ_OK) break;
    }
  }
  SzArEx_Free(&db, &allocate);
  ISzAlloc_Free(&allocate, look.buf);
  File_Close(&archive.file);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const char* tmp = getenv("TMPDIR");
  if(!tmp || !*tmp) tmp = "/tmp";
  char path[4096];
  snprintf(path, sizeof(path), "%s/bsnes-lzma-XXXXXX.7z", tmp);
  int fd = mkstemps(path, 3);
  if(fd < 0) return 0;
  size_t off = 0;
  while(off < size) {
    ssize_t w = write(fd, data + off, size - off);
    if(w <= 0) break;
    off += (size_t)w;
  }
  close(fd);
  extract(path);
  unlink(path);
  return 0;
}
