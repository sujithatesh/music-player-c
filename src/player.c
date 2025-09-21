#include <sys/mman.h>
#include <pthread.h>
#include <time.h>

#include "raylib.h"
#include "spall.h"
#include "font_data.h"

#include "base_basic_types.h"
#include "utils.c"
#include "base_types.c"
#include "string_functions.c"
#include "linux.c"
#include "sound.c"
#include "generic.c"

#define FONT_SIZE 23
#define SPALL_BUFFER_SIZE 10 * 1024 * 1024
#define TABLE_SIZE 101

#define SPALL_BEGIN(event_name) \
spall_buffer_begin(&spall_ctx, &spall_buffer, event_name, sizeof(event_name)-1, get_time_in_nanos())

#define SPALL_END() \
spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos())

static SpallProfile spall_ctx;
static SpallBuffer  spall_buffer;

typedef enum {
	ERROR,
	WARNING,
	SUCESS
} MessageType;

typedef struct {
	U32 hours;
	U8 min;
	U8 sec;
} formatted_time;

typedef struct{
	Rectangle rec;
	Color color;
	String8 title;
}Button;

typedef struct Entry Entry;

struct Entry
{
	String8 key;
	U32 *values;
	U32 value_count;
	U32 capacity; 
	Entry *next; 
};

typedef struct {
	Entry* buckets[TABLE_SIZE];
} StringArrayHashMap;

// djb2 string hash function
U64 hash(String8 *str) {
	U64 hash = 5381;
	for (U32 i = 0; i < str->size; i++) {
    U32 c = str->str[i];
    hash = ((hash << 5) + hash) + c;
	}
	return hash % TABLE_SIZE;
}

void insert(Arena* arena, StringArrayHashMap* map, String8* key, size_t capacity) {
	U64 h = hash(key);
	Entry *entry = arena_alloc(arena, sizeof(Entry));
	entry->key = duplicateString(arena, *key);
	entry->values = arena_alloc(arena, capacity * sizeof(U32));
	entry->value_count = 0;
	entry->capacity = capacity;
	entry->next = map->buckets[h];
	map->buckets[h] = entry;
}

B32 
contains(StringArrayHashMap *map, String8 *key) {
	U64 h = hash(key);
	for (Entry *e = map->buckets[h]; e; e = e->next) {
		if (compareStrings(e->key, *key) == 0) {
			return 1; 
		}
	}
	return 0;
}


Entry*
get(StringArrayHashMap *map, String8 *key) {
	U64 h = hash(key);
	for (Entry *e = map->buckets[h]; e; e = e->next) {
		if (compareStrings(e->key, *key) == 0) return e;
	}
	return NULL;
}

U64 get_time_in_nanos(void) {
	struct timespec spec;
	clock_gettime(CLOCK_MONOTONIC, &spec);
	U64 ts = ((U64)spec.tv_sec * 1000000000ull) + (U64)spec.tv_nsec;
	return ts;
}

B8 button_is_hovering(Button *button, F32 mouse_x, F32 mouse_y)
{
	if(mouse_x >= button->rec.x 
		 && mouse_x < button->rec.x + button->rec.width 
		 && mouse_y >= button->rec.y 
		 && mouse_y < button->rec.y + button->rec.height)
	{
		return 1;
	}
	
	return 0;
}

void pause_button_on_click(Button *button, B8* pause_button_clicked, String8 play_pause[])
{
	if(*pause_button_clicked == 0){
		*pause_button_clicked = 1;
		button->title = play_pause[0];
	}
	else if(*pause_button_clicked == 1)
	{
		*pause_button_clicked = 0;
		button->title = play_pause[1];
	}
}

// seconds to min and sec
void get_formatted_time_from_sec(formatted_time *for_time, U32 sec)
{
	for_time->sec = sec % 60;
	U32 total_minutes = sec / 60;
	for_time->min = total_minutes % 60;
	for_time->hours = total_minutes / 60;
}

// Get total track duration in seconds
F64 get_track_duration(AudioContext *ctx) {
	return (F64)ctx->totalFrames / ctx->header->sampleFreq;
}

