#include <alsa/asoundlib.h>
#include <raylib.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <gtk/gtk.h>

#include "base_basic_types.h"
#include "sound.c"
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
	WaveHeader *header;
	U32 chunk_size;
	pthread_mutex_t mutex;
}AudioContext;

typedef struct {
	U32 hours;
	U8 min;
	U8 sec;
} formatted_time;

char* open_file_dialog() {
	GtkFileChooserNative *dialog;
	gint res;
	char *selected_filename = NULL;
	
	dialog = gtk_file_chooser_native_new("Open File", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
	
	// Add file filters...
	GtkFileFilter *wav_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(wav_filter, "WAV Files");
	gtk_file_filter_add_pattern(wav_filter, "*.wav");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), wav_filter);
	
	// Add file filters...
	GtkFileFilter *all_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(all_filter, "All");
	gtk_file_filter_add_pattern(all_filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);
	
	res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog));
	
	if (res == GTK_RESPONSE_ACCEPT) {
		selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}
	
	g_object_unref(dialog);
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
	return selected_filename;
}

// seconds to min and sec
void get_formatted_time_from_sec(formatted_time *for_time, U32 sec){
	for_time->sec = sec % 60;
	U32 total_minutes = sec / 60;
	for_time->min = total_minutes % 60;
	for_time->hours = total_minutes / 60;
}

