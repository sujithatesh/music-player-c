#include "base_basic_types.h"
#include "base_types.h"
#include <stdio.h>
#include <string.h>

#define FILE_PATH "./assets/file.wav"
#define BUFFER_SIZE 20000

int main(void) {
	FILE* wav = fopen(FILE_PATH, "r");
	
	U8 buffer[BUFFER_SIZE];
	U8 c;
	U32 i = 0;
	
	// Reading the whole file
	c = getc(wav);
	while(i < BUFFER_SIZE && c != EOF){
		buffer[i] = c;
		c = getc(wav);
		i++;
	}
	
	// Get silces of width 4
	char window[4];
	char* data = "data";
	
	for(U8 j = 0; j < i; j++){
		window[j%4] = buffer[j];
		if(j % 4 == 3){
			B8 match = 0;
			for(U8 k = 0; k < 4; k++){
				if(window[k] == data[k]){
					match = 1;
				}
			}
			if(match == 1){
				printf("%s", window);
				break;
			}
		}
	}
	
	fclose(wav);
	
	return 0;
}