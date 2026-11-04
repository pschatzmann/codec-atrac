# Arduino ATRAC Decoder Library

ATRAC (Adaptive Transform Acoustic Coding) is a family of lossy audio codecs developed by Sony.
This library decodes **raw ATRAC frames to PCM** on Arduino and embedded platforms, using the
decoders from [FFmpeg](https://ffmpeg.org/).

| Codec | Used by | Class enum |
|-------|---------|-----------|
| ATRAC1 | MiniDisc SP | `ATRAC1` |
| ATRAC3 | MiniDisc LP, PSP `.at3` | `ATRAC3` |
| ATRAC3+ | PSP, Sony players `.oma`/`.aa3` | `ATRAC3PLUS` |
| ATRAC9 | PS Vita, PS4 | `ATRAC9` |

Container parsing (WAV, OMA, AEA, ...) is **not** included: you pass the stream parameters and
one frame at a time.

## Usage

```cpp
#include "ATRACDecoder.h"

ATRACDecoder decoder(ATRAC3);

ATRACConfig cfg;
cfg.sampleRate = 44100;
cfg.channels = 2;
cfg.blockAlign = 192;                 // bytes per frame
cfg.extradata = extradata;            // codec specific header, see below
cfg.extradataSize = sizeof(extradata);
decoder.begin(cfg);

int16_t pcm[2048];                    // interleaved
int n = decoder.decode(frame, 192, pcm, 2048);  // samples per channel (1024)
```

`decodeFloat()` / `channel()` give access to the planar float output (-1..1) instead.
See [examples/atrac3-decode](examples/atrac3-decode).

Parameters (taken from the container header, e.g. the WAV `fmt ` chunk):

* **ATRAC3**: sample rate, channels, block align and the 10/12/14 byte extradata.
* **ATRAC3+**: sample rate, channels, block align.
* **ATRAC9**: the 12 byte extradata (rate and channel layout are read from it); `blockAlign`
  is derived from it when left at 0.
* **ATRAC1**: sample rate (44100), channels, block align (212 per channel).

## Memory

The Huffman tables are generated at vendoring time and stored as `const` data (flash), so they use
no RAM. What remains in RAM is the per-decoder state (heap, allocated in `begin()`) plus about
12 KB of shared static data (FFT and sine window tables).

| Codec | heap after `begin()` | flash (tables + code) |
|-------|----------------------|-----------------------|
| ATRAC3 | ~ 43 KB | ~ 40 KB |
| ATRAC1 | ~ 61 KB | ~ 40 KB |
| ATRAC9 | ~ 63 KB | ~ 130 KB |
| ATRAC3+ | ~ 110 KB | ~ 700 KB (disabled by default when `ARDUINO` is defined) |

Disabled codecs are not compiled. Configure them in [src/atrac_config.h](src/atrac_config.h) or with
`-DATRAC_ENABLE_ATRAC1/3/3PLUS/9=0|1`.

Build check (example sketch): classic ESP32 with ATRAC1/3/9 uses 46 KB of static RAM and 448 KB flash;
with ATRAC3+ enabled 58 KB RAM and 1.09 MB flash.

## Tests

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

The tests decode sample streams and compare them with the output of the `ffmpeg` binary (ATRAC1,
ATRAC3+ and ATRAC9 bit exact, ATRAC3 within ±16 LSB), and cover argument validation, truncated and
random input. The streams come from <https://samples.ffmpeg.org>; the ATRAC9 stream is an excerpt of a
192 kbps 48 kHz stereo `snd0.at9`. See [test/data/README.md](test/data/README.md).

## Performance

Measured on an ESP32-S3 at 240 MHz with [examples/benchmark](examples/benchmark) (default Arduino
build, 44.1 kHz stereo):

| Codec | per frame | speed | `begin()` | heap after `begin()` |
|-------|-----------|-------|-----------|----------------------|
| ATRAC1 (512 samples/frame) | 1.74 ms | 6.7x real time | 5.7 ms | 61 KB |
| ATRAC3 66 kbps (1024 samples) | 3.75 ms | 6.2x real time | 8.0 ms | 43 KB |
| ATRAC3+ (2048 samples) | 10.9 ms | 4.2x real time | 27 ms | 110 KB |
| ATRAC9 192 kbps, 48 kHz (1024 samples) | 2.51 ms | 8.5x real time | 4.5 ms | 63 KB |

The ATRAC3 output on the board matches the desktop build bit for bit.

## Diagnostics

The decoders report problems (corrupt frames, bad headers) through an optional callback:

```cpp
ATRACDecoder::setLogger([](int level, const char* msg) { Serial.print(msg); });
```

Levels follow FFmpeg: 16 = error, 24 = warning (default limit), 32 = info. Without a logger
messages go to `stderr` on desktop and are dropped on Arduino.

## Source layout

* `src/ATRACDecoder.*` – the public C++ API.
* `src/atrac_port.*` – C facade over the FFmpeg decoders, plus small replacements for the
  parts of libavcodec/libavutil that are not vendored: buffer allocation, logging, channel
  layout, `av_gcd`, and the noise generator seeding used by ATRAC9.
* `src/atrac_config.h` – selects the codecs; `src/config.h`, `src/libavutil/avconfig.h` replace the
  files FFmpeg's `configure` would generate.
* `src/libavcodec`, `src/libavutil` – copied from FFmpeg (commit `5048d707`) by
  `./download_ffmpeg_atrac.sh`, which runs [tools/vendor_ffmpeg.sh](tools/vendor_ffmpeg.sh). Only the
  files the ATRAC decoders need are copied (header dependencies are found with `gcc -MM`), and a few
  documented patches are applied: Arduino cannot pass `-DHAVE_AV_CONFIG_H`, codec gates, smaller
  FFT/sine tables, an `int32_t` pointer fix, and the Huffman tables precomputed by
  `tools/gen_vlc_*.c` into `atrac9_vlc_tables.h` / `atrac3plus_vlc_tables.h`.

To update FFmpeg, run `./download_ffmpeg_atrac.sh [path-to-FFmpeg-checkout]` and `ctest`.

## License

FFmpeg's ATRAC decoders and the supporting libavutil/libavcodec code are licensed under
**LGPL 2.1 or later** (see [COPYING.LGPLv2.1](COPYING.LGPLv2.1)); the glue code in this library
uses the same license.
