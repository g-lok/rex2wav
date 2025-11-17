
#include <stdio.h>
#include <assert.h>

#include "Wav.h"

/* Functions local to this file*/
void PackPCM(WAVE_PCM_FORMAT_CHUNK* chunk,uint8_t binary[]);
void PackRIFF(RIFF_FORM_CHUNK* riff,uint8_t binary[]);

uint32_t WritePCMFormatChunk(FILE* file,uint32_t channels, uint32_t sampleSize, uint32_t rate);
uint32_t WriteSoundDataChunk(FILE* file,uint32_t wordCount, uint32_t numChannels, uint32_t sampleSize,float* buffers[2]);
uint32_t WriteCueChunk(FILE* file, WaveCuePoint* cuePoints, uint32_t numCues);

void Pack32BitUnsignedLittle(uint8_t binary[], uint32_t v);
void Pack16BitUnsignedLittle(uint8_t binary[], uint16_t v);

int16_t ScaleAndClip(float f);


/*
	Pack16BitUnsignedLittle,
*/
void Pack16BitUnsignedLittle(uint8_t binary[], uint16_t v){
	binary[0]=(uint8_t)(v);
	binary[1]=(uint8_t)(v >> 8);
}

/*
	Pack32BitUnsignedLittle,
*/
void Pack32BitUnsignedLittle(uint8_t binary[], uint32_t v){
	binary[0]=(uint8_t)(v);
	binary[1]=(uint8_t)(v >> 8);
	binary[2]=(uint8_t)(v >> 16);
	binary[3]=(uint8_t)(v >> 24);
}


/*
	WriteWave,
	WriteWave takes buffers to sample data, an open FILE stream and some parameters.
	It limits the input data and writes a wav-format file to the file stream.
*/
uint32_t WriteWave(FILE* file,uint32_t wordCount, uint32_t numChannels, uint32_t sampleSize,uint32_t sampleRate,float* buffers[2]) {

	uint32_t totalSize=0;

	uint8_t riffFormatImage[RIFF_IMAGE_SIZE];
	RIFF_FORM_CHUNK riff;
	long riffPos=ftell(file);

	/* RIFF chunk*/
	riff.ckID=WAVE_RIFF_ID;
	riff.ckSize=0;
	PackRIFF(&riff,riffFormatImage);
	fwrite(riffFormatImage,RIFF_IMAGE_SIZE,1,file);
	{
		/* Wave head */
		uint8_t waveID[4]; 
		Pack32BitUnsignedLittle(waveID,WAVEID);
		fwrite(waveID,4,1,file);
		totalSize+=4;
		/* fmt chunk */
		{
			totalSize+=WritePCMFormatChunk(file,numChannels,sampleSize,sampleRate);
		}
		/* data chunk */
		{
			totalSize+=WriteSoundDataChunk(file,wordCount,numChannels,sampleSize,buffers);
		}
	}

	fseek(file,riffPos,SEEK_SET);
	riff.ckSize=totalSize;
	PackRIFF(&riff,riffFormatImage);
	fwrite(riffFormatImage,RIFF_IMAGE_SIZE,1,file);

	return(0);
}

/*
	WriteWaveWithCues,
	WriteWaveWithCues takes buffers to sample data, an open FILE stream, some parameters,
	and an array of cue points. It writes a wav-format file with cue markers.
*/
uint32_t WriteWaveWithCues(FILE* file,uint32_t wordCount, uint32_t numChannels, uint32_t sampleSize,uint32_t sampleRate,float* buffers[2], WaveCuePoint* cuePoints, uint32_t numCues) {

	uint32_t totalSize=0;

	uint8_t riffFormatImage[RIFF_IMAGE_SIZE];
	RIFF_FORM_CHUNK riff;
	long riffPos=ftell(file);

	/* RIFF chunk*/
	riff.ckID=WAVE_RIFF_ID;
	riff.ckSize=0;
	PackRIFF(&riff,riffFormatImage);
	fwrite(riffFormatImage,RIFF_IMAGE_SIZE,1,file);
	{
		/* Wave head */
		uint8_t waveID[4]; 
		Pack32BitUnsignedLittle(waveID,WAVEID);
		fwrite(waveID,4,1,file);
		totalSize+=4;
		/* fmt chunk */
		{
			totalSize+=WritePCMFormatChunk(file,numChannels,sampleSize,sampleRate);
		}
		/* data chunk */
		{
			totalSize+=WriteSoundDataChunk(file,wordCount,numChannels,sampleSize,buffers);
		}
		/* cue chunk */
		if (numCues > 0) {
			totalSize+=WriteCueChunk(file,cuePoints,numCues);
		}
	}

	fseek(file,riffPos,SEEK_SET);
	riff.ckSize=totalSize;
	PackRIFF(&riff,riffFormatImage);
	fwrite(riffFormatImage,RIFF_IMAGE_SIZE,1,file);

	return(0);
}

