#ifndef __AUDIO_H__
#define __AUDIO_H__

#define SAMPLE_RATE 44100

typedef struct
{
    int16_t *data;
    uint32_t pos, len;
    float pan[2];
} Sample_t;

bool LoadStaticSound(char *Filename, Sample_t *Sample);

#endif
