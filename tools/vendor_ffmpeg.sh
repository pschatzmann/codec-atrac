#!/bin/bash
# Copies the FFmpeg sources needed for the ATRAC decoders into ../src.
# usage: tools/vendor_ffmpeg.sh <path-to-FFmpeg-checkout>
# Header dependencies are discovered with `gcc -MM`, so only what is needed is copied.
set -euo pipefail
FF=$(realpath "${1:?usage: $0 <FFmpeg checkout>}")
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../src"

SOURCES="
libavcodec/atrac.c libavcodec/atrac1.c libavcodec/atrac3.c
libavcodec/atrac3plus.c libavcodec/atrac3plusdec.c libavcodec/atrac3plusdsp.c
libavcodec/atrac9dec.c libavcodec/vlc.c libavcodec/sinewin.c
libavutil/mem.c libavutil/tx.c libavutil/tx_float.c
libavutil/float_dsp.c libavutil/float_scalarproduct.c libavutil/md5.c
libavutil/reverse.c libavutil/log2_tab.c
"
# Replaced by small implementations in src/atrac_port.c: log.c, channel_layout.c,
# lfg.c (+crc.c), mathematics.c (+rational.c) and their dependencies (bprint,
# avstring, avsscanf, time).

rm -rf "$SRC/libavcodec" "$SRC/libavutil"
mkdir -p "$SRC"
cp "$HERE/config.h" "$SRC/config.h"
mkdir -p "$SRC/libavutil"
cp "$HERE/avconfig.h" "$SRC/libavutil/avconfig.h"
cp "$FF/LICENSE.md" "$FF/COPYING.LGPLv2.1" "$SRC/" 2>/dev/null || true

FILES=$({ for f in $SOURCES; do
  echo "$f"
  gcc -MM -MG -DHAVE_AV_CONFIG_H -I"$FF" -I"$HERE" "$FF/$f" \
    | tr -d '\\' | tr ' ' '\n' | grep -E "^$FF/lib" | sed "s#^$FF/##"
done
# headers needed by the glue code in src/atrac_port.c
gcc -MM -MG -DHAVE_AV_CONFIG_H -I"$FF" -I"$HERE" -I"$SRC" "$SRC/atrac_port.c" \
  | tr -d '\\' | tr ' ' '\n' | grep -E "^$FF/lib" | sed "s#^$FF/##"
} | sort -u)

for f in $FILES; do
  mkdir -p "$SRC/$(dirname "$f")"
  cp "$FF/$f" "$SRC/$f"
  chmod 644 "$SRC/$f"
  # Arduino cannot pass -DHAVE_AV_CONFIG_H, so patch it into every C file.
  # tx_template.c is #included by tx_float.c; Arduino/CMake would also compile
  # it on its own, so only let it through when included as a float template.
  case "$f" in */tx_template.c)
    sed -i '1i #ifdef TX_FLOAT' "$SRC/$f"; echo '#endif /* TX_FLOAT */' >> "$SRC/$f";;
  esac
  case "$f" in *.c)
    sed -i '1i #ifndef HAVE_AV_CONFIG_H\n#define HAVE_AV_CONFIG_H 1\n#endif' "$SRC/$f";;
  esac
