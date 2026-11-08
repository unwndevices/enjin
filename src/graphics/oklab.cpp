// sRGB to Oklab, float and integer (ADR-0026, Tomodachi #459). See oklab.hpp.

#include "../../include/enjin2/graphics/oklab.hpp"

#include <cmath>

namespace enjin2 {

// --- float: the reference (port of enjin_assets/oklab.py) ---------------------

namespace {

float srgbToLinear(uint8_t c8) {
    const float c = c8 / 255.0f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Ottosson's two matrices: linear sRGB -> LMS (M1) and LMS' -> Oklab (M2).
constexpr double M1[3][3] = {
    {0.4122214708, 0.5363325363, 0.0514459929},
    {0.2119034982, 0.6806995451, 0.1073969566},
    {0.0883024619, 0.2817188376, 0.6299787005},
};
constexpr double M2[3][3] = {
    {0.2104542553, 0.7936177850, -0.0040720468},
    {1.9779984951, -2.4285922050, 0.4505937099},
    {0.0259040371, 0.7827717662, -0.8086757660},
};

} // namespace

Oklab srgbToOklab(uint8_t r, uint8_t g, uint8_t b) {
    const float lin[3] = {srgbToLinear(r), srgbToLinear(g), srgbToLinear(b)};
    float lms[3];
    for (int i = 0; i < 3; ++i)
        lms[i] = std::cbrt(float(M1[i][0]) * lin[0] + float(M1[i][1]) * lin[1] +
                           float(M1[i][2]) * lin[2]);
    Oklab o;
    o.L = float(M2[0][0]) * lms[0] + float(M2[0][1]) * lms[1] + float(M2[0][2]) * lms[2];
    o.a = float(M2[1][0]) * lms[0] + float(M2[1][1]) * lms[1] + float(M2[1][2]) * lms[2];
    o.b = float(M2[2][0]) * lms[0] + float(M2[2][1]) * lms[1] + float(M2[2][2]) * lms[2];
    return o;
}

// --- integer: what the dither runs --------------------------------------------
//
// Linear light from a table (Q20), LMS by unsigned Q11 multiplies (Q24, so
// dark colours keep their bits into the cube root), the cube root from a table
// (out at 1.0 = 65535), Oklab by Q12 multiplies. No float and no libm, so the
// bits are the same on every target.

namespace {

// round(linear(i / 255) * 2^20), the sRGB transfer curve. Made with
//   [round((c/12.92 if c <= 0.04045 else ((c+0.055)/1.055)**2.4) * 2**20)
//    for c in (i/255 for i in range(256))]
constexpr uint32_t LINEAR_Q20[256] = {
          0,     318,     637,     955,    1273,    1591,    1910,    2228,    2546,    2864,
       3183,    3509,    3855,    4220,    4605,    5009,    5433,    5878,    6343,    6828,
       7335,    7863,    8413,    8984,    9578,   10193,   10832,   11492,   12176,   12883,
      13614,   14368,   15145,   15947,   16773,   17624,   18499,   19399,   20324,   21274,
      22250,   23251,   24278,   25331,   26410,   27516,   28648,   29807,   30993,   32205,
      33445,   34713,   36008,   37331,   38681,   40060,   41467,   42903,   44367,   45860,
      47381,   48932,   50512,   52121,   53760,   55428,   57127,   58855,   60613,   62402,
      64221,   66071,   67951,   69862,   71805,   73778,   75783,   77819,   79886,   81985,
      84117,   86280,   88475,   90702,   92962,   95254,   97579,   99937,  102328,  104751,
     107208,  109698,  112222,  114779,  117370,  119994,  122653,  125345,  128072,  130833,
     133628,  136458,  139323,  142222,  145156,  148125,  151130,  154169,  157244,  160355,
     163501,  166683,  169900,  173154,  176443,  179769,  183131,  186530,  189964,  193436,
     196944,  200489,  204072,  207691,  211347,  215041,  218772,  222540,  226346,  230190,
     234071,  237991,  241948,  245944,  249978,  254050,  258161,  262310,  266498,  270724,
     274990,  279294,  283637,  288020,  292442,  296903,  301404,  305944,  310523,  315143,
     319802,  324502,  329241,  334021,  338840,  343700,  348601,  353542,  358523,  363546,
     368609,  373713,  378858,  384044,  389271,  394539,  399849,  405201,  410594,  416028,
     421504,  427022,  432582,  438184,  443828,  449515,  455243,  461014,  466827,  472683,
     478582,  484523,  490507,  496534,  502604,  508717,  514873,  521072,  527315,  533601,
     539930,  546303,  552720,  559181,  565685,  572234,  578826,  585462,  592143,  598868,
     605637,  612451,  619309,  626211,  633159,  640151,  647188,  654270,  661397,  668569,
     675786,  683048,  690356,  697709,  705108,  712552,  720042,  727577,  735159,  742786,
     750459,  758178,  765944,  773755,  781613,  789517,  797468,  805465,  813509,  821599,
     829736,  837920,  846151,  854429,  862753,  871125,  879545,  888011,  896525,  905086,
     913695,  922351,  931055,  939807,  948606,  957453,  966349,  975292,  984283,  993323,
    1002411, 1011547, 1020731, 1029964, 1039246, 1048576,
};

// A matrix entry as a rounded fixed-point integer (worked out by the compiler).
constexpr int32_t fixedQ(double c, int bits) {
    return c >= 0 ? int32_t(c * (1 << bits) + 0.5) : -int32_t(-c * (1 << bits) + 0.5);
}

// v / 2^bits rounded half away from zero, for either sign.
constexpr int32_t roundShift(int32_t v, int bits) {
    const int32_t half = 1 << (bits - 1);
    return v >= 0 ? (v + half) >> bits : -((-v + half) >> bits);
}

// The cube root of x / 2^24 (x in 0..2^24), times 65535. x is first moved up
// by whole factors of 8 into [2^21, 2^24], where the curve is gentle, then
// read from a table of 2^13-wide steps with a straight line between entries,
// and the result is halved once per factor of 8.
constexpr int LMS_BITS = 24;
constexpr uint32_t LMS_ONE = 1u << LMS_BITS;
constexpr int CBRT_STEP_BITS = 13;
constexpr uint32_t CBRT_LOW = LMS_ONE >> 3;  // the table starts here
constexpr int CBRT_FIRST = int(CBRT_LOW >> CBRT_STEP_BITS);
constexpr int CBRT_COUNT = int(LMS_ONE >> CBRT_STEP_BITS) - CBRT_FIRST + 1;

struct CbrtTable {
    uint16_t v[CBRT_COUNT];
};

// round(cbrt(n / 2^s)) by bisection, for answers below 2^17.
constexpr uint32_t icbrtRound(uint64_t n, int s) {
    uint64_t lo = 0, hi = 1u << 17;
    while (hi - lo > 1) {
        const uint64_t mid = (lo + hi) / 2;
        if ((mid * mid * mid) << s <= n) lo = mid; else hi = mid;
    }
    // lo = floor; round up when (lo + 1/2)^3 * 2^s <= n.
    const uint64_t up = 2 * lo + 1;
    return uint32_t((up * up * up) << s <= 8 * n ? lo + 1 : lo);
}

constexpr CbrtTable makeCbrtTable() {
    CbrtTable t{};
    for (int i = 0; i < CBRT_COUNT; ++i) {
        // cbrt(j 2^13 / 2^24) * 65535 = cbrt(j * 65535^3 / 2^11)
        const uint64_t j = uint64_t(CBRT_FIRST + i);
        const uint32_t c = icbrtRound(j * 65535ull * 65535ull * 65535ull,
                                      LMS_BITS - CBRT_STEP_BITS);
        t.v[i] = uint16_t(c > 65535 ? 65535 : c);
    }
    return t;
}

constexpr CbrtTable CBRT = makeCbrtTable();

int32_t cbrtQ(uint32_t x) {
    if (x == 0) return 0;
    uint32_t m = x;
    int k = 0;
    while (m < CBRT_LOW) {
        m <<= 3;
        ++k;
    }
    const uint32_t i = (m >> CBRT_STEP_BITS) - CBRT_FIRST;
    const int32_t f = int32_t(m & ((1u << CBRT_STEP_BITS) - 1));
    const int32_t lo = CBRT.v[i];
    const int32_t y = i + 1 < uint32_t(CBRT_COUNT)
                          ? lo + roundShift((CBRT.v[i + 1] - lo) * f, CBRT_STEP_BITS)
                          : lo;
    return k == 0 ? y : roundShift(y, k);
}

constexpr int Q1 = 11;  // M1's fraction bits: Q20 * Q11 sums stay inside uint32
constexpr int Q2 = 12;  // M2's: its sums stay inside int32 at 65535 inputs

constexpr uint32_t M1Q[3][3] = {
    {uint32_t(fixedQ(M1[0][0], Q1)), uint32_t(fixedQ(M1[0][1], Q1)), uint32_t(fixedQ(M1[0][2], Q1))},
    {uint32_t(fixedQ(M1[1][0], Q1)), uint32_t(fixedQ(M1[1][1], Q1)), uint32_t(fixedQ(M1[1][2], Q1))},
    {uint32_t(fixedQ(M1[2][0], Q1)), uint32_t(fixedQ(M1[2][1], Q1)), uint32_t(fixedQ(M1[2][2], Q1))},
};
constexpr int32_t M2Q[3][3] = {
    {fixedQ(M2[0][0], Q2), fixedQ(M2[0][1], Q2), fixedQ(M2[0][2], Q2)},
    {fixedQ(M2[1][0], Q2), fixedQ(M2[1][1], Q2), fixedQ(M2[1][2], Q2)},
    {fixedQ(M2[2][0], Q2), fixedQ(M2[2][1], Q2), fixedQ(M2[2][2], Q2)},
};

} // namespace

OklabQ srgbToOklabQ(uint8_t r, uint8_t g, uint8_t b) {
    const uint32_t lin[3] = {LINEAR_Q20[r], LINEAR_Q20[g], LINEAR_Q20[b]};
    int32_t lms[3];
    constexpr int down = 20 + Q1 - LMS_BITS;
    for (int i = 0; i < 3; ++i) {
        uint32_t v = (M1Q[i][0] * lin[0] + M1Q[i][1] * lin[1] + M1Q[i][2] * lin[2] +
                      (1u << (down - 1))) >> down;
        if (v > LMS_ONE) v = LMS_ONE;  // a row's Q11 entries may sum to just over 1
        lms[i] = cbrtQ(v);
    }
    OklabQ o;
    o.L = roundShift(M2Q[0][0] * lms[0] + M2Q[0][1] * lms[1] + M2Q[0][2] * lms[2], Q2);
    o.a = roundShift(M2Q[1][0] * lms[0] + M2Q[1][1] * lms[1] + M2Q[1][2] * lms[2], Q2);
    o.b = roundShift(M2Q[2][0] * lms[0] + M2Q[2][1] * lms[1] + M2Q[2][2] * lms[2], Q2);
    return o;
}

} // namespace enjin2
