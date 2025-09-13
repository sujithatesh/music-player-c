#include <assert.h>

typedef struct String8{
	U8 *str; U64 size;
} String8;

typedef struct String8Node{
	struct String8Node *next;
	String8 string;
} String8Node;

typedef struct String8List{
	String8Node *first;
	String8Node *last;
	U64 node_count;
	U64 total_size;
} String8List;

typedef struct StringJoin{
	String8 pre;
	String8 mid;
	String8 post;
} StringJoin;

typedef U32 StringMatchFlags;
enum{
	StringMatchFlag_NoCase = 1 << 0
};

typedef struct String16{
	U16 *str;
	U64 size;
} String16;

typedef struct String32{
	U32 *str;
	U64 size;
} String32;

typedef struct StringDecode{
	U32 codepoint;
	U32 size;
} StringDecode;

typedef struct{
	void* memory;
	size_t capacity;
	size_t used;
} Arena;


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

void arena_clear(Arena* arena){
	arena->used = 0;
}

void arena_destroy(Arena* arena){
	arena_clear(arena);
	munmap(arena->memory, arena->capacity);
} 

String8 push_str8_copy(Arena *arena, String8 src) {
	if (arena->used + src.size + 1 > arena->capacity) {
		return (String8){0};
	}
	
	char *dst = (char*)arena->memory + arena->used;
	copy_memory(dst, src.str, src.size);
	dst[src.size] = '\0';
	
	arena->used += src.size + 1;
	
	return (String8){
		.str = (U8*)dst,
		.size = src.size,
	};
}

