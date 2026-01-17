#ifndef PLAYER_H
#define PLAYER_H

#define FONT_SIZE 23
#define SPALL_BUFFER_SIZE 10 * 1024 * 1024
#define TABLE_SIZE 101
#define SPALL_BEGIN(event_name) \
spall_buffer_begin(&spall_ctx, &spall_buffer, event_name, sizeof(event_name)-1, get_time_in_nanos())
#define SPALL_END() \
spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos())


typedef enum {
	ERROR,
	WARNING,
	SUCESS
} MessageType;

typedef enum {
	NOT_HOVERING = -1
} DummyValue;

typedef struct {
	U32 hours;
	U8 min;
	U8 sec;
} formatted_time;

typedef struct{
	Rectangle rec;
	Color color;
	String8 title;
	B8 is_directory;
}Button;

// djb2 string hash function
U64 
hash(String8 *str) {
	U64 hash = 5381;
	for (U32 i = 0; i < str->size; i++) {
    U32 c = str->str[i];
    hash = ((hash << 5) + hash) + c;
	}
	return hash % TABLE_SIZE;
}

typedef struct string_array_node string_array_node;
struct string_array_node{
	String8 key;
	Array32 arr;
	U32 count;
	string_array_node* next;
};

typedef struct {
	String8* file_paths;
	String8 album_art_path;
} file_info;

#endif //PLAYER_H
