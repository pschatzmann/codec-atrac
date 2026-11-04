/* Host tool: runs the original ATRAC3+ VLC table init and dumps the result as
 * constant data. Compiled together with atrac3plus.c. */
#include "gen_vlc_common.h"
#include "atrac3plus_gen.c"

static void desc(FILE *f, const char *name, const VLC *v, int n) {
  fprintf(f, "static const At3pVlcDesc %s[%d] = {\n", name, n);
  for (int i = 0; i < n; i++)
    fprintf(f, "{%d,%ld,%d,%d},\n", v[i].bits, (long)(v[i].table - tables_data),
            v[i].table_size, v[i].table_allocated);
  fprintf(f, "};\n");
}

int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "w");
  if (!f) return 1;
  ff_atrac3p_init_vlcs();
  fputs(VLC_MACRO_DEF, f);
  size_t n = vlc_used(tables_data, 154276), end = 0;
  const VLC *all[] = {wl_vlc_tabs, ct_vlc_tabs, sf_vlc_tabs, spec_vlc_tabs, gain_vlc_tabs, tone_vlc_tabs};
  const int cnt[] = {4, 4, 8, 112, 11, 7};
  for (int a = 0; a < 6; a++)
    for (int i = 0; i < cnt[a]; i++) {
      size_t e = (size_t)(all[a][i].table - tables_data) + all[a][i].table_allocated;
      if (e > end) end = e;
    }
  if (end > n) n = end;
  vlc_dump(f, "tables_data", tables_data, n);
  fprintf(f, "typedef struct At3pVlcDesc { int bits; int off; int size, alloc; } At3pVlcDesc;\n");
  desc(f, "at3p_wl_desc", wl_vlc_tabs, 4);
  desc(f, "at3p_ct_desc", ct_vlc_tabs, 4);
  desc(f, "at3p_sf_desc", sf_vlc_tabs, 8);
  desc(f, "at3p_spec_desc", spec_vlc_tabs, 112);
  desc(f, "at3p_gain_desc", gain_vlc_tabs, 11);
  desc(f, "at3p_tone_desc", tone_vlc_tabs, 7);
  fclose(f);
  return 0;
}
