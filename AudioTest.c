#include <portaudio.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <memory.h>

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

#define MAX_CHANNELS 32

typedef struct
{
    uint8_t *data;
    uint32_t pos, len;
} Sample_t;

Sample_t channels[MAX_CHANNELS];

Sample_t TestSound1;
Sample_t TestSound2;
Sample_t TestSound3;
Sample_t TestSound4;

void Play_Sound(Sample_t *Sample)
{
    int32_t index;

    /* Look for an empty sound channel slot */
    for(index=0;index<MAX_CHANNELS;index++)
    {
        if(channels[index].pos==channels[index].len)
            break;
    }

    if(index==MAX_CHANNELS)
        return;

    channels[index].data=Sample->data;
    channels[index].len=Sample->len;
    channels[index].pos=0;
}

#define RIFF_MAGIC ('R'|('I'<<8)|('F'<<16)|('F'<<24))
#define WAVE_MAGIC ('W'|('A'<<8)|('V'<<16)|('E'<<24))
#define FMT_MAGIC ('f'|('m'<<8)|('t'<<16)|(' '<<24))
#define DATA_MAGIC ('d'|('a'<<8)|('t'<<16)|('a'<<24))

unsigned int LoadStaticSound(char *Filename, Sample_t *Sample)
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

	// Need to covert to match primary buffer here

    Sample->data=buffer;
    Sample->len=length;
    Sample->pos=0;

    return 1;
}

int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;
    float *out=(float *)outputBuffer;
    int active_channels=0;

    memset(out, 0, framesPerBuffer*sizeof(float)*2);

    for(int32_t i=0;i<MAX_CHANNELS;i++)
    {
        Sample_t *channel=&channels[i];

        if(channel->pos==channel->len)
            continue;

        active_channels++;
    }

    for(int32_t i=0;i<MAX_CHANNELS;i++)
    {
        Sample_t *channel=&channels[i];
        int32_t amount=(channel->len-channel->pos);

        // channel is empty, skip
        if(channel->pos==channel->len)
            continue;

        fprintf(stderr, "Playing channel %d %d\n", i, amount);

        // ran off end of primary buffer
        if(amount>framesPerBuffer)
            amount=framesPerBuffer;

        // data to copy
        for(int32_t j=0;j<amount;j++)
        {
            (*out++)+=((float)channel->data[channel->pos+j]/255.0f)/active_channels;
            (*out++)+=((float)channel->data[channel->pos+j]/255.0f)/active_channels;
        }

        channel->pos+=amount;
        out=(float *)outputBuffer;
    }

    return paContinue;
}

int main(int argc, char **argv)
{
    int Done=0;
    PaStreamParameters outputParameters;
    PaStream *stream;

    if(Pa_Initialize()!=paNoError)
        return -1;

    outputParameters.device=Pa_GetDefaultOutputDevice();

    if(outputParameters.device==paNoDevice)
    {
        fprintf(stderr, "Error: No default output device.\n");
        Pa_Terminate();
        return -1;
    }

    outputParameters.channelCount=2;
    outputParameters.sampleFormat=paFloat32;
    outputParameters.suggestedLatency=Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo=NULL;

    if(Pa_OpenStream(&stream, NULL, &outputParameters, 22050, 64, paClipOff, paCallback, NULL)!=paNoError)
    {
        fprintf(stderr, "Unable to open PortAudio Stream.\n");
        Pa_Terminate();
        return -1;
    }

    if(Pa_StartStream(stream)!=paNoError)
    {
        fprintf(stderr, "Unable to start PortAudio Stream.\n");
        Pa_Terminate();
        return -1;
    }

    if(!LoadStaticSound("levelup.wav", &TestSound1))
        return -1;

    if(!LoadStaticSound("line.wav", &TestSound2))
        return -1;

    if(!LoadStaticSound("line4.wav", &TestSound3))
        return -1;

    if(!LoadStaticSound("rotate.wav", &TestSound4))
        return -1;

    while(!Done)
    {
        if(kbhit())
        {
            switch(getch())
            {
                case 'q':
                    Done=1;
                    break;

                case '1':
                    Play_Sound(&TestSound1);
                    break;

                case '2':
                    Play_Sound(&TestSound2);
                    break;

                case '3':
                    Play_Sound(&TestSound3);
                    break;

                case '4':
                    Play_Sound(&TestSound4);
                    break;

                default:
                    break;
            }
        }
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    FREE(TestSound1.data);
    FREE(TestSound2.data);
    FREE(TestSound3.data);
    FREE(TestSound4.data);

	return 0;
}
