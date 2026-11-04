/*
 * Glue between the vendored FFmpeg ATRAC decoders and the Arduino API.
 * Replaces the parts of libavcodec (decode.c, avcodec.c) and libavutil
 * (cpu.c, opt.c, ...) that are not vendored.
 */
#ifndef HAVE_AV_CONFIG_H
#define HAVE_AV_CONFIG_H 1
#endif
#include "atrac_port.h"
#include "atrac_config.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "libavcodec/avcodec.h"
#include "libavcodec/codec_internal.h"
#include "libavcodec/decode.h"
#include "libavcodec/packet.h"
#include "libavutil/channel_layout.h"
#include "libavutil/attributes.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/internal.h"
#include "libavutil/lfg.h"
#include "libavutil/log.h"
#include "libavutil/md5.h"
#include "libavutil/mathematics.h"
#include "libavutil/mem.h"
#include "libavutil/samplefmt.h"
#include "libavutil/tx_priv.h"

#if ATRAC_ENABLE_ATRAC1
extern const FFCodec ff_atrac1_decoder;
#endif
#if ATRAC_ENABLE_ATRAC3
extern const FFCodec ff_atrac3_decoder;
#endif
#if ATRAC_ENABLE_ATRAC3PLUS
extern const FFCodec ff_atrac3p_decoder;
#endif
#if ATRAC_ENABLE_ATRAC9
extern const FFCodec ff_atrac9_decoder;
#endif

/* ---- small replacements for libavutil modules that are not vendored ---- */

/* logging (replaces log.c): messages go to an optional callback */
static atrac_log_fn g_log_fn = NULL;
static int g_log_level = AV_LOG_WARNING;

void atrac_set_log(atrac_log_fn fn, int max_level) {
  g_log_fn = fn;
  g_log_level = max_level;
}

void av_log(void* avcl, int level, const char* fmt, ...) {
  char buf[160];
  va_list ap;
  (void)avcl;
  level &= 0xff;
  if (level > g_log_level) return;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (g_log_fn) {
    g_log_fn(level, buf);
  } else {
#ifndef ARDUINO
    fputs(buf, stderr);
#endif
  }
}

void avpriv_report_missing_feature(void* avc, const char* msg, ...) {
  va_list ap;
  char buf[96];
  va_start(ap, msg);
  vsnprintf(buf, sizeof(buf), msg, ap);
  va_end(ap);
  av_log(avc, AV_LOG_WARNING, "%s not implemented.\n", buf);
}

void avpriv_request_sample(void* avc, const char* msg, ...) {
  va_list ap;
  char buf[96];
  va_start(ap, msg);
  vsnprintf(buf, sizeof(buf), msg, ap);
  va_end(ap);
  av_log(avc, AV_LOG_WARNING, "Unsupported: %s. Please report a sample.\n", buf);
}

/* channel layouts (replaces channel_layout.c): only native masks set through
 * the AV_CHANNEL_LAYOUT_* macros and plain channel counts are used here */
void av_channel_layout_default(AVChannelLayout* l, int nb_channels) {
  memset(l, 0, sizeof(*l));
  l->order = AV_CHANNEL_ORDER_UNSPEC;
  l->nb_channels = nb_channels;
}

void av_channel_layout_uninit(AVChannelLayout* l) { memset(l, 0, sizeof(*l)); }

/* greatest common divisor (from mathematics.c) */
int64_t av_gcd(int64_t a, int64_t b) {
  int za, zb, k;
  int64_t u, v;
  if (a == 0) return b;
  if (b == 0) return a;
  za = ff_ctzll(a);
  zb = ff_ctzll(b);
  k = FFMIN(za, zb);
  u = llabs(a >> za);
  v = llabs(b >> zb);
  while (u != v) {
    if (u > v) FFSWAP(int64_t, v, u);
    v -= u;
    v >>= ff_ctzll(v);
  }
  return (uint64_t)u << k;
}

/* lagged Fibonacci generator seeding and Box-Muller (from lfg.c) */
av_cold void av_lfg_init(AVLFG* c, unsigned int seed) {
  uint8_t tmp[16] = {0};
  int i;
  for (i = 8; i < 64; i += 4) {
    AV_WL32(tmp, seed);
    tmp[4] = i;
    av_md5_sum(tmp, tmp, 16);
    c->state[i] = AV_RL32(tmp);
    c->state[i + 1] = AV_RL32(tmp + 4);
    c->state[i + 2] = AV_RL32(tmp + 8);
    c->state[i + 3] = AV_RL32(tmp + 12);
  }
  c->index = 0;
}

void av_bmg_get(AVLFG* lfg, double out[2]) {
  double x1, x2, w;
  do {
    x1 = 2.0 / UINT_MAX * av_lfg_get(lfg) - 1.0;
    x2 = 2.0 / UINT_MAX * av_lfg_get(lfg) - 1.0;
    w = x1 * x1 + x2 * x2;
  } while (w >= 1.0);
  w = sqrt((-2.0 * log(w)) / w);
  out[0] = x1 * w;
  out[1] = x2 * w;
}

/* ---- stubs for FFmpeg internals that are intentionally not vendored ---- */

int av_get_cpu_flags(void) { return 0; }

int av_opt_get_key_value(const char** ropts, const char* key_val_sep,
                         const char* pairs_sep, unsigned flags,
                         char** rkey, char** rval) {
  (void)ropts; (void)key_val_sep; (void)pairs_sep; (void)flags;
  *rkey = NULL;
  *rval = NULL;
  return AVERROR(ENOSYS); /* only used by channel layout string parsing */
}

