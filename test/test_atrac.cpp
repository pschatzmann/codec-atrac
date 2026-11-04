// Self-contained tests (no framework needed): run with `ctest` or ./test_atrac
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "ATRACDecoder.h"

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                    \
  do {                                                                 \
    g_checks++;                                                        \
    if (!(cond)) {                                                     \
      g_failed++;                                                      \
      printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                  \
  } while (0)

typedef std::vector<uint8_t> Bytes;

static Bytes readFile(const std::string& name) {
  Bytes b;
  FILE* f = fopen((std::string(TEST_DATA_DIR) + "/" + name).c_str(), "rb");
  if (!f) return b;
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) b.insert(b.end(), buf, buf + n);
  fclose(f);
  return b;
}

static uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t* p) { return p[0] | p[1] << 8; }

// Minimal RIFF/WAVE reader: extracts the fmt parameters and the data chunk.
struct Wav {
  ATRACConfig cfg;
  Bytes extradata;
  size_t dataPos = 0, dataSize = 0;
  bool ok = false;
};

static Wav parseWav(const Bytes& d) {
  Wav w;
  if (d.size() < 12 || memcmp(d.data(), "RIFF", 4) || memcmp(d.data() + 8, "WAVE", 4)) return w;
  size_t i = 12;
  while (i + 8 <= d.size()) {
    uint32_t sz = rd32(&d[i + 4]);
    if (!memcmp(&d[i], "fmt ", 4) && sz >= 18) {
      const uint8_t* f = &d[i + 8];
      w.cfg.channels = rd16(f + 2);
      w.cfg.sampleRate = rd32(f + 4);
      w.cfg.blockAlign = rd16(f + 12);
      size_t cb = rd16(f + 16);
      if (cb && 18 + cb <= sz) w.extradata.assign(f + 18, f + 18 + cb);
    } else if (!memcmp(&d[i], "data", 4)) {
      w.dataPos = i + 8;
      w.dataSize = std::min<size_t>(sz, d.size() - w.dataPos);
      w.ok = true;
    }
    i += 8 + sz + (sz & 1);
  }
  w.cfg.extradata = w.extradata.empty() ? nullptr : w.extradata.data();
  w.cfg.extradataSize = w.extradata.size();
  return w;
}

// Decode all frames, return interleaved PCM.
static std::vector<int16_t> decodeAll(ATRACDecoder& dec, const uint8_t* data, size_t size,
                                      size_t frameSize, int* failedFrames = nullptr) {
  std::vector<int16_t> pcm;
  std::vector<int16_t> out(8192 * 8);
  int failed = 0;
  for (size_t pos = 0; pos + frameSize <= size; pos += frameSize) {
    int n = dec.decode(data + pos, frameSize, out.data(), out.size());
    if (n <= 0) { failed++; continue; }
    pcm.insert(pcm.end(), out.begin(), out.begin() + (size_t)n * dec.channels());
  }
  if (failedFrames) *failedFrames = failed;
  return pcm;
}

// Compare against an ffmpeg reference (s16le). Returns max abs difference.
static int maxDiff(const std::vector<int16_t>& pcm, const Bytes& ref, double* rms = nullptr) {
  size_t n = std::min(pcm.size(), ref.size() / 2);
  int maxd = 0;
  double acc = 0;
  for (size_t i = 0; i < n; i++) {
    int r = (int16_t)rd16(&ref[i * 2]);
    int d = abs(r - pcm[i]);
    if (d > maxd) maxd = d;
    acc += (double)d * d;
  }
  if (rms) *rms = n ? sqrt(acc / n) : 1e9;
  return maxd;
}

static void testPcmConversion() {
  printf("pcm conversion\n");
  CHECK(ATRACDecoder::toPCM16(0.0f) == 0);
  CHECK(ATRACDecoder::toPCM16(0.5f) == 16384);
  CHECK(ATRACDecoder::toPCM16(-0.5f) == -16384);
  CHECK(ATRACDecoder::toPCM16(1.0f) == 32767);
  CHECK(ATRACDecoder::toPCM16(7.0f) == 32767);
  CHECK(ATRACDecoder::toPCM16(-1.0f) == -32768);
  CHECK(ATRACDecoder::toPCM16(-7.0f) == -32768);
  CHECK(ATRACDecoder::toPCM16(NAN) == 0);
  CHECK(ATRACDecoder::toPCM16(INFINITY) == 32767);
}

static void testNotOpen() {
  printf("not open / bad usage\n");
  ATRACDecoder dec(ATRAC3);
  uint8_t in[16] = {0};
  int16_t out[64];
  CHECK(!dec.isOpen());
  CHECK(dec.decode(in, sizeof(in), out, 64) == 0);
  CHECK(dec.lastError() != 0);
  CHECK(dec.channel(0) == nullptr);
  CHECK(dec.channels() == 0);
  dec.flush();  // must not crash
  dec.end();
  dec.end();
}

