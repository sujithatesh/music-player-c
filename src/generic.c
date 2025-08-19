#define LINUX 1

// NOTE(sujith): this needs to be updated to runtime only on linux
#define GIGABYTE (1000 * 1000)

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

