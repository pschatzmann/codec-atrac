// Decodes a few raw ATRAC3 frames to 16 bit PCM and prints the first samples.
// The stream parameters normally come from the container header (e.g. the
// WAV "fmt " chunk of an .at3 file); here they are hard coded.
#include "ATRACDecoder.h"
#include "sample_data.h"

ATRACDecoder decoder(ATRAC3);
int16_t pcm[1024 * 2];  // one ATRAC3 frame: 1024 samples per channel

void setup() {
  Serial.begin(115200);

  ATRACConfig cfg;
  cfg.sampleRate = 44100;
  cfg.channels = 2;
  cfg.blockAlign = 192;  // bytes per frame
  cfg.extradata = atrac3_extradata;
  cfg.extradataSize = sizeof(atrac3_extradata);

  if (!decoder.begin(cfg)) {
    Serial.println("decoder setup failed");
    return;
  }

  for (size_t pos = 0; pos + cfg.blockAlign <= sizeof(atrac3_frames); pos += cfg.blockAlign) {
    int samples = decoder.decode(atrac3_frames + pos, cfg.blockAlign, pcm, 1024 * 2);
    Serial.print("frame at ");
    Serial.print(pos);
    Serial.print(": ");
    Serial.print(samples);
    Serial.print(" samples/channel, first L/R: ");
    Serial.print(pcm[0]);
    Serial.print(" / ");
    Serial.println(pcm[1]);
  }
  decoder.end();
}

void loop() {}
