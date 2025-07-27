/* date = June 25th 2025 7:47 pm */
#pragma once
// NOTE(sujith): this needs to be updated to runtime only on linux
typedef struct timespec mp_time; 
#define GIGABYTE 1000 * 1000

typedef struct{
	void* memory;
	size_t capacity;
	size_t used;
}Arena;

Arena arena_commit(U32 size){
	void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	assert(memory != NULL);
	return (Arena){
		.memory = memory,
		.capacity = size,
		.used = 0
	};
}
void* arena_alloc(Arena* arena, size_t size){
	if(arena->used + size > arena->capacity){
		return NULL;
	}
	void* pointer =  (char*)arena->memory + arena->used;
	arena->used += size;
	return pointer;
}

void arena_reset(Arena* arena){
	arena->capacity = 0;
	arena->used = 0;
	arena->memory = NULL;
}

void arena_destroy(Arena* arena){
	arena_reset(arena);
	munmap(arena, arena->capacity);
}
