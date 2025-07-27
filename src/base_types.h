/* date = March 31st 2025 5:18 pm */

#pragma once
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