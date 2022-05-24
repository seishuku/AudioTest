// TODO: Sounds need positional and ID info for tracking through the channels.

#include <Windows.h>
#include <portaudio.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <memory.h>
#include <math.h>
#include "audio.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

#define MAX_CHANNELS 32

typedef struct
{
    int16_t *data;
    uint32_t pos, len;
    bool looping;
    float *pan;
} Channel_t;

float PanOne[2]={ 1.0f, 1.0f };

Channel_t channels[MAX_CHANNELS];

Sample_t TestSound1;
Sample_t TestSound2;
Sample_t TestSound3;
Sample_t TestSound4;

int16_t MixSamples(int16_t a, int16_t b)
{
    // Mix samples
    // result=(a+b)-(a*b)
    // https://atastypixel.com/how-to-mix-audio-samples-properly-on-ios/
    if(a<0&&b<0)
        return (((int32_t)a+(int32_t)b)-(((int32_t)a*(int32_t)b)/INT16_MIN));
    else if(a>0&&b>0)
        return (((int32_t)a+(int32_t)b)-(((int32_t)a*(int32_t)b)/INT16_MAX));

    return ((int32_t)a+(int32_t)b);
}

// Callback function for when PortAudio needs more data.
int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    float one[2]={ 1.0f, 1.0f };
    
// Debug output
#if 1
    printf("\x1B[H");

    for(int i=0;i<MAX_CHANNELS;i++)
        printf("Channel %d: Pos %d Length %d %0.3f %0.3f       \n", i, channels[i].pos, channels[i].len, channels[i].pan[0], channels[i].pan[1]);
#endif

    // Get pointer to output buffer.
    int16_t *out=(int16_t *)outputBuffer;

    // Clear the output buffer, so we don't get annoying repeating samples.
    memset(out, 0, framesPerBuffer*sizeof(int16_t)*2);

    for(int32_t i=0;i<MAX_CHANNELS;i++)
    {
        // Quality of life pointer to current mixing channel.
        Channel_t *channel=&channels[i];

        // Calculate the remaining amount of data to process.
        uint32_t remaining_data=(channel->len-channel->pos);

        // If the channel is empty, skip on the to next.
        if(!remaining_data)
            continue;

        // Remaining data runs off end of primary buffer.
        // Clamp it to buffer size, we'll get the rest later.
        if(remaining_data>framesPerBuffer)
            remaining_data=framesPerBuffer;

        // Transfer/process what we can.
        for(uint32_t j=0;j<remaining_data;j++)
        {
            int16_t input_sample=channel->data[channel->pos+j];
            int16_t output_sampleL=*out+0;
            int16_t output_sampleR=*out+1;

            *out++=(int16_t)MixSamples(output_sampleL, (int16_t)((float)input_sample*channel->pan[0]));
            *out++=(int16_t)MixSamples(output_sampleR, (int16_t)((float)input_sample*channel->pan[1]));
        }

        // Advance the sample position by what we've used, next time around will take another chunk.
        channel->pos+=remaining_data;

        // Reset output buffer pointer for next channel to process.
        out=(int16_t *)outputBuffer;

        // If loop flag was set, reset position to 0 if it's at the end.
        if(channel->pos==channel->len)
        {
            if(channel->looping)
                channel->pos=0;
            else
            {
                // DO STUFF TO REMOVE
                channel->data=NULL;
                channel->pos=0;
                channel->len=0;
                channel->looping=false;
                channel->pan=PanOne;
            }
        }
    }

    return paContinue;
}

// Add a sound to first open channel.
void Play_Sound(Sample_t *Sample, bool looping)
{
    int32_t index;

    // Look for an empty sound channel slot.
    for(index=0;index<MAX_CHANNELS;index++)
    {
        // If it's either done playing or is still the initial zero.
        if(channels[index].pos==channels[index].len)
            break;
    }

    // return if there aren't any channels available.
    if(index>=MAX_CHANNELS)
        return;

    // otherwise set the channel's data pointer to this sample's pointer
    // and set the length, reset play position, and loop flag.
    channels[index].data=Sample->data;
    channels[index].len=Sample->len;
    channels[index].pos=0;
    channels[index].looping=looping;
    channels[index].pan=Sample->pan;
}

bool EnableVTMode(void)
{
    HANDLE hOut=GetStdHandle(STD_OUTPUT_HANDLE);

    if(hOut==INVALID_HANDLE_VALUE)
        return false;

    DWORD dwMode=0;

    if(!GetConsoleMode(hOut, &dwMode))
        return false;

    if(!SetConsoleMode(hOut, dwMode|ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return false;

    return true;
}

int main(int argc, char **argv)
{
    int Done=0;
    PaStreamParameters outputParameters;
    PaStream *stream=NULL;

    EnableVTMode();

    // Clear out mixing channels
    for(int i=0;i<MAX_CHANNELS;i++)
    {
        channels[i].data=NULL;
        channels[i].pos=0;
        channels[i].len=0;
        channels[i].looping=false;
        channels[i].pan=PanOne;
    }

    // Initialize PortAudio
    if(Pa_Initialize()!=paNoError)
        return -1;

    // Set up output device parameters
    outputParameters.device=Pa_GetDefaultOutputDevice();

    if(outputParameters.device==paNoDevice)
    {
        fprintf(stderr, "Error: No default output device.\n");
        Pa_Terminate();
        return -1;
    }

    outputParameters.channelCount=2;
    outputParameters.sampleFormat=paInt16;
    outputParameters.suggestedLatency=Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo=NULL;

    // Open audio stream
    if(Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE, 128, paClipOff, paCallback, NULL)!=paNoError)
    {
        fprintf(stderr, "Unable to open PortAudio Stream.\n");
        Pa_Terminate();
        return -1;
    }

    // Start audio stream
    if(Pa_StartStream(stream)!=paNoError)
    {
        fprintf(stderr, "Unable to start PortAudio Stream.\n");
        Pa_Terminate();
        return -1;
    }

    // Load up some wave sounds
    if(!LoadStaticSound("rotate.wav", &TestSound1))
        return -1;

    if(!LoadStaticSound("bang_6.wav", &TestSound2))
        return -1;

    if(!LoadStaticSound("glass_breaking_2.wav", &TestSound3))
        return -1;

    if(!LoadStaticSound("beep-3.wav", &TestSound4))
        return -1;

    float time=0.0f;

    // Loop around, waiting for a key press
    while(!Done)
    {
        TestSound1.pan[0]=sinf(time)*0.5f+0.5f;
        TestSound1.pan[1]=1.0f-TestSound1.pan[0];
        time+=0.0005f;

        if(kbhit())
        {
            switch(getch())
            {
                case 'q':
                    Done=1;
                    break;

                case '1':
                    Play_Sound(&TestSound1, true);
                    break;

                case '2':
                    Play_Sound(&TestSound2, false);
                    break;

                case '3':
                    Play_Sound(&TestSound3, false);
                    break;

                case '4':
                    Play_Sound(&TestSound4, false);
                    break;

                default:
                    break;
            }
        }
    }

    // Shut down PortAudio
    Pa_AbortStream(stream);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    // Clean up audio samples
    FREE(TestSound1.data);
    FREE(TestSound2.data);
    FREE(TestSound3.data);
    FREE(TestSound4.data);

	return 0;
}
