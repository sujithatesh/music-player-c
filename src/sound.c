typedef struct{
	U32 isPaused;
	U32 isPlaying;
	snd_pcm_t *pcm_handle;
	U8 *audio_data;
	U32 should_stop;
	U32 totalFrames;
	volatile U32 framesWritten;
	U32 remainingFrames;
	WaveHeader *header;
	U32 chunk_size;
	pthread_mutex_t mutex;
}AudioContext;


typedef enum {
	WAV_FILE,
	NOT_SUPPORTED
} file_type;


typedef struct{
	U8 infoBlocId[4];
	U32 infoBlockSize;
	U8 tag1[4];
	U8 *pointer;
} OptionalTags;

U32
FourBit_ASCII_LE(U8* Buffer, U8 Offset){
	return Buffer[Offset] | (Buffer[Offset + 1] << 8) | (Buffer[Offset + 2] << 16) | (Buffer[Offset + 3] << 24);
}

U16
TwoBit_ASCII_LE(U8* Buffer, U8 Offset){
	return Buffer[Offset] | (Buffer[Offset + 1] << 8);
}


void
PrintWaveHeader(WaveHeader* wavHeader){
	printf("fileTypeBlocId\t  : %.4s\n", (char*)wavHeader->fileTypeBlocId);
	printf("fileSize  : %.4d\n", wavHeader->fileSize);
	printf("fileFormatId\t  : %.4s\n", (char*)wavHeader->fileFormatId);
	printf("fmtId \t  : %.4s\n", (char*)wavHeader->fmtId);
	printf("blocSize : %d\n",   wavHeader->blocSize);
	printf("audioFormat   : %d\n",   wavHeader->audioFormat);
	printf("noOfChannels  : %d\n",   wavHeader->noOfChannels);
	printf("sampleFreq : %d\n",  wavHeader->sampleFreq);
	printf("bytePerSec  : %d\n",   wavHeader->bytePerSec);
	printf("bytePerBloc : %d\n",   wavHeader->bytePerBloc);
	printf("bitsPerSample\t : %d\n",       wavHeader->bitsPerSample);
	printf("dataId\t  : %.4s\n", (char*)wavHeader->dataId);
	printf("dataSize  : %d\n",   wavHeader->dataSize);
}


/*
[Master RIFF chunk]
   FileTypeBlocID  (4 bytes) : Identifier « RIFF »  (0x52, 0x49, 0x46, 0x46)
   FileSize        (4 bytes) : Overall file size minus 8 bytes
   FileFormatID    (4 bytes) : Format = « WAVE »  (0x57, 0x41, 0x56, 0x45)

[Chunk describing the data format]
   FormatBlocID    (4 bytes) : Identifier « fmt␣ »  (0x66, 0x6D, 0x74, 0x20)
   BlocSize        (4 bytes) : Chunk size minus 8 bytes, which is 16 bytes here  (0x10)
   AudioFormat     (2 bytes) : Audio format (1: PCM integer, 3: IEEE 754 float)
   NbrChannels     (2 bytes) : Number of channels
   Frequency       (4 bytes) : Sample rate (in hertz)
   BytePerSec      (4 bytes) : Number of bytes to read per second (Frequency * BytePerBloc).
   BytePerBloc     (2 bytes) : Number of bytes per block (NbrChannels * BitsPerSample / 8).
   BitsPerSample   (2 bytes) : Number of bits per sample

[Chunk containing the sampled data]
   DataBlocID      (4 bytes) : Identifier « data »  (0x64, 0x61, 0x74, 0x61)
   DataSize        (4 bytes) : SampledData size
   SampledData
*/
file_type
HeaderSetup(WaveHeader* Header, U8* Buffer){
	// Getting WAVE Header
	memcpy(Header->fileTypeBlocId, Buffer, 4);
	Header->fileSize   = FourBit_ASCII_LE(Buffer, 4);
	memcpy(Header->fileFormatId, Buffer + 8, 4);
	memcpy(Header->fmtId, Buffer + 12, 4);
	Header->blocSize  = FourBit_ASCII_LE(Buffer, 16);
	Header->audioFormat    = TwoBit_ASCII_LE(Buffer, 20);
	Header->noOfChannels   = TwoBit_ASCII_LE(Buffer, 22);
	Header->sampleFreq = FourBit_ASCII_LE(Buffer, 24);
	Header->bytePerSec   = FourBit_ASCII_LE(Buffer, 28);
	Header->bytePerBloc  = TwoBit_ASCII_LE(Buffer, 32);
	Header->bitsPerSample        = TwoBit_ASCII_LE(Buffer, 34);
	memcpy(Header->dataId, Buffer + 36, 4);
	Header->dataSize   = FourBit_ASCII_LE(Buffer, 40);
	
	// TODO(sujith): find a better way to do this
	// verifying wav file format
	if (strncmp((char*)Header->fileTypeBlocId, "RIFF", 4) == 0 &&
			strncmp((char*)Header->fileFormatId, "WAVE", 4) == 0 &&
			strncmp((char*)Header->fmtId, "fmt ", 4) == 0) {
		return WAV_FILE;
	}
	else {
		return NOT_SUPPORTED;
	}
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

F32 
get_playback_position(AudioContext *ctx) {
	snd_pcm_status_t *status;
	snd_pcm_status_alloca(&status);
	
	int err = snd_pcm_status(ctx->pcm_handle, status);
	if (err < 0) {
		fprintf(stderr, "snd_pcm_status failed: %s\n", snd_strerror(err));
		return 0.0f;
	}
	
	// Total frames written to device
	snd_pcm_sframes_t frames_written = ctx->framesWritten;
	
	// Frames still buffered
	snd_pcm_sframes_t delay = snd_pcm_status_get_delay(status);
	
	// Frames already played = written - delay
	snd_pcm_sframes_t frames_played = frames_written - delay;
	if (frames_played < 0) frames_played = 0;
	
	// Clamp against total frames
	float position = (F32)frames_played / ctx->header->sampleFreq;
	float total_duration = (F32)ctx->totalFrames / ctx->header->sampleFreq;
	if (position > total_duration) {
		position = total_duration;
	}
	return position;
}


void
set_playback_position(AudioContext* ctx)
{
	snd_pcm_drop(ctx->pcm_handle);
	snd_pcm_prepare(ctx->pcm_handle);
}