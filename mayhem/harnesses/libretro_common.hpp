// Shared libretro-frontend setup for the bsnes libretro harnesses: the core is a process-wide
// singleton (retro_init once, as a libretro frontend does); every input loads and fully unloads a
// game, and the scratch directory the core may write save RAM into is emptied after each input,
// so no state carries over between inputs.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../../bsnes/target-libretro/libretro.h"

static char scratch_dir[4096];
static char rom_path[4200];

static bool environment(unsigned cmd, void* data) {
  switch(cmd) {
  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: return true;
  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
  case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    *(const char**)data = scratch_dir;
    return true;
  default: return false;
  }
}
static void video(const void*, unsigned, unsigned, size_t) {}
static void audio(int16_t, int16_t) {}
static size_t audio_batch(const int16_t*, size_t frames) { return frames; }
static void input_poll() {}
static int16_t input_state(unsigned, unsigned, unsigned, unsigned) { return 0; }

static void empty_scratch() {
  DIR* dir = opendir(scratch_dir);
  if(!dir) return;
  while(struct dirent* entry = readdir(dir)) {
    if(entry->d_name[0] == '.') continue;
    char path[8192];
    snprintf(path, sizeof(path), "%s/%s", scratch_dir, entry->d_name);
    unlink(path);
  }
  closedir(dir);
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  const char* tmp = getenv("TMPDIR");
  if(!tmp || !*tmp) tmp = "/tmp";
  snprintf(scratch_dir, sizeof(scratch_dir), "%s/bsnes-fuzz-XXXXXX", tmp);
  if(!mkdtemp(scratch_dir)) abort();
  snprintf(rom_path, sizeof(rom_path), "%s/game.sfc", scratch_dir);
  retro_set_environment(environment);
  retro_set_video_refresh(video);
  retro_set_audio_sample(audio);
  retro_set_audio_sample_batch(audio_batch);
  retro_set_input_poll(input_poll);
  retro_set_input_state(input_state);
  retro_init();
  return 0;
}


// Write a buffer to the scratch ROM path; returns false when it cannot.
static bool write_rom(const uint8_t* data, size_t size) {
  FILE* fp = fopen(rom_path, "wb");
  if(!fp) return false;
  fwrite(data, 1, size, fp);
  fclose(fp);
  return true;
}

// A fixed, valid 32 KiB LoROM image (sei; clc; xce; lda #$0f; sta $2100; bra *) for the
// harnesses whose input is not the ROM itself.
static bool load_fixed_rom() {
  static uint8_t rom[0x8000];
  memset(rom, 0, sizeof(rom));
  static const uint8_t program[] = {0x78, 0x18, 0xFB, 0xA9, 0x0F, 0x8D, 0x00, 0x21, 0x80, 0xFE};
  memcpy(rom, program, sizeof(program));
  memcpy(rom + 0x7fc0, "MAYHEM FIXED ROM     ", 21);
  rom[0x7fd5] = 0x20; rom[0x7fd6] = 0x02; rom[0x7fd7] = 0x05; rom[0x7fd8] = 0x01; rom[0x7fd9] = 0x01;
  rom[0x7fdc] = 0xff; rom[0x7fdd] = 0xff;
  rom[0x7ffc] = 0x00; rom[0x7ffd] = 0x80;
  if(!write_rom(rom, sizeof(rom))) return false;
  retro_game_info game = {};
  game.path = rom_path;
  return retro_load_game(&game);
}