// Get total track duration in seconds
double get_track_duration(AudioContext *ctx) {
	return (double)ctx->totalFrames / ctx->header->sampleFreq;
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
	
	return (U32)frames_played / ctx->header->sampleFreq;
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
				ctx->audio_data += (rc * ctx->header->bitsPerSample / 8 * ctx->header->noOfChannels);
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
	// trying to set pywal colors
	char *pywal_colors = "/home/sujith/.cache/wal/colors";
	B8 found_pywal_colors = 0;
	char pywal_background_color[11];
	int pywal_background_color_int = 0;
	if(!access(pywal_colors, F_OK)){
		FILE *pywal = fopen(pywal_colors,"r");
		char Buffer[12] = {0};
		fread(Buffer, 1, sizeof(Buffer), pywal);
		found_pywal_colors = 1;
		pywal_background_color[0]  = '0';
		pywal_background_color[1]  = 'x';
		pywal_background_color[8]  = 'F';
		pywal_background_color[9]  = 'F';
		pywal_background_color[10] = '\0';
		for(int i = 0; i < 6; i++){
			char current_char = Buffer[i + 1];
			pywal_background_color[i + 2] = (current_char >= 'a' && current_char <= 'f') ?
			(current_char - 32) : // Convert to uppercase (e.g., 'a' - 32 = 'A')
			current_char;  ;
		}
		char *endptr; // Pointer to the character after the number
		pywal_background_color_int = strtol(pywal_background_color, &endptr, 16);
		fclose(pywal);
	}
	else{
	}
	
	// TODO(sujith): change this to native dialog
	// open file
	char *file_path;
	if(argc == 1){
		gtk_init(&argc, &argv);
		file_path = open_file_dialog();
	}
	else {
		file_path = argv[1];
	}
	
	// open file in binary mode
	FILE *file = fopen(file_path, "rb");
	fseek(file, 0, SEEK_END);
	U64 file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if(argc == 1)
		g_free(file_path);
	
	// TODO(sujith): remove the WaveHeader and have a generic header(?)
	// init buffer
	U8 Buffer[1000];
	WaveHeader header;
	
	// find out file size and set header filesize
	header.fileSize = file_size;
	fread(Buffer, 1, sizeof(Buffer), file);
	
	// populate header
	file_type file_extension = HeaderSetup(&header, Buffer);
	
	// file descriptor
	U32 fd = fileno(file);
	if(file_extension == WAV_FILE){
		// set the file to 44 <-- NOTE(sujith): this needs to change
		fseek(file, 44, SEEK_SET);
	}
	else {
		return 0;
	}
	
	// reading data via mmap instead of fread
	unsigned char *mapped_data = (unsigned char *)mmap(NULL, header.dataSize, PROT_READ, MAP_PRIVATE, fd, 0);
	
	// pcm_handle and params init
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *params;
	
	// open pcm in non blocking mode
	S32 rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK,SND_PCM_ASYNC);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
	}
	
	// params init
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(pcm_handle, params);
	snd_pcm_hw_params_set_access(pcm_handle, params,SND_PCM_ACCESS_RW_INTERLEAVED);
	
	// setup if wav type is U8 or S16LE -- basically bits per sample
	if (header.bitsPerSample == 8) {
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_U8);
	} else if (header.bitsPerSample == 16) {
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
	} else {
		fprintf(stderr, "Unsupported bits per sample: %d\n", header.bitsPerSample);
		snd_pcm_close(pcm_handle);
	}
	
	//- Setting sample frequency and if track's mode
	snd_pcm_hw_params_set_channels(pcm_handle, params, header.noOfChannels);
	snd_pcm_hw_params_set_rate(pcm_handle, params, header.sampleFreq, 0);
	
	// setting params to pcm_handle
	rc = snd_pcm_hw_params(pcm_handle, params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		snd_pcm_close(pcm_handle);
	}
	
	// init audio_data
	unsigned char *audio_data = 0;
	if(file_extension == WAV_FILE){
		// TODO(sujith): need to calculate header offset
		// init audio_data
		audio_data = mapped_data + 44;
	}
	
	// finding out the remaining frames
	U32 remainingFrames =
		header.dataSize / (header.bitsPerSample / 8 * header.noOfChannels);
	
	
	// Setup AudioContext
	AudioContext audCon = {0};
	audCon.isPaused = 0;
	audCon.isPlaying = 1;
	audCon.should_stop = 0;
	audCon.pcm_handle = pcm_handle;
	audCon.audio_data = audio_data;
	audCon.remainingFrames = remainingFrames;
	audCon.framesWritten = 0;
	audCon.totalFrames = header.dataSize / (header.bitsPerSample / 8 * header.noOfChannels);
	audCon.header = &header;
	
	audCon.chunk_size = header.sampleFreq / 100; // 10ms chunks
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
	// setting current_pos of file to 0
	F32 current_pos = 0;
	
	// no stdout from raylib
	SetTraceLogLevel(LOG_NONE);
	SetConfigFlags(FLAG_VSYNC_HINT);
	
	// --------------------------DRAW CYCLE------------------------------
	InitWindow(1200, 720, "Music Player");
	
	// Load Album art // TODO(sujith): retreive this dynamically and add a fallback image
	Image img = LoadImage("/home/sujith/Music/To Pimp A Butterfly/cover.jpg");
	ImageResize(&img, GetScreenWidth() * 0.1, GetScreenWidth() * 0.1 );
	
	Texture2D texture = LoadTextureFromImage(img);
	UnloadImage(img);
	
	while (!WindowShouldClose()) {
		BeginDrawing();
		// pywal or default color
		Color wal_color = GetColor((found_pywal_colors) ? pywal_background_color_int : 0x6F7587FF);
		ClearBackground(wal_color);
		
		// Draw Album art
		DrawTexture(texture, GetScreenWidth()/2 - 50, GetScreenHeight()/2 - 50, WHITE);
		
		//draw playback position
		current_pos = get_playback_position(&audCon);
		formatted_time current_duration = {0};
		get_formatted_time_from_sec(&current_duration, current_pos);
		DrawText(TextFormat("%02d:%02d / %02d:%02d", current_duration.min, current_duration.sec, total_duration.min, total_duration.sec), 20, 50, 20, RED);
		
		// get current playback pos
		current_pos = get_playback_position(&audCon);
		
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
		
    EndDrawing();
  }
	
	// Cleanup
	audCon.should_stop = 1;
	pthread_join(audio_thread_id, NULL);
	pthread_mutex_destroy(&audCon.mutex);
	snd_pcm_drop(pcm_handle);
	snd_pcm_close(pcm_handle);
	munmap(mapped_data, header.dataSize);
	fclose(file);
	CloseWindow();
	
  return 0;
}