// NOTE(sujith): this needs to be abstracted away
// get current time in seconds
F64 get_current_time(void)
{
	mp_time current_time;
	clock_gettime(CLOCK_MONOTONIC, &current_time);
	return current_time.tv_sec + current_time.tv_nsec / 1000000000.0;
}
void* audio_thread(void* arg) {
	AudioContext *ctx = (AudioContext*)arg;
	
	// -------------------- Spall setup --------------------
#define AUDIO_THREAD_BUFFER_SIZE (10*1024*1024)
	unsigned char *buffer = malloc(AUDIO_THREAD_BUFFER_SIZE);
	
	SpallBuffer thread_buffer = {
		.pid = 0, // optional, 0 = auto
		.tid = (uint32_t)pthread_self(),
		.length = AUDIO_THREAD_BUFFER_SIZE,
		.data = buffer
	};
	
	spall_buffer_init(&spall_ctx, &thread_buffer);
	
	// -------------------- Thread work --------------------
	while(!ctx->should_stop) {
		spall_buffer_begin(&spall_ctx, &thread_buffer, "audio_loop", sizeof("audio_loop")-1, get_time_in_nanos());
		
		pthread_mutex_lock(&ctx->mutex);
		if (!ctx->isPaused && ctx->remainingFrames > 0) { // checking the frames to write 
			U32 writable_size = (ctx->remainingFrames < ctx->chunk_size) ? ctx->remainingFrames : ctx->chunk_size; 
			S32 rc = PCM_Write(ctx, writable_size); //buffer is full just continue 
			if (rc == -EAGAIN) { // Buffer is full, just continue 
			} 
			else if (rc < 0) { if (rc == -EPIPE) { // Underrun occurred, prepare the PCM 
					printf("Audio underrun occurred, recovering...\n"); PCM_Prepare(ctx); } else { printf("Audio write error: %s\n", snd_strerror(rc)); ctx->isPaused = 1; } } else { // Successfully wrote frames 
				ctx->remainingFrames -= rc; 
				ctx->audio_data += (rc * ctx->header->bitsPerSample / 8 * ctx->header->noOfChannels); // update frames written 
				ctx->framesWritten += rc; 
				if (ctx->remainingFrames <= 0) { ctx->isPlaying = 0; } 
			} 
		} 
		pthread_mutex_unlock(&ctx->mutex);
		
		spall_buffer_end(&spall_ctx, &thread_buffer, get_time_in_nanos());
		usleep(1000);
	}
	
	// Flush and cleanup
	spall_buffer_flush(&spall_ctx, &thread_buffer);
	spall_buffer_quit(&spall_ctx, &thread_buffer);
	free(buffer);
	return NULL;
}

void getTagDetails(U8* value, String8 *current_byte, char **tag_detail, U8 byte_padding_u32)
{
	while(true)
	{
		if(!compareValueStringSlice(value, *current_byte, 0, 4))
		{
			current_byte->str += byte_padding_u32;
		}
		else{
			*tag_detail = (char*)(current_byte->str + byte_padding_u32);
			break;
		}
	}
}

void DrawButtonWithFont(Button *clickable_rec, Color font_color, Font font, U32 font_size, U32 text_offset, B32 override_offset)
{
	Vector2 center = (Vector2){clickable_rec->rec.width / 2, clickable_rec->rec.height / 2};
	if(!override_offset)
	{
		text_offset = (U32)center.x / 2 - clickable_rec->title.size / 2; 
	}
	DrawRectangle(clickable_rec->rec.x, clickable_rec->rec.y, clickable_rec->rec.width + 2, clickable_rec->rec.height, clickable_rec->color);
	DrawTextEx(font, (char*)clickable_rec->title.str, (Vector2){clickable_rec->rec.x + text_offset, clickable_rec->rec.y + clickable_rec->rec.height / 2  - font_size / 2}, font_size, 0, font_color);
}

void DrawTextWithOutlineAndFont(Rectangle rec, Color font_color,Color outline_color, U32 margin, String8 text, Font font, U32 font_size)
{
	DrawRectangleLines(rec.x + margin, rec.y, rec.width - margin, rec.height, outline_color);
	DrawTextEx(font, (char*)text.str, (Vector2){rec.x + margin, rec.y + rec.height / 2  - font_size / 2}, font_size, 0, font_color);
	return;
}

