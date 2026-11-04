/*
 * Compile time selection of the codecs. Disabled codecs are not compiled, so
 * their (large) static tables do not use any RAM.
 *
 * Set the macros before including ATRACDecoder.h is NOT enough, since the
 * library sources are compiled separately: edit this file, or pass -D flags
 * (e.g. in build_opt.h / platform.local.txt / CMake).
 */
#ifndef ATRAC_CONFIG_H
#define ATRAC_CONFIG_H

#ifndef ATRAC_ENABLE_ATRAC1
#define ATRAC_ENABLE_ATRAC1 1
#endif
#ifndef ATRAC_ENABLE_ATRAC3
#define ATRAC_ENABLE_ATRAC3 1
#endif
#ifndef ATRAC_ENABLE_ATRAC9
#define ATRAC_ENABLE_ATRAC9 1
#endif
/* ATRAC3+ adds ~700 KB of flash (const tables): off by default on Arduino. */
#ifndef ATRAC_ENABLE_ATRAC3PLUS
#if defined(ARDUINO)
#define ATRAC_ENABLE_ATRAC3PLUS 0
#else
#define ATRAC_ENABLE_ATRAC3PLUS 1
#endif
#endif

#endif