done
# --- ROM tables: run the original (RAM based) VLC table init on the host once and
# --- store the result as const data (flash) instead of building it at runtime.
GEN=$(mktemp -d); trap 'rm -rf "$GEN"' EXIT
sed '/static VLCElem vlc_buf\[24812\];/d' "$SRC/libavcodec/atrac9dec.c" > "$GEN/atrac9dec_gen.c"
cp "$SRC/libavcodec/atrac3plus.c" "$GEN/atrac3plus_gen.c"
CFLAGS="-O1 -w -DHAVE_AV_CONFIG_H -I$SRC -I$SRC/libavcodec -I$GEN -I$HERE"
LIBS=$(ls "$SRC"/libavcodec/*.c "$SRC"/libavutil/*.c "$SRC"/atrac_port.c)
gcc $CFLAGS "$HERE/gen_vlc_atrac9.c" $(echo "$LIBS" | grep -v '/atrac9dec.c') -lm -o "$GEN/gen9"
gcc $CFLAGS "$HERE/gen_vlc_atrac3p.c" $(echo "$LIBS" | grep -v '/atrac3plus.c') -lm -o "$GEN/gen3p"
"$GEN/gen9" "$SRC/libavcodec/atrac9_vlc_tables.h"
"$GEN/gen3p" "$SRC/libavcodec/atrac3plus_vlc_tables.h"

python3 - "$SRC" <<'PY'
import sys
src = sys.argv[1] + "/libavcodec/"
# ATRAC9: replace runtime table building by lookups into the const table
p = src + "atrac9dec.c"; s = open(p).read()
a = s.index("static av_cold const VLCElem *atrac9_init_vlc(")
b = s.index("static av_cold int atrac9_decode_init(")
s = s[:a] + """#include "atrac9_vlc_tables.h"

static av_cold void atrac9_init_static(void)
{
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 8; j++) {
            if (at9_sf_off[i][j] >= 0)
                sf_vlc[i][j] = &at9_vlc_data[at9_sf_off[i][j]];
            for (int k = 0; k < 4; k++)
                if (at9_coeff_off[i][j][k] >= 0)
                    coeff_vlc[i][j][k] = &at9_vlc_data[at9_coeff_off[i][j][k]];
        }
    }
}

""" + s[b:]
open(p, "w").write(s)
# ATRAC3+
p = src + "atrac3plus.c"; s = open(p).read()
s = s.replace("static VLCElem tables_data[154276];\n", '#include "atrac3plus_vlc_tables.h"\n\n')
s = s.replace("static VLC wl_vlc_tabs[4];", "static VLC wl_vlc_tabs[4];", 1)
a = s.index("/**\n * Generate canonical VLC table")
b = s.index("/**\n * Decode number of coded quantization units.")
s = s[:a] + """static av_cold void fill_vlcs(VLC *dst, const At3pVlcDesc *d, int n)
{
    for (int i = 0; i < n; i++) {
        dst[i].bits            = d[i].bits;
        dst[i].table           = (VLCElem *)&tables_data[d[i].off];
        dst[i].table_size      = d[i].size;
        dst[i].table_allocated = d[i].alloc;
    }
}

av_cold void ff_atrac3p_init_vlcs(void)
{
    fill_vlcs(wl_vlc_tabs,   at3p_wl_desc,   4);
    fill_vlcs(ct_vlc_tabs,   at3p_ct_desc,   4);
    fill_vlcs(sf_vlc_tabs,   at3p_sf_desc,   8);
    fill_vlcs(spec_vlc_tabs, at3p_spec_desc, 112);
    fill_vlcs(gain_vlc_tabs, at3p_gain_desc, 11);
    fill_vlcs(tone_vlc_tabs, at3p_tone_desc, 7);
}

""" + s[b:]
open(p, "w").write(s)
PY

# Codec gates: disabled codecs must not compile (their tables are large).
gate() { # file macro
  sed -i "1i #include \"atrac_config.h\"\n#if $2" "$SRC/$1"; echo '#endif' >> "$SRC/$1"
}
gate libavcodec/atrac1.c ATRAC_ENABLE_ATRAC1
gate libavcodec/atrac3.c ATRAC_ENABLE_ATRAC3
gate libavcodec/atrac9dec.c ATRAC_ENABLE_ATRAC9
for f in atrac3plus atrac3plusdec atrac3plusdsp; do gate libavcodec/$f.c ATRAC_ENABLE_ATRAC3PLUS; done
# ensure the HAVE_AV_CONFIG_H header comes first again
for f in atrac1 atrac3 atrac9dec atrac3plus atrac3plusdec atrac3plusdsp; do
  sed -i '/^#ifndef HAVE_AV_CONFIG_H$/,/^#endif$/d' "$SRC/libavcodec/$f.c"
  sed -i '1i #ifndef HAVE_AV_CONFIG_H\n#define HAVE_AV_CONFIG_H 1\n#endif' "$SRC/libavcodec/$f.c"
done

# RAM trimming: the ATRAC decoders only need FFTs up to 4096 points (2048 point
# MDCT) and sine windows up to 128. Drop the larger static tables.
sed -i '/SR_TABLE(\(8192\|16384\|32768\|65536\|131072\))/d; /DECL_SR_CODELET(\(8192\|16384\|32768\|65536\|131072\),/d; /ff_tx_fft\(8192\|16384\|32768\|65536\|131072\)_ns_def/d' "$SRC/libavutil/tx_template.c"
sed -i '/^SINETABLE(\(256\|512\|1024\|2048\|4096\|8192\));/d; /^SINETABLE( *\(256\|512\|1024\|2048\|4096\|8192\));/d; s/ff_sine_\(256\|512\|1024\|2048\|4096\|8192\),/NULL,/g' "$SRC/libavcodec/sinewin_tablegen.h"

# Portability patch: int32_t is "long" on newlib targets (ESP32, ARM), so int32_t
# arrays must not be accessed through "int *" (hard error in recent GCC).
sed -i 's/const int \*baseline/const int32_t *baseline/; s/^\( *\)int \*coeffs = /\1int32_t *coeffs = /' "$SRC/libavcodec/atrac9dec.c"

echo "FFmpeg commit: $(git -C "$FF" rev-parse HEAD 2>/dev/null || echo unknown)"
echo "$(echo "$FILES" | wc -l) files copied"
