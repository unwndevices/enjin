/**
 * @file parse_number.hpp
 * @brief Locale-independent decimal → double for the Shape parsers (internal)
 *
 * `strtod` reads the decimal point from the C locale, so under a
 * decimal-comma locale "10.5" would parse as 10 and a pack would change
 * bytes with the host's settings.  The callers have already checked the
 * syntax (sign, digits, '.', exponent); this only converts.
 */
#pragma once

#include <cmath>
#include <cstdlib>

#if defined(__cpp_lib_to_chars) || (defined(__has_include) && __has_include(<version>))
#include <version>
#endif
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
#include <charconv>
#include <system_error>
#define ENJIN2_FROM_CHARS_DOUBLE 1
#endif

namespace enjin2 {
namespace import_detail {

/// [s, end) as a double; false when it isn't finite.  An optional leading '+' is allowed.
inline bool parseDouble(const char* s, const char* end, double& out) {
    if (s < end && *s == '+') ++s;
#if ENJIN2_FROM_CHARS_DOUBLE
    const auto r = std::from_chars(s, end, out);
    return r.ec == std::errc() && r.ptr == end && std::isfinite(out);
#else
    // No floating-point from_chars (older libc++, e.g. Emscripten): its musl
    // strtod has no locale-dependent decimal point, so it is the same thing.
    char buf[80];
    const size_t n = static_cast<size_t>(end - s);
    if (n >= sizeof buf) return false;
    for (size_t i = 0; i < n; ++i) buf[i] = s[i];
    buf[n] = 0;
    char* stop = nullptr;
    out = std::strtod(buf, &stop);
    return stop == buf + n && std::isfinite(out);
#endif
}

}  // namespace import_detail
}  // namespace enjin2
