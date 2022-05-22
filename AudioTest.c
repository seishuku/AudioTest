// TODO: Sounds need positional and ID info for tracking through the channels.

#include <portaudio.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <memory.h>
#include "audio.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

#define MAX_CHANNELS 32

Sample_t channels[MAX_CHANNELS];

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
        return (a+b)-((a*b)/INT16_MIN);
    else if(a>0&&b>0)
        return (a+b)-((a*b)/INT16_MAX);

    return a+b;
}

// Callback function for when PortAudio needs more data.
int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    // Get pointer to output buffer.
    int16_t *out=(int16_t *)outputBuffer;

    // Clear the output buffer, so we don't get annoying repeating samples.
    memset(out, 0, framesPerBuffer*sizeof(int16_t)*2);

    for(int32_t i=0;i<MAX_CHANNELS;i++)
    {
        // Quality of life pointer to current mixing channel.
        Sample_t *channel=&channels[i];

        // Calculate the remaining amount of data to process.
        uint32_t remaining_data=(channel->len-channel->pos);

        // If the channel is empty, skip on the to next.
        if(!remaining_data)
            continue;

        // Debug output.
        fprintf(stderr, "Playing channel: %d Remaining data: %d\n", i, remaining_data);

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

            *out++=MixSamples(output_sampleL, input_sample);
            *out++=MixSamples(output_sampleR, input_sample);
        }

        // Advance the sample position by what we've used, next time around will take another chunk.
        channel->pos+=remaining_data;

        // Reset output buffer pointer for next channel to process.
        out=(int16_t *)outputBuffer;

        // If loop flag was set, reset position to 0 if it's at the end.
        if(channel->pos==channel->len&&channel->looping)
            channel->pos=0;
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
        if(channels[index].pos==channels[index].len)
            break;
    }

    // return if there aren't any channels available.
    if(index>=MAX_CHANNELS)
        return;

    // otherwise set the channel's data pointer to this sample's pointer
    // and set the length, position, and loop flag.
    channels[index].data=Sample->data;
    channels[index].len=Sample->len;
    channels[index].pos=0;
    channels[index].looping=looping;
}

int main(int argc, char **argv)
{
    int Done=0;
    PaStreamParameters outputParameters;
    PaStream *stream=NULL;

    // Clear out mixing channels
    memset(channels, 0, sizeof(Sample_t)*MAX_CHANNELS);

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
    if(!LoadStaticSound("squeeze-toy-1.wav", &TestSound1))
        return -1;

    if(!LoadStaticSound("bang_6.wav", &TestSound2))
        return -1;

    if(!LoadStaticSound("glass_breaking_2.wav", &TestSound3))
        return -1;

    if(!LoadStaticSound("beep-3.wav", &TestSound4))
        return -1;

    // Loop around, waiting for a key press
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
                    Play_Sound(&TestSound1, false);
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