String8 PopPath(Arena *arena, String8 path) 
{
	if (path.size == 0) {
		return path; 
	}
	
	S64 i = path.size - 1;
	while (i >= 0 && path.str[i] != '/' && path.str[i] != '\\') 
	{
		i--;
	}
	
	if (i < 0) {
		return (String8){0};
	}
	
	if (i == 0) {
		return push_str8_copy(arena, (String8){ .str = (U8*)"/", .size = 1 });
	}
	
	String8 result;
	result = push_str8_copy(arena, (String8){ .str = path.str, .size = (U32)i });
	result.size = i;
	return result;
}

void
ToastMessage(String8 message, Font font, Vector2 font_measure)
{
	U32 width          = GetScreenWidth();
	U32 temp_font_size = font_measure.y - 5;
	U32 message_len    = MeasureText((char*)message.str, temp_font_size);
	U32 rec_x          = width - message_len - temp_font_size;
	
	Rectangle rec;
	rec.x      = rec_x;
	rec.y      = 2;
	rec.width  = message_len;
	rec.height = temp_font_size;
	
	DrawRectangleRoundedLines(rec, 0.2, 10, ORANGE);
	DrawTextEx(font, (char*)message.str, (Vector2){rec_x + 4, 3}, temp_font_size, 0, GREEN);
}

file_type
CheckValidWavFile(String8 file_path, B32 *send_toast)
{
	FILE *file = 0;
	file = fopen((char*)file_path.str, "rb");
	
	fseek(file, 0, SEEK_END);
	U64 file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	// TODO(sujith): remove the WaveHeader and have a generic header(?)
	// init buffer
	U8 Buffer[50];
	WaveHeader header;
	
	// find out file size and set header filesize
	header.fileSize = file_size;
	fread(Buffer, 1, sizeof(Buffer), file);
	
	// populate header
	file_type file_extension = HeaderSetup(&header, Buffer);
	
	return file_extension;
}