static void testBadConfig() {
  printf("bad configuration is rejected\n");
  ATRACConfig c;
  {
    ATRACDecoder d(ATRAC3);  // needs extradata + block align
    CHECK(!d.begin(c));
    CHECK(!d.isOpen());
  }
  {
    ATRACDecoder d(ATRAC9);  // needs the 12 byte config
    CHECK(!d.begin(c));
  }
  {
    ATRACDecoder d(ATRAC9);
    uint8_t junk[12] = {0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0};
    c.extradata = junk;
    c.extradataSize = sizeof(junk);
    CHECK(!d.begin(c));  // wrong magic byte
  }
  {
    ATRACDecoder d(ATRAC3);
    c = ATRACConfig();
    c.channels = 100;  // more than supported
    c.sampleRate = 44100;
    c.blockAlign = 192;
    CHECK(!d.begin(c));
  }
  {
    ATRACDecoder d(ATRAC1);
    c = ATRACConfig();
    c.channels = 0;  // channel count is required
    c.sampleRate = 44100;
    c.blockAlign = 424;
    CHECK(!d.begin(c));
  }
}

static void testAtrac3() {
  printf("atrac3 vs ffmpeg reference\n");
  Wav w = parseWav(readFile("atrac3_stereo_66k.wav"));
  Bytes ref = readFile("atrac3_stereo_66k.ref.s16");
  CHECK(w.ok && !ref.empty());
  if (!w.ok || ref.empty()) return;
  CHECK(w.cfg.channels == 2 && w.cfg.sampleRate == 44100 && w.cfg.blockAlign == 192);

  ATRACDecoder dec(ATRAC3);
  CHECK(dec.begin(w.cfg));
  CHECK(dec.channels() == 2);
  CHECK(dec.sampleRate() == 44100);
  CHECK(strcmp(dec.codecName(), "atrac3") == 0);

  Bytes file = readFile("atrac3_stereo_66k.wav");
  int failed = 0;
  std::vector<int16_t> pcm = decodeAll(dec, file.data() + w.dataPos, w.dataSize, w.cfg.blockAlign, &failed);
  CHECK(failed == 0);
  CHECK(pcm.size() >= ref.size() / 2);
  CHECK(pcm.size() % 2 == 0);
  double rms;
  int md = maxDiff(pcm, ref, &rms);
  printf("  samples=%zu maxdiff=%d rms=%.3f\n", pcm.size(), md, rms);
  CHECK(md <= 16);

  // flush() must keep the decoder usable
  dec.flush();
  int16_t tmp[4096];
  CHECK(dec.decode(file.data() + w.dataPos, w.cfg.blockAlign, tmp, 4096) == 1024);

  // a new session reproduces exactly the same output
  dec.end();
  CHECK(dec.begin(w.cfg));
  std::vector<int16_t> again = decodeAll(dec, file.data() + w.dataPos, w.dataSize, w.cfg.blockAlign);
  CHECK(again == pcm);

  // float interface
  dec.end();
  CHECK(dec.begin(w.cfg));
  int n = dec.decodeFloat(file.data() + w.dataPos, w.cfg.blockAlign);
  CHECK(n == 1024);
  CHECK(dec.channel(0) != nullptr && dec.channel(1) != nullptr);
  CHECK(dec.channel(2) == nullptr);
  CHECK(dec.channel(-1) == nullptr);

  // too small output buffer: reports an error instead of overflowing
  int16_t small[16];
  CHECK(dec.decode(file.data() + w.dataPos + w.cfg.blockAlign, w.cfg.blockAlign, small, 16) == 0);
  CHECK(dec.lastError() != 0);

  // truncated frame must fail gracefully
  int16_t out[4096];
  CHECK(dec.decode(file.data() + w.dataPos, 10, out, 4096) == 0);
  CHECK(dec.lastError() < 0);
}

static void testRawFrames(ATRACType type, const char* name, const char* pkts, const char* ref,
                          int blockAlign) {
  printf("%s vs ffmpeg reference\n", name);
  Bytes data = readFile(pkts), r = readFile(ref);
  CHECK(!data.empty() && !r.empty());
  if (data.empty() || r.empty()) return;
  ATRACConfig c;
  c.sampleRate = 44100;
  c.channels = 2;
  c.blockAlign = blockAlign;
  ATRACDecoder dec(type);
  CHECK(dec.begin(c));
  CHECK(dec.channels() == 2);
  int failed = 0;
  std::vector<int16_t> pcm = decodeAll(dec, data.data(), data.size(), blockAlign, &failed);
  CHECK(failed == 0);
  CHECK(pcm.size() >= r.size() / 2);
  double rms;
  int md = maxDiff(pcm, r, &rms);
  printf("  samples=%zu maxdiff=%d rms=%.3f\n", pcm.size(), md, rms);
  CHECK(md <= 16);
}

