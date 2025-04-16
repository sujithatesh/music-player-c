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


snd_pcm_t*
PlaySound(FILE* wav, WaveHeader* wavHeader, U32 mode, B32* done)
{
  fseek(wav, 44, SEEK_SET);
	
  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *params;
	
	
  S32 rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (rc < 0) {
    fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
  }
	
  snd_pcm_hw_params_alloca(&params);
	
  snd_pcm_hw_params_any(pcm_handle, params);
  snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	
  if (wavHeader->bps == 8)
  {
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_U8);
  } 
  else if (wavHeader->bps == 16) 
  {
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
  } 
  else 
  {
    fprintf(stderr, "Unsupported bits per sample: %d\n", wavHeader->bps);
    snd_pcm_close(pcm_handle);
  }
	
  snd_pcm_hw_params_set_channels(pcm_handle, params, wavHeader->monoFlag);
  snd_pcm_hw_params_set_rate(pcm_handle, params, wavHeader->sampleFreq, 0);
	
  rc = snd_pcm_hw_params(pcm_handle, params);
  if (rc < 0)
  {
    fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
    snd_pcm_close(pcm_handle);
  }
	
  unsigned char audioBuffer[4096]; 
  U32 playing   = 0;
	U32 bytesRead = 0;
	
	if (IsKeyPressed(KEY_SPACE) && (playing == 0 || mode == START) && mode != PAUSE) {
		mode = START;
		playing = 1;
		
		while ((bytesRead = fread(audioBuffer, 1, sizeof(audioBuffer), wav)) > 0) {
			rc = snd_pcm_writei(pcm_handle, audioBuffer,
													bytesRead / (wavHeader->bps / 8 * wavHeader->monoFlag));
			if (rc < 0) {
				printf("rc < 0, snd_pcm_writei faulted\n");
				break;
			}
			
		}
		
  }
	
  *done = 0;
  
	
	
	
	/*snd_pcm_nonblock(pcm_handle, 1);
	snd_pcm_drain(pcm_handle);*/
	printf("this is non-blocking snd_pcm_drain\n");
	
  if(IsKeyPressed(KEY_Q)){
    snd_pcm_close(pcm_handle);
  }
	
	return pcm_handle;
}
