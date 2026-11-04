/*
 * Thin C facade over the FFmpeg ATRAC decoders (libavcodec/atrac*.c).
 * Keeps all FFmpeg headers out of the C++ API.
 */
#ifndef ATRAC_PORT_H
#define ATRAC_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  ATRAC_PORT_ATRAC1 = 0,
  ATRAC_PORT_ATRAC3 = 1,
  ATRAC_PORT_ATRAC3PLUS = 2,
  ATRAC_PORT_ATRAC9 = 3
};

typedef struct ATRACPort ATRACPort;

/** Log message callback; level is an FFmpeg level (16 error, 24 warning, ...). */
typedef void (*atrac_log_fn)(int level, const char* message);
/** Install a log callback (NULL: stderr on desktop, silent on Arduino). */
void atrac_set_log(atrac_log_fn fn, int max_level);

/** Open a decoder. Returns NULL on error. extradata is copied. */
ATRACPort* atrac_port_open(int type, int sample_rate, int channels,
                           int block_align, const uint8_t* extradata,
                           int extradata_size);
void atrac_port_close(ATRACPort* p);
void atrac_port_flush(ATRACPort* p);

/**
 * Decode one packet. Returns the number of bytes consumed (>= 0) or a
 * negative error. *samples receives the number of samples per channel
 * (0 if no frame was produced); the planar float PCM is available via
 * atrac_port_channel() until the next call.
 */
int atrac_port_decode(ATRACPort* p, const uint8_t* data, int size,
                      int* samples);
const float* atrac_port_channel(const ATRACPort* p, int channel);

int atrac_port_sample_rate(const ATRACPort* p);
int atrac_port_channels(const ATRACPort* p);
const char* atrac_port_name(const ATRACPort* p);

#ifdef __cplusplus
}
#endif
#endif
