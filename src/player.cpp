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
  B32 done = 0;
	snd_pcm_t* pcm_handle = 0;
	
  while(!WindowShouldClose()){
    BeginDrawing();
    ClearBackground(RAYWHITE);
		
    if(IsKeyPressed(KEY_SPACE) && !done){
      done = 1;
      printf("done before :  %d\n", done);
      ClearBackground(GREEN);
      
			pcm_handle = PlaySound(wav, &wavHeader, START, &done);
			
			printf("done after  :  %d\n", done);
    }
		if ((IsKeyPressed(KEY_P))) {
			printf("this is inside pause\n");
			snd_pcm_pause(pcm_handle, 1);
		}
		else if (IsKeyPressed(KEY_R)) {
			printf("this is inside resume\n");
			snd_pcm_pause(pcm_handle, 0);
		}
    else{
      ClearBackground(RAYWHITE);
    }
		
    EndDrawing();
  }
	
  fclose(wav);
	
  return 0;
}
