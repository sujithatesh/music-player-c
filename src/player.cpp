#include "base_basic_types.h"
#include "base_types.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

#include <pthread.h>

#include "sound.cpp"
#define FILE_PATH "../assets/file.wav"
#define BUFFER_SIZE 20000

int
main(void) {
  U8 Buffer[BUFFER_SIZE];
  WaveHeader wavHeader;
  FILE* wav = fopen(FILE_PATH, "rb");

  U64 file_size = fread(Buffer, 1, BUFFER_SIZE, wav);
  wavHeader.fileSize = file_size;
  WaveHeaderSetup(&wavHeader, Buffer);
  SetTraceLogLevel(LOG_NONE);

  // --------------------------------------------------------
  InitWindow(1200, 720, "Music Player");
  B32 playing = 0;


  while(!WindowShouldClose()){
    BeginDrawing();
    ClearBackground(RAYWHITE);

    if(IsKeyPressed(KEY_SPACE) && !playing){
      playing = 1;
      printf("playing before :  %d\n", playing);
      ClearBackground(GREEN);
      PlaySound(wav, &wavHeader, START, &playing);
      printf("playing after  :  %d\n", playing);
    }
    else{
      ClearBackground(RAYWHITE);
    }

    EndDrawing();
  }

  fclose(wav);

  return 0;
}