static void testAtrac9() {
  printf("atrac9 vs ffmpeg reference\n");
  Bytes data = readFile("atrac9_stereo.pkts"), extra = readFile("atrac9_stereo.extradata");
  Bytes r = readFile("atrac9_stereo.ref.s16");
  CHECK(!data.empty() && extra.size() == 12 && !r.empty());
  if (data.empty() || extra.size() != 12 || r.empty()) return;
  ATRACConfig c;
  c.extradata = extra.data();  // sample rate and layout come from the header
  c.extradataSize = extra.size();
  c.blockAlign = 512;
  ATRACDecoder dec(ATRAC9);
  CHECK(dec.begin(c));
  CHECK(dec.channels() == 2);
  CHECK(dec.sampleRate() == 48000);
  CHECK(strcmp(dec.codecName(), "atrac9") == 0);
  int failed = 0;
  std::vector<int16_t> pcm = decodeAll(dec, data.data(), data.size(), 512, &failed);
  CHECK(failed == 0);
  CHECK(pcm.size() >= r.size() / 2);
  double rms;
  int md = maxDiff(pcm, r, &rms);
  printf("  samples=%zu maxdiff=%d rms=%.3f\n", pcm.size(), md, rms);
  CHECK(md <= 16);

  // blockAlign derived from the header gives the same result
  ATRACDecoder dec2(ATRAC9);
  c.blockAlign = 0;
  CHECK(dec2.begin(c));
  std::vector<int16_t> pcm2 = decodeAll(dec2, data.data(), data.size(), 512);
  CHECK(pcm2 == pcm);
}

static void testAtrac9Robustness() {
  printf("atrac9 robustness\n");
  // 12 byte config: version(4) | magic 0xFE(8) rate idx(4) block config(3)
  // verify(1) avg frame size-1(11) superframe idx(2) | reserved
  uint8_t extra[12] = {0};
  uint64_t bits = 0xFEull;
  bits = (bits << 4) | 7;      // sample rate index
  bits = (bits << 3) | 1;      // block config: stereo
  bits = (bits << 1) | 0;      // verification bit
  bits = (bits << 11) | 186;   // average frame size - 1
  bits = (bits << 2) | 0;      // superframe index 0 -> 1 frame per superframe
  bits <<= 3;                  // pad to 32 bits (29 used)
  for (int i = 0; i < 4; i++) extra[4 + i] = (uint8_t)(bits >> (24 - 8 * i));
  ATRACConfig c;
  c.extradata = extra;
  c.extradataSize = sizeof(extra);
  ATRACDecoder dec(ATRAC9);
  CHECK(dec.begin(c));  // blockAlign is derived from the header when 0
  if (!dec.isOpen()) return;
  CHECK(dec.channels() == 2);
  CHECK(dec.sampleRate() > 0);
  srand(1234);
  std::vector<uint8_t> junk(187);
  std::vector<int16_t> out(8192 * 2);
  for (int i = 0; i < 200; i++) {  // random data must never crash or overflow
    for (auto& b : junk) b = (uint8_t)rand();
    dec.decode(junk.data(), junk.size(), out.data(), out.size());
  }
  CHECK(true);
}

static void testLifecycle() {
  printf("begin/end lifecycle\n");
  Wav w = parseWav(readFile("atrac3_stereo_66k.wav"));
  if (!w.ok) { CHECK(false); return; }
  ATRACDecoder dec(ATRAC3);
  for (int i = 0; i < 5; i++) {  // repeated begin() must not leak or crash
    CHECK(dec.begin(w.cfg));
    dec.end();
    CHECK(!dec.isOpen());
  }
  CHECK(dec.begin(w.cfg));
  CHECK(dec.begin(w.cfg));  // implicit end() of the previous session
  CHECK(dec.isOpen());
}

int main() {
  testPcmConversion();
  testNotOpen();
  testBadConfig();
  testLifecycle();
  testAtrac3();
  testRawFrames(ATRAC1, "atrac1", "atrac1_chirp.pkts", "atrac1_chirp.ref.s16", 424);
  testRawFrames(ATRAC3PLUS, "atrac3plus", "atrac3plus_short.pkts", "atrac3plus_short.ref.s16", 1488);
  testAtrac9();
  testAtrac9Robustness();
  printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
