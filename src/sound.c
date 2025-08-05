
typedef enum {
	WAV_FILE,
	NOT_SUPPORTED
} file_type;

// WAV HEADER
typedef struct{
	U8  fileTypeBlocId[4];
	U32 fileSize;
	U8  fileFormatId[4];
	U8  fmtId[4];
	U32 blocSize;
	U16 audioFormat;
	U16 noOfChannels;
	U32 sampleFreq;
	U32 bytePerSec;
	U32 bytePerBloc;
	U16 bitsPerSample;
	U8  dataId[4];
	U32 dataSize;
} WaveHeader;

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
	if (strncmp((char*)Header->fileTypeBlocId, "RIFF", 4) == 0 ||
			strncmp((char*)Header->fileFormatId, "WAVE", 4) == 0 ||
			strncmp((char*)Header->fmtId, "fmt ", 4) == 0) {
		return WAV_FILE;
	}
	else {
		return NOT_SUPPORTED;
	}
}

void
PrintWaveHeader(WaveHeader* wavHeader){
	printf("fileTypeBlocId\t  : %.4s\n", wavHeader->fileTypeBlocId);
	printf("fileSize  : %.4d\n", wavHeader->fileSize);
	printf("fileFormatId\t  : %.4s\n", wavHeader->fileFormatId);
	printf("fmtId \t  : %.4s\n", wavHeader->fmtId);
	printf("blocSize : %d\n",   wavHeader->blocSize);
	printf("audioFormat   : %d\n",   wavHeader->audioFormat);
	printf("noOfChannels  : %d\n",   wavHeader->noOfChannels);
	printf("sampleFreq : %d\n",  wavHeader->sampleFreq);
	printf("bytePerSec  : %d\n",   wavHeader->bytePerSec);
	printf("bytePerBloc : %d\n",   wavHeader->bytePerBloc);
	printf("bitsPerSample\t : %d\n",       wavHeader->bitsPerSample);
	printf("dataId\t  : %.4s\n", wavHeader->dataId);
	printf("dataSize  : %d\n",   wavHeader->dataSize);
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
