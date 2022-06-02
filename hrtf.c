#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "math/math.h"
#include "math/fft.h"
#include "system/system.h"
#include "audio.h"

HRTF_t hrtf[AZIMUTH_CNT];

void init_hrtf_data(HRTF_t *hrtf, Sample_t *sample)
{
	Complex_t *hrir_l=(Complex_t *)malloc(sizeof(Complex_t)*FFT_SAMPLES);
	Complex_t *hrir_r=(Complex_t *)malloc(sizeof(Complex_t)*FFT_SAMPLES);

	hrtf->hrtf_l=(Complex_t *)malloc(sizeof(Complex_t)*FFT_SAMPLES);
	hrtf->hrtf_r=(Complex_t *)malloc(sizeof(Complex_t)*FFT_SAMPLES);

	for(uint32_t i=0;i<FFT_SAMPLES;i++)
	{
		if(i<sample->len)
		{
			hrir_l[i].r=(float)sample->data[2*i+0]/INT16_MAX;
			hrir_r[i].r=(float)sample->data[2*i+1]/INT16_MAX;
		}
		else
		{
			hrir_l[i].r=0.0f;
			hrir_r[i].r=0.0f;
		}

		hrir_l[i].i=0.0f;
		hrir_r[i].i=0.0f;
	}

	fft(hrir_l, hrtf->hrtf_l, FFT_SAMPLES, 1);
	fft(hrir_r, hrtf->hrtf_r, FFT_SAMPLES, 1);

	free(hrir_l);
	free(hrir_r);
}

void free_hrtf_data(HRTF_t *hrtf)
{
	free(hrtf->hrtf_l);
	free(hrtf->hrtf_r);
}

bool init_hrtf(void)
{
	char filename[100];

	for(int i=0;i<AZIMUTH_CNT;i++)
	{
		sprintf(filename, "hrtf_samples/azi_%d_ele_0_RAW.wav", i*5);
		printf("Loading: %s\n", filename);

		Sample_t hrtf_sample;

		if(!Audio_LoadStatic(filename, &hrtf_sample))
		{
			printf("Could not load hrtf file: %s\n", filename);
			return false;
		}

		init_hrtf_data(&hrtf[i], &hrtf_sample);

		free(hrtf_sample.data);
	}

	return true;
}