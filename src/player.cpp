#include "base_basic_types.h"
#include "base_types.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound.cpp"
#define FILE_PATH "../assets/file.wav"
#define BUFFER_SIZE 20000

#define MODE_ENUM_H

#ifndef MODE_ENUM_H
#define MODE_ENUM_H

enum Mode{
  START,
  PAUSE, RESUME
};

#endif // MODE_ENUM_H

int
main(void) {
  U8 Buffer[BUFFER_SIZE];
  WaveHeader wavHeader;
  FILE* wav = fopen(FILE_PATH, "rb");

  U64 file_size = fread(Buffer, 1, BUFFER_SIZE, wav);
  wavHeader.fileSize = file_size;
  WaveHeaderSetup(&wavHeader, Buffer);

  PlaySound(wav, &wavHeader, START);

  return 0;
}
