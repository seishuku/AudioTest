#include <Windows.h>
#include <portaudio.h>
#include <conio.h>
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

Sample_t TestSound1;
Sample_t TestSound2;
Sample_t TestSound3;
Sample_t TestSound4;

extern float Azimuth, Elevation;

bool MakeSinewave(Sample_t *Sample, const float Frequency, const float Length)
{
	const uint32_t samplesPerSec=44100;
	const uint32_t samplesLength=(uint32_t)(samplesPerSec*Length);

	Sample->pos=0;
	Sample->len=samplesLength;
	Sample->channels=1;
	Vec3_Sets(Sample->xyz, 0.0f);

	Sample->data=malloc(samplesLength*sizeof(int16_t));

	if(Sample->data==NULL)
		return false;

	for(uint32_t i=0;i<samplesLength;i++)
	{
		float Time=(float)i/samplesPerSec;
		Sample->data[i]=(int16_t)(sinf(Time*Frequency*2.0f*PI)*INT16_MAX);
	}

	return true;
}

int main(int argc, char **argv)
{
	int Done=0;

	// Bring up audio system
	Audio_Init();

	// Load up some wave sounds
	if(!MakeSinewave(&TestSound1, 440.0f, 1.0f))
		return -1;

	if(!Audio_LoadStatic("pinky_idle.wav", &TestSound2))
		return -1;

	TestSound2.xyz[0]=-1.0f;
	TestSound2.xyz[1]=0.0f;
	TestSound2.xyz[2]=-1.0f;

	if(!Audio_LoadStatic("pinky_idle.wav", &TestSound3))
		return -1;

	TestSound3.xyz[0]=1.0f;
	TestSound3.xyz[1]=0.0f;
	TestSound3.xyz[2]=1.0f;

	if(!Audio_LoadStatic("beep-3.wav", &TestSound4))
		return -1;

	TestSound4.xyz[0]=0.0f;
	TestSound4.xyz[1]=0.0f;
	TestSound4.xyz[2]=1.0f;

	init_hrtf();

	float time=0.0f;

	// Loop around, waiting for a key press
	while(!Done)
	{
		TestSound1.xyz[0]=sinf(time);
		TestSound1.xyz[1]=0.0f;
		TestSound1.xyz[2]=cosf(time);

		time+=0.0001f;

		if(kbhit())
		{
			switch(getch())
			{
				case 'q':
					Done=1;
					break;

				case '1':
					Audio_PlaySample(&TestSound1, false);
					break;

				case '2':
					Audio_PlaySample(&TestSound2, false);
					break;

				case '3':
					Audio_PlaySample(&TestSound3, false);
					break;

				case '4':
					Audio_PlaySample(&TestSound4, false);
					break;

				default:
					break;
			}
		}
	}

	// Shut down audio system
	Audio_Destroy();

	// Clean up audio samples
	FREE(TestSound1.data);
	FREE(TestSound2.data);
	FREE(TestSound3.data);
	FREE(TestSound4.data);

	return 0;
}
