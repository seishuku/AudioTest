// TODO: Sounds need positional and ID info for tracking through the channels.

#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <memory.h>
#include "system/system.h"
#include "math/math.h"
#include "math/fft.h"
#include "audio.h"

PaStream *stream=NULL;

#define MAX_VOLUME 255
#define MAX_CHANNELS 64

typedef struct
{
	int16_t *data;
	uint32_t pos, len;
	bool looping;
	float *xyz;
	int16_t working[NUM_SAMPLES*2];
} Channel_t;

vec3 Zero={ 0.0f, 0.0f, 0.0f };

Channel_t channels[MAX_CHANNELS];

// HRTF samples and buffers for interpolated values
extern HRIR_Sphere_t Sphere;
float hrir_l[NUM_SAMPLES], hrir_r[NUM_SAMPLES];
int16_t sample_l[NUM_SAMPLES], sample_r[NUM_SAMPLES];

// This function is very naive, it just interpolates *all* the HRIR positions
//   and weights then according to whatever is closest to the position.
void hrir_interpolate(vec3 xyz)
{
	for(uint32_t i=0;i<Sphere.SampleLength;i++)
	{
		float hrirSum_l=0.0f;
		float hrirSum_r=0.0f;
		float weightSum=0.0f;

		for(uint32_t j=0;j<Sphere.NumVertex;j++)
		{
			Vec3_Normalize(xyz);

			float distance=Vec3_Distance(xyz, Sphere.Vertices[j].Vertex);
			float weight=expf(-distance*distance);

			hrirSum_l+=Sphere.Vertices[j].Left[i]*weight;
			hrirSum_r+=Sphere.Vertices[j].Right[i]*weight;
			weightSum+=weight;
		}

		weightSum=1.0f/weightSum;
		hrir_l[i]=hrirSum_l*weightSum;
		hrir_r[i]=hrirSum_r*weightSum;
	}
}

void convolve(int16_t *input, int16_t *audio_l, int16_t *audio_r, size_t audio_len, float *kernel_l, float *kernel_r, size_t kernel_len)
{
	for(size_t i=0;i<audio_len;i++)
	{
		float sum_l=0;
		float sum_r=0;

		for(size_t j=0;j<kernel_len;j++)
		{
			size_t offset=i+j;

			sum_l+=((float)input[offset]/INT16_MAX)*kernel_l[kernel_len-1-j];
			sum_r+=((float)input[offset]/INT16_MAX)*kernel_r[kernel_len-1-j];
		}

		audio_l[i]=(int16_t)(sum_l*INT16_MAX);
		audio_r[i]=(int16_t)(sum_r*INT16_MAX);
	}
}

// Callback function for when PortAudio needs more data.
int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
	// Get pointer to output buffer.
	int16_t *out=(int16_t *)outputBuffer;

	// Clear the output buffer, so we don't get annoying repeating samples.
	memset(out, 0, framesPerBuffer*sizeof(int16_t)*2);

	for(uint32_t i=0;i<MAX_CHANNELS;i++)
	{
		// Quality of life pointer to current mixing channel.
		Channel_t *channel=&channels[i];

		// If the channel is empty, skip on the to next.
		if(!channel->data)
			continue;

		// Calculate the remaining amount of data to process.
		uint32_t remaining_data=channel->len-channel->pos;

		// Remaining data runs off end of primary buffer.
		// Clamp it to buffer size, we'll get the rest later.
		if(remaining_data>=framesPerBuffer)
			remaining_data=framesPerBuffer;

		// Interpolate HRIR samples that are closest to the sound's position
		// TODO: this needs work, it works, but not great
		hrir_interpolate(channel->xyz);

		// Calculate the amount to fill the convolution buffer.
		// The convolve buffer needs to be at least NUM_SAMPLE+HRIR length,
		//   but to stop annoying pops/clicks and other discontinuities, we need to copy ahead,
		//   which is either the full input sample length OR the full buffer+HRIR sample length.
		uint32_t toFill=(channel->len-channel->pos);

		if(toFill>=(NUM_SAMPLES+Sphere.SampleLength))
			toFill=(NUM_SAMPLES+Sphere.SampleLength);
		else if(toFill>=channel->len)
			toFill=channel->len;

		// Zero out the full buffer size.
		memset(channel->working, 0, NUM_SAMPLES*2*sizeof(int16_t));
		// Copy the samples.
		memcpy(channel->working, &channel->data[channel->pos], toFill*sizeof(int16_t));

		// Convolve the samples with the interpolated HRIR sample to produce a stereo sample to mix into the output buffer
		convolve(channel->working, sample_l, sample_r, remaining_data, hrir_l, hrir_r, Sphere.SampleLength);

		// Mix out the samples into the output buffer
		for(uint32_t j=0;j<remaining_data;j++)
		{
			(*out++)+=sample_l[j];
			(*out++)+=sample_r[j];
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
				channel->xyz=Zero;
			}
		}
	}

	return paContinue;
}

// Add a sound to first open channel.
void Audio_PlaySample(Sample_t *Sample, bool looping)
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
	channels[index].xyz=Sample->xyz;
}

void Audio_StopSample(Sample_t *Sample)
{
	int32_t index;

	// Search for the sample in the channels list
	for(index=0;index<MAX_CHANNELS;index++)
	{
		if(channels[index].data==Sample->data)
			break;
	}

	// return if it didn't find the sample
	if(index>=MAX_CHANNELS)
		return;

	// Set the position to the end and allow the callback to resolve the removal
	channels[index].pos=channels[index].len-1;
	channels[index].looping=false;
}

int Audio_Init(void)
{
	PaStreamParameters outputParameters;

	// Clear out mixing channels
	for(int i=0;i<MAX_CHANNELS;i++)
	{
		channels[i].data=NULL;
		channels[i].pos=0;
		channels[i].len=0;
		channels[i].looping=false;
		channels[i].xyz=Zero;
	}

	// Initialize PortAudio
	if(Pa_Initialize()!=paNoError)
	{
		DBGPRINTF("Audio: PortAudio failed to initialize.\n");
		return false;
	}

	// Set up output device parameters
	outputParameters.device=Pa_GetDefaultOutputDevice();

	if(outputParameters.device==paNoDevice)
	{
		DBGPRINTF("Audio: No default output device.\n");
		Pa_Terminate();
		return false;
	}

	outputParameters.channelCount=2;
	outputParameters.sampleFormat=paInt16;
	outputParameters.suggestedLatency=Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo=NULL;

	// Open audio stream
	if(Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE, NUM_SAMPLES, paNoFlag, paCallback, NULL)!=paNoError)
	{
		DBGPRINTF("Audio: Unable to open PortAudio stream.\n");
		Pa_Terminate();
		return false;
	}

	// Start audio stream
	if(Pa_StartStream(stream)!=paNoError)
	{
		DBGPRINTF("Audio: Unable to start PortAudio Stream.\n");
		Pa_Terminate();
		return false;
	}

	return true;
}

void Audio_Destroy(void)
{
	// Shut down PortAudio
	Pa_AbortStream(stream);
	Pa_StopStream(stream);
	Pa_CloseStream(stream);
	Pa_Terminate();
}
