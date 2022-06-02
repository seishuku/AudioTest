#ifndef __FFT_H__
#define __FFT_H__

typedef struct
{
	float r, i;
} Complex_t;

void fft(const Complex_t *input, Complex_t *output, uint32_t n, int8_t flag);

#endif
