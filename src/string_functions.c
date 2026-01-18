#define STRING8(s) (String8){.str = (U8*)s, .size = strlen(s)}

static B8 compareStrings(String8 string1, String8 string2) {
	if(string1.size != string2.size) return 0;
	
	for(U32 i = 0; i < string1.size; i++) {
		if(string1.str[i] != string2.str[i]) {
			return 0;
		}
	}
	
	return 1;
}

String8 
duplicateString(Arena* arena, String8 str)
{
	String8 str2 = {0};
	str2.str = arena_alloc(arena, str.size);
	for(U32 i = 0; i < str.size; i++)
	{
		str2.str[i] = str.str[i];
	}
	str2.size = str.size;
	return str2;
}

String8
appendStrings(Arena *arena, String8 s1, String8 s2)
{
	String8 result = {0};
	
	result.size = s1.size + s2.size;
	result.str  = arena_alloc(arena, result.size + 1);
	
	memcpy(result.str, s1.str, s1.size);
	memcpy(result.str + s1.size, s2.str, s2.size);
	
	result.str[result.size] = '\0';
	return result;
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

