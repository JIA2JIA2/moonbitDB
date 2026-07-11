#ifdef __cplusplus
extern "C" {
#endif

#include "moonbit.h"
#include "moonbit_simd.h"

#ifdef _MSC_VER
#define _Noreturn __declspec(noreturn)
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wshift-op-parentheses"
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif

MOONBIT_EXPORT _Noreturn void moonbit_panic(void);
MOONBIT_EXPORT void *moonbit_malloc_array(enum moonbit_block_kind kind,
                                          int elem_size_shift, int32_t len);
int memcmp(const void *s1, const void *s2, size_t n);
MOONBIT_EXPORT int moonbit_val_array_equal_sized(const void *lhs,
                                                 const void *rhs,
                                                 int32_t elem_size);
MOONBIT_EXPORT moonbit_string_t moonbit_add_string(moonbit_string_t s1,
                                                   moonbit_string_t s2);
MOONBIT_EXPORT void moonbit_unsafe_bytes_blit(moonbit_bytes_t dst,
                                              int32_t dst_start,
                                              moonbit_bytes_t src,
                                              int32_t src_offset, int32_t len);
MOONBIT_EXPORT moonbit_string_t moonbit_unsafe_bytes_sub_string(
    moonbit_bytes_t bytes, int32_t start, int32_t len);
MOONBIT_EXPORT int32_t moonbit_unsafe_val_array_blit(void *dst,
                                                     int32_t dst_offset,
                                                     void *src,
                                                     int32_t src_offset,
                                                     int32_t len,
                                                     int32_t elem_size);
MOONBIT_EXPORT int32_t moonbit_unsafe_ref_array_blit(void *dst,
                                                     int32_t dst_offset,
                                                     void *src,
                                                     int32_t src_offset,
                                                     int32_t len);
MOONBIT_EXPORT void moonbit_println(moonbit_string_t str);
MOONBIT_EXPORT moonbit_bytes_t *moonbit_get_cli_args(void);
MOONBIT_EXPORT void moonbit_runtime_init(int argc, char **argv);
MOONBIT_EXPORT void moonbit_drop_object(void *);
MOONBIT_EXPORT int32_t moonbit_utf16_len_from_utf8(moonbit_bytes_t src,
                                                   int32_t src_offset,
                                                   int32_t src_length);
MOONBIT_EXPORT int32_t moonbit_utf8_decode_into_utf16(
    moonbit_bytes_t src, int32_t src_offset, int32_t src_length,
    moonbit_string_t dst, int32_t dst_offset);
MOONBIT_EXPORT int32_t moonbit_utf8_decode_lossy_into_utf16(
    moonbit_bytes_t src, int32_t src_offset, int32_t src_length,
    moonbit_string_t dst, int32_t dst_offset);
MOONBIT_EXPORT int32_t moonbit_utf8_len_from_utf16(moonbit_string_t src,
                                                   int32_t src_offset,
                                                   int32_t src_length);
MOONBIT_EXPORT int32_t moonbit_utf8_encode_from_utf16(
    moonbit_string_t src, int32_t src_offset, int32_t src_length,
    moonbit_bytes_t dst, int32_t dst_offset);

#if !defined(_WIN64) && !defined(_WIN32)
void *malloc(size_t size);
void free(void *ptr);
#define libc_malloc malloc
#define libc_free free
#endif

// several important runtime functions are inlined
static void *moonbit_malloc_inlined(size_t size) {
  struct moonbit_object *ptr = (struct moonbit_object *)libc_malloc(
      sizeof(struct moonbit_object) + size);
  Moonbit_init_dynamic_rc(ptr, moonbit_BLOCK_KIND_REGULAR);
  return ptr + 1;
}

#define moonbit_malloc(obj) moonbit_malloc_inlined(obj)
#define moonbit_free(obj) libc_free(Moonbit_object_header(obj))

#define MOONBIT_RC_COUNT_UNIT ((int32_t)(1u << MOONBIT_RC_COUNT_SHIFT))
#define raw_rc_is_dynamic(rc) ((int32_t)(rc) >= MOONBIT_RC_COUNT_UNIT)
#define raw_rc_is_shared(rc) ((int32_t)(rc) >= (MOONBIT_RC_COUNT_UNIT * 2))

extern const uint32_t *moonbit_layout_table;

static void moonbit_incref_inlined(void *ptr) {
  struct moonbit_object *header = Moonbit_object_header(ptr);
  int32_t const rc = header->rc;
  if (raw_rc_is_dynamic(rc)) {
    Moonbit_increase_rc_count(header);
  }
}

#define moonbit_incref moonbit_incref_inlined

static void moonbit_decref_inlined(void *ptr) {
  struct moonbit_object *header = Moonbit_object_header(ptr);
  int32_t const rc = header->rc;
  if (raw_rc_is_shared(rc)) {
    header->rc = rc - MOONBIT_RC_COUNT_UNIT;
  } else if (raw_rc_is_dynamic(rc)) {
    moonbit_drop_object(ptr);
  }
}

#define moonbit_decref moonbit_decref_inlined

#define moonbit_unsafe_make_string moonbit_make_string

#if defined(MOONBIT_V128_NEON)
#define Moonbit_v128_make(lo, hi)                                             \
  vreinterpretq_u8_u64(                                                       \
      vcombine_u64(vcreate_u64((uint64_t)(lo)), vcreate_u64((uint64_t)(hi))))
#define Moonbit_v128_lo(v) vgetq_lane_u64(vreinterpretq_u64_u8(v), 0)
#define Moonbit_v128_hi(v) vgetq_lane_u64(vreinterpretq_u64_u8(v), 1)
#define Moonbit_v128_load_storage(p) vld1q_u8((const uint8_t *)(p))
#define Moonbit_v128_store_storage(p, v) vst1q_u8((uint8_t *)(p), (v))
#elif defined(MOONBIT_V128_SSE2)
#define Moonbit_v128_make(lo, hi) _mm_set_epi64x((int64_t)(hi), (int64_t)(lo))
#define Moonbit_v128_lo(v) ((uint64_t)_mm_cvtsi128_si64(v))
#define Moonbit_v128_hi(v) ((uint64_t)_mm_cvtsi128_si64(_mm_srli_si128((v), 8)))
#define Moonbit_v128_load_storage(p) _mm_loadu_si128((const __m128i *)(p))
#define Moonbit_v128_store_storage(p, v) _mm_storeu_si128((__m128i *)(p), (v))
#else
#define Moonbit_v128_make(lo, hi) ((moonbit_v128_t){(lo), (hi)})
#define Moonbit_v128_lo(v) ((v).lo)
#define Moonbit_v128_hi(v) ((v).hi)
#define Moonbit_v128_load_storage(p) (*(p))
#define Moonbit_v128_store_storage(p, v) (*(p) = (v))
#endif

// detect whether compiler builtins exist for advanced bitwise operations
#ifdef __has_builtin

#if __has_builtin(__builtin_clz)
#define HAS_BUILTIN_CLZ
#endif

#if __has_builtin(__builtin_ctz)
#define HAS_BUILTIN_CTZ
#endif

#if __has_builtin(__builtin_popcount)
#define HAS_BUILTIN_POPCNT
#endif

#if __has_builtin(__builtin_sqrt)
#define HAS_BUILTIN_SQRT
#endif

#if __has_builtin(__builtin_sqrtf)
#define HAS_BUILTIN_SQRTF
#endif

#if __has_builtin(__builtin_fabs)
#define HAS_BUILTIN_FABS
#endif

#if __has_builtin(__builtin_fabsf)
#define HAS_BUILTIN_FABSF
#endif

#endif

// if there is no builtin operators, use software implementation
#ifdef HAS_BUILTIN_CLZ
static inline int32_t moonbit_clz32(int32_t x) {
  return x == 0 ? 32 : __builtin_clz(x);
}

static inline int32_t moonbit_clz64(int64_t x) {
  return x == 0 ? 64 : __builtin_clzll(x);
}

#undef HAS_BUILTIN_CLZ
#else
// table for [clz] value of 4bit integer.
static const uint8_t moonbit_clz4[] = {4, 3, 2, 2, 1, 1, 1, 1,
                                       0, 0, 0, 0, 0, 0, 0, 0};

int32_t moonbit_clz32(uint32_t x) {
  /* The ideas is to:

     1. narrow down the 4bit block where the most signficant "1" bit lies,
        using binary search
     2. find the number of leading zeros in that 4bit block via table lookup

     Different time/space tradeoff can be made here by enlarging the table
     and do less binary search.
     One benefit of the 4bit lookup table is that it can fit into a single cache
     line.
  */
  int32_t result = 0;
  if (x > 0xffff) {
    x >>= 16;
  } else {
    result += 16;
  }
  if (x > 0xff) {
    x >>= 8;
  } else {
    result += 8;
  }
  if (x > 0xf) {
    x >>= 4;
  } else {
    result += 4;
  }
  return result + moonbit_clz4[x];
}

int32_t moonbit_clz64(uint64_t x) {
  int32_t result = 0;
  if (x > 0xffffffff) {
    x >>= 32;
  } else {
    result += 32;
  }
  return result + moonbit_clz32((uint32_t)x);
}
#endif

#ifdef HAS_BUILTIN_CTZ
static inline int32_t moonbit_ctz32(int32_t x) {
  return x == 0 ? 32 : __builtin_ctz(x);
}

static inline int32_t moonbit_ctz64(int64_t x) {
  return x == 0 ? 64 : __builtin_ctzll(x);
}

#undef HAS_BUILTIN_CTZ
#else
int32_t moonbit_ctz32(int32_t x) {
  /* The algorithm comes from:

       Leiserson, Charles E. et al. “Using de Bruijn Sequences to Index a 1 in a
     Computer Word.” (1998).

     The ideas is:

     1. leave only the least significant "1" bit in the input,
        set all other bits to "0". This is achieved via [x & -x]
     2. now we have [x * n == n << ctz(x)], if [n] is a de bruijn sequence
        (every 5bit pattern occurn exactly once when you cycle through the bit
     string), we can find [ctz(x)] from the most significant 5 bits of [x * n]
 */
  static const uint32_t de_bruijn_32 = 0x077CB531;
  static const uint8_t index32[] = {0,  1,  28, 2,  29, 14, 24, 3,  30, 22, 20,
                                    15, 25, 17, 4,  8,  31, 27, 13, 23, 21, 19,
                                    16, 7,  26, 12, 18, 6,  11, 5,  10, 9};
  return (x == 0) * 32 + index32[(de_bruijn_32 * (x & -x)) >> 27];
}

int32_t moonbit_ctz64(int64_t x) {
  static const uint64_t de_bruijn_64 = 0x0218A392CD3D5DBF;
  static const uint8_t index64[] = {
      0,  1,  2,  7,  3,  13, 8,  19, 4,  25, 14, 28, 9,  34, 20, 40,
      5,  17, 26, 38, 15, 46, 29, 48, 10, 31, 35, 54, 21, 50, 41, 57,
      63, 6,  12, 18, 24, 27, 33, 39, 16, 37, 45, 47, 30, 53, 49, 56,
      62, 11, 23, 32, 36, 44, 52, 55, 61, 22, 43, 51, 60, 42, 59, 58};
  return (x == 0) * 64 + index64[(de_bruijn_64 * (x & -x)) >> 58];
}
#endif

#ifdef HAS_BUILTIN_POPCNT

#define moonbit_popcnt32 __builtin_popcount
#define moonbit_popcnt64 __builtin_popcountll
#undef HAS_BUILTIN_POPCNT

#else
int32_t moonbit_popcnt32(uint32_t x) {
  /* The classic SIMD Within A Register algorithm.
     ref: [https://nimrod.blog/posts/algorithms-behind-popcount/]
 */
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F;
  return (x * 0x01010101) >> 24;
}

int32_t moonbit_popcnt64(uint64_t x) {
  x = x - ((x >> 1) & 0x5555555555555555);
  x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F;
  return (x * 0x0101010101010101) >> 56;
}
#endif

/* The following sqrt implementation comes from
   [musl](https://git.musl-libc.org/cgit/musl),
   with some helpers inlined to make it zero dependency.
 */
#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
const uint16_t __rsqrt_tab[128] = {
    0xb451, 0xb2f0, 0xb196, 0xb044, 0xaef9, 0xadb6, 0xac79, 0xab43, 0xaa14,
    0xa8eb, 0xa7c8, 0xa6aa, 0xa592, 0xa480, 0xa373, 0xa26b, 0xa168, 0xa06a,
    0x9f70, 0x9e7b, 0x9d8a, 0x9c9d, 0x9bb5, 0x9ad1, 0x99f0, 0x9913, 0x983a,
    0x9765, 0x9693, 0x95c4, 0x94f8, 0x9430, 0x936b, 0x92a9, 0x91ea, 0x912e,
    0x9075, 0x8fbe, 0x8f0a, 0x8e59, 0x8daa, 0x8cfe, 0x8c54, 0x8bac, 0x8b07,
    0x8a64, 0x89c4, 0x8925, 0x8889, 0x87ee, 0x8756, 0x86c0, 0x862b, 0x8599,
    0x8508, 0x8479, 0x83ec, 0x8361, 0x82d8, 0x8250, 0x81c9, 0x8145, 0x80c2,
    0x8040, 0xff02, 0xfd0e, 0xfb25, 0xf947, 0xf773, 0xf5aa, 0xf3ea, 0xf234,
    0xf087, 0xeee3, 0xed47, 0xebb3, 0xea27, 0xe8a3, 0xe727, 0xe5b2, 0xe443,
    0xe2dc, 0xe17a, 0xe020, 0xdecb, 0xdd7d, 0xdc34, 0xdaf1, 0xd9b3, 0xd87b,
    0xd748, 0xd61a, 0xd4f1, 0xd3cd, 0xd2ad, 0xd192, 0xd07b, 0xcf69, 0xce5b,
    0xcd51, 0xcc4a, 0xcb48, 0xca4a, 0xc94f, 0xc858, 0xc764, 0xc674, 0xc587,
    0xc49d, 0xc3b7, 0xc2d4, 0xc1f4, 0xc116, 0xc03c, 0xbf65, 0xbe90, 0xbdbe,
    0xbcef, 0xbc23, 0xbb59, 0xba91, 0xb9cc, 0xb90a, 0xb84a, 0xb78c, 0xb6d0,
    0xb617, 0xb560,
};

/* returns a*b*2^-32 - e, with error 0 <= e < 1.  */
static inline uint32_t mul32(uint32_t a, uint32_t b) {
  return (uint64_t)a * b >> 32;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
float sqrtf(float x) {
  uint32_t ix, m, m1, m0, even, ey;

  ix = *(uint32_t *)&x;
  if (ix - 0x00800000 >= 0x7f800000 - 0x00800000) {
    /* x < 0x1p-126 or inf or nan.  */
    if (ix * 2 == 0)
      return x;
    if (ix == 0x7f800000)
      return x;
    if (ix > 0x7f800000)
      return (x - x) / (x - x);
    /* x is subnormal, normalize it.  */
    x *= 0x1p23f;
    ix = *(uint32_t *)&x;
    ix -= 23 << 23;
  }

  /* x = 4^e m; with int e and m in [1, 4).  */
  even = ix & 0x00800000;
  m1 = (ix << 8) | 0x80000000;
  m0 = (ix << 7) & 0x7fffffff;
  m = even ? m0 : m1;

  /* 2^e is the exponent part of the return value.  */
  ey = ix >> 1;
  ey += 0x3f800000 >> 1;
  ey &= 0x7f800000;

  /* compute r ~ 1/sqrt(m), s ~ sqrt(m) with 2 goldschmidt iterations.  */
  static const uint32_t three = 0xc0000000;
  uint32_t r, s, d, u, i;
  i = (ix >> 17) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r*sqrt(m) - 1| < 0x1p-8 */
  s = mul32(m, r);
  /* |s/sqrt(m) - 1| < 0x1p-8 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r*sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  s = mul32(s, u);
  /* -0x1.03p-28 < s/sqrt(m) - 1 < 0x1.fp-31 */
  s = (s - 1) >> 6;
  /* s < sqrt(m) < s + 0x1.08p-23 */

  /* compute nearest rounded result.  */
  uint32_t d0, d1, d2;
  float y, t;
  d0 = (m << 16) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 31;
  s &= 0x007fffff;
  s |= ey;
  y = *(float *)&s;
  /* handle rounding and inexact exception. */
  uint32_t tiny = d2 == 0 ? 0 : 0x01000000;
  tiny |= (d1 ^ d2) & 0x80000000;
  t = *(float *)&tiny;
  y = y + t;
  return y;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
/* returns a*b*2^-64 - e, with error 0 <= e < 3.  */
static inline uint64_t mul64(uint64_t a, uint64_t b) {
  uint64_t ahi = a >> 32;
  uint64_t alo = a & 0xffffffff;
  uint64_t bhi = b >> 32;
  uint64_t blo = b & 0xffffffff;
  return ahi * bhi + (ahi * blo >> 32) + (alo * bhi >> 32);
}

double sqrt(double x) {
  uint64_t ix, top, m;

  /* special case handling.  */
  ix = *(uint64_t *)&x;
  top = ix >> 52;
  if (top - 0x001 >= 0x7ff - 0x001) {
    /* x < 0x1p-1022 or inf or nan.  */
    if (ix * 2 == 0)
      return x;
    if (ix == 0x7ff0000000000000)
      return x;
    if (ix > 0x7ff0000000000000)
      return (x - x) / (x - x);
    /* x is subnormal, normalize it.  */
    x *= 0x1p52;
    ix = *(uint64_t *)&x;
    top = ix >> 52;
    top -= 52;
  }

  /* argument reduction:
     x = 4^e m; with integer e, and m in [1, 4)
     m: fixed point representation [2.62]
     2^e is the exponent part of the result.  */
  int even = top & 1;
  m = (ix << 11) | 0x8000000000000000;
  if (even)
    m >>= 1;
  top = (top + 0x3ff) >> 1;

  /* approximate r ~ 1/sqrt(m) and s ~ sqrt(m) when m in [1,4)

     initial estimate:
     7bit table lookup (1bit exponent and 6bit significand).

     iterative approximation:
     using 2 goldschmidt iterations with 32bit int arithmetics
     and a final iteration with 64bit int arithmetics.

     details:

     the relative error (e = r0 sqrt(m)-1) of a linear estimate
     (r0 = a m + b) is |e| < 0.085955 ~ 0x1.6p-4 at best,
     a table lookup is faster and needs one less iteration
     6 bit lookup table (128b) gives |e| < 0x1.f9p-8
     7 bit lookup table (256b) gives |e| < 0x1.fdp-9
     for single and double prec 6bit is enough but for quad
     prec 7bit is needed (or modified iterations). to avoid
     one more iteration >=13bit table would be needed (16k).

     a newton-raphson iteration for r is
       w = r*r
       u = 3 - m*w
       r = r*u/2
     can use a goldschmidt iteration for s at the end or
       s = m*r

     first goldschmidt iteration is
       s = m*r
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     next goldschmidt iteration is
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     and at the end r is not computed only s.

     they use the same amount of operations and converge at the
     same quadratic rate, i.e. if
       r1 sqrt(m) - 1 = e, then
       r2 sqrt(m) - 1 = -3/2 e^2 - 1/2 e^3
     the advantage of goldschmidt is that the mul for s and r
     are independent (computed in parallel), however it is not
     "self synchronizing": it only uses the input m in the
     first iteration so rounding errors accumulate. at the end
     or when switching to larger precision arithmetics rounding
     errors dominate so the first iteration should be used.

     the fixed point representations are
       m: 2.30 r: 0.32, s: 2.30, d: 2.30, u: 2.30, three: 2.30
     and after switching to 64 bit
       m: 2.62 r: 0.64, s: 2.62, d: 2.62, u: 2.62, three: 2.62  */

  static const uint64_t three = 0xc0000000;
  uint64_t r, s, d, u, i;

  i = (ix >> 46) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r sqrt(m) - 1| < 0x1.fdp-9 */
  s = mul32(m >> 32, r);
  /* |s/sqrt(m) - 1| < 0x1.fdp-9 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.3704p-29 (measured worst-case) */
  r = r << 32;
  s = mul64(m, r);
  d = mul64(s, r);
  u = (three << 32) - d;
  s = mul64(s, u); /* repr: 3.61 */
  /* -0x1p-57 < s - sqrt(m) < 0x1.8001p-61 */
  s = (s - 2) >> 9; /* repr: 12.52 */
  /* -0x1.09p-52 < s - sqrt(m) < -0x1.fffcp-63 */

  /* s < sqrt(m) < s + 0x1.09p-52,
     compute nearest rounded result:
     the nearest result to 52 bits is either s or s+0x1p-52,
     we can decide by comparing (2^52 s + 0.5)^2 to 2^104 m.  */
  uint64_t d0, d1, d2;
  double y, t;
  d0 = (m << 42) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 63;
  s &= 0x000fffffffffffff;
  s |= top << 52;
  y = *(double *)&s;
  return y;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
double fabs(double x) {
  union {
    double f;
    uint64_t i;
  } u = {x};
  u.i &= 0x7fffffffffffffffULL;
  return u.f;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
float fabsf(float x) {
  union {
    float f;
    uint32_t i;
  } u = {x};
  u.i &= 0x7fffffff;
  return u.f;
}
#endif

#ifdef _MSC_VER
/* MSVC treats syntactic division by zero as fatal error,
   even for float point numbers,
   so we have to use a constant variable to work around this */
static const int MOONBIT_ZERO = 0;
#else
#define MOONBIT_ZERO 0
#endif

#ifdef __cplusplus
}
#endif
struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__;

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0TPB9ArrayViewGUsfEE;

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0TWEOUssE;

struct _M0TUsiE;

struct _M0TPB3MapGsfE;

struct _M0TUsbE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsiEE;

struct _M0TPB17FloatingDecimal64;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List;

struct _M0TPB5EntryGssE;

struct _M0TUssE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String;

struct _M0BTPB6Logger;

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0TPB6Logger;

struct _M0TP38JIA2JIA29moonbitdb3lib8Database;

struct _M0TUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsfEE;

struct _M0TPB5EntryGsbE;

struct _M0TPB8MutLocalGORPB5EntryGssEE;

struct _M0DTPC16option6OptionGfE4Some;

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__;

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque;

struct _M0TPB19MulShiftAll64Result;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set;

struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0TPB5ArrayGOsE;

struct _M0TWEOUsbE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet;

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__;

struct _M0TPB5EntryGsfE;

struct _M0TPB8MutLocalGiE;

struct _M0TPB5ArrayGUsfEE;

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0TPB3MapGssE;

struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__;

struct _M0TPB4Show;

struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash;

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0BTPB4Show;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB8MutLocalGORPB5EntryGsbEE;

struct _M0TPB3MapGsbE;

struct _M0TPB5ArrayGsE;

struct _M0TPB3MapGsiE;

struct _M0TPB9ArrayViewGUssEE;

struct _M0TPB9ArrayViewGUsbEE;

struct _M0TPB9ArrayViewGsE;

struct _M0TPB4IterGUsbEE;

struct _M0TPB4IterGUssEE;

struct _M0TPB5EntryGsiE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
};

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE {
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* $0;
  int64_t $1;
  
};

struct _M0TPB9ArrayViewGUsfEE {
  struct _M0TUsfE** $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE {
  moonbit_string_t $0;
  void* $1;
  
};

struct _M0TWEOUssE {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  
};

struct _M0TUsiE {
  moonbit_string_t $0;
  int32_t $1;
  
};

struct _M0TPB3MapGsfE {
  struct _M0TPB5EntryGsfE** $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGsfE* $5;
  int32_t $6;
  
};

struct _M0TUsbE {
  moonbit_string_t $0;
  int32_t $1;
  
};

struct _M0TPB13StringBuilder {
  uint16_t* $0;
  int32_t $1;
  
};

struct _M0TPB9ArrayViewGUsiEE {
  struct _M0TUsiE** $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TPB17FloatingDecimal64 {
  uint64_t $0;
  int32_t $1;
  
};

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List {
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* $0;
  
};

struct _M0TPB5EntryGssE {
  int32_t $0;
  struct _M0TPB5EntryGssE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  moonbit_string_t $5;
  
};

struct _M0TUssE {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String {
  moonbit_string_t $0;
  
};

struct _M0BTPB6Logger {
  int32_t(* $method_0)(void*, moonbit_string_t);
  int32_t(* $method_1)(void*, moonbit_string_t, int32_t, int32_t);
  int32_t(* $method_2)(void*, struct _M0TPC16string10StringView);
  int32_t(* $method_3)(void*, int32_t);
  int32_t(* $method_4)(void*, struct _M0TPB4Show);
  int32_t(* $method_5)(void*, struct _M0TPB4Show);
  
};

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* $5;
  int32_t $6;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TP38JIA2JIA29moonbitdb3lib8Database {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* $0;
  struct _M0TPB3MapGsiE* $1;
  int32_t $2;
  
};

struct _M0TUsfE {
  moonbit_string_t $0;
  float $1;
  
};

struct _M0TPB8MutLocalGORPB5EntryGsfEE {
  struct _M0TPB5EntryGsfE* $0;
  
};

struct _M0TPB5EntryGsbE {
  int32_t $0;
  struct _M0TPB5EntryGsbE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  int32_t $5;
  
};

struct _M0TPB8MutLocalGORPB5EntryGssEE {
  struct _M0TPB5EntryGssE* $0;
  
};

struct _M0DTPC16option6OptionGfE4Some {
  float $0;
  
};

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__ {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* $1;
  
};

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque {
  struct _M0TPB5ArrayGsE* $0;
  struct _M0TPB5ArrayGsE* $1;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set {
  struct _M0TPB3MapGsbE* $0;
  
};

struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TPB5ArrayGOsE {
  moonbit_string_t* $0;
  int32_t $1;
  
};

struct _M0TWEOUsbE {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  
};

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet {
  struct _M0TPB3MapGsfE* $0;
  
};

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
};

struct _M0TPB5EntryGsfE {
  int32_t $0;
  struct _M0TPB5EntryGsfE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  float $5;
  
};

struct _M0TPB8MutLocalGiE {
  int32_t $0;
  
};

struct _M0TPB5ArrayGUsfEE {
  struct _M0TUsfE** $0;
  int32_t $1;
  
};

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE {
  int32_t $0;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  void* $5;
  
};

struct _M0TPB3MapGssE {
  struct _M0TPB5EntryGssE** $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGssE* $5;
  int32_t $6;
  
};

struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__ {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*(* code)(
    struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* $1;
  
};

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*(* code)(
    struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
  );
  
};

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash {
  struct _M0TPB3MapGssE* $0;
  
};

struct _M0TPB4IterGUsfEE {
  struct _M0TWEOUsfE* $0;
  int64_t $1;
  
};

struct _M0TWEOUsfE {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  
};

struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* $0;
  
};

struct _M0BTPB4Show {
  int32_t(* $method_0)(void*, struct _M0TPB6Logger);
  moonbit_string_t(* $method_1)(void*);
  
};

struct _M0TPC16string10StringView {
  moonbit_string_t $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TPB8MutLocalGORPB5EntryGsbEE {
  struct _M0TPB5EntryGsbE* $0;
  
};

struct _M0TPB3MapGsbE {
  struct _M0TPB5EntryGsbE** $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGsbE* $5;
  int32_t $6;
  
};

struct _M0TPB5ArrayGsE {
  moonbit_string_t* $0;
  int32_t $1;
  
};

struct _M0TPB3MapGsiE {
  struct _M0TPB5EntryGsiE** $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGsiE* $5;
  int32_t $6;
  
};

struct _M0TPB9ArrayViewGUssEE {
  struct _M0TUssE** $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TPB9ArrayViewGUsbEE {
  struct _M0TUsbE** $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TPB9ArrayViewGsE {
  moonbit_string_t* $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TPB4IterGUsbEE {
  struct _M0TWEOUsbE* $0;
  int64_t $1;
  
};

struct _M0TPB4IterGUssEE {
  struct _M0TWEOUssE* $0;
  int64_t $1;
  
};

struct _M0TPB5EntryGsiE {
  int32_t $0;
  struct _M0TPB5EntryGsiE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  int32_t $5;
  
};

struct _M0TPB7Umul128 {
  uint64_t $0;
  uint64_t $1;
  
};

struct _M0TPB8Pow5Pair {
  uint64_t $0;
  uint64_t $1;
  
};

moonbit_string_t _M0FP28JIA2JIA29moonbitdb16show__opt__float(void*);

moonbit_string_t _M0FP28JIA2JIA29moonbitdb14show__opt__int(int64_t);

moonbit_string_t _M0FP28JIA2JIA29moonbitdb9show__opt(moonbit_string_t);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database7command();

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4ping();

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database9randomkey(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database5zrank(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database9zrevrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPB5ArrayGUsfEE* _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

void* _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5zcard(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib5merge(
  struct _M0TPB5ArrayGUsfEE*,
  struct _M0TPB5ArrayGUsfEE*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  float,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database9sismember(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5scard(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database8smembers(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6lrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4rpop(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4lpop(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hlen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB3MapGssE* _M0MP38JIA2JIA29moonbitdb3lib8Database7hgetall(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t
);

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database4decr(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database4incr(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0FP38JIA2JIA29moonbitdb3lib15int__to__string(int32_t);

int64_t _M0FP38JIA2JIA29moonbitdb3lib10parse__int(moonbit_string_t);

int64_t _M0FP38JIA2JIA29moonbitdb3lib17uint16__to__digit(int32_t);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6strlen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database4keys(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6exists(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database3get(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGOsE* _M0MP38JIA2JIA29moonbitdb3lib8Database4mget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4mset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB5ArrayGsE*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3set(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  int32_t
);

struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0MP38JIA2JIA29moonbitdb3lib8Database3new(
  
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib5Deque9to__array(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib5Deque9pop__back(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib5Deque10pop__front(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque10push__back(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*,
  moonbit_string_t
);

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0MP38JIA2JIA29moonbitdb3lib5Deque3new(
  
);

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float);

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC15array5Array3popGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE*,
  int32_t
);

struct _M0TUsfE* _M0MPC15array5Array2atGUsfEE(
  struct _M0TPB5ArrayGUsfEE*,
  int32_t
);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

moonbit_string_t _M0MPC16double6Double10to__string(double);

moonbit_string_t _M0FPB15ryu__to__string(double);

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(uint64_t, int32_t);

moonbit_string_t _M0FPB9to__chars(struct _M0TPB17FloatingDecimal64*, int32_t);

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(uint64_t, uint32_t);

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t);

int64_t _M0MPC14bool4Bool9to__int64(int32_t);

int32_t _M0MPC14bool4Bool7to__int(int32_t);

int32_t _M0FPB17decimal__length17(uint64_t);

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t);

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t);

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t,
  struct _M0TPB8Pow5Pair,
  int32_t,
  int32_t
);

int32_t _M0FPB18multipleOfPowerOf2(uint64_t, int32_t);

int32_t _M0FPB18multipleOfPowerOf5(uint64_t, int32_t);

int32_t _M0FPB10pow5Factor(uint64_t);

uint64_t _M0FPB13shiftright128(uint64_t, uint64_t, int32_t);

struct _M0TPB7Umul128 _M0FPB7umul128(uint64_t, uint64_t);

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t,
  int32_t,
  int32_t
);

int32_t _M0FPB9log10Pow2(int32_t);

int32_t _M0FPB9log10Pow5(int32_t);

moonbit_string_t _M0FPB18copy__special__str(int32_t, int32_t, int32_t);

int32_t _M0FPB8pow5bits(int32_t);

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t);

struct _M0TUssE* _M0MPB5Iter24nextGssE(struct _M0TPB4IterGUssEE*);

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(struct _M0TPB4IterGUsbEE*);

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*
);

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(struct _M0TPB4IterGUsfEE*);

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(struct _M0TPB3MapGsfE*);

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2833l711(struct _M0TWEOUsfE*);

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2823l711(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2813l711(struct _M0TWEOUsbE*);

struct _M0TUssE* _M0MPB3Map4iterGssEC2803l711(struct _M0TWEOUssE*);

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map11shift__backGsiE(struct _M0TPB3MapGsiE*, int32_t);

int32_t _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map8containsGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map8containsGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

void* _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

moonbit_string_t _M0MPB3Map3getGssE(struct _M0TPB3MapGssE*, moonbit_string_t);

void* _M0MPB3Map3getGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

int64_t _M0MPB3Map3getGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE,
  int64_t
);

struct _M0TPB3MapGsiE* _M0MPB3Map3MapGsiE(
  struct _M0TPB9ArrayViewGUsiEE,
  int64_t
);

struct _M0TPB3MapGssE* _M0MPB3Map3MapGssE(
  struct _M0TPB9ArrayViewGUssEE,
  int64_t
);

struct _M0TPB3MapGsbE* _M0MPB3Map3MapGsbE(
  struct _M0TPB9ArrayViewGUsbEE,
  int64_t
);

struct _M0TPB3MapGsfE* _M0MPB3Map3MapGsfE(
  struct _M0TPB9ArrayViewGUsfEE,
  int64_t
);

int32_t _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t,
  void*
);

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB3Map3setGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t, int32_t);

int32_t _M0MPB3Map3setGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t, float);

int32_t _M0MPB3Map3setGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t, int32_t);

int32_t _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t,
  void*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsbE(
  struct _M0TPB3MapGsbE*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsfE(
  struct _M0TPB3MapGsfE*,
  moonbit_string_t,
  float,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE*);

int32_t _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*
);

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE*,
  struct _M0TPB5EntryGsfE*
);

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE*,
  int32_t,
  struct _M0TPB5EntryGssE*
);

int32_t _M0MPB3Map10push__awayGsbE(
  struct _M0TPB3MapGsbE*,
  int32_t,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map10push__awayGsfE(
  struct _M0TPB3MapGsfE*,
  int32_t,
  struct _M0TPB5EntryGsfE*
);

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE*,
  int32_t,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsfE(
  struct _M0TPB3MapGsfE*,
  struct _M0TPB5EntryGsfE*,
  int32_t
);

int32_t _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE*,
  int32_t,
  struct _M0TPB5EntryGssE*
);

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE*,
  int32_t,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE*,
  int32_t,
  struct _M0TPB5EntryGsfE*
);

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE*,
  int32_t,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPC13int3Int3max(int32_t, int32_t);

int32_t _M0FPB21capacity__for__length(int32_t);

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  int32_t
);

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t);

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t);

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t);

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

int32_t _M0MPC16option6Option6unwrapGiE(int64_t);

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE*
);

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE*
);

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE*
);

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE*
);

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE,
  struct _M0TPC16string10StringView
);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

moonbit_string_t _M0IPC16uint646UInt64PB4Show10to__string(uint64_t);

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t);

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE*,
  struct _M0TUsfE*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGOsE(struct _M0TPB5ArrayGOsE*);

int32_t _M0MPC15array5Array7reallocGUsfEE(struct _M0TPB5ArrayGUsfEE*);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGOsE(
  struct _M0TPB5ArrayGOsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array6lengthGOsE(struct _M0TPB5ArrayGOsE*);

int32_t _M0MPC15array5Array6lengthGUsfEE(struct _M0TPB5ArrayGUsfEE*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

int32_t _M0IPC14byte4BytePB7Default7default();

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(struct _M0TPB5ArrayGOsE*);

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(struct _M0TWEOUssE*, int64_t);

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(struct _M0TWEOUsbE*, int64_t);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int64_t
);

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(struct _M0TWEOUsfE*, int64_t);

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(uint64_t, int32_t);

int32_t _M0FPB22int64__to__string__dec(uint16_t*, uint64_t, int32_t, int32_t);

int32_t _M0FPB26int64__to__string__generic(
  uint16_t*,
  uint64_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0FPB22int64__to__string__hex(uint16_t*, uint64_t, int32_t, int32_t);

int32_t _M0FPB14radix__count64(uint64_t, int32_t);

int32_t _M0FPB12hex__count64(uint64_t);

int32_t _M0FPB12dec__count64(uint64_t);

moonbit_string_t _M0MPC13int3Int18to__string_2einner(int32_t, int32_t);

int32_t _M0FPB14radix__count32(uint32_t, int32_t);

int32_t _M0FPB12hex__count32(uint32_t);

int32_t _M0FPB12dec__count32(uint32_t);

int32_t _M0FPB20int__to__string__dec(uint16_t*, uint32_t, int32_t, int32_t);

int32_t _M0FPB24int__to__string__generic(
  uint16_t*,
  uint32_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0FPB20int__to__string__hex(uint16_t*, uint32_t, int32_t, int32_t);

struct _M0TUssE* _M0MPB4Iter4nextGUssEE(struct _M0TPB4IterGUssEE*);

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(struct _M0TPB4IterGUsbEE*);

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*
);

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(struct _M0TPB4IterGUsfEE*);

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t,
  struct _M0TPB6Logger
);

int32_t _M0IP016_24default__implPB4Show6outputGbE(
  int32_t,
  struct _M0TPB6Logger
);

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float,
  struct _M0TPB6Logger
);

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t,
  struct _M0TPB6Logger
);

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder*,
  struct _M0TPB4Show
);

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder*,
  struct _M0TPB4Show
);

int32_t _M0FPB13finalize__acc(uint32_t);

uint32_t _M0FPB14avalanche__acc(uint32_t);

uint64_t _M0MPC13int3Int10to__uint64(int32_t);

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t
);

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t*,
  int32_t,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t);

uint32_t _M0MPC14char4Char8to__uint(int32_t);

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder*
);

int32_t _M0IPC16uint166UInt16PB7Default7default();

uint16_t* _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(
  uint16_t*,
  int32_t,
  int32_t,
  int32_t,
  int32_t,
  int32_t
);

uint16_t* _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(
  uint16_t*,
  int32_t,
  int32_t,
  int32_t,
  int32_t,
  int32_t
);

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder21StringBuilder_2einner(
  int32_t
);

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t*,
  int32_t,
  int32_t,
  int32_t,
  int32_t
);

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t*,
  int32_t,
  int32_t,
  int32_t,
  int32_t
);

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE**,
  int32_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t
);

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder*,
  float
);

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder*,
  uint64_t
);

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGsE(
  moonbit_string_t*,
  int32_t,
  int32_t,
  int32_t,
  int32_t
);

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGOsE(
  moonbit_string_t*,
  int32_t,
  int32_t,
  int32_t,
  int32_t
);

struct _M0TUsfE** _M0MPB18UninitializedArray23unsafe__make__and__blitGUsfEE(
  struct _M0TUsfE**,
  int32_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGOsE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsfEE(
  struct _M0TUsfE**,
  int32_t,
  struct _M0TUsfE**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t*,
  int32_t,
  uint16_t*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGOsEE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsfEEE(
  struct _M0TUsfE**,
  int32_t,
  struct _M0TUsfE**,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray6lengthGsE(moonbit_string_t*);

int32_t _M0MPB18UninitializedArray6lengthGOsE(moonbit_string_t*);

int32_t _M0MPB18UninitializedArray6lengthGUsfEE(struct _M0TUsfE**);

uint32_t _M0FPB13consume4__acc(uint32_t, uint32_t);

uint32_t _M0FPB4rotl(uint32_t, int32_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

uint16_t* _M0FPC15abort5abortGAkE(moonbit_string_t);

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(
  moonbit_string_t
);

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(
  moonbit_string_t
);

struct _M0TUsfE** _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGiE(moonbit_string_t);

int32_t _M0IP016_24default__implPB6Logger61write_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void*,
  struct _M0TPB4Show
);

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void*,
  struct _M0TPB4Show
);

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  int32_t
);

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  moonbit_string_t
);

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_120 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 49, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_95 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 0, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_91 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 55, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_196 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 10, 55357,
    56524, 32, 56, 46, 32, 26381, 21153, 22120, 20449, 24687, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_64 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_10 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_11 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 68, 69, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_134 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 32, 32, 
    72, 76, 69, 78, 32, 117, 115, 101, 114, 58, 49, 48, 48, 49, 32, 61, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_61 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 90, 82, 
    65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_144 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 32, 32, 32, 
    82, 80, 85, 83, 72, 32, 116, 97, 115, 107, 115, 32, 91, 23436, 25104,
    25253, 21578, 44, 32, 20195, 30721, 23457, 26597, 44, 32, 22242, 
    38431, 20250, 35758, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_72 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 68, 66, 
    83, 73, 90, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_98 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 110, 
    102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_97 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 78, 97, 78, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_44 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 83, 77, 
    69, 77, 66, 69, 82, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_22 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_83 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 79, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_167 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    90, 67, 65, 82, 68, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_159 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    83, 77, 69, 77, 66, 69, 82, 83, 32, 61, 32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_85 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_84 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_202 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    25903, 25345, 21629, 20196, 25968, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_111 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 32, 32, 
    77, 111, 111, 110, 66, 105, 116, 68, 66, 32, 45, 32, 24555, 36895, 
    24320, 22987, 28436, 31034, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_53 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 68, 
    73, 70, 70, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_165 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 29609, 23478,
    68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_155 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 32, 32, 
    83, 65, 68, 68, 32, 116, 97, 103, 115, 58, 112, 111, 115, 116, 58, 
    49, 32, 123, 25216, 26415, 44, 32, 32534, 31243, 44, 32, 77, 111, 
    111, 110, 66, 105, 116, 44, 32, 25216, 26415, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_40 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_36 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 73, 
    78, 68, 69, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_56 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_117 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 32, 32, 
    71, 69, 84, 32, 103, 114, 101, 101, 116, 105, 110, 103, 32, 61, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_168 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 32, 32, 
    90, 83, 67, 79, 82, 69, 32, 29609, 23478, 66, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_149 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 32, 32, 
    21097, 20313, 32, 76, 76, 69, 78, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_63 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_135 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    101, 109, 97, 105, 108, 23383, 27573, 23384, 22312, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_88 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_173 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_157 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 32, 40, 
    33258, 21160, 21435, 37325, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_109 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_170 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 32, 40, 
    20174, 23567, 21040, 22823, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_15 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 84, 
    84, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_198 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    80, 73, 78, 71, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_195 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 61, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_106 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_3 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 71, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_70 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 73, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_140 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 116, 97, 
    115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_96 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 73, 108, 
    108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 116, 69, 
    120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_174 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 45, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_54 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 77, 
    79, 86, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_180 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 31186, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_129 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 97, 108, 
    105, 99, 101, 64, 101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 
    109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_47 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_177 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 115, 101, 
    115, 115, 105, 111, 110, 58, 116, 111, 107, 101, 110, 95, 97, 98, 
    99, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_146 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 23, 32, 32, 
    76, 82, 65, 78, 71, 69, 32, 116, 97, 115, 107, 115, 32, 48, 32, 45, 
    49, 32, 61, 32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_142 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 20195, 30721,
    23457, 26597, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_121 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    83, 69, 84, 32, 99, 111, 117, 110, 116, 101, 114, 32, 49, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_94 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_127 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 97, 108, 
    105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_114 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 103, 114, 
    101, 101, 116, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_154 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 77, 111, 
    111, 110, 66, 105, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_89 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_68 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_27 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 86, 
    65, 76, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_116 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 34, 32, 32, 
    83, 69, 84, 32, 103, 114, 101, 101, 116, 105, 110, 103, 32, 39, 72, 
    101, 108, 108, 111, 44, 32, 77, 111, 111, 110, 66, 105, 116, 68, 
    66, 33, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_21 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_9 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 84, 
    82, 76, 69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_133 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 28, 32, 32, 
    72, 71, 69, 84, 32, 117, 115, 101, 114, 58, 49, 48, 48, 49, 32, 117, 
    115, 101, 114, 110, 97, 109, 101, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_131 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 52, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_37 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_7 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 89, 
    80, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_166 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 59, 32, 32, 
    90, 65, 68, 68, 32, 108, 101, 97, 100, 101, 114, 98, 111, 97, 114, 
    100, 32, 123, 29609, 23478, 65, 58, 49, 48, 48, 48, 44, 32, 29609, 
    23478, 66, 58, 50, 53, 48, 48, 44, 32, 29609, 23478, 67, 58, 49, 
    56, 48, 48, 44, 32, 29609, 23478, 68, 58, 51, 50, 48, 48, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_75 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 65, 
    78, 68, 79, 77, 75, 69, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_172 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 32, 32, 
    32, 32, 35, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_13 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    88, 80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_8 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 65, 80, 
    80, 69, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_130 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 108, 101, 
    118, 101, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_46 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_45 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_32 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_110 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_66 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 90, 82, 
    69, 86, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_176 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 10, 55357,
    56524, 32, 54, 46, 32, 75, 101, 121, 32, 36807, 26399, 26426, 21046, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_204 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    9989, 32, 28436, 31034, 23436, 25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_76 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 69, 
    78, 65, 77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_59 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 83, 
    67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_184 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 10, 55357,
    56524, 32, 55, 46, 32, 25209, 37327, 25805, 20316, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_55 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_186 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 99, 111, 
    110, 102, 105, 103, 58, 108, 97, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_185 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 99, 111, 
    110, 102, 105, 103, 58, 116, 104, 101, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_25 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 69, 
    88, 73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 40, 110, 
    105, 108, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_87 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_80 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    70, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_6 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 75, 69, 
    89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_5 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_115 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 72, 101, 
    108, 108, 111, 44, 32, 77, 111, 111, 110, 66, 105, 116, 68, 66, 33, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_65 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 82, 
    65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_82 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 67, 79, 
    77, 77, 65, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_183 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 40, 
    45, 50, 32, 34920, 31034, 19981, 23384, 22312, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_194 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_193 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    77, 71, 69, 84, 32, 32467, 26524, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_148 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 32, 32, 
    82, 80, 79, 80, 32, 116, 97, 115, 107, 115, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_122 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 32, 32, 
    73, 78, 67, 82, 32, 99, 111, 117, 110, 116, 101, 114, 32, 61, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_58 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_137 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 44, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_14 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 84, 84, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_189 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 122, 104, 
    45, 67, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_190 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 65, 115, 
    105, 97, 47, 83, 104, 97, 110, 103, 104, 97, 105, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_178 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 32, 32, 
    83, 69, 84, 32, 115, 101, 115, 115, 105, 111, 110, 58, 116, 111, 
    107, 101, 110, 95, 97, 98, 99, 32, 43, 32, 69, 88, 80, 73, 82, 69, 
    32, 54, 48, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_161 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 108, 101, 
    97, 100, 101, 114, 98, 111, 97, 114, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_192 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 99, 111, 
    110, 102, 105, 103, 58, 110, 111, 110, 101, 120, 105, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_181 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 32, 32, 
    51, 48, 31186, 21518, 32, 84, 84, 76, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_162 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 29609, 23478,
    65, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_102 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 114, 97, 
    100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 98, 101, 
    116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_86 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_197 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    68, 66, 83, 73, 90, 69, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_199 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    82, 65, 78, 68, 79, 77, 75, 69, 89, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_30 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 76, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_24 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_108 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 44, 32, 
    108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_101 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 102, 97, 
    108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_145 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 32, 32, 
    76, 76, 69, 78, 32, 116, 97, 115, 107, 115, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_105 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 98, 111, 
    117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 97, 105, 
    108, 101, 100, 58, 32, 97, 108, 108, 111, 99, 97, 116, 101, 95, 108, 
    101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_126 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 117, 115, 
    101, 114, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_203 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 41, 10, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_60 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_43 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_187 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 99, 111, 
    110, 102, 105, 103, 58, 116, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_163 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 29609, 23478,
    66, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_71 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 65, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_29 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    71, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_158 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 32, 32, 
    83, 73, 83, 77, 69, 77, 66, 69, 82, 32, 77, 111, 111, 110, 66, 105, 
    116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_57 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_90 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_81 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 73, 
    77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_179 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    84, 84, 76, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_103 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 
    118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_52 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 68, 
    73, 70, 70, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_42 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 80, 
    79, 80, 76, 80, 85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_175 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 20998, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_152 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 25216, 26415, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_139 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 55357,
    56524, 32, 51, 46, 32, 76, 105, 115, 116, 32, 20219, 21153, 38431, 
    21015, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_164 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 29609, 23478,
    67, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_132 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 41, 32, 32, 
    72, 83, 69, 84, 32, 117, 115, 101, 114, 58, 49, 48, 48, 49, 32, 123, 
    117, 115, 101, 114, 110, 97, 109, 101, 44, 32, 101, 109, 97, 105, 
    108, 44, 32, 108, 101, 118, 101, 108, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 73, 
    78, 67, 82, 66, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_128 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 101, 109, 
    97, 105, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_31 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 82, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_191 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 32, 32, 
    77, 83, 69, 84, 32, 51, 20010, 37197, 32622, 39033, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_150 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 10, 55357,
    56524, 32, 52, 46, 32, 83, 101, 116, 32, 26631, 31614, 31995, 32479, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_69 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_188 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 100, 97, 
    114, 107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_74 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 70, 76, 
    85, 83, 72, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_49 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 73, 
    78, 84, 69, 82, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_48 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 73, 
    78, 84, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_124 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55357,
    56524, 32, 50, 46, 32, 72, 97, 115, 104, 32, 29992, 25143, 20449, 
    24687, 23384, 20648, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_118 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 32, 32, 
    83, 84, 82, 76, 69, 78, 32, 103, 114, 101, 101, 116, 105, 110, 103, 
    32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_51 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 85, 
    78, 73, 79, 78, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_123 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 32, 32, 
    68, 69, 67, 82, 32, 99, 111, 117, 110, 116, 101, 114, 32, 61, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_28 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    83, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_107 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    100, 115, 116, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_104 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 105, 110, 
    118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 111, 105, 
    110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_50 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 85, 
    78, 73, 79, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_38 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_23 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 71, 
    69, 84, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_19 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_201 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 20010, 
    41, 58, 32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 73, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_2 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 83, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_93 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 57, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_136 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    23383, 27573, 21015, 34920, 32, 61, 32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_26 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 75, 
    69, 89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_33 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 82, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_156 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    83, 67, 65, 82, 68, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_113 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_39 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 76, 73, 
    78, 83, 69, 82, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_12 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_171 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 24, 32, 32, 
    84, 79, 80, 32, 51, 32, 40, 90, 82, 69, 86, 82, 65, 78, 71, 69, 32, 
    48, 32, 50, 41, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_147 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 32, 32, 
    76, 80, 79, 80, 32, 116, 97, 115, 107, 115, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_138 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_125 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 117, 115, 
    101, 114, 58, 49, 48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_200 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    75, 69, 89, 83, 32, 42, 32, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_151 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 116, 97, 
    103, 115, 58, 112, 111, 115, 116, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_100 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 114, 
    117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_182 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    54, 53, 31186, 21518, 32, 69, 88, 73, 83, 84, 83, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_141 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 23436, 25104,
    25253, 21578, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_119 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 99, 111, 
    117, 110, 116, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_17 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_160 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 10, 55357,
    56524, 32, 53, 46, 32, 83, 111, 114, 116, 101, 100, 32, 83, 101, 
    116, 32, 25490, 34892, 27036, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_1 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_92 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_41 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_153 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32534, 31243, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_62 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 67, 
    79, 85, 78, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_169 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    90, 82, 65, 78, 75, 32, 29609, 23478, 66, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_143 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 22242, 38431,
    20250, 35758, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_35 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_77 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 82, 69, 
    78, 65, 77, 69, 78, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_4 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 68, 69, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_79 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 69, 67, 
    72, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_99 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 48, 46, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_73 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 70, 76, 
    85, 83, 72, 68, 66, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_112 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55357,
    56524, 32, 49, 46, 32, 83, 116, 114, 105, 110, 103, 32, 22522, 30784,
    25805, 20316, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_16 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    82, 83, 73, 83, 84, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0)
  };

uint32_t const moonbit_layout_table_data[131] =
  {
    sizeof(struct _M0TPB5ArrayGsE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGsE, $0) / 4,
    sizeof(struct _M0TPB5ArrayGUsfEE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGUsfEE, $0) / 4, sizeof(struct _M0TUsfE) / 4,
    1, offsetof(struct _M0TUsfE, $0) / 4,
    sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet) / 4, 
    1,
    offsetof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet, $0) / 4,
    sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set) / 4, 
    1,
    offsetof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set, $0) / 4,
    sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List) / 4, 
    1,
    offsetof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List, $0) / 4,
    sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash) / 4, 
    1,
    offsetof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash, $0) / 4,
    sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String) / 4, 
    1,
    offsetof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String, $0)
    / 4, sizeof(struct _M0TPB5ArrayGOsE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGOsE, $0) / 4,
    sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database) / 4, 2,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database, $0) / 4,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database, $1) / 4,
    sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque) / 4, 2,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque, $0) / 4,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque, $1) / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGssEE, $0) / 4,
    sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__)
    / 4, 2,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__, $0)
    / 4,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__)
    / 4, 2,
    offsetof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__, $0)
    / 4,
    offsetof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__, $1)
    / 4, sizeof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE) / 4, 
    2,
    offsetof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $0) / 4,
    offsetof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $1) / 4,
    sizeof(struct _M0TUsbE) / 4, 1, offsetof(struct _M0TUsbE, $0) / 4,
    sizeof(struct _M0TUssE) / 4, 2, offsetof(struct _M0TUssE, $0) / 4,
    offsetof(struct _M0TUssE, $1) / 4,
    sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE) / 4,
    3,
    offsetof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $1)
    / 4,
    offsetof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $4)
    / 4,
    offsetof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $5)
    / 4, sizeof(struct _M0TPB5EntryGssE) / 4, 3,
    offsetof(struct _M0TPB5EntryGssE, $1) / 4,
    offsetof(struct _M0TPB5EntryGssE, $4) / 4,
    offsetof(struct _M0TPB5EntryGssE, $5) / 4,
    sizeof(struct _M0TPB5EntryGsbE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsbE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsbE, $4) / 4,
    sizeof(struct _M0TPB5EntryGsfE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsfE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsfE, $4) / 4,
    sizeof(struct _M0TPB5EntryGsiE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsiE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsiE, $4) / 4,
    sizeof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE) / 4,
    2,
    offsetof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $0)
    / 4,
    offsetof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $5)
    / 4, sizeof(struct _M0TPB3MapGsiE) / 4, 2,
    offsetof(struct _M0TPB3MapGsiE, $0) / 4,
    offsetof(struct _M0TPB3MapGsiE, $5) / 4,
    sizeof(struct _M0TPB3MapGssE) / 4, 2,
    offsetof(struct _M0TPB3MapGssE, $0) / 4,
    offsetof(struct _M0TPB3MapGssE, $5) / 4,
    sizeof(struct _M0TPB3MapGsbE) / 4, 2,
    offsetof(struct _M0TPB3MapGsbE, $0) / 4,
    offsetof(struct _M0TPB3MapGsbE, $5) / 4,
    sizeof(struct _M0TPB3MapGsfE) / 4, 2,
    offsetof(struct _M0TPB3MapGsfE, $0) / 4,
    offsetof(struct _M0TPB3MapGsfE, $5) / 4,
    sizeof(struct _M0TPB4IterGUssEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUssEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsbEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsbEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE) / 4,
    1,
    offsetof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE, $0)
    / 4, sizeof(struct _M0TPB4IterGUsfEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsfEE, $0) / 4,
    sizeof(struct _M0TPB13StringBuilder) / 4, 1,
    offsetof(struct _M0TPB13StringBuilder, $0) / 4
  };

struct { int32_t rc; uint32_t meta; struct _M0BTPB6Logger data; 
} _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id$object =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0),
    {.$method_0 = _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_1 = _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE,
       .$method_2 = _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_3 = _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_4 = _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE,
       .$method_5 = _M0IP016_24default__implPB6Logger61write_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE}
  };

struct _M0BTPB6Logger* _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id =
  &_M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; uint64_t data[30]; 
} _M0FPB26gDOUBLE__POW5__INV__SPLIT2$object =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 1ull,
    2305843009213693952ull, 5955668970331000884ull, 1784059615882449851ull,
    8982663654677661702ull, 1380349269358112757ull, 7286864317269821294ull,
    2135987035920910082ull, 7005857020398200553ull, 1652639921975621497ull,
    17965325103354776697ull, 1278668206209430417ull, 8928596168509315048ull,
    1978643211784836272ull, 10075671573058298858ull, 1530901034580419511ull,
    597001226353042382ull, 1184477304306571148ull, 1527430471115325346ull,
    1832889850782397517ull, 12533209867169019542ull, 1418129833677084982ull,
    5577825024675947042ull, 2194449627517475473ull, 11006974540203867551ull,
    1697873161311732311ull, 10313493231639821582ull, 1313665730009899186ull,
    12701016819766672773ull, 2032799256770390445ull
  };

uint64_t* _M0FPB26gDOUBLE__POW5__INV__SPLIT2 =
  _M0FPB26gDOUBLE__POW5__INV__SPLIT2$object.data;

struct { int32_t rc; uint32_t meta; uint32_t data[19]; 
} _M0FPB19gPOW5__INV__OFFSETS$object =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 1414808916u,
    67458373u, 268701696u, 4195348u, 1073807360u, 1091917141u, 1108u, 
    65604u, 1073741824u, 1140850753u, 1346716752u, 1431634004u, 1365595476u,
    1073758208u, 16777217u, 66816u, 1364284433u, 89478484u, 0u
  };

uint32_t* _M0FPB19gPOW5__INV__OFFSETS =
  _M0FPB19gPOW5__INV__OFFSETS$object.data;

struct { int32_t rc; uint32_t meta; uint64_t data[26]; 
} _M0FPB21gDOUBLE__POW5__SPLIT2$object =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 26, 0ull,
    1152921504606846976ull, 0ull, 1490116119384765625ull,
    1032610780636961552ull, 1925929944387235853ull, 7910200175544436838ull,
    1244603055572228341ull, 16941905809032713930ull, 1608611746708759036ull,
    13024893955298202172ull, 2079081953128979843ull, 6607496772837067824ull,
    1343575221513417750ull, 17332926989895652603ull, 1736530273035216783ull,
    13037379183483547984ull, 2244412773384604712ull, 1605989338741628675ull,
    1450417759929778918ull, 9630225068416591280ull, 1874621017369538693ull,
    665883850346957067ull, 1211445438634777304ull, 14931890668723713708ull,
    1565756531257009982ull
  };

uint64_t* _M0FPB21gDOUBLE__POW5__SPLIT2 =
  _M0FPB21gDOUBLE__POW5__SPLIT2$object.data;

struct { int32_t rc; uint32_t meta; uint32_t data[21]; 
} _M0FPB14gPOW5__OFFSETS$object =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 0u, 0u, 
    0u, 0u, 1073741824u, 1500076437u, 1431590229u, 1448432917u, 1091896580u,
    1079333904u, 1146442053u, 1146111296u, 1163220304u, 1073758208u,
    2521039936u, 1431721317u, 1413824581u, 1075134801u, 1431671125u,
    1363170645u, 261u
  };

uint32_t* _M0FPB14gPOW5__OFFSETS = _M0FPB14gPOW5__OFFSETS$object.data;

struct { int32_t rc; uint32_t meta; uint64_t data[26]; 
} _M0FPB20gDOUBLE__POW5__TABLE$object =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 26, 1ull, 5ull,
    25ull, 125ull, 625ull, 3125ull, 15625ull, 78125ull, 390625ull,
    1953125ull, 9765625ull, 48828125ull, 244140625ull, 1220703125ull,
    6103515625ull, 30517578125ull, 152587890625ull, 762939453125ull,
    3814697265625ull, 19073486328125ull, 95367431640625ull,
    476837158203125ull, 2384185791015625ull, 11920928955078125ull,
    59604644775390625ull, 298023223876953125ull
  };

uint64_t* _M0FPB20gDOUBLE__POW5__TABLE =
  _M0FPB20gDOUBLE__POW5__TABLE$object.data;

int64_t _M0MPB4Iter4nextN6constrS9980GUssEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUssEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsfEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUssEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsbEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsfEE = 0ll;

moonbit_string_t _M0FP28JIA2JIA29moonbitdb16show__opt__float(
  void* _M0L3optS1715
) {
  float _M0L1vS1713;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1714;
  moonbit_string_t _result_3876;
  #line 15 "/home/developer/Documents2/moonbitDB/demo.mbt"
  switch (Moonbit_object_tag(_M0L3optS1715)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1716 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1715;
      float _M0L4_2avS1717 = _M0L7_2aSomeS1716->$0;
      _M0L1vS1713 = _M0L4_2avS1717;
      goto join_1712;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_0.data;
      break;
    }
  }
  join_1712:;
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1714
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1714, _M0L1vS1713);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_3876
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1714);
  moonbit_decref(_M0L18_2astring__builderS1714);
  return _result_3876;
}

moonbit_string_t _M0FP28JIA2JIA29moonbitdb14show__opt__int(
  int64_t _M0L3optS1709
) {
  int32_t _M0L1vS1707;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1708;
  moonbit_string_t _result_3878;
  #line 8 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS1709 == 4294967296ll) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int64_t _M0L7_2aSomeS1710 = _M0L3optS1709;
    int32_t _M0L4_2avS1711 = (int32_t)_M0L7_2aSomeS1710;
    _M0L1vS1707 = _M0L4_2avS1711;
    goto join_1706;
  }
  join_1706:;
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1708
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1708, _M0L1vS1707);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_3878
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1708);
  moonbit_decref(_M0L18_2astring__builderS1708);
  return _result_3878;
}

moonbit_string_t _M0FP28JIA2JIA29moonbitdb9show__opt(
  moonbit_string_t _M0L3optS1703
) {
  moonbit_string_t _M0L1vS1701;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1702;
  moonbit_string_t _result_3880;
  #line 1 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS1703 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS1704 = _M0L3optS1703;
    moonbit_string_t _M0L4_2avS1705 = _M0L7_2aSomeS1704;
    moonbit_incref(_M0L4_2avS1705);
    _M0L1vS1701 = _M0L4_2avS1705;
    goto join_1700;
  }
  join_1700:;
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1702
  = _M0MPB13StringBuilder21StringBuilder_2einner(2);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1702, (moonbit_string_t)moonbit_string_literal_1.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1702, _M0L1vS1701);
  moonbit_decref(_M0L1vS1701);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1702, (moonbit_string_t)moonbit_string_literal_1.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_3880
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1702);
  moonbit_decref(_M0L18_2astring__builderS1702);
  return _result_3880;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database7command() {
  moonbit_string_t* _M0L6_2atmpS3449;
  struct _M0TPB5ArrayGsE* _block_3881;
  #line 1594 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3449 = (moonbit_string_t*)moonbit_make_ref_array_raw(81);
  _M0L6_2atmpS3449[0] = (moonbit_string_t)moonbit_string_literal_2.data;
  _M0L6_2atmpS3449[1] = (moonbit_string_t)moonbit_string_literal_3.data;
  _M0L6_2atmpS3449[2] = (moonbit_string_t)moonbit_string_literal_4.data;
  _M0L6_2atmpS3449[3] = (moonbit_string_t)moonbit_string_literal_5.data;
  _M0L6_2atmpS3449[4] = (moonbit_string_t)moonbit_string_literal_6.data;
  _M0L6_2atmpS3449[5] = (moonbit_string_t)moonbit_string_literal_7.data;
  _M0L6_2atmpS3449[6] = (moonbit_string_t)moonbit_string_literal_8.data;
  _M0L6_2atmpS3449[7] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3449[8] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3449[9] = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3449[10] = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3449[11] = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3449[12] = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3449[13] = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS3449[14] = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3449[15] = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS3449[16] = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3449[17] = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3449[18] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS3449[19] = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3449[20] = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3449[21] = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS3449[22] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS3449[23] = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3449[24] = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS3449[25] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS3449[26] = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS3449[27] = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3449[28] = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3449[29] = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3449[30] = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS3449[31] = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS3449[32] = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3449[33] = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS3449[34] = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS3449[35] = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS3449[36] = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3449[37] = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS3449[38] = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS3449[39] = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS3449[40] = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3449[41] = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3449[42] = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS3449[43] = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS3449[44] = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS3449[45] = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS3449[46] = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS3449[47] = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS3449[48] = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS3449[49] = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3449[50] = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS3449[51] = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L6_2atmpS3449[52] = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS3449[53] = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS3449[54] = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS3449[55] = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS3449[56] = (moonbit_string_t)moonbit_string_literal_58.data;
  _M0L6_2atmpS3449[57] = (moonbit_string_t)moonbit_string_literal_59.data;
  _M0L6_2atmpS3449[58] = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS3449[59] = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L6_2atmpS3449[60] = (moonbit_string_t)moonbit_string_literal_62.data;
  _M0L6_2atmpS3449[61] = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS3449[62] = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS3449[63] = (moonbit_string_t)moonbit_string_literal_65.data;
  _M0L6_2atmpS3449[64] = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L6_2atmpS3449[65] = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS3449[66] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS3449[67] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS3449[68] = (moonbit_string_t)moonbit_string_literal_70.data;
  _M0L6_2atmpS3449[69] = (moonbit_string_t)moonbit_string_literal_71.data;
  _M0L6_2atmpS3449[70] = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS3449[71] = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS3449[72] = (moonbit_string_t)moonbit_string_literal_74.data;
  _M0L6_2atmpS3449[73] = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS3449[74] = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS3449[75] = (moonbit_string_t)moonbit_string_literal_77.data;
  _M0L6_2atmpS3449[76] = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L6_2atmpS3449[77] = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L6_2atmpS3449[78] = (moonbit_string_t)moonbit_string_literal_80.data;
  _M0L6_2atmpS3449[79] = (moonbit_string_t)moonbit_string_literal_81.data;
  _M0L6_2atmpS3449[80] = (moonbit_string_t)moonbit_string_literal_82.data;
  _block_3881
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_3881)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _block_3881->$0 = _M0L6_2atmpS3449;
  _block_3881->$1 = 81;
  return _block_3881;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4ping() {
  #line 1556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  return (moonbit_string_t)moonbit_string_literal_83.data;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database9randomkey(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1690
) {
  moonbit_string_t* _M0L6_2atmpS3448;
  struct _M0TPB5ArrayGsE* _M0L9all__keysS1688;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3443;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L5_2aitS1689;
  int32_t _M0L6_2atmpS3444;
  #line 1503 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3448 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L9all__keysS1688
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9all__keysS1688)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L9all__keysS1688->$0 = _M0L6_2atmpS3448;
  _M0L9all__keysS1688->$1 = 0;
  _M0L4dataS3443 = _M0L4selfS1690->$0;
  moonbit_incref(_M0L4dataS3443);
  #line 1504 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1689
  = _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3443);
  moonbit_decref(_M0L4dataS3443);
  while (1) {
    moonbit_string_t _M0L3keyS1692;
    struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1694;
    int32_t _M0L6_2atmpS3442;
    #line 1505 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1694
    = _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L5_2aitS1689);
    if (_M0L7_2abindS1694 == 0) {
      if (_M0L7_2abindS1694) {
        moonbit_decref(_M0L7_2abindS1694);
      }
      moonbit_decref(_M0L5_2aitS1689);
    } else {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1695 =
        _M0L7_2abindS1694;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1696 =
        _M0L7_2aSomeS1695;
      moonbit_string_t _M0L8_2afieldS3450 = _M0L4_2axS1696->$0;
      int32_t _M0L6_2acntS3809 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1696));
      moonbit_string_t _M0L6_2akeyS1697;
      if (_M0L6_2acntS3809 > 1) {
        int32_t _M0L11_2anew__cntS3811 = _M0L6_2acntS3809 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1696), _M0L11_2anew__cntS3811);
        moonbit_incref(_M0L8_2afieldS3450);
      } else if (_M0L6_2acntS3809 == 1) {
        void* _M0L8_2afieldS3810 = _M0L4_2axS1696->$1;
        moonbit_decref(_M0L8_2afieldS3810);
        #line 1505 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1696);
      }
      _M0L6_2akeyS1697 = _M0L8_2afieldS3450;
      _M0L3keyS1692 = _M0L6_2akeyS1697;
      goto join_1691;
    }
    goto joinlet_3883;
    join_1691:;
    #line 1506 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3442
    = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1690, _M0L3keyS1692);
    if (!_M0L6_2atmpS3442) {
      #line 1507 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L9all__keysS1688, _M0L3keyS1692);
      moonbit_decref(_M0L3keyS1692);
    } else {
      moonbit_decref(_M0L3keyS1692);
    }
    continue;
    joinlet_3883:;
    break;
  }
  #line 1510 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3444 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1688);
  if (_M0L6_2atmpS3444 == 0) {
    moonbit_decref(_M0L9all__keysS1688);
    return 0;
  } else {
    int32_t _M0L13current__timeS3446 = _M0L4selfS1690->$2;
    int32_t _M0L6_2atmpS3447;
    int32_t _M0L3idxS1698;
    int32_t _M0L9safe__idxS1699;
    moonbit_string_t _M0L6_2atmpS3445;
    #line 1513 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3447 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1688);
    _M0L3idxS1698 = _M0L13current__timeS3446 % _M0L6_2atmpS3447;
    if (_M0L3idxS1698 < 0) {
      _M0L9safe__idxS1699 = -_M0L3idxS1698;
    } else {
      _M0L9safe__idxS1699 = _M0L3idxS1698;
    }
    #line 1515 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3445
    = _M0MPC15array5Array2atGsE(_M0L9all__keysS1688, _M0L9safe__idxS1699);
    moonbit_decref(_M0L9all__keysS1688);
    return _M0L6_2atmpS3445;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1680
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1678;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3441;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L5_2aitS1679;
  int32_t _result_3886;
  #line 1484 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5countS1678
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1678)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1678->$0 = 0;
  _M0L4dataS3441 = _M0L4selfS1680->$0;
  moonbit_incref(_M0L4dataS3441);
  #line 1485 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1679
  = _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3441);
  moonbit_decref(_M0L4dataS3441);
  while (1) {
    moonbit_string_t _M0L3keyS1682;
    struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1684;
    int32_t _M0L6_2atmpS3438;
    #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1684
    = _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L5_2aitS1679);
    if (_M0L7_2abindS1684 == 0) {
      if (_M0L7_2abindS1684) {
        moonbit_decref(_M0L7_2abindS1684);
      }
      moonbit_decref(_M0L5_2aitS1679);
    } else {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1685 =
        _M0L7_2abindS1684;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1686 =
        _M0L7_2aSomeS1685;
      moonbit_string_t _M0L8_2afieldS3452 = _M0L4_2axS1686->$0;
      int32_t _M0L6_2acntS3812 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1686));
      moonbit_string_t _M0L6_2akeyS1687;
      if (_M0L6_2acntS3812 > 1) {
        int32_t _M0L11_2anew__cntS3814 = _M0L6_2acntS3812 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1686), _M0L11_2anew__cntS3814);
        moonbit_incref(_M0L8_2afieldS3452);
      } else if (_M0L6_2acntS3812 == 1) {
        void* _M0L8_2afieldS3813 = _M0L4_2axS1686->$1;
        moonbit_decref(_M0L8_2afieldS3813);
        #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1686);
      }
      _M0L6_2akeyS1687 = _M0L8_2afieldS3452;
      _M0L3keyS1682 = _M0L6_2akeyS1687;
      goto join_1681;
    }
    goto joinlet_3885;
    join_1681:;
    #line 1487 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3438
    = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1680, _M0L3keyS1682);
    moonbit_decref(_M0L3keyS1682);
    if (!_M0L6_2atmpS3438) {
      int32_t _M0L3valS3440 = _M0L5countS1678->$0;
      int32_t _M0L6_2atmpS3439 = _M0L3valS3440 + 1;
      _M0L5countS1678->$0 = _M0L6_2atmpS3439;
    }
    continue;
    joinlet_3885:;
    break;
  }
  _result_3886 = _M0L5countS1678->$0;
  moonbit_decref(_M0L5countS1678);
  return _result_3886;
}

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database5zrank(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1663,
  moonbit_string_t _M0L3keyS1664,
  moonbit_string_t _M0L11member__valS1668
) {
  #line 1328 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1329 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1663, _M0L3keyS1664)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1667;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3437 =
      _M0L4selfS1663->$0;
    void* _M0L7_2abindS1673;
    int32_t _M0L6_2atmpS3431;
    moonbit_incref(_M0L4dataS3437);
    #line 1332 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1673
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3437, _M0L3keyS1664);
    moonbit_decref(_M0L4dataS3437);
    if (_M0L7_2abindS1673 == 0) {
      if (_M0L7_2abindS1673) {
        moonbit_decref(_M0L7_2abindS1673);
      }
      goto join_1665;
    } else {
      void* _M0L7_2aSomeS1674 = _M0L7_2abindS1673;
      void* _M0L4_2axS1675 = _M0L7_2aSomeS1674;
      switch (Moonbit_object_tag(_M0L4_2axS1675)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1676 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1675;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3455 = _M0L7_2aZSetS1676->$0;
          int32_t _M0L6_2acntS3817 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1676));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1677;
          if (_M0L6_2acntS3817 > 1) {
            int32_t _M0L11_2anew__cntS3818 = _M0L6_2acntS3817 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1676), _M0L11_2anew__cntS3818);
            moonbit_incref(_M0L8_2afieldS3455);
          } else if (_M0L6_2acntS3817 == 1) {
            #line 1332 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1676);
          }
          _M0L7_2azsetS1677 = _M0L8_2afieldS3455;
          _M0L4zsetS1667 = _M0L7_2azsetS1677;
          goto join_1666;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1675);
          goto join_1665;
          break;
        }
      }
    }
    join_1666:;
    #line 1334 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3431
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1667, _M0L11member__valS1668);
    moonbit_decref(_M0L4zsetS1667);
    if (!_M0L6_2atmpS3431) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1669;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1670;
      int32_t _M0L1iS1671;
      int32_t _M0L3valS3436;
      #line 1337 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6sortedS1669
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1663, _M0L3keyS1664);
      _M0L4rankS1670
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1670)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1670->$0 = 0;
      _M0L1iS1671 = 0;
      while (1) {
        int32_t _M0L6_2atmpS3432;
        #line 1339 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3432 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1669);
        if (_M0L1iS1671 < _M0L6_2atmpS3432) {
          struct _M0TUsfE* _M0L6_2atmpS3434;
          moonbit_string_t _M0L8_2afieldS3454;
          int32_t _M0L6_2acntS3815;
          moonbit_string_t _M0L6_2atmpS3433;
          int32_t _result_3890;
          int32_t _M0L6_2atmpS3435;
          #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0L6_2atmpS3434
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1669, _M0L1iS1671);
          _M0L8_2afieldS3454 = _M0L6_2atmpS3434->$0;
          _M0L6_2acntS3815
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3434));
          if (_M0L6_2acntS3815 > 1) {
            int32_t _M0L11_2anew__cntS3816 = _M0L6_2acntS3815 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3434), _M0L11_2anew__cntS3816);
            moonbit_incref(_M0L8_2afieldS3454);
          } else if (_M0L6_2acntS3815 == 1) {
            #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2atmpS3434);
          }
          _M0L6_2atmpS3433 = _M0L8_2afieldS3454;
          #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _result_3890
          = _M0L6_2atmpS3433 == _M0L11member__valS1668
            || Moonbit_array_length(_M0L6_2atmpS3433)
               == Moonbit_array_length(_M0L11member__valS1668)
               && 0
                  == memcmp(_M0L6_2atmpS3433, _M0L11member__valS1668, Moonbit_array_length(_M0L6_2atmpS3433) * 2);
          moonbit_decref(_M0L6_2atmpS3433);
          if (_result_3890) {
            moonbit_decref(_M0L6sortedS1669);
            _M0L4rankS1670->$0 = _M0L1iS1671;
            break;
          }
          _M0L6_2atmpS3435 = _M0L1iS1671 + 1;
          _M0L1iS1671 = _M0L6_2atmpS3435;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1669);
        }
        break;
      }
      _M0L3valS3436 = _M0L4rankS1670->$0;
      moonbit_decref(_M0L4rankS1670);
      return (int64_t)_M0L3valS3436;
    }
    join_1665:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database9zrevrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1652,
  moonbit_string_t _M0L3keyS1653,
  int32_t _M0L5startS1657,
  int32_t _M0L3endS1659
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1651;
  int32_t _M0L3lenS1654;
  moonbit_string_t* _M0L6_2atmpS3430;
  struct _M0TPB5ArrayGsE* _M0L6resultS1655;
  int32_t _M0L10start__idxS1656;
  int32_t _M0L8end__idxS1658;
  int32_t _M0L1iS1660;
  #line 1302 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1303 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6sortedS1651
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1652, _M0L3keyS1653);
  #line 1304 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1654 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1651);
  _M0L6_2atmpS3430 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1655
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1655)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1655->$0 = _M0L6_2atmpS3430;
  _M0L6resultS1655->$1 = 0;
  if (_M0L5startS1657 < 0) {
    _M0L10start__idxS1656 = _M0L3lenS1654 + _M0L5startS1657;
  } else {
    _M0L10start__idxS1656 = _M0L5startS1657;
  }
  if (_M0L3endS1659 < 0) {
    _M0L8end__idxS1658 = _M0L3lenS1654 + _M0L3endS1659;
  } else {
    _M0L8end__idxS1658 = _M0L3endS1659;
  }
  _M0L1iS1660 = _M0L10start__idxS1656;
  while (1) {
    int32_t _if__result_3892;
    if (_M0L1iS1660 <= _M0L8end__idxS1658) {
      _if__result_3892 = _M0L1iS1660 < _M0L3lenS1654;
    } else {
      _if__result_3892 = 0;
    }
    if (_if__result_3892) {
      int32_t _M0L6_2atmpS3428 = _M0L3lenS1654 - 1;
      int32_t _M0L8rev__idxS1661 = _M0L6_2atmpS3428 - _M0L1iS1660;
      int32_t _M0L6_2atmpS3429;
      if (_M0L8rev__idxS1661 >= 0) {
        struct _M0TUsfE* _M0L6_2atmpS3427;
        moonbit_string_t _M0L8_2afieldS3457;
        int32_t _M0L6_2acntS3819;
        moonbit_string_t _M0L6_2atmpS3426;
        #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3427
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1651, _M0L8rev__idxS1661);
        _M0L8_2afieldS3457 = _M0L6_2atmpS3427->$0;
        _M0L6_2acntS3819
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3427));
        if (_M0L6_2acntS3819 > 1) {
          int32_t _M0L11_2anew__cntS3820 = _M0L6_2acntS3819 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3427), _M0L11_2anew__cntS3820);
          moonbit_incref(_M0L8_2afieldS3457);
        } else if (_M0L6_2acntS3819 == 1) {
          #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2atmpS3427);
        }
        _M0L6_2atmpS3426 = _M0L8_2afieldS3457;
        #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1655, _M0L6_2atmpS3426);
        moonbit_decref(_M0L6_2atmpS3426);
      }
      _M0L6_2atmpS3429 = _M0L1iS1660 + 1;
      _M0L1iS1660 = _M0L6_2atmpS3429;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1651);
    }
    break;
  }
  return _M0L6resultS1655;
}

struct _M0TPB5ArrayGUsfEE* _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1630,
  moonbit_string_t _M0L3keyS1631
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1630, _M0L3keyS1631)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS3421 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_3893 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_3893)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _block_3893->$0 = _M0L6_2atmpS3421;
    _block_3893->$1 = 0;
    return _block_3893;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1634;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3425 =
      _M0L4selfS1630->$0;
    void* _M0L7_2abindS1646;
    struct _M0TUsfE** _M0L6_2atmpS3424;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1635;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1636;
    struct _M0TPB5ArrayGUsfEE* _result_3898;
    struct _M0TUsfE** _M0L6_2atmpS3422;
    struct _M0TPB5ArrayGUsfEE* _block_3899;
    moonbit_incref(_M0L4dataS3425);
    #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1646
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3425, _M0L3keyS1631);
    moonbit_decref(_M0L4dataS3425);
    if (_M0L7_2abindS1646 == 0) {
      if (_M0L7_2abindS1646) {
        moonbit_decref(_M0L7_2abindS1646);
      }
      goto join_1632;
    } else {
      void* _M0L7_2aSomeS1647 = _M0L7_2abindS1646;
      void* _M0L4_2axS1648 = _M0L7_2aSomeS1647;
      switch (Moonbit_object_tag(_M0L4_2axS1648)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1649 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1648;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3459 = _M0L7_2aZSetS1649->$0;
          int32_t _M0L6_2acntS3823 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1649));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1650;
          if (_M0L6_2acntS3823 > 1) {
            int32_t _M0L11_2anew__cntS3824 = _M0L6_2acntS3823 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1649), _M0L11_2anew__cntS3824);
            moonbit_incref(_M0L8_2afieldS3459);
          } else if (_M0L6_2acntS3823 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1649);
          }
          _M0L7_2azsetS1650 = _M0L8_2afieldS3459;
          _M0L4zsetS1634 = _M0L7_2azsetS1650;
          goto join_1633;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1648);
          goto join_1632;
          break;
        }
      }
    }
    join_1633:;
    _M0L6_2atmpS3424 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1635
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1635)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _M0L5itemsS1635->$0 = _M0L6_2atmpS3424;
    _M0L5itemsS1635->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1636 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1634);
    moonbit_decref(_M0L4zsetS1634);
    while (1) {
      moonbit_string_t _M0L1mS1638;
      float _M0L1sS1639;
      struct _M0TUsfE* _M0L7_2abindS1641;
      struct _M0TUsfE* _M0L8_2atupleS3423;
      #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1641 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1636);
      if (_M0L7_2abindS1641 == 0) {
        if (_M0L7_2abindS1641) {
          moonbit_decref(_M0L7_2abindS1641);
        }
        moonbit_decref(_M0L5_2aitS1636);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1642 = _M0L7_2abindS1641;
        struct _M0TUsfE* _M0L4_2axS1643 = _M0L7_2aSomeS1642;
        moonbit_string_t _M0L4_2amS1644 = _M0L4_2axS1643->$0;
        float _M0L4_2asS1645 = _M0L4_2axS1643->$1;
        int32_t _M0L6_2acntS3821 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1643));
        if (_M0L6_2acntS3821 > 1) {
          int32_t _M0L11_2anew__cntS3822 = _M0L6_2acntS3821 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1643), _M0L11_2anew__cntS3822);
          moonbit_incref(_M0L4_2amS1644);
        } else if (_M0L6_2acntS3821 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1643);
        }
        _M0L1mS1638 = _M0L4_2amS1644;
        _M0L1sS1639 = _M0L4_2asS1645;
        goto join_1637;
      }
      goto joinlet_3897;
      join_1637:;
      _M0L8_2atupleS3423
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3423)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
      _M0L8_2atupleS3423->$0 = _M0L1mS1638;
      _M0L8_2atupleS3423->$1 = _M0L1sS1639;
      #line 1271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1635, _M0L8_2atupleS3423);
      moonbit_decref(_M0L8_2atupleS3423);
      continue;
      joinlet_3897:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3898
    = _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(_M0L5itemsS1635);
    moonbit_decref(_M0L5itemsS1635);
    return _result_3898;
    join_1632:;
    _M0L6_2atmpS3422 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_3899
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_3899)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _block_3899->$0 = _M0L6_2atmpS3422;
    _block_3899->$1 = 0;
    return _block_3899;
  }
}

void* _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1619,
  moonbit_string_t _M0L3keyS1620,
  moonbit_string_t _M0L11member__valS1624
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1619, _M0L3keyS1620)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1623;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3420 =
      _M0L4selfS1619->$0;
    void* _M0L7_2abindS1625;
    void* _result_3902;
    moonbit_incref(_M0L4dataS3420);
    #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1625
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3420, _M0L3keyS1620);
    moonbit_decref(_M0L4dataS3420);
    if (_M0L7_2abindS1625 == 0) {
      if (_M0L7_2abindS1625) {
        moonbit_decref(_M0L7_2abindS1625);
      }
      goto join_1621;
    } else {
      void* _M0L7_2aSomeS1626 = _M0L7_2abindS1625;
      void* _M0L4_2axS1627 = _M0L7_2aSomeS1626;
      switch (Moonbit_object_tag(_M0L4_2axS1627)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1628 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1627;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3461 = _M0L7_2aZSetS1628->$0;
          int32_t _M0L6_2acntS3825 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1628));
          struct _M0TPB3MapGsfE* _M0L4_2azS1629;
          if (_M0L6_2acntS3825 > 1) {
            int32_t _M0L11_2anew__cntS3826 = _M0L6_2acntS3825 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1628), _M0L11_2anew__cntS3826);
            moonbit_incref(_M0L8_2afieldS3461);
          } else if (_M0L6_2acntS3825 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1628);
          }
          _M0L4_2azS1629 = _M0L8_2afieldS3461;
          _M0L1zS1623 = _M0L4_2azS1629;
          goto join_1622;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1627);
          goto join_1621;
          break;
        }
      }
    }
    join_1622:;
    #line 1239 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3902 = _M0MPB3Map3getGsfE(_M0L1zS1623, _M0L11member__valS1624);
    moonbit_decref(_M0L1zS1623);
    return _result_3902;
    join_1621:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5zcard(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1610,
  moonbit_string_t _M0L3keyS1611
) {
  #line 1223 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1224 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1610, _M0L3keyS1611)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1613;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3419 =
      _M0L4selfS1610->$0;
    void* _M0L7_2abindS1614;
    int32_t _result_3904;
    moonbit_incref(_M0L4dataS3419);
    #line 1227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1614
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3419, _M0L3keyS1611);
    moonbit_decref(_M0L4dataS3419);
    if (_M0L7_2abindS1614 == 0) {
      if (_M0L7_2abindS1614) {
        moonbit_decref(_M0L7_2abindS1614);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1615 = _M0L7_2abindS1614;
      void* _M0L4_2axS1616 = _M0L7_2aSomeS1615;
      switch (Moonbit_object_tag(_M0L4_2axS1616)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1617 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1616;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3463 = _M0L7_2aZSetS1617->$0;
          int32_t _M0L6_2acntS3827 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1617));
          struct _M0TPB3MapGsfE* _M0L4_2azS1618;
          if (_M0L6_2acntS3827 > 1) {
            int32_t _M0L11_2anew__cntS3828 = _M0L6_2acntS3827 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1617), _M0L11_2anew__cntS3828);
            moonbit_incref(_M0L8_2afieldS3463);
          } else if (_M0L6_2acntS3827 == 1) {
            #line 1227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1617);
          }
          _M0L4_2azS1618 = _M0L8_2afieldS3463;
          _M0L1zS1613 = _M0L4_2azS1618;
          goto join_1612;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1616);
          return 0;
          break;
        }
      }
    }
    join_1612:;
    #line 1228 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3904 = _M0MPB3Map6lengthGsfE(_M0L1zS1613);
    moonbit_decref(_M0L1zS1613);
    return _result_3904;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1609
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  return _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L5itemsS1609);
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1601
) {
  int32_t _M0L3lenS1600;
  #line 1181 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1600 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1601);
  if (_M0L3lenS1600 <= 1) {
    moonbit_incref(_M0L3arrS1601);
    return _M0L3arrS1601;
  } else {
    int32_t _M0L3midS1602 = _M0L3lenS1600 / 2;
    struct _M0TUsfE** _M0L6_2atmpS3418 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1603 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS3417;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1604;
    int32_t _M0L1iS1605;
    int32_t _M0L1iS1607;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3415;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3416;
    struct _M0TPB5ArrayGUsfEE* _result_3907;
    Moonbit_object_header(_M0L4leftS1603)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _M0L4leftS1603->$0 = _M0L6_2atmpS3418;
    _M0L4leftS1603->$1 = 0;
    _M0L6_2atmpS3417 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1604
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1604)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _M0L5rightS1604->$0 = _M0L6_2atmpS3417;
    _M0L5rightS1604->$1 = 0;
    _M0L1iS1605 = 0;
    while (1) {
      if (_M0L1iS1605 < _M0L3midS1602) {
        struct _M0TUsfE* _M0L6_2atmpS3411;
        int32_t _M0L6_2atmpS3412;
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3411
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1601, _M0L1iS1605);
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1603, _M0L6_2atmpS3411);
        moonbit_decref(_M0L6_2atmpS3411);
        _M0L6_2atmpS3412 = _M0L1iS1605 + 1;
        _M0L1iS1605 = _M0L6_2atmpS3412;
        continue;
      }
      break;
    }
    _M0L1iS1607 = _M0L3midS1602;
    while (1) {
      if (_M0L1iS1607 < _M0L3lenS1600) {
        struct _M0TUsfE* _M0L6_2atmpS3413;
        int32_t _M0L6_2atmpS3414;
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3413
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1601, _M0L1iS1607);
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1604, _M0L6_2atmpS3413);
        moonbit_decref(_M0L6_2atmpS3413);
        _M0L6_2atmpS3414 = _M0L1iS1607 + 1;
        _M0L1iS1607 = _M0L6_2atmpS3414;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3415
    = _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L4leftS1603);
    moonbit_decref(_M0L4leftS1603);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3416
    = _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L5rightS1604);
    moonbit_decref(_M0L5rightS1604);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3907
    = _M0FP38JIA2JIA29moonbitdb3lib5merge(_M0L6_2atmpS3415, _M0L6_2atmpS3416);
    moonbit_decref(_M0L6_2atmpS3415);
    moonbit_decref(_M0L6_2atmpS3416);
    return _result_3907;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1595,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1596
) {
  struct _M0TUsfE** _M0L6_2atmpS3410;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1592;
  struct _M0TPB8MutLocalGiE* _M0L1iS1593;
  struct _M0TPB8MutLocalGiE* _M0L1jS1594;
  #line 1199 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3410 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1592
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1592)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
  _M0L6resultS1592->$0 = _M0L6_2atmpS3410;
  _M0L6resultS1592->$1 = 0;
  _M0L1iS1593
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1593)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1593->$0 = 0;
  _M0L1jS1594
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1594)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1594->$0 = 0;
  while (1) {
    int32_t _M0L3valS3382 = _M0L1iS1593->$0;
    int32_t _M0L6_2atmpS3383;
    int32_t _if__result_3909;
    #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3383 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1595);
    if (_M0L3valS3382 < _M0L6_2atmpS3383) {
      int32_t _M0L3valS3380 = _M0L1jS1594->$0;
      int32_t _M0L6_2atmpS3381;
      #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3381 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1596);
      _if__result_3909 = _M0L3valS3380 < _M0L6_2atmpS3381;
    } else {
      _if__result_3909 = 0;
    }
    if (_if__result_3909) {
      int32_t _M0L3valS3389 = _M0L1iS1593->$0;
      struct _M0TUsfE* _M0L6_2atmpS3388;
      float _M0L6_2atmpS3384;
      int32_t _M0L3valS3387;
      struct _M0TUsfE* _M0L6_2atmpS3386;
      float _M0L6_2atmpS3385;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3388
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1595, _M0L3valS3389);
      _M0L6_2atmpS3384 = _M0L6_2atmpS3388->$1;
      moonbit_decref(_M0L6_2atmpS3388);
      _M0L3valS3387 = _M0L1jS1594->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3386
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1596, _M0L3valS3387);
      _M0L6_2atmpS3385 = _M0L6_2atmpS3386->$1;
      moonbit_decref(_M0L6_2atmpS3386);
      if (_M0L6_2atmpS3384 <= _M0L6_2atmpS3385) {
        int32_t _M0L3valS3391 = _M0L1iS1593->$0;
        struct _M0TUsfE* _M0L6_2atmpS3390;
        int32_t _M0L3valS3393;
        int32_t _M0L6_2atmpS3392;
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3390
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1595, _M0L3valS3391);
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1592, _M0L6_2atmpS3390);
        moonbit_decref(_M0L6_2atmpS3390);
        _M0L3valS3393 = _M0L1iS1593->$0;
        _M0L6_2atmpS3392 = _M0L3valS3393 + 1;
        _M0L1iS1593->$0 = _M0L6_2atmpS3392;
      } else {
        int32_t _M0L3valS3395 = _M0L1jS1594->$0;
        struct _M0TUsfE* _M0L6_2atmpS3394;
        int32_t _M0L3valS3397;
        int32_t _M0L6_2atmpS3396;
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3394
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1596, _M0L3valS3395);
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1592, _M0L6_2atmpS3394);
        moonbit_decref(_M0L6_2atmpS3394);
        _M0L3valS3397 = _M0L1jS1594->$0;
        _M0L6_2atmpS3396 = _M0L3valS3397 + 1;
        _M0L1jS1594->$0 = _M0L6_2atmpS3396;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3398 = _M0L1iS1593->$0;
    int32_t _M0L6_2atmpS3399;
    #line 1212 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3399 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1595);
    if (_M0L3valS3398 < _M0L6_2atmpS3399) {
      int32_t _M0L3valS3401 = _M0L1iS1593->$0;
      struct _M0TUsfE* _M0L6_2atmpS3400;
      int32_t _M0L3valS3403;
      int32_t _M0L6_2atmpS3402;
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3400
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1595, _M0L3valS3401);
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1592, _M0L6_2atmpS3400);
      moonbit_decref(_M0L6_2atmpS3400);
      _M0L3valS3403 = _M0L1iS1593->$0;
      _M0L6_2atmpS3402 = _M0L3valS3403 + 1;
      _M0L1iS1593->$0 = _M0L6_2atmpS3402;
      continue;
    } else {
      moonbit_decref(_M0L1iS1593);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3404 = _M0L1jS1594->$0;
    int32_t _M0L6_2atmpS3405;
    #line 1216 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3405 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1596);
    if (_M0L3valS3404 < _M0L6_2atmpS3405) {
      int32_t _M0L3valS3407 = _M0L1jS1594->$0;
      struct _M0TUsfE* _M0L6_2atmpS3406;
      int32_t _M0L3valS3409;
      int32_t _M0L6_2atmpS3408;
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3406
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1596, _M0L3valS3407);
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1592, _M0L6_2atmpS3406);
      moonbit_decref(_M0L6_2atmpS3406);
      _M0L3valS3409 = _M0L1jS1594->$0;
      _M0L6_2atmpS3408 = _M0L3valS3409 + 1;
      _M0L1jS1594->$0 = _M0L6_2atmpS3408;
      continue;
    } else {
      moonbit_decref(_M0L1jS1594);
    }
    break;
  }
  return _M0L6resultS1592;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1577,
  moonbit_string_t _M0L3keyS1578,
  float _M0L5scoreS1591,
  moonbit_string_t _M0L11member__valS1590
) {
  int32_t _M0L6_2atmpS3374;
  struct _M0TPB3MapGsfE* _M0L4zsetS1579;
  struct _M0TPB3MapGsfE* _M0L1zS1583;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3379;
  void* _M0L7_2abindS1584;
  struct _M0TUsfE** _M0L7_2abindS1581;
  struct _M0TUsfE** _M0L6_2atmpS3378;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3377;
  int32_t _M0L7existedS1589;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3375;
  void* _M0L4ZSetS3376;
  #line 1140 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3374
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1577, _M0L3keyS1578);
  _M0L4dataS3379 = _M0L4selfS1577->$0;
  moonbit_incref(_M0L4dataS3379);
  #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1584
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3379, _M0L3keyS1578);
  moonbit_decref(_M0L4dataS3379);
  if (_M0L7_2abindS1584 == 0) {
    if (_M0L7_2abindS1584) {
      moonbit_decref(_M0L7_2abindS1584);
    }
    goto join_1580;
  } else {
    void* _M0L7_2aSomeS1585 = _M0L7_2abindS1584;
    void* _M0L4_2axS1586 = _M0L7_2aSomeS1585;
    switch (Moonbit_object_tag(_M0L4_2axS1586)) {
      case 4: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1587 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1586;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3466 = _M0L7_2aZSetS1587->$0;
        int32_t _M0L6_2acntS3829 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1587));
        struct _M0TPB3MapGsfE* _M0L4_2azS1588;
        if (_M0L6_2acntS3829 > 1) {
          int32_t _M0L11_2anew__cntS3830 = _M0L6_2acntS3829 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1587), _M0L11_2anew__cntS3830);
          moonbit_incref(_M0L8_2afieldS3466);
        } else if (_M0L6_2acntS3829 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aZSetS1587);
        }
        _M0L4_2azS1588 = _M0L8_2afieldS3466;
        _M0L1zS1583 = _M0L4_2azS1588;
        goto join_1582;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1586);
        goto join_1580;
        break;
      }
    }
  }
  goto joinlet_3913;
  join_1582:;
  _M0L4zsetS1579 = _M0L1zS1583;
  joinlet_3913:;
  goto joinlet_3912;
  join_1580:;
  _M0L7_2abindS1581 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3378 = _M0L7_2abindS1581;
  _M0L6_2atmpS3377
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3378, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4zsetS1579 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3377, 10ll);
  moonbit_decref(_M0L6_2atmpS3377.$0);
  joinlet_3912:;
  #line 1146 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7existedS1589
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1579, _M0L11member__valS1590);
  #line 1147 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1579, _M0L11member__valS1590, _M0L5scoreS1591);
  _M0L4dataS3375 = _M0L4selfS1577->$0;
  _M0L4ZSetS3376
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3376)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 4);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4ZSetS3376)->$0
  = _M0L4zsetS1579;
  moonbit_incref(_M0L4dataS3375);
  #line 1148 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3375, _M0L3keyS1578, _M0L4ZSetS3376);
  moonbit_decref(_M0L4dataS3375);
  moonbit_decref(_M0L4ZSetS3376);
  return !_M0L7existedS1589;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database9sismember(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1567,
  moonbit_string_t _M0L3keyS1568,
  moonbit_string_t _M0L5valueS1571
) {
  #line 963 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 964 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1567, _M0L3keyS1568)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1570;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3373 =
      _M0L4selfS1567->$0;
    void* _M0L7_2abindS1572;
    int32_t _result_3915;
    moonbit_incref(_M0L4dataS3373);
    #line 967 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1572
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3373, _M0L3keyS1568);
    moonbit_decref(_M0L4dataS3373);
    if (_M0L7_2abindS1572 == 0) {
      if (_M0L7_2abindS1572) {
        moonbit_decref(_M0L7_2abindS1572);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1573 = _M0L7_2abindS1572;
      void* _M0L4_2axS1574 = _M0L7_2aSomeS1573;
      switch (Moonbit_object_tag(_M0L4_2axS1574)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1575 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1574;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3468 = _M0L6_2aSetS1575->$0;
          int32_t _M0L6_2acntS3831 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1575));
          struct _M0TPB3MapGsbE* _M0L4_2asS1576;
          if (_M0L6_2acntS3831 > 1) {
            int32_t _M0L11_2anew__cntS3832 = _M0L6_2acntS3831 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1575), _M0L11_2anew__cntS3832);
            moonbit_incref(_M0L8_2afieldS3468);
          } else if (_M0L6_2acntS3831 == 1) {
            #line 967 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1575);
          }
          _M0L4_2asS1576 = _M0L8_2afieldS3468;
          _M0L1sS1570 = _M0L4_2asS1576;
          goto join_1569;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1574);
          return 0;
          break;
        }
      }
    }
    join_1569:;
    #line 968 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3915 = _M0MPB3Map8containsGsbE(_M0L1sS1570, _M0L5valueS1571);
    moonbit_decref(_M0L1sS1570);
    return _result_3915;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5scard(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1558,
  moonbit_string_t _M0L3keyS1559
) {
  #line 952 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 953 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1558, _M0L3keyS1559)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1561;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3372 =
      _M0L4selfS1558->$0;
    void* _M0L7_2abindS1562;
    int32_t _result_3917;
    moonbit_incref(_M0L4dataS3372);
    #line 956 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1562
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3372, _M0L3keyS1559);
    moonbit_decref(_M0L4dataS3372);
    if (_M0L7_2abindS1562 == 0) {
      if (_M0L7_2abindS1562) {
        moonbit_decref(_M0L7_2abindS1562);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1563 = _M0L7_2abindS1562;
      void* _M0L4_2axS1564 = _M0L7_2aSomeS1563;
      switch (Moonbit_object_tag(_M0L4_2axS1564)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1565 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1564;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3470 = _M0L6_2aSetS1565->$0;
          int32_t _M0L6_2acntS3833 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1565));
          struct _M0TPB3MapGsbE* _M0L4_2asS1566;
          if (_M0L6_2acntS3833 > 1) {
            int32_t _M0L11_2anew__cntS3834 = _M0L6_2acntS3833 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1565), _M0L11_2anew__cntS3834);
            moonbit_incref(_M0L8_2afieldS3470);
          } else if (_M0L6_2acntS3833 == 1) {
            #line 956 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1565);
          }
          _M0L4_2asS1566 = _M0L8_2afieldS3470;
          _M0L1sS1561 = _M0L4_2asS1566;
          goto join_1560;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1564);
          return 0;
          break;
        }
      }
    }
    join_1560:;
    #line 957 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3917 = _M0MPB3Map6lengthGsbE(_M0L1sS1561);
    moonbit_decref(_M0L1sS1561);
    return _result_3917;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database8smembers(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1539,
  moonbit_string_t _M0L3keyS1540
) {
  #line 917 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 918 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1539, _M0L3keyS1540)
  ) {
    moonbit_string_t* _M0L6_2atmpS3368 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3918 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3918)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_3918->$0 = _M0L6_2atmpS3368;
    _block_3918->$1 = 0;
    return _block_3918;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1543;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3371 =
      _M0L4selfS1539->$0;
    void* _M0L7_2abindS1553;
    moonbit_string_t* _M0L6_2atmpS3370;
    struct _M0TPB5ArrayGsE* _M0L6resultS1544;
    struct _M0TPB4IterGUsbEE* _M0L5_2aitS1545;
    moonbit_string_t* _M0L6_2atmpS3369;
    struct _M0TPB5ArrayGsE* _block_3923;
    moonbit_incref(_M0L4dataS3371);
    #line 921 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1553
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3371, _M0L3keyS1540);
    moonbit_decref(_M0L4dataS3371);
    if (_M0L7_2abindS1553 == 0) {
      if (_M0L7_2abindS1553) {
        moonbit_decref(_M0L7_2abindS1553);
      }
      goto join_1541;
    } else {
      void* _M0L7_2aSomeS1554 = _M0L7_2abindS1553;
      void* _M0L4_2axS1555 = _M0L7_2aSomeS1554;
      switch (Moonbit_object_tag(_M0L4_2axS1555)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1556 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1555;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3473 = _M0L6_2aSetS1556->$0;
          int32_t _M0L6_2acntS3837 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1556));
          struct _M0TPB3MapGsbE* _M0L4_2asS1557;
          if (_M0L6_2acntS3837 > 1) {
            int32_t _M0L11_2anew__cntS3838 = _M0L6_2acntS3837 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1556), _M0L11_2anew__cntS3838);
            moonbit_incref(_M0L8_2afieldS3473);
          } else if (_M0L6_2acntS3837 == 1) {
            #line 921 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1556);
          }
          _M0L4_2asS1557 = _M0L8_2afieldS3473;
          _M0L1sS1543 = _M0L4_2asS1557;
          goto join_1542;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1555);
          goto join_1541;
          break;
        }
      }
    }
    join_1542:;
    _M0L6_2atmpS3370 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1544
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1544)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1544->$0 = _M0L6_2atmpS3370;
    _M0L6resultS1544->$1 = 0;
    #line 923 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1545 = _M0MPB3Map5iter2GsbE(_M0L1sS1543);
    moonbit_decref(_M0L1sS1543);
    while (1) {
      moonbit_string_t _M0L1mS1547;
      struct _M0TUsbE* _M0L7_2abindS1549;
      #line 924 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1549 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1545);
      if (_M0L7_2abindS1549 == 0) {
        if (_M0L7_2abindS1549) {
          moonbit_decref(_M0L7_2abindS1549);
        }
        moonbit_decref(_M0L5_2aitS1545);
      } else {
        struct _M0TUsbE* _M0L7_2aSomeS1550 = _M0L7_2abindS1549;
        struct _M0TUsbE* _M0L4_2axS1551 = _M0L7_2aSomeS1550;
        moonbit_string_t _M0L8_2afieldS3472 = _M0L4_2axS1551->$0;
        int32_t _M0L6_2acntS3835 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1551));
        moonbit_string_t _M0L4_2amS1552;
        if (_M0L6_2acntS3835 > 1) {
          int32_t _M0L11_2anew__cntS3836 = _M0L6_2acntS3835 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1551), _M0L11_2anew__cntS3836);
          moonbit_incref(_M0L8_2afieldS3472);
        } else if (_M0L6_2acntS3835 == 1) {
          #line 924 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1551);
        }
        _M0L4_2amS1552 = _M0L8_2afieldS3472;
        _M0L1mS1547 = _M0L4_2amS1552;
        goto join_1546;
      }
      goto joinlet_3922;
      join_1546:;
      #line 925 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1544, _M0L1mS1547);
      moonbit_decref(_M0L1mS1547);
      continue;
      joinlet_3922:;
      break;
    }
    return _M0L6resultS1544;
    join_1541:;
    _M0L6_2atmpS3369 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_3923
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3923)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_3923->$0 = _M0L6_2atmpS3369;
    _block_3923->$1 = 0;
    return _block_3923;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1526,
  moonbit_string_t _M0L3keyS1527,
  moonbit_string_t _M0L5valueS1538
) {
  int32_t _M0L6_2atmpS3362;
  struct _M0TPB3MapGsbE* _M0L3setS1528;
  struct _M0TPB3MapGsbE* _M0L1sS1532;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3367;
  void* _M0L7_2abindS1533;
  struct _M0TUsbE** _M0L7_2abindS1530;
  struct _M0TUsbE** _M0L6_2atmpS3366;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3365;
  #line 902 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3362
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1526, _M0L3keyS1527);
  _M0L4dataS3367 = _M0L4selfS1526->$0;
  moonbit_incref(_M0L4dataS3367);
  #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1533
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3367, _M0L3keyS1527);
  moonbit_decref(_M0L4dataS3367);
  if (_M0L7_2abindS1533 == 0) {
    if (_M0L7_2abindS1533) {
      moonbit_decref(_M0L7_2abindS1533);
    }
    goto join_1529;
  } else {
    void* _M0L7_2aSomeS1534 = _M0L7_2abindS1533;
    void* _M0L4_2axS1535 = _M0L7_2aSomeS1534;
    switch (Moonbit_object_tag(_M0L4_2axS1535)) {
      case 3: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1536 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1535;
        struct _M0TPB3MapGsbE* _M0L8_2afieldS3476 = _M0L6_2aSetS1536->$0;
        int32_t _M0L6_2acntS3839 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1536));
        struct _M0TPB3MapGsbE* _M0L4_2asS1537;
        if (_M0L6_2acntS3839 > 1) {
          int32_t _M0L11_2anew__cntS3840 = _M0L6_2acntS3839 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1536), _M0L11_2anew__cntS3840);
          moonbit_incref(_M0L8_2afieldS3476);
        } else if (_M0L6_2acntS3839 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2aSetS1536);
        }
        _M0L4_2asS1537 = _M0L8_2afieldS3476;
        _M0L1sS1532 = _M0L4_2asS1537;
        goto join_1531;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1535);
        goto join_1529;
        break;
      }
    }
  }
  goto joinlet_3925;
  join_1531:;
  _M0L3setS1528 = _M0L1sS1532;
  joinlet_3925:;
  goto joinlet_3924;
  join_1529:;
  _M0L7_2abindS1530 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3366 = _M0L7_2abindS1530;
  _M0L6_2atmpS3365
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3366, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3setS1528 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3365, 10ll);
  moonbit_decref(_M0L6_2atmpS3365.$0);
  joinlet_3924:;
  #line 908 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1528, _M0L5valueS1538)) {
    moonbit_decref(_M0L3setS1528);
    return 0;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3363;
    void* _M0L3SetS3364;
    #line 911 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1528, _M0L5valueS1538, 1);
    _M0L4dataS3363 = _M0L4selfS1526->$0;
    _M0L3SetS3364
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS3364)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 12, 3);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L3SetS3364)->$0
    = _M0L3setS1528;
    moonbit_incref(_M0L4dataS3363);
    #line 912 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3363, _M0L3keyS1527, _M0L3SetS3364);
    moonbit_decref(_M0L4dataS3363);
    moonbit_decref(_M0L3SetS3364);
    return 1;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6lrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1507,
  moonbit_string_t _M0L3keyS1508,
  int32_t _M0L5startS1515,
  int32_t _M0L3endS1517
) {
  #line 720 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 721 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1507, _M0L3keyS1508)
  ) {
    moonbit_string_t* _M0L6_2atmpS3356 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3926 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3926)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_3926->$0 = _M0L6_2atmpS3356;
    _block_3926->$1 = 0;
    return _block_3926;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1511;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3361 =
      _M0L4selfS1507->$0;
    void* _M0L7_2abindS1521;
    struct _M0TPB5ArrayGsE* _M0L3arrS1512;
    int32_t _M0L3lenS1513;
    int32_t _M0L10start__idxS1514;
    int32_t _M0L8end__idxS1516;
    moonbit_string_t* _M0L6_2atmpS3360;
    struct _M0TPB5ArrayGsE* _M0L6resultS1518;
    int32_t _M0L1iS1519;
    moonbit_string_t* _M0L6_2atmpS3357;
    struct _M0TPB5ArrayGsE* _block_3931;
    moonbit_incref(_M0L4dataS3361);
    #line 724 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1521
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3361, _M0L3keyS1508);
    moonbit_decref(_M0L4dataS3361);
    if (_M0L7_2abindS1521 == 0) {
      if (_M0L7_2abindS1521) {
        moonbit_decref(_M0L7_2abindS1521);
      }
      goto join_1509;
    } else {
      void* _M0L7_2aSomeS1522 = _M0L7_2abindS1521;
      void* _M0L4_2axS1523 = _M0L7_2aSomeS1522;
      switch (Moonbit_object_tag(_M0L4_2axS1523)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1524 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1523;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3478 =
            _M0L7_2aListS1524->$0;
          int32_t _M0L6_2acntS3841 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1524));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2adequeS1525;
          if (_M0L6_2acntS3841 > 1) {
            int32_t _M0L11_2anew__cntS3842 = _M0L6_2acntS3841 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1524), _M0L11_2anew__cntS3842);
            moonbit_incref(_M0L8_2afieldS3478);
          } else if (_M0L6_2acntS3841 == 1) {
            #line 724 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1524);
          }
          _M0L8_2adequeS1525 = _M0L8_2afieldS3478;
          _M0L5dequeS1511 = _M0L8_2adequeS1525;
          goto join_1510;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1523);
          goto join_1509;
          break;
        }
      }
    }
    join_1510:;
    #line 726 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3arrS1512
    = _M0MP38JIA2JIA29moonbitdb3lib5Deque9to__array(_M0L5dequeS1511);
    moonbit_decref(_M0L5dequeS1511);
    #line 727 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3lenS1513 = _M0MPC15array5Array6lengthGsE(_M0L3arrS1512);
    if (_M0L5startS1515 < 0) {
      _M0L10start__idxS1514 = _M0L3lenS1513 + _M0L5startS1515;
    } else {
      _M0L10start__idxS1514 = _M0L5startS1515;
    }
    if (_M0L3endS1517 < 0) {
      _M0L8end__idxS1516 = _M0L3lenS1513 + _M0L3endS1517;
    } else {
      _M0L8end__idxS1516 = _M0L3endS1517;
    }
    _M0L6_2atmpS3360 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1518
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1518)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1518->$0 = _M0L6_2atmpS3360;
    _M0L6resultS1518->$1 = 0;
    _M0L1iS1519 = _M0L10start__idxS1514;
    while (1) {
      int32_t _if__result_3930;
      if (_M0L1iS1519 <= _M0L8end__idxS1516) {
        if (_M0L1iS1519 >= 0) {
          _if__result_3930 = _M0L1iS1519 < _M0L3lenS1513;
        } else {
          _if__result_3930 = 0;
        }
      } else {
        _if__result_3930 = 0;
      }
      if (_if__result_3930) {
        moonbit_string_t _M0L6_2atmpS3358;
        int32_t _M0L6_2atmpS3359;
        #line 732 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3358
        = _M0MPC15array5Array2atGsE(_M0L3arrS1512, _M0L1iS1519);
        #line 732 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1518, _M0L6_2atmpS3358);
        moonbit_decref(_M0L6_2atmpS3358);
        _M0L6_2atmpS3359 = _M0L1iS1519 + 1;
        _M0L1iS1519 = _M0L6_2atmpS3359;
        continue;
      } else {
        moonbit_decref(_M0L3arrS1512);
      }
      break;
    }
    return _M0L6resultS1518;
    join_1509:;
    _M0L6_2atmpS3357 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_3931
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3931)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_3931->$0 = _M0L6_2atmpS3357;
    _block_3931->$1 = 0;
    return _block_3931;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1498,
  moonbit_string_t _M0L3keyS1499
) {
  #line 709 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1498, _M0L3keyS1499)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1501;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3355 =
      _M0L4selfS1498->$0;
    void* _M0L7_2abindS1502;
    int32_t _result_3933;
    moonbit_incref(_M0L4dataS3355);
    #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1502
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3355, _M0L3keyS1499);
    moonbit_decref(_M0L4dataS3355);
    if (_M0L7_2abindS1502 == 0) {
      if (_M0L7_2abindS1502) {
        moonbit_decref(_M0L7_2abindS1502);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1503 = _M0L7_2abindS1502;
      void* _M0L4_2axS1504 = _M0L7_2aSomeS1503;
      switch (Moonbit_object_tag(_M0L4_2axS1504)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1505 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1504;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3480 =
            _M0L7_2aListS1505->$0;
          int32_t _M0L6_2acntS3843 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1505));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1506;
          if (_M0L6_2acntS3843 > 1) {
            int32_t _M0L11_2anew__cntS3844 = _M0L6_2acntS3843 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1505), _M0L11_2anew__cntS3844);
            moonbit_incref(_M0L8_2afieldS3480);
          } else if (_M0L6_2acntS3843 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1505);
          }
          _M0L4_2adS1506 = _M0L8_2afieldS3480;
          _M0L1dS1501 = _M0L4_2adS1506;
          goto join_1500;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1504);
          return 0;
          break;
        }
      }
    }
    join_1500:;
    #line 714 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3933 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L1dS1501);
    moonbit_decref(_M0L1dS1501);
    return _result_3933;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4rpop(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1487,
  moonbit_string_t _M0L3keyS1488
) {
  #line 694 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 695 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1487, _M0L3keyS1488)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1491;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3354 =
      _M0L4selfS1487->$0;
    void* _M0L7_2abindS1493;
    moonbit_string_t _M0L3valS1492;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3352;
    void* _M0L4ListS3353;
    moonbit_incref(_M0L4dataS3354);
    #line 698 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1493
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3354, _M0L3keyS1488);
    moonbit_decref(_M0L4dataS3354);
    if (_M0L7_2abindS1493 == 0) {
      if (_M0L7_2abindS1493) {
        moonbit_decref(_M0L7_2abindS1493);
      }
      goto join_1489;
    } else {
      void* _M0L7_2aSomeS1494 = _M0L7_2abindS1493;
      void* _M0L4_2axS1495 = _M0L7_2aSomeS1494;
      switch (Moonbit_object_tag(_M0L4_2axS1495)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1496 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1495;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3483 =
            _M0L7_2aListS1496->$0;
          int32_t _M0L6_2acntS3845 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1496));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2adequeS1497;
          if (_M0L6_2acntS3845 > 1) {
            int32_t _M0L11_2anew__cntS3846 = _M0L6_2acntS3845 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1496), _M0L11_2anew__cntS3846);
            moonbit_incref(_M0L8_2afieldS3483);
          } else if (_M0L6_2acntS3845 == 1) {
            #line 698 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1496);
          }
          _M0L8_2adequeS1497 = _M0L8_2afieldS3483;
          _M0L5dequeS1491 = _M0L8_2adequeS1497;
          goto join_1490;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1495);
          goto join_1489;
          break;
        }
      }
    }
    join_1490:;
    #line 700 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3valS1492
    = _M0MP38JIA2JIA29moonbitdb3lib5Deque9pop__back(_M0L5dequeS1491);
    _M0L4dataS3352 = _M0L4selfS1487->$0;
    _M0L4ListS3353
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3353)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS3353)->$0
    = _M0L5dequeS1491;
    moonbit_incref(_M0L4dataS3352);
    #line 701 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3352, _M0L3keyS1488, _M0L4ListS3353);
    moonbit_decref(_M0L4dataS3352);
    moonbit_decref(_M0L4ListS3353);
    return _M0L3valS1492;
    join_1489:;
    return 0;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4lpop(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1476,
  moonbit_string_t _M0L3keyS1477
) {
  #line 679 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 680 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1476, _M0L3keyS1477)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1480;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3351 =
      _M0L4selfS1476->$0;
    void* _M0L7_2abindS1482;
    moonbit_string_t _M0L3valS1481;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3349;
    void* _M0L4ListS3350;
    moonbit_incref(_M0L4dataS3351);
    #line 683 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1482
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3351, _M0L3keyS1477);
    moonbit_decref(_M0L4dataS3351);
    if (_M0L7_2abindS1482 == 0) {
      if (_M0L7_2abindS1482) {
        moonbit_decref(_M0L7_2abindS1482);
      }
      goto join_1478;
    } else {
      void* _M0L7_2aSomeS1483 = _M0L7_2abindS1482;
      void* _M0L4_2axS1484 = _M0L7_2aSomeS1483;
      switch (Moonbit_object_tag(_M0L4_2axS1484)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1485 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1484;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3486 =
            _M0L7_2aListS1485->$0;
          int32_t _M0L6_2acntS3847 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1485));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2adequeS1486;
          if (_M0L6_2acntS3847 > 1) {
            int32_t _M0L11_2anew__cntS3848 = _M0L6_2acntS3847 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1485), _M0L11_2anew__cntS3848);
            moonbit_incref(_M0L8_2afieldS3486);
          } else if (_M0L6_2acntS3847 == 1) {
            #line 683 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1485);
          }
          _M0L8_2adequeS1486 = _M0L8_2afieldS3486;
          _M0L5dequeS1480 = _M0L8_2adequeS1486;
          goto join_1479;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1484);
          goto join_1478;
          break;
        }
      }
    }
    join_1479:;
    #line 685 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3valS1481
    = _M0MP38JIA2JIA29moonbitdb3lib5Deque10pop__front(_M0L5dequeS1480);
    _M0L4dataS3349 = _M0L4selfS1476->$0;
    _M0L4ListS3350
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3350)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS3350)->$0
    = _M0L5dequeS1480;
    moonbit_incref(_M0L4dataS3349);
    #line 686 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3349, _M0L3keyS1477, _M0L4ListS3350);
    moonbit_decref(_M0L4dataS3349);
    moonbit_decref(_M0L4ListS3350);
    return _M0L3valS1481;
    join_1478:;
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1464,
  moonbit_string_t _M0L3keyS1465,
  moonbit_string_t _M0L5valueS1475
) {
  int32_t _M0L6_2atmpS3345;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1466;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1469;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3348;
  void* _M0L7_2abindS1470;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3346;
  void* _M0L4ListS3347;
  int32_t _result_3940;
  #line 668 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 669 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3345
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1464, _M0L3keyS1465);
  _M0L4dataS3348 = _M0L4selfS1464->$0;
  moonbit_incref(_M0L4dataS3348);
  #line 670 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1470
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3348, _M0L3keyS1465);
  moonbit_decref(_M0L4dataS3348);
  if (_M0L7_2abindS1470 == 0) {
    if (_M0L7_2abindS1470) {
      moonbit_decref(_M0L7_2abindS1470);
    }
    goto join_1467;
  } else {
    void* _M0L7_2aSomeS1471 = _M0L7_2abindS1470;
    void* _M0L4_2axS1472 = _M0L7_2aSomeS1471;
    switch (Moonbit_object_tag(_M0L4_2axS1472)) {
      case 2: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1473 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1472;
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3489 =
          _M0L7_2aListS1473->$0;
        int32_t _M0L6_2acntS3849 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1473));
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1474;
        if (_M0L6_2acntS3849 > 1) {
          int32_t _M0L11_2anew__cntS3850 = _M0L6_2acntS3849 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1473), _M0L11_2anew__cntS3850);
          moonbit_incref(_M0L8_2afieldS3489);
        } else if (_M0L6_2acntS3849 == 1) {
          #line 670 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aListS1473);
        }
        _M0L4_2adS1474 = _M0L8_2afieldS3489;
        _M0L1dS1469 = _M0L4_2adS1474;
        goto join_1468;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1472);
        goto join_1467;
        break;
      }
    }
  }
  goto joinlet_3939;
  join_1468:;
  _M0L5dequeS1466 = _M0L1dS1469;
  joinlet_3939:;
  goto joinlet_3938;
  join_1467:;
  #line 672 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5dequeS1466 = _M0MP38JIA2JIA29moonbitdb3lib5Deque3new();
  joinlet_3938:;
  #line 674 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib5Deque10push__back(_M0L5dequeS1466, _M0L5valueS1475);
  _M0L4dataS3346 = _M0L4selfS1464->$0;
  moonbit_incref(_M0L5dequeS1466);
  _M0L4ListS3347
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
  Moonbit_object_header(_M0L4ListS3347)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS3347)->$0
  = _M0L5dequeS1466;
  moonbit_incref(_M0L4dataS3346);
  #line 675 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3346, _M0L3keyS1465, _M0L4ListS3347);
  moonbit_decref(_M0L4dataS3346);
  moonbit_decref(_M0L4ListS3347);
  #line 676 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3940 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L5dequeS1466);
  moonbit_decref(_M0L5dequeS1466);
  return _result_3940;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hlen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1455,
  moonbit_string_t _M0L3keyS1456
) {
  #line 646 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 647 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1455, _M0L3keyS1456)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1458;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3344 =
      _M0L4selfS1455->$0;
    void* _M0L7_2abindS1459;
    int32_t _result_3942;
    moonbit_incref(_M0L4dataS3344);
    #line 650 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1459
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3344, _M0L3keyS1456);
    moonbit_decref(_M0L4dataS3344);
    if (_M0L7_2abindS1459 == 0) {
      if (_M0L7_2abindS1459) {
        moonbit_decref(_M0L7_2abindS1459);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1460 = _M0L7_2abindS1459;
      void* _M0L4_2axS1461 = _M0L7_2aSomeS1460;
      switch (Moonbit_object_tag(_M0L4_2axS1461)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1462 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1461;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3491 = _M0L7_2aHashS1462->$0;
          int32_t _M0L6_2acntS3851 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1462));
          struct _M0TPB3MapGssE* _M0L4_2ahS1463;
          if (_M0L6_2acntS3851 > 1) {
            int32_t _M0L11_2anew__cntS3852 = _M0L6_2acntS3851 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1462), _M0L11_2anew__cntS3852);
            moonbit_incref(_M0L8_2afieldS3491);
          } else if (_M0L6_2acntS3851 == 1) {
            #line 650 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1462);
          }
          _M0L4_2ahS1463 = _M0L8_2afieldS3491;
          _M0L1hS1458 = _M0L4_2ahS1463;
          goto join_1457;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1461);
          return 0;
          break;
        }
      }
    }
    join_1457:;
    #line 651 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3942 = _M0MPB3Map6lengthGssE(_M0L1hS1458);
    moonbit_decref(_M0L1hS1458);
    return _result_3942;
  }
}

struct _M0TPB3MapGssE* _M0MP38JIA2JIA29moonbitdb3lib8Database7hgetall(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1443,
  moonbit_string_t _M0L3keyS1444
) {
  #line 635 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 636 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1443, _M0L3keyS1444)
  ) {
    struct _M0TUssE** _M0L7_2abindS1445 =
      (struct _M0TUssE**)moonbit_empty_ref_array;
    struct _M0TUssE** _M0L6_2atmpS3340 = _M0L7_2abindS1445;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3339 =
      (struct _M0TPB9ArrayViewGUssEE){.$0 = _M0L6_2atmpS3340,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGssE* _result_3943;
    #line 637 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3943 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3339, 0ll);
    moonbit_decref(_M0L6_2atmpS3339.$0);
    return _result_3943;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1449;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3343 =
      _M0L4selfS1443->$0;
    void* _M0L7_2abindS1450;
    struct _M0TUssE** _M0L7_2abindS1447;
    struct _M0TUssE** _M0L6_2atmpS3342;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3341;
    struct _M0TPB3MapGssE* _result_3946;
    moonbit_incref(_M0L4dataS3343);
    #line 639 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1450
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3343, _M0L3keyS1444);
    moonbit_decref(_M0L4dataS3343);
    if (_M0L7_2abindS1450 == 0) {
      if (_M0L7_2abindS1450) {
        moonbit_decref(_M0L7_2abindS1450);
      }
      goto join_1446;
    } else {
      void* _M0L7_2aSomeS1451 = _M0L7_2abindS1450;
      void* _M0L4_2axS1452 = _M0L7_2aSomeS1451;
      switch (Moonbit_object_tag(_M0L4_2axS1452)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1453 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1452;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3493 = _M0L7_2aHashS1453->$0;
          int32_t _M0L6_2acntS3853 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1453));
          struct _M0TPB3MapGssE* _M0L4_2ahS1454;
          if (_M0L6_2acntS3853 > 1) {
            int32_t _M0L11_2anew__cntS3854 = _M0L6_2acntS3853 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1453), _M0L11_2anew__cntS3854);
            moonbit_incref(_M0L8_2afieldS3493);
          } else if (_M0L6_2acntS3853 == 1) {
            #line 639 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1453);
          }
          _M0L4_2ahS1454 = _M0L8_2afieldS3493;
          _M0L1hS1449 = _M0L4_2ahS1454;
          goto join_1448;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1452);
          goto join_1446;
          break;
        }
      }
    }
    join_1448:;
    return _M0L1hS1449;
    join_1446:;
    _M0L7_2abindS1447 = (struct _M0TUssE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3342 = _M0L7_2abindS1447;
    _M0L6_2atmpS3341
    = (struct _M0TPB9ArrayViewGUssEE){
      .$0 = _M0L6_2atmpS3342, .$1 = 0, .$2 = 0
    };
    #line 641 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3946 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3341, 0ll);
    moonbit_decref(_M0L6_2atmpS3341.$0);
    return _result_3946;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1432,
  moonbit_string_t _M0L3keyS1433,
  moonbit_string_t _M0L5fieldS1437
) {
  #line 606 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1432, _M0L3keyS1433)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1436;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3338 =
      _M0L4selfS1432->$0;
    void* _M0L7_2abindS1438;
    moonbit_string_t _result_3949;
    moonbit_incref(_M0L4dataS3338);
    #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1438
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3338, _M0L3keyS1433);
    moonbit_decref(_M0L4dataS3338);
    if (_M0L7_2abindS1438 == 0) {
      if (_M0L7_2abindS1438) {
        moonbit_decref(_M0L7_2abindS1438);
      }
      goto join_1434;
    } else {
      void* _M0L7_2aSomeS1439 = _M0L7_2abindS1438;
      void* _M0L4_2axS1440 = _M0L7_2aSomeS1439;
      switch (Moonbit_object_tag(_M0L4_2axS1440)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1441 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1440;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3495 = _M0L7_2aHashS1441->$0;
          int32_t _M0L6_2acntS3855 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1441));
          struct _M0TPB3MapGssE* _M0L4_2ahS1442;
          if (_M0L6_2acntS3855 > 1) {
            int32_t _M0L11_2anew__cntS3856 = _M0L6_2acntS3855 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1441), _M0L11_2anew__cntS3856);
            moonbit_incref(_M0L8_2afieldS3495);
          } else if (_M0L6_2acntS3855 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1441);
          }
          _M0L4_2ahS1442 = _M0L8_2afieldS3495;
          _M0L1hS1436 = _M0L4_2ahS1442;
          goto join_1435;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1440);
          goto join_1434;
          break;
        }
      }
    }
    join_1435:;
    #line 611 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3949 = _M0MPB3Map3getGssE(_M0L1hS1436, _M0L5fieldS1437);
    moonbit_decref(_M0L1hS1436);
    return _result_3949;
    join_1434:;
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1418,
  moonbit_string_t _M0L3keyS1419,
  moonbit_string_t _M0L5fieldS1430,
  moonbit_string_t _M0L5valueS1431
) {
  int32_t _M0L6_2atmpS3332;
  struct _M0TPB3MapGssE* _M0L4hashS1420;
  struct _M0TPB3MapGssE* _M0L1hS1424;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3337;
  void* _M0L7_2abindS1425;
  struct _M0TUssE** _M0L7_2abindS1422;
  struct _M0TUssE** _M0L6_2atmpS3336;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3335;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3333;
  void* _M0L4HashS3334;
  #line 596 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3332
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1418, _M0L3keyS1419);
  _M0L4dataS3337 = _M0L4selfS1418->$0;
  moonbit_incref(_M0L4dataS3337);
  #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1425
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3337, _M0L3keyS1419);
  moonbit_decref(_M0L4dataS3337);
  if (_M0L7_2abindS1425 == 0) {
    if (_M0L7_2abindS1425) {
      moonbit_decref(_M0L7_2abindS1425);
    }
    goto join_1421;
  } else {
    void* _M0L7_2aSomeS1426 = _M0L7_2abindS1425;
    void* _M0L4_2axS1427 = _M0L7_2aSomeS1426;
    switch (Moonbit_object_tag(_M0L4_2axS1427)) {
      case 1: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1428 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1427;
        struct _M0TPB3MapGssE* _M0L8_2afieldS3498 = _M0L7_2aHashS1428->$0;
        int32_t _M0L6_2acntS3857 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1428));
        struct _M0TPB3MapGssE* _M0L4_2ahS1429;
        if (_M0L6_2acntS3857 > 1) {
          int32_t _M0L11_2anew__cntS3858 = _M0L6_2acntS3857 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1428), _M0L11_2anew__cntS3858);
          moonbit_incref(_M0L8_2afieldS3498);
        } else if (_M0L6_2acntS3857 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aHashS1428);
        }
        _M0L4_2ahS1429 = _M0L8_2afieldS3498;
        _M0L1hS1424 = _M0L4_2ahS1429;
        goto join_1423;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1427);
        goto join_1421;
        break;
      }
    }
  }
  goto joinlet_3951;
  join_1423:;
  _M0L4hashS1420 = _M0L1hS1424;
  joinlet_3951:;
  goto joinlet_3950;
  join_1421:;
  _M0L7_2abindS1422 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3336 = _M0L7_2abindS1422;
  _M0L6_2atmpS3335
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS3336, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4hashS1420 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3335, 10ll);
  moonbit_decref(_M0L6_2atmpS3335.$0);
  joinlet_3950:;
  #line 602 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1420, _M0L5fieldS1430, _M0L5valueS1431);
  _M0L4dataS3333 = _M0L4selfS1418->$0;
  _M0L4HashS3334
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS3334)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 18, 1);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4HashS3334)->$0
  = _M0L4hashS1420;
  moonbit_incref(_M0L4dataS3333);
  #line 603 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3333, _M0L3keyS1419, _M0L4HashS3334);
  moonbit_decref(_M0L4dataS3333);
  moonbit_decref(_M0L4HashS3334);
  return 0;
}

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database4decr(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1402,
  moonbit_string_t _M0L3keyS1403
) {
  int32_t _M0L6_2atmpS3325;
  moonbit_string_t _M0L1sS1406;
  moonbit_string_t _M0L7currentS1404;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3331;
  void* _M0L7_2abindS1407;
  int32_t _M0L1nS1413;
  int64_t _M0L7_2abindS1415;
  int32_t _M0L6_2atmpS3330;
  moonbit_string_t _M0L8new__valS1414;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3326;
  void* _M0L6StringS3327;
  struct _M0TPB3MapGsiE* _M0L7expiresS3328;
  int32_t _M0L6_2atmpS3329;
  #line 579 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 580 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3325
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1402, _M0L3keyS1403);
  _M0L4dataS3331 = _M0L4selfS1402->$0;
  moonbit_incref(_M0L4dataS3331);
  #line 581 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1407
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3331, _M0L3keyS1403);
  moonbit_decref(_M0L4dataS3331);
  if (_M0L7_2abindS1407 == 0) {
    if (_M0L7_2abindS1407) {
      moonbit_decref(_M0L7_2abindS1407);
    }
    _M0L7currentS1404 = (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    void* _M0L7_2aSomeS1408 = _M0L7_2abindS1407;
    void* _M0L4_2axS1409 = _M0L7_2aSomeS1408;
    switch (Moonbit_object_tag(_M0L4_2axS1409)) {
      case 0: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1410 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1409;
        moonbit_string_t _M0L8_2afieldS3502 = _M0L9_2aStringS1410->$0;
        int32_t _M0L6_2acntS3859 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1410));
        moonbit_string_t _M0L4_2asS1411;
        if (_M0L6_2acntS3859 > 1) {
          int32_t _M0L11_2anew__cntS3860 = _M0L6_2acntS3859 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1410), _M0L11_2anew__cntS3860);
          moonbit_incref(_M0L8_2afieldS3502);
        } else if (_M0L6_2acntS3859 == 1) {
          #line 581 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L9_2aStringS1410);
        }
        _M0L4_2asS1411 = _M0L8_2afieldS3502;
        _M0L1sS1406 = _M0L4_2asS1411;
        goto join_1405;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1409);
        _M0L7currentS1404 = (moonbit_string_t)moonbit_string_literal_84.data;
        break;
      }
    }
  }
  goto joinlet_3952;
  join_1405:;
  _M0L7currentS1404 = _M0L1sS1406;
  joinlet_3952:;
  #line 585 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1415
  = _M0FP38JIA2JIA29moonbitdb3lib10parse__int(_M0L7currentS1404);
  moonbit_decref(_M0L7currentS1404);
  if (_M0L7_2abindS1415 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1416 = _M0L7_2abindS1415;
    int32_t _M0L4_2anS1417 = (int32_t)_M0L7_2aSomeS1416;
    _M0L1nS1413 = _M0L4_2anS1417;
    goto join_1412;
  }
  join_1412:;
  _M0L6_2atmpS3330 = _M0L1nS1413 - 1;
  #line 587 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L8new__valS1414
  = _M0FP38JIA2JIA29moonbitdb3lib15int__to__string(_M0L6_2atmpS3330);
  _M0L4dataS3326 = _M0L4selfS1402->$0;
  _M0L6StringS3327
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3327)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3327)->$0
  = _M0L8new__valS1414;
  moonbit_incref(_M0L4dataS3326);
  #line 588 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3326, _M0L3keyS1403, _M0L6StringS3327);
  moonbit_decref(_M0L4dataS3326);
  moonbit_decref(_M0L6StringS3327);
  _M0L7expiresS3328 = _M0L4selfS1402->$1;
  moonbit_incref(_M0L7expiresS3328);
  #line 589 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3328, _M0L3keyS1403);
  moonbit_decref(_M0L7expiresS3328);
  _M0L6_2atmpS3329 = _M0L1nS1413 - 1;
  return (int64_t)_M0L6_2atmpS3329;
}

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database4incr(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1386,
  moonbit_string_t _M0L3keyS1387
) {
  int32_t _M0L6_2atmpS3318;
  moonbit_string_t _M0L1sS1390;
  moonbit_string_t _M0L7currentS1388;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3324;
  void* _M0L7_2abindS1391;
  int32_t _M0L1nS1397;
  int64_t _M0L7_2abindS1399;
  int32_t _M0L6_2atmpS3323;
  moonbit_string_t _M0L8new__valS1398;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3319;
  void* _M0L6StringS3320;
  struct _M0TPB3MapGsiE* _M0L7expiresS3321;
  int32_t _M0L6_2atmpS3322;
  #line 562 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 563 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3318
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1386, _M0L3keyS1387);
  _M0L4dataS3324 = _M0L4selfS1386->$0;
  moonbit_incref(_M0L4dataS3324);
  #line 564 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1391
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3324, _M0L3keyS1387);
  moonbit_decref(_M0L4dataS3324);
  if (_M0L7_2abindS1391 == 0) {
    if (_M0L7_2abindS1391) {
      moonbit_decref(_M0L7_2abindS1391);
    }
    _M0L7currentS1388 = (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    void* _M0L7_2aSomeS1392 = _M0L7_2abindS1391;
    void* _M0L4_2axS1393 = _M0L7_2aSomeS1392;
    switch (Moonbit_object_tag(_M0L4_2axS1393)) {
      case 0: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1394 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1393;
        moonbit_string_t _M0L8_2afieldS3506 = _M0L9_2aStringS1394->$0;
        int32_t _M0L6_2acntS3861 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1394));
        moonbit_string_t _M0L4_2asS1395;
        if (_M0L6_2acntS3861 > 1) {
          int32_t _M0L11_2anew__cntS3862 = _M0L6_2acntS3861 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1394), _M0L11_2anew__cntS3862);
          moonbit_incref(_M0L8_2afieldS3506);
        } else if (_M0L6_2acntS3861 == 1) {
          #line 564 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L9_2aStringS1394);
        }
        _M0L4_2asS1395 = _M0L8_2afieldS3506;
        _M0L1sS1390 = _M0L4_2asS1395;
        goto join_1389;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1393);
        _M0L7currentS1388 = (moonbit_string_t)moonbit_string_literal_84.data;
        break;
      }
    }
  }
  goto joinlet_3954;
  join_1389:;
  _M0L7currentS1388 = _M0L1sS1390;
  joinlet_3954:;
  #line 568 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1399
  = _M0FP38JIA2JIA29moonbitdb3lib10parse__int(_M0L7currentS1388);
  moonbit_decref(_M0L7currentS1388);
  if (_M0L7_2abindS1399 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1400 = _M0L7_2abindS1399;
    int32_t _M0L4_2anS1401 = (int32_t)_M0L7_2aSomeS1400;
    _M0L1nS1397 = _M0L4_2anS1401;
    goto join_1396;
  }
  join_1396:;
  _M0L6_2atmpS3323 = _M0L1nS1397 + 1;
  #line 570 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L8new__valS1398
  = _M0FP38JIA2JIA29moonbitdb3lib15int__to__string(_M0L6_2atmpS3323);
  _M0L4dataS3319 = _M0L4selfS1386->$0;
  _M0L6StringS3320
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3320)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3320)->$0
  = _M0L8new__valS1398;
  moonbit_incref(_M0L4dataS3319);
  #line 571 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3319, _M0L3keyS1387, _M0L6StringS3320);
  moonbit_decref(_M0L4dataS3319);
  moonbit_decref(_M0L6StringS3320);
  _M0L7expiresS3321 = _M0L4selfS1386->$1;
  moonbit_incref(_M0L7expiresS3321);
  #line 572 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3321, _M0L3keyS1387);
  moonbit_decref(_M0L7expiresS3321);
  _M0L6_2atmpS3322 = _M0L1nS1397 + 1;
  return (int64_t)_M0L6_2atmpS3322;
}

moonbit_string_t _M0FP38JIA2JIA29moonbitdb3lib15int__to__string(
  int32_t _M0L1nS1377
) {
  #line 526 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (_M0L1nS1377 == 0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L3numS1378 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    moonbit_string_t* _M0L6_2atmpS3317;
    struct _M0TPB5ArrayGsE* _M0L5charsS1379;
    int32_t _M0L3valS3303;
    moonbit_string_t* _M0L6_2atmpS3316;
    struct _M0TPB5ArrayGsE* _M0L6resultS1382;
    int32_t _M0L6_2atmpS3313;
    int32_t _M0L6_2atmpS3312;
    int32_t _M0L1iS1383;
    moonbit_string_t _M0L7_2abindS1385;
    int32_t _M0L6_2atmpS3315;
    struct _M0TPC16string10StringView _M0L6_2atmpS3314;
    moonbit_string_t _result_3958;
    Moonbit_object_header(_M0L3numS1378)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L3numS1378->$0 = _M0L1nS1377;
    _M0L6_2atmpS3317 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L5charsS1379
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L5charsS1379)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L5charsS1379->$0 = _M0L6_2atmpS3317;
    _M0L5charsS1379->$1 = 0;
    _M0L3valS3303 = _M0L3numS1378->$0;
    if (_M0L3valS3303 < 0) {
      int32_t _M0L3valS3305 = _M0L3numS1378->$0;
      int32_t _M0L6_2atmpS3304 = -_M0L3valS3305;
      _M0L3numS1378->$0 = _M0L6_2atmpS3304;
    }
    while (1) {
      int32_t _M0L3valS3306 = _M0L3numS1378->$0;
      if (_M0L3valS3306 > 0) {
        int32_t _M0L3valS3307 = _M0L3numS1378->$0;
        int32_t _M0L7_2abindS1380 = _M0L3valS3307 % 10;
        int32_t _M0L3valS3309;
        int32_t _M0L6_2atmpS3308;
        switch (_M0L7_2abindS1380) {
          case 0: {
            #line 537 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_84.data);
            break;
          }
          
          case 1: {
            #line 538 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_85.data);
            break;
          }
          
          case 2: {
            #line 539 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_86.data);
            break;
          }
          
          case 3: {
            #line 540 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_87.data);
            break;
          }
          
          case 4: {
            #line 541 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_88.data);
            break;
          }
          
          case 5: {
            #line 542 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_89.data);
            break;
          }
          
          case 6: {
            #line 543 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_90.data);
            break;
          }
          
          case 7: {
            #line 544 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_91.data);
            break;
          }
          
          case 8: {
            #line 545 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_92.data);
            break;
          }
          
          case 9: {
            #line 546 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_93.data);
            break;
          }
          default: {
            #line 547 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_84.data);
            break;
          }
        }
        _M0L3valS3309 = _M0L3numS1378->$0;
        _M0L6_2atmpS3308 = _M0L3valS3309 / 10;
        _M0L3numS1378->$0 = _M0L6_2atmpS3308;
        continue;
      } else {
        moonbit_decref(_M0L3numS1378);
      }
      break;
    }
    if (_M0L1nS1377 < 0) {
      #line 552 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5charsS1379, (moonbit_string_t)moonbit_string_literal_94.data);
    }
    _M0L6_2atmpS3316 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1382
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1382)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1382->$0 = _M0L6_2atmpS3316;
    _M0L6resultS1382->$1 = 0;
    #line 555 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3313 = _M0MPC15array5Array6lengthGsE(_M0L5charsS1379);
    _M0L6_2atmpS3312 = _M0L6_2atmpS3313 - 1;
    _M0L1iS1383 = _M0L6_2atmpS3312;
    while (1) {
      if (_M0L1iS1383 >= 0) {
        moonbit_string_t _M0L6_2atmpS3310;
        int32_t _M0L6_2atmpS3311;
        #line 556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3310
        = _M0MPC15array5Array2atGsE(_M0L5charsS1379, _M0L1iS1383);
        #line 556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1382, _M0L6_2atmpS3310);
        moonbit_decref(_M0L6_2atmpS3310);
        _M0L6_2atmpS3311 = _M0L1iS1383 - 1;
        _M0L1iS1383 = _M0L6_2atmpS3311;
        continue;
      } else {
        moonbit_decref(_M0L5charsS1379);
      }
      break;
    }
    _M0L7_2abindS1385 = (moonbit_string_t)moonbit_string_literal_95.data;
    _M0L6_2atmpS3315 = Moonbit_array_length(_M0L7_2abindS1385);
    _M0L6_2atmpS3314
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS1385, .$1 = 0, .$2 = _M0L6_2atmpS3315
    };
    #line 558 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3958
    = _M0MPC15array5Array4joinGsE(_M0L6resultS1382, _M0L6_2atmpS3314);
    moonbit_decref(_M0L6resultS1382);
    moonbit_decref(_M0L6_2atmpS3314.$0);
    return _result_3958;
  }
}

int64_t _M0FP38JIA2JIA29moonbitdb3lib10parse__int(
  moonbit_string_t _M0L1sS1366
) {
  int32_t _M0L6_2atmpS3290;
  #line 503 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3290 = Moonbit_array_length(_M0L1sS1366);
  if (_M0L6_2atmpS3290 == 0) {
    return 4294967296ll;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L6resultS1367 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    struct _M0TPB8MutLocalGiE* _M0L4signS1368;
    struct _M0TPB8MutLocalGiE* _M0L5startS1369;
    int32_t _M0L6_2atmpS3291;
    int32_t _M0L3valS3299;
    int32_t _M0L1iS1370;
    int32_t _M0L3valS3301;
    int32_t _M0L3valS3302;
    int32_t _M0L6_2atmpS3300;
    Moonbit_object_header(_M0L6resultS1367)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L6resultS1367->$0 = 0;
    _M0L4signS1368
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L4signS1368)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L4signS1368->$0 = 1;
    _M0L5startS1369
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L5startS1369)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L5startS1369->$0 = 0;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1366)) {
      #line 510 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3291 = _M0L1sS1366[0];
    if (_M0L6_2atmpS3291 == 45) {
      _M0L4signS1368->$0 = -1;
      _M0L5startS1369->$0 = 1;
    } else {
      int32_t _M0L6_2atmpS3292;
      if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1366)) {
        #line 513 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3292 = _M0L1sS1366[0];
      if (_M0L6_2atmpS3292 == 43) {
        _M0L5startS1369->$0 = 1;
      }
    }
    _M0L3valS3299 = _M0L5startS1369->$0;
    moonbit_decref(_M0L5startS1369);
    _M0L1iS1370 = _M0L3valS3299;
    while (1) {
      int32_t _M0L6_2atmpS3293 = Moonbit_array_length(_M0L1sS1366);
      if (_M0L1iS1370 < _M0L6_2atmpS3293) {
        int32_t _M0L5digitS1372;
        int32_t _M0L6_2atmpS3297;
        int64_t _M0L7_2abindS1373;
        int32_t _M0L3valS3296;
        int32_t _M0L6_2atmpS3295;
        int32_t _M0L6_2atmpS3294;
        int32_t _M0L6_2atmpS3298;
        if (
          _M0L1iS1370 < 0 || _M0L1iS1370 >= Moonbit_array_length(_M0L1sS1366)
        ) {
          #line 517 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3297 = _M0L1sS1366[_M0L1iS1370];
        #line 517 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1373
        = _M0FP38JIA2JIA29moonbitdb3lib17uint16__to__digit(_M0L6_2atmpS3297);
        if (_M0L7_2abindS1373 == 4294967296ll) {
          moonbit_decref(_M0L4signS1368);
          moonbit_decref(_M0L6resultS1367);
          return 4294967296ll;
        } else {
          int64_t _M0L7_2aSomeS1374 = _M0L7_2abindS1373;
          int32_t _M0L8_2adigitS1375 = (int32_t)_M0L7_2aSomeS1374;
          _M0L5digitS1372 = _M0L8_2adigitS1375;
          goto join_1371;
        }
        goto joinlet_3960;
        join_1371:;
        _M0L3valS3296 = _M0L6resultS1367->$0;
        _M0L6_2atmpS3295 = _M0L3valS3296 * 10;
        _M0L6_2atmpS3294 = _M0L6_2atmpS3295 + _M0L5digitS1372;
        _M0L6resultS1367->$0 = _M0L6_2atmpS3294;
        joinlet_3960:;
        _M0L6_2atmpS3298 = _M0L1iS1370 + 1;
        _M0L1iS1370 = _M0L6_2atmpS3298;
        continue;
      }
      break;
    }
    _M0L3valS3301 = _M0L6resultS1367->$0;
    moonbit_decref(_M0L6resultS1367);
    _M0L3valS3302 = _M0L4signS1368->$0;
    moonbit_decref(_M0L4signS1368);
    _M0L6_2atmpS3300 = _M0L3valS3301 * _M0L3valS3302;
    return (int64_t)_M0L6_2atmpS3300;
  }
}

int64_t _M0FP38JIA2JIA29moonbitdb3lib17uint16__to__digit(int32_t _M0L1cS1365) {
  #line 471 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  switch (_M0L1cS1365) {
    case 48: {
      return 0ll;
      break;
    }
    
    case 49: {
      return 1ll;
      break;
    }
    
    case 50: {
      return 2ll;
      break;
    }
    
    case 51: {
      return 3ll;
      break;
    }
    
    case 52: {
      return 4ll;
      break;
    }
    
    case 53: {
      return 5ll;
      break;
    }
    
    case 54: {
      return 6ll;
      break;
    }
    
    case 55: {
      return 7ll;
      break;
    }
    
    case 56: {
      return 8ll;
      break;
    }
    
    case 57: {
      return 9ll;
      break;
    }
    default: {
      return 4294967296ll;
      break;
    }
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6strlen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1356,
  moonbit_string_t _M0L3keyS1357
) {
  #line 460 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 461 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1356, _M0L3keyS1357)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1359;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3289 =
      _M0L4selfS1356->$0;
    void* _M0L7_2abindS1360;
    int32_t _result_3962;
    moonbit_incref(_M0L4dataS3289);
    #line 464 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1360
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3289, _M0L3keyS1357);
    moonbit_decref(_M0L4dataS3289);
    if (_M0L7_2abindS1360 == 0) {
      if (_M0L7_2abindS1360) {
        moonbit_decref(_M0L7_2abindS1360);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1361 = _M0L7_2abindS1360;
      void* _M0L4_2axS1362 = _M0L7_2aSomeS1361;
      switch (Moonbit_object_tag(_M0L4_2axS1362)) {
        case 0: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1363 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1362;
          moonbit_string_t _M0L8_2afieldS3508 = _M0L9_2aStringS1363->$0;
          int32_t _M0L6_2acntS3863 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1363));
          moonbit_string_t _M0L4_2asS1364;
          if (_M0L6_2acntS3863 > 1) {
            int32_t _M0L11_2anew__cntS3864 = _M0L6_2acntS3863 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1363), _M0L11_2anew__cntS3864);
            moonbit_incref(_M0L8_2afieldS3508);
          } else if (_M0L6_2acntS3863 == 1) {
            #line 464 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1363);
          }
          _M0L4_2asS1364 = _M0L8_2afieldS3508;
          _M0L1sS1359 = _M0L4_2asS1364;
          goto join_1358;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1362);
          return 0;
          break;
        }
      }
    }
    join_1358:;
    _result_3962 = Moonbit_array_length(_M0L1sS1359);
    moonbit_decref(_M0L1sS1359);
    return _result_3962;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database4keys(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1348
) {
  moonbit_string_t* _M0L6_2atmpS3288;
  struct _M0TPB5ArrayGsE* _M0L6resultS1346;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3287;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L5_2aitS1347;
  #line 377 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3288 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1346
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1346)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1346->$0 = _M0L6_2atmpS3288;
  _M0L6resultS1346->$1 = 0;
  _M0L4dataS3287 = _M0L4selfS1348->$0;
  moonbit_incref(_M0L4dataS3287);
  #line 378 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1347
  = _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3287);
  moonbit_decref(_M0L4dataS3287);
  while (1) {
    moonbit_string_t _M0L3keyS1350;
    struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1352;
    int32_t _M0L6_2atmpS3286;
    #line 379 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1352
    = _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L5_2aitS1347);
    if (_M0L7_2abindS1352 == 0) {
      if (_M0L7_2abindS1352) {
        moonbit_decref(_M0L7_2abindS1352);
      }
      moonbit_decref(_M0L5_2aitS1347);
    } else {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1353 =
        _M0L7_2abindS1352;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1354 =
        _M0L7_2aSomeS1353;
      moonbit_string_t _M0L8_2afieldS3510 = _M0L4_2axS1354->$0;
      int32_t _M0L6_2acntS3865 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1354));
      moonbit_string_t _M0L6_2akeyS1355;
      if (_M0L6_2acntS3865 > 1) {
        int32_t _M0L11_2anew__cntS3867 = _M0L6_2acntS3865 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1354), _M0L11_2anew__cntS3867);
        moonbit_incref(_M0L8_2afieldS3510);
      } else if (_M0L6_2acntS3865 == 1) {
        void* _M0L8_2afieldS3866 = _M0L4_2axS1354->$1;
        moonbit_decref(_M0L8_2afieldS3866);
        #line 379 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1354);
      }
      _M0L6_2akeyS1355 = _M0L8_2afieldS3510;
      _M0L3keyS1350 = _M0L6_2akeyS1355;
      goto join_1349;
    }
    goto joinlet_3964;
    join_1349:;
    #line 380 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3286
    = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1348, _M0L3keyS1350);
    if (!_M0L6_2atmpS3286) {
      #line 381 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1346, _M0L3keyS1350);
      moonbit_decref(_M0L3keyS1350);
    } else {
      moonbit_decref(_M0L3keyS1350);
    }
    continue;
    joinlet_3964:;
    break;
  }
  return _M0L6resultS1346;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6exists(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1344,
  moonbit_string_t _M0L3keyS1345
) {
  #line 369 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 370 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1344, _M0L3keyS1345)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3285 =
      _M0L4selfS1344->$0;
    int32_t _result_3965;
    moonbit_incref(_M0L4dataS3285);
    #line 373 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3965
    = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3285, _M0L3keyS1345);
    moonbit_decref(_M0L4dataS3285);
    return _result_3965;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database3get(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1334,
  moonbit_string_t _M0L3keyS1335
) {
  #line 345 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 346 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1334, _M0L3keyS1335)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1338;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3284 =
      _M0L4selfS1334->$0;
    void* _M0L7_2abindS1339;
    moonbit_incref(_M0L4dataS3284);
    #line 349 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1339
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3284, _M0L3keyS1335);
    moonbit_decref(_M0L4dataS3284);
    if (_M0L7_2abindS1339 == 0) {
      if (_M0L7_2abindS1339) {
        moonbit_decref(_M0L7_2abindS1339);
      }
      goto join_1336;
    } else {
      void* _M0L7_2aSomeS1340 = _M0L7_2abindS1339;
      void* _M0L4_2axS1341 = _M0L7_2aSomeS1340;
      switch (Moonbit_object_tag(_M0L4_2axS1341)) {
        case 0: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1342 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1341;
          moonbit_string_t _M0L8_2afieldS3513 = _M0L9_2aStringS1342->$0;
          int32_t _M0L6_2acntS3868 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1342));
          moonbit_string_t _M0L4_2asS1343;
          if (_M0L6_2acntS3868 > 1) {
            int32_t _M0L11_2anew__cntS3869 = _M0L6_2acntS3868 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1342), _M0L11_2anew__cntS3869);
            moonbit_incref(_M0L8_2afieldS3513);
          } else if (_M0L6_2acntS3868 == 1) {
            #line 349 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1342);
          }
          _M0L4_2asS1343 = _M0L8_2afieldS3513;
          _M0L1sS1338 = _M0L4_2asS1343;
          goto join_1337;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1341);
          goto join_1336;
          break;
        }
      }
    }
    join_1337:;
    return _M0L1sS1338;
    join_1336:;
    return 0;
  }
}

struct _M0TPB5ArrayGOsE* _M0MP38JIA2JIA29moonbitdb3lib8Database4mget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1324,
  struct _M0TPB5ArrayGsE* _M0L4keysS1321
) {
  moonbit_string_t* _M0L6_2atmpS3283;
  struct _M0TPB5ArrayGOsE* _M0L6resultS1319;
  int32_t _M0L7_2abindS1320;
  int32_t _M0L2__S1322;
  #line 276 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3283 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1319
  = (struct _M0TPB5ArrayGOsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGOsE));
  Moonbit_object_header(_M0L6resultS1319)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 24, 0);
  _M0L6resultS1319->$0 = _M0L6_2atmpS3283;
  _M0L6resultS1319->$1 = 0;
  _M0L7_2abindS1320 = _M0L4keysS1321->$1;
  _M0L2__S1322 = 0;
  while (1) {
    if (_M0L2__S1322 < _M0L7_2abindS1320) {
      moonbit_string_t* _M0L3bufS3282 = _M0L4keysS1321->$0;
      moonbit_string_t _M0L3keyS1323 =
        (moonbit_string_t)_M0L3bufS3282[_M0L2__S1322];
      int32_t _M0L6_2atmpS3281;
      moonbit_incref(_M0L3keyS1323);
      #line 279 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      if (
        _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1324, _M0L3keyS1323)
      ) {
        moonbit_string_t _M0L6_2atmpS3277;
        moonbit_decref(_M0L3keyS1323);
        _M0L6_2atmpS3277 = 0;
        #line 280 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1319, _M0L6_2atmpS3277);
        if (_M0L6_2atmpS3277) {
          moonbit_decref(_M0L6_2atmpS3277);
        }
      } else {
        moonbit_string_t _M0L1sS1327;
        struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3280 =
          _M0L4selfS1324->$0;
        void* _M0L7_2abindS1328;
        moonbit_string_t _M0L6_2atmpS3279;
        moonbit_string_t _M0L6_2atmpS3278;
        moonbit_incref(_M0L4dataS3280);
        #line 282 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1328
        = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3280, _M0L3keyS1323);
        moonbit_decref(_M0L4dataS3280);
        moonbit_decref(_M0L3keyS1323);
        if (_M0L7_2abindS1328 == 0) {
          if (_M0L7_2abindS1328) {
            moonbit_decref(_M0L7_2abindS1328);
          }
          goto join_1325;
        } else {
          void* _M0L7_2aSomeS1329 = _M0L7_2abindS1328;
          void* _M0L4_2axS1330 = _M0L7_2aSomeS1329;
          switch (Moonbit_object_tag(_M0L4_2axS1330)) {
            case 0: {
              struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1331 =
                (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1330;
              moonbit_string_t _M0L8_2afieldS3515 = _M0L9_2aStringS1331->$0;
              int32_t _M0L6_2acntS3870 =
                Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1331));
              moonbit_string_t _M0L4_2asS1332;
              if (_M0L6_2acntS3870 > 1) {
                int32_t _M0L11_2anew__cntS3871 = _M0L6_2acntS3870 - 1;
                Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1331), _M0L11_2anew__cntS3871);
                moonbit_incref(_M0L8_2afieldS3515);
              } else if (_M0L6_2acntS3870 == 1) {
                #line 282 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
                moonbit_free(_M0L9_2aStringS1331);
              }
              _M0L4_2asS1332 = _M0L8_2afieldS3515;
              _M0L1sS1327 = _M0L4_2asS1332;
              goto join_1326;
              break;
            }
            default: {
              moonbit_decref(_M0L4_2axS1330);
              goto join_1325;
              break;
            }
          }
        }
        goto joinlet_3970;
        join_1326:;
        _M0L6_2atmpS3279 = _M0L1sS1327;
        #line 283 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1319, _M0L6_2atmpS3279);
        if (_M0L6_2atmpS3279) {
          moonbit_decref(_M0L6_2atmpS3279);
        }
        joinlet_3970:;
        goto joinlet_3969;
        join_1325:;
        _M0L6_2atmpS3278 = 0;
        #line 284 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1319, _M0L6_2atmpS3278);
        if (_M0L6_2atmpS3278) {
          moonbit_decref(_M0L6_2atmpS3278);
        }
        joinlet_3969:;
      }
      _M0L6_2atmpS3281 = _M0L2__S1322 + 1;
      _M0L2__S1322 = _M0L6_2atmpS3281;
      continue;
    }
    break;
  }
  return _M0L6resultS1319;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4mset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1317,
  struct _M0TPB5ArrayGsE* _M0L4keysS1315,
  struct _M0TPB5ArrayGsE* _M0L6valuesS1316
) {
  int32_t _M0L1iS1314;
  #line 269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L1iS1314 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3269;
    int32_t _if__result_3972;
    #line 270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3269 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1315);
    if (_M0L1iS1314 < _M0L6_2atmpS3269) {
      int32_t _M0L6_2atmpS3268;
      #line 270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3268 = _M0MPC15array5Array6lengthGsE(_M0L6valuesS1316);
      _if__result_3972 = _M0L1iS1314 < _M0L6_2atmpS3268;
    } else {
      _if__result_3972 = 0;
    }
    if (_if__result_3972) {
      struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3270 =
        _M0L4selfS1317->$0;
      moonbit_string_t _M0L6_2atmpS3271;
      moonbit_string_t _M0L6_2atmpS3273;
      void* _M0L6StringS3272;
      struct _M0TPB3MapGsiE* _M0L7expiresS3274;
      moonbit_string_t _M0L6_2atmpS3275;
      int32_t _M0L6_2atmpS3276;
      moonbit_incref(_M0L4dataS3270);
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3271
      = _M0MPC15array5Array2atGsE(_M0L4keysS1315, _M0L1iS1314);
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3273
      = _M0MPC15array5Array2atGsE(_M0L6valuesS1316, _M0L1iS1314);
      _M0L6StringS3272
      = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
      Moonbit_object_header(_M0L6StringS3272)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
      ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3272)->$0
      = _M0L6_2atmpS3273;
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3270, _M0L6_2atmpS3271, _M0L6StringS3272);
      moonbit_decref(_M0L4dataS3270);
      moonbit_decref(_M0L6_2atmpS3271);
      moonbit_decref(_M0L6StringS3272);
      _M0L7expiresS3274 = _M0L4selfS1317->$1;
      moonbit_incref(_M0L7expiresS3274);
      #line 272 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3275
      = _M0MPC15array5Array2atGsE(_M0L4keysS1315, _M0L1iS1314);
      #line 272 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS3274, _M0L6_2atmpS3275);
      moonbit_decref(_M0L7expiresS3274);
      moonbit_decref(_M0L6_2atmpS3275);
      _M0L6_2atmpS3276 = _M0L1iS1314 + 1;
      _M0L1iS1314 = _M0L6_2atmpS3276;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1311,
  moonbit_string_t _M0L3keyS1312
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3260;
  int32_t _M0L6_2atmpS3259;
  #line 226 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3260 = _M0L4selfS1311->$0;
  moonbit_incref(_M0L4dataS3260);
  #line 227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3259
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3260, _M0L3keyS1312);
  moonbit_decref(_M0L4dataS3260);
  if (!_M0L6_2atmpS3259) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS3261 = _M0L4selfS1311->$1;
    int32_t _result_3973;
    moonbit_incref(_M0L7expiresS3261);
    #line 229 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3973 = _M0MPB3Map8containsGsiE(_M0L7expiresS3261, _M0L3keyS1312);
    moonbit_decref(_M0L7expiresS3261);
    if (_result_3973) {
      struct _M0TPB3MapGsiE* _M0L7expiresS3267 = _M0L4selfS1311->$1;
      int64_t _M0L6_2atmpS3266;
      int32_t _M0L6_2atmpS3264;
      int32_t _M0L13current__timeS3265;
      int32_t _M0L9remainingS1313;
      moonbit_incref(_M0L7expiresS3267);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3266 = _M0MPB3Map3getGsiE(_M0L7expiresS3267, _M0L3keyS1312);
      moonbit_decref(_M0L7expiresS3267);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3264 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS3266);
      _M0L13current__timeS3265 = _M0L4selfS1311->$2;
      _M0L9remainingS1313 = _M0L6_2atmpS3264 - _M0L13current__timeS3265;
      if (_M0L9remainingS1313 <= 0) {
        struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3262 =
          _M0L4selfS1311->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS3263;
        moonbit_incref(_M0L4dataS3262);
        #line 232 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3262, _M0L3keyS1312);
        moonbit_decref(_M0L4dataS3262);
        _M0L7expiresS3263 = _M0L4selfS1311->$1;
        moonbit_incref(_M0L7expiresS3263);
        #line 233 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS3263, _M0L3keyS1312);
        moonbit_decref(_M0L7expiresS3263);
        return -2;
      } else {
        return _M0L9remainingS1313 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1308,
  moonbit_string_t _M0L3keyS1309,
  int32_t _M0L7secondsS1310
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3254;
  int32_t _result_3974;
  #line 208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3254 = _M0L4selfS1308->$0;
  moonbit_incref(_M0L4dataS3254);
  #line 209 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3974
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3254, _M0L3keyS1309);
  moonbit_decref(_M0L4dataS3254);
  if (_result_3974) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3255 = _M0L4selfS1308->$1;
    int32_t _M0L13current__timeS3257 = _M0L4selfS1308->$2;
    int32_t _M0L6_2atmpS3258 = _M0L7secondsS1310 * 1000;
    int32_t _M0L6_2atmpS3256 = _M0L13current__timeS3257 + _M0L6_2atmpS3258;
    moonbit_incref(_M0L7expiresS3255);
    #line 210 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS3255, _M0L3keyS1309, _M0L6_2atmpS3256);
    moonbit_decref(_M0L7expiresS3255);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3set(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1305,
  moonbit_string_t _M0L3keyS1306,
  moonbit_string_t _M0L5valueS1307
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3251;
  void* _M0L6StringS3252;
  struct _M0TPB3MapGsiE* _M0L7expiresS3253;
  #line 203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3251 = _M0L4selfS1305->$0;
  moonbit_incref(_M0L5valueS1307);
  _M0L6StringS3252
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3252)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3252)->$0
  = _M0L5valueS1307;
  moonbit_incref(_M0L4dataS3251);
  #line 204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3251, _M0L3keyS1306, _M0L6StringS3252);
  moonbit_decref(_M0L4dataS3251);
  moonbit_decref(_M0L6StringS3252);
  _M0L7expiresS3253 = _M0L4selfS1305->$1;
  moonbit_incref(_M0L7expiresS3253);
  #line 205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3253, _M0L3keyS1306);
  moonbit_decref(_M0L7expiresS3253);
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1300,
  moonbit_string_t _M0L3keyS1301
) {
  int32_t _M0L12expire__timeS1299;
  struct _M0TPB3MapGsiE* _M0L7expiresS3250;
  int64_t _M0L7_2abindS1302;
  int32_t _M0L13current__timeS3247;
  #line 188 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7expiresS3250 = _M0L4selfS1300->$1;
  moonbit_incref(_M0L7expiresS3250);
  #line 189 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1302 = _M0MPB3Map3getGsiE(_M0L7expiresS3250, _M0L3keyS1301);
  moonbit_decref(_M0L7expiresS3250);
  if (_M0L7_2abindS1302 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1303 = _M0L7_2abindS1302;
    int32_t _M0L15_2aexpire__timeS1304 = (int32_t)_M0L7_2aSomeS1303;
    _M0L12expire__timeS1299 = _M0L15_2aexpire__timeS1304;
    goto join_1298;
  }
  join_1298:;
  _M0L13current__timeS3247 = _M0L4selfS1300->$2;
  if (_M0L12expire__timeS1299 <= _M0L13current__timeS3247) {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3248 =
      _M0L4selfS1300->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS3249;
    moonbit_incref(_M0L4dataS3248);
    #line 192 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3248, _M0L3keyS1301);
    moonbit_decref(_M0L4dataS3248);
    _M0L7expiresS3249 = _M0L4selfS1300->$1;
    moonbit_incref(_M0L7expiresS3249);
    #line 193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3249, _M0L3keyS1301);
    moonbit_decref(_M0L7expiresS3249);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1296,
  int32_t _M0L2msS1297
) {
  int32_t _M0L13current__timeS3246;
  int32_t _M0L6_2atmpS3245;
  #line 184 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L13current__timeS3246 = _M0L4selfS1296->$2;
  _M0L6_2atmpS3245 = _M0L13current__timeS3246 + _M0L2msS1297;
  _M0L4selfS1296->$2 = _M0L6_2atmpS3245;
  return 0;
}

struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0MP38JIA2JIA29moonbitdb3lib8Database3new(
  
) {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS1294;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS3244;
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L6_2atmpS3243;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3239;
  struct _M0TUsiE** _M0L7_2abindS1295;
  struct _M0TUsiE** _M0L6_2atmpS3242;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3241;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3240;
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _block_3976;
  #line 176 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1294
  = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3244 = _M0L7_2abindS1294;
  _M0L6_2atmpS3243
  = (struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE){
    .$0 = _M0L6_2atmpS3244, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3239
  = _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L6_2atmpS3243, 1000ll);
  moonbit_decref(_M0L6_2atmpS3243.$0);
  _M0L7_2abindS1295 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3242 = _M0L7_2abindS1295;
  _M0L6_2atmpS3241
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3242, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3240 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3241, 1000ll);
  moonbit_decref(_M0L6_2atmpS3241.$0);
  _block_3976
  = (struct _M0TP38JIA2JIA29moonbitdb3lib8Database*)moonbit_malloc(sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database));
  Moonbit_object_header(_block_3976)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 27, 0);
  _block_3976->$0 = _M0L6_2atmpS3239;
  _block_3976->$1 = _M0L6_2atmpS3240;
  _block_3976->$2 = 0;
  return _block_3976;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib5Deque9to__array(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1287
) {
  moonbit_string_t* _M0L6_2atmpS3238;
  struct _M0TPB5ArrayGsE* _M0L6resultS1285;
  struct _M0TPB5ArrayGsE* _M0L5frontS3235;
  int32_t _M0L6_2atmpS3234;
  int32_t _M0L6_2atmpS3233;
  int32_t _M0L1iS1286;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1289;
  int32_t _M0L7_2abindS1290;
  int32_t _M0L2__S1291;
  #line 48 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3238 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1285
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1285)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1285->$0 = _M0L6_2atmpS3238;
  _M0L6resultS1285->$1 = 0;
  _M0L5frontS3235 = _M0L4selfS1287->$0;
  moonbit_incref(_M0L5frontS3235);
  #line 50 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3234 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3235);
  moonbit_decref(_M0L5frontS3235);
  _M0L6_2atmpS3233 = _M0L6_2atmpS3234 - 1;
  _M0L1iS1286 = _M0L6_2atmpS3233;
  while (1) {
    if (_M0L1iS1286 >= 0) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3231 = _M0L4selfS1287->$0;
      moonbit_string_t _M0L6_2atmpS3230;
      int32_t _M0L6_2atmpS3232;
      moonbit_incref(_M0L5frontS3231);
      #line 51 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3230
      = _M0MPC15array5Array2atGsE(_M0L5frontS3231, _M0L1iS1286);
      moonbit_decref(_M0L5frontS3231);
      #line 51 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1285, _M0L6_2atmpS3230);
      moonbit_decref(_M0L6_2atmpS3230);
      _M0L6_2atmpS3232 = _M0L1iS1286 - 1;
      _M0L1iS1286 = _M0L6_2atmpS3232;
      continue;
    }
    break;
  }
  _M0L7_2abindS1289 = _M0L4selfS1287->$1;
  _M0L7_2abindS1290 = _M0L7_2abindS1289->$1;
  moonbit_incref(_M0L7_2abindS1289);
  _M0L2__S1291 = 0;
  while (1) {
    if (_M0L2__S1291 < _M0L7_2abindS1290) {
      moonbit_string_t* _M0L3bufS3237 = _M0L7_2abindS1289->$0;
      moonbit_string_t _M0L4itemS1292 =
        (moonbit_string_t)_M0L3bufS3237[_M0L2__S1291];
      int32_t _M0L6_2atmpS3236;
      moonbit_incref(_M0L4itemS1292);
      #line 54 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1285, _M0L4itemS1292);
      moonbit_decref(_M0L4itemS1292);
      _M0L6_2atmpS3236 = _M0L2__S1291 + 1;
      _M0L2__S1291 = _M0L6_2atmpS3236;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1289);
    }
    break;
  }
  return _M0L6resultS1285;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1284
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3229;
  int32_t _M0L6_2atmpS3226;
  struct _M0TPB5ArrayGsE* _M0L4backS3228;
  int32_t _M0L6_2atmpS3227;
  #line 44 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3229 = _M0L4selfS1284->$0;
  moonbit_incref(_M0L5frontS3229);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3226 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3229);
  moonbit_decref(_M0L5frontS3229);
  _M0L4backS3228 = _M0L4selfS1284->$1;
  moonbit_incref(_M0L4backS3228);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3227 = _M0MPC15array5Array6lengthGsE(_M0L4backS3228);
  moonbit_decref(_M0L4backS3228);
  return _M0L6_2atmpS3226 + _M0L6_2atmpS3227;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib5Deque9pop__back(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1277
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3220;
  int32_t _M0L6_2atmpS3219;
  struct _M0TPB5ArrayGsE* _M0L4backS3225;
  moonbit_string_t _result_3981;
  #line 31 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4backS3220 = _M0L4selfS1277->$1;
  moonbit_incref(_M0L4backS3220);
  #line 32 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3219 = _M0MPC15array5Array6lengthGsE(_M0L4backS3220);
  moonbit_decref(_M0L4backS3220);
  if (_M0L6_2atmpS3219 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3222 = _M0L4selfS1277->$0;
      int32_t _M0L6_2atmpS3221;
      moonbit_incref(_M0L5frontS3222);
      #line 33 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3221 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3222);
      moonbit_decref(_M0L5frontS3222);
      if (_M0L6_2atmpS3221 > 0) {
        struct _M0TPB5ArrayGsE* _M0L5frontS3224 = _M0L4selfS1277->$0;
        moonbit_string_t _M0L4itemS1278;
        moonbit_string_t _M0L1vS1280;
        struct _M0TPB5ArrayGsE* _M0L4backS3223;
        moonbit_incref(_M0L5frontS3224);
        #line 34 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L4itemS1278 = _M0MPC15array5Array3popGsE(_M0L5frontS3224);
        moonbit_decref(_M0L5frontS3224);
        if (_M0L4itemS1278 == 0) {
          if (_M0L4itemS1278) {
            moonbit_decref(_M0L4itemS1278);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1281 = _M0L4itemS1278;
          moonbit_string_t _M0L4_2avS1282 = _M0L7_2aSomeS1281;
          _M0L1vS1280 = _M0L4_2avS1282;
          goto join_1279;
        }
        goto joinlet_3980;
        join_1279:;
        _M0L4backS3223 = _M0L4selfS1277->$1;
        moonbit_incref(_M0L4backS3223);
        #line 36 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4backS3223, _M0L1vS1280);
        moonbit_decref(_M0L4backS3223);
        moonbit_decref(_M0L1vS1280);
        joinlet_3980:;
        continue;
      }
      break;
    }
  }
  _M0L4backS3225 = _M0L4selfS1277->$1;
  moonbit_incref(_M0L4backS3225);
  #line 41 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3981 = _M0MPC15array5Array3popGsE(_M0L4backS3225);
  moonbit_decref(_M0L4backS3225);
  return _result_3981;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib5Deque10pop__front(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1270
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3213;
  int32_t _M0L6_2atmpS3212;
  struct _M0TPB5ArrayGsE* _M0L5frontS3218;
  moonbit_string_t _result_3984;
  #line 18 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3213 = _M0L4selfS1270->$0;
  moonbit_incref(_M0L5frontS3213);
  #line 19 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3212 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3213);
  moonbit_decref(_M0L5frontS3213);
  if (_M0L6_2atmpS3212 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L4backS3215 = _M0L4selfS1270->$1;
      int32_t _M0L6_2atmpS3214;
      moonbit_incref(_M0L4backS3215);
      #line 20 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3214 = _M0MPC15array5Array6lengthGsE(_M0L4backS3215);
      moonbit_decref(_M0L4backS3215);
      if (_M0L6_2atmpS3214 > 0) {
        struct _M0TPB5ArrayGsE* _M0L4backS3217 = _M0L4selfS1270->$1;
        moonbit_string_t _M0L4itemS1271;
        moonbit_string_t _M0L1vS1273;
        struct _M0TPB5ArrayGsE* _M0L5frontS3216;
        moonbit_incref(_M0L4backS3217);
        #line 21 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L4itemS1271 = _M0MPC15array5Array3popGsE(_M0L4backS3217);
        moonbit_decref(_M0L4backS3217);
        if (_M0L4itemS1271 == 0) {
          if (_M0L4itemS1271) {
            moonbit_decref(_M0L4itemS1271);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1274 = _M0L4itemS1271;
          moonbit_string_t _M0L4_2avS1275 = _M0L7_2aSomeS1274;
          _M0L1vS1273 = _M0L4_2avS1275;
          goto join_1272;
        }
        goto joinlet_3983;
        join_1272:;
        _M0L5frontS3216 = _M0L4selfS1270->$0;
        moonbit_incref(_M0L5frontS3216);
        #line 23 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L5frontS3216, _M0L1vS1273);
        moonbit_decref(_M0L5frontS3216);
        moonbit_decref(_M0L1vS1273);
        joinlet_3983:;
        continue;
      }
      break;
    }
  }
  _M0L5frontS3218 = _M0L4selfS1270->$0;
  moonbit_incref(_M0L5frontS3218);
  #line 28 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3984 = _M0MPC15array5Array3popGsE(_M0L5frontS3218);
  moonbit_decref(_M0L5frontS3218);
  return _result_3984;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque10push__back(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1268,
  moonbit_string_t _M0L5valueS1269
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3211;
  #line 14 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4backS3211 = _M0L4selfS1268->$1;
  moonbit_incref(_M0L4backS3211);
  #line 15 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPC15array5Array4pushGsE(_M0L4backS3211, _M0L5valueS1269);
  moonbit_decref(_M0L4backS3211);
  return 0;
}

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0MP38JIA2JIA29moonbitdb3lib5Deque3new(
  
) {
  moonbit_string_t* _M0L6_2atmpS3210;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3207;
  moonbit_string_t* _M0L6_2atmpS3209;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3208;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _block_3985;
  #line 6 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3210 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3207
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3207)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS3207->$0 = _M0L6_2atmpS3210;
  _M0L6_2atmpS3207->$1 = 0;
  _M0L6_2atmpS3209 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3208
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3208)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS3208->$0 = _M0L6_2atmpS3209;
  _M0L6_2atmpS3208->$1 = 0;
  _block_3985
  = (struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*)moonbit_malloc(sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque));
  Moonbit_object_header(_block_3985)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 31, 0);
  _block_3985->$0 = _M0L6_2atmpS3207;
  _block_3985->$1 = _M0L6_2atmpS3208;
  return _block_3985;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1267) {
  double _M0L6_2atmpS3206;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3206 = (double)_M0L4selfS1267;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3206);
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1265,
  struct _M0TPC16string10StringView _M0L9separatorS1266
) {
  moonbit_string_t* _M0L3bufS3204;
  int32_t _M0L3lenS3205;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3203;
  moonbit_string_t _result_3986;
  #line 2184 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS3204 = _M0L4selfS1265->$0;
  _M0L3lenS3205 = _M0L4selfS1265->$1;
  moonbit_incref(_M0L3bufS3204);
  _M0L6_2atmpS3203
  = (struct _M0TPB9ArrayViewGsE){
    .$0 = _M0L3bufS3204, .$1 = 0, .$2 = _M0L3lenS3205
  };
  #line 2188 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _result_3986
  = _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS3203, _M0L9separatorS1266);
  moonbit_decref(_M0L6_2atmpS3203.$0);
  return _result_3986;
}

moonbit_string_t _M0MPC15array5Array3popGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1262
) {
  int32_t _M0L3lenS1261;
  #line 325 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1261 = _M0L4selfS1262->$1;
  if (_M0L3lenS1261 == 0) {
    return 0;
  } else {
    int32_t _M0L5indexS1263 = _M0L3lenS1261 - 1;
    moonbit_string_t* _M0L3bufS3202 = _M0L4selfS1262->$0;
    moonbit_string_t _M0L1vS1264 =
      (moonbit_string_t)_M0L3bufS3202[_M0L5indexS1263];
    moonbit_string_t* _M0L3bufS3201 = _M0L4selfS1262->$0;
    moonbit_string_t _M0L6_2aoldS3552;
    if (
      _M0L5indexS1263 < 0
      || _M0L5indexS1263 >= Moonbit_array_length(_M0L3bufS3201)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3552 = (moonbit_string_t)_M0L3bufS3201[_M0L5indexS1263];
    moonbit_incref(_M0L1vS1264);
    moonbit_decref(_M0L6_2aoldS3552);
    if (
      _M0L5indexS1263 < 0
      || _M0L5indexS1263 >= Moonbit_array_length(_M0L3bufS3201)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3201[_M0L5indexS1263]
    = (moonbit_string_t)moonbit_string_literal_95.data;
    _M0L4selfS1262->$1 = _M0L5indexS1263;
    return _M0L1vS1264;
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1253,
  int32_t _M0L5indexS1254
) {
  int32_t _M0L3lenS1252;
  int32_t _if__result_3987;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1252 = _M0L4selfS1253->$1;
  if (_M0L5indexS1254 >= 0) {
    _if__result_3987 = _M0L5indexS1254 < _M0L3lenS1252;
  } else {
    _if__result_3987 = 0;
  }
  if (_if__result_3987) {
    moonbit_string_t* _M0L6_2atmpS3198;
    moonbit_string_t _M0L6_2atmpS3556;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3198 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1253);
    _M0L6_2atmpS3556 = (moonbit_string_t)_M0L6_2atmpS3198[_M0L5indexS1254];
    moonbit_incref(_M0L6_2atmpS3556);
    moonbit_decref(_M0L6_2atmpS3198);
    return _M0L6_2atmpS3556;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS1256,
  int32_t _M0L5indexS1257
) {
  int32_t _M0L3lenS1255;
  int32_t _if__result_3988;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1255 = _M0L4selfS1256->$1;
  if (_M0L5indexS1257 >= 0) {
    _if__result_3988 = _M0L5indexS1257 < _M0L3lenS1255;
  } else {
    _if__result_3988 = 0;
  }
  if (_if__result_3988) {
    moonbit_string_t* _M0L6_2atmpS3199;
    moonbit_string_t _M0L6_2atmpS3557;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3199 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS1256);
    _M0L6_2atmpS3557 = (moonbit_string_t)_M0L6_2atmpS3199[_M0L5indexS1257];
    if (_M0L6_2atmpS3557) {
      moonbit_incref(_M0L6_2atmpS3557);
    }
    moonbit_decref(_M0L6_2atmpS3199);
    return _M0L6_2atmpS3557;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TUsfE* _M0MPC15array5Array2atGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS1259,
  int32_t _M0L5indexS1260
) {
  int32_t _M0L3lenS1258;
  int32_t _if__result_3989;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1258 = _M0L4selfS1259->$1;
  if (_M0L5indexS1260 >= 0) {
    _if__result_3989 = _M0L5indexS1260 < _M0L3lenS1258;
  } else {
    _if__result_3989 = 0;
  }
  if (_if__result_3989) {
    struct _M0TUsfE** _M0L6_2atmpS3200;
    struct _M0TUsfE* _M0L6_2atmpS3558;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3200 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1259);
    _M0L6_2atmpS3558 = (struct _M0TUsfE*)_M0L6_2atmpS3200[_M0L5indexS1260];
    if (_M0L6_2atmpS3558) {
      moonbit_incref(_M0L6_2atmpS3558);
    }
    moonbit_decref(_M0L6_2atmpS3200);
    return _M0L6_2atmpS3558;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1251) {
  moonbit_string_t _M0L6_2atmpS3197;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3197
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1251);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3197);
  moonbit_decref(_M0L6_2atmpS3197);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1250) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1250);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1237) {
  uint64_t _M0L4bitsS1238;
  uint64_t _M0L6_2atmpS3196;
  uint64_t _M0L6_2atmpS3195;
  int32_t _M0L8ieeeSignS1239;
  uint64_t _M0L12ieeeMantissaS1240;
  uint64_t _M0L6_2atmpS3194;
  uint64_t _M0L6_2atmpS3193;
  int32_t _M0L12ieeeExponentS1241;
  int32_t _if__result_3990;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1242;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1243;
  moonbit_string_t _result_3992;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1237 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  _M0L4bitsS1238 = *(int64_t*)&_M0L3valS1237;
  _M0L6_2atmpS3196 = _M0L4bitsS1238 >> 63;
  _M0L6_2atmpS3195 = _M0L6_2atmpS3196 & 1ull;
  _M0L8ieeeSignS1239 = _M0L6_2atmpS3195 != 0ull;
  _M0L12ieeeMantissaS1240 = _M0L4bitsS1238 & 4503599627370495ull;
  _M0L6_2atmpS3194 = _M0L4bitsS1238 >> 52;
  _M0L6_2atmpS3193 = _M0L6_2atmpS3194 & 2047ull;
  _M0L12ieeeExponentS1241 = (int32_t)_M0L6_2atmpS3193;
  if (_M0L12ieeeExponentS1241 == 2047) {
    _if__result_3990 = 1;
  } else if (_M0L12ieeeExponentS1241 == 0) {
    _if__result_3990 = _M0L12ieeeMantissaS1240 == 0ull;
  } else {
    _if__result_3990 = 0;
  }
  if (_if__result_3990) {
    int32_t _M0L6_2atmpS3184 = _M0L12ieeeExponentS1241 != 0;
    int32_t _M0L6_2atmpS3185 = _M0L12ieeeMantissaS1240 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1239, _M0L6_2atmpS3184, _M0L6_2atmpS3185);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1242
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1240, _M0L12ieeeExponentS1241);
  if (_M0L7_2abindS1242 == 0) {
    uint32_t _M0L6_2atmpS3186;
    if (_M0L7_2abindS1242) {
      moonbit_decref(_M0L7_2abindS1242);
    }
    _M0L6_2atmpS3186 = *(uint32_t*)&_M0L12ieeeExponentS1241;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1243 = _M0FPB3d2d(_M0L12ieeeMantissaS1240, _M0L6_2atmpS3186);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1244 = _M0L7_2abindS1242;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1245 = _M0L7_2aSomeS1244;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1246 = _M0L4_2afS1245;
    while (1) {
      uint64_t _M0L8mantissaS3192 = _M0L1xS1246->$0;
      uint64_t _M0L1qS1247 = _M0L8mantissaS3192 / 10ull;
      uint64_t _M0L8mantissaS3190 = _M0L1xS1246->$0;
      uint64_t _M0L6_2atmpS3191 = 10ull * _M0L1qS1247;
      uint64_t _M0L1rS1248 = _M0L8mantissaS3190 - _M0L6_2atmpS3191;
      int32_t _M0L8exponentS3189;
      int32_t _M0L6_2atmpS3188;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3187;
      if (_M0L1rS1248 != 0ull) {
        _M0L1vS1243 = _M0L1xS1246;
        break;
      }
      _M0L8exponentS3189 = _M0L1xS1246->$1;
      moonbit_decref(_M0L1xS1246);
      _M0L6_2atmpS3188 = _M0L8exponentS3189 + 1;
      _M0L6_2atmpS3187
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3187)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3187->$0 = _M0L1qS1247;
      _M0L6_2atmpS3187->$1 = _M0L6_2atmpS3188;
      _M0L1xS1246 = _M0L6_2atmpS3187;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_3992 = _M0FPB9to__chars(_M0L1vS1243, _M0L8ieeeSignS1239);
  moonbit_decref(_M0L1vS1243);
  return _result_3992;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1232,
  int32_t _M0L12ieeeExponentS1234
) {
  uint64_t _M0L2m2S1231;
  int32_t _M0L6_2atmpS3183;
  int32_t _M0L2e2S1233;
  int32_t _M0L6_2atmpS3182;
  uint64_t _M0L6_2atmpS3181;
  uint64_t _M0L4maskS1235;
  uint64_t _M0L8fractionS1236;
  int32_t _M0L6_2atmpS3180;
  uint64_t _M0L6_2atmpS3179;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3178;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1231 = 4503599627370496ull | _M0L12ieeeMantissaS1232;
  _M0L6_2atmpS3183 = _M0L12ieeeExponentS1234 - 1023;
  _M0L2e2S1233 = _M0L6_2atmpS3183 - 52;
  if (_M0L2e2S1233 > 0) {
    return 0;
  }
  if (_M0L2e2S1233 < -52) {
    return 0;
  }
  _M0L6_2atmpS3182 = -_M0L2e2S1233;
  _M0L6_2atmpS3181 = 1ull << (_M0L6_2atmpS3182 & 63);
  _M0L4maskS1235 = _M0L6_2atmpS3181 - 1ull;
  _M0L8fractionS1236 = _M0L2m2S1231 & _M0L4maskS1235;
  if (_M0L8fractionS1236 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3180 = -_M0L2e2S1233;
  _M0L6_2atmpS3179 = _M0L2m2S1231 >> (_M0L6_2atmpS3180 & 63);
  _M0L6_2atmpS3178
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3178)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3178->$0 = _M0L6_2atmpS3179;
  _M0L6_2atmpS3178->$1 = 0;
  return _M0L6_2atmpS3178;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1199,
  int32_t _M0L4signS1197
) {
  int32_t _M0L6_2atmpS3177;
  moonbit_bytes_t _M0L6resultS1195;
  int32_t _M0Lm5indexS1196;
  uint64_t _M0L6outputS1198;
  int32_t _M0L7olengthS1200;
  int32_t _M0L8exponentS3176;
  int32_t _M0L6_2atmpS3175;
  int32_t _M0Lm3expS1201;
  int32_t _M0L6_2atmpS3174;
  int32_t _M0L6_2atmpS3172;
  int32_t _M0L18scientificNotationS1202;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3177 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1195
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3177);
  _M0Lm5indexS1196 = 0;
  if (_M0L4signS1197) {
    int32_t _M0L6_2atmpS3046 = _M0Lm5indexS1196;
    int32_t _M0L6_2atmpS3047;
    if (
      _M0L6_2atmpS3046 < 0
      || _M0L6_2atmpS3046 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3046] = 45;
    _M0L6_2atmpS3047 = _M0Lm5indexS1196;
    _M0Lm5indexS1196 = _M0L6_2atmpS3047 + 1;
  }
  _M0L6outputS1198 = _M0L1vS1199->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1200 = _M0FPB17decimal__length17(_M0L6outputS1198);
  _M0L8exponentS3176 = _M0L1vS1199->$1;
  _M0L6_2atmpS3175 = _M0L8exponentS3176 + _M0L7olengthS1200;
  _M0Lm3expS1201 = _M0L6_2atmpS3175 - 1;
  _M0L6_2atmpS3174 = _M0Lm3expS1201;
  if (_M0L6_2atmpS3174 >= -6) {
    int32_t _M0L6_2atmpS3173 = _M0Lm3expS1201;
    _M0L6_2atmpS3172 = _M0L6_2atmpS3173 < 21;
  } else {
    _M0L6_2atmpS3172 = 0;
  }
  _M0L18scientificNotationS1202 = !_M0L6_2atmpS3172;
  if (_M0L18scientificNotationS1202) {
    int32_t _M0L7_2abindS1203 = _M0L7olengthS1200 - 1;
    uint64_t _M0L6outputS1204;
    int32_t _M0L1iS1205 = 0;
    uint64_t _M0L6outputS1206 = _M0L6outputS1198;
    int32_t _M0L6_2atmpS3048;
    int32_t _M0L6_2atmpS3052;
    int32_t _M0L6_2atmpS3051;
    int32_t _M0L6_2atmpS3050;
    int32_t _M0L6_2atmpS3049;
    int32_t _M0L6_2atmpS3056;
    int32_t _M0L6_2atmpS3057;
    int32_t _M0L6_2atmpS3058;
    int32_t _M0L6_2atmpS3059;
    int32_t _M0L6_2atmpS3060;
    int32_t _M0L6_2atmpS3066;
    int32_t _M0L6_2atmpS3099;
    moonbit_string_t _result_3994;
    while (1) {
      if (_M0L1iS1205 < _M0L7_2abindS1203) {
        uint64_t _M0L1cS1207 = _M0L6outputS1206 % 10ull;
        int32_t _M0L6_2atmpS3105 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3104 = _M0L6_2atmpS3105 + _M0L7olengthS1200;
        int32_t _M0L6_2atmpS3100 = _M0L6_2atmpS3104 - _M0L1iS1205;
        int32_t _M0L6_2atmpS3103 = (int32_t)_M0L1cS1207;
        int32_t _M0L6_2atmpS3102 = 48 + _M0L6_2atmpS3103;
        int32_t _M0L6_2atmpS3101 = _M0L6_2atmpS3102 & 0xff;
        int32_t _M0L6_2atmpS3106;
        uint64_t _M0L6_2atmpS3107;
        if (
          _M0L6_2atmpS3100 < 0
          || _M0L6_2atmpS3100 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3100] = _M0L6_2atmpS3101;
        _M0L6_2atmpS3106 = _M0L1iS1205 + 1;
        _M0L6_2atmpS3107 = _M0L6outputS1206 / 10ull;
        _M0L1iS1205 = _M0L6_2atmpS3106;
        _M0L6outputS1206 = _M0L6_2atmpS3107;
        continue;
      } else {
        _M0L6outputS1204 = _M0L6outputS1206;
      }
      break;
    }
    _M0L6_2atmpS3048 = _M0Lm5indexS1196;
    _M0L6_2atmpS3052 = (int32_t)_M0L6outputS1204;
    _M0L6_2atmpS3051 = _M0L6_2atmpS3052 % 10;
    _M0L6_2atmpS3050 = 48 + _M0L6_2atmpS3051;
    _M0L6_2atmpS3049 = _M0L6_2atmpS3050 & 0xff;
    if (
      _M0L6_2atmpS3048 < 0
      || _M0L6_2atmpS3048 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3048] = _M0L6_2atmpS3049;
    if (_M0L7olengthS1200 > 1) {
      int32_t _M0L6_2atmpS3054 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3053 = _M0L6_2atmpS3054 + 1;
      if (
        _M0L6_2atmpS3053 < 0
        || _M0L6_2atmpS3053 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3053] = 46;
    } else {
      int32_t _M0L6_2atmpS3055 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3055 - 1;
    }
    _M0L6_2atmpS3056 = _M0Lm5indexS1196;
    _M0L6_2atmpS3057 = _M0L7olengthS1200 + 1;
    _M0Lm5indexS1196 = _M0L6_2atmpS3056 + _M0L6_2atmpS3057;
    _M0L6_2atmpS3058 = _M0Lm5indexS1196;
    if (
      _M0L6_2atmpS3058 < 0
      || _M0L6_2atmpS3058 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3058] = 101;
    _M0L6_2atmpS3059 = _M0Lm5indexS1196;
    _M0Lm5indexS1196 = _M0L6_2atmpS3059 + 1;
    _M0L6_2atmpS3060 = _M0Lm3expS1201;
    if (_M0L6_2atmpS3060 < 0) {
      int32_t _M0L6_2atmpS3061 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3062;
      int32_t _M0L6_2atmpS3063;
      if (
        _M0L6_2atmpS3061 < 0
        || _M0L6_2atmpS3061 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3061] = 45;
      _M0L6_2atmpS3062 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3062 + 1;
      _M0L6_2atmpS3063 = _M0Lm3expS1201;
      _M0Lm3expS1201 = -_M0L6_2atmpS3063;
    } else {
      int32_t _M0L6_2atmpS3064 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3065;
      if (
        _M0L6_2atmpS3064 < 0
        || _M0L6_2atmpS3064 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3064] = 43;
      _M0L6_2atmpS3065 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3065 + 1;
    }
    _M0L6_2atmpS3066 = _M0Lm3expS1201;
    if (_M0L6_2atmpS3066 >= 100) {
      int32_t _M0L6_2atmpS3082 = _M0Lm3expS1201;
      int32_t _M0L1aS1209 = _M0L6_2atmpS3082 / 100;
      int32_t _M0L6_2atmpS3081 = _M0Lm3expS1201;
      int32_t _M0L6_2atmpS3080 = _M0L6_2atmpS3081 / 10;
      int32_t _M0L1bS1210 = _M0L6_2atmpS3080 % 10;
      int32_t _M0L6_2atmpS3079 = _M0Lm3expS1201;
      int32_t _M0L1cS1211 = _M0L6_2atmpS3079 % 10;
      int32_t _M0L6_2atmpS3067 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3069 = 48 + _M0L1aS1209;
      int32_t _M0L6_2atmpS3068 = _M0L6_2atmpS3069 & 0xff;
      int32_t _M0L6_2atmpS3073;
      int32_t _M0L6_2atmpS3070;
      int32_t _M0L6_2atmpS3072;
      int32_t _M0L6_2atmpS3071;
      int32_t _M0L6_2atmpS3077;
      int32_t _M0L6_2atmpS3074;
      int32_t _M0L6_2atmpS3076;
      int32_t _M0L6_2atmpS3075;
      int32_t _M0L6_2atmpS3078;
      if (
        _M0L6_2atmpS3067 < 0
        || _M0L6_2atmpS3067 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3067] = _M0L6_2atmpS3068;
      _M0L6_2atmpS3073 = _M0Lm5indexS1196;
      _M0L6_2atmpS3070 = _M0L6_2atmpS3073 + 1;
      _M0L6_2atmpS3072 = 48 + _M0L1bS1210;
      _M0L6_2atmpS3071 = _M0L6_2atmpS3072 & 0xff;
      if (
        _M0L6_2atmpS3070 < 0
        || _M0L6_2atmpS3070 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3070] = _M0L6_2atmpS3071;
      _M0L6_2atmpS3077 = _M0Lm5indexS1196;
      _M0L6_2atmpS3074 = _M0L6_2atmpS3077 + 2;
      _M0L6_2atmpS3076 = 48 + _M0L1cS1211;
      _M0L6_2atmpS3075 = _M0L6_2atmpS3076 & 0xff;
      if (
        _M0L6_2atmpS3074 < 0
        || _M0L6_2atmpS3074 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3074] = _M0L6_2atmpS3075;
      _M0L6_2atmpS3078 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3078 + 3;
    } else {
      int32_t _M0L6_2atmpS3083 = _M0Lm3expS1201;
      if (_M0L6_2atmpS3083 >= 10) {
        int32_t _M0L6_2atmpS3093 = _M0Lm3expS1201;
        int32_t _M0L1aS1212 = _M0L6_2atmpS3093 / 10;
        int32_t _M0L6_2atmpS3092 = _M0Lm3expS1201;
        int32_t _M0L1bS1213 = _M0L6_2atmpS3092 % 10;
        int32_t _M0L6_2atmpS3084 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3086 = 48 + _M0L1aS1212;
        int32_t _M0L6_2atmpS3085 = _M0L6_2atmpS3086 & 0xff;
        int32_t _M0L6_2atmpS3090;
        int32_t _M0L6_2atmpS3087;
        int32_t _M0L6_2atmpS3089;
        int32_t _M0L6_2atmpS3088;
        int32_t _M0L6_2atmpS3091;
        if (
          _M0L6_2atmpS3084 < 0
          || _M0L6_2atmpS3084 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3084] = _M0L6_2atmpS3085;
        _M0L6_2atmpS3090 = _M0Lm5indexS1196;
        _M0L6_2atmpS3087 = _M0L6_2atmpS3090 + 1;
        _M0L6_2atmpS3089 = 48 + _M0L1bS1213;
        _M0L6_2atmpS3088 = _M0L6_2atmpS3089 & 0xff;
        if (
          _M0L6_2atmpS3087 < 0
          || _M0L6_2atmpS3087 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3087] = _M0L6_2atmpS3088;
        _M0L6_2atmpS3091 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3091 + 2;
      } else {
        int32_t _M0L6_2atmpS3094 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3097 = _M0Lm3expS1201;
        int32_t _M0L6_2atmpS3096 = 48 + _M0L6_2atmpS3097;
        int32_t _M0L6_2atmpS3095 = _M0L6_2atmpS3096 & 0xff;
        int32_t _M0L6_2atmpS3098;
        if (
          _M0L6_2atmpS3094 < 0
          || _M0L6_2atmpS3094 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3094] = _M0L6_2atmpS3095;
        _M0L6_2atmpS3098 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3098 + 1;
      }
    }
    _M0L6_2atmpS3099 = _M0Lm5indexS1196;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_3994
    = _M0FPB19string__from__bytes(_M0L6resultS1195, 0, _M0L6_2atmpS3099);
    moonbit_decref(_M0L6resultS1195);
    return _result_3994;
  } else {
    int32_t _M0L6_2atmpS3108 = _M0Lm3expS1201;
    int32_t _M0L6_2atmpS3171;
    moonbit_string_t _result_4000;
    if (_M0L6_2atmpS3108 < 0) {
      int32_t _M0L6_2atmpS3109 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3111;
      int32_t _M0L6_2atmpS3110;
      int32_t _M0L6_2atmpS3112;
      int32_t _M0L1iS1214;
      int32_t _M0L6_2atmpS3127;
      int32_t _M0L6_2atmpS3129;
      int32_t _M0L6_2atmpS3128;
      int32_t _M0L7currentS1216;
      int32_t _M0L1iS1217;
      uint64_t _M0L6outputS1218;
      if (
        _M0L6_2atmpS3109 < 0
        || _M0L6_2atmpS3109 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3109] = 48;
      _M0L6_2atmpS3111 = _M0Lm5indexS1196;
      _M0L6_2atmpS3110 = _M0L6_2atmpS3111 + 1;
      if (
        _M0L6_2atmpS3110 < 0
        || _M0L6_2atmpS3110 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3110] = 46;
      _M0L6_2atmpS3112 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3112 + 2;
      _M0L1iS1214 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3113 = _M0Lm3expS1201;
        if (_M0L1iS1214 > _M0L6_2atmpS3113) {
          int32_t _M0L6_2atmpS3116 = _M0Lm5indexS1196;
          int32_t _M0L6_2atmpS3115 = _M0L6_2atmpS3116 - _M0L1iS1214;
          int32_t _M0L6_2atmpS3114 = _M0L6_2atmpS3115 - 1;
          int32_t _M0L6_2atmpS3117;
          if (
            _M0L6_2atmpS3114 < 0
            || _M0L6_2atmpS3114 >= Moonbit_array_length(_M0L6resultS1195)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1195[_M0L6_2atmpS3114] = 48;
          _M0L6_2atmpS3117 = _M0L1iS1214 - 1;
          _M0L1iS1214 = _M0L6_2atmpS3117;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3127 = _M0Lm5indexS1196;
      _M0L6_2atmpS3129 = _M0Lm3expS1201;
      _M0L6_2atmpS3128 = -1 - _M0L6_2atmpS3129;
      _M0L7currentS1216 = _M0L6_2atmpS3127 + _M0L6_2atmpS3128;
      _M0L1iS1217 = 0;
      _M0L6outputS1218 = _M0L6outputS1198;
      while (1) {
        if (_M0L1iS1217 < _M0L7olengthS1200) {
          int32_t _M0L6_2atmpS3124 = _M0L7currentS1216 + _M0L7olengthS1200;
          int32_t _M0L6_2atmpS3123 = _M0L6_2atmpS3124 - _M0L1iS1217;
          int32_t _M0L6_2atmpS3118 = _M0L6_2atmpS3123 - 1;
          uint64_t _M0L6_2atmpS3122 = _M0L6outputS1218 % 10ull;
          int32_t _M0L6_2atmpS3121 = (int32_t)_M0L6_2atmpS3122;
          int32_t _M0L6_2atmpS3120 = 48 + _M0L6_2atmpS3121;
          int32_t _M0L6_2atmpS3119 = _M0L6_2atmpS3120 & 0xff;
          int32_t _M0L6_2atmpS3125;
          uint64_t _M0L6_2atmpS3126;
          if (
            _M0L6_2atmpS3118 < 0
            || _M0L6_2atmpS3118 >= Moonbit_array_length(_M0L6resultS1195)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1195[_M0L6_2atmpS3118] = _M0L6_2atmpS3119;
          _M0L6_2atmpS3125 = _M0L1iS1217 + 1;
          _M0L6_2atmpS3126 = _M0L6outputS1218 / 10ull;
          _M0L1iS1217 = _M0L6_2atmpS3125;
          _M0L6outputS1218 = _M0L6_2atmpS3126;
          continue;
        }
        break;
      }
      _M0Lm5indexS1196 = _M0L7currentS1216 + _M0L7olengthS1200;
    } else {
      int32_t _M0L6_2atmpS3131 = _M0Lm3expS1201;
      int32_t _M0L6_2atmpS3130 = _M0L6_2atmpS3131 + 1;
      if (_M0L6_2atmpS3130 >= _M0L7olengthS1200) {
        int32_t _M0L1iS1220 = 0;
        uint64_t _M0L6outputS1221 = _M0L6outputS1198;
        int32_t _M0L6_2atmpS3142;
        int32_t _M0L6_2atmpS3147;
        int32_t _M0L7_2abindS1223;
        int32_t _M0L1iS1224;
        int32_t _M0L6_2atmpS3148;
        int32_t _M0L6_2atmpS3151;
        int32_t _M0L6_2atmpS3150;
        int32_t _M0L6_2atmpS3149;
        while (1) {
          if (_M0L1iS1220 < _M0L7olengthS1200) {
            int32_t _M0L6_2atmpS3139 = _M0Lm5indexS1196;
            int32_t _M0L6_2atmpS3138 = _M0L6_2atmpS3139 + _M0L7olengthS1200;
            int32_t _M0L6_2atmpS3137 = _M0L6_2atmpS3138 - _M0L1iS1220;
            int32_t _M0L6_2atmpS3132 = _M0L6_2atmpS3137 - 1;
            uint64_t _M0L6_2atmpS3136 = _M0L6outputS1221 % 10ull;
            int32_t _M0L6_2atmpS3135 = (int32_t)_M0L6_2atmpS3136;
            int32_t _M0L6_2atmpS3134 = 48 + _M0L6_2atmpS3135;
            int32_t _M0L6_2atmpS3133 = _M0L6_2atmpS3134 & 0xff;
            int32_t _M0L6_2atmpS3140;
            uint64_t _M0L6_2atmpS3141;
            if (
              _M0L6_2atmpS3132 < 0
              || _M0L6_2atmpS3132 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3132] = _M0L6_2atmpS3133;
            _M0L6_2atmpS3140 = _M0L1iS1220 + 1;
            _M0L6_2atmpS3141 = _M0L6outputS1221 / 10ull;
            _M0L1iS1220 = _M0L6_2atmpS3140;
            _M0L6outputS1221 = _M0L6_2atmpS3141;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3142 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3142 + _M0L7olengthS1200;
        _M0L6_2atmpS3147 = _M0Lm3expS1201;
        _M0L7_2abindS1223 = _M0L6_2atmpS3147 + 1;
        _M0L1iS1224 = _M0L7olengthS1200;
        while (1) {
          if (_M0L1iS1224 < _M0L7_2abindS1223) {
            int32_t _M0L6_2atmpS3145 = _M0Lm5indexS1196;
            int32_t _M0L6_2atmpS3144 = _M0L6_2atmpS3145 + _M0L1iS1224;
            int32_t _M0L6_2atmpS3143 = _M0L6_2atmpS3144 - _M0L7olengthS1200;
            int32_t _M0L6_2atmpS3146;
            if (
              _M0L6_2atmpS3143 < 0
              || _M0L6_2atmpS3143 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3143] = 48;
            _M0L6_2atmpS3146 = _M0L1iS1224 + 1;
            _M0L1iS1224 = _M0L6_2atmpS3146;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3148 = _M0Lm5indexS1196;
        _M0L6_2atmpS3151 = _M0Lm3expS1201;
        _M0L6_2atmpS3150 = _M0L6_2atmpS3151 + 1;
        _M0L6_2atmpS3149 = _M0L6_2atmpS3150 - _M0L7olengthS1200;
        _M0Lm5indexS1196 = _M0L6_2atmpS3148 + _M0L6_2atmpS3149;
      } else {
        int32_t _M0L6_2atmpS3168 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3167 = _M0L6_2atmpS3168 + 1;
        int32_t _M0L1iS1226 = 0;
        int32_t _M0L7currentS1227 = _M0L6_2atmpS3167;
        uint64_t _M0L6outputS1228 = _M0L6outputS1198;
        int32_t _M0L6_2atmpS3169;
        int32_t _M0L6_2atmpS3170;
        while (1) {
          if (_M0L1iS1226 < _M0L7olengthS1200) {
            int32_t _M0L6_2atmpS3163 = _M0L7olengthS1200 - _M0L1iS1226;
            int32_t _M0L6_2atmpS3161 = _M0L6_2atmpS3163 - 1;
            int32_t _M0L6_2atmpS3162 = _M0Lm3expS1201;
            int32_t _M0L7currentS1229;
            int32_t _M0L6_2atmpS3158;
            int32_t _M0L6_2atmpS3157;
            int32_t _M0L6_2atmpS3152;
            uint64_t _M0L6_2atmpS3156;
            int32_t _M0L6_2atmpS3155;
            int32_t _M0L6_2atmpS3154;
            int32_t _M0L6_2atmpS3153;
            int32_t _M0L6_2atmpS3159;
            uint64_t _M0L6_2atmpS3160;
            if (_M0L6_2atmpS3161 == _M0L6_2atmpS3162) {
              int32_t _M0L6_2atmpS3166 =
                _M0L7currentS1227 + _M0L7olengthS1200;
              int32_t _M0L6_2atmpS3165 = _M0L6_2atmpS3166 - _M0L1iS1226;
              int32_t _M0L6_2atmpS3164 = _M0L6_2atmpS3165 - 1;
              if (
                _M0L6_2atmpS3164 < 0
                || _M0L6_2atmpS3164 >= Moonbit_array_length(_M0L6resultS1195)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1195[_M0L6_2atmpS3164] = 46;
              _M0L7currentS1229 = _M0L7currentS1227 - 1;
            } else {
              _M0L7currentS1229 = _M0L7currentS1227;
            }
            _M0L6_2atmpS3158 = _M0L7currentS1229 + _M0L7olengthS1200;
            _M0L6_2atmpS3157 = _M0L6_2atmpS3158 - _M0L1iS1226;
            _M0L6_2atmpS3152 = _M0L6_2atmpS3157 - 1;
            _M0L6_2atmpS3156 = _M0L6outputS1228 % 10ull;
            _M0L6_2atmpS3155 = (int32_t)_M0L6_2atmpS3156;
            _M0L6_2atmpS3154 = 48 + _M0L6_2atmpS3155;
            _M0L6_2atmpS3153 = _M0L6_2atmpS3154 & 0xff;
            if (
              _M0L6_2atmpS3152 < 0
              || _M0L6_2atmpS3152 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3152] = _M0L6_2atmpS3153;
            _M0L6_2atmpS3159 = _M0L1iS1226 + 1;
            _M0L6_2atmpS3160 = _M0L6outputS1228 / 10ull;
            _M0L1iS1226 = _M0L6_2atmpS3159;
            _M0L7currentS1227 = _M0L7currentS1229;
            _M0L6outputS1228 = _M0L6_2atmpS3160;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3169 = _M0Lm5indexS1196;
        _M0L6_2atmpS3170 = _M0L7olengthS1200 + 1;
        _M0Lm5indexS1196 = _M0L6_2atmpS3169 + _M0L6_2atmpS3170;
      }
    }
    _M0L6_2atmpS3171 = _M0Lm5indexS1196;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4000
    = _M0FPB19string__from__bytes(_M0L6resultS1195, 0, _M0L6_2atmpS3171);
    moonbit_decref(_M0L6resultS1195);
    return _result_4000;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1141,
  uint32_t _M0L12ieeeExponentS1140
) {
  int32_t _M0Lm2e2S1138;
  uint64_t _M0Lm2m2S1139;
  uint64_t _M0L6_2atmpS3045;
  uint64_t _M0L6_2atmpS3044;
  int32_t _M0L4evenS1142;
  uint64_t _M0L6_2atmpS3043;
  uint64_t _M0L2mvS1143;
  int32_t _M0L7mmShiftS1144;
  uint64_t _M0Lm2vrS1145;
  uint64_t _M0Lm2vpS1146;
  uint64_t _M0Lm2vmS1147;
  int32_t _M0Lm3e10S1148;
  int32_t _M0Lm17vmIsTrailingZerosS1149;
  int32_t _M0Lm17vrIsTrailingZerosS1150;
  int32_t _M0L6_2atmpS2945;
  int32_t _M0Lm7removedS1169;
  int32_t _M0Lm16lastRemovedDigitS1170;
  uint64_t _M0Lm6outputS1171;
  int32_t _M0L6_2atmpS3041;
  int32_t _M0L6_2atmpS3042;
  int32_t _M0L3expS1194;
  uint64_t _M0L6_2atmpS3040;
  struct _M0TPB17FloatingDecimal64* _block_4006;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1138 = 0;
  _M0Lm2m2S1139 = 0ull;
  if (_M0L12ieeeExponentS1140 == 0u) {
    _M0Lm2e2S1138 = -1076;
    _M0Lm2m2S1139 = _M0L12ieeeMantissaS1141;
  } else {
    int32_t _M0L6_2atmpS2944 = *(int32_t*)&_M0L12ieeeExponentS1140;
    int32_t _M0L6_2atmpS2943 = _M0L6_2atmpS2944 - 1023;
    int32_t _M0L6_2atmpS2942 = _M0L6_2atmpS2943 - 52;
    _M0Lm2e2S1138 = _M0L6_2atmpS2942 - 2;
    _M0Lm2m2S1139 = 4503599627370496ull | _M0L12ieeeMantissaS1141;
  }
  _M0L6_2atmpS3045 = _M0Lm2m2S1139;
  _M0L6_2atmpS3044 = _M0L6_2atmpS3045 & 1ull;
  _M0L4evenS1142 = _M0L6_2atmpS3044 == 0ull;
  _M0L6_2atmpS3043 = _M0Lm2m2S1139;
  _M0L2mvS1143 = 4ull * _M0L6_2atmpS3043;
  if (_M0L12ieeeMantissaS1141 != 0ull) {
    _M0L7mmShiftS1144 = 1;
  } else {
    _M0L7mmShiftS1144 = _M0L12ieeeExponentS1140 <= 1u;
  }
  _M0Lm2vrS1145 = 0ull;
  _M0Lm2vpS1146 = 0ull;
  _M0Lm2vmS1147 = 0ull;
  _M0Lm3e10S1148 = 0;
  _M0Lm17vmIsTrailingZerosS1149 = 0;
  _M0Lm17vrIsTrailingZerosS1150 = 0;
  _M0L6_2atmpS2945 = _M0Lm2e2S1138;
  if (_M0L6_2atmpS2945 >= 0) {
    int32_t _M0L6_2atmpS2967 = _M0Lm2e2S1138;
    int32_t _M0L6_2atmpS2963;
    int32_t _M0L6_2atmpS2966;
    int32_t _M0L6_2atmpS2965;
    int32_t _M0L6_2atmpS2964;
    int32_t _M0L1qS1151;
    int32_t _M0L6_2atmpS2962;
    int32_t _M0L6_2atmpS2961;
    int32_t _M0L1kS1152;
    int32_t _M0L6_2atmpS2960;
    int32_t _M0L6_2atmpS2959;
    int32_t _M0L6_2atmpS2958;
    int32_t _M0L1iS1153;
    struct _M0TPB8Pow5Pair _M0L4pow5S1154;
    uint64_t _M0L6_2atmpS2957;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1155;
    uint64_t _M0L8_2avrOutS1156;
    uint64_t _M0L8_2avpOutS1157;
    uint64_t _M0L8_2avmOutS1158;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2963 = _M0FPB9log10Pow2(_M0L6_2atmpS2967);
    _M0L6_2atmpS2966 = _M0Lm2e2S1138;
    _M0L6_2atmpS2965 = _M0L6_2atmpS2966 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2964 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2965);
    _M0L1qS1151 = _M0L6_2atmpS2963 - _M0L6_2atmpS2964;
    _M0Lm3e10S1148 = _M0L1qS1151;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2962 = _M0FPB8pow5bits(_M0L1qS1151);
    _M0L6_2atmpS2961 = 125 + _M0L6_2atmpS2962;
    _M0L1kS1152 = _M0L6_2atmpS2961 - 1;
    _M0L6_2atmpS2960 = _M0Lm2e2S1138;
    _M0L6_2atmpS2959 = -_M0L6_2atmpS2960;
    _M0L6_2atmpS2958 = _M0L6_2atmpS2959 + _M0L1qS1151;
    _M0L1iS1153 = _M0L6_2atmpS2958 + _M0L1kS1152;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1154 = _M0FPB22double__computeInvPow5(_M0L1qS1151);
    _M0L6_2atmpS2957 = _M0Lm2m2S1139;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1155
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2957, _M0L4pow5S1154, _M0L1iS1153, _M0L7mmShiftS1144);
    _M0L8_2avrOutS1156 = _M0L7_2abindS1155.$0;
    _M0L8_2avpOutS1157 = _M0L7_2abindS1155.$1;
    _M0L8_2avmOutS1158 = _M0L7_2abindS1155.$2;
    _M0Lm2vrS1145 = _M0L8_2avrOutS1156;
    _M0Lm2vpS1146 = _M0L8_2avpOutS1157;
    _M0Lm2vmS1147 = _M0L8_2avmOutS1158;
    if (_M0L1qS1151 <= 21) {
      int32_t _M0L6_2atmpS2953 = (int32_t)_M0L2mvS1143;
      uint64_t _M0L6_2atmpS2956 = _M0L2mvS1143 / 5ull;
      int32_t _M0L6_2atmpS2955 = (int32_t)_M0L6_2atmpS2956;
      int32_t _M0L6_2atmpS2954 = 5 * _M0L6_2atmpS2955;
      int32_t _M0L6mvMod5S1159 = _M0L6_2atmpS2953 - _M0L6_2atmpS2954;
      if (_M0L6mvMod5S1159 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1150
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1143, _M0L1qS1151);
      } else if (_M0L4evenS1142) {
        uint64_t _M0L6_2atmpS2947 = _M0L2mvS1143 - 1ull;
        uint64_t _M0L6_2atmpS2948;
        uint64_t _M0L6_2atmpS2946;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2948 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1144);
        _M0L6_2atmpS2946 = _M0L6_2atmpS2947 - _M0L6_2atmpS2948;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1149
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2946, _M0L1qS1151);
      } else {
        uint64_t _M0L6_2atmpS2949 = _M0Lm2vpS1146;
        uint64_t _M0L6_2atmpS2952 = _M0L2mvS1143 + 2ull;
        int32_t _M0L6_2atmpS2951;
        uint64_t _M0L6_2atmpS2950;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2951
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2952, _M0L1qS1151);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2950 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2951);
        _M0Lm2vpS1146 = _M0L6_2atmpS2949 - _M0L6_2atmpS2950;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2981 = _M0Lm2e2S1138;
    int32_t _M0L6_2atmpS2980 = -_M0L6_2atmpS2981;
    int32_t _M0L6_2atmpS2975;
    int32_t _M0L6_2atmpS2979;
    int32_t _M0L6_2atmpS2978;
    int32_t _M0L6_2atmpS2977;
    int32_t _M0L6_2atmpS2976;
    int32_t _M0L1qS1160;
    int32_t _M0L6_2atmpS2968;
    int32_t _M0L6_2atmpS2974;
    int32_t _M0L6_2atmpS2973;
    int32_t _M0L1iS1161;
    int32_t _M0L6_2atmpS2972;
    int32_t _M0L1kS1162;
    int32_t _M0L1jS1163;
    struct _M0TPB8Pow5Pair _M0L4pow5S1164;
    uint64_t _M0L6_2atmpS2971;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1165;
    uint64_t _M0L8_2avrOutS1166;
    uint64_t _M0L8_2avpOutS1167;
    uint64_t _M0L8_2avmOutS1168;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2975 = _M0FPB9log10Pow5(_M0L6_2atmpS2980);
    _M0L6_2atmpS2979 = _M0Lm2e2S1138;
    _M0L6_2atmpS2978 = -_M0L6_2atmpS2979;
    _M0L6_2atmpS2977 = _M0L6_2atmpS2978 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2976 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2977);
    _M0L1qS1160 = _M0L6_2atmpS2975 - _M0L6_2atmpS2976;
    _M0L6_2atmpS2968 = _M0Lm2e2S1138;
    _M0Lm3e10S1148 = _M0L1qS1160 + _M0L6_2atmpS2968;
    _M0L6_2atmpS2974 = _M0Lm2e2S1138;
    _M0L6_2atmpS2973 = -_M0L6_2atmpS2974;
    _M0L1iS1161 = _M0L6_2atmpS2973 - _M0L1qS1160;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2972 = _M0FPB8pow5bits(_M0L1iS1161);
    _M0L1kS1162 = _M0L6_2atmpS2972 - 125;
    _M0L1jS1163 = _M0L1qS1160 - _M0L1kS1162;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1164 = _M0FPB19double__computePow5(_M0L1iS1161);
    _M0L6_2atmpS2971 = _M0Lm2m2S1139;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1165
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2971, _M0L4pow5S1164, _M0L1jS1163, _M0L7mmShiftS1144);
    _M0L8_2avrOutS1166 = _M0L7_2abindS1165.$0;
    _M0L8_2avpOutS1167 = _M0L7_2abindS1165.$1;
    _M0L8_2avmOutS1168 = _M0L7_2abindS1165.$2;
    _M0Lm2vrS1145 = _M0L8_2avrOutS1166;
    _M0Lm2vpS1146 = _M0L8_2avpOutS1167;
    _M0Lm2vmS1147 = _M0L8_2avmOutS1168;
    if (_M0L1qS1160 <= 1) {
      _M0Lm17vrIsTrailingZerosS1150 = 1;
      if (_M0L4evenS1142) {
        int32_t _M0L6_2atmpS2969;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2969 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1144);
        _M0Lm17vmIsTrailingZerosS1149 = _M0L6_2atmpS2969 == 1;
      } else {
        uint64_t _M0L6_2atmpS2970 = _M0Lm2vpS1146;
        _M0Lm2vpS1146 = _M0L6_2atmpS2970 - 1ull;
      }
    } else if (_M0L1qS1160 < 63) {
      #line 437 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1150
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1143, _M0L1qS1160);
    }
  }
  _M0Lm7removedS1169 = 0;
  _M0Lm16lastRemovedDigitS1170 = 0;
  _M0Lm6outputS1171 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1149 || _M0Lm17vrIsTrailingZerosS1150) {
    int32_t _if__result_4003;
    uint64_t _M0L6_2atmpS3011;
    uint64_t _M0L6_2atmpS3017;
    uint64_t _M0L6_2atmpS3018;
    int32_t _if__result_4004;
    int32_t _M0L6_2atmpS3014;
    int64_t _M0L6_2atmpS3013;
    uint64_t _M0L6_2atmpS3012;
    while (1) {
      uint64_t _M0L6_2atmpS2994 = _M0Lm2vpS1146;
      uint64_t _M0L7vpDiv10S1172 = _M0L6_2atmpS2994 / 10ull;
      uint64_t _M0L6_2atmpS2993 = _M0Lm2vmS1147;
      uint64_t _M0L7vmDiv10S1173 = _M0L6_2atmpS2993 / 10ull;
      uint64_t _M0L6_2atmpS2992;
      int32_t _M0L6_2atmpS2989;
      int32_t _M0L6_2atmpS2991;
      int32_t _M0L6_2atmpS2990;
      int32_t _M0L7vmMod10S1175;
      uint64_t _M0L6_2atmpS2988;
      uint64_t _M0L7vrDiv10S1176;
      uint64_t _M0L6_2atmpS2987;
      int32_t _M0L6_2atmpS2984;
      int32_t _M0L6_2atmpS2986;
      int32_t _M0L6_2atmpS2985;
      int32_t _M0L7vrMod10S1177;
      int32_t _M0L6_2atmpS2983;
      if (_M0L7vpDiv10S1172 <= _M0L7vmDiv10S1173) {
        break;
      }
      _M0L6_2atmpS2992 = _M0Lm2vmS1147;
      _M0L6_2atmpS2989 = (int32_t)_M0L6_2atmpS2992;
      _M0L6_2atmpS2991 = (int32_t)_M0L7vmDiv10S1173;
      _M0L6_2atmpS2990 = 10 * _M0L6_2atmpS2991;
      _M0L7vmMod10S1175 = _M0L6_2atmpS2989 - _M0L6_2atmpS2990;
      _M0L6_2atmpS2988 = _M0Lm2vrS1145;
      _M0L7vrDiv10S1176 = _M0L6_2atmpS2988 / 10ull;
      _M0L6_2atmpS2987 = _M0Lm2vrS1145;
      _M0L6_2atmpS2984 = (int32_t)_M0L6_2atmpS2987;
      _M0L6_2atmpS2986 = (int32_t)_M0L7vrDiv10S1176;
      _M0L6_2atmpS2985 = 10 * _M0L6_2atmpS2986;
      _M0L7vrMod10S1177 = _M0L6_2atmpS2984 - _M0L6_2atmpS2985;
      if (_M0Lm17vmIsTrailingZerosS1149) {
        _M0Lm17vmIsTrailingZerosS1149 = _M0L7vmMod10S1175 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1149 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1150) {
        int32_t _M0L6_2atmpS2982 = _M0Lm16lastRemovedDigitS1170;
        _M0Lm17vrIsTrailingZerosS1150 = _M0L6_2atmpS2982 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1150 = 0;
      }
      _M0Lm16lastRemovedDigitS1170 = _M0L7vrMod10S1177;
      _M0Lm2vrS1145 = _M0L7vrDiv10S1176;
      _M0Lm2vpS1146 = _M0L7vpDiv10S1172;
      _M0Lm2vmS1147 = _M0L7vmDiv10S1173;
      _M0L6_2atmpS2983 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS2983 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1149) {
      while (1) {
        uint64_t _M0L6_2atmpS3007 = _M0Lm2vmS1147;
        uint64_t _M0L7vmDiv10S1178 = _M0L6_2atmpS3007 / 10ull;
        uint64_t _M0L6_2atmpS3006 = _M0Lm2vmS1147;
        int32_t _M0L6_2atmpS3003 = (int32_t)_M0L6_2atmpS3006;
        int32_t _M0L6_2atmpS3005 = (int32_t)_M0L7vmDiv10S1178;
        int32_t _M0L6_2atmpS3004 = 10 * _M0L6_2atmpS3005;
        int32_t _M0L7vmMod10S1179 = _M0L6_2atmpS3003 - _M0L6_2atmpS3004;
        uint64_t _M0L6_2atmpS3002;
        uint64_t _M0L7vpDiv10S1181;
        uint64_t _M0L6_2atmpS3001;
        uint64_t _M0L7vrDiv10S1182;
        uint64_t _M0L6_2atmpS3000;
        int32_t _M0L6_2atmpS2997;
        int32_t _M0L6_2atmpS2999;
        int32_t _M0L6_2atmpS2998;
        int32_t _M0L7vrMod10S1183;
        int32_t _M0L6_2atmpS2996;
        if (_M0L7vmMod10S1179 != 0) {
          break;
        }
        _M0L6_2atmpS3002 = _M0Lm2vpS1146;
        _M0L7vpDiv10S1181 = _M0L6_2atmpS3002 / 10ull;
        _M0L6_2atmpS3001 = _M0Lm2vrS1145;
        _M0L7vrDiv10S1182 = _M0L6_2atmpS3001 / 10ull;
        _M0L6_2atmpS3000 = _M0Lm2vrS1145;
        _M0L6_2atmpS2997 = (int32_t)_M0L6_2atmpS3000;
        _M0L6_2atmpS2999 = (int32_t)_M0L7vrDiv10S1182;
        _M0L6_2atmpS2998 = 10 * _M0L6_2atmpS2999;
        _M0L7vrMod10S1183 = _M0L6_2atmpS2997 - _M0L6_2atmpS2998;
        if (_M0Lm17vrIsTrailingZerosS1150) {
          int32_t _M0L6_2atmpS2995 = _M0Lm16lastRemovedDigitS1170;
          _M0Lm17vrIsTrailingZerosS1150 = _M0L6_2atmpS2995 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1150 = 0;
        }
        _M0Lm16lastRemovedDigitS1170 = _M0L7vrMod10S1183;
        _M0Lm2vrS1145 = _M0L7vrDiv10S1182;
        _M0Lm2vpS1146 = _M0L7vpDiv10S1181;
        _M0Lm2vmS1147 = _M0L7vmDiv10S1178;
        _M0L6_2atmpS2996 = _M0Lm7removedS1169;
        _M0Lm7removedS1169 = _M0L6_2atmpS2996 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1150) {
      int32_t _M0L6_2atmpS3010 = _M0Lm16lastRemovedDigitS1170;
      if (_M0L6_2atmpS3010 == 5) {
        uint64_t _M0L6_2atmpS3009 = _M0Lm2vrS1145;
        uint64_t _M0L6_2atmpS3008 = _M0L6_2atmpS3009 % 2ull;
        _if__result_4003 = _M0L6_2atmpS3008 == 0ull;
      } else {
        _if__result_4003 = 0;
      }
    } else {
      _if__result_4003 = 0;
    }
    if (_if__result_4003) {
      _M0Lm16lastRemovedDigitS1170 = 4;
    }
    _M0L6_2atmpS3011 = _M0Lm2vrS1145;
    _M0L6_2atmpS3017 = _M0Lm2vrS1145;
    _M0L6_2atmpS3018 = _M0Lm2vmS1147;
    if (_M0L6_2atmpS3017 == _M0L6_2atmpS3018) {
      if (!_M0L4evenS1142) {
        _if__result_4004 = 1;
      } else {
        int32_t _M0L6_2atmpS3016 = _M0Lm17vmIsTrailingZerosS1149;
        _if__result_4004 = !_M0L6_2atmpS3016;
      }
    } else {
      _if__result_4004 = 0;
    }
    if (_if__result_4004) {
      _M0L6_2atmpS3014 = 1;
    } else {
      int32_t _M0L6_2atmpS3015 = _M0Lm16lastRemovedDigitS1170;
      _M0L6_2atmpS3014 = _M0L6_2atmpS3015 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3013 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3014);
    _M0L6_2atmpS3012 = *(uint64_t*)&_M0L6_2atmpS3013;
    _M0Lm6outputS1171 = _M0L6_2atmpS3011 + _M0L6_2atmpS3012;
  } else {
    int32_t _M0Lm7roundUpS1184 = 0;
    uint64_t _M0L6_2atmpS3039 = _M0Lm2vpS1146;
    uint64_t _M0L8vpDiv100S1185 = _M0L6_2atmpS3039 / 100ull;
    uint64_t _M0L6_2atmpS3038 = _M0Lm2vmS1147;
    uint64_t _M0L8vmDiv100S1186 = _M0L6_2atmpS3038 / 100ull;
    uint64_t _M0L6_2atmpS3033;
    uint64_t _M0L6_2atmpS3036;
    uint64_t _M0L6_2atmpS3037;
    int32_t _M0L6_2atmpS3035;
    uint64_t _M0L6_2atmpS3034;
    if (_M0L8vpDiv100S1185 > _M0L8vmDiv100S1186) {
      uint64_t _M0L6_2atmpS3024 = _M0Lm2vrS1145;
      uint64_t _M0L8vrDiv100S1187 = _M0L6_2atmpS3024 / 100ull;
      uint64_t _M0L6_2atmpS3023 = _M0Lm2vrS1145;
      int32_t _M0L6_2atmpS3020 = (int32_t)_M0L6_2atmpS3023;
      int32_t _M0L6_2atmpS3022 = (int32_t)_M0L8vrDiv100S1187;
      int32_t _M0L6_2atmpS3021 = 100 * _M0L6_2atmpS3022;
      int32_t _M0L8vrMod100S1188 = _M0L6_2atmpS3020 - _M0L6_2atmpS3021;
      int32_t _M0L6_2atmpS3019;
      _M0Lm7roundUpS1184 = _M0L8vrMod100S1188 >= 50;
      _M0Lm2vrS1145 = _M0L8vrDiv100S1187;
      _M0Lm2vpS1146 = _M0L8vpDiv100S1185;
      _M0Lm2vmS1147 = _M0L8vmDiv100S1186;
      _M0L6_2atmpS3019 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS3019 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3032 = _M0Lm2vpS1146;
      uint64_t _M0L7vpDiv10S1189 = _M0L6_2atmpS3032 / 10ull;
      uint64_t _M0L6_2atmpS3031 = _M0Lm2vmS1147;
      uint64_t _M0L7vmDiv10S1190 = _M0L6_2atmpS3031 / 10ull;
      uint64_t _M0L6_2atmpS3030;
      uint64_t _M0L7vrDiv10S1192;
      uint64_t _M0L6_2atmpS3029;
      int32_t _M0L6_2atmpS3026;
      int32_t _M0L6_2atmpS3028;
      int32_t _M0L6_2atmpS3027;
      int32_t _M0L7vrMod10S1193;
      int32_t _M0L6_2atmpS3025;
      if (_M0L7vpDiv10S1189 <= _M0L7vmDiv10S1190) {
        break;
      }
      _M0L6_2atmpS3030 = _M0Lm2vrS1145;
      _M0L7vrDiv10S1192 = _M0L6_2atmpS3030 / 10ull;
      _M0L6_2atmpS3029 = _M0Lm2vrS1145;
      _M0L6_2atmpS3026 = (int32_t)_M0L6_2atmpS3029;
      _M0L6_2atmpS3028 = (int32_t)_M0L7vrDiv10S1192;
      _M0L6_2atmpS3027 = 10 * _M0L6_2atmpS3028;
      _M0L7vrMod10S1193 = _M0L6_2atmpS3026 - _M0L6_2atmpS3027;
      _M0Lm7roundUpS1184 = _M0L7vrMod10S1193 >= 5;
      _M0Lm2vrS1145 = _M0L7vrDiv10S1192;
      _M0Lm2vpS1146 = _M0L7vpDiv10S1189;
      _M0Lm2vmS1147 = _M0L7vmDiv10S1190;
      _M0L6_2atmpS3025 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS3025 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3033 = _M0Lm2vrS1145;
    _M0L6_2atmpS3036 = _M0Lm2vrS1145;
    _M0L6_2atmpS3037 = _M0Lm2vmS1147;
    _M0L6_2atmpS3035
    = _M0L6_2atmpS3036 == _M0L6_2atmpS3037 || _M0Lm7roundUpS1184;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3034 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3035);
    _M0Lm6outputS1171 = _M0L6_2atmpS3033 + _M0L6_2atmpS3034;
  }
  _M0L6_2atmpS3041 = _M0Lm3e10S1148;
  _M0L6_2atmpS3042 = _M0Lm7removedS1169;
  _M0L3expS1194 = _M0L6_2atmpS3041 + _M0L6_2atmpS3042;
  _M0L6_2atmpS3040 = _M0Lm6outputS1171;
  _block_4006
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4006)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_4006->$0 = _M0L6_2atmpS3040;
  _block_4006->$1 = _M0L3expS1194;
  return _block_4006;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1137) {
  #line 110 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1137) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1136) {
  #line 58 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1136) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1135) {
  #line 32 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1135) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1134) {
  #line 280 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L1vS1134 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1134 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1134 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1134 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1134 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1134 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1134 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1134 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1134 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1134 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1134 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1134 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1134 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1134 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1134 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1134 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1117) {
  int32_t _M0L6_2atmpS2941;
  int32_t _M0L6_2atmpS2940;
  int32_t _M0L4baseS1116;
  int32_t _M0L5base2S1118;
  int32_t _M0L6offsetS1119;
  int32_t _M0L6_2atmpS2939;
  uint64_t _M0L4mul0S1120;
  int32_t _M0L6_2atmpS2938;
  int32_t _M0L6_2atmpS2937;
  uint64_t _M0L4mul1S1121;
  uint64_t _M0L1mS1122;
  struct _M0TPB7Umul128 _M0L7_2abindS1123;
  uint64_t _M0L7_2alow1S1124;
  uint64_t _M0L8_2ahigh1S1125;
  struct _M0TPB7Umul128 _M0L7_2abindS1126;
  uint64_t _M0L7_2alow0S1127;
  uint64_t _M0L8_2ahigh0S1128;
  uint64_t _M0L3sumS1129;
  uint64_t _M0Lm5high1S1130;
  int32_t _M0L6_2atmpS2935;
  int32_t _M0L6_2atmpS2936;
  int32_t _M0L5deltaS1131;
  uint64_t _M0L6_2atmpS2934;
  uint64_t _M0L6_2atmpS2926;
  int32_t _M0L6_2atmpS2933;
  uint32_t _M0L6_2atmpS2930;
  int32_t _M0L6_2atmpS2932;
  int32_t _M0L6_2atmpS2931;
  uint32_t _M0L6_2atmpS2929;
  uint32_t _M0L6_2atmpS2928;
  uint64_t _M0L6_2atmpS2927;
  uint64_t _M0L1aS1132;
  uint64_t _M0L6_2atmpS2925;
  uint64_t _M0L1bS1133;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2941 = _M0L1iS1117 + 26;
  _M0L6_2atmpS2940 = _M0L6_2atmpS2941 - 1;
  _M0L4baseS1116 = _M0L6_2atmpS2940 / 26;
  _M0L5base2S1118 = _M0L4baseS1116 * 26;
  _M0L6offsetS1119 = _M0L5base2S1118 - _M0L1iS1117;
  _M0L6_2atmpS2939 = _M0L4baseS1116 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1120
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2939);
  _M0L6_2atmpS2938 = _M0L4baseS1116 * 2;
  _M0L6_2atmpS2937 = _M0L6_2atmpS2938 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1121
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2937);
  if (_M0L6offsetS1119 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1120,
                                      .$1 = _M0L4mul1S1121};
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1122
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1119);
  #line 249 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1123 = _M0FPB7umul128(_M0L1mS1122, _M0L4mul1S1121);
  _M0L7_2alow1S1124 = _M0L7_2abindS1123.$0;
  _M0L8_2ahigh1S1125 = _M0L7_2abindS1123.$1;
  #line 250 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1126 = _M0FPB7umul128(_M0L1mS1122, _M0L4mul0S1120);
  _M0L7_2alow0S1127 = _M0L7_2abindS1126.$0;
  _M0L8_2ahigh0S1128 = _M0L7_2abindS1126.$1;
  _M0L3sumS1129 = _M0L8_2ahigh0S1128 + _M0L7_2alow1S1124;
  _M0Lm5high1S1130 = _M0L8_2ahigh1S1125;
  if (_M0L3sumS1129 < _M0L8_2ahigh0S1128) {
    uint64_t _M0L6_2atmpS2924 = _M0Lm5high1S1130;
    _M0Lm5high1S1130 = _M0L6_2atmpS2924 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2935 = _M0FPB8pow5bits(_M0L5base2S1118);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2936 = _M0FPB8pow5bits(_M0L1iS1117);
  _M0L5deltaS1131 = _M0L6_2atmpS2935 - _M0L6_2atmpS2936;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2934
  = _M0FPB13shiftright128(_M0L7_2alow0S1127, _M0L3sumS1129, _M0L5deltaS1131);
  _M0L6_2atmpS2926 = _M0L6_2atmpS2934 + 1ull;
  _M0L6_2atmpS2933 = _M0L1iS1117 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2930
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2933);
  _M0L6_2atmpS2932 = _M0L1iS1117 % 16;
  _M0L6_2atmpS2931 = _M0L6_2atmpS2932 << 1;
  _M0L6_2atmpS2929 = _M0L6_2atmpS2930 >> (_M0L6_2atmpS2931 & 31);
  _M0L6_2atmpS2928 = _M0L6_2atmpS2929 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2927 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2928);
  _M0L1aS1132 = _M0L6_2atmpS2926 + _M0L6_2atmpS2927;
  _M0L6_2atmpS2925 = _M0Lm5high1S1130;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1133
  = _M0FPB13shiftright128(_M0L3sumS1129, _M0L6_2atmpS2925, _M0L5deltaS1131);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1132, .$1 = _M0L1bS1133};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1099) {
  int32_t _M0L4baseS1098;
  int32_t _M0L5base2S1100;
  int32_t _M0L6offsetS1101;
  int32_t _M0L6_2atmpS2923;
  uint64_t _M0L4mul0S1102;
  int32_t _M0L6_2atmpS2922;
  int32_t _M0L6_2atmpS2921;
  uint64_t _M0L4mul1S1103;
  uint64_t _M0L1mS1104;
  struct _M0TPB7Umul128 _M0L7_2abindS1105;
  uint64_t _M0L7_2alow1S1106;
  uint64_t _M0L8_2ahigh1S1107;
  struct _M0TPB7Umul128 _M0L7_2abindS1108;
  uint64_t _M0L7_2alow0S1109;
  uint64_t _M0L8_2ahigh0S1110;
  uint64_t _M0L3sumS1111;
  uint64_t _M0Lm5high1S1112;
  int32_t _M0L6_2atmpS2919;
  int32_t _M0L6_2atmpS2920;
  int32_t _M0L5deltaS1113;
  uint64_t _M0L6_2atmpS2911;
  int32_t _M0L6_2atmpS2918;
  uint32_t _M0L6_2atmpS2915;
  int32_t _M0L6_2atmpS2917;
  int32_t _M0L6_2atmpS2916;
  uint32_t _M0L6_2atmpS2914;
  uint32_t _M0L6_2atmpS2913;
  uint64_t _M0L6_2atmpS2912;
  uint64_t _M0L1aS1114;
  uint64_t _M0L6_2atmpS2910;
  uint64_t _M0L1bS1115;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1098 = _M0L1iS1099 / 26;
  _M0L5base2S1100 = _M0L4baseS1098 * 26;
  _M0L6offsetS1101 = _M0L1iS1099 - _M0L5base2S1100;
  _M0L6_2atmpS2923 = _M0L4baseS1098 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1102
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2923);
  _M0L6_2atmpS2922 = _M0L4baseS1098 * 2;
  _M0L6_2atmpS2921 = _M0L6_2atmpS2922 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1103
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2921);
  if (_M0L6offsetS1101 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1102,
                                      .$1 = _M0L4mul1S1103};
  }
  #line 222 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1104
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1101);
  #line 223 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1105 = _M0FPB7umul128(_M0L1mS1104, _M0L4mul1S1103);
  _M0L7_2alow1S1106 = _M0L7_2abindS1105.$0;
  _M0L8_2ahigh1S1107 = _M0L7_2abindS1105.$1;
  #line 224 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1108 = _M0FPB7umul128(_M0L1mS1104, _M0L4mul0S1102);
  _M0L7_2alow0S1109 = _M0L7_2abindS1108.$0;
  _M0L8_2ahigh0S1110 = _M0L7_2abindS1108.$1;
  _M0L3sumS1111 = _M0L8_2ahigh0S1110 + _M0L7_2alow1S1106;
  _M0Lm5high1S1112 = _M0L8_2ahigh1S1107;
  if (_M0L3sumS1111 < _M0L8_2ahigh0S1110) {
    uint64_t _M0L6_2atmpS2909 = _M0Lm5high1S1112;
    _M0Lm5high1S1112 = _M0L6_2atmpS2909 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2919 = _M0FPB8pow5bits(_M0L1iS1099);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2920 = _M0FPB8pow5bits(_M0L5base2S1100);
  _M0L5deltaS1113 = _M0L6_2atmpS2919 - _M0L6_2atmpS2920;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2911
  = _M0FPB13shiftright128(_M0L7_2alow0S1109, _M0L3sumS1111, _M0L5deltaS1113);
  _M0L6_2atmpS2918 = _M0L1iS1099 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2915
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2918);
  _M0L6_2atmpS2917 = _M0L1iS1099 % 16;
  _M0L6_2atmpS2916 = _M0L6_2atmpS2917 << 1;
  _M0L6_2atmpS2914 = _M0L6_2atmpS2915 >> (_M0L6_2atmpS2916 & 31);
  _M0L6_2atmpS2913 = _M0L6_2atmpS2914 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2912 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2913);
  _M0L1aS1114 = _M0L6_2atmpS2911 + _M0L6_2atmpS2912;
  _M0L6_2atmpS2910 = _M0Lm5high1S1112;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1115
  = _M0FPB13shiftright128(_M0L3sumS1111, _M0L6_2atmpS2910, _M0L5deltaS1113);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1114, .$1 = _M0L1bS1115};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1072,
  struct _M0TPB8Pow5Pair _M0L3mulS1069,
  int32_t _M0L1jS1085,
  int32_t _M0L7mmShiftS1087
) {
  uint64_t _M0L7_2amul0S1068;
  uint64_t _M0L7_2amul1S1070;
  uint64_t _M0L1mS1071;
  struct _M0TPB7Umul128 _M0L7_2abindS1073;
  uint64_t _M0L5_2aloS1074;
  uint64_t _M0L6_2atmpS1075;
  struct _M0TPB7Umul128 _M0L7_2abindS1076;
  uint64_t _M0L6_2alo2S1077;
  uint64_t _M0L6_2ahi2S1078;
  uint64_t _M0L3midS1079;
  uint64_t _M0L6_2atmpS2908;
  uint64_t _M0L2hiS1080;
  uint64_t _M0L3lo2S1081;
  uint64_t _M0L6_2atmpS2906;
  uint64_t _M0L6_2atmpS2907;
  uint64_t _M0L4mid2S1082;
  uint64_t _M0L6_2atmpS2905;
  uint64_t _M0L3hi2S1083;
  int32_t _M0L6_2atmpS2904;
  int32_t _M0L6_2atmpS2903;
  uint64_t _M0L2vpS1084;
  uint64_t _M0Lm2vmS1086;
  int32_t _M0L6_2atmpS2902;
  int32_t _M0L6_2atmpS2901;
  uint64_t _M0L2vrS1097;
  uint64_t _M0L6_2atmpS2900;
  #line 129 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2amul0S1068 = _M0L3mulS1069.$0;
  _M0L7_2amul1S1070 = _M0L3mulS1069.$1;
  _M0L1mS1071 = _M0L1mS1072 << 1;
  #line 137 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1073 = _M0FPB7umul128(_M0L1mS1071, _M0L7_2amul0S1068);
  _M0L5_2aloS1074 = _M0L7_2abindS1073.$0;
  _M0L6_2atmpS1075 = _M0L7_2abindS1073.$1;
  #line 138 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1076 = _M0FPB7umul128(_M0L1mS1071, _M0L7_2amul1S1070);
  _M0L6_2alo2S1077 = _M0L7_2abindS1076.$0;
  _M0L6_2ahi2S1078 = _M0L7_2abindS1076.$1;
  _M0L3midS1079 = _M0L6_2atmpS1075 + _M0L6_2alo2S1077;
  if (_M0L3midS1079 < _M0L6_2atmpS1075) {
    _M0L6_2atmpS2908 = 1ull;
  } else {
    _M0L6_2atmpS2908 = 0ull;
  }
  _M0L2hiS1080 = _M0L6_2ahi2S1078 + _M0L6_2atmpS2908;
  _M0L3lo2S1081 = _M0L5_2aloS1074 + _M0L7_2amul0S1068;
  _M0L6_2atmpS2906 = _M0L3midS1079 + _M0L7_2amul1S1070;
  if (_M0L3lo2S1081 < _M0L5_2aloS1074) {
    _M0L6_2atmpS2907 = 1ull;
  } else {
    _M0L6_2atmpS2907 = 0ull;
  }
  _M0L4mid2S1082 = _M0L6_2atmpS2906 + _M0L6_2atmpS2907;
  if (_M0L4mid2S1082 < _M0L3midS1079) {
    _M0L6_2atmpS2905 = 1ull;
  } else {
    _M0L6_2atmpS2905 = 0ull;
  }
  _M0L3hi2S1083 = _M0L2hiS1080 + _M0L6_2atmpS2905;
  _M0L6_2atmpS2904 = _M0L1jS1085 - 64;
  _M0L6_2atmpS2903 = _M0L6_2atmpS2904 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1084
  = _M0FPB13shiftright128(_M0L4mid2S1082, _M0L3hi2S1083, _M0L6_2atmpS2903);
  _M0Lm2vmS1086 = 0ull;
  if (_M0L7mmShiftS1087) {
    uint64_t _M0L3lo3S1088 = _M0L5_2aloS1074 - _M0L7_2amul0S1068;
    uint64_t _M0L6_2atmpS2890 = _M0L3midS1079 - _M0L7_2amul1S1070;
    uint64_t _M0L6_2atmpS2891;
    uint64_t _M0L4mid3S1089;
    uint64_t _M0L6_2atmpS2889;
    uint64_t _M0L3hi3S1090;
    int32_t _M0L6_2atmpS2888;
    int32_t _M0L6_2atmpS2887;
    if (_M0L5_2aloS1074 < _M0L3lo3S1088) {
      _M0L6_2atmpS2891 = 1ull;
    } else {
      _M0L6_2atmpS2891 = 0ull;
    }
    _M0L4mid3S1089 = _M0L6_2atmpS2890 - _M0L6_2atmpS2891;
    if (_M0L3midS1079 < _M0L4mid3S1089) {
      _M0L6_2atmpS2889 = 1ull;
    } else {
      _M0L6_2atmpS2889 = 0ull;
    }
    _M0L3hi3S1090 = _M0L2hiS1080 - _M0L6_2atmpS2889;
    _M0L6_2atmpS2888 = _M0L1jS1085 - 64;
    _M0L6_2atmpS2887 = _M0L6_2atmpS2888 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1086
    = _M0FPB13shiftright128(_M0L4mid3S1089, _M0L3hi3S1090, _M0L6_2atmpS2887);
  } else {
    uint64_t _M0L3lo3S1091 = _M0L5_2aloS1074 + _M0L5_2aloS1074;
    uint64_t _M0L6_2atmpS2898 = _M0L3midS1079 + _M0L3midS1079;
    uint64_t _M0L6_2atmpS2899;
    uint64_t _M0L4mid3S1092;
    uint64_t _M0L6_2atmpS2896;
    uint64_t _M0L6_2atmpS2897;
    uint64_t _M0L3hi3S1093;
    uint64_t _M0L3lo4S1094;
    uint64_t _M0L6_2atmpS2894;
    uint64_t _M0L6_2atmpS2895;
    uint64_t _M0L4mid4S1095;
    uint64_t _M0L6_2atmpS2893;
    uint64_t _M0L3hi4S1096;
    int32_t _M0L6_2atmpS2892;
    if (_M0L3lo3S1091 < _M0L5_2aloS1074) {
      _M0L6_2atmpS2899 = 1ull;
    } else {
      _M0L6_2atmpS2899 = 0ull;
    }
    _M0L4mid3S1092 = _M0L6_2atmpS2898 + _M0L6_2atmpS2899;
    _M0L6_2atmpS2896 = _M0L2hiS1080 + _M0L2hiS1080;
    if (_M0L4mid3S1092 < _M0L3midS1079) {
      _M0L6_2atmpS2897 = 1ull;
    } else {
      _M0L6_2atmpS2897 = 0ull;
    }
    _M0L3hi3S1093 = _M0L6_2atmpS2896 + _M0L6_2atmpS2897;
    _M0L3lo4S1094 = _M0L3lo3S1091 - _M0L7_2amul0S1068;
    _M0L6_2atmpS2894 = _M0L4mid3S1092 - _M0L7_2amul1S1070;
    if (_M0L3lo3S1091 < _M0L3lo4S1094) {
      _M0L6_2atmpS2895 = 1ull;
    } else {
      _M0L6_2atmpS2895 = 0ull;
    }
    _M0L4mid4S1095 = _M0L6_2atmpS2894 - _M0L6_2atmpS2895;
    if (_M0L4mid3S1092 < _M0L4mid4S1095) {
      _M0L6_2atmpS2893 = 1ull;
    } else {
      _M0L6_2atmpS2893 = 0ull;
    }
    _M0L3hi4S1096 = _M0L3hi3S1093 - _M0L6_2atmpS2893;
    _M0L6_2atmpS2892 = _M0L1jS1085 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1086
    = _M0FPB13shiftright128(_M0L4mid4S1095, _M0L3hi4S1096, _M0L6_2atmpS2892);
  }
  _M0L6_2atmpS2902 = _M0L1jS1085 - 64;
  _M0L6_2atmpS2901 = _M0L6_2atmpS2902 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1097
  = _M0FPB13shiftright128(_M0L3midS1079, _M0L2hiS1080, _M0L6_2atmpS2901);
  _M0L6_2atmpS2900 = _M0Lm2vmS1086;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1097,
                                                .$1 = _M0L2vpS1084,
                                                .$2 = _M0L6_2atmpS2900};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1066,
  int32_t _M0L1pS1067
) {
  uint64_t _M0L6_2atmpS2886;
  uint64_t _M0L6_2atmpS2885;
  uint64_t _M0L6_2atmpS2884;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2886 = 1ull << (_M0L1pS1067 & 63);
  _M0L6_2atmpS2885 = _M0L6_2atmpS2886 - 1ull;
  _M0L6_2atmpS2884 = _M0L5valueS1066 & _M0L6_2atmpS2885;
  return _M0L6_2atmpS2884 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1064,
  int32_t _M0L1pS1065
) {
  int32_t _M0L6_2atmpS2883;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2883 = _M0FPB10pow5Factor(_M0L5valueS1064);
  return _M0L6_2atmpS2883 >= _M0L1pS1065;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1059) {
  uint64_t _M0L6_2atmpS2874;
  uint64_t _M0L6_2atmpS2875;
  uint64_t _M0L6_2atmpS2876;
  uint64_t _M0L6_2atmpS2877;
  uint64_t _M0L6_2atmpS2882;
  int32_t _M0L5countS1060;
  uint64_t _M0L1vS1061;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2874 = _M0L5valueS1059 % 5ull;
  if (_M0L6_2atmpS2874 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2875 = _M0L5valueS1059 % 25ull;
  if (_M0L6_2atmpS2875 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2876 = _M0L5valueS1059 % 125ull;
  if (_M0L6_2atmpS2876 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2877 = _M0L5valueS1059 % 625ull;
  if (_M0L6_2atmpS2877 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS2882 = _M0L5valueS1059 / 625ull;
  _M0L5countS1060 = 4;
  _M0L1vS1061 = _M0L6_2atmpS2882;
  while (1) {
    if (_M0L1vS1061 > 0ull) {
      uint64_t _M0L6_2atmpS2878 = _M0L1vS1061 % 5ull;
      int32_t _M0L6_2atmpS2879;
      uint64_t _M0L6_2atmpS2880;
      if (_M0L6_2atmpS2878 != 0ull) {
        return _M0L5countS1060;
      }
      _M0L6_2atmpS2879 = _M0L5countS1060 + 1;
      _M0L6_2atmpS2880 = _M0L1vS1061 / 5ull;
      _M0L5countS1060 = _M0L6_2atmpS2879;
      _M0L1vS1061 = _M0L6_2atmpS2880;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1063;
      moonbit_string_t _M0L6_2atmpS2881;
      int32_t _result_4008;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1063
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1063, (moonbit_string_t)moonbit_string_literal_96.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1063, _M0L5valueS1059);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS2881
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1063);
      moonbit_decref(_M0L18_2astring__builderS1063);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_4008 = _M0FPC15abort5abortGiE(_M0L6_2atmpS2881);
      moonbit_decref(_M0L6_2atmpS2881);
      return _result_4008;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1058,
  uint64_t _M0L2hiS1056,
  int32_t _M0L4distS1057
) {
  int32_t _M0L6_2atmpS2873;
  uint64_t _M0L6_2atmpS2871;
  uint64_t _M0L6_2atmpS2872;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2873 = 64 - _M0L4distS1057;
  _M0L6_2atmpS2871 = _M0L2hiS1056 << (_M0L6_2atmpS2873 & 63);
  _M0L6_2atmpS2872 = _M0L2loS1058 >> (_M0L4distS1057 & 63);
  return _M0L6_2atmpS2871 | _M0L6_2atmpS2872;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS1046,
  uint64_t _M0L1bS1049
) {
  uint64_t _M0L3aLoS1045;
  uint64_t _M0L3aHiS1047;
  uint64_t _M0L3bLoS1048;
  uint64_t _M0L3bHiS1050;
  uint64_t _M0L1xS1051;
  uint64_t _M0L6_2atmpS2869;
  uint64_t _M0L6_2atmpS2870;
  uint64_t _M0L1yS1052;
  uint64_t _M0L6_2atmpS2867;
  uint64_t _M0L6_2atmpS2868;
  uint64_t _M0L1zS1053;
  uint64_t _M0L6_2atmpS2865;
  uint64_t _M0L6_2atmpS2866;
  uint64_t _M0L6_2atmpS2863;
  uint64_t _M0L6_2atmpS2864;
  uint64_t _M0L1wS1054;
  uint64_t _M0L2loS1055;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1045 = _M0L1aS1046 & 4294967295ull;
  _M0L3aHiS1047 = _M0L1aS1046 >> 32;
  _M0L3bLoS1048 = _M0L1bS1049 & 4294967295ull;
  _M0L3bHiS1050 = _M0L1bS1049 >> 32;
  _M0L1xS1051 = _M0L3aLoS1045 * _M0L3bLoS1048;
  _M0L6_2atmpS2869 = _M0L3aHiS1047 * _M0L3bLoS1048;
  _M0L6_2atmpS2870 = _M0L1xS1051 >> 32;
  _M0L1yS1052 = _M0L6_2atmpS2869 + _M0L6_2atmpS2870;
  _M0L6_2atmpS2867 = _M0L3aLoS1045 * _M0L3bHiS1050;
  _M0L6_2atmpS2868 = _M0L1yS1052 & 4294967295ull;
  _M0L1zS1053 = _M0L6_2atmpS2867 + _M0L6_2atmpS2868;
  _M0L6_2atmpS2865 = _M0L3aHiS1047 * _M0L3bHiS1050;
  _M0L6_2atmpS2866 = _M0L1yS1052 >> 32;
  _M0L6_2atmpS2863 = _M0L6_2atmpS2865 + _M0L6_2atmpS2866;
  _M0L6_2atmpS2864 = _M0L1zS1053 >> 32;
  _M0L1wS1054 = _M0L6_2atmpS2863 + _M0L6_2atmpS2864;
  _M0L2loS1055 = _M0L1aS1046 * _M0L1bS1049;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1055, .$1 = _M0L1wS1054};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1043,
  int32_t _M0L4fromS1040,
  int32_t _M0L2toS1039
) {
  int32_t _M0L3lenS1038;
  int32_t _M0L6_2atmpS2862;
  uint16_t* _M0L6bufferS1041;
  int32_t _M0L1iS1042;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1038 = _M0L2toS1039 - _M0L4fromS1040;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2862 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1041
  = (uint16_t*)moonbit_make_string(_M0L3lenS1038, _M0L6_2atmpS2862);
  _M0L1iS1042 = 0;
  while (1) {
    if (_M0L1iS1042 < _M0L3lenS1038) {
      int32_t _M0L6_2atmpS2860 = _M0L4fromS1040 + _M0L1iS1042;
      int32_t _M0L6_2atmpS2859;
      int32_t _M0L6_2atmpS2858;
      int32_t _M0L6_2atmpS2861;
      if (
        _M0L6_2atmpS2860 < 0
        || _M0L6_2atmpS2860 >= Moonbit_array_length(_M0L5bytesS1043)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2859 = (int32_t)_M0L5bytesS1043[_M0L6_2atmpS2860];
      _M0L6_2atmpS2858 = (uint16_t)_M0L6_2atmpS2859;
      if (
        _M0L1iS1042 < 0
        || _M0L1iS1042 >= Moonbit_array_length(_M0L6bufferS1041)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1041[_M0L1iS1042] = _M0L6_2atmpS2858;
      _M0L6_2atmpS2861 = _M0L1iS1042 + 1;
      _M0L1iS1042 = _M0L6_2atmpS2861;
      continue;
    }
    break;
  }
  return _M0L6bufferS1041;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1037) {
  int32_t _M0L6_2atmpS2857;
  uint32_t _M0L6_2atmpS2856;
  uint32_t _M0L6_2atmpS2855;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2857 = _M0L1eS1037 * 78913;
  _M0L6_2atmpS2856 = *(uint32_t*)&_M0L6_2atmpS2857;
  _M0L6_2atmpS2855 = _M0L6_2atmpS2856 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2855;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1036) {
  int32_t _M0L6_2atmpS2854;
  uint32_t _M0L6_2atmpS2853;
  uint32_t _M0L6_2atmpS2852;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2854 = _M0L1eS1036 * 732923;
  _M0L6_2atmpS2853 = *(uint32_t*)&_M0L6_2atmpS2854;
  _M0L6_2atmpS2852 = _M0L6_2atmpS2853 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2852;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1034,
  int32_t _M0L8exponentS1035,
  int32_t _M0L8mantissaS1032
) {
  moonbit_string_t _M0L1sS1033;
  moonbit_string_t _result_4011;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1032) {
    return (moonbit_string_t)moonbit_string_literal_97.data;
  }
  if (_M0L4signS1034) {
    _M0L1sS1033 = (moonbit_string_t)moonbit_string_literal_94.data;
  } else {
    _M0L1sS1033 = (moonbit_string_t)moonbit_string_literal_95.data;
  }
  if (_M0L8exponentS1035) {
    moonbit_string_t _result_4010;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4010
    = moonbit_add_string(_M0L1sS1033, (moonbit_string_t)moonbit_string_literal_98.data);
    moonbit_decref(_M0L1sS1033);
    return _result_4010;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4011
  = moonbit_add_string(_M0L1sS1033, (moonbit_string_t)moonbit_string_literal_99.data);
  moonbit_decref(_M0L1sS1033);
  return _result_4011;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1031) {
  int32_t _M0L6_2atmpS2851;
  uint32_t _M0L6_2atmpS2850;
  uint32_t _M0L6_2atmpS2849;
  int32_t _M0L6_2atmpS2848;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2851 = _M0L1eS1031 * 1217359;
  _M0L6_2atmpS2850 = *(uint32_t*)&_M0L6_2atmpS2851;
  _M0L6_2atmpS2849 = _M0L6_2atmpS2850 >> 19;
  _M0L6_2atmpS2848 = *(int32_t*)&_M0L6_2atmpS2849;
  return _M0L6_2atmpS2848 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1027) {
  int32_t _tmp_4012;
  uint32_t _M0L6_2atmpS2847;
  uint32_t _M0Lm3accS1025;
  int32_t _M0L7_2abindS1026;
  int32_t _M0L1iS1028;
  uint32_t _M0L6_2atmpS2846;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_4012 = 0;
  _M0L6_2atmpS2847 = *(uint32_t*)&_tmp_4012;
  _M0Lm3accS1025 = _M0L6_2atmpS2847 + 374761393u;
  _M0L7_2abindS1026 = Moonbit_array_length(_M0L4selfS1027);
  _M0L1iS1028 = 0;
  while (1) {
    if (_M0L1iS1028 < _M0L7_2abindS1026) {
      uint32_t _M0L6_2atmpS2841 = _M0Lm3accS1025;
      int32_t _M0L6_2atmpS2844;
      int32_t _M0L6_2atmpS2843;
      uint32_t _M0L1vS1029;
      uint32_t _M0L6_2atmpS2842;
      int32_t _M0L6_2atmpS2845;
      _M0Lm3accS1025 = _M0L6_2atmpS2841 + 4u;
      _M0L6_2atmpS2844 = _M0L4selfS1027[_M0L1iS1028];
      _M0L6_2atmpS2843 = (int32_t)_M0L6_2atmpS2844;
      _M0L1vS1029 = *(uint32_t*)&_M0L6_2atmpS2843;
      _M0L6_2atmpS2842 = _M0Lm3accS1025;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1025 = _M0FPB13consume4__acc(_M0L6_2atmpS2842, _M0L1vS1029);
      _M0L6_2atmpS2845 = _M0L1iS1028 + 1;
      _M0L1iS1028 = _M0L6_2atmpS2845;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2846 = _M0Lm3accS1025;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS2846);
}

struct _M0TUssE* _M0MPB5Iter24nextGssE(
  struct _M0TPB4IterGUssEE* _M0L4selfS1021
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUssEE(_M0L4selfS1021);
}

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS1022
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsbEE(_M0L4selfS1022);
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L4selfS1023
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L4selfS1023);
}

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS1024
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsfEE(_M0L4selfS1024);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(
  struct _M0TPB3MapGssE* _M0L4selfS1017
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGssE(_M0L4selfS1017);
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1018
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsbE(_M0L4selfS1018);
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1019
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS1019);
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1020
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsfE(_M0L4selfS1020);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(
  struct _M0TPB3MapGssE* _M0L4selfS974
) {
  struct _M0TPB5EntryGssE* _M0L4headS2810;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS973;
  int32_t _M0L3lenS975;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS976;
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__* _closure_4014;
  struct _M0TWEOUssE* _M0L6_2atmpS2801;
  int64_t _M0L6_2atmpS2802;
  struct _M0TPB4IterGUssEE* _result_4015;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2810 = _M0L4selfS974->$5;
  if (_M0L4headS2810) {
    moonbit_incref(_M0L4headS2810);
  }
  _M0L11curr__entryS973
  = (struct _M0TPB8MutLocalGORPB5EntryGssEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE));
  Moonbit_object_header(_M0L11curr__entryS973)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 35, 0);
  _M0L11curr__entryS973->$0 = _M0L4headS2810;
  _M0L3lenS975 = _M0L4selfS974->$1;
  _M0L9remainingS976
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS976)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS976->$0 = _M0L3lenS975;
  _closure_4014
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__*)moonbit_malloc(sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__));
  Moonbit_object_header(_closure_4014)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 38, 0);
  _closure_4014->code = &_M0MPB3Map4iterGssEC2803l711;
  _closure_4014->$0 = _M0L9remainingS976;
  _closure_4014->$1 = _M0L11curr__entryS973;
  _M0L6_2atmpS2801 = (struct _M0TWEOUssE*)_closure_4014;
  _M0L6_2atmpS2802 = (int64_t)_M0L3lenS975;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4015 = _M0MPB4Iter3newGUssEE(_M0L6_2atmpS2801, _M0L6_2atmpS2802);
  moonbit_decref(_M0L6_2atmpS2801);
  return _result_4015;
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS985
) {
  struct _M0TPB5EntryGsbE* _M0L4headS2820;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS984;
  int32_t _M0L3lenS986;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS987;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__* _closure_4016;
  struct _M0TWEOUsbE* _M0L6_2atmpS2811;
  int64_t _M0L6_2atmpS2812;
  struct _M0TPB4IterGUsbEE* _result_4017;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2820 = _M0L4selfS985->$5;
  if (_M0L4headS2820) {
    moonbit_incref(_M0L4headS2820);
  }
  _M0L11curr__entryS984
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS984)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 42, 0);
  _M0L11curr__entryS984->$0 = _M0L4headS2820;
  _M0L3lenS986 = _M0L4selfS985->$1;
  _M0L9remainingS987
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS987)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS987->$0 = _M0L3lenS986;
  _closure_4016
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__));
  Moonbit_object_header(_closure_4016)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 45, 0);
  _closure_4016->code = &_M0MPB3Map4iterGsbEC2813l711;
  _closure_4016->$0 = _M0L9remainingS987;
  _closure_4016->$1 = _M0L11curr__entryS984;
  _M0L6_2atmpS2811 = (struct _M0TWEOUsbE*)_closure_4016;
  _M0L6_2atmpS2812 = (int64_t)_M0L3lenS986;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4017 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS2811, _M0L6_2atmpS2812);
  moonbit_decref(_M0L6_2atmpS2811);
  return _result_4017;
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS996
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4headS2830;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L11curr__entryS995;
  int32_t _M0L3lenS997;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS998;
  struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__* _closure_4018;
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2821;
  int64_t _M0L6_2atmpS2822;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _result_4019;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2830 = _M0L4selfS996->$5;
  if (_M0L4headS2830) {
    moonbit_incref(_M0L4headS2830);
  }
  _M0L11curr__entryS995
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS995)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 49, 0);
  _M0L11curr__entryS995->$0 = _M0L4headS2830;
  _M0L3lenS997 = _M0L4selfS996->$1;
  _M0L9remainingS998
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS998)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS998->$0 = _M0L3lenS997;
  _closure_4018
  = (struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__*)moonbit_malloc(sizeof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__));
  Moonbit_object_header(_closure_4018)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 52, 0);
  _closure_4018->code
  = &_M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2823l711;
  _closure_4018->$0 = _M0L9remainingS998;
  _closure_4018->$1 = _M0L11curr__entryS995;
  _M0L6_2atmpS2821
  = (struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_closure_4018;
  _M0L6_2atmpS2822 = (int64_t)_M0L3lenS997;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4019
  = _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2821, _M0L6_2atmpS2822);
  moonbit_decref(_M0L6_2atmpS2821);
  return _result_4019;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1007
) {
  struct _M0TPB5EntryGsfE* _M0L4headS2840;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1006;
  int32_t _M0L3lenS1008;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1009;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__* _closure_4020;
  struct _M0TWEOUsfE* _M0L6_2atmpS2831;
  int64_t _M0L6_2atmpS2832;
  struct _M0TPB4IterGUsfEE* _result_4021;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2840 = _M0L4selfS1007->$5;
  if (_M0L4headS2840) {
    moonbit_incref(_M0L4headS2840);
  }
  _M0L11curr__entryS1006
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS1006)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 56, 0);
  _M0L11curr__entryS1006->$0 = _M0L4headS2840;
  _M0L3lenS1008 = _M0L4selfS1007->$1;
  _M0L9remainingS1009
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1009)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1009->$0 = _M0L3lenS1008;
  _closure_4020
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__));
  Moonbit_object_header(_closure_4020)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 59, 0);
  _closure_4020->code = &_M0MPB3Map4iterGsfEC2833l711;
  _closure_4020->$0 = _M0L9remainingS1009;
  _closure_4020->$1 = _M0L11curr__entryS1006;
  _M0L6_2atmpS2831 = (struct _M0TWEOUsfE*)_closure_4020;
  _M0L6_2atmpS2832 = (int64_t)_M0L3lenS1008;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4021 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS2831, _M0L6_2atmpS2832);
  moonbit_decref(_M0L6_2atmpS2831);
  return _result_4021;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2833l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS2834
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__* _M0L14_2acasted__envS2835;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1006;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1009;
  int32_t _M0L3valS2836;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2835
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2833__l711__*)_M0L6_2aenvS2834;
  _M0L11curr__entryS1006 = _M0L14_2acasted__envS2835->$1;
  _M0L9remainingS1009 = _M0L14_2acasted__envS2835->$0;
  _M0L3valS2836 = _M0L9remainingS1009->$0;
  if (_M0L3valS2836 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1011 = _M0L11curr__entryS1006->$0;
    if (_M0L7_2abindS1011 == 0) {
      goto join_1010;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1012 = _M0L7_2abindS1011;
      struct _M0TPB5EntryGsfE* _M0L4_2axS1013 = _M0L7_2aSomeS1012;
      moonbit_string_t _M0L6_2akeyS1014 = _M0L4_2axS1013->$4;
      float _M0L8_2avalueS1015 = _M0L4_2axS1013->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1016 = _M0L4_2axS1013->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3563 = _M0L11curr__entryS1006->$0;
      int32_t _M0L3valS2838;
      int32_t _M0L6_2atmpS2837;
      struct _M0TUsfE* _M0L8_2atupleS2839;
      if (_M0L7_2anextS1016) {
        moonbit_incref(_M0L7_2anextS1016);
      }
      moonbit_incref(_M0L6_2akeyS1014);
      if (_M0L6_2aoldS3563) {
        moonbit_decref(_M0L6_2aoldS3563);
      }
      _M0L11curr__entryS1006->$0 = _M0L7_2anextS1016;
      _M0L3valS2838 = _M0L9remainingS1009->$0;
      _M0L6_2atmpS2837 = _M0L3valS2838 - 1;
      _M0L9remainingS1009->$0 = _M0L6_2atmpS2837;
      _M0L8_2atupleS2839
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS2839)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
      _M0L8_2atupleS2839->$0 = _M0L6_2akeyS1014;
      _M0L8_2atupleS2839->$1 = _M0L8_2avalueS1015;
      return _M0L8_2atupleS2839;
    }
  } else {
    goto join_1010;
  }
  join_1010:;
  return 0;
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2823l711(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aenvS2824
) {
  struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__* _M0L14_2acasted__envS2825;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L11curr__entryS995;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS998;
  int32_t _M0L3valS2826;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2825
  = (struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2823__l711__*)_M0L6_2aenvS2824;
  _M0L11curr__entryS995 = _M0L14_2acasted__envS2825->$1;
  _M0L9remainingS998 = _M0L14_2acasted__envS2825->$0;
  _M0L3valS2826 = _M0L9remainingS998->$0;
  if (_M0L3valS2826 > 0) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1000 =
      _M0L11curr__entryS995->$0;
    if (_M0L7_2abindS1000 == 0) {
      goto join_999;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1001 =
        _M0L7_2abindS1000;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1002 =
        _M0L7_2aSomeS1001;
      moonbit_string_t _M0L6_2akeyS1003 = _M0L4_2axS1002->$4;
      void* _M0L8_2avalueS1004 = _M0L4_2axS1002->$5;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS1005 =
        _M0L4_2axS1002->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3567 =
        _M0L11curr__entryS995->$0;
      int32_t _M0L3valS2828;
      int32_t _M0L6_2atmpS2827;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2atupleS2829;
      if (_M0L7_2anextS1005) {
        moonbit_incref(_M0L7_2anextS1005);
      }
      moonbit_incref(_M0L8_2avalueS1004);
      moonbit_incref(_M0L6_2akeyS1003);
      if (_M0L6_2aoldS3567) {
        moonbit_decref(_M0L6_2aoldS3567);
      }
      _M0L11curr__entryS995->$0 = _M0L7_2anextS1005;
      _M0L3valS2828 = _M0L9remainingS998->$0;
      _M0L6_2atmpS2827 = _M0L3valS2828 - 1;
      _M0L9remainingS998->$0 = _M0L6_2atmpS2827;
      _M0L8_2atupleS2829
      = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS2829)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 63, 0);
      _M0L8_2atupleS2829->$0 = _M0L6_2akeyS1003;
      _M0L8_2atupleS2829->$1 = _M0L8_2avalueS1004;
      return _M0L8_2atupleS2829;
    }
  } else {
    goto join_999;
  }
  join_999:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2813l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS2814
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__* _M0L14_2acasted__envS2815;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS984;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS987;
  int32_t _M0L3valS2816;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2815
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2813__l711__*)_M0L6_2aenvS2814;
  _M0L11curr__entryS984 = _M0L14_2acasted__envS2815->$1;
  _M0L9remainingS987 = _M0L14_2acasted__envS2815->$0;
  _M0L3valS2816 = _M0L9remainingS987->$0;
  if (_M0L3valS2816 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS989 = _M0L11curr__entryS984->$0;
    if (_M0L7_2abindS989 == 0) {
      goto join_988;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS990 = _M0L7_2abindS989;
      struct _M0TPB5EntryGsbE* _M0L4_2axS991 = _M0L7_2aSomeS990;
      moonbit_string_t _M0L6_2akeyS992 = _M0L4_2axS991->$4;
      int32_t _M0L8_2avalueS993 = _M0L4_2axS991->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS994 = _M0L4_2axS991->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3572 = _M0L11curr__entryS984->$0;
      int32_t _M0L3valS2818;
      int32_t _M0L6_2atmpS2817;
      struct _M0TUsbE* _M0L8_2atupleS2819;
      if (_M0L7_2anextS994) {
        moonbit_incref(_M0L7_2anextS994);
      }
      moonbit_incref(_M0L6_2akeyS992);
      if (_M0L6_2aoldS3572) {
        moonbit_decref(_M0L6_2aoldS3572);
      }
      _M0L11curr__entryS984->$0 = _M0L7_2anextS994;
      _M0L3valS2818 = _M0L9remainingS987->$0;
      _M0L6_2atmpS2817 = _M0L3valS2818 - 1;
      _M0L9remainingS987->$0 = _M0L6_2atmpS2817;
      _M0L8_2atupleS2819
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS2819)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 67, 0);
      _M0L8_2atupleS2819->$0 = _M0L6_2akeyS992;
      _M0L8_2atupleS2819->$1 = _M0L8_2avalueS993;
      return _M0L8_2atupleS2819;
    }
  } else {
    goto join_988;
  }
  join_988:;
  return 0;
}

struct _M0TUssE* _M0MPB3Map4iterGssEC2803l711(
  struct _M0TWEOUssE* _M0L6_2aenvS2804
) {
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__* _M0L14_2acasted__envS2805;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS973;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS976;
  int32_t _M0L3valS2806;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2805
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2803__l711__*)_M0L6_2aenvS2804;
  _M0L11curr__entryS973 = _M0L14_2acasted__envS2805->$1;
  _M0L9remainingS976 = _M0L14_2acasted__envS2805->$0;
  _M0L3valS2806 = _M0L9remainingS976->$0;
  if (_M0L3valS2806 > 0) {
    struct _M0TPB5EntryGssE* _M0L7_2abindS978 = _M0L11curr__entryS973->$0;
    if (_M0L7_2abindS978 == 0) {
      goto join_977;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS979 = _M0L7_2abindS978;
      struct _M0TPB5EntryGssE* _M0L4_2axS980 = _M0L7_2aSomeS979;
      moonbit_string_t _M0L6_2akeyS981 = _M0L4_2axS980->$4;
      moonbit_string_t _M0L8_2avalueS982 = _M0L4_2axS980->$5;
      struct _M0TPB5EntryGssE* _M0L7_2anextS983 = _M0L4_2axS980->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3576 = _M0L11curr__entryS973->$0;
      int32_t _M0L3valS2808;
      int32_t _M0L6_2atmpS2807;
      struct _M0TUssE* _M0L8_2atupleS2809;
      if (_M0L7_2anextS983) {
        moonbit_incref(_M0L7_2anextS983);
      }
      moonbit_incref(_M0L8_2avalueS982);
      moonbit_incref(_M0L6_2akeyS981);
      if (_M0L6_2aoldS3576) {
        moonbit_decref(_M0L6_2aoldS3576);
      }
      _M0L11curr__entryS973->$0 = _M0L7_2anextS983;
      _M0L3valS2808 = _M0L9remainingS976->$0;
      _M0L6_2atmpS2807 = _M0L3valS2808 - 1;
      _M0L9remainingS976->$0 = _M0L6_2atmpS2807;
      _M0L8_2atupleS2809
      = (struct _M0TUssE*)moonbit_malloc(sizeof(struct _M0TUssE));
      Moonbit_object_header(_M0L8_2atupleS2809)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 70, 0);
      _M0L8_2atupleS2809->$0 = _M0L6_2akeyS981;
      _M0L8_2atupleS2809->$1 = _M0L8_2avalueS982;
      return _M0L8_2atupleS2809;
    }
  } else {
    goto join_977;
  }
  join_977:;
  return 0;
}

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE* _M0L4selfS970) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS970->$1;
}

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE* _M0L4selfS971) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS971->$1;
}

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE* _M0L4selfS972) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS972->$1;
}

int32_t _M0MPB3Map6removeGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS966,
  moonbit_string_t _M0L3keyS967
) {
  int32_t _M0L6_2atmpS2799;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2799 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS967);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS966, _M0L3keyS967, _M0L6_2atmpS2799);
  return 0;
}

int32_t _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS968,
  moonbit_string_t _M0L3keyS969
) {
  int32_t _M0L6_2atmpS2800;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2800 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS969);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS968, _M0L3keyS969, _M0L6_2atmpS2800);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS951,
  moonbit_string_t _M0L3keyS955,
  int32_t _M0L4hashS954
) {
  int32_t _M0L14capacity__maskS2786;
  int32_t _M0L6_2atmpS2785;
  int32_t _M0L1iS948;
  int32_t _M0L3idxS949;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2786 = _M0L4selfS951->$3;
  _M0L6_2atmpS2785 = _M0L4hashS954 & _M0L14capacity__maskS2786;
  _M0L1iS948 = 0;
  _M0L3idxS949 = _M0L6_2atmpS2785;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2784 = _M0L4selfS951->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS950;
    if (
      _M0L3idxS949 < 0
      || _M0L3idxS949 >= Moonbit_array_length(_M0L7entriesS2784)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS950
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2784[_M0L3idxS949];
    if (_M0L7_2abindS950 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS952 = _M0L7_2abindS950;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS953 = _M0L7_2aSomeS952;
      int32_t _M0L4hashS2776 = _M0L8_2aentryS953->$3;
      int32_t _if__result_4027;
      int32_t _M0L3pslS2779;
      int32_t _M0L6_2atmpS2780;
      int32_t _M0L6_2atmpS2782;
      int32_t _M0L14capacity__maskS2783;
      int32_t _M0L6_2atmpS2781;
      if (_M0L4hashS2776 == _M0L4hashS954) {
        moonbit_string_t _M0L3keyS2775 = _M0L8_2aentryS953->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4027
        = _M0L3keyS2775 == _M0L3keyS955
          || Moonbit_array_length(_M0L3keyS2775)
             == Moonbit_array_length(_M0L3keyS955)
             && 0
                == memcmp(_M0L3keyS2775, _M0L3keyS955, Moonbit_array_length(_M0L3keyS2775) * 2);
      } else {
        _if__result_4027 = 0;
      }
      if (_if__result_4027) {
        int32_t _M0L4sizeS2778;
        int32_t _M0L6_2atmpS2777;
        moonbit_incref(_M0L8_2aentryS953);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS951, _M0L8_2aentryS953);
        moonbit_decref(_M0L8_2aentryS953);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS951, _M0L3idxS949);
        _M0L4sizeS2778 = _M0L4selfS951->$1;
        _M0L6_2atmpS2777 = _M0L4sizeS2778 - 1;
        _M0L4selfS951->$1 = _M0L6_2atmpS2777;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS953);
      }
      _M0L3pslS2779 = _M0L8_2aentryS953->$2;
      moonbit_decref(_M0L8_2aentryS953);
      if (_M0L1iS948 > _M0L3pslS2779) {
        break;
      }
      _M0L6_2atmpS2780 = _M0L1iS948 + 1;
      _M0L6_2atmpS2782 = _M0L3idxS949 + 1;
      _M0L14capacity__maskS2783 = _M0L4selfS951->$3;
      _M0L6_2atmpS2781 = _M0L6_2atmpS2782 & _M0L14capacity__maskS2783;
      _M0L1iS948 = _M0L6_2atmpS2780;
      _M0L3idxS949 = _M0L6_2atmpS2781;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS960,
  moonbit_string_t _M0L3keyS964,
  int32_t _M0L4hashS963
) {
  int32_t _M0L14capacity__maskS2798;
  int32_t _M0L6_2atmpS2797;
  int32_t _M0L1iS957;
  int32_t _M0L3idxS958;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2798 = _M0L4selfS960->$3;
  _M0L6_2atmpS2797 = _M0L4hashS963 & _M0L14capacity__maskS2798;
  _M0L1iS957 = 0;
  _M0L3idxS958 = _M0L6_2atmpS2797;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2796 =
      _M0L4selfS960->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS959;
    if (
      _M0L3idxS958 < 0
      || _M0L3idxS958 >= Moonbit_array_length(_M0L7entriesS2796)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS959
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2796[
        _M0L3idxS958
      ];
    if (_M0L7_2abindS959 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS961 =
        _M0L7_2abindS959;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS962 =
        _M0L7_2aSomeS961;
      int32_t _M0L4hashS2788 = _M0L8_2aentryS962->$3;
      int32_t _if__result_4029;
      int32_t _M0L3pslS2791;
      int32_t _M0L6_2atmpS2792;
      int32_t _M0L6_2atmpS2794;
      int32_t _M0L14capacity__maskS2795;
      int32_t _M0L6_2atmpS2793;
      if (_M0L4hashS2788 == _M0L4hashS963) {
        moonbit_string_t _M0L3keyS2787 = _M0L8_2aentryS962->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4029
        = _M0L3keyS2787 == _M0L3keyS964
          || Moonbit_array_length(_M0L3keyS2787)
             == Moonbit_array_length(_M0L3keyS964)
             && 0
                == memcmp(_M0L3keyS2787, _M0L3keyS964, Moonbit_array_length(_M0L3keyS2787) * 2);
      } else {
        _if__result_4029 = 0;
      }
      if (_if__result_4029) {
        int32_t _M0L4sizeS2790;
        int32_t _M0L6_2atmpS2789;
        moonbit_incref(_M0L8_2aentryS962);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS960, _M0L8_2aentryS962);
        moonbit_decref(_M0L8_2aentryS962);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS960, _M0L3idxS958);
        _M0L4sizeS2790 = _M0L4selfS960->$1;
        _M0L6_2atmpS2789 = _M0L4sizeS2790 - 1;
        _M0L4selfS960->$1 = _M0L6_2atmpS2789;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS962);
      }
      _M0L3pslS2791 = _M0L8_2aentryS962->$2;
      moonbit_decref(_M0L8_2aentryS962);
      if (_M0L1iS957 > _M0L3pslS2791) {
        break;
      }
      _M0L6_2atmpS2792 = _M0L1iS957 + 1;
      _M0L6_2atmpS2794 = _M0L3idxS958 + 1;
      _M0L14capacity__maskS2795 = _M0L4selfS960->$3;
      _M0L6_2atmpS2793 = _M0L6_2atmpS2794 & _M0L14capacity__maskS2795;
      _M0L1iS957 = _M0L6_2atmpS2792;
      _M0L3idxS958 = _M0L6_2atmpS2793;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS930,
  int32_t _M0L3idxS937
) {
  int32_t _M0L3curS928;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS928 = _M0L3idxS937;
  _2afor_932:;
  while (1) {
    int32_t _M0L6_2atmpS2766 = _M0L3curS928 + 1;
    int32_t _M0L14capacity__maskS2767 = _M0L4selfS930->$3;
    int32_t _M0L4nextS929 = _M0L6_2atmpS2766 & _M0L14capacity__maskS2767;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2765 = _M0L4selfS930->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS933;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2761;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS2762;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS3587;
    int32_t _tmp_4032;
    if (
      _M0L4nextS929 < 0
      || _M0L4nextS929 >= Moonbit_array_length(_M0L7entriesS2765)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS933
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2765[_M0L4nextS929];
    if (_M0L7_2abindS933 == 0) {
      goto join_931;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS934 = _M0L7_2abindS933;
      struct _M0TPB5EntryGsiE* _M0L4_2axS935 = _M0L7_2aSomeS934;
      int32_t _M0L4_2axS936 = _M0L4_2axS935->$2;
      switch (_M0L4_2axS936) {
        case 0: {
          goto join_931;
          break;
        }
        default: {
          int32_t _M0L3pslS2764 = _M0L4_2axS935->$2;
          int32_t _M0L6_2atmpS2763 = _M0L3pslS2764 - 1;
          _M0L4_2axS935->$2 = _M0L6_2atmpS2763;
          moonbit_incref(_M0L4_2axS935);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsiE(_M0L4selfS930, _M0L4_2axS935, _M0L3curS928);
          moonbit_decref(_M0L4_2axS935);
          _M0L3curS928 = _M0L4nextS929;
          goto _2afor_932;
          break;
        }
      }
    }
    goto joinlet_4031;
    join_931:;
    _M0L7entriesS2761 = _M0L4selfS930->$0;
    _M0L6_2atmpS2762 = 0;
    if (
      _M0L3curS928 < 0
      || _M0L3curS928 >= Moonbit_array_length(_M0L7entriesS2761)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3587
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2761[_M0L3curS928];
    if (_M0L6_2aoldS3587) {
      moonbit_decref(_M0L6_2aoldS3587);
    }
    _M0L7entriesS2761[_M0L3curS928] = _M0L6_2atmpS2762;
    break;
    joinlet_4031:;
    _tmp_4032 = _M0L3curS928;
    _M0L3curS928 = _tmp_4032;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS940,
  int32_t _M0L3idxS947
) {
  int32_t _M0L3curS938;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS938 = _M0L3idxS947;
  _2afor_942:;
  while (1) {
    int32_t _M0L6_2atmpS2773 = _M0L3curS938 + 1;
    int32_t _M0L14capacity__maskS2774 = _M0L4selfS940->$3;
    int32_t _M0L4nextS939 = _M0L6_2atmpS2773 & _M0L14capacity__maskS2774;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2772 =
      _M0L4selfS940->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS943;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2768;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2769;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3591;
    int32_t _tmp_4035;
    if (
      _M0L4nextS939 < 0
      || _M0L4nextS939 >= Moonbit_array_length(_M0L7entriesS2772)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS943
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2772[
        _M0L4nextS939
      ];
    if (_M0L7_2abindS943 == 0) {
      goto join_941;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS944 =
        _M0L7_2abindS943;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS945 =
        _M0L7_2aSomeS944;
      int32_t _M0L4_2axS946 = _M0L4_2axS945->$2;
      switch (_M0L4_2axS946) {
        case 0: {
          goto join_941;
          break;
        }
        default: {
          int32_t _M0L3pslS2771 = _M0L4_2axS945->$2;
          int32_t _M0L6_2atmpS2770 = _M0L3pslS2771 - 1;
          _M0L4_2axS945->$2 = _M0L6_2atmpS2770;
          moonbit_incref(_M0L4_2axS945);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS940, _M0L4_2axS945, _M0L3curS938);
          moonbit_decref(_M0L4_2axS945);
          _M0L3curS938 = _M0L4nextS939;
          goto _2afor_942;
          break;
        }
      }
    }
    goto joinlet_4034;
    join_941:;
    _M0L7entriesS2768 = _M0L4selfS940->$0;
    _M0L6_2atmpS2769 = 0;
    if (
      _M0L3curS938 < 0
      || _M0L3curS938 >= Moonbit_array_length(_M0L7entriesS2768)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3591
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2768[
        _M0L3curS938
      ];
    if (_M0L6_2aoldS3591) {
      moonbit_decref(_M0L6_2aoldS3591);
    }
    _M0L7entriesS2768[_M0L3curS938] = _M0L6_2atmpS2769;
    break;
    joinlet_4034:;
    _tmp_4035 = _M0L3curS938;
    _M0L3curS938 = _tmp_4035;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS918,
  struct _M0TPB5EntryGsiE* _M0L5entryS917
) {
  int32_t _M0L7_2abindS916;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS919;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS916 = _M0L5entryS917->$0;
  switch (_M0L7_2abindS916) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L4nextS2747 = _M0L5entryS917->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3596 = _M0L4selfS918->$5;
      if (_M0L4nextS2747) {
        moonbit_incref(_M0L4nextS2747);
      }
      if (_M0L6_2aoldS3596) {
        moonbit_decref(_M0L6_2aoldS3596);
      }
      _M0L4selfS918->$5 = _M0L4nextS2747;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2751 = _M0L4selfS918->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2750;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2748;
      struct _M0TPB5EntryGsiE* _M0L4nextS2749;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3598;
      if (
        _M0L7_2abindS916 < 0
        || _M0L7_2abindS916 >= Moonbit_array_length(_M0L7entriesS2751)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2750
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2751[_M0L7_2abindS916];
      if (_M0L6_2atmpS2750) {
        moonbit_incref(_M0L6_2atmpS2750);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2748
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2750);
      if (_M0L6_2atmpS2750) {
        moonbit_decref(_M0L6_2atmpS2750);
      }
      _M0L4nextS2749 = _M0L5entryS917->$1;
      _M0L6_2aoldS3598 = _M0L6_2atmpS2748->$1;
      if (_M0L4nextS2749) {
        moonbit_incref(_M0L4nextS2749);
      }
      if (_M0L6_2aoldS3598) {
        moonbit_decref(_M0L6_2aoldS3598);
      }
      _M0L6_2atmpS2748->$1 = _M0L4nextS2749;
      moonbit_decref(_M0L6_2atmpS2748);
      break;
    }
  }
  _M0L7_2abindS919 = _M0L5entryS917->$1;
  if (_M0L7_2abindS919 == 0) {
    int32_t _M0L4prevS2752 = _M0L5entryS917->$0;
    _M0L4selfS918->$6 = _M0L4prevS2752;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS920 = _M0L7_2abindS919;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS921 = _M0L7_2aSomeS920;
    int32_t _M0L4prevS2753 = _M0L5entryS917->$0;
    _M0L7_2anextS921->$0 = _M0L4prevS2753;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS924,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS923
) {
  int32_t _M0L7_2abindS922;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS925;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS922 = _M0L5entryS923->$0;
  switch (_M0L7_2abindS922) {
    case -1: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4nextS2754 =
        _M0L5entryS923->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3603 =
        _M0L4selfS924->$5;
      if (_M0L4nextS2754) {
        moonbit_incref(_M0L4nextS2754);
      }
      if (_M0L6_2aoldS3603) {
        moonbit_decref(_M0L6_2aoldS3603);
      }
      _M0L4selfS924->$5 = _M0L4nextS2754;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2758 =
        _M0L4selfS924->$0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2757;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2755;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4nextS2756;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3605;
      if (
        _M0L7_2abindS922 < 0
        || _M0L7_2abindS922 >= Moonbit_array_length(_M0L7entriesS2758)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2757
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2758[
          _M0L7_2abindS922
        ];
      if (_M0L6_2atmpS2757) {
        moonbit_incref(_M0L6_2atmpS2757);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2755
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2757);
      if (_M0L6_2atmpS2757) {
        moonbit_decref(_M0L6_2atmpS2757);
      }
      _M0L4nextS2756 = _M0L5entryS923->$1;
      _M0L6_2aoldS3605 = _M0L6_2atmpS2755->$1;
      if (_M0L4nextS2756) {
        moonbit_incref(_M0L4nextS2756);
      }
      if (_M0L6_2aoldS3605) {
        moonbit_decref(_M0L6_2aoldS3605);
      }
      _M0L6_2atmpS2755->$1 = _M0L4nextS2756;
      moonbit_decref(_M0L6_2atmpS2755);
      break;
    }
  }
  _M0L7_2abindS925 = _M0L5entryS923->$1;
  if (_M0L7_2abindS925 == 0) {
    int32_t _M0L4prevS2759 = _M0L5entryS923->$0;
    _M0L4selfS924->$6 = _M0L4prevS2759;
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS926 =
      _M0L7_2abindS925;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS927 =
      _M0L7_2aSomeS926;
    int32_t _M0L4prevS2760 = _M0L5entryS923->$0;
    _M0L7_2anextS927->$0 = _M0L4prevS2760;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS885,
  moonbit_string_t _M0L3keyS881
) {
  int32_t _M0L4hashS880;
  int32_t _M0L14capacity__maskS2716;
  int32_t _M0L6_2atmpS2715;
  int32_t _M0L1iS882;
  int32_t _M0L3idxS883;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS880 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS881);
  _M0L14capacity__maskS2716 = _M0L4selfS885->$3;
  _M0L6_2atmpS2715 = _M0L4hashS880 & _M0L14capacity__maskS2716;
  _M0L1iS882 = 0;
  _M0L3idxS883 = _M0L6_2atmpS2715;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2714 = _M0L4selfS885->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS884;
    if (
      _M0L3idxS883 < 0
      || _M0L3idxS883 >= Moonbit_array_length(_M0L7entriesS2714)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS884
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2714[_M0L3idxS883];
    if (_M0L7_2abindS884 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS886 = _M0L7_2abindS884;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS887 = _M0L7_2aSomeS886;
      int32_t _M0L4hashS2708 = _M0L8_2aentryS887->$3;
      int32_t _if__result_4037;
      int32_t _M0L3pslS2709;
      int32_t _M0L6_2atmpS2710;
      int32_t _M0L6_2atmpS2712;
      int32_t _M0L14capacity__maskS2713;
      int32_t _M0L6_2atmpS2711;
      if (_M0L4hashS2708 == _M0L4hashS880) {
        moonbit_string_t _M0L3keyS2707 = _M0L8_2aentryS887->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4037
        = _M0L3keyS2707 == _M0L3keyS881
          || Moonbit_array_length(_M0L3keyS2707)
             == Moonbit_array_length(_M0L3keyS881)
             && 0
                == memcmp(_M0L3keyS2707, _M0L3keyS881, Moonbit_array_length(_M0L3keyS2707) * 2);
      } else {
        _if__result_4037 = 0;
      }
      if (_if__result_4037) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS887);
      }
      _M0L3pslS2709 = _M0L8_2aentryS887->$2;
      moonbit_decref(_M0L8_2aentryS887);
      if (_M0L1iS882 > _M0L3pslS2709) {
        return 0;
      }
      _M0L6_2atmpS2710 = _M0L1iS882 + 1;
      _M0L6_2atmpS2712 = _M0L3idxS883 + 1;
      _M0L14capacity__maskS2713 = _M0L4selfS885->$3;
      _M0L6_2atmpS2711 = _M0L6_2atmpS2712 & _M0L14capacity__maskS2713;
      _M0L1iS882 = _M0L6_2atmpS2710;
      _M0L3idxS883 = _M0L6_2atmpS2711;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS894,
  moonbit_string_t _M0L3keyS890
) {
  int32_t _M0L4hashS889;
  int32_t _M0L14capacity__maskS2726;
  int32_t _M0L6_2atmpS2725;
  int32_t _M0L1iS891;
  int32_t _M0L3idxS892;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS889 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS890);
  _M0L14capacity__maskS2726 = _M0L4selfS894->$3;
  _M0L6_2atmpS2725 = _M0L4hashS889 & _M0L14capacity__maskS2726;
  _M0L1iS891 = 0;
  _M0L3idxS892 = _M0L6_2atmpS2725;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2724 = _M0L4selfS894->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS893;
    if (
      _M0L3idxS892 < 0
      || _M0L3idxS892 >= Moonbit_array_length(_M0L7entriesS2724)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS893
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2724[_M0L3idxS892];
    if (_M0L7_2abindS893 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS895 = _M0L7_2abindS893;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS896 = _M0L7_2aSomeS895;
      int32_t _M0L4hashS2718 = _M0L8_2aentryS896->$3;
      int32_t _if__result_4039;
      int32_t _M0L3pslS2719;
      int32_t _M0L6_2atmpS2720;
      int32_t _M0L6_2atmpS2722;
      int32_t _M0L14capacity__maskS2723;
      int32_t _M0L6_2atmpS2721;
      if (_M0L4hashS2718 == _M0L4hashS889) {
        moonbit_string_t _M0L3keyS2717 = _M0L8_2aentryS896->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4039
        = _M0L3keyS2717 == _M0L3keyS890
          || Moonbit_array_length(_M0L3keyS2717)
             == Moonbit_array_length(_M0L3keyS890)
             && 0
                == memcmp(_M0L3keyS2717, _M0L3keyS890, Moonbit_array_length(_M0L3keyS2717) * 2);
      } else {
        _if__result_4039 = 0;
      }
      if (_if__result_4039) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS896);
      }
      _M0L3pslS2719 = _M0L8_2aentryS896->$2;
      moonbit_decref(_M0L8_2aentryS896);
      if (_M0L1iS891 > _M0L3pslS2719) {
        return 0;
      }
      _M0L6_2atmpS2720 = _M0L1iS891 + 1;
      _M0L6_2atmpS2722 = _M0L3idxS892 + 1;
      _M0L14capacity__maskS2723 = _M0L4selfS894->$3;
      _M0L6_2atmpS2721 = _M0L6_2atmpS2722 & _M0L14capacity__maskS2723;
      _M0L1iS891 = _M0L6_2atmpS2720;
      _M0L3idxS892 = _M0L6_2atmpS2721;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS903,
  moonbit_string_t _M0L3keyS899
) {
  int32_t _M0L4hashS898;
  int32_t _M0L14capacity__maskS2736;
  int32_t _M0L6_2atmpS2735;
  int32_t _M0L1iS900;
  int32_t _M0L3idxS901;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS898 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS899);
  _M0L14capacity__maskS2736 = _M0L4selfS903->$3;
  _M0L6_2atmpS2735 = _M0L4hashS898 & _M0L14capacity__maskS2736;
  _M0L1iS900 = 0;
  _M0L3idxS901 = _M0L6_2atmpS2735;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2734 =
      _M0L4selfS903->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS902;
    if (
      _M0L3idxS901 < 0
      || _M0L3idxS901 >= Moonbit_array_length(_M0L7entriesS2734)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS902
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2734[
        _M0L3idxS901
      ];
    if (_M0L7_2abindS902 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS904 =
        _M0L7_2abindS902;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS905 =
        _M0L7_2aSomeS904;
      int32_t _M0L4hashS2728 = _M0L8_2aentryS905->$3;
      int32_t _if__result_4041;
      int32_t _M0L3pslS2729;
      int32_t _M0L6_2atmpS2730;
      int32_t _M0L6_2atmpS2732;
      int32_t _M0L14capacity__maskS2733;
      int32_t _M0L6_2atmpS2731;
      if (_M0L4hashS2728 == _M0L4hashS898) {
        moonbit_string_t _M0L3keyS2727 = _M0L8_2aentryS905->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4041
        = _M0L3keyS2727 == _M0L3keyS899
          || Moonbit_array_length(_M0L3keyS2727)
             == Moonbit_array_length(_M0L3keyS899)
             && 0
                == memcmp(_M0L3keyS2727, _M0L3keyS899, Moonbit_array_length(_M0L3keyS2727) * 2);
      } else {
        _if__result_4041 = 0;
      }
      if (_if__result_4041) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS905);
      }
      _M0L3pslS2729 = _M0L8_2aentryS905->$2;
      moonbit_decref(_M0L8_2aentryS905);
      if (_M0L1iS900 > _M0L3pslS2729) {
        return 0;
      }
      _M0L6_2atmpS2730 = _M0L1iS900 + 1;
      _M0L6_2atmpS2732 = _M0L3idxS901 + 1;
      _M0L14capacity__maskS2733 = _M0L4selfS903->$3;
      _M0L6_2atmpS2731 = _M0L6_2atmpS2732 & _M0L14capacity__maskS2733;
      _M0L1iS900 = _M0L6_2atmpS2730;
      _M0L3idxS901 = _M0L6_2atmpS2731;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS912,
  moonbit_string_t _M0L3keyS908
) {
  int32_t _M0L4hashS907;
  int32_t _M0L14capacity__maskS2746;
  int32_t _M0L6_2atmpS2745;
  int32_t _M0L1iS909;
  int32_t _M0L3idxS910;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS907 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS908);
  _M0L14capacity__maskS2746 = _M0L4selfS912->$3;
  _M0L6_2atmpS2745 = _M0L4hashS907 & _M0L14capacity__maskS2746;
  _M0L1iS909 = 0;
  _M0L3idxS910 = _M0L6_2atmpS2745;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2744 = _M0L4selfS912->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS911;
    if (
      _M0L3idxS910 < 0
      || _M0L3idxS910 >= Moonbit_array_length(_M0L7entriesS2744)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS911
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2744[_M0L3idxS910];
    if (_M0L7_2abindS911 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS913 = _M0L7_2abindS911;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS914 = _M0L7_2aSomeS913;
      int32_t _M0L4hashS2738 = _M0L8_2aentryS914->$3;
      int32_t _if__result_4043;
      int32_t _M0L3pslS2739;
      int32_t _M0L6_2atmpS2740;
      int32_t _M0L6_2atmpS2742;
      int32_t _M0L14capacity__maskS2743;
      int32_t _M0L6_2atmpS2741;
      if (_M0L4hashS2738 == _M0L4hashS907) {
        moonbit_string_t _M0L3keyS2737 = _M0L8_2aentryS914->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4043
        = _M0L3keyS2737 == _M0L3keyS908
          || Moonbit_array_length(_M0L3keyS2737)
             == Moonbit_array_length(_M0L3keyS908)
             && 0
                == memcmp(_M0L3keyS2737, _M0L3keyS908, Moonbit_array_length(_M0L3keyS2737) * 2);
      } else {
        _if__result_4043 = 0;
      }
      if (_if__result_4043) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS914);
      }
      _M0L3pslS2739 = _M0L8_2aentryS914->$2;
      moonbit_decref(_M0L8_2aentryS914);
      if (_M0L1iS909 > _M0L3pslS2739) {
        return 0;
      }
      _M0L6_2atmpS2740 = _M0L1iS909 + 1;
      _M0L6_2atmpS2742 = _M0L3idxS910 + 1;
      _M0L14capacity__maskS2743 = _M0L4selfS912->$3;
      _M0L6_2atmpS2741 = _M0L6_2atmpS2742 & _M0L14capacity__maskS2743;
      _M0L1iS909 = _M0L6_2atmpS2740;
      _M0L3idxS910 = _M0L6_2atmpS2741;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS849,
  moonbit_string_t _M0L3keyS845
) {
  int32_t _M0L4hashS844;
  int32_t _M0L14capacity__maskS2666;
  int32_t _M0L6_2atmpS2665;
  int32_t _M0L1iS846;
  int32_t _M0L3idxS847;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS844 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS845);
  _M0L14capacity__maskS2666 = _M0L4selfS849->$3;
  _M0L6_2atmpS2665 = _M0L4hashS844 & _M0L14capacity__maskS2666;
  _M0L1iS846 = 0;
  _M0L3idxS847 = _M0L6_2atmpS2665;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2664 =
      _M0L4selfS849->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS848;
    if (
      _M0L3idxS847 < 0
      || _M0L3idxS847 >= Moonbit_array_length(_M0L7entriesS2664)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS848
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2664[
        _M0L3idxS847
      ];
    if (_M0L7_2abindS848 == 0) {
      void* _M0L6_2atmpS2653 = 0;
      return _M0L6_2atmpS2653;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS850 =
        _M0L7_2abindS848;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS851 =
        _M0L7_2aSomeS850;
      int32_t _M0L4hashS2655 = _M0L8_2aentryS851->$3;
      int32_t _if__result_4045;
      int32_t _M0L3pslS2658;
      int32_t _M0L6_2atmpS2660;
      int32_t _M0L6_2atmpS2662;
      int32_t _M0L14capacity__maskS2663;
      int32_t _M0L6_2atmpS2661;
      if (_M0L4hashS2655 == _M0L4hashS844) {
        moonbit_string_t _M0L3keyS2654 = _M0L8_2aentryS851->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4045
        = _M0L3keyS2654 == _M0L3keyS845
          || Moonbit_array_length(_M0L3keyS2654)
             == Moonbit_array_length(_M0L3keyS845)
             && 0
                == memcmp(_M0L3keyS2654, _M0L3keyS845, Moonbit_array_length(_M0L3keyS2654) * 2);
      } else {
        _if__result_4045 = 0;
      }
      if (_if__result_4045) {
        void* _M0L5valueS2657 = _M0L8_2aentryS851->$5;
        void* _M0L6_2atmpS2656;
        moonbit_incref(_M0L5valueS2657);
        _M0L6_2atmpS2656 = _M0L5valueS2657;
        return _M0L6_2atmpS2656;
      } else {
        moonbit_incref(_M0L8_2aentryS851);
      }
      _M0L3pslS2658 = _M0L8_2aentryS851->$2;
      moonbit_decref(_M0L8_2aentryS851);
      if (_M0L1iS846 > _M0L3pslS2658) {
        void* _M0L6_2atmpS2659 = 0;
        return _M0L6_2atmpS2659;
      }
      _M0L6_2atmpS2660 = _M0L1iS846 + 1;
      _M0L6_2atmpS2662 = _M0L3idxS847 + 1;
      _M0L14capacity__maskS2663 = _M0L4selfS849->$3;
      _M0L6_2atmpS2661 = _M0L6_2atmpS2662 & _M0L14capacity__maskS2663;
      _M0L1iS846 = _M0L6_2atmpS2660;
      _M0L3idxS847 = _M0L6_2atmpS2661;
      continue;
    }
    break;
  }
}

moonbit_string_t _M0MPB3Map3getGssE(
  struct _M0TPB3MapGssE* _M0L4selfS858,
  moonbit_string_t _M0L3keyS854
) {
  int32_t _M0L4hashS853;
  int32_t _M0L14capacity__maskS2680;
  int32_t _M0L6_2atmpS2679;
  int32_t _M0L1iS855;
  int32_t _M0L3idxS856;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS853 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS854);
  _M0L14capacity__maskS2680 = _M0L4selfS858->$3;
  _M0L6_2atmpS2679 = _M0L4hashS853 & _M0L14capacity__maskS2680;
  _M0L1iS855 = 0;
  _M0L3idxS856 = _M0L6_2atmpS2679;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2678 = _M0L4selfS858->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS857;
    if (
      _M0L3idxS856 < 0
      || _M0L3idxS856 >= Moonbit_array_length(_M0L7entriesS2678)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS857
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2678[_M0L3idxS856];
    if (_M0L7_2abindS857 == 0) {
      moonbit_string_t _M0L6_2atmpS2667 = 0;
      return _M0L6_2atmpS2667;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS859 = _M0L7_2abindS857;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS860 = _M0L7_2aSomeS859;
      int32_t _M0L4hashS2669 = _M0L8_2aentryS860->$3;
      int32_t _if__result_4047;
      int32_t _M0L3pslS2672;
      int32_t _M0L6_2atmpS2674;
      int32_t _M0L6_2atmpS2676;
      int32_t _M0L14capacity__maskS2677;
      int32_t _M0L6_2atmpS2675;
      if (_M0L4hashS2669 == _M0L4hashS853) {
        moonbit_string_t _M0L3keyS2668 = _M0L8_2aentryS860->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4047
        = _M0L3keyS2668 == _M0L3keyS854
          || Moonbit_array_length(_M0L3keyS2668)
             == Moonbit_array_length(_M0L3keyS854)
             && 0
                == memcmp(_M0L3keyS2668, _M0L3keyS854, Moonbit_array_length(_M0L3keyS2668) * 2);
      } else {
        _if__result_4047 = 0;
      }
      if (_if__result_4047) {
        moonbit_string_t _M0L5valueS2671 = _M0L8_2aentryS860->$5;
        moonbit_string_t _M0L6_2atmpS2670;
        moonbit_incref(_M0L5valueS2671);
        _M0L6_2atmpS2670 = _M0L5valueS2671;
        return _M0L6_2atmpS2670;
      } else {
        moonbit_incref(_M0L8_2aentryS860);
      }
      _M0L3pslS2672 = _M0L8_2aentryS860->$2;
      moonbit_decref(_M0L8_2aentryS860);
      if (_M0L1iS855 > _M0L3pslS2672) {
        moonbit_string_t _M0L6_2atmpS2673 = 0;
        return _M0L6_2atmpS2673;
      }
      _M0L6_2atmpS2674 = _M0L1iS855 + 1;
      _M0L6_2atmpS2676 = _M0L3idxS856 + 1;
      _M0L14capacity__maskS2677 = _M0L4selfS858->$3;
      _M0L6_2atmpS2675 = _M0L6_2atmpS2676 & _M0L14capacity__maskS2677;
      _M0L1iS855 = _M0L6_2atmpS2674;
      _M0L3idxS856 = _M0L6_2atmpS2675;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS867,
  moonbit_string_t _M0L3keyS863
) {
  int32_t _M0L4hashS862;
  int32_t _M0L14capacity__maskS2694;
  int32_t _M0L6_2atmpS2693;
  int32_t _M0L1iS864;
  int32_t _M0L3idxS865;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS862 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS863);
  _M0L14capacity__maskS2694 = _M0L4selfS867->$3;
  _M0L6_2atmpS2693 = _M0L4hashS862 & _M0L14capacity__maskS2694;
  _M0L1iS864 = 0;
  _M0L3idxS865 = _M0L6_2atmpS2693;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2692 = _M0L4selfS867->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS866;
    if (
      _M0L3idxS865 < 0
      || _M0L3idxS865 >= Moonbit_array_length(_M0L7entriesS2692)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS866
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2692[_M0L3idxS865];
    if (_M0L7_2abindS866 == 0) {
      void* _M0L4NoneS2681 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS2681;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS868 = _M0L7_2abindS866;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS869 = _M0L7_2aSomeS868;
      int32_t _M0L4hashS2683 = _M0L8_2aentryS869->$3;
      int32_t _if__result_4049;
      int32_t _M0L3pslS2686;
      int32_t _M0L6_2atmpS2688;
      int32_t _M0L6_2atmpS2690;
      int32_t _M0L14capacity__maskS2691;
      int32_t _M0L6_2atmpS2689;
      if (_M0L4hashS2683 == _M0L4hashS862) {
        moonbit_string_t _M0L3keyS2682 = _M0L8_2aentryS869->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4049
        = _M0L3keyS2682 == _M0L3keyS863
          || Moonbit_array_length(_M0L3keyS2682)
             == Moonbit_array_length(_M0L3keyS863)
             && 0
                == memcmp(_M0L3keyS2682, _M0L3keyS863, Moonbit_array_length(_M0L3keyS2682) * 2);
      } else {
        _if__result_4049 = 0;
      }
      if (_if__result_4049) {
        float _M0L5valueS2685 = _M0L8_2aentryS869->$5;
        void* _M0L4SomeS2684 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS2684)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS2684)->$0
        = _M0L5valueS2685;
        return _M0L4SomeS2684;
      } else {
        moonbit_incref(_M0L8_2aentryS869);
      }
      _M0L3pslS2686 = _M0L8_2aentryS869->$2;
      moonbit_decref(_M0L8_2aentryS869);
      if (_M0L1iS864 > _M0L3pslS2686) {
        void* _M0L4NoneS2687 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS2687;
      }
      _M0L6_2atmpS2688 = _M0L1iS864 + 1;
      _M0L6_2atmpS2690 = _M0L3idxS865 + 1;
      _M0L14capacity__maskS2691 = _M0L4selfS867->$3;
      _M0L6_2atmpS2689 = _M0L6_2atmpS2690 & _M0L14capacity__maskS2691;
      _M0L1iS864 = _M0L6_2atmpS2688;
      _M0L3idxS865 = _M0L6_2atmpS2689;
      continue;
    }
    break;
  }
}

int64_t _M0MPB3Map3getGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS876,
  moonbit_string_t _M0L3keyS872
) {
  int32_t _M0L4hashS871;
  int32_t _M0L14capacity__maskS2706;
  int32_t _M0L6_2atmpS2705;
  int32_t _M0L1iS873;
  int32_t _M0L3idxS874;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS871 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS872);
  _M0L14capacity__maskS2706 = _M0L4selfS876->$3;
  _M0L6_2atmpS2705 = _M0L4hashS871 & _M0L14capacity__maskS2706;
  _M0L1iS873 = 0;
  _M0L3idxS874 = _M0L6_2atmpS2705;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2704 = _M0L4selfS876->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS875;
    if (
      _M0L3idxS874 < 0
      || _M0L3idxS874 >= Moonbit_array_length(_M0L7entriesS2704)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS875
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2704[_M0L3idxS874];
    if (_M0L7_2abindS875 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS877 = _M0L7_2abindS875;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS878 = _M0L7_2aSomeS877;
      int32_t _M0L4hashS2696 = _M0L8_2aentryS878->$3;
      int32_t _if__result_4051;
      int32_t _M0L3pslS2699;
      int32_t _M0L6_2atmpS2700;
      int32_t _M0L6_2atmpS2702;
      int32_t _M0L14capacity__maskS2703;
      int32_t _M0L6_2atmpS2701;
      if (_M0L4hashS2696 == _M0L4hashS871) {
        moonbit_string_t _M0L3keyS2695 = _M0L8_2aentryS878->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4051
        = _M0L3keyS2695 == _M0L3keyS872
          || Moonbit_array_length(_M0L3keyS2695)
             == Moonbit_array_length(_M0L3keyS872)
             && 0
                == memcmp(_M0L3keyS2695, _M0L3keyS872, Moonbit_array_length(_M0L3keyS2695) * 2);
      } else {
        _if__result_4051 = 0;
      }
      if (_if__result_4051) {
        int32_t _M0L5valueS2698 = _M0L8_2aentryS878->$5;
        int64_t _M0L6_2atmpS2697 = (int64_t)_M0L5valueS2698;
        return _M0L6_2atmpS2697;
      } else {
        moonbit_incref(_M0L8_2aentryS878);
      }
      _M0L3pslS2699 = _M0L8_2aentryS878->$2;
      moonbit_decref(_M0L8_2aentryS878);
      if (_M0L1iS873 > _M0L3pslS2699) {
        return 4294967296ll;
      }
      _M0L6_2atmpS2700 = _M0L1iS873 + 1;
      _M0L6_2atmpS2702 = _M0L3idxS874 + 1;
      _M0L14capacity__maskS2703 = _M0L4selfS876->$3;
      _M0L6_2atmpS2701 = _M0L6_2atmpS2702 & _M0L14capacity__maskS2703;
      _M0L1iS873 = _M0L6_2atmpS2700;
      _M0L3idxS874 = _M0L6_2atmpS2701;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L3arrS790,
  int64_t _M0L8capacityS792
) {
  int32_t _M0L3endS2607;
  int32_t _M0L5startS2608;
  int32_t _M0L6lengthS789;
  int32_t _M0L8capacityS791;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1mS795;
  int32_t _M0L3endS2604;
  int32_t _M0L5startS2605;
  int32_t _M0L7_2abindS796;
  int32_t _M0L2__S797;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2607 = _M0L3arrS790.$2;
  _M0L5startS2608 = _M0L3arrS790.$1;
  _M0L6lengthS789 = _M0L3endS2607 - _M0L5startS2608;
  if (_M0L8capacityS792 == 4294967296ll) {
    if (_M0L6lengthS789 == 0) {
      _M0L8capacityS791 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS791 = _M0FPB21capacity__for__length(_M0L6lengthS789);
    }
  } else {
    int64_t _M0L7_2aSomeS793 = _M0L8capacityS792;
    int32_t _M0L11_2acapacityS794 = (int32_t)_M0L7_2aSomeS793;
    int32_t _M0L6_2atmpS2606;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2606 = _M0FPB21capacity__for__length(_M0L6lengthS789);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS791
    = _M0MPC13int3Int3max(_M0L11_2acapacityS794, _M0L6_2atmpS2606);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS795
  = _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L8capacityS791);
  _M0L3endS2604 = _M0L3arrS790.$2;
  _M0L5startS2605 = _M0L3arrS790.$1;
  _M0L7_2abindS796 = _M0L3endS2604 - _M0L5startS2605;
  _M0L2__S797 = 0;
  while (1) {
    if (_M0L2__S797 < _M0L7_2abindS796) {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L3bufS2601 =
        _M0L3arrS790.$0;
      int32_t _M0L5startS2603 = _M0L3arrS790.$1;
      int32_t _M0L6_2atmpS2602 = _M0L5startS2603 + _M0L2__S797;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1eS798 =
        (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L3bufS2601[
          _M0L6_2atmpS2602
        ];
      moonbit_string_t _M0L6_2atmpS2598 = _M0L1eS798->$0;
      void* _M0L6_2atmpS2599 = _M0L1eS798->$1;
      int32_t _M0L6_2atmpS2600;
      moonbit_incref(_M0L6_2atmpS2599);
      moonbit_incref(_M0L6_2atmpS2598);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L1mS795, _M0L6_2atmpS2598, _M0L6_2atmpS2599);
      moonbit_decref(_M0L6_2atmpS2598);
      moonbit_decref(_M0L6_2atmpS2599);
      _M0L6_2atmpS2600 = _M0L2__S797 + 1;
      _M0L2__S797 = _M0L6_2atmpS2600;
      continue;
    }
    break;
  }
  return _M0L1mS795;
}

struct _M0TPB3MapGsiE* _M0MPB3Map3MapGsiE(
  struct _M0TPB9ArrayViewGUsiEE _M0L3arrS801,
  int64_t _M0L8capacityS803
) {
  int32_t _M0L3endS2618;
  int32_t _M0L5startS2619;
  int32_t _M0L6lengthS800;
  int32_t _M0L8capacityS802;
  struct _M0TPB3MapGsiE* _M0L1mS806;
  int32_t _M0L3endS2615;
  int32_t _M0L5startS2616;
  int32_t _M0L7_2abindS807;
  int32_t _M0L2__S808;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2618 = _M0L3arrS801.$2;
  _M0L5startS2619 = _M0L3arrS801.$1;
  _M0L6lengthS800 = _M0L3endS2618 - _M0L5startS2619;
  if (_M0L8capacityS803 == 4294967296ll) {
    if (_M0L6lengthS800 == 0) {
      _M0L8capacityS802 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS802 = _M0FPB21capacity__for__length(_M0L6lengthS800);
    }
  } else {
    int64_t _M0L7_2aSomeS804 = _M0L8capacityS803;
    int32_t _M0L11_2acapacityS805 = (int32_t)_M0L7_2aSomeS804;
    int32_t _M0L6_2atmpS2617;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2617 = _M0FPB21capacity__for__length(_M0L6lengthS800);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS802
    = _M0MPC13int3Int3max(_M0L11_2acapacityS805, _M0L6_2atmpS2617);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS806 = _M0FPB8new__mapGsiE(_M0L8capacityS802);
  _M0L3endS2615 = _M0L3arrS801.$2;
  _M0L5startS2616 = _M0L3arrS801.$1;
  _M0L7_2abindS807 = _M0L3endS2615 - _M0L5startS2616;
  _M0L2__S808 = 0;
  while (1) {
    if (_M0L2__S808 < _M0L7_2abindS807) {
      struct _M0TUsiE** _M0L3bufS2612 = _M0L3arrS801.$0;
      int32_t _M0L5startS2614 = _M0L3arrS801.$1;
      int32_t _M0L6_2atmpS2613 = _M0L5startS2614 + _M0L2__S808;
      struct _M0TUsiE* _M0L1eS809 =
        (struct _M0TUsiE*)_M0L3bufS2612[_M0L6_2atmpS2613];
      moonbit_string_t _M0L6_2atmpS2609 = _M0L1eS809->$0;
      int32_t _M0L6_2atmpS2610 = _M0L1eS809->$1;
      int32_t _M0L6_2atmpS2611;
      moonbit_incref(_M0L6_2atmpS2609);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS806, _M0L6_2atmpS2609, _M0L6_2atmpS2610);
      moonbit_decref(_M0L6_2atmpS2609);
      _M0L6_2atmpS2611 = _M0L2__S808 + 1;
      _M0L2__S808 = _M0L6_2atmpS2611;
      continue;
    }
    break;
  }
  return _M0L1mS806;
}

struct _M0TPB3MapGssE* _M0MPB3Map3MapGssE(
  struct _M0TPB9ArrayViewGUssEE _M0L3arrS812,
  int64_t _M0L8capacityS814
) {
  int32_t _M0L3endS2629;
  int32_t _M0L5startS2630;
  int32_t _M0L6lengthS811;
  int32_t _M0L8capacityS813;
  struct _M0TPB3MapGssE* _M0L1mS817;
  int32_t _M0L3endS2626;
  int32_t _M0L5startS2627;
  int32_t _M0L7_2abindS818;
  int32_t _M0L2__S819;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2629 = _M0L3arrS812.$2;
  _M0L5startS2630 = _M0L3arrS812.$1;
  _M0L6lengthS811 = _M0L3endS2629 - _M0L5startS2630;
  if (_M0L8capacityS814 == 4294967296ll) {
    if (_M0L6lengthS811 == 0) {
      _M0L8capacityS813 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS813 = _M0FPB21capacity__for__length(_M0L6lengthS811);
    }
  } else {
    int64_t _M0L7_2aSomeS815 = _M0L8capacityS814;
    int32_t _M0L11_2acapacityS816 = (int32_t)_M0L7_2aSomeS815;
    int32_t _M0L6_2atmpS2628;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2628 = _M0FPB21capacity__for__length(_M0L6lengthS811);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS813
    = _M0MPC13int3Int3max(_M0L11_2acapacityS816, _M0L6_2atmpS2628);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS817 = _M0FPB8new__mapGssE(_M0L8capacityS813);
  _M0L3endS2626 = _M0L3arrS812.$2;
  _M0L5startS2627 = _M0L3arrS812.$1;
  _M0L7_2abindS818 = _M0L3endS2626 - _M0L5startS2627;
  _M0L2__S819 = 0;
  while (1) {
    if (_M0L2__S819 < _M0L7_2abindS818) {
      struct _M0TUssE** _M0L3bufS2623 = _M0L3arrS812.$0;
      int32_t _M0L5startS2625 = _M0L3arrS812.$1;
      int32_t _M0L6_2atmpS2624 = _M0L5startS2625 + _M0L2__S819;
      struct _M0TUssE* _M0L1eS820 =
        (struct _M0TUssE*)_M0L3bufS2623[_M0L6_2atmpS2624];
      moonbit_string_t _M0L6_2atmpS2620 = _M0L1eS820->$0;
      moonbit_string_t _M0L6_2atmpS2621 = _M0L1eS820->$1;
      int32_t _M0L6_2atmpS2622;
      moonbit_incref(_M0L6_2atmpS2621);
      moonbit_incref(_M0L6_2atmpS2620);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS817, _M0L6_2atmpS2620, _M0L6_2atmpS2621);
      moonbit_decref(_M0L6_2atmpS2620);
      moonbit_decref(_M0L6_2atmpS2621);
      _M0L6_2atmpS2622 = _M0L2__S819 + 1;
      _M0L2__S819 = _M0L6_2atmpS2622;
      continue;
    }
    break;
  }
  return _M0L1mS817;
}

struct _M0TPB3MapGsbE* _M0MPB3Map3MapGsbE(
  struct _M0TPB9ArrayViewGUsbEE _M0L3arrS823,
  int64_t _M0L8capacityS825
) {
  int32_t _M0L3endS2640;
  int32_t _M0L5startS2641;
  int32_t _M0L6lengthS822;
  int32_t _M0L8capacityS824;
  struct _M0TPB3MapGsbE* _M0L1mS828;
  int32_t _M0L3endS2637;
  int32_t _M0L5startS2638;
  int32_t _M0L7_2abindS829;
  int32_t _M0L2__S830;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2640 = _M0L3arrS823.$2;
  _M0L5startS2641 = _M0L3arrS823.$1;
  _M0L6lengthS822 = _M0L3endS2640 - _M0L5startS2641;
  if (_M0L8capacityS825 == 4294967296ll) {
    if (_M0L6lengthS822 == 0) {
      _M0L8capacityS824 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS824 = _M0FPB21capacity__for__length(_M0L6lengthS822);
    }
  } else {
    int64_t _M0L7_2aSomeS826 = _M0L8capacityS825;
    int32_t _M0L11_2acapacityS827 = (int32_t)_M0L7_2aSomeS826;
    int32_t _M0L6_2atmpS2639;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2639 = _M0FPB21capacity__for__length(_M0L6lengthS822);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS824
    = _M0MPC13int3Int3max(_M0L11_2acapacityS827, _M0L6_2atmpS2639);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS828 = _M0FPB8new__mapGsbE(_M0L8capacityS824);
  _M0L3endS2637 = _M0L3arrS823.$2;
  _M0L5startS2638 = _M0L3arrS823.$1;
  _M0L7_2abindS829 = _M0L3endS2637 - _M0L5startS2638;
  _M0L2__S830 = 0;
  while (1) {
    if (_M0L2__S830 < _M0L7_2abindS829) {
      struct _M0TUsbE** _M0L3bufS2634 = _M0L3arrS823.$0;
      int32_t _M0L5startS2636 = _M0L3arrS823.$1;
      int32_t _M0L6_2atmpS2635 = _M0L5startS2636 + _M0L2__S830;
      struct _M0TUsbE* _M0L1eS831 =
        (struct _M0TUsbE*)_M0L3bufS2634[_M0L6_2atmpS2635];
      moonbit_string_t _M0L6_2atmpS2631 = _M0L1eS831->$0;
      int32_t _M0L6_2atmpS2632 = _M0L1eS831->$1;
      int32_t _M0L6_2atmpS2633;
      moonbit_incref(_M0L6_2atmpS2631);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS828, _M0L6_2atmpS2631, _M0L6_2atmpS2632);
      moonbit_decref(_M0L6_2atmpS2631);
      _M0L6_2atmpS2633 = _M0L2__S830 + 1;
      _M0L2__S830 = _M0L6_2atmpS2633;
      continue;
    }
    break;
  }
  return _M0L1mS828;
}

struct _M0TPB3MapGsfE* _M0MPB3Map3MapGsfE(
  struct _M0TPB9ArrayViewGUsfEE _M0L3arrS834,
  int64_t _M0L8capacityS836
) {
  int32_t _M0L3endS2651;
  int32_t _M0L5startS2652;
  int32_t _M0L6lengthS833;
  int32_t _M0L8capacityS835;
  struct _M0TPB3MapGsfE* _M0L1mS839;
  int32_t _M0L3endS2648;
  int32_t _M0L5startS2649;
  int32_t _M0L7_2abindS840;
  int32_t _M0L2__S841;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2651 = _M0L3arrS834.$2;
  _M0L5startS2652 = _M0L3arrS834.$1;
  _M0L6lengthS833 = _M0L3endS2651 - _M0L5startS2652;
  if (_M0L8capacityS836 == 4294967296ll) {
    if (_M0L6lengthS833 == 0) {
      _M0L8capacityS835 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS835 = _M0FPB21capacity__for__length(_M0L6lengthS833);
    }
  } else {
    int64_t _M0L7_2aSomeS837 = _M0L8capacityS836;
    int32_t _M0L11_2acapacityS838 = (int32_t)_M0L7_2aSomeS837;
    int32_t _M0L6_2atmpS2650;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2650 = _M0FPB21capacity__for__length(_M0L6lengthS833);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS835
    = _M0MPC13int3Int3max(_M0L11_2acapacityS838, _M0L6_2atmpS2650);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS839 = _M0FPB8new__mapGsfE(_M0L8capacityS835);
  _M0L3endS2648 = _M0L3arrS834.$2;
  _M0L5startS2649 = _M0L3arrS834.$1;
  _M0L7_2abindS840 = _M0L3endS2648 - _M0L5startS2649;
  _M0L2__S841 = 0;
  while (1) {
    if (_M0L2__S841 < _M0L7_2abindS840) {
      struct _M0TUsfE** _M0L3bufS2645 = _M0L3arrS834.$0;
      int32_t _M0L5startS2647 = _M0L3arrS834.$1;
      int32_t _M0L6_2atmpS2646 = _M0L5startS2647 + _M0L2__S841;
      struct _M0TUsfE* _M0L1eS842 =
        (struct _M0TUsfE*)_M0L3bufS2645[_M0L6_2atmpS2646];
      moonbit_string_t _M0L6_2atmpS2642 = _M0L1eS842->$0;
      float _M0L6_2atmpS2643 = _M0L1eS842->$1;
      int32_t _M0L6_2atmpS2644;
      moonbit_incref(_M0L6_2atmpS2642);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS839, _M0L6_2atmpS2642, _M0L6_2atmpS2643);
      moonbit_decref(_M0L6_2atmpS2642);
      _M0L6_2atmpS2644 = _M0L2__S841 + 1;
      _M0L2__S841 = _M0L6_2atmpS2644;
      continue;
    }
    break;
  }
  return _M0L1mS839;
}

int32_t _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS774,
  moonbit_string_t _M0L3keyS775,
  void* _M0L5valueS776
) {
  int32_t _M0L6_2atmpS2593;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2593 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS775);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS774, _M0L3keyS775, _M0L5valueS776, _M0L6_2atmpS2593);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS777,
  moonbit_string_t _M0L3keyS778,
  moonbit_string_t _M0L5valueS779
) {
  int32_t _M0L6_2atmpS2594;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2594 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS778);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS777, _M0L3keyS778, _M0L5valueS779, _M0L6_2atmpS2594);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS780,
  moonbit_string_t _M0L3keyS781,
  int32_t _M0L5valueS782
) {
  int32_t _M0L6_2atmpS2595;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2595 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS781);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS780, _M0L3keyS781, _M0L5valueS782, _M0L6_2atmpS2595);
  return 0;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS783,
  moonbit_string_t _M0L3keyS784,
  float _M0L5valueS785
) {
  int32_t _M0L6_2atmpS2596;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2596 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS784);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS783, _M0L3keyS784, _M0L5valueS785, _M0L6_2atmpS2596);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS786,
  moonbit_string_t _M0L3keyS787,
  int32_t _M0L5valueS788
) {
  int32_t _M0L6_2atmpS2597;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2597 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS787);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS786, _M0L3keyS787, _M0L5valueS788, _M0L6_2atmpS2597);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS697,
  moonbit_string_t _M0L3keyS703,
  void* _M0L5valueS704,
  int32_t _M0L4hashS699
) {
  int32_t _M0L14capacity__maskS2520;
  int32_t _M0L6_2atmpS2519;
  int32_t _M0L3pslS694;
  int32_t _M0L3idxS695;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2520 = _M0L4selfS697->$3;
  _M0L6_2atmpS2519 = _M0L4hashS699 & _M0L14capacity__maskS2520;
  _M0L3pslS694 = 0;
  _M0L3idxS695 = _M0L6_2atmpS2519;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2518 =
      _M0L4selfS697->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS696;
    if (
      _M0L3idxS695 < 0
      || _M0L3idxS695 >= Moonbit_array_length(_M0L7entriesS2518)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS696
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2518[
        _M0L3idxS695
      ];
    if (_M0L7_2abindS696 == 0) {
      int32_t _M0L4sizeS2503 = _M0L4selfS697->$1;
      int32_t _M0L8grow__atS2504 = _M0L4selfS697->$4;
      int32_t _M0L7_2abindS700;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS701;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS702;
      if (_M0L4sizeS2503 >= _M0L8grow__atS2504) {
        int32_t _M0L14capacity__maskS2506;
        int32_t _M0L6_2atmpS2505;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS697);
        _M0L14capacity__maskS2506 = _M0L4selfS697->$3;
        _M0L6_2atmpS2505 = _M0L4hashS699 & _M0L14capacity__maskS2506;
        _M0L3pslS694 = 0;
        _M0L3idxS695 = _M0L6_2atmpS2505;
        continue;
      }
      _M0L7_2abindS700 = _M0L4selfS697->$6;
      _M0L7_2abindS701 = 0;
      moonbit_incref(_M0L3keyS703);
      moonbit_incref(_M0L5valueS704);
      _M0L5entryS702
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L5entryS702)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
      _M0L5entryS702->$0 = _M0L7_2abindS700;
      _M0L5entryS702->$1 = _M0L7_2abindS701;
      _M0L5entryS702->$2 = _M0L3pslS694;
      _M0L5entryS702->$3 = _M0L4hashS699;
      _M0L5entryS702->$4 = _M0L3keyS703;
      _M0L5entryS702->$5 = _M0L5valueS704;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L5entryS702);
      moonbit_decref(_M0L5entryS702);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS705 =
        _M0L7_2abindS696;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS706 =
        _M0L7_2aSomeS705;
      int32_t _M0L4hashS2508 = _M0L14_2acurr__entryS706->$3;
      int32_t _if__result_4058;
      int32_t _M0L3pslS2509;
      int32_t _M0L6_2atmpS2514;
      int32_t _M0L6_2atmpS2516;
      int32_t _M0L14capacity__maskS2517;
      int32_t _M0L6_2atmpS2515;
      if (_M0L4hashS2508 == _M0L4hashS699) {
        moonbit_string_t _M0L3keyS2507 = _M0L14_2acurr__entryS706->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4058
        = _M0L3keyS2507 == _M0L3keyS703
          || Moonbit_array_length(_M0L3keyS2507)
             == Moonbit_array_length(_M0L3keyS703)
             && 0
                == memcmp(_M0L3keyS2507, _M0L3keyS703, Moonbit_array_length(_M0L3keyS2507) * 2);
      } else {
        _if__result_4058 = 0;
      }
      if (_if__result_4058) {
        void* _M0L6_2aoldS3652 = _M0L14_2acurr__entryS706->$5;
        moonbit_incref(_M0L5valueS704);
        moonbit_decref(_M0L6_2aoldS3652);
        _M0L14_2acurr__entryS706->$5 = _M0L5valueS704;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS706);
      }
      _M0L3pslS2509 = _M0L14_2acurr__entryS706->$2;
      if (_M0L3pslS694 > _M0L3pslS2509) {
        int32_t _M0L4sizeS2510 = _M0L4selfS697->$1;
        int32_t _M0L8grow__atS2511 = _M0L4selfS697->$4;
        int32_t _M0L7_2abindS707;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS708;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS709;
        if (_M0L4sizeS2510 >= _M0L8grow__atS2511) {
          int32_t _M0L14capacity__maskS2513;
          int32_t _M0L6_2atmpS2512;
          moonbit_decref(_M0L14_2acurr__entryS706);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS697);
          _M0L14capacity__maskS2513 = _M0L4selfS697->$3;
          _M0L6_2atmpS2512 = _M0L4hashS699 & _M0L14capacity__maskS2513;
          _M0L3pslS694 = 0;
          _M0L3idxS695 = _M0L6_2atmpS2512;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L14_2acurr__entryS706);
        moonbit_decref(_M0L14_2acurr__entryS706);
        _M0L7_2abindS707 = _M0L4selfS697->$6;
        _M0L7_2abindS708 = 0;
        moonbit_incref(_M0L3keyS703);
        moonbit_incref(_M0L5valueS704);
        _M0L5entryS709
        = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
        Moonbit_object_header(_M0L5entryS709)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
        _M0L5entryS709->$0 = _M0L7_2abindS707;
        _M0L5entryS709->$1 = _M0L7_2abindS708;
        _M0L5entryS709->$2 = _M0L3pslS694;
        _M0L5entryS709->$3 = _M0L4hashS699;
        _M0L5entryS709->$4 = _M0L3keyS703;
        _M0L5entryS709->$5 = _M0L5valueS704;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L5entryS709);
        moonbit_decref(_M0L5entryS709);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS706);
      }
      _M0L6_2atmpS2514 = _M0L3pslS694 + 1;
      _M0L6_2atmpS2516 = _M0L3idxS695 + 1;
      _M0L14capacity__maskS2517 = _M0L4selfS697->$3;
      _M0L6_2atmpS2515 = _M0L6_2atmpS2516 & _M0L14capacity__maskS2517;
      _M0L3pslS694 = _M0L6_2atmpS2514;
      _M0L3idxS695 = _M0L6_2atmpS2515;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS713,
  moonbit_string_t _M0L3keyS719,
  moonbit_string_t _M0L5valueS720,
  int32_t _M0L4hashS715
) {
  int32_t _M0L14capacity__maskS2538;
  int32_t _M0L6_2atmpS2537;
  int32_t _M0L3pslS710;
  int32_t _M0L3idxS711;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2538 = _M0L4selfS713->$3;
  _M0L6_2atmpS2537 = _M0L4hashS715 & _M0L14capacity__maskS2538;
  _M0L3pslS710 = 0;
  _M0L3idxS711 = _M0L6_2atmpS2537;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2536 = _M0L4selfS713->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS712;
    if (
      _M0L3idxS711 < 0
      || _M0L3idxS711 >= Moonbit_array_length(_M0L7entriesS2536)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS712
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2536[_M0L3idxS711];
    if (_M0L7_2abindS712 == 0) {
      int32_t _M0L4sizeS2521 = _M0L4selfS713->$1;
      int32_t _M0L8grow__atS2522 = _M0L4selfS713->$4;
      int32_t _M0L7_2abindS716;
      struct _M0TPB5EntryGssE* _M0L7_2abindS717;
      struct _M0TPB5EntryGssE* _M0L5entryS718;
      if (_M0L4sizeS2521 >= _M0L8grow__atS2522) {
        int32_t _M0L14capacity__maskS2524;
        int32_t _M0L6_2atmpS2523;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS713);
        _M0L14capacity__maskS2524 = _M0L4selfS713->$3;
        _M0L6_2atmpS2523 = _M0L4hashS715 & _M0L14capacity__maskS2524;
        _M0L3pslS710 = 0;
        _M0L3idxS711 = _M0L6_2atmpS2523;
        continue;
      }
      _M0L7_2abindS716 = _M0L4selfS713->$6;
      _M0L7_2abindS717 = 0;
      moonbit_incref(_M0L3keyS719);
      moonbit_incref(_M0L5valueS720);
      _M0L5entryS718
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS718)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 79, 0);
      _M0L5entryS718->$0 = _M0L7_2abindS716;
      _M0L5entryS718->$1 = _M0L7_2abindS717;
      _M0L5entryS718->$2 = _M0L3pslS710;
      _M0L5entryS718->$3 = _M0L4hashS715;
      _M0L5entryS718->$4 = _M0L3keyS719;
      _M0L5entryS718->$5 = _M0L5valueS720;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS713, _M0L3idxS711, _M0L5entryS718);
      moonbit_decref(_M0L5entryS718);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS721 = _M0L7_2abindS712;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS722 = _M0L7_2aSomeS721;
      int32_t _M0L4hashS2526 = _M0L14_2acurr__entryS722->$3;
      int32_t _if__result_4060;
      int32_t _M0L3pslS2527;
      int32_t _M0L6_2atmpS2532;
      int32_t _M0L6_2atmpS2534;
      int32_t _M0L14capacity__maskS2535;
      int32_t _M0L6_2atmpS2533;
      if (_M0L4hashS2526 == _M0L4hashS715) {
        moonbit_string_t _M0L3keyS2525 = _M0L14_2acurr__entryS722->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4060
        = _M0L3keyS2525 == _M0L3keyS719
          || Moonbit_array_length(_M0L3keyS2525)
             == Moonbit_array_length(_M0L3keyS719)
             && 0
                == memcmp(_M0L3keyS2525, _M0L3keyS719, Moonbit_array_length(_M0L3keyS2525) * 2);
      } else {
        _if__result_4060 = 0;
      }
      if (_if__result_4060) {
        moonbit_string_t _M0L6_2aoldS3656 = _M0L14_2acurr__entryS722->$5;
        moonbit_incref(_M0L5valueS720);
        moonbit_decref(_M0L6_2aoldS3656);
        _M0L14_2acurr__entryS722->$5 = _M0L5valueS720;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS722);
      }
      _M0L3pslS2527 = _M0L14_2acurr__entryS722->$2;
      if (_M0L3pslS710 > _M0L3pslS2527) {
        int32_t _M0L4sizeS2528 = _M0L4selfS713->$1;
        int32_t _M0L8grow__atS2529 = _M0L4selfS713->$4;
        int32_t _M0L7_2abindS723;
        struct _M0TPB5EntryGssE* _M0L7_2abindS724;
        struct _M0TPB5EntryGssE* _M0L5entryS725;
        if (_M0L4sizeS2528 >= _M0L8grow__atS2529) {
          int32_t _M0L14capacity__maskS2531;
          int32_t _M0L6_2atmpS2530;
          moonbit_decref(_M0L14_2acurr__entryS722);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS713);
          _M0L14capacity__maskS2531 = _M0L4selfS713->$3;
          _M0L6_2atmpS2530 = _M0L4hashS715 & _M0L14capacity__maskS2531;
          _M0L3pslS710 = 0;
          _M0L3idxS711 = _M0L6_2atmpS2530;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS713, _M0L3idxS711, _M0L14_2acurr__entryS722);
        moonbit_decref(_M0L14_2acurr__entryS722);
        _M0L7_2abindS723 = _M0L4selfS713->$6;
        _M0L7_2abindS724 = 0;
        moonbit_incref(_M0L3keyS719);
        moonbit_incref(_M0L5valueS720);
        _M0L5entryS725
        = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
        Moonbit_object_header(_M0L5entryS725)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 79, 0);
        _M0L5entryS725->$0 = _M0L7_2abindS723;
        _M0L5entryS725->$1 = _M0L7_2abindS724;
        _M0L5entryS725->$2 = _M0L3pslS710;
        _M0L5entryS725->$3 = _M0L4hashS715;
        _M0L5entryS725->$4 = _M0L3keyS719;
        _M0L5entryS725->$5 = _M0L5valueS720;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS713, _M0L3idxS711, _M0L5entryS725);
        moonbit_decref(_M0L5entryS725);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS722);
      }
      _M0L6_2atmpS2532 = _M0L3pslS710 + 1;
      _M0L6_2atmpS2534 = _M0L3idxS711 + 1;
      _M0L14capacity__maskS2535 = _M0L4selfS713->$3;
      _M0L6_2atmpS2533 = _M0L6_2atmpS2534 & _M0L14capacity__maskS2535;
      _M0L3pslS710 = _M0L6_2atmpS2532;
      _M0L3idxS711 = _M0L6_2atmpS2533;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS729,
  moonbit_string_t _M0L3keyS735,
  int32_t _M0L5valueS736,
  int32_t _M0L4hashS731
) {
  int32_t _M0L14capacity__maskS2556;
  int32_t _M0L6_2atmpS2555;
  int32_t _M0L3pslS726;
  int32_t _M0L3idxS727;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2556 = _M0L4selfS729->$3;
  _M0L6_2atmpS2555 = _M0L4hashS731 & _M0L14capacity__maskS2556;
  _M0L3pslS726 = 0;
  _M0L3idxS727 = _M0L6_2atmpS2555;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2554 = _M0L4selfS729->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS728;
    if (
      _M0L3idxS727 < 0
      || _M0L3idxS727 >= Moonbit_array_length(_M0L7entriesS2554)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS728
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2554[_M0L3idxS727];
    if (_M0L7_2abindS728 == 0) {
      int32_t _M0L4sizeS2539 = _M0L4selfS729->$1;
      int32_t _M0L8grow__atS2540 = _M0L4selfS729->$4;
      int32_t _M0L7_2abindS732;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS733;
      struct _M0TPB5EntryGsbE* _M0L5entryS734;
      if (_M0L4sizeS2539 >= _M0L8grow__atS2540) {
        int32_t _M0L14capacity__maskS2542;
        int32_t _M0L6_2atmpS2541;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS729);
        _M0L14capacity__maskS2542 = _M0L4selfS729->$3;
        _M0L6_2atmpS2541 = _M0L4hashS731 & _M0L14capacity__maskS2542;
        _M0L3pslS726 = 0;
        _M0L3idxS727 = _M0L6_2atmpS2541;
        continue;
      }
      _M0L7_2abindS732 = _M0L4selfS729->$6;
      _M0L7_2abindS733 = 0;
      moonbit_incref(_M0L3keyS735);
      _M0L5entryS734
      = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
      Moonbit_object_header(_M0L5entryS734)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 84, 0);
      _M0L5entryS734->$0 = _M0L7_2abindS732;
      _M0L5entryS734->$1 = _M0L7_2abindS733;
      _M0L5entryS734->$2 = _M0L3pslS726;
      _M0L5entryS734->$3 = _M0L4hashS731;
      _M0L5entryS734->$4 = _M0L3keyS735;
      _M0L5entryS734->$5 = _M0L5valueS736;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS729, _M0L3idxS727, _M0L5entryS734);
      moonbit_decref(_M0L5entryS734);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS737 = _M0L7_2abindS728;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS738 = _M0L7_2aSomeS737;
      int32_t _M0L4hashS2544 = _M0L14_2acurr__entryS738->$3;
      int32_t _if__result_4062;
      int32_t _M0L3pslS2545;
      int32_t _M0L6_2atmpS2550;
      int32_t _M0L6_2atmpS2552;
      int32_t _M0L14capacity__maskS2553;
      int32_t _M0L6_2atmpS2551;
      if (_M0L4hashS2544 == _M0L4hashS731) {
        moonbit_string_t _M0L3keyS2543 = _M0L14_2acurr__entryS738->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4062
        = _M0L3keyS2543 == _M0L3keyS735
          || Moonbit_array_length(_M0L3keyS2543)
             == Moonbit_array_length(_M0L3keyS735)
             && 0
                == memcmp(_M0L3keyS2543, _M0L3keyS735, Moonbit_array_length(_M0L3keyS2543) * 2);
      } else {
        _if__result_4062 = 0;
      }
      if (_if__result_4062) {
        _M0L14_2acurr__entryS738->$5 = _M0L5valueS736;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS738);
      }
      _M0L3pslS2545 = _M0L14_2acurr__entryS738->$2;
      if (_M0L3pslS726 > _M0L3pslS2545) {
        int32_t _M0L4sizeS2546 = _M0L4selfS729->$1;
        int32_t _M0L8grow__atS2547 = _M0L4selfS729->$4;
        int32_t _M0L7_2abindS739;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS740;
        struct _M0TPB5EntryGsbE* _M0L5entryS741;
        if (_M0L4sizeS2546 >= _M0L8grow__atS2547) {
          int32_t _M0L14capacity__maskS2549;
          int32_t _M0L6_2atmpS2548;
          moonbit_decref(_M0L14_2acurr__entryS738);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS729);
          _M0L14capacity__maskS2549 = _M0L4selfS729->$3;
          _M0L6_2atmpS2548 = _M0L4hashS731 & _M0L14capacity__maskS2549;
          _M0L3pslS726 = 0;
          _M0L3idxS727 = _M0L6_2atmpS2548;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS729, _M0L3idxS727, _M0L14_2acurr__entryS738);
        moonbit_decref(_M0L14_2acurr__entryS738);
        _M0L7_2abindS739 = _M0L4selfS729->$6;
        _M0L7_2abindS740 = 0;
        moonbit_incref(_M0L3keyS735);
        _M0L5entryS741
        = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
        Moonbit_object_header(_M0L5entryS741)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 84, 0);
        _M0L5entryS741->$0 = _M0L7_2abindS739;
        _M0L5entryS741->$1 = _M0L7_2abindS740;
        _M0L5entryS741->$2 = _M0L3pslS726;
        _M0L5entryS741->$3 = _M0L4hashS731;
        _M0L5entryS741->$4 = _M0L3keyS735;
        _M0L5entryS741->$5 = _M0L5valueS736;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS729, _M0L3idxS727, _M0L5entryS741);
        moonbit_decref(_M0L5entryS741);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS738);
      }
      _M0L6_2atmpS2550 = _M0L3pslS726 + 1;
      _M0L6_2atmpS2552 = _M0L3idxS727 + 1;
      _M0L14capacity__maskS2553 = _M0L4selfS729->$3;
      _M0L6_2atmpS2551 = _M0L6_2atmpS2552 & _M0L14capacity__maskS2553;
      _M0L3pslS726 = _M0L6_2atmpS2550;
      _M0L3idxS727 = _M0L6_2atmpS2551;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS745,
  moonbit_string_t _M0L3keyS751,
  float _M0L5valueS752,
  int32_t _M0L4hashS747
) {
  int32_t _M0L14capacity__maskS2574;
  int32_t _M0L6_2atmpS2573;
  int32_t _M0L3pslS742;
  int32_t _M0L3idxS743;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2574 = _M0L4selfS745->$3;
  _M0L6_2atmpS2573 = _M0L4hashS747 & _M0L14capacity__maskS2574;
  _M0L3pslS742 = 0;
  _M0L3idxS743 = _M0L6_2atmpS2573;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2572 = _M0L4selfS745->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS744;
    if (
      _M0L3idxS743 < 0
      || _M0L3idxS743 >= Moonbit_array_length(_M0L7entriesS2572)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS744
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2572[_M0L3idxS743];
    if (_M0L7_2abindS744 == 0) {
      int32_t _M0L4sizeS2557 = _M0L4selfS745->$1;
      int32_t _M0L8grow__atS2558 = _M0L4selfS745->$4;
      int32_t _M0L7_2abindS748;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS749;
      struct _M0TPB5EntryGsfE* _M0L5entryS750;
      if (_M0L4sizeS2557 >= _M0L8grow__atS2558) {
        int32_t _M0L14capacity__maskS2560;
        int32_t _M0L6_2atmpS2559;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS745);
        _M0L14capacity__maskS2560 = _M0L4selfS745->$3;
        _M0L6_2atmpS2559 = _M0L4hashS747 & _M0L14capacity__maskS2560;
        _M0L3pslS742 = 0;
        _M0L3idxS743 = _M0L6_2atmpS2559;
        continue;
      }
      _M0L7_2abindS748 = _M0L4selfS745->$6;
      _M0L7_2abindS749 = 0;
      moonbit_incref(_M0L3keyS751);
      _M0L5entryS750
      = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
      Moonbit_object_header(_M0L5entryS750)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 88, 0);
      _M0L5entryS750->$0 = _M0L7_2abindS748;
      _M0L5entryS750->$1 = _M0L7_2abindS749;
      _M0L5entryS750->$2 = _M0L3pslS742;
      _M0L5entryS750->$3 = _M0L4hashS747;
      _M0L5entryS750->$4 = _M0L3keyS751;
      _M0L5entryS750->$5 = _M0L5valueS752;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS745, _M0L3idxS743, _M0L5entryS750);
      moonbit_decref(_M0L5entryS750);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS753 = _M0L7_2abindS744;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS754 = _M0L7_2aSomeS753;
      int32_t _M0L4hashS2562 = _M0L14_2acurr__entryS754->$3;
      int32_t _if__result_4064;
      int32_t _M0L3pslS2563;
      int32_t _M0L6_2atmpS2568;
      int32_t _M0L6_2atmpS2570;
      int32_t _M0L14capacity__maskS2571;
      int32_t _M0L6_2atmpS2569;
      if (_M0L4hashS2562 == _M0L4hashS747) {
        moonbit_string_t _M0L3keyS2561 = _M0L14_2acurr__entryS754->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4064
        = _M0L3keyS2561 == _M0L3keyS751
          || Moonbit_array_length(_M0L3keyS2561)
             == Moonbit_array_length(_M0L3keyS751)
             && 0
                == memcmp(_M0L3keyS2561, _M0L3keyS751, Moonbit_array_length(_M0L3keyS2561) * 2);
      } else {
        _if__result_4064 = 0;
      }
      if (_if__result_4064) {
        _M0L14_2acurr__entryS754->$5 = _M0L5valueS752;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS754);
      }
      _M0L3pslS2563 = _M0L14_2acurr__entryS754->$2;
      if (_M0L3pslS742 > _M0L3pslS2563) {
        int32_t _M0L4sizeS2564 = _M0L4selfS745->$1;
        int32_t _M0L8grow__atS2565 = _M0L4selfS745->$4;
        int32_t _M0L7_2abindS755;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS756;
        struct _M0TPB5EntryGsfE* _M0L5entryS757;
        if (_M0L4sizeS2564 >= _M0L8grow__atS2565) {
          int32_t _M0L14capacity__maskS2567;
          int32_t _M0L6_2atmpS2566;
          moonbit_decref(_M0L14_2acurr__entryS754);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS745);
          _M0L14capacity__maskS2567 = _M0L4selfS745->$3;
          _M0L6_2atmpS2566 = _M0L4hashS747 & _M0L14capacity__maskS2567;
          _M0L3pslS742 = 0;
          _M0L3idxS743 = _M0L6_2atmpS2566;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS745, _M0L3idxS743, _M0L14_2acurr__entryS754);
        moonbit_decref(_M0L14_2acurr__entryS754);
        _M0L7_2abindS755 = _M0L4selfS745->$6;
        _M0L7_2abindS756 = 0;
        moonbit_incref(_M0L3keyS751);
        _M0L5entryS757
        = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
        Moonbit_object_header(_M0L5entryS757)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 88, 0);
        _M0L5entryS757->$0 = _M0L7_2abindS755;
        _M0L5entryS757->$1 = _M0L7_2abindS756;
        _M0L5entryS757->$2 = _M0L3pslS742;
        _M0L5entryS757->$3 = _M0L4hashS747;
        _M0L5entryS757->$4 = _M0L3keyS751;
        _M0L5entryS757->$5 = _M0L5valueS752;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS745, _M0L3idxS743, _M0L5entryS757);
        moonbit_decref(_M0L5entryS757);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS754);
      }
      _M0L6_2atmpS2568 = _M0L3pslS742 + 1;
      _M0L6_2atmpS2570 = _M0L3idxS743 + 1;
      _M0L14capacity__maskS2571 = _M0L4selfS745->$3;
      _M0L6_2atmpS2569 = _M0L6_2atmpS2570 & _M0L14capacity__maskS2571;
      _M0L3pslS742 = _M0L6_2atmpS2568;
      _M0L3idxS743 = _M0L6_2atmpS2569;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS761,
  moonbit_string_t _M0L3keyS767,
  int32_t _M0L5valueS768,
  int32_t _M0L4hashS763
) {
  int32_t _M0L14capacity__maskS2592;
  int32_t _M0L6_2atmpS2591;
  int32_t _M0L3pslS758;
  int32_t _M0L3idxS759;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2592 = _M0L4selfS761->$3;
  _M0L6_2atmpS2591 = _M0L4hashS763 & _M0L14capacity__maskS2592;
  _M0L3pslS758 = 0;
  _M0L3idxS759 = _M0L6_2atmpS2591;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2590 = _M0L4selfS761->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS760;
    if (
      _M0L3idxS759 < 0
      || _M0L3idxS759 >= Moonbit_array_length(_M0L7entriesS2590)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS760
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2590[_M0L3idxS759];
    if (_M0L7_2abindS760 == 0) {
      int32_t _M0L4sizeS2575 = _M0L4selfS761->$1;
      int32_t _M0L8grow__atS2576 = _M0L4selfS761->$4;
      int32_t _M0L7_2abindS764;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS765;
      struct _M0TPB5EntryGsiE* _M0L5entryS766;
      if (_M0L4sizeS2575 >= _M0L8grow__atS2576) {
        int32_t _M0L14capacity__maskS2578;
        int32_t _M0L6_2atmpS2577;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS761);
        _M0L14capacity__maskS2578 = _M0L4selfS761->$3;
        _M0L6_2atmpS2577 = _M0L4hashS763 & _M0L14capacity__maskS2578;
        _M0L3pslS758 = 0;
        _M0L3idxS759 = _M0L6_2atmpS2577;
        continue;
      }
      _M0L7_2abindS764 = _M0L4selfS761->$6;
      _M0L7_2abindS765 = 0;
      moonbit_incref(_M0L3keyS767);
      _M0L5entryS766
      = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
      Moonbit_object_header(_M0L5entryS766)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 92, 0);
      _M0L5entryS766->$0 = _M0L7_2abindS764;
      _M0L5entryS766->$1 = _M0L7_2abindS765;
      _M0L5entryS766->$2 = _M0L3pslS758;
      _M0L5entryS766->$3 = _M0L4hashS763;
      _M0L5entryS766->$4 = _M0L3keyS767;
      _M0L5entryS766->$5 = _M0L5valueS768;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS761, _M0L3idxS759, _M0L5entryS766);
      moonbit_decref(_M0L5entryS766);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS769 = _M0L7_2abindS760;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS770 = _M0L7_2aSomeS769;
      int32_t _M0L4hashS2580 = _M0L14_2acurr__entryS770->$3;
      int32_t _if__result_4066;
      int32_t _M0L3pslS2581;
      int32_t _M0L6_2atmpS2586;
      int32_t _M0L6_2atmpS2588;
      int32_t _M0L14capacity__maskS2589;
      int32_t _M0L6_2atmpS2587;
      if (_M0L4hashS2580 == _M0L4hashS763) {
        moonbit_string_t _M0L3keyS2579 = _M0L14_2acurr__entryS770->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4066
        = _M0L3keyS2579 == _M0L3keyS767
          || Moonbit_array_length(_M0L3keyS2579)
             == Moonbit_array_length(_M0L3keyS767)
             && 0
                == memcmp(_M0L3keyS2579, _M0L3keyS767, Moonbit_array_length(_M0L3keyS2579) * 2);
      } else {
        _if__result_4066 = 0;
      }
      if (_if__result_4066) {
        _M0L14_2acurr__entryS770->$5 = _M0L5valueS768;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS770);
      }
      _M0L3pslS2581 = _M0L14_2acurr__entryS770->$2;
      if (_M0L3pslS758 > _M0L3pslS2581) {
        int32_t _M0L4sizeS2582 = _M0L4selfS761->$1;
        int32_t _M0L8grow__atS2583 = _M0L4selfS761->$4;
        int32_t _M0L7_2abindS771;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS772;
        struct _M0TPB5EntryGsiE* _M0L5entryS773;
        if (_M0L4sizeS2582 >= _M0L8grow__atS2583) {
          int32_t _M0L14capacity__maskS2585;
          int32_t _M0L6_2atmpS2584;
          moonbit_decref(_M0L14_2acurr__entryS770);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS761);
          _M0L14capacity__maskS2585 = _M0L4selfS761->$3;
          _M0L6_2atmpS2584 = _M0L4hashS763 & _M0L14capacity__maskS2585;
          _M0L3pslS758 = 0;
          _M0L3idxS759 = _M0L6_2atmpS2584;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS761, _M0L3idxS759, _M0L14_2acurr__entryS770);
        moonbit_decref(_M0L14_2acurr__entryS770);
        _M0L7_2abindS771 = _M0L4selfS761->$6;
        _M0L7_2abindS772 = 0;
        moonbit_incref(_M0L3keyS767);
        _M0L5entryS773
        = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
        Moonbit_object_header(_M0L5entryS773)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 92, 0);
        _M0L5entryS773->$0 = _M0L7_2abindS771;
        _M0L5entryS773->$1 = _M0L7_2abindS772;
        _M0L5entryS773->$2 = _M0L3pslS758;
        _M0L5entryS773->$3 = _M0L4hashS763;
        _M0L5entryS773->$4 = _M0L3keyS767;
        _M0L5entryS773->$5 = _M0L5valueS768;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS761, _M0L3idxS759, _M0L5entryS773);
        moonbit_decref(_M0L5entryS773);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS770);
      }
      _M0L6_2atmpS2586 = _M0L3pslS758 + 1;
      _M0L6_2atmpS2588 = _M0L3idxS759 + 1;
      _M0L14capacity__maskS2589 = _M0L4selfS761->$3;
      _M0L6_2atmpS2587 = _M0L6_2atmpS2588 & _M0L14capacity__maskS2589;
      _M0L3pslS758 = _M0L6_2atmpS2586;
      _M0L3idxS759 = _M0L6_2atmpS2587;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS655
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L9old__headS654;
  int32_t _M0L8capacityS2470;
  int32_t _M0L13new__capacityS656;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2464;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS2463;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2aoldS3672;
  int32_t _M0L6_2atmpS2465;
  int32_t _M0L8capacityS2467;
  int32_t _M0L6_2atmpS2466;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2468;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3671;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1xS657;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS654 = _M0L4selfS655->$5;
  _M0L8capacityS2470 = _M0L4selfS655->$2;
  _M0L13new__capacityS656 = _M0L8capacityS2470 << 1;
  _M0L6_2atmpS2464 = 0;
  _M0L6_2atmpS2463
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS656, _M0L6_2atmpS2464);
  _M0L6_2aoldS3672 = _M0L4selfS655->$0;
  if (_M0L9old__headS654) {
    moonbit_incref(_M0L9old__headS654);
  }
  moonbit_decref(_M0L6_2aoldS3672);
  _M0L4selfS655->$0 = _M0L6_2atmpS2463;
  _M0L4selfS655->$2 = _M0L13new__capacityS656;
  _M0L6_2atmpS2465 = _M0L13new__capacityS656 - 1;
  _M0L4selfS655->$3 = _M0L6_2atmpS2465;
  _M0L8capacityS2467 = _M0L4selfS655->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2466 = _M0FPB21calc__grow__threshold(_M0L8capacityS2467);
  _M0L4selfS655->$4 = _M0L6_2atmpS2466;
  _M0L4selfS655->$1 = 0;
  _M0L6_2atmpS2468 = 0;
  _M0L6_2aoldS3671 = _M0L4selfS655->$5;
  if (_M0L6_2aoldS3671) {
    moonbit_decref(_M0L6_2aoldS3671);
  }
  _M0L4selfS655->$5 = _M0L6_2atmpS2468;
  _M0L4selfS655->$6 = -1;
  _M0L1xS657 = _M0L9old__headS654;
  while (1) {
    if (_M0L1xS657 == 0) {
      if (_M0L1xS657) {
        moonbit_decref(_M0L1xS657);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS659 =
        _M0L1xS657;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2aeS660 =
        _M0L7_2aSomeS659;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L15next__in__chainS661 =
        _M0L4_2aeS660->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2469 =
        0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3669 =
        _M0L4_2aeS660->$1;
      if (_M0L15next__in__chainS661) {
        moonbit_incref(_M0L15next__in__chainS661);
      }
      if (_M0L6_2aoldS3669) {
        moonbit_decref(_M0L6_2aoldS3669);
      }
      _M0L4_2aeS660->$1 = _M0L6_2atmpS2469;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS655, _M0L4_2aeS660);
      moonbit_decref(_M0L4_2aeS660);
      _M0L1xS657 = _M0L15next__in__chainS661;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE* _M0L4selfS663) {
  struct _M0TPB5EntryGssE* _M0L9old__headS662;
  int32_t _M0L8capacityS2478;
  int32_t _M0L13new__capacityS664;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2472;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS2471;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS3677;
  int32_t _M0L6_2atmpS2473;
  int32_t _M0L8capacityS2475;
  int32_t _M0L6_2atmpS2474;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2476;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3676;
  struct _M0TPB5EntryGssE* _M0L1xS665;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS662 = _M0L4selfS663->$5;
  _M0L8capacityS2478 = _M0L4selfS663->$2;
  _M0L13new__capacityS664 = _M0L8capacityS2478 << 1;
  _M0L6_2atmpS2472 = 0;
  _M0L6_2atmpS2471
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS664, _M0L6_2atmpS2472);
  _M0L6_2aoldS3677 = _M0L4selfS663->$0;
  if (_M0L9old__headS662) {
    moonbit_incref(_M0L9old__headS662);
  }
  moonbit_decref(_M0L6_2aoldS3677);
  _M0L4selfS663->$0 = _M0L6_2atmpS2471;
  _M0L4selfS663->$2 = _M0L13new__capacityS664;
  _M0L6_2atmpS2473 = _M0L13new__capacityS664 - 1;
  _M0L4selfS663->$3 = _M0L6_2atmpS2473;
  _M0L8capacityS2475 = _M0L4selfS663->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2474 = _M0FPB21calc__grow__threshold(_M0L8capacityS2475);
  _M0L4selfS663->$4 = _M0L6_2atmpS2474;
  _M0L4selfS663->$1 = 0;
  _M0L6_2atmpS2476 = 0;
  _M0L6_2aoldS3676 = _M0L4selfS663->$5;
  if (_M0L6_2aoldS3676) {
    moonbit_decref(_M0L6_2aoldS3676);
  }
  _M0L4selfS663->$5 = _M0L6_2atmpS2476;
  _M0L4selfS663->$6 = -1;
  _M0L1xS665 = _M0L9old__headS662;
  while (1) {
    if (_M0L1xS665 == 0) {
      if (_M0L1xS665) {
        moonbit_decref(_M0L1xS665);
      }
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS667 = _M0L1xS665;
      struct _M0TPB5EntryGssE* _M0L4_2aeS668 = _M0L7_2aSomeS667;
      struct _M0TPB5EntryGssE* _M0L15next__in__chainS669 = _M0L4_2aeS668->$1;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2477 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3674 = _M0L4_2aeS668->$1;
      if (_M0L15next__in__chainS669) {
        moonbit_incref(_M0L15next__in__chainS669);
      }
      if (_M0L6_2aoldS3674) {
        moonbit_decref(_M0L6_2aoldS3674);
      }
      _M0L4_2aeS668->$1 = _M0L6_2atmpS2477;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGssE(_M0L4selfS663, _M0L4_2aeS668);
      moonbit_decref(_M0L4_2aeS668);
      _M0L1xS665 = _M0L15next__in__chainS669;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE* _M0L4selfS671) {
  struct _M0TPB5EntryGsbE* _M0L9old__headS670;
  int32_t _M0L8capacityS2486;
  int32_t _M0L13new__capacityS672;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2480;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS2479;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS3682;
  int32_t _M0L6_2atmpS2481;
  int32_t _M0L8capacityS2483;
  int32_t _M0L6_2atmpS2482;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2484;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3681;
  struct _M0TPB5EntryGsbE* _M0L1xS673;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS670 = _M0L4selfS671->$5;
  _M0L8capacityS2486 = _M0L4selfS671->$2;
  _M0L13new__capacityS672 = _M0L8capacityS2486 << 1;
  _M0L6_2atmpS2480 = 0;
  _M0L6_2atmpS2479
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS672, _M0L6_2atmpS2480);
  _M0L6_2aoldS3682 = _M0L4selfS671->$0;
  if (_M0L9old__headS670) {
    moonbit_incref(_M0L9old__headS670);
  }
  moonbit_decref(_M0L6_2aoldS3682);
  _M0L4selfS671->$0 = _M0L6_2atmpS2479;
  _M0L4selfS671->$2 = _M0L13new__capacityS672;
  _M0L6_2atmpS2481 = _M0L13new__capacityS672 - 1;
  _M0L4selfS671->$3 = _M0L6_2atmpS2481;
  _M0L8capacityS2483 = _M0L4selfS671->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2482 = _M0FPB21calc__grow__threshold(_M0L8capacityS2483);
  _M0L4selfS671->$4 = _M0L6_2atmpS2482;
  _M0L4selfS671->$1 = 0;
  _M0L6_2atmpS2484 = 0;
  _M0L6_2aoldS3681 = _M0L4selfS671->$5;
  if (_M0L6_2aoldS3681) {
    moonbit_decref(_M0L6_2aoldS3681);
  }
  _M0L4selfS671->$5 = _M0L6_2atmpS2484;
  _M0L4selfS671->$6 = -1;
  _M0L1xS673 = _M0L9old__headS670;
  while (1) {
    if (_M0L1xS673 == 0) {
      if (_M0L1xS673) {
        moonbit_decref(_M0L1xS673);
      }
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS675 = _M0L1xS673;
      struct _M0TPB5EntryGsbE* _M0L4_2aeS676 = _M0L7_2aSomeS675;
      struct _M0TPB5EntryGsbE* _M0L15next__in__chainS677 = _M0L4_2aeS676->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2485 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3679 = _M0L4_2aeS676->$1;
      if (_M0L15next__in__chainS677) {
        moonbit_incref(_M0L15next__in__chainS677);
      }
      if (_M0L6_2aoldS3679) {
        moonbit_decref(_M0L6_2aoldS3679);
      }
      _M0L4_2aeS676->$1 = _M0L6_2atmpS2485;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsbE(_M0L4selfS671, _M0L4_2aeS676);
      moonbit_decref(_M0L4_2aeS676);
      _M0L1xS673 = _M0L15next__in__chainS677;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE* _M0L4selfS679) {
  struct _M0TPB5EntryGsfE* _M0L9old__headS678;
  int32_t _M0L8capacityS2494;
  int32_t _M0L13new__capacityS680;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2488;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS2487;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS3687;
  int32_t _M0L6_2atmpS2489;
  int32_t _M0L8capacityS2491;
  int32_t _M0L6_2atmpS2490;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2492;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3686;
  struct _M0TPB5EntryGsfE* _M0L1xS681;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS678 = _M0L4selfS679->$5;
  _M0L8capacityS2494 = _M0L4selfS679->$2;
  _M0L13new__capacityS680 = _M0L8capacityS2494 << 1;
  _M0L6_2atmpS2488 = 0;
  _M0L6_2atmpS2487
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS680, _M0L6_2atmpS2488);
  _M0L6_2aoldS3687 = _M0L4selfS679->$0;
  if (_M0L9old__headS678) {
    moonbit_incref(_M0L9old__headS678);
  }
  moonbit_decref(_M0L6_2aoldS3687);
  _M0L4selfS679->$0 = _M0L6_2atmpS2487;
  _M0L4selfS679->$2 = _M0L13new__capacityS680;
  _M0L6_2atmpS2489 = _M0L13new__capacityS680 - 1;
  _M0L4selfS679->$3 = _M0L6_2atmpS2489;
  _M0L8capacityS2491 = _M0L4selfS679->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2490 = _M0FPB21calc__grow__threshold(_M0L8capacityS2491);
  _M0L4selfS679->$4 = _M0L6_2atmpS2490;
  _M0L4selfS679->$1 = 0;
  _M0L6_2atmpS2492 = 0;
  _M0L6_2aoldS3686 = _M0L4selfS679->$5;
  if (_M0L6_2aoldS3686) {
    moonbit_decref(_M0L6_2aoldS3686);
  }
  _M0L4selfS679->$5 = _M0L6_2atmpS2492;
  _M0L4selfS679->$6 = -1;
  _M0L1xS681 = _M0L9old__headS678;
  while (1) {
    if (_M0L1xS681 == 0) {
      if (_M0L1xS681) {
        moonbit_decref(_M0L1xS681);
      }
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS683 = _M0L1xS681;
      struct _M0TPB5EntryGsfE* _M0L4_2aeS684 = _M0L7_2aSomeS683;
      struct _M0TPB5EntryGsfE* _M0L15next__in__chainS685 = _M0L4_2aeS684->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2493 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3684 = _M0L4_2aeS684->$1;
      if (_M0L15next__in__chainS685) {
        moonbit_incref(_M0L15next__in__chainS685);
      }
      if (_M0L6_2aoldS3684) {
        moonbit_decref(_M0L6_2aoldS3684);
      }
      _M0L4_2aeS684->$1 = _M0L6_2atmpS2493;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsfE(_M0L4selfS679, _M0L4_2aeS684);
      moonbit_decref(_M0L4_2aeS684);
      _M0L1xS681 = _M0L15next__in__chainS685;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE* _M0L4selfS687) {
  struct _M0TPB5EntryGsiE* _M0L9old__headS686;
  int32_t _M0L8capacityS2502;
  int32_t _M0L13new__capacityS688;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2496;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS2495;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS3692;
  int32_t _M0L6_2atmpS2497;
  int32_t _M0L8capacityS2499;
  int32_t _M0L6_2atmpS2498;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2500;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3691;
  struct _M0TPB5EntryGsiE* _M0L1xS689;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS686 = _M0L4selfS687->$5;
  _M0L8capacityS2502 = _M0L4selfS687->$2;
  _M0L13new__capacityS688 = _M0L8capacityS2502 << 1;
  _M0L6_2atmpS2496 = 0;
  _M0L6_2atmpS2495
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS688, _M0L6_2atmpS2496);
  _M0L6_2aoldS3692 = _M0L4selfS687->$0;
  if (_M0L9old__headS686) {
    moonbit_incref(_M0L9old__headS686);
  }
  moonbit_decref(_M0L6_2aoldS3692);
  _M0L4selfS687->$0 = _M0L6_2atmpS2495;
  _M0L4selfS687->$2 = _M0L13new__capacityS688;
  _M0L6_2atmpS2497 = _M0L13new__capacityS688 - 1;
  _M0L4selfS687->$3 = _M0L6_2atmpS2497;
  _M0L8capacityS2499 = _M0L4selfS687->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2498 = _M0FPB21calc__grow__threshold(_M0L8capacityS2499);
  _M0L4selfS687->$4 = _M0L6_2atmpS2498;
  _M0L4selfS687->$1 = 0;
  _M0L6_2atmpS2500 = 0;
  _M0L6_2aoldS3691 = _M0L4selfS687->$5;
  if (_M0L6_2aoldS3691) {
    moonbit_decref(_M0L6_2aoldS3691);
  }
  _M0L4selfS687->$5 = _M0L6_2atmpS2500;
  _M0L4selfS687->$6 = -1;
  _M0L1xS689 = _M0L9old__headS686;
  while (1) {
    if (_M0L1xS689 == 0) {
      if (_M0L1xS689) {
        moonbit_decref(_M0L1xS689);
      }
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS691 = _M0L1xS689;
      struct _M0TPB5EntryGsiE* _M0L4_2aeS692 = _M0L7_2aSomeS691;
      struct _M0TPB5EntryGsiE* _M0L15next__in__chainS693 = _M0L4_2aeS692->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2501 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3689 = _M0L4_2aeS692->$1;
      if (_M0L15next__in__chainS693) {
        moonbit_incref(_M0L15next__in__chainS693);
      }
      if (_M0L6_2aoldS3689) {
        moonbit_decref(_M0L6_2aoldS3689);
      }
      _M0L4_2aeS692->$1 = _M0L6_2atmpS2501;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsiE(_M0L4selfS687, _M0L4_2aeS692);
      moonbit_decref(_M0L4_2aeS692);
      _M0L1xS689 = _M0L15next__in__chainS693;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS614,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5outerS610
) {
  int32_t _M0L4hashS609;
  int32_t _M0L14capacity__maskS2422;
  int32_t _M0L6_2atmpS2421;
  int32_t _M0L3pslS611;
  int32_t _M0L3idxS612;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS609 = _M0L5outerS610->$3;
  _M0L14capacity__maskS2422 = _M0L4selfS614->$3;
  _M0L6_2atmpS2421 = _M0L4hashS609 & _M0L14capacity__maskS2422;
  _M0L3pslS611 = 0;
  _M0L3idxS612 = _M0L6_2atmpS2421;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2420 =
      _M0L4selfS614->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS613;
    if (
      _M0L3idxS612 < 0
      || _M0L3idxS612 >= Moonbit_array_length(_M0L7entriesS2420)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS613
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2420[
        _M0L3idxS612
      ];
    if (_M0L7_2abindS613 == 0) {
      int32_t _M0L4tailS2413;
      _M0L5outerS610->$2 = _M0L3pslS611;
      _M0L4tailS2413 = _M0L4selfS614->$6;
      _M0L5outerS610->$0 = _M0L4tailS2413;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS615 =
        _M0L7_2abindS613;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2acurrS616 =
        _M0L7_2aSomeS615;
      int32_t _M0L3pslS2414 = _M0L7_2acurrS616->$2;
      if (_M0L3pslS611 > _M0L3pslS2414) {
        int32_t _M0L4tailS2415;
        moonbit_incref(_M0L7_2acurrS616);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L7_2acurrS616);
        moonbit_decref(_M0L7_2acurrS616);
        _M0L5outerS610->$2 = _M0L3pslS611;
        _M0L4tailS2415 = _M0L4selfS614->$6;
        _M0L5outerS610->$0 = _M0L4tailS2415;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2416 = _M0L3pslS611 + 1;
        int32_t _M0L6_2atmpS2418 = _M0L3idxS612 + 1;
        int32_t _M0L14capacity__maskS2419 = _M0L4selfS614->$3;
        int32_t _M0L6_2atmpS2417 =
          _M0L6_2atmpS2418 & _M0L14capacity__maskS2419;
        _M0L3pslS611 = _M0L6_2atmpS2416;
        _M0L3idxS612 = _M0L6_2atmpS2417;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS623,
  struct _M0TPB5EntryGssE* _M0L5outerS619
) {
  int32_t _M0L4hashS618;
  int32_t _M0L14capacity__maskS2432;
  int32_t _M0L6_2atmpS2431;
  int32_t _M0L3pslS620;
  int32_t _M0L3idxS621;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS618 = _M0L5outerS619->$3;
  _M0L14capacity__maskS2432 = _M0L4selfS623->$3;
  _M0L6_2atmpS2431 = _M0L4hashS618 & _M0L14capacity__maskS2432;
  _M0L3pslS620 = 0;
  _M0L3idxS621 = _M0L6_2atmpS2431;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2430 = _M0L4selfS623->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS622;
    if (
      _M0L3idxS621 < 0
      || _M0L3idxS621 >= Moonbit_array_length(_M0L7entriesS2430)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS622
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2430[_M0L3idxS621];
    if (_M0L7_2abindS622 == 0) {
      int32_t _M0L4tailS2423;
      _M0L5outerS619->$2 = _M0L3pslS620;
      _M0L4tailS2423 = _M0L4selfS623->$6;
      _M0L5outerS619->$0 = _M0L4tailS2423;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS624 = _M0L7_2abindS622;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS625 = _M0L7_2aSomeS624;
      int32_t _M0L3pslS2424 = _M0L7_2acurrS625->$2;
      if (_M0L3pslS620 > _M0L3pslS2424) {
        int32_t _M0L4tailS2425;
        moonbit_incref(_M0L7_2acurrS625);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS623, _M0L3idxS621, _M0L7_2acurrS625);
        moonbit_decref(_M0L7_2acurrS625);
        _M0L5outerS619->$2 = _M0L3pslS620;
        _M0L4tailS2425 = _M0L4selfS623->$6;
        _M0L5outerS619->$0 = _M0L4tailS2425;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2426 = _M0L3pslS620 + 1;
        int32_t _M0L6_2atmpS2428 = _M0L3idxS621 + 1;
        int32_t _M0L14capacity__maskS2429 = _M0L4selfS623->$3;
        int32_t _M0L6_2atmpS2427 =
          _M0L6_2atmpS2428 & _M0L14capacity__maskS2429;
        _M0L3pslS620 = _M0L6_2atmpS2426;
        _M0L3idxS621 = _M0L6_2atmpS2427;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS632,
  struct _M0TPB5EntryGsbE* _M0L5outerS628
) {
  int32_t _M0L4hashS627;
  int32_t _M0L14capacity__maskS2442;
  int32_t _M0L6_2atmpS2441;
  int32_t _M0L3pslS629;
  int32_t _M0L3idxS630;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS627 = _M0L5outerS628->$3;
  _M0L14capacity__maskS2442 = _M0L4selfS632->$3;
  _M0L6_2atmpS2441 = _M0L4hashS627 & _M0L14capacity__maskS2442;
  _M0L3pslS629 = 0;
  _M0L3idxS630 = _M0L6_2atmpS2441;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2440 = _M0L4selfS632->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS631;
    if (
      _M0L3idxS630 < 0
      || _M0L3idxS630 >= Moonbit_array_length(_M0L7entriesS2440)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS631
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2440[_M0L3idxS630];
    if (_M0L7_2abindS631 == 0) {
      int32_t _M0L4tailS2433;
      _M0L5outerS628->$2 = _M0L3pslS629;
      _M0L4tailS2433 = _M0L4selfS632->$6;
      _M0L5outerS628->$0 = _M0L4tailS2433;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS632, _M0L3idxS630, _M0L5outerS628);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS633 = _M0L7_2abindS631;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS634 = _M0L7_2aSomeS633;
      int32_t _M0L3pslS2434 = _M0L7_2acurrS634->$2;
      if (_M0L3pslS629 > _M0L3pslS2434) {
        int32_t _M0L4tailS2435;
        moonbit_incref(_M0L7_2acurrS634);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS632, _M0L3idxS630, _M0L7_2acurrS634);
        moonbit_decref(_M0L7_2acurrS634);
        _M0L5outerS628->$2 = _M0L3pslS629;
        _M0L4tailS2435 = _M0L4selfS632->$6;
        _M0L5outerS628->$0 = _M0L4tailS2435;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS632, _M0L3idxS630, _M0L5outerS628);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2436 = _M0L3pslS629 + 1;
        int32_t _M0L6_2atmpS2438 = _M0L3idxS630 + 1;
        int32_t _M0L14capacity__maskS2439 = _M0L4selfS632->$3;
        int32_t _M0L6_2atmpS2437 =
          _M0L6_2atmpS2438 & _M0L14capacity__maskS2439;
        _M0L3pslS629 = _M0L6_2atmpS2436;
        _M0L3idxS630 = _M0L6_2atmpS2437;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS641,
  struct _M0TPB5EntryGsfE* _M0L5outerS637
) {
  int32_t _M0L4hashS636;
  int32_t _M0L14capacity__maskS2452;
  int32_t _M0L6_2atmpS2451;
  int32_t _M0L3pslS638;
  int32_t _M0L3idxS639;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS636 = _M0L5outerS637->$3;
  _M0L14capacity__maskS2452 = _M0L4selfS641->$3;
  _M0L6_2atmpS2451 = _M0L4hashS636 & _M0L14capacity__maskS2452;
  _M0L3pslS638 = 0;
  _M0L3idxS639 = _M0L6_2atmpS2451;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2450 = _M0L4selfS641->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS640;
    if (
      _M0L3idxS639 < 0
      || _M0L3idxS639 >= Moonbit_array_length(_M0L7entriesS2450)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS640
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2450[_M0L3idxS639];
    if (_M0L7_2abindS640 == 0) {
      int32_t _M0L4tailS2443;
      _M0L5outerS637->$2 = _M0L3pslS638;
      _M0L4tailS2443 = _M0L4selfS641->$6;
      _M0L5outerS637->$0 = _M0L4tailS2443;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5outerS637);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS642 = _M0L7_2abindS640;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS643 = _M0L7_2aSomeS642;
      int32_t _M0L3pslS2444 = _M0L7_2acurrS643->$2;
      if (_M0L3pslS638 > _M0L3pslS2444) {
        int32_t _M0L4tailS2445;
        moonbit_incref(_M0L7_2acurrS643);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS641, _M0L3idxS639, _M0L7_2acurrS643);
        moonbit_decref(_M0L7_2acurrS643);
        _M0L5outerS637->$2 = _M0L3pslS638;
        _M0L4tailS2445 = _M0L4selfS641->$6;
        _M0L5outerS637->$0 = _M0L4tailS2445;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5outerS637);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2446 = _M0L3pslS638 + 1;
        int32_t _M0L6_2atmpS2448 = _M0L3idxS639 + 1;
        int32_t _M0L14capacity__maskS2449 = _M0L4selfS641->$3;
        int32_t _M0L6_2atmpS2447 =
          _M0L6_2atmpS2448 & _M0L14capacity__maskS2449;
        _M0L3pslS638 = _M0L6_2atmpS2446;
        _M0L3idxS639 = _M0L6_2atmpS2447;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS650,
  struct _M0TPB5EntryGsiE* _M0L5outerS646
) {
  int32_t _M0L4hashS645;
  int32_t _M0L14capacity__maskS2462;
  int32_t _M0L6_2atmpS2461;
  int32_t _M0L3pslS647;
  int32_t _M0L3idxS648;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS645 = _M0L5outerS646->$3;
  _M0L14capacity__maskS2462 = _M0L4selfS650->$3;
  _M0L6_2atmpS2461 = _M0L4hashS645 & _M0L14capacity__maskS2462;
  _M0L3pslS647 = 0;
  _M0L3idxS648 = _M0L6_2atmpS2461;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2460 = _M0L4selfS650->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS649;
    if (
      _M0L3idxS648 < 0
      || _M0L3idxS648 >= Moonbit_array_length(_M0L7entriesS2460)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS649
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2460[_M0L3idxS648];
    if (_M0L7_2abindS649 == 0) {
      int32_t _M0L4tailS2453;
      _M0L5outerS646->$2 = _M0L3pslS647;
      _M0L4tailS2453 = _M0L4selfS650->$6;
      _M0L5outerS646->$0 = _M0L4tailS2453;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS650, _M0L3idxS648, _M0L5outerS646);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS651 = _M0L7_2abindS649;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS652 = _M0L7_2aSomeS651;
      int32_t _M0L3pslS2454 = _M0L7_2acurrS652->$2;
      if (_M0L3pslS647 > _M0L3pslS2454) {
        int32_t _M0L4tailS2455;
        moonbit_incref(_M0L7_2acurrS652);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS650, _M0L3idxS648, _M0L7_2acurrS652);
        moonbit_decref(_M0L7_2acurrS652);
        _M0L5outerS646->$2 = _M0L3pslS647;
        _M0L4tailS2455 = _M0L4selfS650->$6;
        _M0L5outerS646->$0 = _M0L4tailS2455;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS650, _M0L3idxS648, _M0L5outerS646);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2456 = _M0L3pslS647 + 1;
        int32_t _M0L6_2atmpS2458 = _M0L3idxS648 + 1;
        int32_t _M0L14capacity__maskS2459 = _M0L4selfS650->$3;
        int32_t _M0L6_2atmpS2457 =
          _M0L6_2atmpS2458 & _M0L14capacity__maskS2459;
        _M0L3pslS647 = _M0L6_2atmpS2456;
        _M0L3idxS648 = _M0L6_2atmpS2457;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS563,
  int32_t _M0L3idxS568,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS567
) {
  int32_t _M0L3pslS2348;
  int32_t _M0L6_2atmpS2344;
  int32_t _M0L6_2atmpS2346;
  int32_t _M0L14capacity__maskS2347;
  int32_t _M0L6_2atmpS2345;
  int32_t _M0L3pslS559;
  int32_t _M0L3idxS560;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS561;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2348 = _M0L5entryS567->$2;
  _M0L6_2atmpS2344 = _M0L3pslS2348 + 1;
  _M0L6_2atmpS2346 = _M0L3idxS568 + 1;
  _M0L14capacity__maskS2347 = _M0L4selfS563->$3;
  _M0L6_2atmpS2345 = _M0L6_2atmpS2346 & _M0L14capacity__maskS2347;
  moonbit_incref(_M0L5entryS567);
  _M0L3pslS559 = _M0L6_2atmpS2344;
  _M0L3idxS560 = _M0L6_2atmpS2345;
  _M0L5entryS561 = _M0L5entryS567;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2343 =
      _M0L4selfS563->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS562;
    if (
      _M0L3idxS560 < 0
      || _M0L3idxS560 >= Moonbit_array_length(_M0L7entriesS2343)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS562
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2343[
        _M0L3idxS560
      ];
    if (_M0L7_2abindS562 == 0) {
      _M0L5entryS561->$2 = _M0L3pslS559;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS563, _M0L5entryS561, _M0L3idxS560);
      moonbit_decref(_M0L5entryS561);
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS565 =
        _M0L7_2abindS562;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS566 =
        _M0L7_2aSomeS565;
      int32_t _M0L3pslS2333 = _M0L14_2acurr__entryS566->$2;
      if (_M0L3pslS559 > _M0L3pslS2333) {
        int32_t _M0L3pslS2338;
        int32_t _M0L6_2atmpS2334;
        int32_t _M0L6_2atmpS2336;
        int32_t _M0L14capacity__maskS2337;
        int32_t _M0L6_2atmpS2335;
        _M0L5entryS561->$2 = _M0L3pslS559;
        moonbit_incref(_M0L14_2acurr__entryS566);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS563, _M0L5entryS561, _M0L3idxS560);
        moonbit_decref(_M0L5entryS561);
        _M0L3pslS2338 = _M0L14_2acurr__entryS566->$2;
        _M0L6_2atmpS2334 = _M0L3pslS2338 + 1;
        _M0L6_2atmpS2336 = _M0L3idxS560 + 1;
        _M0L14capacity__maskS2337 = _M0L4selfS563->$3;
        _M0L6_2atmpS2335 = _M0L6_2atmpS2336 & _M0L14capacity__maskS2337;
        _M0L3pslS559 = _M0L6_2atmpS2334;
        _M0L3idxS560 = _M0L6_2atmpS2335;
        _M0L5entryS561 = _M0L14_2acurr__entryS566;
        continue;
      } else {
        int32_t _M0L6_2atmpS2339 = _M0L3pslS559 + 1;
        int32_t _M0L6_2atmpS2341 = _M0L3idxS560 + 1;
        int32_t _M0L14capacity__maskS2342 = _M0L4selfS563->$3;
        int32_t _M0L6_2atmpS2340 =
          _M0L6_2atmpS2341 & _M0L14capacity__maskS2342;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _tmp_4078 =
          _M0L5entryS561;
        _M0L3pslS559 = _M0L6_2atmpS2339;
        _M0L3idxS560 = _M0L6_2atmpS2340;
        _M0L5entryS561 = _tmp_4078;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE* _M0L4selfS573,
  int32_t _M0L3idxS578,
  struct _M0TPB5EntryGssE* _M0L5entryS577
) {
  int32_t _M0L3pslS2364;
  int32_t _M0L6_2atmpS2360;
  int32_t _M0L6_2atmpS2362;
  int32_t _M0L14capacity__maskS2363;
  int32_t _M0L6_2atmpS2361;
  int32_t _M0L3pslS569;
  int32_t _M0L3idxS570;
  struct _M0TPB5EntryGssE* _M0L5entryS571;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2364 = _M0L5entryS577->$2;
  _M0L6_2atmpS2360 = _M0L3pslS2364 + 1;
  _M0L6_2atmpS2362 = _M0L3idxS578 + 1;
  _M0L14capacity__maskS2363 = _M0L4selfS573->$3;
  _M0L6_2atmpS2361 = _M0L6_2atmpS2362 & _M0L14capacity__maskS2363;
  moonbit_incref(_M0L5entryS577);
  _M0L3pslS569 = _M0L6_2atmpS2360;
  _M0L3idxS570 = _M0L6_2atmpS2361;
  _M0L5entryS571 = _M0L5entryS577;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2359 = _M0L4selfS573->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS572;
    if (
      _M0L3idxS570 < 0
      || _M0L3idxS570 >= Moonbit_array_length(_M0L7entriesS2359)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS572
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2359[_M0L3idxS570];
    if (_M0L7_2abindS572 == 0) {
      _M0L5entryS571->$2 = _M0L3pslS569;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS573, _M0L5entryS571, _M0L3idxS570);
      moonbit_decref(_M0L5entryS571);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS575 = _M0L7_2abindS572;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS576 = _M0L7_2aSomeS575;
      int32_t _M0L3pslS2349 = _M0L14_2acurr__entryS576->$2;
      if (_M0L3pslS569 > _M0L3pslS2349) {
        int32_t _M0L3pslS2354;
        int32_t _M0L6_2atmpS2350;
        int32_t _M0L6_2atmpS2352;
        int32_t _M0L14capacity__maskS2353;
        int32_t _M0L6_2atmpS2351;
        _M0L5entryS571->$2 = _M0L3pslS569;
        moonbit_incref(_M0L14_2acurr__entryS576);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS573, _M0L5entryS571, _M0L3idxS570);
        moonbit_decref(_M0L5entryS571);
        _M0L3pslS2354 = _M0L14_2acurr__entryS576->$2;
        _M0L6_2atmpS2350 = _M0L3pslS2354 + 1;
        _M0L6_2atmpS2352 = _M0L3idxS570 + 1;
        _M0L14capacity__maskS2353 = _M0L4selfS573->$3;
        _M0L6_2atmpS2351 = _M0L6_2atmpS2352 & _M0L14capacity__maskS2353;
        _M0L3pslS569 = _M0L6_2atmpS2350;
        _M0L3idxS570 = _M0L6_2atmpS2351;
        _M0L5entryS571 = _M0L14_2acurr__entryS576;
        continue;
      } else {
        int32_t _M0L6_2atmpS2355 = _M0L3pslS569 + 1;
        int32_t _M0L6_2atmpS2357 = _M0L3idxS570 + 1;
        int32_t _M0L14capacity__maskS2358 = _M0L4selfS573->$3;
        int32_t _M0L6_2atmpS2356 =
          _M0L6_2atmpS2357 & _M0L14capacity__maskS2358;
        struct _M0TPB5EntryGssE* _tmp_4080 = _M0L5entryS571;
        _M0L3pslS569 = _M0L6_2atmpS2355;
        _M0L3idxS570 = _M0L6_2atmpS2356;
        _M0L5entryS571 = _tmp_4080;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS583,
  int32_t _M0L3idxS588,
  struct _M0TPB5EntryGsbE* _M0L5entryS587
) {
  int32_t _M0L3pslS2380;
  int32_t _M0L6_2atmpS2376;
  int32_t _M0L6_2atmpS2378;
  int32_t _M0L14capacity__maskS2379;
  int32_t _M0L6_2atmpS2377;
  int32_t _M0L3pslS579;
  int32_t _M0L3idxS580;
  struct _M0TPB5EntryGsbE* _M0L5entryS581;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2380 = _M0L5entryS587->$2;
  _M0L6_2atmpS2376 = _M0L3pslS2380 + 1;
  _M0L6_2atmpS2378 = _M0L3idxS588 + 1;
  _M0L14capacity__maskS2379 = _M0L4selfS583->$3;
  _M0L6_2atmpS2377 = _M0L6_2atmpS2378 & _M0L14capacity__maskS2379;
  moonbit_incref(_M0L5entryS587);
  _M0L3pslS579 = _M0L6_2atmpS2376;
  _M0L3idxS580 = _M0L6_2atmpS2377;
  _M0L5entryS581 = _M0L5entryS587;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2375 = _M0L4selfS583->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS582;
    if (
      _M0L3idxS580 < 0
      || _M0L3idxS580 >= Moonbit_array_length(_M0L7entriesS2375)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS582
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2375[_M0L3idxS580];
    if (_M0L7_2abindS582 == 0) {
      _M0L5entryS581->$2 = _M0L3pslS579;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
      moonbit_decref(_M0L5entryS581);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS585 = _M0L7_2abindS582;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS586 = _M0L7_2aSomeS585;
      int32_t _M0L3pslS2365 = _M0L14_2acurr__entryS586->$2;
      if (_M0L3pslS579 > _M0L3pslS2365) {
        int32_t _M0L3pslS2370;
        int32_t _M0L6_2atmpS2366;
        int32_t _M0L6_2atmpS2368;
        int32_t _M0L14capacity__maskS2369;
        int32_t _M0L6_2atmpS2367;
        _M0L5entryS581->$2 = _M0L3pslS579;
        moonbit_incref(_M0L14_2acurr__entryS586);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
        moonbit_decref(_M0L5entryS581);
        _M0L3pslS2370 = _M0L14_2acurr__entryS586->$2;
        _M0L6_2atmpS2366 = _M0L3pslS2370 + 1;
        _M0L6_2atmpS2368 = _M0L3idxS580 + 1;
        _M0L14capacity__maskS2369 = _M0L4selfS583->$3;
        _M0L6_2atmpS2367 = _M0L6_2atmpS2368 & _M0L14capacity__maskS2369;
        _M0L3pslS579 = _M0L6_2atmpS2366;
        _M0L3idxS580 = _M0L6_2atmpS2367;
        _M0L5entryS581 = _M0L14_2acurr__entryS586;
        continue;
      } else {
        int32_t _M0L6_2atmpS2371 = _M0L3pslS579 + 1;
        int32_t _M0L6_2atmpS2373 = _M0L3idxS580 + 1;
        int32_t _M0L14capacity__maskS2374 = _M0L4selfS583->$3;
        int32_t _M0L6_2atmpS2372 =
          _M0L6_2atmpS2373 & _M0L14capacity__maskS2374;
        struct _M0TPB5EntryGsbE* _tmp_4082 = _M0L5entryS581;
        _M0L3pslS579 = _M0L6_2atmpS2371;
        _M0L3idxS580 = _M0L6_2atmpS2372;
        _M0L5entryS581 = _tmp_4082;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS593,
  int32_t _M0L3idxS598,
  struct _M0TPB5EntryGsfE* _M0L5entryS597
) {
  int32_t _M0L3pslS2396;
  int32_t _M0L6_2atmpS2392;
  int32_t _M0L6_2atmpS2394;
  int32_t _M0L14capacity__maskS2395;
  int32_t _M0L6_2atmpS2393;
  int32_t _M0L3pslS589;
  int32_t _M0L3idxS590;
  struct _M0TPB5EntryGsfE* _M0L5entryS591;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2396 = _M0L5entryS597->$2;
  _M0L6_2atmpS2392 = _M0L3pslS2396 + 1;
  _M0L6_2atmpS2394 = _M0L3idxS598 + 1;
  _M0L14capacity__maskS2395 = _M0L4selfS593->$3;
  _M0L6_2atmpS2393 = _M0L6_2atmpS2394 & _M0L14capacity__maskS2395;
  moonbit_incref(_M0L5entryS597);
  _M0L3pslS589 = _M0L6_2atmpS2392;
  _M0L3idxS590 = _M0L6_2atmpS2393;
  _M0L5entryS591 = _M0L5entryS597;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2391 = _M0L4selfS593->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS592;
    if (
      _M0L3idxS590 < 0
      || _M0L3idxS590 >= Moonbit_array_length(_M0L7entriesS2391)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS592
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2391[_M0L3idxS590];
    if (_M0L7_2abindS592 == 0) {
      _M0L5entryS591->$2 = _M0L3pslS589;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
      moonbit_decref(_M0L5entryS591);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS595 = _M0L7_2abindS592;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS596 = _M0L7_2aSomeS595;
      int32_t _M0L3pslS2381 = _M0L14_2acurr__entryS596->$2;
      if (_M0L3pslS589 > _M0L3pslS2381) {
        int32_t _M0L3pslS2386;
        int32_t _M0L6_2atmpS2382;
        int32_t _M0L6_2atmpS2384;
        int32_t _M0L14capacity__maskS2385;
        int32_t _M0L6_2atmpS2383;
        _M0L5entryS591->$2 = _M0L3pslS589;
        moonbit_incref(_M0L14_2acurr__entryS596);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
        moonbit_decref(_M0L5entryS591);
        _M0L3pslS2386 = _M0L14_2acurr__entryS596->$2;
        _M0L6_2atmpS2382 = _M0L3pslS2386 + 1;
        _M0L6_2atmpS2384 = _M0L3idxS590 + 1;
        _M0L14capacity__maskS2385 = _M0L4selfS593->$3;
        _M0L6_2atmpS2383 = _M0L6_2atmpS2384 & _M0L14capacity__maskS2385;
        _M0L3pslS589 = _M0L6_2atmpS2382;
        _M0L3idxS590 = _M0L6_2atmpS2383;
        _M0L5entryS591 = _M0L14_2acurr__entryS596;
        continue;
      } else {
        int32_t _M0L6_2atmpS2387 = _M0L3pslS589 + 1;
        int32_t _M0L6_2atmpS2389 = _M0L3idxS590 + 1;
        int32_t _M0L14capacity__maskS2390 = _M0L4selfS593->$3;
        int32_t _M0L6_2atmpS2388 =
          _M0L6_2atmpS2389 & _M0L14capacity__maskS2390;
        struct _M0TPB5EntryGsfE* _tmp_4084 = _M0L5entryS591;
        _M0L3pslS589 = _M0L6_2atmpS2387;
        _M0L3idxS590 = _M0L6_2atmpS2388;
        _M0L5entryS591 = _tmp_4084;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS603,
  int32_t _M0L3idxS608,
  struct _M0TPB5EntryGsiE* _M0L5entryS607
) {
  int32_t _M0L3pslS2412;
  int32_t _M0L6_2atmpS2408;
  int32_t _M0L6_2atmpS2410;
  int32_t _M0L14capacity__maskS2411;
  int32_t _M0L6_2atmpS2409;
  int32_t _M0L3pslS599;
  int32_t _M0L3idxS600;
  struct _M0TPB5EntryGsiE* _M0L5entryS601;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2412 = _M0L5entryS607->$2;
  _M0L6_2atmpS2408 = _M0L3pslS2412 + 1;
  _M0L6_2atmpS2410 = _M0L3idxS608 + 1;
  _M0L14capacity__maskS2411 = _M0L4selfS603->$3;
  _M0L6_2atmpS2409 = _M0L6_2atmpS2410 & _M0L14capacity__maskS2411;
  moonbit_incref(_M0L5entryS607);
  _M0L3pslS599 = _M0L6_2atmpS2408;
  _M0L3idxS600 = _M0L6_2atmpS2409;
  _M0L5entryS601 = _M0L5entryS607;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2407 = _M0L4selfS603->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS602;
    if (
      _M0L3idxS600 < 0
      || _M0L3idxS600 >= Moonbit_array_length(_M0L7entriesS2407)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS602
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2407[_M0L3idxS600];
    if (_M0L7_2abindS602 == 0) {
      _M0L5entryS601->$2 = _M0L3pslS599;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
      moonbit_decref(_M0L5entryS601);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS605 = _M0L7_2abindS602;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS606 = _M0L7_2aSomeS605;
      int32_t _M0L3pslS2397 = _M0L14_2acurr__entryS606->$2;
      if (_M0L3pslS599 > _M0L3pslS2397) {
        int32_t _M0L3pslS2402;
        int32_t _M0L6_2atmpS2398;
        int32_t _M0L6_2atmpS2400;
        int32_t _M0L14capacity__maskS2401;
        int32_t _M0L6_2atmpS2399;
        _M0L5entryS601->$2 = _M0L3pslS599;
        moonbit_incref(_M0L14_2acurr__entryS606);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
        moonbit_decref(_M0L5entryS601);
        _M0L3pslS2402 = _M0L14_2acurr__entryS606->$2;
        _M0L6_2atmpS2398 = _M0L3pslS2402 + 1;
        _M0L6_2atmpS2400 = _M0L3idxS600 + 1;
        _M0L14capacity__maskS2401 = _M0L4selfS603->$3;
        _M0L6_2atmpS2399 = _M0L6_2atmpS2400 & _M0L14capacity__maskS2401;
        _M0L3pslS599 = _M0L6_2atmpS2398;
        _M0L3idxS600 = _M0L6_2atmpS2399;
        _M0L5entryS601 = _M0L14_2acurr__entryS606;
        continue;
      } else {
        int32_t _M0L6_2atmpS2403 = _M0L3pslS599 + 1;
        int32_t _M0L6_2atmpS2405 = _M0L3idxS600 + 1;
        int32_t _M0L14capacity__maskS2406 = _M0L4selfS603->$3;
        int32_t _M0L6_2atmpS2404 =
          _M0L6_2atmpS2405 & _M0L14capacity__maskS2406;
        struct _M0TPB5EntryGsiE* _tmp_4086 = _M0L5entryS601;
        _M0L3pslS599 = _M0L6_2atmpS2403;
        _M0L3idxS600 = _M0L6_2atmpS2404;
        _M0L5entryS601 = _tmp_4086;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS529,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS531,
  int32_t _M0L8new__idxS530
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2323;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2324;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3715;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS532;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2323 = _M0L4selfS529->$0;
  _M0L6_2atmpS2324 = _M0L5entryS531;
  if (
    _M0L8new__idxS530 < 0
    || _M0L8new__idxS530 >= Moonbit_array_length(_M0L7entriesS2323)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3715
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2323[
      _M0L8new__idxS530
    ];
  if (_M0L6_2atmpS2324) {
    moonbit_incref(_M0L6_2atmpS2324);
  }
  if (_M0L6_2aoldS3715) {
    moonbit_decref(_M0L6_2aoldS3715);
  }
  _M0L7entriesS2323[_M0L8new__idxS530] = _M0L6_2atmpS2324;
  _M0L7_2abindS532 = _M0L5entryS531->$1;
  if (_M0L7_2abindS532 == 0) {
    _M0L4selfS529->$6 = _M0L8new__idxS530;
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS533 =
      _M0L7_2abindS532;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS534 =
      _M0L7_2aSomeS533;
    _M0L7_2anextS534->$0 = _M0L8new__idxS530;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS535,
  struct _M0TPB5EntryGsiE* _M0L5entryS537,
  int32_t _M0L8new__idxS536
) {
  struct _M0TPB5EntryGsiE** _M0L7entriesS2325;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2326;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3718;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS538;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2325 = _M0L4selfS535->$0;
  _M0L6_2atmpS2326 = _M0L5entryS537;
  if (
    _M0L8new__idxS536 < 0
    || _M0L8new__idxS536 >= Moonbit_array_length(_M0L7entriesS2325)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3718
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2325[_M0L8new__idxS536];
  if (_M0L6_2atmpS2326) {
    moonbit_incref(_M0L6_2atmpS2326);
  }
  if (_M0L6_2aoldS3718) {
    moonbit_decref(_M0L6_2aoldS3718);
  }
  _M0L7entriesS2325[_M0L8new__idxS536] = _M0L6_2atmpS2326;
  _M0L7_2abindS538 = _M0L5entryS537->$1;
  if (_M0L7_2abindS538 == 0) {
    _M0L4selfS535->$6 = _M0L8new__idxS536;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS539 = _M0L7_2abindS538;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS540 = _M0L7_2aSomeS539;
    _M0L7_2anextS540->$0 = _M0L8new__idxS536;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS541,
  struct _M0TPB5EntryGssE* _M0L5entryS543,
  int32_t _M0L8new__idxS542
) {
  struct _M0TPB5EntryGssE** _M0L7entriesS2327;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2328;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3721;
  struct _M0TPB5EntryGssE* _M0L7_2abindS544;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2327 = _M0L4selfS541->$0;
  _M0L6_2atmpS2328 = _M0L5entryS543;
  if (
    _M0L8new__idxS542 < 0
    || _M0L8new__idxS542 >= Moonbit_array_length(_M0L7entriesS2327)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3721
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2327[_M0L8new__idxS542];
  if (_M0L6_2atmpS2328) {
    moonbit_incref(_M0L6_2atmpS2328);
  }
  if (_M0L6_2aoldS3721) {
    moonbit_decref(_M0L6_2aoldS3721);
  }
  _M0L7entriesS2327[_M0L8new__idxS542] = _M0L6_2atmpS2328;
  _M0L7_2abindS544 = _M0L5entryS543->$1;
  if (_M0L7_2abindS544 == 0) {
    _M0L4selfS541->$6 = _M0L8new__idxS542;
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS545 = _M0L7_2abindS544;
    struct _M0TPB5EntryGssE* _M0L7_2anextS546 = _M0L7_2aSomeS545;
    _M0L7_2anextS546->$0 = _M0L8new__idxS542;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS547,
  struct _M0TPB5EntryGsbE* _M0L5entryS549,
  int32_t _M0L8new__idxS548
) {
  struct _M0TPB5EntryGsbE** _M0L7entriesS2329;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2330;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3724;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS550;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2329 = _M0L4selfS547->$0;
  _M0L6_2atmpS2330 = _M0L5entryS549;
  if (
    _M0L8new__idxS548 < 0
    || _M0L8new__idxS548 >= Moonbit_array_length(_M0L7entriesS2329)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3724
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2329[_M0L8new__idxS548];
  if (_M0L6_2atmpS2330) {
    moonbit_incref(_M0L6_2atmpS2330);
  }
  if (_M0L6_2aoldS3724) {
    moonbit_decref(_M0L6_2aoldS3724);
  }
  _M0L7entriesS2329[_M0L8new__idxS548] = _M0L6_2atmpS2330;
  _M0L7_2abindS550 = _M0L5entryS549->$1;
  if (_M0L7_2abindS550 == 0) {
    _M0L4selfS547->$6 = _M0L8new__idxS548;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS551 = _M0L7_2abindS550;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS552 = _M0L7_2aSomeS551;
    _M0L7_2anextS552->$0 = _M0L8new__idxS548;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS553,
  struct _M0TPB5EntryGsfE* _M0L5entryS555,
  int32_t _M0L8new__idxS554
) {
  struct _M0TPB5EntryGsfE** _M0L7entriesS2331;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2332;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3727;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS556;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2331 = _M0L4selfS553->$0;
  _M0L6_2atmpS2332 = _M0L5entryS555;
  if (
    _M0L8new__idxS554 < 0
    || _M0L8new__idxS554 >= Moonbit_array_length(_M0L7entriesS2331)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3727
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2331[_M0L8new__idxS554];
  if (_M0L6_2atmpS2332) {
    moonbit_incref(_M0L6_2atmpS2332);
  }
  if (_M0L6_2aoldS3727) {
    moonbit_decref(_M0L6_2aoldS3727);
  }
  _M0L7entriesS2331[_M0L8new__idxS554] = _M0L6_2atmpS2332;
  _M0L7_2abindS556 = _M0L5entryS555->$1;
  if (_M0L7_2abindS556 == 0) {
    _M0L4selfS553->$6 = _M0L8new__idxS554;
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS557 = _M0L7_2abindS556;
    struct _M0TPB5EntryGsfE* _M0L7_2anextS558 = _M0L7_2aSomeS557;
    _M0L7_2anextS558->$0 = _M0L8new__idxS554;
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS510,
  int32_t _M0L3idxS512,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS511
) {
  int32_t _M0L7_2abindS509;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2283;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2284;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3729;
  int32_t _M0L4sizeS2286;
  int32_t _M0L6_2atmpS2285;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS509 = _M0L4selfS510->$6;
  switch (_M0L7_2abindS509) {
    case -1: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2278 =
        _M0L5entryS511;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3731 =
        _M0L4selfS510->$5;
      if (_M0L6_2atmpS2278) {
        moonbit_incref(_M0L6_2atmpS2278);
      }
      if (_M0L6_2aoldS3731) {
        moonbit_decref(_M0L6_2aoldS3731);
      }
      _M0L4selfS510->$5 = _M0L6_2atmpS2278;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2282 =
        _M0L4selfS510->$0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2281;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2279;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2280;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3732;
      if (
        _M0L7_2abindS509 < 0
        || _M0L7_2abindS509 >= Moonbit_array_length(_M0L7entriesS2282)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2281
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2282[
          _M0L7_2abindS509
        ];
      if (_M0L6_2atmpS2281) {
        moonbit_incref(_M0L6_2atmpS2281);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2279
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2281);
      if (_M0L6_2atmpS2281) {
        moonbit_decref(_M0L6_2atmpS2281);
      }
      _M0L6_2atmpS2280 = _M0L5entryS511;
      _M0L6_2aoldS3732 = _M0L6_2atmpS2279->$1;
      if (_M0L6_2atmpS2280) {
        moonbit_incref(_M0L6_2atmpS2280);
      }
      if (_M0L6_2aoldS3732) {
        moonbit_decref(_M0L6_2aoldS3732);
      }
      _M0L6_2atmpS2279->$1 = _M0L6_2atmpS2280;
      moonbit_decref(_M0L6_2atmpS2279);
      break;
    }
  }
  _M0L4selfS510->$6 = _M0L3idxS512;
  _M0L7entriesS2283 = _M0L4selfS510->$0;
  _M0L6_2atmpS2284 = _M0L5entryS511;
  if (
    _M0L3idxS512 < 0
    || _M0L3idxS512 >= Moonbit_array_length(_M0L7entriesS2283)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3729
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2283[
      _M0L3idxS512
    ];
  if (_M0L6_2atmpS2284) {
    moonbit_incref(_M0L6_2atmpS2284);
  }
  if (_M0L6_2aoldS3729) {
    moonbit_decref(_M0L6_2aoldS3729);
  }
  _M0L7entriesS2283[_M0L3idxS512] = _M0L6_2atmpS2284;
  _M0L4sizeS2286 = _M0L4selfS510->$1;
  _M0L6_2atmpS2285 = _M0L4sizeS2286 + 1;
  _M0L4selfS510->$1 = _M0L6_2atmpS2285;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS514,
  int32_t _M0L3idxS516,
  struct _M0TPB5EntryGssE* _M0L5entryS515
) {
  int32_t _M0L7_2abindS513;
  struct _M0TPB5EntryGssE** _M0L7entriesS2292;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2293;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3735;
  int32_t _M0L4sizeS2295;
  int32_t _M0L6_2atmpS2294;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS513 = _M0L4selfS514->$6;
  switch (_M0L7_2abindS513) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2287 = _M0L5entryS515;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3737 = _M0L4selfS514->$5;
      if (_M0L6_2atmpS2287) {
        moonbit_incref(_M0L6_2atmpS2287);
      }
      if (_M0L6_2aoldS3737) {
        moonbit_decref(_M0L6_2aoldS3737);
      }
      _M0L4selfS514->$5 = _M0L6_2atmpS2287;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS2291 = _M0L4selfS514->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2290;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2288;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2289;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3738;
      if (
        _M0L7_2abindS513 < 0
        || _M0L7_2abindS513 >= Moonbit_array_length(_M0L7entriesS2291)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2290
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS2291[_M0L7_2abindS513];
      if (_M0L6_2atmpS2290) {
        moonbit_incref(_M0L6_2atmpS2290);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2288
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS2290);
      if (_M0L6_2atmpS2290) {
        moonbit_decref(_M0L6_2atmpS2290);
      }
      _M0L6_2atmpS2289 = _M0L5entryS515;
      _M0L6_2aoldS3738 = _M0L6_2atmpS2288->$1;
      if (_M0L6_2atmpS2289) {
        moonbit_incref(_M0L6_2atmpS2289);
      }
      if (_M0L6_2aoldS3738) {
        moonbit_decref(_M0L6_2aoldS3738);
      }
      _M0L6_2atmpS2288->$1 = _M0L6_2atmpS2289;
      moonbit_decref(_M0L6_2atmpS2288);
      break;
    }
  }
  _M0L4selfS514->$6 = _M0L3idxS516;
  _M0L7entriesS2292 = _M0L4selfS514->$0;
  _M0L6_2atmpS2293 = _M0L5entryS515;
  if (
    _M0L3idxS516 < 0
    || _M0L3idxS516 >= Moonbit_array_length(_M0L7entriesS2292)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3735
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2292[_M0L3idxS516];
  if (_M0L6_2atmpS2293) {
    moonbit_incref(_M0L6_2atmpS2293);
  }
  if (_M0L6_2aoldS3735) {
    moonbit_decref(_M0L6_2aoldS3735);
  }
  _M0L7entriesS2292[_M0L3idxS516] = _M0L6_2atmpS2293;
  _M0L4sizeS2295 = _M0L4selfS514->$1;
  _M0L6_2atmpS2294 = _M0L4sizeS2295 + 1;
  _M0L4selfS514->$1 = _M0L6_2atmpS2294;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS518,
  int32_t _M0L3idxS520,
  struct _M0TPB5EntryGsbE* _M0L5entryS519
) {
  int32_t _M0L7_2abindS517;
  struct _M0TPB5EntryGsbE** _M0L7entriesS2301;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2302;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3741;
  int32_t _M0L4sizeS2304;
  int32_t _M0L6_2atmpS2303;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS517 = _M0L4selfS518->$6;
  switch (_M0L7_2abindS517) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2296 = _M0L5entryS519;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3743 = _M0L4selfS518->$5;
      if (_M0L6_2atmpS2296) {
        moonbit_incref(_M0L6_2atmpS2296);
      }
      if (_M0L6_2aoldS3743) {
        moonbit_decref(_M0L6_2aoldS3743);
      }
      _M0L4selfS518->$5 = _M0L6_2atmpS2296;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2300 = _M0L4selfS518->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2299;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2297;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2298;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3744;
      if (
        _M0L7_2abindS517 < 0
        || _M0L7_2abindS517 >= Moonbit_array_length(_M0L7entriesS2300)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2299
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2300[_M0L7_2abindS517];
      if (_M0L6_2atmpS2299) {
        moonbit_incref(_M0L6_2atmpS2299);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2297
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2299);
      if (_M0L6_2atmpS2299) {
        moonbit_decref(_M0L6_2atmpS2299);
      }
      _M0L6_2atmpS2298 = _M0L5entryS519;
      _M0L6_2aoldS3744 = _M0L6_2atmpS2297->$1;
      if (_M0L6_2atmpS2298) {
        moonbit_incref(_M0L6_2atmpS2298);
      }
      if (_M0L6_2aoldS3744) {
        moonbit_decref(_M0L6_2aoldS3744);
      }
      _M0L6_2atmpS2297->$1 = _M0L6_2atmpS2298;
      moonbit_decref(_M0L6_2atmpS2297);
      break;
    }
  }
  _M0L4selfS518->$6 = _M0L3idxS520;
  _M0L7entriesS2301 = _M0L4selfS518->$0;
  _M0L6_2atmpS2302 = _M0L5entryS519;
  if (
    _M0L3idxS520 < 0
    || _M0L3idxS520 >= Moonbit_array_length(_M0L7entriesS2301)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3741
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2301[_M0L3idxS520];
  if (_M0L6_2atmpS2302) {
    moonbit_incref(_M0L6_2atmpS2302);
  }
  if (_M0L6_2aoldS3741) {
    moonbit_decref(_M0L6_2aoldS3741);
  }
  _M0L7entriesS2301[_M0L3idxS520] = _M0L6_2atmpS2302;
  _M0L4sizeS2304 = _M0L4selfS518->$1;
  _M0L6_2atmpS2303 = _M0L4sizeS2304 + 1;
  _M0L4selfS518->$1 = _M0L6_2atmpS2303;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS522,
  int32_t _M0L3idxS524,
  struct _M0TPB5EntryGsfE* _M0L5entryS523
) {
  int32_t _M0L7_2abindS521;
  struct _M0TPB5EntryGsfE** _M0L7entriesS2310;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2311;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3747;
  int32_t _M0L4sizeS2313;
  int32_t _M0L6_2atmpS2312;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS521 = _M0L4selfS522->$6;
  switch (_M0L7_2abindS521) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2305 = _M0L5entryS523;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3749 = _M0L4selfS522->$5;
      if (_M0L6_2atmpS2305) {
        moonbit_incref(_M0L6_2atmpS2305);
      }
      if (_M0L6_2aoldS3749) {
        moonbit_decref(_M0L6_2aoldS3749);
      }
      _M0L4selfS522->$5 = _M0L6_2atmpS2305;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2309 = _M0L4selfS522->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2308;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2306;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2307;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3750;
      if (
        _M0L7_2abindS521 < 0
        || _M0L7_2abindS521 >= Moonbit_array_length(_M0L7entriesS2309)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2308
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2309[_M0L7_2abindS521];
      if (_M0L6_2atmpS2308) {
        moonbit_incref(_M0L6_2atmpS2308);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2306
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2308);
      if (_M0L6_2atmpS2308) {
        moonbit_decref(_M0L6_2atmpS2308);
      }
      _M0L6_2atmpS2307 = _M0L5entryS523;
      _M0L6_2aoldS3750 = _M0L6_2atmpS2306->$1;
      if (_M0L6_2atmpS2307) {
        moonbit_incref(_M0L6_2atmpS2307);
      }
      if (_M0L6_2aoldS3750) {
        moonbit_decref(_M0L6_2aoldS3750);
      }
      _M0L6_2atmpS2306->$1 = _M0L6_2atmpS2307;
      moonbit_decref(_M0L6_2atmpS2306);
      break;
    }
  }
  _M0L4selfS522->$6 = _M0L3idxS524;
  _M0L7entriesS2310 = _M0L4selfS522->$0;
  _M0L6_2atmpS2311 = _M0L5entryS523;
  if (
    _M0L3idxS524 < 0
    || _M0L3idxS524 >= Moonbit_array_length(_M0L7entriesS2310)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3747
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2310[_M0L3idxS524];
  if (_M0L6_2atmpS2311) {
    moonbit_incref(_M0L6_2atmpS2311);
  }
  if (_M0L6_2aoldS3747) {
    moonbit_decref(_M0L6_2aoldS3747);
  }
  _M0L7entriesS2310[_M0L3idxS524] = _M0L6_2atmpS2311;
  _M0L4sizeS2313 = _M0L4selfS522->$1;
  _M0L6_2atmpS2312 = _M0L4sizeS2313 + 1;
  _M0L4selfS522->$1 = _M0L6_2atmpS2312;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS526,
  int32_t _M0L3idxS528,
  struct _M0TPB5EntryGsiE* _M0L5entryS527
) {
  int32_t _M0L7_2abindS525;
  struct _M0TPB5EntryGsiE** _M0L7entriesS2319;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2320;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3753;
  int32_t _M0L4sizeS2322;
  int32_t _M0L6_2atmpS2321;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS525 = _M0L4selfS526->$6;
  switch (_M0L7_2abindS525) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2314 = _M0L5entryS527;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3755 = _M0L4selfS526->$5;
      if (_M0L6_2atmpS2314) {
        moonbit_incref(_M0L6_2atmpS2314);
      }
      if (_M0L6_2aoldS3755) {
        moonbit_decref(_M0L6_2aoldS3755);
      }
      _M0L4selfS526->$5 = _M0L6_2atmpS2314;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2318 = _M0L4selfS526->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2317;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2315;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2316;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3756;
      if (
        _M0L7_2abindS525 < 0
        || _M0L7_2abindS525 >= Moonbit_array_length(_M0L7entriesS2318)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2317
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2318[_M0L7_2abindS525];
      if (_M0L6_2atmpS2317) {
        moonbit_incref(_M0L6_2atmpS2317);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2315
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2317);
      if (_M0L6_2atmpS2317) {
        moonbit_decref(_M0L6_2atmpS2317);
      }
      _M0L6_2atmpS2316 = _M0L5entryS527;
      _M0L6_2aoldS3756 = _M0L6_2atmpS2315->$1;
      if (_M0L6_2atmpS2316) {
        moonbit_incref(_M0L6_2atmpS2316);
      }
      if (_M0L6_2aoldS3756) {
        moonbit_decref(_M0L6_2aoldS3756);
      }
      _M0L6_2atmpS2315->$1 = _M0L6_2atmpS2316;
      moonbit_decref(_M0L6_2atmpS2315);
      break;
    }
  }
  _M0L4selfS526->$6 = _M0L3idxS528;
  _M0L7entriesS2319 = _M0L4selfS526->$0;
  _M0L6_2atmpS2320 = _M0L5entryS527;
  if (
    _M0L3idxS528 < 0
    || _M0L3idxS528 >= Moonbit_array_length(_M0L7entriesS2319)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3753
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2319[_M0L3idxS528];
  if (_M0L6_2atmpS2320) {
    moonbit_incref(_M0L6_2atmpS2320);
  }
  if (_M0L6_2aoldS3753) {
    moonbit_decref(_M0L6_2aoldS3753);
  }
  _M0L7entriesS2319[_M0L3idxS528] = _M0L6_2atmpS2320;
  _M0L4sizeS2322 = _M0L4selfS526->$1;
  _M0L6_2atmpS2321 = _M0L4sizeS2322 + 1;
  _M0L4selfS526->$1 = _M0L6_2atmpS2321;
  return 0;
}

int32_t _M0MPC13int3Int3max(int32_t _M0L4selfS507, int32_t _M0L5otherS508) {
  #line 75 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS507 > _M0L5otherS508) {
    return _M0L4selfS507;
  } else {
    return _M0L5otherS508;
  }
}

int32_t _M0FPB21capacity__for__length(int32_t _M0L6lengthS506) {
  int32_t _M0Lm8capacityS505;
  int32_t _M0L6_2atmpS2276;
  int32_t _M0L6_2atmpS2275;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS505 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS506);
  _M0L6_2atmpS2276 = _M0Lm8capacityS505;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2275 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2276);
  if (_M0L6lengthS506 > _M0L6_2atmpS2275) {
    int32_t _M0L6_2atmpS2277 = _M0Lm8capacityS505;
    _M0Lm8capacityS505 = _M0L6_2atmpS2277 * 2;
  }
  return _M0Lm8capacityS505;
}

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  int32_t _M0L8capacityS476
) {
  int32_t _M0L8capacityS475;
  int32_t _M0L7_2abindS477;
  int32_t _M0L7_2abindS478;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2270;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS479;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS480;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _block_4087;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS475
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS476);
  _M0L7_2abindS477 = _M0L8capacityS475 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS478 = _M0FPB21calc__grow__threshold(_M0L8capacityS475);
  _M0L6_2atmpS2270 = 0;
  _M0L7_2abindS479
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS475, _M0L6_2atmpS2270);
  _M0L7_2abindS480 = 0;
  _block_4087
  = (struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
  Moonbit_object_header(_block_4087)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
  _block_4087->$0 = _M0L7_2abindS479;
  _block_4087->$1 = 0;
  _block_4087->$2 = _M0L8capacityS475;
  _block_4087->$3 = _M0L7_2abindS477;
  _block_4087->$4 = _M0L7_2abindS478;
  _block_4087->$5 = _M0L7_2abindS480;
  _block_4087->$6 = -1;
  return _block_4087;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS482) {
  int32_t _M0L8capacityS481;
  int32_t _M0L7_2abindS483;
  int32_t _M0L7_2abindS484;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2271;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS485;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS486;
  struct _M0TPB3MapGsiE* _block_4088;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS481
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS482);
  _M0L7_2abindS483 = _M0L8capacityS481 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS484 = _M0FPB21calc__grow__threshold(_M0L8capacityS481);
  _M0L6_2atmpS2271 = 0;
  _M0L7_2abindS485
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS481, _M0L6_2atmpS2271);
  _M0L7_2abindS486 = 0;
  _block_4088
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_4088)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 100, 0);
  _block_4088->$0 = _M0L7_2abindS485;
  _block_4088->$1 = 0;
  _block_4088->$2 = _M0L8capacityS481;
  _block_4088->$3 = _M0L7_2abindS483;
  _block_4088->$4 = _M0L7_2abindS484;
  _block_4088->$5 = _M0L7_2abindS486;
  _block_4088->$6 = -1;
  return _block_4088;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS488) {
  int32_t _M0L8capacityS487;
  int32_t _M0L7_2abindS489;
  int32_t _M0L7_2abindS490;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2272;
  struct _M0TPB5EntryGssE** _M0L7_2abindS491;
  struct _M0TPB5EntryGssE* _M0L7_2abindS492;
  struct _M0TPB3MapGssE* _block_4089;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS487
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS488);
  _M0L7_2abindS489 = _M0L8capacityS487 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS490 = _M0FPB21calc__grow__threshold(_M0L8capacityS487);
  _M0L6_2atmpS2272 = 0;
  _M0L7_2abindS491
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS487, _M0L6_2atmpS2272);
  _M0L7_2abindS492 = 0;
  _block_4089
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_4089)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 104, 0);
  _block_4089->$0 = _M0L7_2abindS491;
  _block_4089->$1 = 0;
  _block_4089->$2 = _M0L8capacityS487;
  _block_4089->$3 = _M0L7_2abindS489;
  _block_4089->$4 = _M0L7_2abindS490;
  _block_4089->$5 = _M0L7_2abindS492;
  _block_4089->$6 = -1;
  return _block_4089;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS494) {
  int32_t _M0L8capacityS493;
  int32_t _M0L7_2abindS495;
  int32_t _M0L7_2abindS496;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2273;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS497;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS498;
  struct _M0TPB3MapGsbE* _block_4090;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS493
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS494);
  _M0L7_2abindS495 = _M0L8capacityS493 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS496 = _M0FPB21calc__grow__threshold(_M0L8capacityS493);
  _M0L6_2atmpS2273 = 0;
  _M0L7_2abindS497
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS493, _M0L6_2atmpS2273);
  _M0L7_2abindS498 = 0;
  _block_4090
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_4090)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 108, 0);
  _block_4090->$0 = _M0L7_2abindS497;
  _block_4090->$1 = 0;
  _block_4090->$2 = _M0L8capacityS493;
  _block_4090->$3 = _M0L7_2abindS495;
  _block_4090->$4 = _M0L7_2abindS496;
  _block_4090->$5 = _M0L7_2abindS498;
  _block_4090->$6 = -1;
  return _block_4090;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS500) {
  int32_t _M0L8capacityS499;
  int32_t _M0L7_2abindS501;
  int32_t _M0L7_2abindS502;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2274;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS503;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS504;
  struct _M0TPB3MapGsfE* _block_4091;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS499
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS500);
  _M0L7_2abindS501 = _M0L8capacityS499 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS502 = _M0FPB21calc__grow__threshold(_M0L8capacityS499);
  _M0L6_2atmpS2274 = 0;
  _M0L7_2abindS503
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS499, _M0L6_2atmpS2274);
  _M0L7_2abindS504 = 0;
  _block_4091
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_4091)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 112, 0);
  _block_4091->$0 = _M0L7_2abindS503;
  _block_4091->$1 = 0;
  _block_4091->$2 = _M0L8capacityS499;
  _block_4091->$3 = _M0L7_2abindS501;
  _block_4091->$4 = _M0L7_2abindS502;
  _block_4091->$5 = _M0L7_2abindS504;
  _block_4091->$6 = -1;
  return _block_4091;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS474) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS474 >= 0) {
    int32_t _M0L6_2atmpS2269;
    int32_t _M0L6_2atmpS2268;
    int32_t _M0L6_2atmpS2267;
    int32_t _M0L6_2atmpS2266;
    if (_M0L4selfS474 <= 1) {
      return 1;
    }
    if (_M0L4selfS474 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2269 = _M0L4selfS474 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2268 = moonbit_clz32(_M0L6_2atmpS2269);
    _M0L6_2atmpS2267 = _M0L6_2atmpS2268 - 1;
    _M0L6_2atmpS2266 = 2147483647 >> (_M0L6_2atmpS2267 & 31);
    return _M0L6_2atmpS2266 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS473) {
  int32_t _M0L6_2atmpS2265;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2265 = _M0L8capacityS473 * 13;
  return _M0L6_2atmpS2265 / 16;
}

int32_t _M0MPC16option6Option6unwrapGiE(int64_t _M0L4selfS461) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS461 == 4294967296ll) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    int64_t _M0L7_2aSomeS462 = _M0L4selfS461;
    return (int32_t)_M0L7_2aSomeS462;
  }
}

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS463
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS463 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS464 =
      _M0L4selfS463;
    if (_M0L7_2aSomeS464) {
      moonbit_incref(_M0L7_2aSomeS464);
    }
    return _M0L7_2aSomeS464;
  }
}

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE* _M0L4selfS465
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS465 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS466 = _M0L4selfS465;
    if (_M0L7_2aSomeS466) {
      moonbit_incref(_M0L7_2aSomeS466);
    }
    return _M0L7_2aSomeS466;
  }
}

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE* _M0L4selfS467
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS467 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS468 = _M0L4selfS467;
    if (_M0L7_2aSomeS468) {
      moonbit_incref(_M0L7_2aSomeS468);
    }
    return _M0L7_2aSomeS468;
  }
}

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE* _M0L4selfS469
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS469 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS470 = _M0L4selfS469;
    if (_M0L7_2aSomeS470) {
      moonbit_incref(_M0L7_2aSomeS470);
    }
    return _M0L7_2aSomeS470;
  }
}

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE* _M0L4selfS471
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS471 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS472 = _M0L4selfS471;
    if (_M0L7_2aSomeS472) {
      moonbit_incref(_M0L7_2aSomeS472);
    }
    return _M0L7_2aSomeS472;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS435,
  struct _M0TPC16string10StringView _M0L9separatorS448
) {
  int32_t _M0L3endS2240;
  int32_t _M0L5startS2241;
  int32_t _M0L6_2atmpS2239;
  #line 1497 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2240 = _M0L4selfS435.$2;
  _M0L5startS2241 = _M0L4selfS435.$1;
  _M0L6_2atmpS2239 = _M0L3endS2240 - _M0L5startS2241;
  if (_M0L6_2atmpS2239 == 0) {
    return (moonbit_string_t)moonbit_string_literal_95.data;
  } else {
    moonbit_string_t* _M0L3bufS2263 = _M0L4selfS435.$0;
    int32_t _M0L5startS2264 = _M0L4selfS435.$1;
    moonbit_string_t _M0L5_2ahdS436 =
      (moonbit_string_t)_M0L3bufS2263[_M0L5startS2264];
    moonbit_string_t* _M0L9_2ax__bufS437 = _M0L4selfS435.$0;
    int32_t _M0L5startS2262 = _M0L4selfS435.$1;
    int32_t _M0L11_2ax__startS438 = 1 + _M0L5startS2262;
    int32_t _M0L9_2ax__endS439 = _M0L4selfS435.$2;
    struct _M0TPC16string10StringView _M0L2hdS440;
    int32_t _M0L7_2abindS441;
    int32_t _M0L3endS2260;
    int32_t _M0L5startS2261;
    int32_t _M0L6_2atmpS2259;
    int32_t _M0L10size__hintS442;
    int32_t _M0L2__S443;
    int32_t _M0L10size__hintS444;
    int32_t _M0L10size__hintS449;
    struct _M0TPB13StringBuilder* _M0L3bufS450;
    int32_t _M0L3endS2243;
    int32_t _M0L5startS2244;
    int32_t _M0L6_2atmpS2242;
    moonbit_string_t _result_4095;
    moonbit_incref(_M0L9_2ax__bufS437);
    moonbit_incref(_M0L5_2ahdS436);
    #line 1504 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS440
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS436);
    moonbit_decref(_M0L5_2ahdS436);
    _M0L7_2abindS441 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
    _M0L3endS2260 = _M0L2hdS440.$2;
    _M0L5startS2261 = _M0L2hdS440.$1;
    _M0L6_2atmpS2259 = _M0L3endS2260 - _M0L5startS2261;
    _M0L2__S443 = 0;
    _M0L10size__hintS444 = _M0L6_2atmpS2259;
    while (1) {
      if (_M0L2__S443 < _M0L7_2abindS441) {
        int32_t _M0L6_2atmpS2258 = _M0L11_2ax__startS438 + _M0L2__S443;
        moonbit_string_t _M0L1sS445 =
          (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2258];
        int32_t _M0L6_2atmpS2249 = _M0L2__S443 + 1;
        struct _M0TPC16string10StringView _M0L7_2abindS447;
        int32_t _M0L3endS2256;
        int32_t _M0L5startS2257;
        int32_t _M0L6_2atmpS2255;
        int32_t _M0L6_2atmpS2251;
        int32_t _M0L3endS2253;
        int32_t _M0L5startS2254;
        int32_t _M0L6_2atmpS2252;
        int32_t _M0L6_2atmpS2250;
        moonbit_incref(_M0L1sS445);
        #line 1506 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
        _M0L7_2abindS447
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS445);
        moonbit_decref(_M0L1sS445);
        _M0L3endS2256 = _M0L7_2abindS447.$2;
        _M0L5startS2257 = _M0L7_2abindS447.$1;
        moonbit_decref(_M0L7_2abindS447.$0);
        _M0L6_2atmpS2255 = _M0L3endS2256 - _M0L5startS2257;
        _M0L6_2atmpS2251 = _M0L10size__hintS444 + _M0L6_2atmpS2255;
        _M0L3endS2253 = _M0L9separatorS448.$2;
        _M0L5startS2254 = _M0L9separatorS448.$1;
        _M0L6_2atmpS2252 = _M0L3endS2253 - _M0L5startS2254;
        _M0L6_2atmpS2250 = _M0L6_2atmpS2251 + _M0L6_2atmpS2252;
        _M0L2__S443 = _M0L6_2atmpS2249;
        _M0L10size__hintS444 = _M0L6_2atmpS2250;
        continue;
      } else {
        _M0L10size__hintS442 = _M0L10size__hintS444;
      }
      break;
    }
    _M0L10size__hintS449 = _M0L10size__hintS442 << 1;
    #line 1511 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS450
    = _M0MPB13StringBuilder21StringBuilder_2einner(_M0L10size__hintS449);
    #line 1513 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS450, _M0L2hdS440);
    moonbit_decref(_M0L2hdS440.$0);
    _M0L3endS2243 = _M0L9separatorS448.$2;
    _M0L5startS2244 = _M0L9separatorS448.$1;
    _M0L6_2atmpS2242 = _M0L3endS2243 - _M0L5startS2244;
    if (_M0L6_2atmpS2242 == 0) {
      int32_t _M0L7_2abindS451 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
      int32_t _M0L2__S452 = 0;
      while (1) {
        if (_M0L2__S452 < _M0L7_2abindS451) {
          int32_t _M0L6_2atmpS2246 = _M0L11_2ax__startS438 + _M0L2__S452;
          moonbit_string_t _M0L1sS453 =
            (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2246];
          struct _M0TPC16string10StringView _M0L1sS454;
          int32_t _M0L6_2atmpS2245;
          moonbit_incref(_M0L1sS453);
          #line 1517 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS454
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS453);
          moonbit_decref(_M0L1sS453);
          #line 1518 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS450, _M0L1sS454);
          moonbit_decref(_M0L1sS454.$0);
          _M0L6_2atmpS2245 = _M0L2__S452 + 1;
          _M0L2__S452 = _M0L6_2atmpS2245;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS437);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS456 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
      int32_t _M0L2__S457 = 0;
      while (1) {
        if (_M0L2__S457 < _M0L7_2abindS456) {
          int32_t _M0L6_2atmpS2248 = _M0L11_2ax__startS438 + _M0L2__S457;
          moonbit_string_t _M0L1sS458 =
            (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2248];
          struct _M0TPC16string10StringView _M0L1sS459;
          int32_t _M0L6_2atmpS2247;
          moonbit_incref(_M0L1sS458);
          #line 1522 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS459
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS458);
          moonbit_decref(_M0L1sS458);
          #line 1523 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS450, _M0L9separatorS448);
          #line 1525 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS450, _M0L1sS459);
          moonbit_decref(_M0L1sS459.$0);
          _M0L6_2atmpS2247 = _M0L2__S457 + 1;
          _M0L2__S457 = _M0L6_2atmpS2247;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS437);
        }
        break;
      }
    }
    #line 1528 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _result_4095 = _M0MPB13StringBuilder10to__string(_M0L3bufS450);
    moonbit_decref(_M0L3bufS450);
    return _result_4095;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS431,
  int32_t _M0L5indexS432
) {
  uint64_t* _M0L6_2atmpS2237;
  uint64_t _result_4096;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS431);
  _M0L6_2atmpS2237 = _M0L4selfS431;
  if (
    _M0L5indexS432 < 0
    || _M0L5indexS432 >= Moonbit_array_length(_M0L6_2atmpS2237)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4096 = (uint64_t)_M0L6_2atmpS2237[_M0L5indexS432];
  moonbit_decref(_M0L6_2atmpS2237);
  return _result_4096;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS433,
  int32_t _M0L5indexS434
) {
  uint32_t* _M0L6_2atmpS2238;
  uint32_t _result_4097;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS433);
  _M0L6_2atmpS2238 = _M0L4selfS433;
  if (
    _M0L5indexS434 < 0
    || _M0L5indexS434 >= Moonbit_array_length(_M0L6_2atmpS2238)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4097 = (uint32_t)_M0L6_2atmpS2238[_M0L5indexS434];
  moonbit_decref(_M0L6_2atmpS2238);
  return _result_4097;
}

moonbit_string_t _M0IPC16uint646UInt64PB4Show10to__string(
  uint64_t _M0L4selfS430
) {
  #line 50 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 51 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS430, 10);
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS429) {
  #line 35 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 36 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS429, 10);
}

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t _M0L4selfS428) {
  #line 26 "/home/developer/.moon/lib/core/builtin/show.mbt"
  if (_M0L4selfS428) {
    return (moonbit_string_t)moonbit_string_literal_100.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_101.data;
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS427) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS427;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS426
) {
  int32_t _M0L6_2atmpS2236;
  #line 24 "/home/developer/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2236 = Moonbit_array_length(_M0L4selfS426);
  moonbit_incref(_M0L4selfS426);
  return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS426,
                                               .$1 = 0,
                                               .$2 = _M0L6_2atmpS2236};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS417,
  moonbit_string_t _M0L5valueS419
) {
  int32_t _M0L3lenS2221;
  moonbit_string_t* _M0L6_2atmpS2223;
  int32_t _M0L6_2atmpS2222;
  int32_t _M0L6lengthS418;
  moonbit_string_t* _M0L3bufS2224;
  moonbit_string_t _M0L6_2aoldS3765;
  int32_t _M0L6_2atmpS2225;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2221 = _M0L4selfS417->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2223 = _M0MPC15array5Array6bufferGsE(_M0L4selfS417);
  _M0L6_2atmpS2222 = Moonbit_array_length(_M0L6_2atmpS2223);
  moonbit_decref(_M0L6_2atmpS2223);
  if (_M0L3lenS2221 == _M0L6_2atmpS2222) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS417);
  }
  _M0L6lengthS418 = _M0L4selfS417->$1;
  _M0L3bufS2224 = _M0L4selfS417->$0;
  _M0L6_2aoldS3765 = (moonbit_string_t)_M0L3bufS2224[_M0L6lengthS418];
  moonbit_incref(_M0L5valueS419);
  moonbit_decref(_M0L6_2aoldS3765);
  _M0L3bufS2224[_M0L6lengthS418] = _M0L5valueS419;
  _M0L6_2atmpS2225 = _M0L6lengthS418 + 1;
  _M0L4selfS417->$1 = _M0L6_2atmpS2225;
  return 0;
}

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS420,
  moonbit_string_t _M0L5valueS422
) {
  int32_t _M0L3lenS2226;
  moonbit_string_t* _M0L6_2atmpS2228;
  int32_t _M0L6_2atmpS2227;
  int32_t _M0L6lengthS421;
  moonbit_string_t* _M0L3bufS2229;
  moonbit_string_t _M0L6_2aoldS3767;
  int32_t _M0L6_2atmpS2230;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2226 = _M0L4selfS420->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2228 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS420);
  _M0L6_2atmpS2227 = Moonbit_array_length(_M0L6_2atmpS2228);
  moonbit_decref(_M0L6_2atmpS2228);
  if (_M0L3lenS2226 == _M0L6_2atmpS2227) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGOsE(_M0L4selfS420);
  }
  _M0L6lengthS421 = _M0L4selfS420->$1;
  _M0L3bufS2229 = _M0L4selfS420->$0;
  _M0L6_2aoldS3767 = (moonbit_string_t)_M0L3bufS2229[_M0L6lengthS421];
  if (_M0L5valueS422) {
    moonbit_incref(_M0L5valueS422);
  }
  if (_M0L6_2aoldS3767) {
    moonbit_decref(_M0L6_2aoldS3767);
  }
  _M0L3bufS2229[_M0L6lengthS421] = _M0L5valueS422;
  _M0L6_2atmpS2230 = _M0L6lengthS421 + 1;
  _M0L4selfS420->$1 = _M0L6_2atmpS2230;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS423,
  struct _M0TUsfE* _M0L5valueS425
) {
  int32_t _M0L3lenS2231;
  struct _M0TUsfE** _M0L6_2atmpS2233;
  int32_t _M0L6_2atmpS2232;
  int32_t _M0L6lengthS424;
  struct _M0TUsfE** _M0L3bufS2234;
  struct _M0TUsfE* _M0L6_2aoldS3769;
  int32_t _M0L6_2atmpS2235;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2231 = _M0L4selfS423->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2233 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS423);
  _M0L6_2atmpS2232 = Moonbit_array_length(_M0L6_2atmpS2233);
  moonbit_decref(_M0L6_2atmpS2233);
  if (_M0L3lenS2231 == _M0L6_2atmpS2232) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS423);
  }
  _M0L6lengthS424 = _M0L4selfS423->$1;
  _M0L3bufS2234 = _M0L4selfS423->$0;
  _M0L6_2aoldS3769 = (struct _M0TUsfE*)_M0L3bufS2234[_M0L6lengthS424];
  moonbit_incref(_M0L5valueS425);
  if (_M0L6_2aoldS3769) {
    moonbit_decref(_M0L6_2aoldS3769);
  }
  _M0L3bufS2234[_M0L6lengthS424] = _M0L5valueS425;
  _M0L6_2atmpS2235 = _M0L6lengthS424 + 1;
  _M0L4selfS423->$1 = _M0L6_2atmpS2235;
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS409) {
  int32_t _M0L8old__capS408;
  int32_t _M0L8new__capS410;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS408 = _M0L4selfS409->$1;
  if (_M0L8old__capS408 == 0) {
    _M0L8new__capS410 = 8;
  } else {
    _M0L8new__capS410 = _M0L8old__capS408 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS409, _M0L8new__capS410);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS412
) {
  int32_t _M0L8old__capS411;
  int32_t _M0L8new__capS413;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS411 = _M0L4selfS412->$1;
  if (_M0L8old__capS411 == 0) {
    _M0L8new__capS413 = 8;
  } else {
    _M0L8new__capS413 = _M0L8old__capS411 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGOsE(_M0L4selfS412, _M0L8new__capS413);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS415
) {
  int32_t _M0L8old__capS414;
  int32_t _M0L8new__capS416;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS414 = _M0L4selfS415->$1;
  if (_M0L8old__capS414 == 0) {
    _M0L8new__capS416 = 8;
  } else {
    _M0L8new__capS416 = _M0L8old__capS414 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsfEE(_M0L4selfS415, _M0L8new__capS416);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS391,
  int32_t _M0L13new__capacityS394
) {
  moonbit_string_t* _M0L8old__bufS390;
  int32_t _M0L8old__capS392;
  int32_t _M0L9copy__lenS393;
  moonbit_string_t* _M0L8new__bufS395;
  moonbit_string_t* _M0L6_2aoldS3771;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS390 = _M0L4selfS391->$0;
  _M0L8old__capS392 = Moonbit_array_length(_M0L8old__bufS390);
  if (_M0L8old__capS392 < _M0L13new__capacityS394) {
    _M0L9copy__lenS393 = _M0L8old__capS392;
  } else {
    _M0L9copy__lenS393 = _M0L13new__capacityS394;
  }
  moonbit_incref(_M0L8old__bufS390);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS395
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(_M0L8old__bufS390, _M0L13new__capacityS394, _M0L9copy__lenS393, 0, 0);
  moonbit_decref(_M0L8old__bufS390);
  _M0L6_2aoldS3771 = _M0L4selfS391->$0;
  moonbit_decref(_M0L6_2aoldS3771);
  _M0L4selfS391->$0 = _M0L8new__bufS395;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS397,
  int32_t _M0L13new__capacityS400
) {
  moonbit_string_t* _M0L8old__bufS396;
  int32_t _M0L8old__capS398;
  int32_t _M0L9copy__lenS399;
  moonbit_string_t* _M0L8new__bufS401;
  moonbit_string_t* _M0L6_2aoldS3773;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS396 = _M0L4selfS397->$0;
  _M0L8old__capS398 = Moonbit_array_length(_M0L8old__bufS396);
  if (_M0L8old__capS398 < _M0L13new__capacityS400) {
    _M0L9copy__lenS399 = _M0L8old__capS398;
  } else {
    _M0L9copy__lenS399 = _M0L13new__capacityS400;
  }
  moonbit_incref(_M0L8old__bufS396);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS401
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(_M0L8old__bufS396, _M0L13new__capacityS400, _M0L9copy__lenS399, 0, 0);
  moonbit_decref(_M0L8old__bufS396);
  _M0L6_2aoldS3773 = _M0L4selfS397->$0;
  moonbit_decref(_M0L6_2aoldS3773);
  _M0L4selfS397->$0 = _M0L8new__bufS401;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS403,
  int32_t _M0L13new__capacityS406
) {
  struct _M0TUsfE** _M0L8old__bufS402;
  int32_t _M0L8old__capS404;
  int32_t _M0L9copy__lenS405;
  struct _M0TUsfE** _M0L8new__bufS407;
  struct _M0TUsfE** _M0L6_2aoldS3775;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS402 = _M0L4selfS403->$0;
  _M0L8old__capS404 = Moonbit_array_length(_M0L8old__bufS402);
  if (_M0L8old__capS404 < _M0L13new__capacityS406) {
    _M0L9copy__lenS405 = _M0L8old__capS404;
  } else {
    _M0L9copy__lenS405 = _M0L13new__capacityS406;
  }
  moonbit_incref(_M0L8old__bufS402);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS407
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(_M0L8old__bufS402, _M0L13new__capacityS406, _M0L9copy__lenS405, 0, 0);
  moonbit_decref(_M0L8old__bufS402);
  _M0L6_2aoldS3775 = _M0L4selfS403->$0;
  moonbit_decref(_M0L6_2aoldS3775);
  _M0L4selfS403->$0 = _M0L8new__bufS407;
  return 0;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS387) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS387->$1;
}

int32_t _M0MPC15array5Array6lengthGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS388
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS388->$1;
}

int32_t _M0MPC15array5Array6lengthGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS389
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS389->$1;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS386
) {
  #line 222 "/home/developer/.moon/lib/core/builtin/show.mbt"
  moonbit_incref(_M0L4selfS386);
  return _M0L4selfS386;
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS385,
  struct _M0TPC16string10StringView _M0L3strS384
) {
  int32_t _M0L3endS2219;
  int32_t _M0L5startS2220;
  int32_t _M0L8str__lenS383;
  int32_t _M0L3lenS2212;
  int32_t _M0L6_2atmpS2211;
  uint16_t* _M0L4dataS2213;
  int32_t _M0L3lenS2214;
  moonbit_string_t _M0L6_2atmpS2215;
  int32_t _M0L6_2atmpS2216;
  int32_t _M0L3lenS2218;
  int32_t _M0L6_2atmpS2217;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2219 = _M0L3strS384.$2;
  _M0L5startS2220 = _M0L3strS384.$1;
  _M0L8str__lenS383 = _M0L3endS2219 - _M0L5startS2220;
  _M0L3lenS2212 = _M0L4selfS385->$1;
  _M0L6_2atmpS2211 = _M0L3lenS2212 + _M0L8str__lenS383;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS385, _M0L6_2atmpS2211);
  _M0L4dataS2213 = _M0L4selfS385->$0;
  _M0L3lenS2214 = _M0L4selfS385->$1;
  moonbit_incref(_M0L4dataS2213);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2215 = _M0MPC16string10StringView4data(_M0L3strS384);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2216 = _M0MPC16string10StringView13start__offset(_M0L3strS384);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2213, _M0L3lenS2214, _M0L6_2atmpS2215, _M0L6_2atmpS2216, _M0L8str__lenS383);
  moonbit_decref(_M0L4dataS2213);
  moonbit_decref(_M0L6_2atmpS2215);
  _M0L3lenS2218 = _M0L4selfS385->$1;
  _M0L6_2atmpS2217 = _M0L3lenS2218 + _M0L8str__lenS383;
  _M0L4selfS385->$1 = _M0L6_2atmpS2217;
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS380
) {
  moonbit_string_t* _M0L8_2afieldS3778;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3778 = _M0L4selfS380->$0;
  moonbit_incref(_M0L8_2afieldS3778);
  return _M0L8_2afieldS3778;
}

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS381
) {
  moonbit_string_t* _M0L8_2afieldS3779;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3779 = _M0L4selfS381->$0;
  moonbit_incref(_M0L8_2afieldS3779);
  return _M0L8_2afieldS3779;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS382
) {
  struct _M0TUsfE** _M0L8_2afieldS3780;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3780 = _M0L4selfS382->$0;
  moonbit_incref(_M0L8_2afieldS3780);
  return _M0L8_2afieldS3780;
}

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(
  struct _M0TWEOUssE* _M0L1fS364,
  int64_t _M0L10size__hintS361
) {
  int64_t _M0L10size__hintS360;
  struct _M0TPB4IterGUssEE* _block_4098;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS361 == 4294967296ll) {
    _M0L10size__hintS360 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS362 = _M0L10size__hintS361;
    int32_t _M0L4_2anS363 = (int32_t)_M0L7_2aSomeS362;
    if (_M0L4_2anS363 > 0) {
      _M0L10size__hintS360 = (int64_t)_M0L4_2anS363;
    } else {
      _M0L10size__hintS360 = _M0MPB4Iter3newN6constrS9988GUssEE;
    }
  }
  moonbit_incref(_M0L1fS364);
  _block_4098
  = (struct _M0TPB4IterGUssEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUssEE));
  Moonbit_object_header(_block_4098)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 116, 0);
  _block_4098->$0 = _M0L1fS364;
  _block_4098->$1 = _M0L10size__hintS360;
  return _block_4098;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS369,
  int64_t _M0L10size__hintS366
) {
  int64_t _M0L10size__hintS365;
  struct _M0TPB4IterGUsbEE* _block_4099;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS366 == 4294967296ll) {
    _M0L10size__hintS365 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS367 = _M0L10size__hintS366;
    int32_t _M0L4_2anS368 = (int32_t)_M0L7_2aSomeS367;
    if (_M0L4_2anS368 > 0) {
      _M0L10size__hintS365 = (int64_t)_M0L4_2anS368;
    } else {
      _M0L10size__hintS365 = _M0MPB4Iter3newN6constrS9988GUsbEE;
    }
  }
  moonbit_incref(_M0L1fS369);
  _block_4099
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_4099)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 119, 0);
  _block_4099->$0 = _M0L1fS369;
  _block_4099->$1 = _M0L10size__hintS365;
  return _block_4099;
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1fS374,
  int64_t _M0L10size__hintS371
) {
  int64_t _M0L10size__hintS370;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _block_4100;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS371 == 4294967296ll) {
    _M0L10size__hintS370 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS372 = _M0L10size__hintS371;
    int32_t _M0L4_2anS373 = (int32_t)_M0L7_2aSomeS372;
    if (_M0L4_2anS373 > 0) {
      _M0L10size__hintS370 = (int64_t)_M0L4_2anS373;
    } else {
      _M0L10size__hintS370
      = _M0MPB4Iter3newN6constrS9988GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
    }
  }
  moonbit_incref(_M0L1fS374);
  _block_4100
  = (struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_block_4100)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 122, 0);
  _block_4100->$0 = _M0L1fS374;
  _block_4100->$1 = _M0L10size__hintS370;
  return _block_4100;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS379,
  int64_t _M0L10size__hintS376
) {
  int64_t _M0L10size__hintS375;
  struct _M0TPB4IterGUsfEE* _block_4101;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS376 == 4294967296ll) {
    _M0L10size__hintS375 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS377 = _M0L10size__hintS376;
    int32_t _M0L4_2anS378 = (int32_t)_M0L7_2aSomeS377;
    if (_M0L4_2anS378 > 0) {
      _M0L10size__hintS375 = (int64_t)_M0L4_2anS378;
    } else {
      _M0L10size__hintS375 = _M0MPB4Iter3newN6constrS9988GUsfEE;
    }
  }
  moonbit_incref(_M0L1fS379);
  _block_4101
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_4101)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 125, 0);
  _block_4101->$0 = _M0L1fS379;
  _block_4101->$1 = _M0L10size__hintS375;
  return _block_4101;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS352,
  int32_t _M0L5radixS351
) {
  int32_t _if__result_4102;
  uint16_t* _M0L6bufferS353;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS351 < 2) {
    _if__result_4102 = 1;
  } else {
    _if__result_4102 = _M0L5radixS351 > 36;
  }
  if (_if__result_4102) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_102.data);
  }
  if (_M0L4selfS352 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  switch (_M0L5radixS351) {
    case 10: {
      int32_t _M0L3lenS354;
      uint16_t* _M0L6bufferS355;
      #line 622 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS354 = _M0FPB12dec__count64(_M0L4selfS352);
      _M0L6bufferS355 = (uint16_t*)moonbit_make_string(_M0L3lenS354, 0);
      #line 624 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS355, _M0L4selfS352, 0, _M0L3lenS354);
      _M0L6bufferS353 = _M0L6bufferS355;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS356;
      uint16_t* _M0L6bufferS357;
      #line 628 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS356 = _M0FPB12hex__count64(_M0L4selfS352);
      _M0L6bufferS357 = (uint16_t*)moonbit_make_string(_M0L3lenS356, 0);
      #line 630 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS357, _M0L4selfS352, 0, _M0L3lenS356);
      _M0L6bufferS353 = _M0L6bufferS357;
      break;
    }
    default: {
      int32_t _M0L3lenS358;
      uint16_t* _M0L6bufferS359;
      #line 634 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS358 = _M0FPB14radix__count64(_M0L4selfS352, _M0L5radixS351);
      _M0L6bufferS359 = (uint16_t*)moonbit_make_string(_M0L3lenS358, 0);
      #line 636 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS359, _M0L4selfS352, 0, _M0L3lenS358, _M0L5radixS351);
      _M0L6bufferS353 = _M0L6bufferS359;
      break;
    }
  }
  return _M0L6bufferS353;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS337,
  uint64_t _M0L3numS349,
  int32_t _M0L12digit__startS338,
  int32_t _M0L10total__lenS350
) {
  int32_t _M0L6_2atmpS2210;
  uint64_t _M0L3numS327;
  int32_t _M0L6offsetS328;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2210 = _M0L10total__lenS350 - _M0L12digit__startS338;
  _M0L3numS327 = _M0L3numS349;
  _M0L6offsetS328 = _M0L6_2atmpS2210;
  while (1) {
    if (_M0L3numS327 >= 10000ull) {
      uint64_t _M0L1tS329 = _M0L3numS327 / 10000ull;
      uint64_t _M0L6_2atmpS2187 = _M0L3numS327 % 10000ull;
      int32_t _M0L1rS330 = (int32_t)_M0L6_2atmpS2187;
      int32_t _M0L2d1S331 = _M0L1rS330 / 100;
      int32_t _M0L2d2S332 = _M0L1rS330 % 100;
      int32_t _M0L6_2atmpS2186 = _M0L2d1S331 / 10;
      int32_t _M0L6_2atmpS2185 = 48 + _M0L6_2atmpS2186;
      int32_t _M0L6d1__hiS333 = (uint16_t)_M0L6_2atmpS2185;
      int32_t _M0L6_2atmpS2184 = _M0L2d1S331 % 10;
      int32_t _M0L6_2atmpS2183 = 48 + _M0L6_2atmpS2184;
      int32_t _M0L6d1__loS334 = (uint16_t)_M0L6_2atmpS2183;
      int32_t _M0L6_2atmpS2182 = _M0L2d2S332 / 10;
      int32_t _M0L6_2atmpS2181 = 48 + _M0L6_2atmpS2182;
      int32_t _M0L6d2__hiS335 = (uint16_t)_M0L6_2atmpS2181;
      int32_t _M0L6_2atmpS2180 = _M0L2d2S332 % 10;
      int32_t _M0L6_2atmpS2179 = 48 + _M0L6_2atmpS2180;
      int32_t _M0L6d2__loS336 = (uint16_t)_M0L6_2atmpS2179;
      int32_t _M0L6_2atmpS2171 = _M0L12digit__startS338 + _M0L6offsetS328;
      int32_t _M0L6_2atmpS2170 = _M0L6_2atmpS2171 - 4;
      int32_t _M0L6_2atmpS2173;
      int32_t _M0L6_2atmpS2172;
      int32_t _M0L6_2atmpS2175;
      int32_t _M0L6_2atmpS2174;
      int32_t _M0L6_2atmpS2177;
      int32_t _M0L6_2atmpS2176;
      int32_t _M0L6_2atmpS2178;
      _M0L6bufferS337[_M0L6_2atmpS2170] = _M0L6d1__hiS333;
      _M0L6_2atmpS2173 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2172 = _M0L6_2atmpS2173 - 3;
      _M0L6bufferS337[_M0L6_2atmpS2172] = _M0L6d1__loS334;
      _M0L6_2atmpS2175 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2174 = _M0L6_2atmpS2175 - 2;
      _M0L6bufferS337[_M0L6_2atmpS2174] = _M0L6d2__hiS335;
      _M0L6_2atmpS2177 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2176 = _M0L6_2atmpS2177 - 1;
      _M0L6bufferS337[_M0L6_2atmpS2176] = _M0L6d2__loS336;
      _M0L6_2atmpS2178 = _M0L6offsetS328 - 4;
      _M0L3numS327 = _M0L1tS329;
      _M0L6offsetS328 = _M0L6_2atmpS2178;
      continue;
    } else {
      int32_t _M0L6_2atmpS2209 = (int32_t)_M0L3numS327;
      int32_t _M0L9remainingS340 = _M0L6_2atmpS2209;
      int32_t _M0L6offsetS341 = _M0L6offsetS328;
      while (1) {
        if (_M0L9remainingS340 >= 100) {
          int32_t _M0L1tS342 = _M0L9remainingS340 / 100;
          int32_t _M0L1dS343 = _M0L9remainingS340 % 100;
          int32_t _M0L6_2atmpS2196 = _M0L1dS343 / 10;
          int32_t _M0L6_2atmpS2195 = 48 + _M0L6_2atmpS2196;
          int32_t _M0L5d__hiS344 = (uint16_t)_M0L6_2atmpS2195;
          int32_t _M0L6_2atmpS2194 = _M0L1dS343 % 10;
          int32_t _M0L6_2atmpS2193 = 48 + _M0L6_2atmpS2194;
          int32_t _M0L5d__loS345 = (uint16_t)_M0L6_2atmpS2193;
          int32_t _M0L6_2atmpS2189 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2188 = _M0L6_2atmpS2189 - 2;
          int32_t _M0L6_2atmpS2191;
          int32_t _M0L6_2atmpS2190;
          int32_t _M0L6_2atmpS2192;
          _M0L6bufferS337[_M0L6_2atmpS2188] = _M0L5d__hiS344;
          _M0L6_2atmpS2191 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2190 = _M0L6_2atmpS2191 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2190] = _M0L5d__loS345;
          _M0L6_2atmpS2192 = _M0L6offsetS341 - 2;
          _M0L9remainingS340 = _M0L1tS342;
          _M0L6offsetS341 = _M0L6_2atmpS2192;
          continue;
        } else if (_M0L9remainingS340 >= 10) {
          int32_t _M0L6_2atmpS2204 = _M0L9remainingS340 / 10;
          int32_t _M0L6_2atmpS2203 = 48 + _M0L6_2atmpS2204;
          int32_t _M0L5d__hiS347 = (uint16_t)_M0L6_2atmpS2203;
          int32_t _M0L6_2atmpS2202 = _M0L9remainingS340 % 10;
          int32_t _M0L6_2atmpS2201 = 48 + _M0L6_2atmpS2202;
          int32_t _M0L5d__loS348 = (uint16_t)_M0L6_2atmpS2201;
          int32_t _M0L6_2atmpS2198 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2197 = _M0L6_2atmpS2198 - 2;
          int32_t _M0L6_2atmpS2200;
          int32_t _M0L6_2atmpS2199;
          _M0L6bufferS337[_M0L6_2atmpS2197] = _M0L5d__hiS347;
          _M0L6_2atmpS2200 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2199 = _M0L6_2atmpS2200 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2199] = _M0L5d__loS348;
        } else {
          int32_t _M0L6_2atmpS2208 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2205 = _M0L6_2atmpS2208 - 1;
          int32_t _M0L6_2atmpS2207 = 48 + _M0L9remainingS340;
          int32_t _M0L6_2atmpS2206 = (uint16_t)_M0L6_2atmpS2207;
          _M0L6bufferS337[_M0L6_2atmpS2205] = _M0L6_2atmpS2206;
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS317,
  uint64_t _M0L3numS321,
  int32_t _M0L12digit__startS318,
  int32_t _M0L10total__lenS320,
  int32_t _M0L5radixS311
) {
  uint64_t _M0L4baseS310;
  int32_t _M0L6_2atmpS2155;
  int32_t _M0L6_2atmpS2154;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS310 = _M0MPC13int3Int10to__uint64(_M0L5radixS311);
  _M0L6_2atmpS2155 = _M0L5radixS311 - 1;
  _M0L6_2atmpS2154 = _M0L5radixS311 & _M0L6_2atmpS2155;
  if (_M0L6_2atmpS2154 == 0) {
    int32_t _M0L5shiftS312;
    uint64_t _M0L4maskS313;
    int32_t _M0L6_2atmpS2162;
    int32_t _M0L6offsetS314;
    uint64_t _M0L1nS315;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS312 = moonbit_ctz32(_M0L5radixS311);
    _M0L4maskS313 = _M0L4baseS310 - 1ull;
    _M0L6_2atmpS2162 = _M0L10total__lenS320 - _M0L12digit__startS318;
    _M0L6offsetS314 = _M0L6_2atmpS2162;
    _M0L1nS315 = _M0L3numS321;
    while (1) {
      if (_M0L1nS315 > 0ull) {
        uint64_t _M0L6_2atmpS2161 = _M0L1nS315 & _M0L4maskS313;
        int32_t _M0L5digitS316 = (int32_t)_M0L6_2atmpS2161;
        int32_t _M0L6_2atmpS2158 = _M0L12digit__startS318 + _M0L6offsetS314;
        int32_t _M0L6_2atmpS2156 = _M0L6_2atmpS2158 - 1;
        int32_t _M0L6_2atmpS2157 =
          ((moonbit_string_t)moonbit_string_literal_103.data)[_M0L5digitS316];
        int32_t _M0L6_2atmpS2159;
        uint64_t _M0L6_2atmpS2160;
        _M0L6bufferS317[_M0L6_2atmpS2156] = _M0L6_2atmpS2157;
        _M0L6_2atmpS2159 = _M0L6offsetS314 - 1;
        _M0L6_2atmpS2160 = _M0L1nS315 >> (_M0L5shiftS312 & 63);
        _M0L6offsetS314 = _M0L6_2atmpS2159;
        _M0L1nS315 = _M0L6_2atmpS2160;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2169 = _M0L10total__lenS320 - _M0L12digit__startS318;
    int32_t _M0L6offsetS322 = _M0L6_2atmpS2169;
    uint64_t _M0L1nS323 = _M0L3numS321;
    while (1) {
      if (_M0L1nS323 > 0ull) {
        uint64_t _M0L1qS324 = _M0L1nS323 / _M0L4baseS310;
        uint64_t _M0L6_2atmpS2168 = _M0L1qS324 * _M0L4baseS310;
        uint64_t _M0L6_2atmpS2167 = _M0L1nS323 - _M0L6_2atmpS2168;
        int32_t _M0L5digitS325 = (int32_t)_M0L6_2atmpS2167;
        int32_t _M0L6_2atmpS2165 = _M0L12digit__startS318 + _M0L6offsetS322;
        int32_t _M0L6_2atmpS2163 = _M0L6_2atmpS2165 - 1;
        int32_t _M0L6_2atmpS2164 =
          ((moonbit_string_t)moonbit_string_literal_103.data)[_M0L5digitS325];
        int32_t _M0L6_2atmpS2166;
        _M0L6bufferS317[_M0L6_2atmpS2163] = _M0L6_2atmpS2164;
        _M0L6_2atmpS2166 = _M0L6offsetS322 - 1;
        _M0L6offsetS322 = _M0L6_2atmpS2166;
        _M0L1nS323 = _M0L1qS324;
        continue;
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS304,
  uint64_t _M0L3numS309,
  int32_t _M0L12digit__startS305,
  int32_t _M0L10total__lenS308
) {
  int32_t _M0L6_2atmpS2153;
  int32_t _M0L6offsetS299;
  uint64_t _M0L1nS300;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2153 = _M0L10total__lenS308 - _M0L12digit__startS305;
  _M0L6offsetS299 = _M0L6_2atmpS2153;
  _M0L1nS300 = _M0L3numS309;
  while (1) {
    if (_M0L6offsetS299 >= 2) {
      uint64_t _M0L6_2atmpS2150 = _M0L1nS300 & 255ull;
      int32_t _M0L9byte__valS301 = (int32_t)_M0L6_2atmpS2150;
      int32_t _M0L2hiS302 = _M0L9byte__valS301 / 16;
      int32_t _M0L2loS303 = _M0L9byte__valS301 % 16;
      int32_t _M0L6_2atmpS2144 = _M0L12digit__startS305 + _M0L6offsetS299;
      int32_t _M0L6_2atmpS2142 = _M0L6_2atmpS2144 - 2;
      int32_t _M0L6_2atmpS2143 =
        ((moonbit_string_t)moonbit_string_literal_103.data)[_M0L2hiS302];
      int32_t _M0L6_2atmpS2147;
      int32_t _M0L6_2atmpS2145;
      int32_t _M0L6_2atmpS2146;
      int32_t _M0L6_2atmpS2148;
      uint64_t _M0L6_2atmpS2149;
      _M0L6bufferS304[_M0L6_2atmpS2142] = _M0L6_2atmpS2143;
      _M0L6_2atmpS2147 = _M0L12digit__startS305 + _M0L6offsetS299;
      _M0L6_2atmpS2145 = _M0L6_2atmpS2147 - 1;
      _M0L6_2atmpS2146
      = ((moonbit_string_t)moonbit_string_literal_103.data)[
        _M0L2loS303
      ];
      _M0L6bufferS304[_M0L6_2atmpS2145] = _M0L6_2atmpS2146;
      _M0L6_2atmpS2148 = _M0L6offsetS299 - 2;
      _M0L6_2atmpS2149 = _M0L1nS300 >> 8;
      _M0L6offsetS299 = _M0L6_2atmpS2148;
      _M0L1nS300 = _M0L6_2atmpS2149;
      continue;
    } else if (_M0L6offsetS299 == 1) {
      uint64_t _M0L6_2atmpS2152 = _M0L1nS300 & 15ull;
      int32_t _M0L6nibbleS307 = (int32_t)_M0L6_2atmpS2152;
      int32_t _M0L6_2atmpS2151 =
        ((moonbit_string_t)moonbit_string_literal_103.data)[_M0L6nibbleS307];
      _M0L6bufferS304[_M0L12digit__startS305] = _M0L6_2atmpS2151;
    }
    break;
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS293,
  int32_t _M0L5radixS295
) {
  uint64_t _M0L4baseS294;
  uint64_t _M0L3numS296;
  int32_t _M0L5countS297;
  #line 419 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS293 == 0ull) {
    return 1;
  }
  #line 424 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS294 = _M0MPC13int3Int10to__uint64(_M0L5radixS295);
  _M0L3numS296 = _M0L5valueS293;
  _M0L5countS297 = 0;
  while (1) {
    if (_M0L3numS296 > 0ull) {
      uint64_t _M0L6_2atmpS2140 = _M0L3numS296 / _M0L4baseS294;
      int32_t _M0L6_2atmpS2141 = _M0L5countS297 + 1;
      _M0L3numS296 = _M0L6_2atmpS2140;
      _M0L5countS297 = _M0L6_2atmpS2141;
      continue;
    } else {
      return _M0L5countS297;
    }
    break;
  }
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS291) {
  #line 407 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS291 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS292;
    int32_t _M0L6_2atmpS2139;
    int32_t _M0L6_2atmpS2138;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS292 = moonbit_clz64(_M0L5valueS291);
    _M0L6_2atmpS2139 = 63 - _M0L14leading__zerosS292;
    _M0L6_2atmpS2138 = _M0L6_2atmpS2139 / 4;
    return _M0L6_2atmpS2138 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS290) {
  #line 343 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS290 >= 10000000000ull) {
    if (_M0L5valueS290 >= 100000000000000ull) {
      if (_M0L5valueS290 >= 10000000000000000ull) {
        if (_M0L5valueS290 >= 1000000000000000000ull) {
          if (_M0L5valueS290 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS290 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS290 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS290 >= 1000000000000ull) {
      if (_M0L5valueS290 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS290 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS290 >= 100000ull) {
    if (_M0L5valueS290 >= 10000000ull) {
      if (_M0L5valueS290 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS290 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS290 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS290 >= 1000ull) {
    if (_M0L5valueS290 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS290 >= 100ull) {
    return 3;
  } else if (_M0L5valueS290 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS274,
  int32_t _M0L5radixS273
) {
  int32_t _if__result_4109;
  int32_t _M0L12is__negativeS275;
  uint32_t _M0L3numS276;
  uint16_t* _M0L6bufferS277;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS273 < 2) {
    _if__result_4109 = 1;
  } else {
    _if__result_4109 = _M0L5radixS273 > 36;
  }
  if (_if__result_4109) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_102.data);
  }
  if (_M0L4selfS274 == 0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  _M0L12is__negativeS275 = _M0L4selfS274 < 0;
  if (_M0L12is__negativeS275) {
    int32_t _M0L6_2atmpS2137 = -_M0L4selfS274;
    _M0L3numS276 = *(uint32_t*)&_M0L6_2atmpS2137;
  } else {
    _M0L3numS276 = *(uint32_t*)&_M0L4selfS274;
  }
  switch (_M0L5radixS273) {
    case 10: {
      int32_t _M0L10digit__lenS278;
      int32_t _M0L6_2atmpS2134;
      int32_t _M0L10total__lenS279;
      uint16_t* _M0L6bufferS280;
      int32_t _M0L12digit__startS281;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS278 = _M0FPB12dec__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2134 = 1;
      } else {
        _M0L6_2atmpS2134 = 0;
      }
      _M0L10total__lenS279 = _M0L10digit__lenS278 + _M0L6_2atmpS2134;
      _M0L6bufferS280
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS279, 0);
      if (_M0L12is__negativeS275) {
        _M0L12digit__startS281 = 1;
      } else {
        _M0L12digit__startS281 = 0;
      }
      #line 239 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS280, _M0L3numS276, _M0L12digit__startS281, _M0L10total__lenS279);
      _M0L6bufferS277 = _M0L6bufferS280;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS282;
      int32_t _M0L6_2atmpS2135;
      int32_t _M0L10total__lenS283;
      uint16_t* _M0L6bufferS284;
      int32_t _M0L12digit__startS285;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS282 = _M0FPB12hex__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2135 = 1;
      } else {
        _M0L6_2atmpS2135 = 0;
      }
      _M0L10total__lenS283 = _M0L10digit__lenS282 + _M0L6_2atmpS2135;
      _M0L6bufferS284
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS283, 0);
      if (_M0L12is__negativeS275) {
        _M0L12digit__startS285 = 1;
      } else {
        _M0L12digit__startS285 = 0;
      }
      #line 247 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS284, _M0L3numS276, _M0L12digit__startS285, _M0L10total__lenS283);
      _M0L6bufferS277 = _M0L6bufferS284;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS286;
      int32_t _M0L6_2atmpS2136;
      int32_t _M0L10total__lenS287;
      uint16_t* _M0L6bufferS288;
      int32_t _M0L12digit__startS289;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS286
      = _M0FPB14radix__count32(_M0L3numS276, _M0L5radixS273);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2136 = 1;
      } else {
        _M0L6_2atmpS2136 = 0;
      }
      _M0L10total__lenS287 = _M0L10digit__lenS286 + _M0L6_2atmpS2136;
      _M0L6bufferS288
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS287, 0);
      if (_M0L12is__negativeS275) {
        _M0L12digit__startS289 = 1;
      } else {
        _M0L12digit__startS289 = 0;
      }
      #line 255 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS288, _M0L3numS276, _M0L12digit__startS289, _M0L10total__lenS287, _M0L5radixS273);
      _M0L6bufferS277 = _M0L6bufferS288;
      break;
    }
  }
  if (_M0L12is__negativeS275) {
    _M0L6bufferS277[0] = 45;
  }
  return _M0L6bufferS277;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS267,
  int32_t _M0L5radixS269
) {
  uint32_t _M0L4baseS268;
  uint32_t _M0L3numS270;
  int32_t _M0L5countS271;
  #line 189 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS267 == 0u) {
    return 1;
  }
  _M0L4baseS268 = *(uint32_t*)&_M0L5radixS269;
  _M0L3numS270 = _M0L5valueS267;
  _M0L5countS271 = 0;
  while (1) {
    if (_M0L3numS270 > 0u) {
      uint32_t _M0L6_2atmpS2132 = _M0L3numS270 / _M0L4baseS268;
      int32_t _M0L6_2atmpS2133 = _M0L5countS271 + 1;
      _M0L3numS270 = _M0L6_2atmpS2132;
      _M0L5countS271 = _M0L6_2atmpS2133;
      continue;
    } else {
      return _M0L5countS271;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS265) {
  #line 177 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS265 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS266;
    int32_t _M0L6_2atmpS2131;
    int32_t _M0L6_2atmpS2130;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS266 = moonbit_clz32(_M0L5valueS265);
    _M0L6_2atmpS2131 = 31 - _M0L14leading__zerosS266;
    _M0L6_2atmpS2130 = _M0L6_2atmpS2131 / 4;
    return _M0L6_2atmpS2130 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS264) {
  #line 143 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS264 >= 100000u) {
    if (_M0L5valueS264 >= 10000000u) {
      if (_M0L5valueS264 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS264 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS264 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS264 >= 1000u) {
    if (_M0L5valueS264 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS264 >= 100u) {
    return 3;
  } else if (_M0L5valueS264 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS250,
  uint32_t _M0L3numS262,
  int32_t _M0L12digit__startS251,
  int32_t _M0L10total__lenS263
) {
  int32_t _M0L6_2atmpS2129;
  uint32_t _M0L3numS240;
  int32_t _M0L6offsetS241;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2129 = _M0L10total__lenS263 - _M0L12digit__startS251;
  _M0L3numS240 = _M0L3numS262;
  _M0L6offsetS241 = _M0L6_2atmpS2129;
  while (1) {
    if (_M0L3numS240 >= 10000u) {
      uint32_t _M0L1tS242 = _M0L3numS240 / 10000u;
      uint32_t _M0L6_2atmpS2106 = _M0L3numS240 % 10000u;
      int32_t _M0L1rS243 = *(int32_t*)&_M0L6_2atmpS2106;
      int32_t _M0L2d1S244 = _M0L1rS243 / 100;
      int32_t _M0L2d2S245 = _M0L1rS243 % 100;
      int32_t _M0L6_2atmpS2105 = _M0L2d1S244 / 10;
      int32_t _M0L6_2atmpS2104 = 48 + _M0L6_2atmpS2105;
      int32_t _M0L6d1__hiS246 = (uint16_t)_M0L6_2atmpS2104;
      int32_t _M0L6_2atmpS2103 = _M0L2d1S244 % 10;
      int32_t _M0L6_2atmpS2102 = 48 + _M0L6_2atmpS2103;
      int32_t _M0L6d1__loS247 = (uint16_t)_M0L6_2atmpS2102;
      int32_t _M0L6_2atmpS2101 = _M0L2d2S245 / 10;
      int32_t _M0L6_2atmpS2100 = 48 + _M0L6_2atmpS2101;
      int32_t _M0L6d2__hiS248 = (uint16_t)_M0L6_2atmpS2100;
      int32_t _M0L6_2atmpS2099 = _M0L2d2S245 % 10;
      int32_t _M0L6_2atmpS2098 = 48 + _M0L6_2atmpS2099;
      int32_t _M0L6d2__loS249 = (uint16_t)_M0L6_2atmpS2098;
      int32_t _M0L6_2atmpS2090 = _M0L12digit__startS251 + _M0L6offsetS241;
      int32_t _M0L6_2atmpS2089 = _M0L6_2atmpS2090 - 4;
      int32_t _M0L6_2atmpS2092;
      int32_t _M0L6_2atmpS2091;
      int32_t _M0L6_2atmpS2094;
      int32_t _M0L6_2atmpS2093;
      int32_t _M0L6_2atmpS2096;
      int32_t _M0L6_2atmpS2095;
      int32_t _M0L6_2atmpS2097;
      _M0L6bufferS250[_M0L6_2atmpS2089] = _M0L6d1__hiS246;
      _M0L6_2atmpS2092 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2091 = _M0L6_2atmpS2092 - 3;
      _M0L6bufferS250[_M0L6_2atmpS2091] = _M0L6d1__loS247;
      _M0L6_2atmpS2094 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2093 = _M0L6_2atmpS2094 - 2;
      _M0L6bufferS250[_M0L6_2atmpS2093] = _M0L6d2__hiS248;
      _M0L6_2atmpS2096 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2095 = _M0L6_2atmpS2096 - 1;
      _M0L6bufferS250[_M0L6_2atmpS2095] = _M0L6d2__loS249;
      _M0L6_2atmpS2097 = _M0L6offsetS241 - 4;
      _M0L3numS240 = _M0L1tS242;
      _M0L6offsetS241 = _M0L6_2atmpS2097;
      continue;
    } else {
      int32_t _M0L6_2atmpS2128 = *(int32_t*)&_M0L3numS240;
      int32_t _M0L9remainingS253 = _M0L6_2atmpS2128;
      int32_t _M0L6offsetS254 = _M0L6offsetS241;
      while (1) {
        if (_M0L9remainingS253 >= 100) {
          int32_t _M0L1tS255 = _M0L9remainingS253 / 100;
          int32_t _M0L1dS256 = _M0L9remainingS253 % 100;
          int32_t _M0L6_2atmpS2115 = _M0L1dS256 / 10;
          int32_t _M0L6_2atmpS2114 = 48 + _M0L6_2atmpS2115;
          int32_t _M0L5d__hiS257 = (uint16_t)_M0L6_2atmpS2114;
          int32_t _M0L6_2atmpS2113 = _M0L1dS256 % 10;
          int32_t _M0L6_2atmpS2112 = 48 + _M0L6_2atmpS2113;
          int32_t _M0L5d__loS258 = (uint16_t)_M0L6_2atmpS2112;
          int32_t _M0L6_2atmpS2108 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2107 = _M0L6_2atmpS2108 - 2;
          int32_t _M0L6_2atmpS2110;
          int32_t _M0L6_2atmpS2109;
          int32_t _M0L6_2atmpS2111;
          _M0L6bufferS250[_M0L6_2atmpS2107] = _M0L5d__hiS257;
          _M0L6_2atmpS2110 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2109 = _M0L6_2atmpS2110 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2109] = _M0L5d__loS258;
          _M0L6_2atmpS2111 = _M0L6offsetS254 - 2;
          _M0L9remainingS253 = _M0L1tS255;
          _M0L6offsetS254 = _M0L6_2atmpS2111;
          continue;
        } else if (_M0L9remainingS253 >= 10) {
          int32_t _M0L6_2atmpS2123 = _M0L9remainingS253 / 10;
          int32_t _M0L6_2atmpS2122 = 48 + _M0L6_2atmpS2123;
          int32_t _M0L5d__hiS260 = (uint16_t)_M0L6_2atmpS2122;
          int32_t _M0L6_2atmpS2121 = _M0L9remainingS253 % 10;
          int32_t _M0L6_2atmpS2120 = 48 + _M0L6_2atmpS2121;
          int32_t _M0L5d__loS261 = (uint16_t)_M0L6_2atmpS2120;
          int32_t _M0L6_2atmpS2117 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2116 = _M0L6_2atmpS2117 - 2;
          int32_t _M0L6_2atmpS2119;
          int32_t _M0L6_2atmpS2118;
          _M0L6bufferS250[_M0L6_2atmpS2116] = _M0L5d__hiS260;
          _M0L6_2atmpS2119 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2118 = _M0L6_2atmpS2119 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2118] = _M0L5d__loS261;
        } else {
          int32_t _M0L6_2atmpS2127 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2124 = _M0L6_2atmpS2127 - 1;
          int32_t _M0L6_2atmpS2126 = 48 + _M0L9remainingS253;
          int32_t _M0L6_2atmpS2125 = (uint16_t)_M0L6_2atmpS2126;
          _M0L6bufferS250[_M0L6_2atmpS2124] = _M0L6_2atmpS2125;
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS230,
  uint32_t _M0L3numS234,
  int32_t _M0L12digit__startS231,
  int32_t _M0L10total__lenS233,
  int32_t _M0L5radixS224
) {
  uint32_t _M0L4baseS223;
  int32_t _M0L6_2atmpS2074;
  int32_t _M0L6_2atmpS2073;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS223 = *(uint32_t*)&_M0L5radixS224;
  _M0L6_2atmpS2074 = _M0L5radixS224 - 1;
  _M0L6_2atmpS2073 = _M0L5radixS224 & _M0L6_2atmpS2074;
  if (_M0L6_2atmpS2073 == 0) {
    int32_t _M0L5shiftS225;
    uint32_t _M0L4maskS226;
    int32_t _M0L6_2atmpS2081;
    int32_t _M0L6offsetS227;
    uint32_t _M0L1nS228;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS225 = moonbit_ctz32(_M0L5radixS224);
    _M0L4maskS226 = _M0L4baseS223 - 1u;
    _M0L6_2atmpS2081 = _M0L10total__lenS233 - _M0L12digit__startS231;
    _M0L6offsetS227 = _M0L6_2atmpS2081;
    _M0L1nS228 = _M0L3numS234;
    while (1) {
      if (_M0L1nS228 > 0u) {
        uint32_t _M0L6_2atmpS2080 = _M0L1nS228 & _M0L4maskS226;
        int32_t _M0L5digitS229 = *(int32_t*)&_M0L6_2atmpS2080;
        int32_t _M0L6_2atmpS2077 = _M0L12digit__startS231 + _M0L6offsetS227;
        int32_t _M0L6_2atmpS2075 = _M0L6_2atmpS2077 - 1;
        int32_t _M0L6_2atmpS2076 =
          ((moonbit_string_t)moonbit_string_literal_103.data)[_M0L5digitS229];
        int32_t _M0L6_2atmpS2078;
        uint32_t _M0L6_2atmpS2079;
        _M0L6bufferS230[_M0L6_2atmpS2075] = _M0L6_2atmpS2076;
        _M0L6_2atmpS2078 = _M0L6offsetS227 - 1;
        _M0L6_2atmpS2079 = _M0L1nS228 >> (_M0L5shiftS225 & 31);
        _M0L6offsetS227 = _M0L6_2atmpS2078;
        _M0L1nS228 = _M0L6_2atmpS2079;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2088 = _M0L10total__lenS233 - _M0L12digit__startS231;
    int32_t _M0L6offsetS235 = _M0L6_2atmpS2088;
    uint32_t _M0L1nS236 = _M0L3numS234;
    while (1) {
      if (_M0L1nS236 > 0u) {
        uint32_t _M0L1qS237 = _M0L1nS236 / _M0L4baseS223;
        uint32_t _M0L6_2atmpS2087 = _M0L1qS237 * _M0L4baseS223;
        uint32_t _M0L6_2atmpS2086 = _M0L1nS236 - _M0L6_2atmpS2087;
        int32_t _M0L5digitS238 = *(int32_t*)&_M0L6_2atmpS2086;
        int32_t _M0L6_2atmpS2084 = _M0L12digit__startS231 + _M0L6offsetS235;
        int32_t _M0L6_2atmpS2082 = _M0L6_2atmpS2084 - 1;
        int32_t _M0L6_2atmpS2083 =
          ((moonbit_string_t)moonbit_string_literal_103.data)[_M0L5digitS238];
        int32_t _M0L6_2atmpS2085;
        _M0L6bufferS230[_M0L6_2atmpS2082] = _M0L6_2atmpS2083;
        _M0L6_2atmpS2085 = _M0L6offsetS235 - 1;
        _M0L6offsetS235 = _M0L6_2atmpS2085;
        _M0L1nS236 = _M0L1qS237;
        continue;
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS217,
  uint32_t _M0L3numS222,
  int32_t _M0L12digit__startS218,
  int32_t _M0L10total__lenS221
) {
  int32_t _M0L6_2atmpS2072;
  int32_t _M0L6offsetS212;
  uint32_t _M0L1nS213;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2072 = _M0L10total__lenS221 - _M0L12digit__startS218;
  _M0L6offsetS212 = _M0L6_2atmpS2072;
  _M0L1nS213 = _M0L3numS222;
  while (1) {
    if (_M0L6offsetS212 >= 2) {
      uint32_t _M0L6_2atmpS2069 = _M0L1nS213 & 255u;
      int32_t _M0L9byte__valS214 = *(int32_t*)&_M0L6_2atmpS2069;
      int32_t _M0L2hiS215 = _M0L9byte__valS214 / 16;
      int32_t _M0L2loS216 = _M0L9byte__valS214 % 16;
      int32_t _M0L6_2atmpS2063 = _M0L12digit__startS218 + _M0L6offsetS212;
      int32_t _M0L6_2atmpS2061 = _M0L6_2atmpS2063 - 2;
      int32_t _M0L6_2atmpS2062 =
        ((moonbit_string_t)moonbit_string_literal_103.data)[_M0L2hiS215];
      int32_t _M0L6_2atmpS2066;
      int32_t _M0L6_2atmpS2064;
      int32_t _M0L6_2atmpS2065;
      int32_t _M0L6_2atmpS2067;
      uint32_t _M0L6_2atmpS2068;
      _M0L6bufferS217[_M0L6_2atmpS2061] = _M0L6_2atmpS2062;
      _M0L6_2atmpS2066 = _M0L12digit__startS218 + _M0L6offsetS212;
      _M0L6_2atmpS2064 = _M0L6_2atmpS2066 - 1;
      _M0L6_2atmpS2065
      = ((moonbit_string_t)moonbit_string_literal_103.data)[
        _M0L2loS216
      ];
      _M0L6bufferS217[_M0L6_2atmpS2064] = _M0L6_2atmpS2065;
      _M0L6_2atmpS2067 = _M0L6offsetS212 - 2;
      _M0L6_2atmpS2068 = _M0L1nS213 >> 8;
      _M0L6offsetS212 = _M0L6_2atmpS2067;
      _M0L1nS213 = _M0L6_2atmpS2068;
      continue;
    } else if (_M0L6offsetS212 == 1) {
      uint32_t _M0L6_2atmpS2071 = _M0L1nS213 & 15u;
      int32_t _M0L6nibbleS220 = *(int32_t*)&_M0L6_2atmpS2071;
      int32_t _M0L6_2atmpS2070 =
        ((moonbit_string_t)moonbit_string_literal_103.data)[_M0L6nibbleS220];
      _M0L6bufferS217[_M0L12digit__startS218] = _M0L6_2atmpS2070;
    }
    break;
  }
  return 0;
}

struct _M0TUssE* _M0MPB4Iter4nextGUssEE(
  struct _M0TPB4IterGUssEE* _M0L4selfS189
) {
  struct _M0TWEOUssE* _M0L7_2afuncS188;
  struct _M0TUssE* _M0L6resultS190;
  int64_t _M0L7_2abindS191;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS188 = _M0L4selfS189->$0;
  moonbit_incref(_M0L7_2afuncS188);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS190 = _M0L7_2afuncS188->code(_M0L7_2afuncS188);
  moonbit_decref(_M0L7_2afuncS188);
  _M0L7_2abindS191 = _M0L4selfS189->$1;
  if (_M0L6resultS190 == 0) {
    _M0L4selfS189->$1 = _M0MPB4Iter4nextN6constrS9981GUssEE;
  } else if (_M0L7_2abindS191 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS192 = _M0L7_2abindS191;
    int32_t _M0L4_2anS193 = (int32_t)_M0L7_2aSomeS192;
    int64_t _M0L6_2atmpS2053;
    if (_M0L4_2anS193 > 0) {
      int32_t _M0L6_2atmpS2054 = _M0L4_2anS193 - 1;
      _M0L6_2atmpS2053 = (int64_t)_M0L6_2atmpS2054;
    } else {
      _M0L6_2atmpS2053 = _M0MPB4Iter4nextN6constrS9980GUssEE;
    }
    _M0L4selfS189->$1 = _M0L6_2atmpS2053;
  }
  return _M0L6resultS190;
}

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS195
) {
  struct _M0TWEOUsbE* _M0L7_2afuncS194;
  struct _M0TUsbE* _M0L6resultS196;
  int64_t _M0L7_2abindS197;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS194 = _M0L4selfS195->$0;
  moonbit_incref(_M0L7_2afuncS194);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS196 = _M0L7_2afuncS194->code(_M0L7_2afuncS194);
  moonbit_decref(_M0L7_2afuncS194);
  _M0L7_2abindS197 = _M0L4selfS195->$1;
  if (_M0L6resultS196 == 0) {
    _M0L4selfS195->$1 = _M0MPB4Iter4nextN6constrS9981GUsbEE;
  } else if (_M0L7_2abindS197 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS198 = _M0L7_2abindS197;
    int32_t _M0L4_2anS199 = (int32_t)_M0L7_2aSomeS198;
    int64_t _M0L6_2atmpS2055;
    if (_M0L4_2anS199 > 0) {
      int32_t _M0L6_2atmpS2056 = _M0L4_2anS199 - 1;
      _M0L6_2atmpS2055 = (int64_t)_M0L6_2atmpS2056;
    } else {
      _M0L6_2atmpS2055 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS195->$1 = _M0L6_2atmpS2055;
  }
  return _M0L6resultS196;
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L4selfS201
) {
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2afuncS200;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6resultS202;
  int64_t _M0L7_2abindS203;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS200 = _M0L4selfS201->$0;
  moonbit_incref(_M0L7_2afuncS200);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS202 = _M0L7_2afuncS200->code(_M0L7_2afuncS200);
  moonbit_decref(_M0L7_2afuncS200);
  _M0L7_2abindS203 = _M0L4selfS201->$1;
  if (_M0L6resultS202 == 0) {
    _M0L4selfS201->$1
    = _M0MPB4Iter4nextN6constrS9981GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
  } else if (_M0L7_2abindS203 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS204 = _M0L7_2abindS203;
    int32_t _M0L4_2anS205 = (int32_t)_M0L7_2aSomeS204;
    int64_t _M0L6_2atmpS2057;
    if (_M0L4_2anS205 > 0) {
      int32_t _M0L6_2atmpS2058 = _M0L4_2anS205 - 1;
      _M0L6_2atmpS2057 = (int64_t)_M0L6_2atmpS2058;
    } else {
      _M0L6_2atmpS2057
      = _M0MPB4Iter4nextN6constrS9980GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
    }
    _M0L4selfS201->$1 = _M0L6_2atmpS2057;
  }
  return _M0L6resultS202;
}

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS207
) {
  struct _M0TWEOUsfE* _M0L7_2afuncS206;
  struct _M0TUsfE* _M0L6resultS208;
  int64_t _M0L7_2abindS209;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS206 = _M0L4selfS207->$0;
  moonbit_incref(_M0L7_2afuncS206);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS208 = _M0L7_2afuncS206->code(_M0L7_2afuncS206);
  moonbit_decref(_M0L7_2afuncS206);
  _M0L7_2abindS209 = _M0L4selfS207->$1;
  if (_M0L6resultS208 == 0) {
    _M0L4selfS207->$1 = _M0MPB4Iter4nextN6constrS9981GUsfEE;
  } else if (_M0L7_2abindS209 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS210 = _M0L7_2abindS209;
    int32_t _M0L4_2anS211 = (int32_t)_M0L7_2aSomeS210;
    int64_t _M0L6_2atmpS2059;
    if (_M0L4_2anS211 > 0) {
      int32_t _M0L6_2atmpS2060 = _M0L4_2anS211 - 1;
      _M0L6_2atmpS2059 = (int64_t)_M0L6_2atmpS2060;
    } else {
      _M0L6_2atmpS2059 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS207->$1 = _M0L6_2atmpS2059;
  }
  return _M0L6resultS208;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS179,
  struct _M0TPB6Logger _M0L6loggerS178
) {
  moonbit_string_t _M0L6_2atmpS2048;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2048 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS179);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS178.$0->$method_0(_M0L6loggerS178.$1, _M0L6_2atmpS2048);
  moonbit_decref(_M0L6_2atmpS2048);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS181,
  struct _M0TPB6Logger _M0L6loggerS180
) {
  moonbit_string_t _M0L6_2atmpS2049;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2049 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS181);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS180.$0->$method_0(_M0L6loggerS180.$1, _M0L6_2atmpS2049);
  moonbit_decref(_M0L6_2atmpS2049);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGbE(
  int32_t _M0L4selfS183,
  struct _M0TPB6Logger _M0L6loggerS182
) {
  moonbit_string_t _M0L6_2atmpS2050;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2050 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS183);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS182.$0->$method_0(_M0L6loggerS182.$1, _M0L6_2atmpS2050);
  moonbit_decref(_M0L6_2atmpS2050);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS185,
  struct _M0TPB6Logger _M0L6loggerS184
) {
  moonbit_string_t _M0L6_2atmpS2051;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2051 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS185);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS184.$0->$method_0(_M0L6loggerS184.$1, _M0L6_2atmpS2051);
  moonbit_decref(_M0L6_2atmpS2051);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS187,
  struct _M0TPB6Logger _M0L6loggerS186
) {
  moonbit_string_t _M0L6_2atmpS2052;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2052 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS187);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS186.$0->$method_0(_M0L6loggerS186.$1, _M0L6_2atmpS2052);
  moonbit_decref(_M0L6_2atmpS2052);
  return 0;
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS177
) {
  #line 99 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  return _M0L4selfS177.$1;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS176
) {
  moonbit_string_t _M0L8_2afieldS3785;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS3785 = _M0L4selfS176.$0;
  moonbit_incref(_M0L8_2afieldS3785);
  return _M0L8_2afieldS3785;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS172,
  moonbit_string_t _M0L5valueS173,
  int32_t _M0L5startS174,
  int32_t _M0L3lenS175
) {
  int32_t _M0L6_2atmpS2047;
  int64_t _M0L6_2atmpS2046;
  struct _M0TPC16string10StringView _M0L6_2atmpS2045;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2047 = _M0L5startS174 + _M0L3lenS175;
  _M0L6_2atmpS2046 = (int64_t)_M0L6_2atmpS2047;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2045
  = _M0MPC16string6String11sub_2einner(_M0L5valueS173, _M0L5startS174, _M0L6_2atmpS2046);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS172, _M0L6_2atmpS2045);
  moonbit_decref(_M0L6_2atmpS2045.$0);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS165,
  int32_t _M0L5startS171,
  int64_t _M0L3endS167
) {
  int32_t _M0L3lenS164;
  int32_t _M0L3endS166;
  int32_t _M0L5startS170;
  int32_t _if__result_4116;
  #line 755 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS164 = Moonbit_array_length(_M0L4selfS165);
  if (_M0L3endS167 == 4294967296ll) {
    _M0L3endS166 = _M0L3lenS164;
  } else {
    int64_t _M0L7_2aSomeS168 = _M0L3endS167;
    int32_t _M0L6_2aendS169 = (int32_t)_M0L7_2aSomeS168;
    if (_M0L6_2aendS169 < 0) {
      _M0L3endS166 = _M0L3lenS164 + _M0L6_2aendS169;
    } else {
      _M0L3endS166 = _M0L6_2aendS169;
    }
  }
  if (_M0L5startS171 < 0) {
    _M0L5startS170 = _M0L3lenS164 + _M0L5startS171;
  } else {
    _M0L5startS170 = _M0L5startS171;
  }
  if (_M0L5startS170 >= 0) {
    if (_M0L5startS170 <= _M0L3endS166) {
      _if__result_4116 = _M0L3endS166 <= _M0L3lenS164;
    } else {
      _if__result_4116 = 0;
    }
  } else {
    _if__result_4116 = 0;
  }
  if (_if__result_4116) {
    if (_M0L5startS170 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2042 = _M0L4selfS165[_M0L5startS170];
      int32_t _M0L6_2atmpS2041;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2041
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2042);
      if (!_M0L6_2atmpS2041) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS166 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2044 = _M0L4selfS165[_M0L3endS166];
      int32_t _M0L6_2atmpS2043;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2043
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2044);
      if (!_M0L6_2atmpS2043) {
        
      } else {
        #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    moonbit_incref(_M0L4selfS165);
    return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS165,
                                                 .$1 = _M0L5startS170,
                                                 .$2 = _M0L3endS166};
  } else {
    #line 763 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS163,
  struct _M0TPB4Show _M0L4showS162
) {
  struct _M0TPB6Logger _M0L6_2atmpS2040;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS163);
  _M0L6_2atmpS2040
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS163
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS162.$0->$method_0(_M0L4showS162.$1, _M0L6_2atmpS2040);
  if (_M0L6_2atmpS2040.$1) {
    moonbit_decref(_M0L6_2atmpS2040.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS161,
  struct _M0TPB4Show _M0L4showS160
) {
  struct _M0TPB6Logger _M0L6_2atmpS2039;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS161);
  _M0L6_2atmpS2039
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS161
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS160.$0->$method_0(_M0L4showS160.$1, _M0L6_2atmpS2039);
  if (_M0L6_2atmpS2039.$1) {
    moonbit_decref(_M0L6_2atmpS2039.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS159) {
  uint32_t _M0L6_2atmpS2038;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2038 = _M0FPB14avalanche__acc(_M0L3accS159);
  return *(int32_t*)&_M0L6_2atmpS2038;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS158) {
  uint32_t _M0Lm3accS157;
  uint32_t _M0L6_2atmpS2027;
  uint32_t _M0L6_2atmpS2029;
  uint32_t _M0L6_2atmpS2028;
  uint32_t _M0L6_2atmpS2030;
  uint32_t _M0L6_2atmpS2031;
  uint32_t _M0L6_2atmpS2033;
  uint32_t _M0L6_2atmpS2032;
  uint32_t _M0L6_2atmpS2034;
  uint32_t _M0L6_2atmpS2035;
  uint32_t _M0L6_2atmpS2037;
  uint32_t _M0L6_2atmpS2036;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS157 = _M0L3accS158;
  _M0L6_2atmpS2027 = _M0Lm3accS157;
  _M0L6_2atmpS2029 = _M0Lm3accS157;
  _M0L6_2atmpS2028 = _M0L6_2atmpS2029 >> 15;
  _M0Lm3accS157 = _M0L6_2atmpS2027 ^ _M0L6_2atmpS2028;
  _M0L6_2atmpS2030 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2030 * 2246822519u;
  _M0L6_2atmpS2031 = _M0Lm3accS157;
  _M0L6_2atmpS2033 = _M0Lm3accS157;
  _M0L6_2atmpS2032 = _M0L6_2atmpS2033 >> 13;
  _M0Lm3accS157 = _M0L6_2atmpS2031 ^ _M0L6_2atmpS2032;
  _M0L6_2atmpS2034 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2034 * 3266489917u;
  _M0L6_2atmpS2035 = _M0Lm3accS157;
  _M0L6_2atmpS2037 = _M0Lm3accS157;
  _M0L6_2atmpS2036 = _M0L6_2atmpS2037 >> 16;
  _M0Lm3accS157 = _M0L6_2atmpS2035 ^ _M0L6_2atmpS2036;
  return _M0Lm3accS157;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS156) {
  int64_t _M0L6_2atmpS2026;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2026 = (int64_t)_M0L4selfS156;
  return *(uint64_t*)&_M0L6_2atmpS2026;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS155,
  moonbit_string_t _M0L3strS154
) {
  int32_t _M0L8str__lenS153;
  int32_t _M0L3lenS2021;
  int32_t _M0L6_2atmpS2020;
  uint16_t* _M0L4dataS2022;
  int32_t _M0L3lenS2023;
  int32_t _M0L3lenS2025;
  int32_t _M0L6_2atmpS2024;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS153 = Moonbit_array_length(_M0L3strS154);
  _M0L3lenS2021 = _M0L4selfS155->$1;
  _M0L6_2atmpS2020 = _M0L3lenS2021 + _M0L8str__lenS153;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS155, _M0L6_2atmpS2020);
  _M0L4dataS2022 = _M0L4selfS155->$0;
  _M0L3lenS2023 = _M0L4selfS155->$1;
  moonbit_incref(_M0L4dataS2022);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2022, _M0L3lenS2023, _M0L3strS154, 0, _M0L8str__lenS153);
  moonbit_decref(_M0L4dataS2022);
  _M0L3lenS2025 = _M0L4selfS155->$1;
  _M0L6_2atmpS2024 = _M0L3lenS2025 + _M0L8str__lenS153;
  _M0L4selfS155->$1 = _M0L6_2atmpS2024;
  return 0;
}

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t* _M0L4selfS149,
  int32_t _M0L11dst__offsetS152,
  moonbit_string_t _M0L3strS150,
  int32_t _M0L11str__offsetS145,
  int32_t _M0L3lenS146
) {
  int32_t _M0L16end__str__offsetS144;
  int32_t _M0L1iS147;
  int32_t _M0L1jS148;
  #line 71 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L16end__str__offsetS144 = _M0L11str__offsetS145 + _M0L3lenS146;
  _M0L1iS147 = _M0L11str__offsetS145;
  _M0L1jS148 = _M0L11dst__offsetS152;
  while (1) {
    if (_M0L1iS147 < _M0L16end__str__offsetS144) {
      int32_t _M0L6_2atmpS2017 = _M0L3strS150[_M0L1iS147];
      int32_t _M0L6_2atmpS2018;
      int32_t _M0L6_2atmpS2019;
      if (
        _M0L1jS148 < 0 || _M0L1jS148 >= Moonbit_array_length(_M0L4selfS149)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS149[_M0L1jS148] = _M0L6_2atmpS2017;
      _M0L6_2atmpS2018 = _M0L1iS147 + 1;
      _M0L6_2atmpS2019 = _M0L1jS148 + 1;
      _M0L1iS147 = _M0L6_2atmpS2018;
      _M0L1jS148 = _M0L6_2atmpS2019;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS143) {
  #line 45 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS143 >= 56320) {
    return _M0L4selfS143 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS141,
  int32_t _M0L2chS140
) {
  uint32_t _M0L4codeS139;
  #line 95 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  #line 96 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4codeS139 = _M0MPC14char4Char8to__uint(_M0L2chS140);
  if (_M0L4codeS139 <= 65535u) {
    int32_t _M0L3lenS1996 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS1995 = _M0L3lenS1996 + 1;
    uint16_t* _M0L4dataS1997;
    int32_t _M0L3lenS1998;
    int32_t _M0L6_2atmpS1999;
    int32_t _M0L3lenS2001;
    int32_t _M0L6_2atmpS2000;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS1995);
    _M0L4dataS1997 = _M0L4selfS141->$0;
    _M0L3lenS1998 = _M0L4selfS141->$1;
    moonbit_incref(_M0L4dataS1997);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1999 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS139);
    if (
      _M0L3lenS1998 < 0
      || _M0L3lenS1998 >= Moonbit_array_length(_M0L4dataS1997)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1997[_M0L3lenS1998] = _M0L6_2atmpS1999;
    moonbit_decref(_M0L4dataS1997);
    _M0L3lenS2001 = _M0L4selfS141->$1;
    _M0L6_2atmpS2000 = _M0L3lenS2001 + 1;
    _M0L4selfS141->$1 = _M0L6_2atmpS2000;
  } else if (_M0L4codeS139 <= 1114111u) {
    int32_t _M0L3lenS2003 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2002 = _M0L3lenS2003 + 2;
    uint32_t _M0L4codeS142;
    uint16_t* _M0L4dataS2004;
    int32_t _M0L3lenS2005;
    uint32_t _M0L6_2atmpS2008;
    uint32_t _M0L6_2atmpS2007;
    int32_t _M0L6_2atmpS2006;
    uint16_t* _M0L4dataS2009;
    int32_t _M0L3lenS2014;
    int32_t _M0L6_2atmpS2010;
    uint32_t _M0L6_2atmpS2013;
    uint32_t _M0L6_2atmpS2012;
    int32_t _M0L6_2atmpS2011;
    int32_t _M0L3lenS2016;
    int32_t _M0L6_2atmpS2015;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2002);
    _M0L4codeS142 = _M0L4codeS139 - 65536u;
    _M0L4dataS2004 = _M0L4selfS141->$0;
    _M0L3lenS2005 = _M0L4selfS141->$1;
    _M0L6_2atmpS2008 = _M0L4codeS142 >> 10;
    _M0L6_2atmpS2007 = 55296u + _M0L6_2atmpS2008;
    moonbit_incref(_M0L4dataS2004);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2006 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2007);
    if (
      _M0L3lenS2005 < 0
      || _M0L3lenS2005 >= Moonbit_array_length(_M0L4dataS2004)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2004[_M0L3lenS2005] = _M0L6_2atmpS2006;
    moonbit_decref(_M0L4dataS2004);
    _M0L4dataS2009 = _M0L4selfS141->$0;
    _M0L3lenS2014 = _M0L4selfS141->$1;
    _M0L6_2atmpS2010 = _M0L3lenS2014 + 1;
    _M0L6_2atmpS2013 = _M0L4codeS142 & 1023u;
    _M0L6_2atmpS2012 = 56320u + _M0L6_2atmpS2013;
    moonbit_incref(_M0L4dataS2009);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2011 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2012);
    if (
      _M0L6_2atmpS2010 < 0
      || _M0L6_2atmpS2010 >= Moonbit_array_length(_M0L4dataS2009)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2009[_M0L6_2atmpS2010] = _M0L6_2atmpS2011;
    moonbit_decref(_M0L4dataS2009);
    _M0L3lenS2016 = _M0L4selfS141->$1;
    _M0L6_2atmpS2015 = _M0L3lenS2016 + 2;
    _M0L4selfS141->$1 = _M0L6_2atmpS2015;
  } else {
    #line 108 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_104.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS133,
  int32_t _M0L8requiredS134
) {
  uint16_t* _M0L4dataS1994;
  int32_t _M0L12current__lenS132;
  int32_t _M0L13enough__spaceS135;
  int32_t _M0L13enough__spaceS136;
  uint16_t* _M0L4dataS1990;
  int32_t _M0L6_2atmpS1991;
  int32_t _M0L3lenS1992;
  uint16_t* _M0L9new__dataS138;
  uint16_t* _M0L6_2aoldS3790;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS1994 = _M0L4selfS133->$0;
  _M0L12current__lenS132 = Moonbit_array_length(_M0L4dataS1994);
  if (_M0L8requiredS134 <= _M0L12current__lenS132) {
    return 0;
  }
  _M0L13enough__spaceS136 = _M0L12current__lenS132;
  while (1) {
    if (_M0L13enough__spaceS136 < _M0L8requiredS134) {
      int32_t _M0L6_2atmpS1993 = _M0L13enough__spaceS136 * 2;
      _M0L13enough__spaceS136 = _M0L6_2atmpS1993;
      continue;
    } else {
      _M0L13enough__spaceS135 = _M0L13enough__spaceS136;
    }
    break;
  }
  _M0L4dataS1990 = _M0L4selfS133->$0;
  moonbit_incref(_M0L4dataS1990);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1991 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS1992 = _M0L4selfS133->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS138
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS1990, _M0L13enough__spaceS135, _M0L6_2atmpS1991, _M0L3lenS1992, 0, 0);
  moonbit_decref(_M0L4dataS1990);
  _M0L6_2aoldS3790 = _M0L4selfS133->$0;
  moonbit_decref(_M0L6_2aoldS3790);
  _M0L4selfS133->$0 = _M0L9new__dataS138;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS131) {
  int32_t _M0L6_2atmpS1989;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1989 = *(int32_t*)&_M0L4selfS131;
  return (uint16_t)_M0L6_2atmpS1989;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS130) {
  int32_t _M0L6_2atmpS1988;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1988 = _M0L4selfS130;
  return *(uint32_t*)&_M0L6_2atmpS1988;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS128
) {
  int32_t _M0L3lenS1980;
  uint16_t* _M0L4dataS1982;
  int32_t _M0L6_2atmpS1981;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1980 = _M0L4selfS128->$1;
  _M0L4dataS1982 = _M0L4selfS128->$0;
  _M0L6_2atmpS1981 = Moonbit_array_length(_M0L4dataS1982);
  if (_M0L3lenS1980 == _M0L6_2atmpS1981) {
    uint16_t* _M0L4dataS1983 = _M0L4selfS128->$0;
    moonbit_incref(_M0L4dataS1983);
    return _M0L4dataS1983;
  } else {
    uint16_t* _M0L4dataS1984 = _M0L4selfS128->$0;
    int32_t _M0L3lenS1985 = _M0L4selfS128->$1;
    int32_t _M0L6_2atmpS1986;
    int32_t _M0L3lenS1987;
    uint16_t* _M0L4dataS129;
    moonbit_incref(_M0L4dataS1984);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1986 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS1987 = _M0L4selfS128->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS129
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS1984, _M0L3lenS1985, _M0L6_2atmpS1986, _M0L3lenS1987, 0, 0);
    moonbit_decref(_M0L4dataS1984);
    return _M0L4dataS129;
  }
}

int32_t _M0IPC16uint166UInt16PB7Default7default() {
  #line 176 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  return 0;
}

uint16_t* _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(
  uint16_t* _M0L3srcS125,
  int32_t _M0L13allocate__lenS121,
  int32_t _M0L4initS126,
  int32_t _M0L3lenS122,
  int32_t _M0L11src__offsetS123,
  int32_t _M0L11dst__offsetS124
) {
  int32_t _if__result_4119;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS121 >= 0) {
    if (_M0L3lenS122 >= 0) {
      if (_M0L11src__offsetS123 >= 0) {
        if (_M0L11dst__offsetS124 >= 0) {
          int32_t _M0L6_2atmpS1976 = _M0L11src__offsetS123 + _M0L3lenS122;
          int32_t _M0L6_2atmpS1977 = Moonbit_array_length(_M0L3srcS125);
          if (_M0L6_2atmpS1976 <= _M0L6_2atmpS1977) {
            int32_t _M0L6_2atmpS1975 = _M0L11dst__offsetS124 + _M0L3lenS122;
            _if__result_4119 = _M0L6_2atmpS1975 <= _M0L13allocate__lenS121;
          } else {
            _if__result_4119 = 0;
          }
        } else {
          _if__result_4119 = 0;
        }
      } else {
        _if__result_4119 = 0;
      }
    } else {
      _if__result_4119 = 0;
    }
  } else {
    _if__result_4119 = 0;
  }
  if (_if__result_4119) {
    moonbit_incref(_M0L3srcS125);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS125, _M0L13allocate__lenS121, _M0L4initS126, _M0L11src__offsetS123, _M0L11dst__offsetS124, _M0L3lenS122);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS127;
    int32_t _M0L6_2atmpS1979;
    moonbit_string_t _M0L6_2atmpS1978;
    uint16_t* _result_4120;
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L18_2astring__builderS127
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_105.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L13allocate__lenS121);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_106.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11src__offsetS123);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_107.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11dst__offsetS124);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_108.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L3lenS122);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_109.data);
    _M0L6_2atmpS1979 = Moonbit_array_length(_M0L3srcS125);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L6_2atmpS1979);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS1978
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS127);
    moonbit_decref(_M0L18_2astring__builderS127);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_4120 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS1978);
    moonbit_decref(_M0L6_2atmpS1978);
    return _result_4120;
  }
}

uint16_t* _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(
  uint16_t* _M0L3srcS118,
  int32_t _M0L13allocate__lenS115,
  int32_t _M0L4initS116,
  int32_t _M0L11src__offsetS119,
  int32_t _M0L11dst__offsetS117,
  int32_t _M0L9blit__lenS120
) {
  uint16_t* _M0L3dstS114;
  #line 79 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  _M0L3dstS114
  = (uint16_t*)moonbit_make_string(_M0L13allocate__lenS115, _M0L4initS116);
  moonbit_incref(_M0L3dstS114);
  #line 90 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  moonbit_unsafe_val_array_blit(_M0L3dstS114, _M0L11dst__offsetS117, _M0L3srcS118, _M0L11src__offsetS119, _M0L9blit__lenS120, sizeof(uint16_t));
  return _M0L3dstS114;
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder21StringBuilder_2einner(
  int32_t _M0L10size__hintS112
) {
  int32_t _M0L7initialS111;
  uint16_t* _M0L4dataS113;
  struct _M0TPB13StringBuilder* _block_4121;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS112 < 1) {
    _M0L7initialS111 = 1;
  } else {
    int32_t _M0L6_2atmpS1974 = _M0L10size__hintS112 + 1;
    _M0L7initialS111 = _M0L6_2atmpS1974 / 2;
  }
  _M0L4dataS113 = (uint16_t*)moonbit_make_string(_M0L7initialS111, 0);
  _block_4121
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4121)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 128, 0);
  _block_4121->$0 = _M0L4dataS113;
  _block_4121->$1 = 0;
  return _block_4121;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS97,
  int32_t _M0L13allocate__lenS93,
  int32_t _M0L3lenS94,
  int32_t _M0L11src__offsetS95,
  int32_t _M0L11dst__offsetS96
) {
  int32_t _if__result_4122;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS93 >= 0) {
    if (_M0L3lenS94 >= 0) {
      if (_M0L11src__offsetS95 >= 0) {
        if (_M0L11dst__offsetS96 >= 0) {
          int32_t _M0L6_2atmpS1960 = _M0L11src__offsetS95 + _M0L3lenS94;
          int32_t _M0L6_2atmpS1961;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS1961
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
          if (_M0L6_2atmpS1960 <= _M0L6_2atmpS1961) {
            int32_t _M0L6_2atmpS1959 = _M0L11dst__offsetS96 + _M0L3lenS94;
            _if__result_4122 = _M0L6_2atmpS1959 <= _M0L13allocate__lenS93;
          } else {
            _if__result_4122 = 0;
          }
        } else {
          _if__result_4122 = 0;
        }
      } else {
        _if__result_4122 = 0;
      }
    } else {
      _if__result_4122 = 0;
    }
  } else {
    _if__result_4122 = 0;
  }
  if (_if__result_4122) {
    moonbit_incref(_M0L3srcS97);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS93, (moonbit_string_t)moonbit_string_literal_95.data, _M0L3srcS97, _M0L11src__offsetS95, _M0L11dst__offsetS96, _M0L3lenS94);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS98;
    int32_t _M0L6_2atmpS1963;
    moonbit_string_t _M0L6_2atmpS1962;
    moonbit_string_t* _result_4123;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS98
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_105.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L13allocate__lenS93);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_106.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11src__offsetS95);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_107.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11dst__offsetS96);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_108.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L3lenS94);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1963 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L6_2atmpS1963);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1962
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS98);
    moonbit_decref(_M0L18_2astring__builderS98);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4123
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS1962);
    moonbit_decref(_M0L6_2atmpS1962);
    return _result_4123;
  }
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t* _M0L3srcS103,
  int32_t _M0L13allocate__lenS99,
  int32_t _M0L3lenS100,
  int32_t _M0L11src__offsetS101,
  int32_t _M0L11dst__offsetS102
) {
  int32_t _if__result_4124;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS99 >= 0) {
    if (_M0L3lenS100 >= 0) {
      if (_M0L11src__offsetS101 >= 0) {
        if (_M0L11dst__offsetS102 >= 0) {
          int32_t _M0L6_2atmpS1965 = _M0L11src__offsetS101 + _M0L3lenS100;
          int32_t _M0L6_2atmpS1966;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS1966
          = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
          if (_M0L6_2atmpS1965 <= _M0L6_2atmpS1966) {
            int32_t _M0L6_2atmpS1964 = _M0L11dst__offsetS102 + _M0L3lenS100;
            _if__result_4124 = _M0L6_2atmpS1964 <= _M0L13allocate__lenS99;
          } else {
            _if__result_4124 = 0;
          }
        } else {
          _if__result_4124 = 0;
        }
      } else {
        _if__result_4124 = 0;
      }
    } else {
      _if__result_4124 = 0;
    }
  } else {
    _if__result_4124 = 0;
  }
  if (_if__result_4124) {
    moonbit_incref(_M0L3srcS103);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS99, 0, _M0L3srcS103, _M0L11src__offsetS101, _M0L11dst__offsetS102, _M0L3lenS100);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS104;
    int32_t _M0L6_2atmpS1968;
    moonbit_string_t _M0L6_2atmpS1967;
    moonbit_string_t* _result_4125;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS104
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_105.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L13allocate__lenS99);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_106.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11src__offsetS101);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_107.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11dst__offsetS102);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_108.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L3lenS100);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1968 = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L6_2atmpS1968);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1967
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS104);
    moonbit_decref(_M0L18_2astring__builderS104);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4125
    = _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(_M0L6_2atmpS1967);
    moonbit_decref(_M0L6_2atmpS1967);
    return _result_4125;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS109,
  int32_t _M0L13allocate__lenS105,
  int32_t _M0L3lenS106,
  int32_t _M0L11src__offsetS107,
  int32_t _M0L11dst__offsetS108
) {
  int32_t _if__result_4126;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS105 >= 0) {
    if (_M0L3lenS106 >= 0) {
      if (_M0L11src__offsetS107 >= 0) {
        if (_M0L11dst__offsetS108 >= 0) {
          int32_t _M0L6_2atmpS1970 = _M0L11src__offsetS107 + _M0L3lenS106;
          int32_t _M0L6_2atmpS1971;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS1971
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
          if (_M0L6_2atmpS1970 <= _M0L6_2atmpS1971) {
            int32_t _M0L6_2atmpS1969 = _M0L11dst__offsetS108 + _M0L3lenS106;
            _if__result_4126 = _M0L6_2atmpS1969 <= _M0L13allocate__lenS105;
          } else {
            _if__result_4126 = 0;
          }
        } else {
          _if__result_4126 = 0;
        }
      } else {
        _if__result_4126 = 0;
      }
    } else {
      _if__result_4126 = 0;
    }
  } else {
    _if__result_4126 = 0;
  }
  if (_if__result_4126) {
    moonbit_incref(_M0L3srcS109);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS105, 0, _M0L3srcS109, _M0L11src__offsetS107, _M0L11dst__offsetS108, _M0L3lenS106);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS110;
    int32_t _M0L6_2atmpS1973;
    moonbit_string_t _M0L6_2atmpS1972;
    struct _M0TUsfE** _result_4127;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS110
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_105.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L13allocate__lenS105);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_106.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11src__offsetS107);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_107.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11dst__offsetS108);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_108.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L3lenS106);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1973 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L6_2atmpS1973);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1972
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS110);
    moonbit_decref(_M0L18_2astring__builderS110);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4127
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS1972);
    moonbit_decref(_M0L6_2atmpS1972);
    return _result_4127;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS84,
  moonbit_string_t _M0L3objS83
) {
  struct _M0TPB6Logger _M0L6_2atmpS1954;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS84);
  _M0L6_2atmpS1954
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS84
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS83, _M0L6_2atmpS1954);
  if (_M0L6_2atmpS1954.$1) {
    moonbit_decref(_M0L6_2atmpS1954.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS86,
  int32_t _M0L3objS85
) {
  struct _M0TPB6Logger _M0L6_2atmpS1955;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS86);
  _M0L6_2atmpS1955
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS86
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS85, _M0L6_2atmpS1955);
  if (_M0L6_2atmpS1955.$1) {
    moonbit_decref(_M0L6_2atmpS1955.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder* _M0L4selfS88,
  int32_t _M0L3objS87
) {
  struct _M0TPB6Logger _M0L6_2atmpS1956;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS88);
  _M0L6_2atmpS1956
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS88
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGbE(_M0L3objS87, _M0L6_2atmpS1956);
  if (_M0L6_2atmpS1956.$1) {
    moonbit_decref(_M0L6_2atmpS1956.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS90,
  float _M0L3objS89
) {
  struct _M0TPB6Logger _M0L6_2atmpS1957;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS90);
  _M0L6_2atmpS1957
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS90
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS89, _M0L6_2atmpS1957);
  if (_M0L6_2atmpS1957.$1) {
    moonbit_decref(_M0L6_2atmpS1957.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS92,
  uint64_t _M0L3objS91
) {
  struct _M0TPB6Logger _M0L6_2atmpS1958;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS92);
  _M0L6_2atmpS1958
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS92
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS91, _M0L6_2atmpS1958);
  if (_M0L6_2atmpS1958.$1) {
    moonbit_decref(_M0L6_2atmpS1958.$1);
  }
  return 0;
}

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGsE(
  moonbit_string_t* _M0L3srcS68,
  int32_t _M0L13allocate__lenS66,
  int32_t _M0L11src__offsetS69,
  int32_t _M0L11dst__offsetS67,
  int32_t _M0L9blit__lenS70
) {
  moonbit_string_t* _M0L3dstS65;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS65
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS66, (moonbit_string_t)moonbit_string_literal_95.data);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L3dstS65, _M0L11dst__offsetS67, _M0L3srcS68, _M0L11src__offsetS69, _M0L9blit__lenS70);
  moonbit_decref(_M0L3srcS68);
  return _M0L3dstS65;
}

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGOsE(
  moonbit_string_t* _M0L3srcS74,
  int32_t _M0L13allocate__lenS72,
  int32_t _M0L11src__offsetS75,
  int32_t _M0L11dst__offsetS73,
  int32_t _M0L9blit__lenS76
) {
  moonbit_string_t* _M0L3dstS71;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS71
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS72, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGOsE(_M0L3dstS71, _M0L11dst__offsetS73, _M0L3srcS74, _M0L11src__offsetS75, _M0L9blit__lenS76);
  moonbit_decref(_M0L3srcS74);
  return _M0L3dstS71;
}

struct _M0TUsfE** _M0MPB18UninitializedArray23unsafe__make__and__blitGUsfEE(
  struct _M0TUsfE** _M0L3srcS80,
  int32_t _M0L13allocate__lenS78,
  int32_t _M0L11src__offsetS81,
  int32_t _M0L11dst__offsetS79,
  int32_t _M0L9blit__lenS82
) {
  struct _M0TUsfE** _M0L3dstS77;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS77
  = (struct _M0TUsfE**)moonbit_make_ref_array(_M0L13allocate__lenS78, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsfEE(_M0L3dstS77, _M0L11dst__offsetS79, _M0L3srcS80, _M0L11src__offsetS81, _M0L9blit__lenS82);
  moonbit_decref(_M0L3srcS80);
  return _M0L3dstS77;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS50,
  int32_t _M0L11dst__offsetS51,
  moonbit_string_t* _M0L3srcS52,
  int32_t _M0L11src__offsetS53,
  int32_t _M0L3lenS54
) {
  #line 119 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_incref(_M0L3srcS52);
  moonbit_incref(_M0L3dstS50);
  #line 128 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_unsafe_ref_array_blit(_M0L3dstS50, _M0L11dst__offsetS51, _M0L3srcS52, _M0L11src__offsetS53, _M0L3lenS54);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGOsE(
  moonbit_string_t* _M0L3dstS55,
  int32_t _M0L11dst__offsetS56,
  moonbit_string_t* _M0L3srcS57,
  int32_t _M0L11src__offsetS58,
  int32_t _M0L3lenS59
) {
  #line 119 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_incref(_M0L3srcS57);
  moonbit_incref(_M0L3dstS55);
  #line 128 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_unsafe_ref_array_blit(_M0L3dstS55, _M0L11dst__offsetS56, _M0L3srcS57, _M0L11src__offsetS58, _M0L3lenS59);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsfEE(
  struct _M0TUsfE** _M0L3dstS60,
  int32_t _M0L11dst__offsetS61,
  struct _M0TUsfE** _M0L3srcS62,
  int32_t _M0L11src__offsetS63,
  int32_t _M0L3lenS64
) {
  #line 119 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_incref(_M0L3srcS62);
  moonbit_incref(_M0L3dstS60);
  #line 128 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_unsafe_ref_array_blit(_M0L3dstS60, _M0L11dst__offsetS61, _M0L3srcS62, _M0L11src__offsetS63, _M0L3lenS64);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t* _M0L3dstS14,
  int32_t _M0L11dst__offsetS16,
  uint16_t* _M0L3srcS15,
  int32_t _M0L11src__offsetS17,
  int32_t _M0L3lenS19
) {
  int32_t _if__result_4128;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS14 == _M0L3srcS15) {
    _if__result_4128 = _M0L11dst__offsetS16 < _M0L11src__offsetS17;
  } else {
    _if__result_4128 = 0;
  }
  if (_if__result_4128) {
    int32_t _M0L1iS18 = 0;
    while (1) {
      if (_M0L1iS18 < _M0L3lenS19) {
        int32_t _M0L6_2atmpS1918 = _M0L11dst__offsetS16 + _M0L1iS18;
        int32_t _M0L6_2atmpS1920 = _M0L11src__offsetS17 + _M0L1iS18;
        int32_t _M0L6_2atmpS1919;
        int32_t _M0L6_2atmpS1921;
        if (
          _M0L6_2atmpS1920 < 0
          || _M0L6_2atmpS1920 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1919 = (int32_t)_M0L3srcS15[_M0L6_2atmpS1920];
        if (
          _M0L6_2atmpS1918 < 0
          || _M0L6_2atmpS1918 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS1918] = _M0L6_2atmpS1919;
        _M0L6_2atmpS1921 = _M0L1iS18 + 1;
        _M0L1iS18 = _M0L6_2atmpS1921;
        continue;
      } else {
        moonbit_decref(_M0L3srcS15);
        moonbit_decref(_M0L3dstS14);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1926 = _M0L3lenS19 - 1;
    int32_t _M0L1iS21 = _M0L6_2atmpS1926;
    while (1) {
      if (_M0L1iS21 >= 0) {
        int32_t _M0L6_2atmpS1922 = _M0L11dst__offsetS16 + _M0L1iS21;
        int32_t _M0L6_2atmpS1924 = _M0L11src__offsetS17 + _M0L1iS21;
        int32_t _M0L6_2atmpS1923;
        int32_t _M0L6_2atmpS1925;
        if (
          _M0L6_2atmpS1924 < 0
          || _M0L6_2atmpS1924 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1923 = (int32_t)_M0L3srcS15[_M0L6_2atmpS1924];
        if (
          _M0L6_2atmpS1922 < 0
          || _M0L6_2atmpS1922 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS1922] = _M0L6_2atmpS1923;
        _M0L6_2atmpS1925 = _M0L1iS21 - 1;
        _M0L1iS21 = _M0L6_2atmpS1925;
        continue;
      } else {
        moonbit_decref(_M0L3srcS15);
        moonbit_decref(_M0L3dstS14);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS23,
  int32_t _M0L11dst__offsetS25,
  moonbit_string_t* _M0L3srcS24,
  int32_t _M0L11src__offsetS26,
  int32_t _M0L3lenS28
) {
  int32_t _if__result_4131;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_4131 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_4131 = 0;
  }
  if (_if__result_4131) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS1927 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS1929 = _M0L11src__offsetS26 + _M0L1iS27;
        moonbit_string_t _M0L6_2atmpS1928;
        moonbit_string_t _M0L6_2aoldS3796;
        int32_t _M0L6_2atmpS1930;
        if (
          _M0L6_2atmpS1929 < 0
          || _M0L6_2atmpS1929 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1928 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS1929];
        if (
          _M0L6_2atmpS1927 < 0
          || _M0L6_2atmpS1927 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3796 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS1927];
        moonbit_incref(_M0L6_2atmpS1928);
        moonbit_decref(_M0L6_2aoldS3796);
        _M0L3dstS23[_M0L6_2atmpS1927] = _M0L6_2atmpS1928;
        _M0L6_2atmpS1930 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS1930;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1935 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS1935;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS1931 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS1933 = _M0L11src__offsetS26 + _M0L1iS30;
        moonbit_string_t _M0L6_2atmpS1932;
        moonbit_string_t _M0L6_2aoldS3798;
        int32_t _M0L6_2atmpS1934;
        if (
          _M0L6_2atmpS1933 < 0
          || _M0L6_2atmpS1933 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1932 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS1933];
        if (
          _M0L6_2atmpS1931 < 0
          || _M0L6_2atmpS1931 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3798 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS1931];
        moonbit_incref(_M0L6_2atmpS1932);
        moonbit_decref(_M0L6_2aoldS3798);
        _M0L3dstS23[_M0L6_2atmpS1931] = _M0L6_2atmpS1932;
        _M0L6_2atmpS1934 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS1934;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGOsEE(
  moonbit_string_t* _M0L3dstS32,
  int32_t _M0L11dst__offsetS34,
  moonbit_string_t* _M0L3srcS33,
  int32_t _M0L11src__offsetS35,
  int32_t _M0L3lenS37
) {
  int32_t _if__result_4134;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_4134 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_4134 = 0;
  }
  if (_if__result_4134) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS1936 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS1938 = _M0L11src__offsetS35 + _M0L1iS36;
        moonbit_string_t _M0L6_2atmpS1937;
        moonbit_string_t _M0L6_2aoldS3800;
        int32_t _M0L6_2atmpS1939;
        if (
          _M0L6_2atmpS1938 < 0
          || _M0L6_2atmpS1938 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1937 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1938];
        if (
          _M0L6_2atmpS1936 < 0
          || _M0L6_2atmpS1936 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3800 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1936];
        if (_M0L6_2atmpS1937) {
          moonbit_incref(_M0L6_2atmpS1937);
        }
        if (_M0L6_2aoldS3800) {
          moonbit_decref(_M0L6_2aoldS3800);
        }
        _M0L3dstS32[_M0L6_2atmpS1936] = _M0L6_2atmpS1937;
        _M0L6_2atmpS1939 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS1939;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1944 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS1944;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS1940 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS1942 = _M0L11src__offsetS35 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS1941;
        moonbit_string_t _M0L6_2aoldS3802;
        int32_t _M0L6_2atmpS1943;
        if (
          _M0L6_2atmpS1942 < 0
          || _M0L6_2atmpS1942 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1941 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1942];
        if (
          _M0L6_2atmpS1940 < 0
          || _M0L6_2atmpS1940 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3802 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1940];
        if (_M0L6_2atmpS1941) {
          moonbit_incref(_M0L6_2atmpS1941);
        }
        if (_M0L6_2aoldS3802) {
          moonbit_decref(_M0L6_2aoldS3802);
        }
        _M0L3dstS32[_M0L6_2atmpS1940] = _M0L6_2atmpS1941;
        _M0L6_2atmpS1943 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS1943;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsfEEE(
  struct _M0TUsfE** _M0L3dstS41,
  int32_t _M0L11dst__offsetS43,
  struct _M0TUsfE** _M0L3srcS42,
  int32_t _M0L11src__offsetS44,
  int32_t _M0L3lenS46
) {
  int32_t _if__result_4137;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_4137 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_4137 = 0;
  }
  if (_if__result_4137) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS1945 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS1947 = _M0L11src__offsetS44 + _M0L1iS45;
        struct _M0TUsfE* _M0L6_2atmpS1946;
        struct _M0TUsfE* _M0L6_2aoldS3804;
        int32_t _M0L6_2atmpS1948;
        if (
          _M0L6_2atmpS1947 < 0
          || _M0L6_2atmpS1947 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1946 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS1947];
        if (
          _M0L6_2atmpS1945 < 0
          || _M0L6_2atmpS1945 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3804 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS1945];
        if (_M0L6_2atmpS1946) {
          moonbit_incref(_M0L6_2atmpS1946);
        }
        if (_M0L6_2aoldS3804) {
          moonbit_decref(_M0L6_2aoldS3804);
        }
        _M0L3dstS41[_M0L6_2atmpS1945] = _M0L6_2atmpS1946;
        _M0L6_2atmpS1948 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS1948;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1953 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS1953;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS1949 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS1951 = _M0L11src__offsetS44 + _M0L1iS48;
        struct _M0TUsfE* _M0L6_2atmpS1950;
        struct _M0TUsfE* _M0L6_2aoldS3806;
        int32_t _M0L6_2atmpS1952;
        if (
          _M0L6_2atmpS1951 < 0
          || _M0L6_2atmpS1951 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1950 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS1951];
        if (
          _M0L6_2atmpS1949 < 0
          || _M0L6_2atmpS1949 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3806 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS1949];
        if (_M0L6_2atmpS1950) {
          moonbit_incref(_M0L6_2atmpS1950);
        }
        if (_M0L6_2aoldS3806) {
          moonbit_decref(_M0L6_2aoldS3806);
        }
        _M0L3dstS41[_M0L6_2atmpS1949] = _M0L6_2atmpS1950;
        _M0L6_2atmpS1952 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS1952;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPB18UninitializedArray6lengthGsE(moonbit_string_t* _M0L4selfS11) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS11);
}

int32_t _M0MPB18UninitializedArray6lengthGOsE(moonbit_string_t* _M0L4selfS12) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS12);
}

int32_t _M0MPB18UninitializedArray6lengthGUsfEE(
  struct _M0TUsfE** _M0L4selfS13
) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS13);
}

uint32_t _M0FPB13consume4__acc(uint32_t _M0L3accS9, uint32_t _M0L5inputS10) {
  uint32_t _M0L6_2atmpS1917;
  uint32_t _M0L6_2atmpS1916;
  uint32_t _M0L6_2atmpS1915;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1917 = _M0L5inputS10 * 3266489917u;
  _M0L6_2atmpS1916 = _M0L3accS9 + _M0L6_2atmpS1917;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1915 = _M0FPB4rotl(_M0L6_2atmpS1916, 17);
  return _M0L6_2atmpS1915 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS7, int32_t _M0L1rS8) {
  uint32_t _M0L6_2atmpS1912;
  int32_t _M0L6_2atmpS1914;
  uint32_t _M0L6_2atmpS1913;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1912 = _M0L1xS7 << (_M0L1rS8 & 31);
  _M0L6_2atmpS1914 = 32 - _M0L1rS8;
  _M0L6_2atmpS1913 = _M0L1xS7 >> (_M0L6_2atmpS1914 & 31);
  return _M0L6_2atmpS1912 | _M0L6_2atmpS1913;
}

int32_t _M0FPC15abort5abortGuE(moonbit_string_t _M0L3msgS1) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS1);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
  return 0;
}

uint16_t* _M0FPC15abort5abortGAkE(moonbit_string_t _M0L3msgS2) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS2);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS3);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS4);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

struct _M0TUsfE** _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(
  moonbit_string_t _M0L3msgS5
) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS5);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS6) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS6);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

int32_t _M0IP016_24default__implPB6Logger61write_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1798,
  struct _M0TPB4Show _M0L8_2aparamS1797
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1796 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1798;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS1796, _M0L8_2aparamS1797);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1795,
  struct _M0TPB4Show _M0L8_2aparamS1794
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1793 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1795;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS1793, _M0L8_2aparamS1794);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1792,
  int32_t _M0L8_2aparamS1791
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1790 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1792;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1790, _M0L8_2aparamS1791);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1789,
  struct _M0TPC16string10StringView _M0L8_2aparamS1788
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1787 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1789;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1787, _M0L8_2aparamS1788);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1786,
  moonbit_string_t _M0L8_2aparamS1783,
  int32_t _M0L8_2aparamS1784,
  int32_t _M0L8_2aparamS1785
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1782 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1786;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1782, _M0L8_2aparamS1783, _M0L8_2aparamS1784, _M0L8_2aparamS1785);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1781,
  moonbit_string_t _M0L8_2aparamS1780
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1779 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1781;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1779, _M0L8_2aparamS1780);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L2dbS1718;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1719;
  moonbit_string_t _M0L6_2atmpS1801;
  moonbit_string_t _M0L6_2atmpS1800;
  moonbit_string_t _M0L6_2atmpS1799;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1720;
  int32_t _M0L6_2atmpS1803;
  moonbit_string_t _M0L6_2atmpS1802;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1721;
  int64_t _M0L6_2atmpS1806;
  moonbit_string_t _M0L6_2atmpS1805;
  moonbit_string_t _M0L6_2atmpS1804;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1722;
  int64_t _M0L6_2atmpS1809;
  moonbit_string_t _M0L6_2atmpS1808;
  moonbit_string_t _M0L6_2atmpS1807;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1723;
  int64_t _M0L6_2atmpS1812;
  moonbit_string_t _M0L6_2atmpS1811;
  moonbit_string_t _M0L6_2atmpS1810;
  int32_t _M0L6_2atmpS1813;
  int32_t _M0L6_2atmpS1814;
  int32_t _M0L6_2atmpS1815;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1724;
  moonbit_string_t _M0L6_2atmpS1818;
  moonbit_string_t _M0L6_2atmpS1817;
  moonbit_string_t _M0L6_2atmpS1816;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1725;
  int32_t _M0L6_2atmpS1820;
  moonbit_string_t _M0L6_2atmpS1819;
  moonbit_string_t _M0L11user__emailS1726;
  int32_t _M0L6_2atmpS1911;
  int32_t _M0L10has__emailS1727;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1728;
  moonbit_string_t _M0L6_2atmpS1821;
  struct _M0TPB3MapGssE* _M0L11all__fieldsS1729;
  moonbit_string_t* _M0L6_2atmpS1910;
  struct _M0TPB5ArrayGsE* _M0L11field__listS1730;
  struct _M0TPB4IterGUssEE* _M0L5_2aitS1731;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1739;
  moonbit_string_t _M0L7_2abindS1740;
  int32_t _M0L6_2atmpS1825;
  struct _M0TPC16string10StringView _M0L6_2atmpS1824;
  moonbit_string_t _M0L6_2atmpS1823;
  moonbit_string_t _M0L6_2atmpS1822;
  int32_t _M0L6_2atmpS1826;
  int32_t _M0L6_2atmpS1827;
  int32_t _M0L6_2atmpS1828;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1741;
  int32_t _M0L6_2atmpS1830;
  moonbit_string_t _M0L6_2atmpS1829;
  struct _M0TPB5ArrayGsE* _M0L10task__listS1742;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1743;
  moonbit_string_t _M0L7_2abindS1744;
  int32_t _M0L6_2atmpS1834;
  struct _M0TPC16string10StringView _M0L6_2atmpS1833;
  moonbit_string_t _M0L6_2atmpS1832;
  moonbit_string_t _M0L6_2atmpS1831;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1745;
  moonbit_string_t _M0L6_2atmpS1837;
  moonbit_string_t _M0L6_2atmpS1836;
  moonbit_string_t _M0L6_2atmpS1835;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1746;
  moonbit_string_t _M0L6_2atmpS1840;
  moonbit_string_t _M0L6_2atmpS1839;
  moonbit_string_t _M0L6_2atmpS1838;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1747;
  int32_t _M0L6_2atmpS1842;
  moonbit_string_t _M0L6_2atmpS1841;
  int32_t _M0L6_2atmpS1843;
  int32_t _M0L6_2atmpS1844;
  int32_t _M0L6_2atmpS1845;
  int32_t _M0L6_2atmpS1846;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1748;
  int32_t _M0L6_2atmpS1848;
  moonbit_string_t _M0L6_2atmpS1847;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1749;
  int32_t _M0L6_2atmpS1850;
  moonbit_string_t _M0L6_2atmpS1849;
  struct _M0TPB5ArrayGsE* _M0L4tagsS1750;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1751;
  moonbit_string_t _M0L7_2abindS1752;
  int32_t _M0L6_2atmpS1854;
  struct _M0TPC16string10StringView _M0L6_2atmpS1853;
  moonbit_string_t _M0L6_2atmpS1852;
  moonbit_string_t _M0L6_2atmpS1851;
  int32_t _M0L6_2atmpS1855;
  int32_t _M0L6_2atmpS1856;
  int32_t _M0L6_2atmpS1857;
  int32_t _M0L6_2atmpS1858;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1753;
  int32_t _M0L6_2atmpS1860;
  moonbit_string_t _M0L6_2atmpS1859;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1754;
  void* _M0L6_2atmpS1863;
  moonbit_string_t _M0L6_2atmpS1862;
  moonbit_string_t _M0L6_2atmpS1861;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1755;
  int64_t _M0L6_2atmpS1866;
  moonbit_string_t _M0L6_2atmpS1865;
  moonbit_string_t _M0L6_2atmpS1864;
  struct _M0TPB5ArrayGsE* _M0L4top3S1756;
  int32_t _M0L1iS1757;
  int32_t _M0L6_2atmpS1873;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1762;
  int32_t _M0L6_2atmpS1875;
  moonbit_string_t _M0L6_2atmpS1874;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1763;
  int32_t _M0L6_2atmpS1877;
  moonbit_string_t _M0L6_2atmpS1876;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1764;
  int32_t _M0L6_2atmpS1879;
  moonbit_string_t _M0L6_2atmpS1878;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1765;
  int32_t _M0L6_2atmpS1881;
  moonbit_string_t _M0L6_2atmpS1880;
  moonbit_string_t* _M0L6_2atmpS1886;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1883;
  moonbit_string_t* _M0L6_2atmpS1885;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1884;
  int32_t _M0L6_2atmpS1882;
  moonbit_string_t* _M0L6_2atmpS1909;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1908;
  struct _M0TPB5ArrayGOsE* _M0L12config__valsS1766;
  moonbit_string_t* _M0L6_2atmpS1907;
  struct _M0TPB5ArrayGsE* _M0L4keysS1767;
  int32_t _M0L1iS1768;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1771;
  int32_t _M0L6_2atmpS1894;
  moonbit_string_t _M0L6_2atmpS1893;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1772;
  moonbit_string_t _M0L6_2atmpS1896;
  moonbit_string_t _M0L6_2atmpS1895;
  moonbit_string_t _M0L2rkS1773;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1774;
  moonbit_string_t _M0L6_2atmpS1898;
  moonbit_string_t _M0L6_2atmpS1897;
  struct _M0TPB5ArrayGsE* _M0L9all__keysS1775;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1776;
  int32_t _M0L6_2atmpS1900;
  moonbit_string_t _M0L7_2abindS1777;
  int32_t _M0L6_2atmpS1903;
  struct _M0TPC16string10StringView _M0L6_2atmpS1902;
  moonbit_string_t _M0L6_2atmpS1901;
  moonbit_string_t _M0L6_2atmpS1899;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1778;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1906;
  int32_t _M0L6_2atmpS1905;
  moonbit_string_t _M0L6_2atmpS1904;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 23 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_110.data);
  #line 24 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_111.data);
  #line 25 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_110.data);
  #line 27 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L2dbS1718 = _M0MP38JIA2JIA29moonbitdb3lib8Database3new();
  #line 29 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_112.data);
  #line 30 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_113.data);
  #line 31 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database3set(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_114.data, (moonbit_string_t)moonbit_string_literal_115.data);
  #line 32 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  #line 33 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1719
  = _M0MPB13StringBuilder21StringBuilder_2einner(17);
  #line 33 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1719, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 33 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1801
  = _M0MP38JIA2JIA29moonbitdb3lib8Database3get(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_114.data);
  #line 33 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1800 = _M0FP28JIA2JIA29moonbitdb9show__opt(_M0L6_2atmpS1801);
  if (_M0L6_2atmpS1801) {
    moonbit_decref(_M0L6_2atmpS1801);
  }
  #line 33 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1719, _M0L6_2atmpS1800);
  moonbit_decref(_M0L6_2atmpS1800);
  #line 33 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1799
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1719);
  moonbit_decref(_M0L18_2astring__builderS1719);
  #line 33 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1799);
  moonbit_decref(_M0L6_2atmpS1799);
  #line 34 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1720
  = _M0MPB13StringBuilder21StringBuilder_2einner(20);
  #line 34 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1720, (moonbit_string_t)moonbit_string_literal_118.data);
  #line 34 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1803
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6strlen(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_114.data);
  #line 34 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1720, _M0L6_2atmpS1803);
  #line 34 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1802
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1720);
  moonbit_decref(_M0L18_2astring__builderS1720);
  #line 34 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1802);
  moonbit_decref(_M0L6_2atmpS1802);
  #line 36 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database3set(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_119.data, (moonbit_string_t)moonbit_string_literal_120.data);
  #line 37 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_121.data);
  #line 38 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1721
  = _M0MPB13StringBuilder21StringBuilder_2einner(17);
  #line 38 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1721, (moonbit_string_t)moonbit_string_literal_122.data);
  #line 38 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1806
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4incr(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 38 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1805
  = _M0FP28JIA2JIA29moonbitdb14show__opt__int(_M0L6_2atmpS1806);
  #line 38 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1721, _M0L6_2atmpS1805);
  moonbit_decref(_M0L6_2atmpS1805);
  #line 38 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1804
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1721);
  moonbit_decref(_M0L18_2astring__builderS1721);
  #line 38 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1804);
  moonbit_decref(_M0L6_2atmpS1804);
  #line 39 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1722
  = _M0MPB13StringBuilder21StringBuilder_2einner(17);
  #line 39 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1722, (moonbit_string_t)moonbit_string_literal_122.data);
  #line 39 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1809
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4incr(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 39 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1808
  = _M0FP28JIA2JIA29moonbitdb14show__opt__int(_M0L6_2atmpS1809);
  #line 39 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1722, _M0L6_2atmpS1808);
  moonbit_decref(_M0L6_2atmpS1808);
  #line 39 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1807
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1722);
  moonbit_decref(_M0L18_2astring__builderS1722);
  #line 39 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1807);
  moonbit_decref(_M0L6_2atmpS1807);
  #line 40 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1723
  = _M0MPB13StringBuilder21StringBuilder_2einner(17);
  #line 40 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1723, (moonbit_string_t)moonbit_string_literal_123.data);
  #line 40 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1812
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4decr(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 40 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1811
  = _M0FP28JIA2JIA29moonbitdb14show__opt__int(_M0L6_2atmpS1812);
  #line 40 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1723, _M0L6_2atmpS1811);
  moonbit_decref(_M0L6_2atmpS1811);
  #line 40 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1810
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1723);
  moonbit_decref(_M0L18_2astring__builderS1723);
  #line 40 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1810);
  moonbit_decref(_M0L6_2atmpS1810);
  #line 42 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  #line 43 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_113.data);
  #line 44 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1813
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_125.data, (moonbit_string_t)moonbit_string_literal_126.data, (moonbit_string_t)moonbit_string_literal_127.data);
  #line 45 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1814
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_125.data, (moonbit_string_t)moonbit_string_literal_128.data, (moonbit_string_t)moonbit_string_literal_129.data);
  #line 46 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1815
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_125.data, (moonbit_string_t)moonbit_string_literal_130.data, (moonbit_string_t)moonbit_string_literal_131.data);
  #line 47 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_132.data);
  #line 48 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1724
  = _M0MPB13StringBuilder21StringBuilder_2einner(28);
  #line 48 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1724, (moonbit_string_t)moonbit_string_literal_133.data);
  #line 48 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1818
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_125.data, (moonbit_string_t)moonbit_string_literal_126.data);
  #line 48 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1817 = _M0FP28JIA2JIA29moonbitdb9show__opt(_M0L6_2atmpS1818);
  if (_M0L6_2atmpS1818) {
    moonbit_decref(_M0L6_2atmpS1818);
  }
  #line 48 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1724, _M0L6_2atmpS1817);
  moonbit_decref(_M0L6_2atmpS1817);
  #line 48 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1816
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1724);
  moonbit_decref(_M0L18_2astring__builderS1724);
  #line 48 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1816);
  moonbit_decref(_M0L6_2atmpS1816);
  #line 49 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1725
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 49 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1725, (moonbit_string_t)moonbit_string_literal_134.data);
  #line 49 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1820
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hlen(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_125.data);
  #line 49 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1725, _M0L6_2atmpS1820);
  #line 49 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1819
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1725);
  moonbit_decref(_M0L18_2astring__builderS1725);
  #line 49 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1819);
  moonbit_decref(_M0L6_2atmpS1819);
  #line 50 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L11user__emailS1726
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_125.data, (moonbit_string_t)moonbit_string_literal_128.data);
  _M0L6_2atmpS1911 = _M0L11user__emailS1726 == 0;
  if (_M0L11user__emailS1726) {
    moonbit_decref(_M0L11user__emailS1726);
  }
  _M0L10has__emailS1727 = !_M0L6_2atmpS1911;
  #line 55 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1728
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 55 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1728, (moonbit_string_t)moonbit_string_literal_135.data);
  #line 55 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1728, _M0L10has__emailS1727);
  #line 55 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1821
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1728);
  moonbit_decref(_M0L18_2astring__builderS1728);
  #line 55 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1821);
  moonbit_decref(_M0L6_2atmpS1821);
  #line 56 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L11all__fieldsS1729
  = _M0MP38JIA2JIA29moonbitdb3lib8Database7hgetall(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_125.data);
  _M0L6_2atmpS1910 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L11field__listS1730
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L11field__listS1730)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L11field__listS1730->$0 = _M0L6_2atmpS1910;
  _M0L11field__listS1730->$1 = 0;
  #line 57 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L5_2aitS1731 = _M0MPB3Map5iter2GssE(_M0L11all__fieldsS1729);
  moonbit_decref(_M0L11all__fieldsS1729);
  while (1) {
    moonbit_string_t _M0L1fS1733;
    struct _M0TUssE* _M0L7_2abindS1735;
    #line 58 "/home/developer/Documents2/moonbitDB/demo.mbt"
    _M0L7_2abindS1735 = _M0MPB5Iter24nextGssE(_M0L5_2aitS1731);
    if (_M0L7_2abindS1735 == 0) {
      if (_M0L7_2abindS1735) {
        moonbit_decref(_M0L7_2abindS1735);
      }
      moonbit_decref(_M0L5_2aitS1731);
    } else {
      struct _M0TUssE* _M0L7_2aSomeS1736 = _M0L7_2abindS1735;
      struct _M0TUssE* _M0L4_2axS1737 = _M0L7_2aSomeS1736;
      moonbit_string_t _M0L8_2afieldS3808 = _M0L4_2axS1737->$0;
      int32_t _M0L6_2acntS3872 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1737));
      moonbit_string_t _M0L4_2afS1738;
      if (_M0L6_2acntS3872 > 1) {
        int32_t _M0L11_2anew__cntS3874 = _M0L6_2acntS3872 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1737), _M0L11_2anew__cntS3874);
        moonbit_incref(_M0L8_2afieldS3808);
      } else if (_M0L6_2acntS3872 == 1) {
        moonbit_string_t _M0L8_2afieldS3873 = _M0L4_2axS1737->$1;
        moonbit_decref(_M0L8_2afieldS3873);
        #line 58 "/home/developer/Documents2/moonbitDB/demo.mbt"
        moonbit_free(_M0L4_2axS1737);
      }
      _M0L4_2afS1738 = _M0L8_2afieldS3808;
      _M0L1fS1733 = _M0L4_2afS1738;
      goto join_1732;
    }
    goto joinlet_4141;
    join_1732:;
    #line 59 "/home/developer/Documents2/moonbitDB/demo.mbt"
    _M0MPC15array5Array4pushGsE(_M0L11field__listS1730, _M0L1fS1733);
    moonbit_decref(_M0L1fS1733);
    continue;
    joinlet_4141:;
    break;
  }
  #line 61 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1739
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 61 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1739, (moonbit_string_t)moonbit_string_literal_136.data);
  _M0L7_2abindS1740 = (moonbit_string_t)moonbit_string_literal_137.data;
  _M0L6_2atmpS1825 = Moonbit_array_length(_M0L7_2abindS1740);
  _M0L6_2atmpS1824
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1740, .$1 = 0, .$2 = _M0L6_2atmpS1825
  };
  #line 61 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1823
  = _M0MPC15array5Array4joinGsE(_M0L11field__listS1730, _M0L6_2atmpS1824);
  moonbit_decref(_M0L11field__listS1730);
  moonbit_decref(_M0L6_2atmpS1824.$0);
  #line 61 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1739, _M0L6_2atmpS1823);
  moonbit_decref(_M0L6_2atmpS1823);
  #line 61 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1739, (moonbit_string_t)moonbit_string_literal_138.data);
  #line 61 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1822
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1739);
  moonbit_decref(_M0L18_2astring__builderS1739);
  #line 61 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1822);
  moonbit_decref(_M0L6_2atmpS1822);
  #line 63 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_139.data);
  #line 64 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_113.data);
  #line 65 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1826
  = _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_140.data, (moonbit_string_t)moonbit_string_literal_141.data);
  #line 66 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1827
  = _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_140.data, (moonbit_string_t)moonbit_string_literal_142.data);
  #line 67 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1828
  = _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_140.data, (moonbit_string_t)moonbit_string_literal_143.data);
  #line 68 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_144.data);
  #line 69 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1741
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 69 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1741, (moonbit_string_t)moonbit_string_literal_145.data);
  #line 69 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1830
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_140.data);
  #line 69 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1741, _M0L6_2atmpS1830);
  #line 69 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1829
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1741);
  moonbit_decref(_M0L18_2astring__builderS1741);
  #line 69 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1829);
  moonbit_decref(_M0L6_2atmpS1829);
  #line 70 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L10task__listS1742
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6lrange(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_140.data, 0, -1);
  #line 71 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1743
  = _M0MPB13StringBuilder21StringBuilder_2einner(24);
  #line 71 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1743, (moonbit_string_t)moonbit_string_literal_146.data);
  _M0L7_2abindS1744 = (moonbit_string_t)moonbit_string_literal_137.data;
  _M0L6_2atmpS1834 = Moonbit_array_length(_M0L7_2abindS1744);
  _M0L6_2atmpS1833
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1744, .$1 = 0, .$2 = _M0L6_2atmpS1834
  };
  #line 71 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1832
  = _M0MPC15array5Array4joinGsE(_M0L10task__listS1742, _M0L6_2atmpS1833);
  moonbit_decref(_M0L10task__listS1742);
  moonbit_decref(_M0L6_2atmpS1833.$0);
  #line 71 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1743, _M0L6_2atmpS1832);
  moonbit_decref(_M0L6_2atmpS1832);
  #line 71 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1743, (moonbit_string_t)moonbit_string_literal_138.data);
  #line 71 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1831
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1743);
  moonbit_decref(_M0L18_2astring__builderS1743);
  #line 71 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1831);
  moonbit_decref(_M0L6_2atmpS1831);
  #line 72 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1745
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 72 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1745, (moonbit_string_t)moonbit_string_literal_147.data);
  #line 72 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1837
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4lpop(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_140.data);
  #line 72 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1836 = _M0FP28JIA2JIA29moonbitdb9show__opt(_M0L6_2atmpS1837);
  if (_M0L6_2atmpS1837) {
    moonbit_decref(_M0L6_2atmpS1837);
  }
  #line 72 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1745, _M0L6_2atmpS1836);
  moonbit_decref(_M0L6_2atmpS1836);
  #line 72 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1835
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1745);
  moonbit_decref(_M0L18_2astring__builderS1745);
  #line 72 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1835);
  moonbit_decref(_M0L6_2atmpS1835);
  #line 73 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1746
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 73 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1746, (moonbit_string_t)moonbit_string_literal_148.data);
  #line 73 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1840
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4rpop(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_140.data);
  #line 73 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1839 = _M0FP28JIA2JIA29moonbitdb9show__opt(_M0L6_2atmpS1840);
  if (_M0L6_2atmpS1840) {
    moonbit_decref(_M0L6_2atmpS1840);
  }
  #line 73 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1746, _M0L6_2atmpS1839);
  moonbit_decref(_M0L6_2atmpS1839);
  #line 73 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1838
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1746);
  moonbit_decref(_M0L18_2astring__builderS1746);
  #line 73 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1838);
  moonbit_decref(_M0L6_2atmpS1838);
  #line 74 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1747
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 74 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1747, (moonbit_string_t)moonbit_string_literal_149.data);
  #line 74 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1842
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_140.data);
  #line 74 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1747, _M0L6_2atmpS1842);
  #line 74 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1841
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1747);
  moonbit_decref(_M0L18_2astring__builderS1747);
  #line 74 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1841);
  moonbit_decref(_M0L6_2atmpS1841);
  #line 76 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_150.data);
  #line 77 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_113.data);
  #line 78 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1843
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_152.data);
  #line 79 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1844
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_153.data);
  #line 80 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1845
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_154.data);
  #line 81 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1846
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_152.data);
  #line 82 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_155.data);
  #line 83 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1748
  = _M0MPB13StringBuilder21StringBuilder_2einner(25);
  #line 83 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1748, (moonbit_string_t)moonbit_string_literal_156.data);
  #line 83 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1848
  = _M0MP38JIA2JIA29moonbitdb3lib8Database5scard(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_151.data);
  #line 83 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1748, _M0L6_2atmpS1848);
  #line 83 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1748, (moonbit_string_t)moonbit_string_literal_157.data);
  #line 83 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1847
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1748);
  moonbit_decref(_M0L18_2astring__builderS1748);
  #line 83 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1847);
  moonbit_decref(_M0L6_2atmpS1847);
  #line 84 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1749
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 84 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1749, (moonbit_string_t)moonbit_string_literal_158.data);
  #line 84 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1850
  = _M0MP38JIA2JIA29moonbitdb3lib8Database9sismember(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_154.data);
  #line 84 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1749, _M0L6_2atmpS1850);
  #line 84 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1849
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1749);
  moonbit_decref(_M0L18_2astring__builderS1749);
  #line 84 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1849);
  moonbit_decref(_M0L6_2atmpS1849);
  #line 85 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L4tagsS1750
  = _M0MP38JIA2JIA29moonbitdb3lib8Database8smembers(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_151.data);
  #line 86 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1751
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 86 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1751, (moonbit_string_t)moonbit_string_literal_159.data);
  _M0L7_2abindS1752 = (moonbit_string_t)moonbit_string_literal_137.data;
  _M0L6_2atmpS1854 = Moonbit_array_length(_M0L7_2abindS1752);
  _M0L6_2atmpS1853
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1752, .$1 = 0, .$2 = _M0L6_2atmpS1854
  };
  #line 86 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1852
  = _M0MPC15array5Array4joinGsE(_M0L4tagsS1750, _M0L6_2atmpS1853);
  moonbit_decref(_M0L4tagsS1750);
  moonbit_decref(_M0L6_2atmpS1853.$0);
  #line 86 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1751, _M0L6_2atmpS1852);
  moonbit_decref(_M0L6_2atmpS1852);
  #line 86 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1751, (moonbit_string_t)moonbit_string_literal_138.data);
  #line 86 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1851
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1751);
  moonbit_decref(_M0L18_2astring__builderS1751);
  #line 86 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1851);
  moonbit_decref(_M0L6_2atmpS1851);
  #line 88 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_160.data);
  #line 89 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_113.data);
  #line 90 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1855
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data, 0x1.f4p+9f, (moonbit_string_t)moonbit_string_literal_162.data);
  #line 91 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1856
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data, 0x1.388p+11f, (moonbit_string_t)moonbit_string_literal_163.data);
  #line 92 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1857
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data, 0x1.c2p+10f, (moonbit_string_t)moonbit_string_literal_164.data);
  #line 93 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1858
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data, 0x1.9p+11f, (moonbit_string_t)moonbit_string_literal_165.data);
  #line 94 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_166.data);
  #line 95 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1753
  = _M0MPB13StringBuilder21StringBuilder_2einner(10);
  #line 95 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1753, (moonbit_string_t)moonbit_string_literal_167.data);
  #line 95 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1860
  = _M0MP38JIA2JIA29moonbitdb3lib8Database5zcard(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data);
  #line 95 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1753, _M0L6_2atmpS1860);
  #line 95 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1859
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1753);
  moonbit_decref(_M0L18_2astring__builderS1753);
  #line 95 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1859);
  moonbit_decref(_M0L6_2atmpS1859);
  #line 96 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1754
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 96 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1754, (moonbit_string_t)moonbit_string_literal_168.data);
  #line 96 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1863
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data, (moonbit_string_t)moonbit_string_literal_163.data);
  #line 96 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1862
  = _M0FP28JIA2JIA29moonbitdb16show__opt__float(_M0L6_2atmpS1863);
  moonbit_decref(_M0L6_2atmpS1863);
  #line 96 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1754, _M0L6_2atmpS1862);
  moonbit_decref(_M0L6_2atmpS1862);
  #line 96 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1861
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1754);
  moonbit_decref(_M0L18_2astring__builderS1754);
  #line 96 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1861);
  moonbit_decref(_M0L6_2atmpS1861);
  #line 97 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1755
  = _M0MPB13StringBuilder21StringBuilder_2einner(33);
  #line 97 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1755, (moonbit_string_t)moonbit_string_literal_169.data);
  #line 97 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1866
  = _M0MP38JIA2JIA29moonbitdb3lib8Database5zrank(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data, (moonbit_string_t)moonbit_string_literal_163.data);
  #line 97 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1865
  = _M0FP28JIA2JIA29moonbitdb14show__opt__int(_M0L6_2atmpS1866);
  #line 97 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1755, _M0L6_2atmpS1865);
  moonbit_decref(_M0L6_2atmpS1865);
  #line 97 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1755, (moonbit_string_t)moonbit_string_literal_170.data);
  #line 97 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1864
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1755);
  moonbit_decref(_M0L18_2astring__builderS1755);
  #line 97 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1864);
  moonbit_decref(_M0L6_2atmpS1864);
  #line 98 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L4top3S1756
  = _M0MP38JIA2JIA29moonbitdb3lib8Database9zrevrange(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data, 0, 2);
  #line 99 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_171.data);
  _M0L1iS1757 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1867;
    #line 100 "/home/developer/Documents2/moonbitDB/demo.mbt"
    _M0L6_2atmpS1867 = _M0MPC15array5Array6lengthGsE(_M0L4top3S1756);
    if (_M0L1iS1757 < _M0L6_2atmpS1867) {
      moonbit_string_t _M0L6_2atmpS1871;
      void* _M0L5scoreS1758;
      moonbit_string_t _M0L10score__strS1759;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1760;
      int32_t _M0L6_2atmpS1869;
      moonbit_string_t _M0L6_2atmpS1870;
      moonbit_string_t _M0L6_2atmpS1868;
      int32_t _M0L6_2atmpS1872;
      #line 101 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L6_2atmpS1871
      = _M0MPC15array5Array2atGsE(_M0L4top3S1756, _M0L1iS1757);
      #line 101 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L5scoreS1758
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_161.data, _M0L6_2atmpS1871);
      moonbit_decref(_M0L6_2atmpS1871);
      #line 102 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L10score__strS1759
      = _M0FP28JIA2JIA29moonbitdb16show__opt__float(_M0L5scoreS1758);
      moonbit_decref(_M0L5scoreS1758);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L18_2astring__builderS1760
      = _M0MPB13StringBuilder21StringBuilder_2einner(12);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1760, (moonbit_string_t)moonbit_string_literal_172.data);
      _M0L6_2atmpS1869 = _M0L1iS1757 + 1;
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1760, _M0L6_2atmpS1869);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1760, (moonbit_string_t)moonbit_string_literal_173.data);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L6_2atmpS1870
      = _M0MPC15array5Array2atGsE(_M0L4top3S1756, _M0L1iS1757);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1760, _M0L6_2atmpS1870);
      moonbit_decref(_M0L6_2atmpS1870);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1760, (moonbit_string_t)moonbit_string_literal_174.data);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1760, _M0L10score__strS1759);
      moonbit_decref(_M0L10score__strS1759);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1760, (moonbit_string_t)moonbit_string_literal_175.data);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L6_2atmpS1868
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1760);
      moonbit_decref(_M0L18_2astring__builderS1760);
      #line 103 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1868);
      moonbit_decref(_M0L6_2atmpS1868);
      _M0L6_2atmpS1872 = _M0L1iS1757 + 1;
      _M0L1iS1757 = _M0L6_2atmpS1872;
      continue;
    } else {
      moonbit_decref(_M0L4top3S1756);
    }
    break;
  }
  #line 106 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_176.data);
  #line 107 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_113.data);
  #line 108 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database3set(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_177.data, (moonbit_string_t)moonbit_string_literal_125.data);
  #line 109 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1873
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_177.data, 60);
  #line 110 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_178.data);
  #line 111 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1762
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 111 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1762, (moonbit_string_t)moonbit_string_literal_179.data);
  #line 111 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1875
  = _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_177.data);
  #line 111 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1762, _M0L6_2atmpS1875);
  #line 111 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1762, (moonbit_string_t)moonbit_string_literal_180.data);
  #line 111 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1874
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1762);
  moonbit_decref(_M0L18_2astring__builderS1762);
  #line 111 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1874);
  moonbit_decref(_M0L6_2atmpS1874);
  #line 112 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(_M0L2dbS1718, 30000);
  #line 113 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1763
  = _M0MPB13StringBuilder21StringBuilder_2einner(21);
  #line 113 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1763, (moonbit_string_t)moonbit_string_literal_181.data);
  #line 113 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1877
  = _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_177.data);
  #line 113 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1763, _M0L6_2atmpS1877);
  #line 113 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1763, (moonbit_string_t)moonbit_string_literal_180.data);
  #line 113 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1876
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1763);
  moonbit_decref(_M0L18_2astring__builderS1763);
  #line 113 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1876);
  moonbit_decref(_M0L6_2atmpS1876);
  #line 114 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(_M0L2dbS1718, 35000);
  #line 115 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1764
  = _M0MPB13StringBuilder21StringBuilder_2einner(20);
  #line 115 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1764, (moonbit_string_t)moonbit_string_literal_182.data);
  #line 115 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1879
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6exists(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_177.data);
  #line 115 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1764, _M0L6_2atmpS1879);
  #line 115 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1878
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1764);
  moonbit_decref(_M0L18_2astring__builderS1764);
  #line 115 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1878);
  moonbit_decref(_M0L6_2atmpS1878);
  #line 116 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1765
  = _M0MPB13StringBuilder21StringBuilder_2einner(29);
  #line 116 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1765, (moonbit_string_t)moonbit_string_literal_179.data);
  #line 116 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1881
  = _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(_M0L2dbS1718, (moonbit_string_t)moonbit_string_literal_177.data);
  #line 116 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1765, _M0L6_2atmpS1881);
  #line 116 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1765, (moonbit_string_t)moonbit_string_literal_183.data);
  #line 116 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1880
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1765);
  moonbit_decref(_M0L18_2astring__builderS1765);
  #line 116 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1880);
  moonbit_decref(_M0L6_2atmpS1880);
  #line 118 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_184.data);
  #line 119 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_113.data);
  _M0L6_2atmpS1886 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS1886[0] = (moonbit_string_t)moonbit_string_literal_185.data;
  _M0L6_2atmpS1886[1] = (moonbit_string_t)moonbit_string_literal_186.data;
  _M0L6_2atmpS1886[2] = (moonbit_string_t)moonbit_string_literal_187.data;
  _M0L6_2atmpS1883
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1883)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS1883->$0 = _M0L6_2atmpS1886;
  _M0L6_2atmpS1883->$1 = 3;
  _M0L6_2atmpS1885 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS1885[0] = (moonbit_string_t)moonbit_string_literal_188.data;
  _M0L6_2atmpS1885[1] = (moonbit_string_t)moonbit_string_literal_189.data;
  _M0L6_2atmpS1885[2] = (moonbit_string_t)moonbit_string_literal_190.data;
  _M0L6_2atmpS1884
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1884)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS1884->$0 = _M0L6_2atmpS1885;
  _M0L6_2atmpS1884->$1 = 3;
  #line 120 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1882
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4mset(_M0L2dbS1718, _M0L6_2atmpS1883, _M0L6_2atmpS1884);
  moonbit_decref(_M0L6_2atmpS1883);
  moonbit_decref(_M0L6_2atmpS1884);
  #line 121 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_191.data);
  _M0L6_2atmpS1909 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1909[0] = (moonbit_string_t)moonbit_string_literal_185.data;
  _M0L6_2atmpS1909[1] = (moonbit_string_t)moonbit_string_literal_186.data;
  _M0L6_2atmpS1909[2] = (moonbit_string_t)moonbit_string_literal_187.data;
  _M0L6_2atmpS1909[3] = (moonbit_string_t)moonbit_string_literal_192.data;
  _M0L6_2atmpS1908
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1908)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS1908->$0 = _M0L6_2atmpS1909;
  _M0L6_2atmpS1908->$1 = 4;
  #line 122 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L12config__valsS1766
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4mget(_M0L2dbS1718, _M0L6_2atmpS1908);
  moonbit_decref(_M0L6_2atmpS1908);
  #line 123 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_193.data);
  _M0L6_2atmpS1907 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1907[0] = (moonbit_string_t)moonbit_string_literal_185.data;
  _M0L6_2atmpS1907[1] = (moonbit_string_t)moonbit_string_literal_186.data;
  _M0L6_2atmpS1907[2] = (moonbit_string_t)moonbit_string_literal_187.data;
  _M0L6_2atmpS1907[3] = (moonbit_string_t)moonbit_string_literal_192.data;
  _M0L4keysS1767
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L4keysS1767)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L4keysS1767->$0 = _M0L6_2atmpS1907;
  _M0L4keysS1767->$1 = 4;
  _M0L1iS1768 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1887;
    #line 125 "/home/developer/Documents2/moonbitDB/demo.mbt"
    _M0L6_2atmpS1887
    = _M0MPC15array5Array6lengthGOsE(_M0L12config__valsS1766);
    if (_M0L1iS1768 < _M0L6_2atmpS1887) {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1769;
      moonbit_string_t _M0L6_2atmpS1889;
      moonbit_string_t _M0L6_2atmpS1891;
      moonbit_string_t _M0L6_2atmpS1890;
      moonbit_string_t _M0L6_2atmpS1888;
      int32_t _M0L6_2atmpS1892;
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L18_2astring__builderS1769
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1769, (moonbit_string_t)moonbit_string_literal_194.data);
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L6_2atmpS1889
      = _M0MPC15array5Array2atGsE(_M0L4keysS1767, _M0L1iS1768);
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1769, _M0L6_2atmpS1889);
      moonbit_decref(_M0L6_2atmpS1889);
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1769, (moonbit_string_t)moonbit_string_literal_195.data);
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L6_2atmpS1891
      = _M0MPC15array5Array2atGOsE(_M0L12config__valsS1766, _M0L1iS1768);
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L6_2atmpS1890
      = _M0FP28JIA2JIA29moonbitdb9show__opt(_M0L6_2atmpS1891);
      if (_M0L6_2atmpS1891) {
        moonbit_decref(_M0L6_2atmpS1891);
      }
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1769, _M0L6_2atmpS1890);
      moonbit_decref(_M0L6_2atmpS1890);
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0L6_2atmpS1888
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1769);
      moonbit_decref(_M0L18_2astring__builderS1769);
      #line 126 "/home/developer/Documents2/moonbitDB/demo.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1888);
      moonbit_decref(_M0L6_2atmpS1888);
      _M0L6_2atmpS1892 = _M0L1iS1768 + 1;
      _M0L1iS1768 = _M0L6_2atmpS1892;
      continue;
    } else {
      moonbit_decref(_M0L4keysS1767);
      moonbit_decref(_M0L12config__valsS1766);
    }
    break;
  }
  #line 129 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_196.data);
  #line 130 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_113.data);
  #line 131 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1771
  = _M0MPB13StringBuilder21StringBuilder_2einner(11);
  #line 131 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1771, (moonbit_string_t)moonbit_string_literal_197.data);
  #line 131 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1894
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(_M0L2dbS1718);
  #line 131 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1771, _M0L6_2atmpS1894);
  #line 131 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1893
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1771);
  moonbit_decref(_M0L18_2astring__builderS1771);
  #line 131 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1893);
  moonbit_decref(_M0L6_2atmpS1893);
  #line 132 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1772
  = _M0MPB13StringBuilder21StringBuilder_2einner(9);
  #line 132 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1772, (moonbit_string_t)moonbit_string_literal_198.data);
  #line 132 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1896 = _M0MP38JIA2JIA29moonbitdb3lib8Database4ping();
  #line 132 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1772, _M0L6_2atmpS1896);
  moonbit_decref(_M0L6_2atmpS1896);
  #line 132 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1895
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1772);
  moonbit_decref(_M0L18_2astring__builderS1772);
  #line 132 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1895);
  moonbit_decref(_M0L6_2atmpS1895);
  #line 133 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L2rkS1773
  = _M0MP38JIA2JIA29moonbitdb3lib8Database9randomkey(_M0L2dbS1718);
  #line 134 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1774
  = _M0MPB13StringBuilder21StringBuilder_2einner(14);
  #line 134 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1774, (moonbit_string_t)moonbit_string_literal_199.data);
  #line 134 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1898 = _M0FP28JIA2JIA29moonbitdb9show__opt(_M0L2rkS1773);
  if (_M0L2rkS1773) {
    moonbit_decref(_M0L2rkS1773);
  }
  #line 134 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1774, _M0L6_2atmpS1898);
  moonbit_decref(_M0L6_2atmpS1898);
  #line 134 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1897
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1774);
  moonbit_decref(_M0L18_2astring__builderS1774);
  #line 134 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1897);
  moonbit_decref(_M0L6_2atmpS1897);
  #line 135 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L9all__keysS1775
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4keys(_M0L2dbS1718);
  moonbit_decref(_M0L2dbS1718);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1776
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1776, (moonbit_string_t)moonbit_string_literal_200.data);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1900 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1775);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1776, _M0L6_2atmpS1900);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1776, (moonbit_string_t)moonbit_string_literal_201.data);
  _M0L7_2abindS1777 = (moonbit_string_t)moonbit_string_literal_137.data;
  _M0L6_2atmpS1903 = Moonbit_array_length(_M0L7_2abindS1777);
  _M0L6_2atmpS1902
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1777, .$1 = 0, .$2 = _M0L6_2atmpS1903
  };
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1901
  = _M0MPC15array5Array4joinGsE(_M0L9all__keysS1775, _M0L6_2atmpS1902);
  moonbit_decref(_M0L9all__keysS1775);
  moonbit_decref(_M0L6_2atmpS1902.$0);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1776, _M0L6_2atmpS1901);
  moonbit_decref(_M0L6_2atmpS1901);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1776, (moonbit_string_t)moonbit_string_literal_138.data);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1899
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1776);
  moonbit_decref(_M0L18_2astring__builderS1776);
  #line 136 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1899);
  moonbit_decref(_M0L6_2atmpS1899);
  #line 137 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1778
  = _M0MPB13StringBuilder21StringBuilder_2einner(20);
  #line 137 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1778, (moonbit_string_t)moonbit_string_literal_202.data);
  #line 137 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1906 = _M0MP38JIA2JIA29moonbitdb3lib8Database7command();
  #line 137 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1905 = _M0MPC15array5Array6lengthGsE(_M0L6_2atmpS1906);
  moonbit_decref(_M0L6_2atmpS1906);
  #line 137 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1778, _M0L6_2atmpS1905);
  #line 137 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L6_2atmpS1904
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1778);
  moonbit_decref(_M0L18_2astring__builderS1778);
  #line 137 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1904);
  moonbit_decref(_M0L6_2atmpS1904);
  #line 139 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_203.data);
  #line 140 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_204.data);
  #line 141 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_110.data);
  return 0;
}