/*
	WriteCueChunk,
	Writes cue point chunk to file.
*/
uint32_t WriteCueChunk(FILE* file, WaveCuePoint* cuePoints, uint32_t numCues) {
	uint8_t chunkHeader[8];
	long startPos = ftell(file);
	uint32_t chunkSize = 4 + (numCues * 24); /* 4 bytes for count + 24 bytes per cue point */
	
	/* Write cue chunk header */
	Pack32BitUnsignedLittle(&chunkHeader[0], WAVE_CUE_ID);
	Pack32BitUnsignedLittle(&chunkHeader[4], chunkSize);
	fwrite(chunkHeader, 8, 1, file);
	
	/* Write number of cue points */
	uint8_t numCuesData[4];
	Pack32BitUnsignedLittle(numCuesData, numCues);
	fwrite(numCuesData, 4, 1, file);
	
	/* Write each cue point */
	for (uint32_t i = 0; i < numCues; i++) {
		uint8_t cuePointData[24];
		
		/* Cue point ID */
		Pack32BitUnsignedLittle(&cuePointData[0], i + 1);
		/* Position (play order position) */
		Pack32BitUnsignedLittle(&cuePointData[4], i);
		/* Data chunk ID ('data') */
		Pack32BitUnsignedLittle(&cuePointData[8], WAVE_SOUND_DATA_ID);
		/* Chunk start (byte offset of data chunk, 0 for uncompressed WAV) */
		Pack32BitUnsignedLittle(&cuePointData[12], 0);
		/* Block start (byte offset to sample of First Channel, 0 for uncompressed WAV) */
		Pack32BitUnsignedLittle(&cuePointData[16], 0);
		/* Sample offset (sample frame offset) */
		Pack32BitUnsignedLittle(&cuePointData[20], cuePoints[i].position);
		
		fwrite(cuePointData, 24, 1, file);
	}
	
	long endPos = ftell(file);
	return (uint32_t)(endPos - startPos);
}

/*
	ScaleAndClip,
	Scales and clips float sample data. 	
*/
int16_t ScaleAndClip(float f){

	if (f>=1.0) {
		return(32767);
	} else if (f<=-1.0) {
		return(-32768);
	} else {
		return ((int16_t) ( 32767.0 * f));
	}

}

/*
	WriteSoundDataChunk,
	Writes sound data chunk to file. 	
*/
uint32_t WriteSoundDataChunk(FILE* file,uint32_t wordCount, uint32_t numChannels, uint32_t sampleSize,float* buffers[2]){
	
	uint8_t dataImage[RIFF_IMAGE_SIZE];
	RIFF_FORM_CHUNK data;
	uint32_t pos=0;
	long endPos=0;
	long dataSize=0;

	long dataPos=ftell(file);
	assert(sampleSize==16);

	data.ckID=WAVE_SOUND_DATA_ID;
	data.ckSize=0;
	PackRIFF(&data,dataImage);
	fwrite(dataImage,RIFF_IMAGE_SIZE,1,file);
	

	if (numChannels==1) {
		float* left=buffers[0];
		for (pos=0;pos<wordCount;pos++) {
			uint8_t sample[2];
			int16_t temp=ScaleAndClip(*left++);
			Pack16BitUnsignedLittle(sample,temp);
			fwrite(sample,sizeof(int16_t),1,file);
		}
	} else if (numChannels==2) {
		float* left=buffers[0];
		float* right=buffers[1];

		for (pos=0;pos<wordCount;pos++) {
			uint8_t sample[2];
			int16_t temp=(int16_t) ScaleAndClip(*left++);
			Pack16BitUnsignedLittle(sample,temp);
			fwrite(sample,sizeof(int16_t),1,file);
			temp=(int16_t) ScaleAndClip(*right++);
			Pack16BitUnsignedLittle(sample,temp);
			fwrite(sample,sizeof(int16_t),1,file);
		}
	}

	endPos=ftell(file);

	fseek(file,dataPos,SEEK_SET);
	/* ckSize only includes chunk size, not chunk header size */
	dataSize=endPos-dataPos-8;  
	data.ckSize=(int32_t)dataSize;
	assert(dataSize == (long)data.ckSize);
	PackRIFF(&data,dataImage);
	fwrite(dataImage,RIFF_IMAGE_SIZE,1,file);
	fseek(file,endPos,SEEK_SET);

	return (uint32_t)(endPos-dataPos);
}

