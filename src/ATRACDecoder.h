#pragma once

#include <stddef.h>
#include <stdint.h>

#include "atrac_port.h"

/**
 * @enum ATRACType
 * @brief Supported ATRAC codec types.
 */
enum ATRACType {
  ATRAC3 = 0,    /**< ATRAC3 (MiniDisc LP, PSP .at3) */
  ATRAC9,        /**< ATRAC9 (PS Vita, PS4) */
  ATRAC1,        /**< ATRAC1 (MiniDisc SP) */
  ATRAC3PLUS     /**< ATRAC3+ (PSP, Sony players) */
};

/**
 * @brief Stream parameters needed to set up a decoder.
 *
 * The values correspond to what is found in the container header (e.g. the
 * WAV `fmt ` chunk of an .at3/.at9 file):
 *  - ATRAC3:  sampleRate, channels, blockAlign and the 10/12/14 byte
 *             extradata (WAV fmt extension).
 *  - ATRAC3+: sampleRate, channels, blockAlign.
 *  - ATRAC9:  the 12 byte extradata only (carries rate and layout).
 *  - ATRAC1:  sampleRate (44100), channels (1/2), blockAlign (212 * channels).
 */
struct ATRACConfig {
  int sampleRate = 0;
  int channels = 0;
  int blockAlign = 0;
  const uint8_t* extradata = nullptr;
  size_t extradataSize = 0;
};

/**
 * @brief ATRAC audio decoder (ATRAC1, ATRAC3, ATRAC3+ and ATRAC9).
 *
 * Decodes raw ATRAC frames (one block of `blockAlign` bytes at a time) to
 * interleaved 16 bit PCM or to planar float. Container parsing is not part of
 * this class.
 */
class ATRACDecoder {
 public:
  explicit ATRACDecoder(ATRACType type = ATRAC3) : m_type(type) {}
  ~ATRACDecoder() { end(); }
  ATRACDecoder(const ATRACDecoder&) = delete;
  ATRACDecoder& operator=(const ATRACDecoder&) = delete;

  /// Open the decoder. Returns false if the parameters are not accepted.
  bool begin(const ATRACConfig& config);

  /// Release all resources. Safe to call repeatedly.
  void end();

  /// Reset the decoder state, e.g. after a seek.
  void flush();

  bool isOpen() const { return m_port != nullptr; }

  /**
   * @brief Decode one ATRAC frame to interleaved 16 bit PCM.
   * @param input ATRAC frame data
   * @param inputSize size of the frame in bytes
   * @param output interleaved PCM destination
   * @param outputSamples capacity of output in int16_t values (all channels)
   * @return number of samples per channel written, 0 if the frame produced no
   * output or on error, see lastError()
   */
  int decode(const uint8_t* input, size_t inputSize, int16_t* output,
             size_t outputSamples);

  /**
   * @brief Decode one frame and keep the result as planar float (-1..1).
   * @return samples per channel, 0 on error; access with channel()
   */
  int decodeFloat(const uint8_t* input, size_t inputSize);

  /// Planar float data of the last decodeFloat()/decode() call.
  const float* channel(int ch) const;

  /// Negative FFmpeg error code of the last call, 0 on success.
  int lastError() const { return m_error; }

  int sampleRate() const { return m_port ? atrac_port_sample_rate(m_port) : 0; }
  int channels() const { return m_port ? atrac_port_channels(m_port) : 0; }
  const char* codecName() const { return m_port ? atrac_port_name(m_port) : ""; }

  /// Receive diagnostics of the decoders (level 16 = error, 24 = warning,
  /// 32 = info). Without a logger messages go to stderr (desktop) or are dropped.
  static void setLogger(void (*fn)(int level, const char* message), int maxLevel = 24) {
    atrac_set_log(fn, maxLevel);
  }

  /// Convert one float sample (-1..1) to a clipped 16 bit value.
  static int16_t toPCM16(float v);

 private:
  static int atrac9BlockAlign(const ATRACConfig& c);
  ATRACType m_type;
  ATRACPort* m_port = nullptr;
  int m_error = 0;
  int m_samples = 0;
};
