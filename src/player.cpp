#include <alsa/asoundlib.h>
#include <raylib.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>

#include "base_basic_types.h"
#include "sound.cpp"
#include "generic.h"

typedef struct{
	U32 isPaused;
	U32 isPlaying;
	snd_pcm_t *pcm_handle;
	U8 *audio_data;
	U32 should_stop;
	U32 totalFrames;
	U32 framesWritten;
	U32 remainingFrames;
	WaveHeader *wavHeader;
	U32 chunk_size;
	pthread_mutex_t mutex;
}AudioContext;

typedef struct {
	U32 hours;
	U8 min;
	U8 sec;
} formatted_time;

// seconds to min and sec
void get_formatted_time_from_sec(formatted_time *for_time, U32 sec){
	for_time->sec = sec % 60;
	U32 total_minutes = sec / 60;
	for_time->min = total_minutes % 60;
	for_time->hours = total_minutes / 60;
}

// Get total track duration in seconds
double get_track_duration(AudioContext *ctx) {
	return (double)ctx->totalFrames / ctx->wavHeader->sampleFreq;
}

// NOTE(sujith): this needs to be abstracted away
// get current time in seconds
U32 get_current_time(){
	mp_time current_time;
	clock_gettime(CLOCK_MONOTONIC, &current_time);
	return current_time.tv_sec + current_time.tv_nsec / 1000000000.0;
}

// get the playback position in seconds
U32 get_playback_position(AudioContext *ctx){
	if (!ctx->isPlaying && ctx->isPaused) return 0;
	
	snd_pcm_sframes_t delay = 0;
	S16 rt = snd_pcm_delay(ctx->pcm_handle, &delay);
	if (rt < 0) delay = 0;
	
	// caculate frames without delay
	U32 frames_played = ctx->framesWritten - ((delay > 0) ? delay : 0);
	
	return (U32)frames_played / ctx->wavHeader->sampleFreq;
}

