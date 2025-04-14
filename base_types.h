/* date = March 31st 2025 5:18 pm */

#ifndef BASE_TYPES_H
#define BASE_TYPES_H
#include <sys/mman.h>
#include <stdlib.h>

#include "base_basic_types.h"
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


// NOTE(sujith): Arena Types

typedef struct Arena{
	void* memory;
	U64 capacity;
	U64 used;
} Arena;


Arena
createArena(U64 size){
	void* memory = (void*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	
	U64 used = 0;
	
	return (Arena){.memory = memory, .capacity = size, .used = used};
}
#endif //BASE_TYPES_H