/* only float transforms are used by the ATRAC decoders */
const FFTXCodelet* const ff_tx_codelet_list_double_c[] = {NULL};
const FFTXCodelet* const ff_tx_codelet_list_int32_c[] = {NULL};

/* Planar sample buffers. The capacity is kept in linesize[0] so that the
 * buffer is reused when the next frame has the same size or smaller. */
int ff_get_buffer(AVCodecContext* avctx, AVFrame* frame, int flags) {
  int channels = avctx->ch_layout.nb_channels;
  int i, stride;
  (void)flags;
  if (channels <= 0 || channels > AV_NUM_DATA_POINTERS || frame->nb_samples <= 0)
    return AVERROR(EINVAL);
  stride = frame->nb_samples * (int)sizeof(float);
  if (!frame->data[0] || frame->linesize[0] < stride) {
    av_freep(&frame->data[0]);
    frame->data[0] = av_mallocz((size_t)stride * channels + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!frame->data[0]) return AVERROR(ENOMEM);
    frame->linesize[0] = stride;
  }
  for (i = 1; i < channels; i++)
    frame->data[i] = frame->data[0] + (size_t)i * frame->linesize[0];
  frame->extended_data = frame->data;
  frame->format = avctx->sample_fmt;
  return 0;
}

/* ---- facade ---- */

struct ATRACPort {
  AVCodecContext avctx;
  AVFrame frame;
  const FFCodec* codec;
  int opened;
};

ATRACPort* atrac_port_open(int type, int sample_rate, int channels,
                           int block_align, const uint8_t* extradata,
                           int extradata_size) {
  ATRACPort* p;
  const FFCodec* codec = NULL;
  switch (type) {
#if ATRAC_ENABLE_ATRAC1
    case ATRAC_PORT_ATRAC1: codec = &ff_atrac1_decoder; break;
#endif
#if ATRAC_ENABLE_ATRAC3
    case ATRAC_PORT_ATRAC3: codec = &ff_atrac3_decoder; break;
#endif
#if ATRAC_ENABLE_ATRAC3PLUS
    case ATRAC_PORT_ATRAC3PLUS: codec = &ff_atrac3p_decoder; break;
#endif
#if ATRAC_ENABLE_ATRAC9
    case ATRAC_PORT_ATRAC9: codec = &ff_atrac9_decoder; break;
#endif
    default: break;
  }
  if (!codec) return NULL; /* unknown or disabled codec */
  if (channels < 0 || channels > AV_NUM_DATA_POINTERS || extradata_size < 0 ||
      (extradata_size > 0 && !extradata))
    return NULL;

  p = av_mallocz(sizeof(*p));
  if (!p) return NULL;
  p->codec = codec;
  p->frame.extended_data = p->frame.data; /* decoders read it before ff_get_buffer() */
  p->avctx.codec = &codec->p;
  p->avctx.codec_id = codec->p.id;
  p->avctx.codec_type = AVMEDIA_TYPE_AUDIO;
  p->avctx.sample_rate = sample_rate;
  p->avctx.block_align = block_align;
  if (channels > 0) av_channel_layout_default(&p->avctx.ch_layout, channels);

  if (extradata_size > 0) {
    p->avctx.extradata = av_mallocz((size_t)extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!p->avctx.extradata) goto fail;
    memcpy(p->avctx.extradata, extradata, extradata_size);
    p->avctx.extradata_size = extradata_size;
  }
  if (codec->priv_data_size) {
    p->avctx.priv_data = av_mallocz(codec->priv_data_size);
    if (!p->avctx.priv_data) goto fail;
  }
  p->opened = 1; /* close() may be called even if init fails (INIT_CLEANUP) */
  if (codec->init && codec->init(&p->avctx) < 0) goto fail;
  return p;
fail:
  atrac_port_close(p);
  return NULL;
}

void atrac_port_close(ATRACPort* p) {
  if (!p) return;
  if (p->opened && p->codec->close) p->codec->close(&p->avctx);
  av_freep(&p->frame.data[0]);
  av_freep(&p->avctx.priv_data);
  av_freep(&p->avctx.extradata);
  av_channel_layout_uninit(&p->avctx.ch_layout);
  av_free(p);
}

void atrac_port_flush(ATRACPort* p) {
  if (p && p->codec->flush) p->codec->flush(&p->avctx);
}

int atrac_port_decode(ATRACPort* p, const uint8_t* data, int size,
                      int* samples) {
  AVPacket pkt;
  int got = 0, rc;
  *samples = 0;
  if (!p || !data || size <= 0) return AVERROR(EINVAL);
  memset(&pkt, 0, sizeof(pkt));
  pkt.data = (uint8_t*)data;
  pkt.size = size;
  p->frame.nb_samples = 0;
  rc = p->codec->cb.decode(&p->avctx, &p->frame, &got, &pkt);
  if (rc >= 0 && got) *samples = p->frame.nb_samples;
  return rc;
}

const float* atrac_port_channel(const ATRACPort* p, int channel) {
  if (!p || channel < 0 || channel >= p->avctx.ch_layout.nb_channels) return NULL;
  return (const float*)p->frame.data[channel];
}

int atrac_port_sample_rate(const ATRACPort* p) { return p->avctx.sample_rate; }
int atrac_port_channels(const ATRACPort* p) { return p->avctx.ch_layout.nb_channels; }
const char* atrac_port_name(const ATRACPort* p) { return p->codec->p.name; }
