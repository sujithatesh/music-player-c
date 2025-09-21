#define LINUX 1

// NOTE(sujith): this needs to be updated to runtime only on linux
#define GIGABYTE (1000 * 1000)

#ifdef LINUX
#define PCM_Handle snd_pcm_t
#endif

void GetCurrentDirectory(String8 *current_directory){
	if(LINUX){
		LINUX_get_current_directory(current_directory);
		current_directory->size = getLengthOfLegacyString((char*)current_directory->str);
	}
}

void LoadDirectory(Arena *arena, String8 current_directory, FileEntry **entries, U32 *entry_count, B8 get_full_path){
	memset(entries, 0, *entry_count * sizeof(FileEntry));
	if(LINUX){
		LINUX_load_directory(arena, current_directory, entries, entry_count, get_full_path);
	}
}

PCM_Handle* PCM_HandlerSetup(WaveHeader *header) 
{
	return LINUX_pcm_handler_setup(header);
}

void
DropPCMHandle(PCM_Handle *pcm_handle) 
{
	if(LINUX)
		snd_pcm_drop(pcm_handle);
}

void
ClosePCMHandle(PCM_Handle *pcm_handle)
{
	if(LINUX)
		snd_pcm_close(pcm_handle);
}

void
PCMPause(PCM_Handle *pcm_handle, U32 pause_state) 
{
	if(LINUX)
		snd_pcm_pause(pcm_handle, pause_state);
}

void
PCMDrain(PCM_Handle *pcm_handle) 
{
	if(LINUX)
		snd_pcm_drain(pcm_handle);
}

S32
PCM_Write(AudioContext* ctx, U32 writable_size) 
{
	if(LINUX)
		return LINUX_PCM_Write(ctx->pcm_handle, ctx->audio_data, writable_size);
	return -1;
}

void
PCM_Prepare(AudioContext* ctx){
	if(LINUX)
		LINUX_PCM_Prepare(ctx->pcm_handle);
}