// audio loop
void* audio_thread(void* arg){
	AudioContext *ctx = (AudioContext*)arg;
	// NOTE(sujith): do we need this should_stop var?
	while(!ctx->should_stop){
		pthread_mutex_lock(&ctx->mutex);
		// if there are frames remaining and track is playing write to buffer
		if (!ctx->isPaused && ctx->remainingFrames > 0) {
			// checking the frames to write
			U32 writable_size = (ctx->remainingFrames < ctx->chunk_size) ? ctx->remainingFrames : ctx->chunk_size;
			S32 rc = snd_pcm_writei(ctx->pcm_handle, ctx->audio_data, writable_size);
			
			//buffer is full just continue
			if (rc == -EAGAIN) {
				// Buffer is full, just continue
			}
			else if (rc < 0) {
				if (rc == -EPIPE) {
					// Underrun occurred, prepare the PCM
					printf("Audio underrun occurred, recovering...\n");
					snd_pcm_prepare(ctx->pcm_handle);
				} else {
					printf("Audio write error: %s\n", snd_strerror(rc));
					ctx->isPaused = 1;
				}
			}
			else {
				// Successfully wrote frames
				ctx->remainingFrames -= rc;
				ctx->audio_data += (rc * ctx->wavHeader->bps / 8 * ctx->wavHeader->monoFlag);
				// update frames written
				ctx->framesWritten += rc;
				if (ctx->remainingFrames <= 0) {
					ctx->isPlaying = 0;
				}
			}
		}
		pthread_mutex_unlock(&ctx->mutex);
		usleep(1000);
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	if(argc < 2){
		printf("usage is ./player <wav_file>\n");
		return 0;
	}
	// init buffer
	U8 Buffer[1000];
	WaveHeader wavHeader;
	
	// open file in binary mode
	FILE *wav = fopen(argv[1], "rb");
	fseek(wav, 0, SEEK_END);
	U64 file_size = ftell(wav);
	fseek(wav, 0, SEEK_SET);
	
	// find out file size and set wavHeader filesize
	wavHeader.fileSize = file_size;
	fread(Buffer, 1, sizeof(Buffer), wav);
	
	// populate wavheader
	WaveHeaderSetup(&wavHeader, Buffer);
	
	// verifying wav file format
	if (strncmp((char*)wavHeader.riffId, "RIFF", 4) != 0 ||
			strncmp((char*)wavHeader.waveId, "WAVE", 4) != 0 ||
			strncmp((char*)wavHeader.fmtId, "fmt ", 4) != 0) {
		fprintf(stderr, "Invalid WAV file format\n");
		fclose(wav);
		return -1;
	}
	
	// no stdout from raylib
	SetTraceLogLevel(LOG_NONE);
	
	// --------------------------DRAW CYCLE------------------------------
	InitWindow(1200, 720, "Music Player");
	
	// for the red square
	U32 counter = 0;
	// file descriptor of wav
	U32 fd = fileno(wav);
	// set the file to 44 <-- NOTE(sujith): this needs to change
	fseek(wav, 44, SEEK_SET);
	
	// reading data via mmap instead of fread
	unsigned char *mapped_data = (unsigned char *)mmap(NULL, wavHeader.dataSize, PROT_READ, MAP_PRIVATE, fd, 0);
	
	// pcm_handle and params init
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *params;
	
	// open pcm in non blocking mode
	S32 rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
	}
	
	// params init
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(pcm_handle, params);
	snd_pcm_hw_params_set_access(pcm_handle, params,SND_PCM_ACCESS_RW_INTERLEAVED);
	
	// setup if wav type is U8 or S16LE -- basically bits per sample
	if (wavHeader.bps == 8) {
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_U8);
	} else if (wavHeader.bps == 16) {
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
	} else {
		fprintf(stderr, "Unsupported bits per sample: %d\n", wavHeader.bps);
		snd_pcm_close(pcm_handle);
	}
	
	//- Setting sample frequency and if track's mode
	snd_pcm_hw_params_set_channels(pcm_handle, params, wavHeader.monoFlag);
	snd_pcm_hw_params_set_rate(pcm_handle, params, wavHeader.sampleFreq, 0);
	
	// setting params to pcm_handle
	rc = snd_pcm_hw_params(pcm_handle, params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		snd_pcm_close(pcm_handle);
	}
	
	// init audio_data
	unsigned char *audio_data = mapped_data + 44;
	
	// finding out the remaining frames
	U32 remainingFrames =
		wavHeader.dataSize / (wavHeader.bps / 8 * wavHeader.monoFlag);
	
	
	// Setup AudioContext
	AudioContext audCon = {0};
	audCon.isPaused = 1;
	audCon.isPlaying = 0;
	audCon.should_stop = 0;
	audCon.pcm_handle = pcm_handle;
	audCon.audio_data = audio_data;
	audCon.remainingFrames = remainingFrames;
	audCon.framesWritten = 0;
	audCon.totalFrames = wavHeader.dataSize / (wavHeader.bps / 8 * wavHeader.monoFlag);
	audCon.wavHeader = &wavHeader;
	
	audCon.chunk_size = wavHeader.sampleFreq / 100; // 10ms chunks
	if (audCon.chunk_size < 64) audCon.chunk_size = 64;   // Minimum chunk size
	if (audCon.chunk_size > 1024) audCon.chunk_size = 1024; // Maximum chunk size
	
	// calling audio
	pthread_mutex_init(&audCon.mutex, NULL);
	
	pthread_t audio_thread_id;
	pthread_create(&audio_thread_id, NULL, audio_thread, &audCon);
	
	// getting total track duration in seconds
	F32 track_duration = get_track_duration(&audCon);
	formatted_time total_duration  = {0}; 
	get_formatted_time_from_sec(&total_duration, track_duration);
	// setting current_pos to 0
	F32 current_pos = 0;
	
	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(GREEN);
		DrawFPS(20, 20);
		
		//draw playback position
		current_pos = get_playback_position(&audCon);
		formatted_time current_duration = {0};
		get_formatted_time_from_sec(&current_duration, current_pos);
		//printf("current_pos : %.1f\n", current_pos);
		DrawText(TextFormat("%d:%d / %d:%d", current_duration.min, current_duration.sec, total_duration.min, total_duration.sec), 20, 50, 20, WHITE);
		
		// starting playback
    if (IsKeyPressed(KEY_SPACE) && !audCon.isPlaying) {
			pthread_mutex_lock(&audCon.mutex);
			audCon.isPaused = 0;
			audCon.isPlaying = 1;
			current_pos = get_playback_position(&audCon);
			pthread_mutex_unlock(&audCon.mutex);
    } 
		
		// pause the playback
		if (IsKeyPressed(KEY_P)) {
			pthread_mutex_lock(&audCon.mutex);
			if (audCon.isPaused == 0) {
				snd_pcm_pause(pcm_handle, 1);
				audCon.isPaused = 1;
			}  
			pthread_mutex_unlock(&audCon.mutex);
		}
		
		// resume the playback
		if (IsKeyPressed(KEY_R)) {
			pthread_mutex_lock(&audCon.mutex);
			snd_pcm_pause(pcm_handle, 0);
			audCon.isPaused = 0;
			pthread_mutex_unlock(&audCon.mutex);
		}
		
		// quit
		if (IsKeyPressed(KEY_Q)) {
			break;
    } 
		
		// Draw a red square going from left to right
    counter++;
    if (counter > 1200)
      counter = 0;
    DrawRectangle(100 + counter, 100, 20, 20, RED);
    EndDrawing();
  }
	
	// Cleanup
	audCon.should_stop = 1;
	pthread_join(audio_thread_id, NULL);
	pthread_mutex_destroy(&audCon.mutex);
	snd_pcm_drop(pcm_handle);
	snd_pcm_close(pcm_handle);
	munmap(mapped_data, wavHeader.dataSize);
	fclose(wav);
	CloseWindow();
	
  return 0;
}