#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

#define RIFF_MAGIC ('R'|('I'<<8)|('F'<<16)|('F'<<24))
#define WAVE_MAGIC ('W'|('A'<<8)|('V'<<16)|('E'<<24))
#define FMT_MAGIC ('f'|('m'<<8)|('t'<<16)|(' '<<24))
#define DATA_MAGIC ('d'|('a'<<8)|('t'<<16)|('a'<<24))

bool LoadStaticSound(char *Filename, Sample_t *Sample)
{
    FILE *stream=NULL;
    uint32_t riff_magic, wave_magic, fmt_magic, data_magic;
    uint16_t format;
    uint16_t channels;
    uint32_t frequency;
    uint16_t sample;
    uint32_t length;
    uint8_t *buffer=NULL;

    if((stream=fopen(Filename, "rb"))==NULL)
        return 0;

    // Header
    fread(&riff_magic, sizeof(uint32_t), 1, stream);    // RIFF magic marker ("RIFF")
    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // File size

    if(riff_magic!=RIFF_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    fread(&wave_magic, sizeof(uint32_t), 1, stream);    // WAVE magic marker ("WAVE")

    if(wave_magic!=WAVE_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    // Wave format block
    fread(&fmt_magic, sizeof(uint32_t), 1, stream);     // Wave format header magic marker ("fmt ")
    if(fmt_magic!=FMT_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // Format header size?

    fread(&format, sizeof(uint16_t), 1, stream);        // wFormatTag
    fread(&channels, sizeof(uint16_t), 1, stream);      // nChannels
    fread(&frequency, sizeof(uint32_t), 1, stream);     // nSamplesPerSec
    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // nAvgBytesPerSec
    fseek(stream, sizeof(uint16_t), SEEK_CUR);          // nBlockAlign
    fread(&sample, sizeof(uint16_t), 1, stream);        // wBitsPerSample

    if(format!=1)
    {
        // Only support PCM streams
        fclose(stream);
        return 0;
    }

    fread(&data_magic, sizeof(uint32_t), 1, stream);    // Data block magic marker ("data")

    if(data_magic!=DATA_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    fread(&length, sizeof(uint32_t), 1, stream);        // Length of data block

    buffer=(uint8_t *)malloc(length);

    if(buffer==NULL)
    {
        fclose(stream);
        return 0;
    }

    fread(buffer, sizeof(uint8_t), length, stream);

    // TODO: Need to covert to match primary buffer here

    Sample->data=buffer;
    Sample->len=length/(sample/8);
    Sample->pos=0;

    return 1;
}
