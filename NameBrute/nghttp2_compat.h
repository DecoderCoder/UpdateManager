// nghttp2.h requires ssize_t, which MSVC does not provide in C or C++ mode.
// Upstream nghttp2's generated config.h maps ssize_t to int for such builds;
// we use a dedicated tag type instead so the project-wide `#define
// ssize_t __nb_compat_ssize` does not break other headers that declare their
// own `ssize_t` (e.g. httplib's `using ssize_t = ptrdiff_t;` simply shadows
// this typedef, which is legal).
#ifndef NAMEBRUTE_NGHTTP2_COMPAT_H
#define NAMEBRUTE_NGHTTP2_COMPAT_H

#include <stddef.h>

typedef ptrdiff_t __nb_compat_ssize;

#endif // !NAMEBRUTE_NGHTTP2_COMPAT_H
