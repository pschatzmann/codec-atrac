/* Host tool: runs the original ATRAC9 VLC table init and dumps the result as
 * constant data. Compiled together with a copy of atrac9dec.c in which the
 * local "vlc_buf" declaration has been removed (see vendor_ffmpeg.sh). */
#include "gen_vlc_common.h"
#include <stdlib.h>
static VLCElem vlc_buf[24812];
#include "atrac9dec_gen.c"

int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "w");
  if (!f) return 1;
  atrac9_init_static();
  size_t n = vlc_used(vlc_buf, 24812);
  fputs(VLC_MACRO_DEF, f);
  vlc_dump(f, "at9_vlc_data", vlc_buf, n);
  fprintf(f, "static const int32_t at9_sf_off[2][8] = {\n");
  for (int i = 0; i < 2; i++) {
    fprintf(f, "{");
    for (int j = 0; j < 8; j++)
      fprintf(f, "%ld,", sf_vlc[i][j] ? (long)(sf_vlc[i][j] - vlc_buf) : -1L);
    fprintf(f, "},\n");
  }
  fprintf(f, "};\nstatic const int32_t at9_coeff_off[2][8][4] = {\n");
  for (int i = 0; i < 2; i++) {
    fprintf(f, "{");
    for (int j = 0; j < 8; j++) {
      fprintf(f, "{");
      for (int k = 0; k < 4; k++)
        fprintf(f, "%ld,", coeff_vlc[i][j][k] ? (long)(coeff_vlc[i][j][k] - vlc_buf) : -1L);
      fprintf(f, "},");
    }
    fprintf(f, "},\n");
  }
  fprintf(f, "};\n");
  fclose(f);
  return 0;
}
