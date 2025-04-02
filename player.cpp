#include "base_basic_types.h"
#include "base_types.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_PATH "./assets/file.wav"
#define BUFFER_SIZE 20000

typedef struct{
	U8  riffId[4];
	U32 fileSize;
	U8  waveId[4];
	U8  fmtId[4];
	U32 chunkSize;
	U16 typeFmt;
	U16 monoFlag;
	U32 sampleFreq;
	U32 dataRate;
	U32 alignment;
	U16 bps;
	U8  dataId[4];
	U32 dataSize;
} WaveHeader;


U32
FourBit_ASCII_LE(U8* Buffer, U8 Offset){
	return Buffer[Offset] | (Buffer[Offset + 1] << 8) | (Buffer[Offset + 2] << 16) | (Buffer[Offset + 3] << 24);
}

U16
TwoBit_ASCII_LE(U8* Buffer, U8 Offset){
	return Buffer[Offset] | (Buffer[Offset + 1] << 8);
}

void
WaveHeaderSetup(WaveHeader* wavHeader, U8* Buffer){
	// Getting WAVE Header
	memcpy(wavHeader->riffId, Buffer, 4);
	wavHeader->fileSize = FourBit_ASCII_LE(Buffer, 4);
	memcpy(wavHeader->waveId, Buffer + 8, 4);
	memcpy(wavHeader->fmtId, Buffer + 12, 4);
	wavHeader->chunkSize = FourBit_ASCII_LE(Buffer, 16);
	wavHeader->typeFmt = TwoBit_ASCII_LE(Buffer, 20);
	wavHeader->monoFlag = TwoBit_ASCII_LE(Buffer, 22);
	wavHeader->sampleFreq = FourBit_ASCII_LE(Buffer, 24);
	wavHeader->dataRate = FourBit_ASCII_LE(Buffer, 28);
	wavHeader->alignment = TwoBit_ASCII_LE(Buffer, 32);
	wavHeader->bps = TwoBit_ASCII_LE(Buffer, 34);
	memcpy(wavHeader->dataId, Buffer + 36, 4);
	wavHeader->dataSize = FourBit_ASCII_LE(Buffer, 40);
	
}

void
PrintWaveHeader(WaveHeader* wavHeader){
	printf("riffId\t  : %.4s\n", wavHeader->riffId);
	printf("fileSize  : %.4d\n", wavHeader->fileSize);
	printf("waveId\t  : %.4s\n", wavHeader->waveId);
	printf("fmtId \t  : %.4s\n", wavHeader->fmtId);
	printf("chunkSize : %d\n",   wavHeader->chunkSize);
	printf("typeFmt : %d\n",   wavHeader->typeFmt);
	printf("monoFlag : %d\n",   wavHeader->monoFlag);
	printf("sampleFreq : %d\n",   wavHeader->sampleFreq);
	printf("dataRate  : %d\n",   wavHeader->dataRate);
	printf("alignment : %d\n",   wavHeader->alignment);
	printf("bps\t : %d\n",   wavHeader->bps);
	printf("dataId\t : %.4s\n",   wavHeader->dataId);
	printf("dataSize : %d\n",   wavHeader->dataSize);
}

int
main(void) {
	U8 Buffer[BUFFER_SIZE];
	
	WaveHeader wavHeader;
	FILE* wav = fopen(FILE_PATH, "rb");
	
	U64 file_size = fread(Buffer, 1, BUFFER_SIZE, wav);
	
	printf("Read %ld bytes of file\n\n", file_size);
	
	WaveHeaderSetup(&wavHeader, Buffer);
	PrintWaveHeader(&wavHeader);
	
	fseek(wav, 44, SEEK_SET);
	
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *params;
	
	// Open PCM device for playback
	int rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
		return 1;
	}
	
	// Allocate hardware parameters object
	snd_pcm_hw_params_alloca(&params);
	
	// Set hardware parameters
	snd_pcm_hw_params_any(pcm_handle, params);
	snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	
	if (wavHeader.bps == 8) {
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_U8);
	} else if (wavHeader.bps == 16) {
		snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
	} else {
		fprintf(stderr, "Unsupported bits per sample: %d\n", wavHeader.bps);
		snd_pcm_close(pcm_handle);
		return 1;
	}
	
	snd_pcm_hw_params_set_channels(pcm_handle, params, wavHeader.monoFlag);
	snd_pcm_hw_params_set_rate(pcm_handle, params, wavHeader.sampleFreq, 0);
	
	// Write parameters to driver
	rc = snd_pcm_hw_params(pcm_handle, params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		snd_pcm_close(pcm_handle);
		return 1;
	}
	
	// Read and play audio data
	U8 audioBuffer[4096];
	size_t bytesRead;
	
	while ((bytesRead = fread(audioBuffer, 1, sizeof(audioBuffer), wav)) > 0) {
		rc = snd_pcm_writei(pcm_handle, audioBuffer, bytesRead / (wavHeader.bps / 8 * wavHeader.monoFlag)); // corrected division.
		if (rc < 0) {
			fprintf(stderr, "error from snd_pcm_writei: %s\n", snd_strerror(rc));
			break;
		}
	}
	
	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
	fclose(wav);
	
	
	
	return 0;
}