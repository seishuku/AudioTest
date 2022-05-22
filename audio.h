#ifndef __AUDIO_H__
#define __AUDIO_H__

typedef struct
{
    uint8_t *data;
    uint32_t pos, len;
    bool looping;
} Sample_t;

bool LoadStaticSound(char *Filename, Sample_t *Sample);

#endif