/*
	WritePCMFormatChunk,
	Writes PCM format chunk to file. It has a fixed size. 	
*/
uint32_t WritePCMFormatChunk(FILE* file,uint32_t channels, uint32_t sampleSize, uint32_t rate) {

	WAVE_PCM_FORMAT_CHUNK PCMFormat;
	uint8_t pcmFormatImage[IMAGE_SIZE];

	assert(channels > 0);
	assert(sampleSize > 0);
	assert(rate > 0);

	PCMFormat.ckID=WAVE_FORMAT_ID;
	PCMFormat.ckSize=16;

	PCMFormat.formatTag = WAVE_FORMAT_PCM_ID;
	PCMFormat.channels = (uint16_t)channels;
	PCMFormat.samplesPerSec = rate;
	PCMFormat.avgBytesPerSec = channels * rate * sampleSize / 8;
	PCMFormat.blockAlign = (uint16_t)(channels * sampleSize / 8);
	PCMFormat.bitsPerSample = (uint16_t)sampleSize;

	PackPCM(&PCMFormat,pcmFormatImage);
	fwrite(pcmFormatImage,IMAGE_SIZE,1,file);

	return(IMAGE_SIZE);

}

/*
	PackPCM,
*/
void PackPCM(WAVE_PCM_FORMAT_CHUNK* chunk,uint8_t binary[]) {
	Pack32BitUnsignedLittle(&binary[IMAGE_CKID_TAG],chunk->ckID);
	Pack32BitUnsignedLittle(&binary[IMAGE_CKSIZE_TAG],chunk->ckSize);
	Pack16BitUnsignedLittle(&binary[IMAGE_FORMAT_TAG],chunk->formatTag);
	Pack16BitUnsignedLittle(&binary[IMAGE_CHANNELS],chunk->channels);
	Pack32BitUnsignedLittle(&binary[IMAGE_SAMPLES_PER_SEC],chunk->samplesPerSec);
	Pack32BitUnsignedLittle(&binary[IMAGE_AVG_BYTES_PER_SEC],chunk->avgBytesPerSec);
	Pack16BitUnsignedLittle(&binary[IMAGE_BLOCK_ALIGN],chunk->blockAlign);
	Pack16BitUnsignedLittle(&binary[IMAGE_BITS_PER_SAMPLE],chunk->bitsPerSample);
}

/*
	PackRIFF,
*/
void PackRIFF(RIFF_FORM_CHUNK* riff,uint8_t binary[]){
	Pack32BitUnsignedLittle(&binary[IMAGE_CKID_TAG],riff->ckID);
	Pack32BitUnsignedLittle(&binary[IMAGE_CKSIZE_TAG],riff->ckSize);
}


/*
	WriteTxt,
	WriteTxt takes buffers to sample data, an open FILE stream and some parameters.
	It writes the float sample values to the file stream.
	
	Format is:
	
	number of channels
	sample rate
	number of frames
	sample 1
	sample 2
	sample 3
	.
	.
	.
	
	The samples are written interleaved and
		if mono (i.e number of channels is 1) the number of samples equals number of frames.
		If stereo (i.e number of channels is 2) the number of samples equals 2*numberOfFrames.
	
*/
void WriteTxt(FILE* file, uint32_t numberOfFrames, uint32_t numChannels, uint32_t sampleRate, float* buffers[2]) {

	fprintf(file, "%d\n", numChannels);
	fprintf(file, "%d\n", sampleRate);
	fprintf(file, "%d\n", numberOfFrames);
	
	for(uint32_t i=0; i<numberOfFrames; i++) {
		if(numChannels == 1) {
			fprintf(file, "%f\n", buffers[0][i]);
		}
		else {
			assert(numChannels==2);
			fprintf(file, "%f\n", buffers[0][i]);
			fprintf(file, "%f\n", buffers[1][i]);
		}
	}
}
