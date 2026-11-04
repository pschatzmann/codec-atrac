// Measures decoding speed: time per frame and real-time factor for each codec.
// ATRAC3+ is disabled on Arduino by default: build with -DATRAC_ENABLE_ATRAC3PLUS=1
// (and ATRAC9 can be disabled with -DATRAC_ENABLE_ATRAC9=0 to save flash).
#include "ATRACDecoder.h"
#include "atrac_config.h"
#include "data_atrac1.h"
#include "data_atrac3.h"
#include "data_atrac3plus.h"
#include "data_atrac9.h"

static int16_t pcm[2048 * 2];

static void bench(const char* name, ATRACType type, const ATRACConfig& cfg,
                  const uint8_t* frames, int nFrames, int samplesPerFrame) {
  ATRACDecoder decoder(type);
  uint32_t heap0 = ESP.getFreeHeap();
  uint32_t t0 = micros();
  if (!decoder.begin(cfg)) {
    Serial.printf("%s: begin FAILED\n", name);
    return;
  }
  uint32_t beginUs = micros() - t0;
  uint32_t heapUsed = heap0 - ESP.getFreeHeap();

  const int rounds = 10;
  int64_t sum = 0;
  uint32_t total = 0, start = micros();
  for (int r = 0; r < rounds; r++) {
    for (int f = 0; f < nFrames; f++) {
      int n = decoder.decode(frames + f * cfg.blockAlign, cfg.blockAlign, pcm, sizeof(pcm) / 2);
      if (n != samplesPerFrame) {
        Serial.printf("%s: decode error %d (n=%d) at frame %d\n", name, decoder.lastError(), n, f);
        return;
      }
      sum += pcm[0] + pcm[100] + pcm[1000];
      total++;
    }
  }
  float perFrame = (float)(micros() - start) / total;
  float frameUs = samplesPerFrame * 1e6f / decoder.sampleRate();
  Serial.printf("%-11s %7.0f us/frame (%d samples)  %5.1fx real time  begin %lu us  heap %u B  checksum %lld\n",
                name, perFrame, samplesPerFrame, frameUs / perFrame, (unsigned long)beginUs,
                (unsigned)heapUsed, (long long)sum);
  decoder.end();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) delay(10);
  Serial.printf("CPU %u MHz, free heap %u\n", (unsigned)getCpuFrequencyMhz(), (unsigned)ESP.getFreeHeap());

  ATRACConfig c;
  c.sampleRate = 44100;
  c.channels = 2;

#if ATRAC_ENABLE_ATRAC3
  c.blockAlign = 192;
  c.extradata = atrac3_extradata;
  c.extradataSize = sizeof(atrac3_extradata);
  bench("ATRAC3", ATRAC3, c, atrac3_frames, sizeof(atrac3_frames) / 192, 1024);
#endif
#if ATRAC_ENABLE_ATRAC1
  c.blockAlign = 424;
  c.extradata = nullptr;
  c.extradataSize = 0;
  bench("ATRAC1", ATRAC1, c, atrac1_frames, sizeof(atrac1_frames) / 424, 512);
#endif
#if ATRAC_ENABLE_ATRAC9
  c.sampleRate = 0;  // taken from the config header
  c.channels = 0;
  c.blockAlign = 512;
  c.extradata = atrac9_extradata;
  c.extradataSize = sizeof(atrac9_extradata);
  bench("ATRAC9", ATRAC9, c, atrac9_frames, sizeof(atrac9_frames) / 512, 1024);
  c.sampleRate = 44100;
  c.channels = 2;
#endif
#if ATRAC_ENABLE_ATRAC3PLUS
  c.blockAlign = 1488;
  c.extradata = nullptr;
  c.extradataSize = 0;
  bench("ATRAC3+", ATRAC3PLUS, c, atrac3plus_frames, sizeof(atrac3plus_frames) / 1488, 2048);
#endif
  Serial.println("done");
}

void loop() {}
