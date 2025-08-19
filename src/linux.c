#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_FILES 1024
#define MAX_PATH 1024

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
		snprintf(fullPath, MAX_PATH, "%s/%s", path.str, dp->d_name);
		
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
