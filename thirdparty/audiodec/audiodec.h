/* Pegasus X: decodes an audio file (mp3, ogg, wav, flac) to mono float
 * samples, for the music visualizer. One function, no Qt. */
#ifndef PEGASUSX_AUDIODEC_H
#define PEGASUSX_AUDIODEC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decodes `path` into mono float samples (-1..1) at the file's sample rate.
 * `*out` is malloc'd and must be free()d by the caller. `*sample_rate` and
 * `*count` (samples) are set. Returns 1 on success, 0 when the format isn't
 * known or the file can't be read. `max_seconds`: more than this is cut. */
int audiodec_decode_mono(const char* path, float** out, size_t* count, int* sample_rate, int max_seconds);

#ifdef __cplusplus
}
#endif

#endif
