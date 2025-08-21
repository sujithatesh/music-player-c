#define STRING8(s) (String8){.str = (U8*)s, .size = strlen(s)}

static void 
println(String8 *string){
	if(string->str == 0) return;
	else if(string->size == 0) return;
	
	for(int i = 0; i < string->size; i++){
		if(string->str[i] != '\0') printf("%c", string->str[i]);
	}
	printf("\n");
	return;
}

//static B8 compareStrings(String8 string1, String8 string2){
//if(string1.size != string2.size) return 0;
//for(int i = 0; i < string1.size; i++){
//}
//return 1;
//}

String8 appendStrings(Arena *arena, String8 s1, String8 s2){
	String8* appended_string = arena_alloc(arena, s1.size + s2.size + 1);
	appended_string->str = s1.str;
	for(U32 i = 0; i <s2.size; i++){
		appended_string->str[s1.size + i] = s2.str[i];
	}
	appended_string->size = s1.size + s2.size;
	appended_string->str[appended_string->size] = '\0';
	return *appended_string;
}

U32 
getLengthOfLegacyString(char* str) {
	U32 size = 0;
	for(; str[size]; size++){}
	return size;
}

B8 
compareStringSlice(String8 string1, String8 string2, U32 first, U32 last){
	if(string1.size != string2.size) return 0;
	if(first > last) return -1;
	if(last > string1.size) return -2;
	for(int i = first; i < last; i++){
		if(string1.str[i] != string2.str[i]) return 0;
	}
	return 1;
}


B8 
compareValueStringSlice(U8* value, String8 string_byte, U32 start, U32 end){
	String8 string = {.str = string_byte.str, .size = 8};
	String8 compare_string = {0};
	compare_string.str = value;
	compare_string.size = 8;
	return compareStringSlice(compare_string, string, start, end);
}

//
//static String8 getStringFromPointer(U8 *string){
//String8 result = {0};
//U8 c = 0;
//U32 length = 0;
//for(;;){
//if(string[c] != '\0'){
//
//length++;
//result.size = length;
//}
//else break;
//}
//return result;
//}
//

