#ifndef MODE_ENUM_H
#define MODE_ENUM_H

enum Mode{
  START,
  PAUSE,
  RESUME,
  QUIT
};
#endif

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

typedef struct 
{
	FILE *wav;
	unsigned char *audioBuffer;
	size_t bufferSize;
	snd_pcm_t *pcm_handle;
	WaveHeader* wavHeader;
	U32 mode;
} AudioData;


