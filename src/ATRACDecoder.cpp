#include "ATRACDecoder.h"

#include <math.h>

// ATRAC9 config header: 4 byte version, 0xFE, rate(4) layout(3) verify(1),
// avg frame size - 1 (11 bits), superframe index (2 bits). The packet size is
// frame size << superframe index.
int ATRACDecoder::atrac9BlockAlign(const ATRACConfig& c) {
  if (!c.extradata || c.extradataSize != 12) return 0;
  const uint8_t* e = c.extradata + 4;
  int frameSize = ((e[2] << 3) | (e[3] >> 5)) + 1;
  int superframe = (e[3] >> 3) & 0x3;
  return frameSize << superframe;
}

bool ATRACDecoder::begin(const ATRACConfig& c) {
  end();
  int t = ATRAC_PORT_ATRAC3;
  switch (m_type) {
    case ATRAC1: t = ATRAC_PORT_ATRAC1; break;
    case ATRAC3: t = ATRAC_PORT_ATRAC3; break;
    case ATRAC3PLUS: t = ATRAC_PORT_ATRAC3PLUS; break;
    case ATRAC9: t = ATRAC_PORT_ATRAC9; break;
  }
  int blockAlign = c.blockAlign;
  if (m_type == ATRAC9 && blockAlign <= 0) blockAlign = atrac9BlockAlign(c);
  m_port = atrac_port_open(t, c.sampleRate, c.channels, blockAlign,
                           c.extradata, (int)c.extradataSize);
  m_error = m_port ? 0 : -1;
  return m_port != nullptr;
}

void ATRACDecoder::end() {
  atrac_port_close(m_port);
  m_port = nullptr;
  m_samples = 0;
}

void ATRACDecoder::flush() { atrac_port_flush(m_port); }

int ATRACDecoder::decodeFloat(const uint8_t* input, size_t inputSize) {
  m_samples = 0;
  if (!m_port || !input || inputSize == 0 || inputSize > 0x7fffffff) {
    m_error = -1;
    return 0;
  }
  int n = 0;
  int rc = atrac_port_decode(m_port, input, (int)inputSize, &n);
  m_error = rc < 0 ? rc : 0;
  if (rc < 0) return 0;
  m_samples = n;
  return n;
}

const float* ATRACDecoder::channel(int ch) const {
  return m_samples > 0 ? atrac_port_channel(m_port, ch) : nullptr;
}

int16_t ATRACDecoder::toPCM16(float v) {
  if (!(v == v)) return 0;  // NaN
  float s = v * 32768.0f;
  if (s >= 32767.0f) return 32767;
  if (s <= -32768.0f) return -32768;
  return (int16_t)lrintf(s);
}

int ATRACDecoder::decode(const uint8_t* input, size_t inputSize,
                         int16_t* output, size_t outputSamples) {
  int n = decodeFloat(input, inputSize);
  int ch = channels();
  if (n <= 0 || !output || ch <= 0) return 0;
  if ((size_t)n * ch > outputSamples) {
    m_error = -2;  // output buffer too small
    return 0;
  }
  for (int c = 0; c < ch; c++) {
    const float* src = channel(c);
    for (int i = 0; i < n; i++) output[i * ch + c] = toPCM16(src[i]);
  }
  return n;
}
