#include "base_basic_types.h"
#include "base_types.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
#include <sys/mman.h>

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
	U32 counter = 0;
	U32 fd = fileno(wav);
	fseek(wav, 44, SEEK_SET);
	
	unsigned char *mapped_data = (unsigned char *)mmap(NULL, wavHeader.dataSize , PROT_READ, MAP_PRIVATE, fd, 0);
	
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *params;
	
	
	S32 rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
	}
	
	snd_pcm_hw_params_alloca(&params);
	
	snd_pcm_hw_params_any(pcm_handle, params);
	snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	
	if (wavHeader.bps == 8)
	{
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_U8);
	} 
	else if (wavHeader.bps == 16) 
	{
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
	} 
	else 
	{
		fprintf(stderr, "Unsupported bits per sample: %d\n", wavHeader.bps);
		snd_pcm_close(pcm_handle);
	}
	
	//- Setting Hardware Params
	snd_pcm_hw_params_set_channels(pcm_handle, params, wavHeader.monoFlag);
	snd_pcm_hw_params_set_rate(pcm_handle, params, wavHeader.sampleFreq, 0);
	
	rc = snd_pcm_hw_params(pcm_handle, params);
	if (rc < 0)
	{
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		snd_pcm_close(pcm_handle);
	}
	
	U32 playing = 0;
	
	unsigned char *audio_data = mapped_data + 44;
	
	U32 remainingFrames =  wavHeader.dataSize / (wavHeader.bps / 8 * wavHeader.monoFlag);
	
  while(!WindowShouldClose()){
    BeginDrawing();
    ClearBackground(RAYWHITE);
		
    if((IsKeyPressed(KEY_SPACE) && !done) || playing){
      done = 1;
			playing = 1;
			
			//PrintWaveHeader(&wavHeader);
			ClearBackground(GREEN);
      
			if (playing == 1) {
				
				rc = snd_pcm_writei(pcm_handle, audio_data, remainingFrames);
				
				if ( rc == -EAGAIN)
					continue;
				
				if (rc < 0) {
					playing = 0;
					printf("rc < 0, snd_pcm_writei faulted\n");
					break;
				}
				
				remainingFrames -= rc;
				audio_data += (rc * wavHeader.bps / 8 * wavHeader.monoFlag);
				
				printf("frameswritten : %d remainingFrames : %d\n", rc, remainingFrames);
				
				if (IsKeyPressed(KEY_P)) {
					printf("this is inside pause\n");
					snd_pcm_pause(pcm_handle, 1);
				}
				else if (IsKeyPressed(KEY_R)) {
					printf("this is inside resume\n");
					snd_pcm_pause(pcm_handle, 0);
				}
			}
		}
		else if(IsKeyPressed(KEY_Q)){
			snd_pcm_drop(pcm_handle);
			snd_pcm_close(pcm_handle);
		}
		else{
			ClearBackground(RAYWHITE);
		}
		counter++;
		if(counter > 1200) counter = 0;
		DrawRectangle(100  + counter,100, 20, 20, RED);
		EndDrawing();
	}
	
	fclose(wav);
	
	return 0;
}
