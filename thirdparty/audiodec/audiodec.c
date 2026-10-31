/* Pegasus X: see audiodec.h */
#include "audiodec.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "minimp3_ex.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO 0
#include "dr_wav.h"

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int ends_with(const char* s, const char* suffix)
{
    const size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcasecmp(s + a - b, suffix) == 0;
}

/* interleaved -> mono, in place; returns the mono count */
static size_t to_mono(float* samples, size_t frames, int channels)
{
    size_t i;
    if (channels <= 1)
        return frames;
    for (i = 0; i < frames; i++) {
        float sum = 0.f;
        int c;
        for (c = 0; c < channels; c++)
            sum += samples[i * (size_t)channels + (size_t)c];
        samples[i] = sum / (float)channels;
    }
    return frames;
}

int audiodec_decode_mono(const char* path, float** out, size_t* count, int* sample_rate, int max_seconds)
{
    *out = NULL;
    *count = 0;
    *sample_rate = 0;
    if (!path)
        return 0;

    if (ends_with(path, ".mp3")) {
        mp3dec_t dec;
        mp3dec_file_info_t info;
        memset(&info, 0, sizeof(info));
        if (mp3dec_load(&dec, path, &info, NULL, NULL) != 0 || !info.buffer || info.samples == 0)
            return 0;
        {
            size_t frames = info.samples / (size_t)(info.channels > 0 ? info.channels : 1);
            const size_t limit = (size_t)max_seconds * (size_t)info.hz;
            if (max_seconds > 0 && frames > limit)
                frames = limit;
            *count = to_mono(info.buffer, frames, info.channels);
            *out = info.buffer;  /* malloc'd by minimp3 */
            *sample_rate = info.hz;
        }
        return 1;
    }
    if (ends_with(path, ".ogg") || ends_with(path, ".oga")) {
        int channels = 0, rate = 0;
        short* pcm = NULL;
        const int frames = stb_vorbis_decode_filename(path, &channels, &rate, &pcm);
        size_t n, i;
        if (frames <= 0 || !pcm)
            return 0;
        n = (size_t)frames;
        if (max_seconds > 0 && n > (size_t)max_seconds * (size_t)rate)
            n = (size_t)max_seconds * (size_t)rate;
        *out = (float*)malloc(n * sizeof(float));
        if (!*out) { free(pcm); return 0; }
        for (i = 0; i < n; i++) {
            float sum = 0.f;
            int c;
            for (c = 0; c < channels; c++)
                sum += (float)pcm[i * (size_t)channels + (size_t)c] / 32768.f;
            (*out)[i] = sum / (float)(channels > 0 ? channels : 1);
        }
        free(pcm);
        *count = n;
        *sample_rate = rate;
        return 1;
    }
    if (ends_with(path, ".wav")) {
        unsigned int channels = 0, rate = 0;
        drwav_uint64 frames = 0;
        float* pcm = drwav_open_file_and_read_pcm_frames_f32(path, &channels, &rate, &frames, NULL);
        size_t n;
        if (!pcm || frames == 0)
            return 0;
        n = (size_t)frames;
        if (max_seconds > 0 && n > (size_t)max_seconds * (size_t)rate)
            n = (size_t)max_seconds * (size_t)rate;
        *count = to_mono(pcm, n, (int)channels);
        *out = pcm;  /* drwav's allocation: freed with free() (default allocator) */
        *sample_rate = (int)rate;
        return 1;
    }
    if (ends_with(path, ".flac")) {
        unsigned int channels = 0, rate = 0;
        drflac_uint64 frames = 0;
        float* pcm = drflac_open_file_and_read_pcm_frames_f32(path, &channels, &rate, &frames, NULL);
        size_t n;
        if (!pcm || frames == 0)
            return 0;
        n = (size_t)frames;
        if (max_seconds > 0 && n > (size_t)max_seconds * (size_t)rate)
            n = (size_t)max_seconds * (size_t)rate;
        *count = to_mono(pcm, n, (int)channels);
        *out = pcm;
        *sample_rate = (int)rate;
        return 1;
    }
    return 0;
}

/* stb_vorbis' implementation, after its header-only use above */
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
