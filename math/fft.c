#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "math.h"
#include "fft.h"

void fft(const Complex_t *input, Complex_t *output, uint32_t n, int8_t flag)
{
	uint32_t half=n>>1;
	Complex_t *buffer=(Complex_t *)calloc(sizeof(Complex_t), n*2);

	if(buffer==NULL)
		return;

	Complex_t *tmp=buffer;
	Complex_t *y=tmp+n;

	memcpy(y, input, sizeof(Complex_t)*n);

	for(uint32_t r=half, l=1;r>=1;r>>=1)
	{
		Complex_t *tp=y;
		y=tmp;
		tmp=tp;

		float factor_w=-flag*3.1415926f/l;

		Complex_t w={ cosf(factor_w), sinf(factor_w) };
		Complex_t wj={ 1.0f, 0.0f };

		for(uint32_t j=0;j<l;j++)
		{
			uint32_t jrs=j*(r<<1);

			for(uint32_t k=jrs, m=jrs>>1;k<jrs+r;k++)
			{
				const Complex_t t=
				{
					(wj.r*tmp[k+r].r)-(wj.i*tmp[k+r].i),
					(wj.i*tmp[k+r].r)+(wj.r*tmp[k+r].i)
				};

				y[m].r=tmp[k].r+t.r;
				y[m].i=tmp[k].i+t.i;
				y[m+half].r=tmp[k].r-t.r;
				y[m+half].i=tmp[k].i-t.i;
				m++;
			}

			const float t=wj.r;

			wj.r=(t*w.r)-(wj.i*w.i);
			wj.i=(wj.i*w.r)+(t*w.i);
		}

		l<<=1;
	}

	memcpy(output, y, sizeof(Complex_t)*n);
	free(buffer);
}