void DrawFileOpenDialog(String8* file_paths,  U32* file_count, Arena *text_arena, String8 current_directory, Color found_pywal_colors)
{
	SetTraceLogLevel(LOG_NONE);
	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(1200, 720, "File Open Dialog");
	
	Camera2D camera = { 0 };
	camera.offset = (Vector2){ 0, 0};
	camera.offset = (Vector2){ 0, 0};
	camera.rotation = 0.0f;
	camera.zoom = 1.0f;
	
	FileEntry* entries[1024]   = {0};
	U32 entry_count            = 0;
	B32 reload_dir             = 0;
	String8 new_directory      = {0};
	U32 hovering_button        = 0;
	U32 index                  = 0;
	U32* selected_buttons      = 0;
	U32 selected_button_count  = 0;
	B32 multiple_selected      = 0;
	B32 send_toast             = 0;
	S32 timer                  = 2000;
	
	LoadDirectory(text_arena, current_directory, entries, &entry_count, 0);
	Font ui_font = LoadFontFromMemory(".ttf", Helvetica_ttf, Helvetica_ttf_len, 35, NULL, 100);
	Vector2 size = MeasureTextEx(ui_font, "Hello", FONT_SIZE, 0);
	U32 font_size = size.y;
	StringArrayHashMap all_buttons;
	
	while(!WindowShouldClose())
	{
		BeginDrawing();
		BeginMode2D(camera);
		ClearBackground(found_pywal_colors);
		
		Button buttons[entry_count];
		
		Rectangle file_content;
		
		// precaculate
		for(U32 temp = 0; temp < entry_count; temp++)
		{
			file_content.x      = 0;
			file_content.y      = temp * font_size + font_size;
			file_content.width  = GetScreenWidth() - 2 * font_size;
			file_content.height = font_size;
			
			buttons[temp].title = push_str8_copy(text_arena, entries[temp]->name);
			buttons[temp].rec   = file_content;
			buttons[temp].color = found_pywal_colors;
		}
		
		F32 wheelDirection = GetMouseWheelMove();
		Vector2 mouseMoved = GetMouseDelta();
		Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
		B32 mouse_moved = 0;
		
		if(mouseMoved.x != 0 || mouseMoved.y != 0)
		{
			mouse_moved = 1;
		}
		
		// Draw loop
		for(index = 0; index < entry_count; index++)
		{
			DrawButtonWithFont(&buttons[index], RED, ui_font, font_size, 2 * font_size, true);
			if(button_is_hovering(&buttons[index], mouseWorld.x, mouseWorld.y) && mouse_moved){
				hovering_button = index;
			}
		}
		
		DrawTextWithOutlineAndFont(buttons[hovering_button].rec, GREEN, RED, 2 * font_size, buttons[hovering_button].title, ui_font, font_size);
		
		if(timer > 0 && send_toast)
		{
			String8 not_correct_file_type;
			not_correct_file_type.str = arena_alloc(text_arena, 40);
			not_correct_file_type = STRING8("Invalid wav file");
			ToastMessage(not_correct_file_type, ui_font, size);
			timer -= GetFrameTime() * 1000;
		}
		else
		{
			send_toast = false;
			timer = 2000;
		}
		
		for(U32 i = 0; i < selected_button_count; i++)
		{
			DrawTextWithOutlineAndFont(buttons[selected_buttons[i]].rec, RED, WHITE, font_size, buttons[selected_buttons[i]].title, ui_font, font_size);
		}
		
		// event loop
		if((IsKeyPressed(KEY_M)) || IsMouseButtonPressed(1))
		{
			String8 current_file = entries[hovering_button]->full_path;;
			file_type file_extension = CheckValidWavFile(current_file, &send_toast);
			
			Entry* e = get(&all_buttons, &current_directory);
			
			if(e)
			{
				selected_buttons = e->values;
			}
			else
			{
				// Key doesn't exist → create a new entry
				insert(text_arena, &all_buttons, &current_directory, 1024);
				
				// Get the newly inserted entry
				e = get(&all_buttons, &current_directory);
				if(e)
				{
					selected_buttons = e->values;
				}
				else
				{
					// This should never happen, but just in case
					selected_buttons = NULL;
					printf("Error: failed to insert or retrieve key\n");
				}
			}
			
			if(file_extension != WAV_FILE)
			{
				send_toast = 1;
			}
			else
			{
				if(!entries[hovering_button]->is_directory)
				{
					file_paths[selected_button_count] = current_file; 
					selected_buttons[selected_button_count] = hovering_button;
					*file_count = ++selected_button_count;
					multiple_selected = 1;
				}
			}
		}
		
		if(IsMouseButtonPressed(0) || IsKeyPressed(KEY_ENTER))
		{
			if(entries[hovering_button]->is_directory)
			{
				if(hovering_button == 0)
				{
					new_directory = PopPath(text_arena, current_directory);
				} else {
					new_directory = appendStrings(text_arena, current_directory,
																				(String8){.str=(U8*)"/", .size = 1});
					new_directory = appendStrings(text_arena, new_directory,
																				entries[hovering_button]->name);
				}
				reload_dir = 1;
			}
			else {
				String8 current_file = entries[hovering_button]->full_path;
				file_type file_extension = CheckValidWavFile(current_file, &send_toast);
				
				if(file_extension != WAV_FILE)
				{
					send_toast = 1;
				}
				else 
				{
					if(multiple_selected != 1) {
						file_paths[selected_button_count] = entries[hovering_button]->full_path;
						*file_count = ++selected_button_count;
					}
					goto close_modal;
				}
			}
		}
		
		if(IsKeyPressed(KEY_DOWN)) {
			if(hovering_button >= entry_count - 1) 
				hovering_button = entry_count - 1;
			else {
				hovering_button += 1;
			}
		}
		
		if(IsKeyPressed(KEY_UP)) 
		{
			if(hovering_button <= 0) 
				hovering_button = 0;
			else 
			{
				hovering_button -= 1;
			}
		}
		
		if(IsKeyPressed(KEY_Q))
		{
			close_modal:
			break;
		}
		
		int content_height = entry_count * font_size;
		int view_height = GetScreenHeight();
		
		// Only allow scrolling if content is taller than the screen
		if (content_height > view_height) 
		{
			int min_offset = view_height - content_height; // negative value or 0
			int max_offset = 0;
			
			if (wheelDirection > 0) 
			{ // scroll up
				camera.offset.y = (camera.offset.y + font_size > max_offset)
					? max_offset
					: camera.offset.y + font_size;
			}
			else if (wheelDirection < 0) 
			{ // scroll down
				camera.offset.y = (camera.offset.y - font_size < min_offset)
					? min_offset
					: camera.offset.y - font_size;
			}
		}
		else {
			// Content fits inside screen → lock to top
			camera.offset.y = 0;
		}
		
		if (reload_dir) {
			current_directory = new_directory;
			entry_count = 0;
			memset(entries, 0, sizeof(entries));
			LoadDirectory(text_arena, current_directory, entries, &entry_count, 0);
		}
		
		EndMode2D();
		EndDrawing();
	}
	CloseWindow();
}


