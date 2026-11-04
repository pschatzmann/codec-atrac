# Test data

Samples come from <https://samples.ffmpeg.org/A-codecs/>:

| File | Origin |
|------|--------|
| `atrac3_stereo_66k.wav` | `ATRAC3/sample.ATRAC3.66kbps.44100Hz.Stereo.wav` |
| `atrac1_chirp.pkts` | first 200 frames of `ATRAC1/Test tones disc - Chirp.aea`, header stripped (raw 424 byte frames) |
| `atrac3plus_short.pkts` | first second of `ATRAC3+/at3p_sample1.oma`, raw packets (1488 bytes) |
| `*.ref.s16` | first 16384 samples/channel (interleaved s16le) decoded by `ffmpeg 5.1.2`, `-f s16le` |
| `atrac9_stereo.pkts`, `.extradata` | first 40 frames (512 bytes each) and the 12 byte config header of a 192 kbps 48 kHz stereo `snd0.at9` |
| `atrac9_stereo.ref.s16` | first 40960 samples/channel decoded by `ffmpeg 5.1.2` |

FFmpeg has no ATRAC encoder, so the streams cannot be generated here.
