#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <alsa/asoundlib.h>

#define MAX_FILES 1024
#define MAX_PATH 1024

// WAV HEADER
typedef struct{
	U8  fileTypeBlocId[4];
	U32 fileSize;
	U8  fileFormatId[4];
	U8  fmtId[4];
	U32 blocSize;
	U16 audioFormat;
	U16 noOfChannels;
	U32 sampleFreq;
	U32 bytePerSec;
	U32 bytePerBloc;
	U16 bitsPerSample;
	U8  dataId[4];
	U32 dataSize;
} WaveHeader;

typedef struct timespec mp_time; 

typedef struct{
	String8 name;
	String8 full_path;
	B8 is_directory;
} FileEntry;

U8 
LINUX_load_directory(Arena *arena, String8 path, FileEntry **entries, U32 *entry_count, B8 get_full_path){
	DIR *dir = opendir((char*)path.str);
	if(!dir) return -1;
	
	struct dirent *dp;
	*entry_count = 0;
	
	// TODO(sujith): WTF IS THIS
	// NOTE(sujith): WTF IS THIS
	
	while ((dp = readdir(dir)) != NULL && (*entry_count < MAX_FILES)) {
		if (strcmp(dp->d_name, ".") == 0) continue;
		
		FileEntry *e = arena_alloc(arena, sizeof(FileEntry));
		entries[*entry_count] = e;
		(*entry_count)++;
		
		char fullPath[MAX_PATH];
		snprintf(fullPath, MAX_PATH, "%s/%s", (char*)path.str, (char*)dp->d_name);
		
		struct stat st;
		stat(fullPath, &st);
		
		U32 len = getLengthOfLegacyString(dp->d_name);
		e->name.str = arena_alloc(arena, len + 1);
		copy_memory(e->name.str, dp->d_name, len + 1);
		e->name.size = getLengthOfLegacyString(dp->d_name);
		
		len = getLengthOfLegacyString(fullPath);
		e->full_path.str = arena_alloc(arena, len + 1);
		copy_memory(e->full_path.str, fullPath, len + 1);
		e->full_path.size = getLengthOfLegacyString(fullPath);
		e->is_directory = S_ISDIR(st.st_mode);
	}
	
	closedir(dir);
	return 0;
}

void 
LINUX_get_current_directory(String8 *current_directory){
	getcwd((char*)current_directory->str, 1024);
}

snd_pcm_t*
LINUX_pcm_handler_setup(WaveHeader* header){
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
	if (header->bitsPerSample == 8) {
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_U8);
	} else if (header->bitsPerSample == 16) {
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
	} else {
		fprintf(stderr, "Unsupported bits per sample: %d\n", header->bitsPerSample);
		snd_pcm_close(pcm_handle);
	}
	
	//- Setting sample frequency and if track's mode
	snd_pcm_hw_params_set_channels(pcm_handle, params, header->noOfChannels);
	snd_pcm_hw_params_set_rate(pcm_handle, params, header->sampleFreq, 0);
	
	// setting params to pcm_handle
	rc = snd_pcm_hw_params(pcm_handle, params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		snd_pcm_close(pcm_handle);
	}
	return pcm_handle;
}

U32 
LINUX_PCM_Write(snd_pcm_t* pcm_handle, U8 *audio_data, U32 writable_size)
{
	return snd_pcm_writei(pcm_handle, audio_data, writable_size);
}

void
LINUX_PCM_Prepare(snd_pcm_t* pcm_handle)
{
	snd_pcm_prepare(pcm_handle);
}