int main(int argc, char* argv[]) 
{
	if (!spall_init_file("music_player.spall", 1, &spall_ctx)) {
		printf("Failed to setup spall?\n");
		return 1;
	}
	int buffer_size = 1 * 1024 * 1024;
	unsigned char *buffer = malloc(buffer_size);
	spall_buffer = (SpallBuffer)
	{
		.length = buffer_size,
		.data = buffer,
	};
	if (!spall_buffer_init(&spall_ctx, &spall_buffer)) {
		printf("Failed to init spall buffer?\n");
		return 1;
	}
	
	SPALL_BEGIN("event_name?");
	String8 home_dir;
	Arena text_arena = arena_commit(1024 * 1024 * 1024);
	home_dir = STRING8(getenv("HOME"));
	char *pywal_colors = (char*)appendStrings(&text_arena, home_dir, STRING8("/.cache/wal/colors")).str;
	B8 found_pywal_colors = 0;
	char pywal_background_color[11];
	int pywal_background_color_int = 0;
	
	// TODO(sujith): find some other way for this
	if(!access(pywal_colors, F_OK))
	{
		FILE *pywal = fopen(pywal_colors,"r");
		char Buffer[12] = {0};
		fread(Buffer, 1, sizeof(Buffer), pywal);
		found_pywal_colors = 1;
		pywal_background_color[0]  = '0';
		pywal_background_color[1]  = 'x';
		pywal_background_color[8]  = 'F';
		pywal_background_color[9]  = 'F';
		pywal_background_color[10] = '\0';
		
		for(int i = 0; i < 6; i++)
		{
			char current_char = Buffer[i + 1];
			pywal_background_color[i + 2] = (current_char >= 'a' && current_char <= 'f') ?
			(current_char - 32) : current_char;
		}
		char *endptr; // Pointer to the character after the number
		pywal_background_color_int = strtol(pywal_background_color, &endptr, 16);
		fclose(pywal);
	}
	else{
		return -1;
	}
	
	// open file
	String8 file_paths[1024] = {0};
	U32 file_count = 0;
	String8 current_directory = {0};
	
	if(argc == 1)
	{
		current_directory.str = arena_alloc(&text_arena, 1024);
		GetCurrentDirectory(&current_directory);
		DrawFileOpenDialog(file_paths, &file_count, &text_arena, current_directory, GetColor((found_pywal_colors) ? pywal_background_color_int : 0x6F7587FF));
	}
	else if(argc == 2) {
		file_paths[0] = STRING8(argv[1]);
	}
	else {
	}
	
	for(U32 i = 1; i < argc; i++)
	{
		file_paths[i - 1] = STRING8(argv[i]);
		file_count = argc - 1;
	}
	
	FILE *file = 0;
	for(U32 currently_playing = 0; currently_playing < file_count; currently_playing++)
	{
		file = fopen((char*)file_paths[currently_playing].str, "rb");
		
		fseek(file, 0, SEEK_END);
		U64 file_size = ftell(file);
		fseek(file, 0, SEEK_SET);
		
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
		if(file_extension == WAV_FILE)
		{
			// set the file to 44 <-- NOTE(sujith): this needs to change
			fseek(file, 44, SEEK_SET);
		}
		else {
			// TODO(sujith): find a way to add errors
			return 0;
		}
		
		U32 offset = header.dataSize;
		fseek(file, offset, SEEK_SET);
		
		U32 temp = header.fileSize - offset;
		U8 BUFF[temp];
		fread(BUFF, 1, temp, file);
		// reading data via mmap instead of fread
		unsigned char *mapped_data = (unsigned char *)mmap(NULL, header.dataSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
		
		// pcm_handle and params init
		PCM_Handle *pcm_handle = PCM_HandlerSetup(&header);
		
		// init audio_data
		unsigned char *audio_data = 0;
		if(file_extension == WAV_FILE)
		{
			// TODO(sujith): need to calculate header offset
			// init audio_data
			audio_data = mapped_data + 44;
		}
		
		// fetching metadata
		U32 *metadata_start = (U32*)(audio_data + header.dataSize);
		
		String8 first_string_byte = {.str = (U8*)metadata_start, .size = 300};
		B8 compare = compareValueStringSlice((U8*)"LIST", first_string_byte, 0, 4);
		String8 byte_padding = {.str = (U8*)metadata_start + 4, .size = 1};
		U32 byte_padding_u32 = atoi((char*)byte_padding.str);
		
		B8 second_compare = 0;
		String8 second_string_byte = {0};
		char* artist_name = 0;
		char* album_name = 0;
		
		if(compare)
		{
			second_string_byte.str = (first_string_byte.str + byte_padding_u32);
			second_string_byte.size = 8;
			second_compare = compareValueStringSlice((U8*)"INFO", second_string_byte, 0, 4);
			if(!second_compare) goto skip_metadata;
		}
		else goto skip_metadata;
		
		String8 current_byte = {.str = second_string_byte.str + 4, .size = byte_padding_u32};
		// Artist Name
		getTagDetails((U8*)"IART", &current_byte, &artist_name, byte_padding_u32);
		// Album Name
		getTagDetails((U8*)"IPRD", &current_byte, &album_name, byte_padding_u32);
		
		U32 remainingFrames = 0;
		
		skip_metadata :
		// finding out the remaining frames
		remainingFrames =
			header.dataSize / (header.bitsPerSample / 8 * header.noOfChannels);
		
		// Setup AudioContext
		AudioContext audCon = {0};
		audCon.isPaused = 0;
		audCon.isPlaying = 1;
		audCon.should_stop = 0;
		audCon.pcm_handle = pcm_handle;
		audCon.audio_data = (U8*)audio_data;
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
		
		// pause unpause state
		B8 pause_button_clicked = 0;
		String8 play_pause[2] = {STRING8("Play"), STRING8("Pause")};
		
		Font ui_font = LoadFontFromMemory(".ttf", Helvetica_ttf, Helvetica_ttf_len, 35, NULL, 100);
		Vector2 size = MeasureTextEx(ui_font, "Hello", FONT_SIZE, 0);
		U32 font_size = size.y;
		U32 screen_width = GetScreenWidth();
		//U32 screen_height = GetScreenHeight();
		
		// Load Album art 
		/*TODO(sujith): retreive this dynamically and add a fallback image
	 The way I could think of doing this is getting the path of the current file which we are playing
and go to that directory in there we need to search for file with 
i. the name as the album name.
ii.the name as the file name.
 iii. any file with .jpg, .jpeg, .png
*/
		Image img = LoadImage("/home/sujith/Music/To Pimp A Butterfly/cover.jpg");
		ImageResize(&img, GetScreenWidth() * 0.1, GetScreenWidth() * 0.1 );
		
		Texture2D texture = LoadTextureFromImage(img);
		
		UnloadImage(img);
		Vector2 center = {.x = GetScreenWidth() / 2, .y = GetScreenHeight() / 2};
		
		Rectangle pause_rectangle = { 
			.x = center.x,
			.y = center.y,
			.width = 100, 
			.height = 40
		};
		
		Button pause_button = {
			.rec = pause_rectangle, .color = RED, 
			.title = play_pause[1], 
		};
		
		while (!WindowShouldClose())
		{
			BeginDrawing();
			
			DrawFPS(font_size, font_size);
			Vector2 center = {.x = GetScreenWidth() / 2, .y = GetScreenHeight() / 2};
			Vector2 component_center = center;
			component_center.y -=  texture.height;
			
			pause_rectangle.x = (component_center.x - 50);
			pause_rectangle.y = (component_center.y + texture.height + 4 * font_size);
			pause_button.rec = pause_rectangle;
			
			// pywal or default color
			Color wal_color = GetColor((found_pywal_colors) ? pywal_background_color_int : 0x6F7587FF);
			
			ClearBackground(wal_color);
			
			// Draw
			Vector2 time_coords = (Vector2){component_center.x - pause_rectangle.width / 2, component_center.y + texture.height / 2 + 4 * font_size};
			Vector2 line_coords = (Vector2){time_coords.x - 10 * font_size, time_coords.y + 2 * font_size};
			
			// Draw Album art
			DrawTexture(texture, component_center.x - texture.width / 2, component_center.y - texture.height / 2, WHITE);
			
			// Draw Album name
			album_name = album_name ? album_name : "Unknown album";
			Vector2 album_name_length = MeasureTextEx(ui_font, album_name, font_size, 0);
			DrawTextEx(ui_font, album_name, (Vector2){
									 component_center.x - album_name_length.x / 2 + font_size, component_center.y + texture.width / 1.5
								 }, 20, 0, GREEN);
			
			// Draw Artist name
			artist_name = artist_name ? artist_name : "Unknown artist";
			Vector2 artist_name_length = MeasureTextEx(ui_font, artist_name, font_size, 0);
			DrawTextEx(ui_font, artist_name, (Vector2){
									 component_center.x - artist_name_length.x / 2 + font_size, component_center.y + texture.width
								 }, 20, 0, GREEN);
			
			// Draw playback position
			current_pos = get_playback_position(&audCon);
			formatted_time current_duration = {0};
			get_formatted_time_from_sec(&current_duration, current_pos);
			DrawTextEx(ui_font,TextFormat("%02d:%02d / %02d:%02d", current_duration.min,
																		current_duration.sec, total_duration.min, total_duration.sec), (Vector2){
									 time_coords.x, time_coords.y
								 }, 20, 0, RED);
			
			// Draw seeking line
			DrawLine(line_coords.x, line_coords.y, line_coords.x + 25 * font_size, line_coords.y, RED);
			float circle_x = line_coords.x + (25.0f * font_size) * ((float)current_pos / (float)track_duration);
			DrawCircle((int)circle_x, line_coords.y, 5, RED);
			
			// Draw Pause Play button
			DrawButtonWithFont(&pause_button, BLACK, ui_font, font_size, 0, false);
			U32 elapsed = get_playback_position(&audCon);
			U32 total   = audCon.totalFrames / audCon.header->sampleFreq;
			
			if (elapsed >= total)
			{
				PCMDrain(pcm_handle);
				break;
			}
			
			// Draw playlist
			for(U32 i = currently_playing; i < file_count; i++)
			{
				DrawTextEx(ui_font, (char*)file_paths[i].str, (Vector2){screen_width - 20 * font_size, font_size * (i - currently_playing)}, font_size / 4 * 3, 0, GREEN);
			}
			
			// playback_position
			Vector2 mousePosition = GetMousePosition();
			B8 space_pressed = IsKeyPressed(KEY_SPACE);
			if(IsMouseButtonPressed(0) || space_pressed)
			{
				if(button_is_hovering(&pause_button, mousePosition.x, mousePosition.y) || space_pressed)
				{
					pause_button_on_click(&pause_button, &pause_button_clicked, play_pause);
				}
			}
			// alternate is mouse right or space
			// pause the playback
			if (pause_button_clicked == 1)
			{
				pthread_mutex_lock(&audCon.mutex);
				if (audCon.isPaused == 0)
				{
					PCMPause(pcm_handle, 1);
					audCon.isPaused = 1;
				}  
				pthread_mutex_unlock(&audCon.mutex);
			}
			// resume the playback
			if (pause_button_clicked == 0) 
			{
				if (audCon.isPaused == 1)
				{
					pthread_mutex_lock(&audCon.mutex);
					PCMPause(pcm_handle, 0);
					audCon.isPaused = 0;
				}
				pthread_mutex_unlock(&audCon.mutex);
			}
			// quit
			if (IsKeyPressed(KEY_Q)) 
			{
				break;
			} 
			
			// event handling
			EndDrawing();
		}
		
		// Cleanup
		audCon.should_stop = 1;
		pthread_join(audio_thread_id, NULL);
		pthread_mutex_destroy(&audCon.mutex);
		DropPCMHandle(pcm_handle);
		ClosePCMHandle(pcm_handle);
		munmap(mapped_data, header.dataSize);
		fclose(file);
		CloseWindow();
	}
	
	spall_buffer_quit(&spall_ctx, &spall_buffer);
	// Freeing our buffer's memory now that spall_buffer_quit has flushed it for us.
	free(buffer);
	spall_quit(&spall_ctx);
	
	arena_destroy(&text_arena);
	return 0;
}
