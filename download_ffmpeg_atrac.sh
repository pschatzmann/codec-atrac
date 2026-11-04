#!/bin/bash
# Re-vendors the FFmpeg ATRAC decoder sources into src/.
# usage: ./download_ffmpeg_atrac.sh [path-to-existing-FFmpeg-checkout]
# Without an argument FFmpeg is cloned into ./original/FFmpeg and updated.
set -euo pipefail
cd "$(dirname "$0")"
FF="${1:-original/FFmpeg}"
if [ ! -d "$FF/.git" ] && [ "$#" -eq 0 ]; then
  mkdir -p original
  git clone https://github.com/FFmpeg/FFmpeg.git "$FF"
fi
tools/vendor_ffmpeg.sh "$FF"
