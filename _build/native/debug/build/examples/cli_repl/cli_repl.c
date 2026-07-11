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
struct _M0TWEOUsiE;

struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__;

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0TPB9ArrayViewGUsfEE;

struct _M0TPB8MutLocalGORPC16string10StringViewE;

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0TWEOUssE;

struct _M0TPB4IterGUsiEE;

struct _M0TPB3MapGsfE;

struct _M0TUsiE;

struct _M0TUsbE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsiEE;

struct _M0TPB17FloatingDecimal64;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List;

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__;

struct _M0TPB5EntryGssE;

struct _M0TUssE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String;

struct _M0BTPB6Logger;

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__;

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__;

struct _M0TPB4IterGcE;

struct _M0TPB6Logger;

struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__;

struct _M0TWcERPC16string10StringView;

struct _M0TP38JIA2JIA29moonbitdb3lib8Database;

struct _M0TUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsfEE;

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__;

struct _M0TPB5EntryGsbE;

struct _M0TPB8MutLocalGORPB5EntryGssEE;

struct _M0DTPC16option6OptionGfE4Some;

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque;

struct _M0TPB19MulShiftAll64Result;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set;

struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0TPB4IterGRPC16string10StringViewE;

struct _M0TPB5ArrayGOsE;

struct _M0TWEOUsbE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet;

struct _M0TPB5EntryGsfE;

struct _M0TPB8MutLocalGiE;

struct _M0TWERPC16option6OptionGRPC16string10StringViewE;

struct _M0TPB5ArrayGUsfEE;

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0TPB3MapGssE;

struct _M0TPB4Show;

struct _M0TPB8MutLocalGfE;

struct _M0TWEOc;

struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash;

struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__;

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0BTPB4Show;

struct _M0TPB8MutLocalGORPB5EntryGsiEE;

struct _M0TPC16string10StringView;

struct _M0TPB8MutLocalGbE;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB8MutLocalGORPB5EntryGsbEE;

struct _M0TPB3MapGsbE;

struct _M0TPB5ArrayGsE;

struct _M0TPB3MapGsiE;

struct _M0TPB9ArrayViewGUssEE;

struct _M0TWcEb;

struct _M0TPB9ArrayViewGUsbEE;

struct _M0TPB9ArrayViewGsE;

struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__;

struct _M0TUiiE;

struct _M0TPB4IterGUsbEE;

struct _M0TPB4IterGUssEE;

struct _M0TPB5EntryGsiE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0TWEOUsiE {
  struct _M0TUsiE*(* code)(struct _M0TWEOUsiE*);
  
};

struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__ {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*(* code)(
    struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* $1;
  
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

struct _M0TPB8MutLocalGORPC16string10StringViewE {
  void* $0;
  
};

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE {
  moonbit_string_t $0;
  void* $1;
  
};

struct _M0TWEOUssE {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  
};

struct _M0TPB4IterGUsiEE {
  struct _M0TWEOUsiE* $0;
  int64_t $1;
  
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

struct _M0TUsiE {
  moonbit_string_t $0;
  int32_t $1;
  
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

struct _M0TPC16string10StringView {
  moonbit_string_t $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  struct _M0TPC16string10StringView $0;
  
};

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List {
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* $0;
  
};

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
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

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__ {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* $1;
  
};

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
};

struct _M0TPB4IterGcE {
  struct _M0TWEOc* $0;
  int64_t $1;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TPB8MutLocalGORPC16string10StringViewE* $0;
  struct _M0TPC16string10StringView $1;
  int32_t $2;
  
};

struct _M0TWcERPC16string10StringView {
  struct _M0TPC16string10StringView(* code)(
    struct _M0TWcERPC16string10StringView*,
    int32_t
  );
  
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

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TWcERPC16string10StringView* $0;
  struct _M0TPB4IterGcE* $1;
  
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

struct _M0TPB4IterGRPC16string10StringViewE {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* $0;
  int64_t $1;
  
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

struct _M0TWERPC16option6OptionGRPC16string10StringViewE {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  
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

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TPB8MutLocalGfE {
  float $0;
  
};

struct _M0TWEOc {
  int32_t(* code)(struct _M0TWEOc*);
  
};

struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*(* code)(
    struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
  );
  
};

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash {
  struct _M0TPB3MapGssE* $0;
  
};

struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__ {
  struct _M0TUsiE*(* code)(struct _M0TWEOUsiE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsiEE* $1;
  
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

struct _M0TPB8MutLocalGORPB5EntryGsiEE {
  struct _M0TPB5EntryGsiE* $0;
  
};

struct _M0TPB8MutLocalGbE {
  int32_t $0;
  
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

struct _M0TWcEb {
  int32_t(* code)(struct _M0TWcEb*, int32_t);
  
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

struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TPB8MutLocalGiE* $0;
  int32_t $1;
  struct _M0TPC16string10StringView $2;
  
};

struct _M0TUiiE {
  int32_t $0;
  int32_t $1;
  
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

struct _M0TPB5ArrayGsE* _M0FP48JIA2JIA29moonbitdb8examples9cli__repl16execute__command(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0FP48JIA2JIA29moonbitdb8examples9cli__repl14split__command(
  moonbit_string_t
);

void* _M0FP48JIA2JIA29moonbitdb8examples9cli__repl12parse__float(
  moonbit_string_t
);

int64_t _M0FP48JIA2JIA29moonbitdb8examples9cli__repl10parse__int(
  moonbit_string_t
);

struct _M0TUiiE* _M0MP38JIA2JIA29moonbitdb3lib8Database4time(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4info(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4ping();

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database7flushdb(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

float _M0MP38JIA2JIA29moonbitdb3lib8Database7zincrby(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  float,
  moonbit_string_t
);

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database5zrank(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
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

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6zrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t,
  int32_t
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4srem(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5lpush(
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hdel(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6append(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database8type__of(
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3del(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database3get(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4mdel(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database7persist(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque11push__front(
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

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*
);

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(struct _M0TPB4IterGUsbEE*);

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(struct _M0TPB4IterGUsfEE*);

struct _M0TUsiE* _M0MPB5Iter24nextGsiE(struct _M0TPB4IterGUsiEE*);

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUsiEE* _M0MPB3Map5iter2GsiE(struct _M0TPB3MapGsiE*);

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUsiEE* _M0MPB3Map4iterGsiE(struct _M0TPB3MapGsiE*);

struct _M0TUsiE* _M0MPB3Map4iterGsiEC3512l711(struct _M0TWEOUsiE*);

struct _M0TUsfE* _M0MPB3Map4iterGsfEC3502l711(struct _M0TWEOUsfE*);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC3492l711(struct _M0TWEOUsbE*);

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC3482l711(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TUssE* _M0MPB3Map4iterGssEC3472l711(struct _M0TWEOUssE*);

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map6removeGssE(struct _M0TPB3MapGssE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

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

int32_t _M0MPB3Map18remove__with__hashGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map11shift__backGsiE(struct _M0TPB3MapGsiE*, int32_t);

int32_t _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map11shift__backGssE(struct _M0TPB3MapGssE*, int32_t);

int32_t _M0MPB3Map11shift__backGsbE(struct _M0TPB3MapGsbE*, int32_t);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map13remove__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*
);

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map8containsGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGssE(struct _M0TPB3MapGssE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

void* _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int64_t _M0MPB3Map3getGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

moonbit_string_t _M0MPB3Map3getGssE(struct _M0TPB3MapGssE*, moonbit_string_t);

void* _M0MPB3Map3getGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

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

int32_t _M0MPB3Map3setGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t, int32_t);

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB3Map3setGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t, int32_t);

int32_t _M0MPB3Map3setGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t, float);

int32_t _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t,
  void*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE*,
  moonbit_string_t,
  int32_t,
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

int32_t _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE*);

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
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

int32_t _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE*,
  int32_t,
  struct _M0TPB5EntryGsiE*
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

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE*,
  int32_t,
  struct _M0TPB5EntryGsiE*
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

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t
);

moonbit_string_t _M0MPC16string6String9to__upper(moonbit_string_t);

int32_t _M0MPC16string6String9to__upperC2839l1791(struct _M0TWcEb*, int32_t);

int32_t _M0MPC14char4Char20is__ascii__lowercase(int32_t);

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

void* _M0MPC16string10StringView5splitC2793l1148(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC2789l1145(
  struct _M0TWcERPC16string10StringView*,
  int32_t
);

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t);

moonbit_string_t _M0FPB16char__to__string(int32_t);

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TPB4IterGcE*,
  struct _M0TWcERPC16string10StringView*
);

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC2782l391(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE*,
  struct _M0TUsfE*
);

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsfEE(struct _M0TPB5ArrayGUsfEE*);

int32_t _M0MPC15array5Array7reallocGOsE(struct _M0TPB5ArrayGOsE*);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGOsE(
  struct _M0TPB5ArrayGOsE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array6lengthGOsE(struct _M0TPB5ArrayGOsE*);

int32_t _M0MPC15array5Array6lengthGUsfEE(struct _M0TPB5ArrayGUsfEE*);

int64_t _M0MPC16string6String8find__by(moonbit_string_t, struct _M0TWcEb*);

int64_t _M0MPC16string10StringView8find__by(
  struct _M0TPC16string10StringView,
  struct _M0TWcEb*
);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

struct _M0TPB4IterGcE* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView4iterC2653l219(struct _M0TWEOc*);

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView,
  struct _M0TPB6Logger
);

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0IPC14byte4BytePB7Default7default();

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

#define _M0FPB19unsafe__sub__string moonbit_unsafe_bytes_sub_string

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t,
  int32_t,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPC14uint4UInt8to__byte(uint32_t);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(struct _M0TPB5ArrayGOsE*);

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView,
  int32_t,
  int64_t
);

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(struct _M0TWEOUssE*, int64_t);

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*,
  int64_t
);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int64_t
);

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(struct _M0TWEOUsbE*, int64_t);

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(struct _M0TWEOUsfE*, int64_t);

struct _M0TPB4IterGUsiEE* _M0MPB4Iter3newGUsiEE(struct _M0TWEOUsiE*, int64_t);

struct _M0TPB4IterGcE* _M0MPB4Iter3newGcE(struct _M0TWEOc*, int64_t);

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

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TPB4IterGRPC16string10StringViewE*
);

struct _M0TUssE* _M0MPB4Iter4nextGUssEE(struct _M0TPB4IterGUssEE*);

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*
);

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(struct _M0TPB4IterGUsbEE*);

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(struct _M0TPB4IterGUsfEE*);

struct _M0TUsiE* _M0MPB4Iter4nextGUsiEE(struct _M0TPB4IterGUsiEE*);

int32_t _M0MPB4Iter4nextGcE(struct _M0TPB4IterGcE*);

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IP016_24default__implPB4Show6outputGiE(
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

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t);

int32_t _M0FPB32code__point__of__surrogate__pair(int32_t, int32_t);

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t);

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t);

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

int32_t _M0MPC13int3Int16unsafe__to__char(int32_t);

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
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

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t*,
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

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder*,
  float
);

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
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

struct _M0TUsfE** _M0MPB18UninitializedArray23unsafe__make__and__blitGUsfEE(
  struct _M0TUsfE**,
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGOsE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsfEEE(
  struct _M0TUsfE**,
  int32_t,
  struct _M0TUsfE**,
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

int32_t _M0MPB18UninitializedArray6lengthGsE(moonbit_string_t*);

int32_t _M0MPB18UninitializedArray6lengthGUsfEE(struct _M0TUsfE**);

int32_t _M0MPB18UninitializedArray6lengthGOsE(moonbit_string_t*);

uint32_t _M0FPB13consume4__acc(uint32_t, uint32_t);

uint32_t _M0FPB4rotl(uint32_t, int32_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

uint16_t* _M0FPC15abort5abortGAkE(moonbit_string_t);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(
  moonbit_string_t
);

struct _M0TUsfE** _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(
  moonbit_string_t
);

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(
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

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_95 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 111, 
    110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_91 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 55, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_75 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 0, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_14 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 69, 82, 
    82, 32, 119, 114, 111, 110, 103, 32, 110, 117, 109, 98, 101, 114, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_65 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_10 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 45, 32, 32, 
    90, 65, 68, 68, 32, 107, 101, 121, 32, 115, 32, 109, 32, 124, 32, 
    90, 82, 65, 78, 71, 69, 32, 107, 101, 121, 32, 115, 32, 101, 32, 
    124, 32, 90, 82, 65, 78, 75, 32, 107, 101, 121, 32, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_64 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 68, 69, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_134 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 83, 65, 
    68, 68, 32, 116, 97, 103, 115, 32, 116, 101, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_9 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 52, 32, 32, 
    83, 65, 68, 68, 32, 107, 101, 121, 32, 109, 32, 124, 32, 83, 77, 
    69, 77, 66, 69, 82, 83, 32, 107, 101, 121, 32, 124, 32, 83, 82, 69, 
    77, 32, 107, 101, 121, 32, 109, 32, 124, 32, 83, 67, 65, 82, 68, 
    32, 107, 101, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_27 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 68, 66, 
    83, 73, 90, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_103 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 110, 
    102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_102 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 78, 97, 78, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_58 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_48 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 83, 77, 
    69, 77, 66, 69, 82, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_83 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 79, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[47]; 
} const moonbit_string_literal_6 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 46, 32, 32, 
    73, 78, 67, 82, 32, 107, 101, 121, 32, 124, 32, 68, 69, 67, 82, 32, 
    107, 101, 121, 32, 124, 32, 69, 88, 80, 73, 82, 69, 32, 107, 101, 
    121, 32, 115, 32, 124, 32, 84, 84, 76, 32, 107, 101, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_85 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_84 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_74 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 29, 69, 82, 
    82, 32, 119, 114, 111, 110, 103, 32, 110, 117, 109, 98, 101, 114, 
    32, 111, 102, 32, 97, 114, 103, 117, 109, 101, 110, 116, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_144 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    65, 78, 75, 32, 115, 99, 111, 114, 101, 115, 32, 98, 111, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_30 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_42 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_135 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 83, 65, 
    68, 68, 32, 116, 97, 103, 115, 32, 112, 114, 111, 103, 114, 97, 109, 
    109, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_121 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 65, 80, 
    80, 69, 78, 68, 32, 110, 97, 109, 101, 32, 39, 32, 83, 109, 105, 
    116, 104, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_88 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_124 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 84, 84, 
    76, 32, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_147 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 77, 71, 
    69, 84, 32, 97, 32, 98, 32, 99, 32, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_113 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 44, 101, 
    120, 112, 105, 114, 101, 115, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_110 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_72 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 71, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_5 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 49, 32, 32, 
    75, 69, 89, 83, 32, 124, 32, 84, 89, 80, 69, 32, 107, 101, 121, 32, 
    124, 32, 65, 80, 80, 69, 78, 68, 32, 107, 101, 121, 32, 118, 97, 
    108, 117, 101, 32, 124, 32, 83, 84, 82, 76, 69, 78, 32, 107, 101, 
    121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_101 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 73, 108, 
    108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 116, 69, 
    120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_80 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 35, 32, 
    83, 101, 114, 118, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_4 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 48, 32, 32, 
    83, 69, 84, 32, 107, 101, 121, 32, 118, 97, 108, 117, 101, 32, 124, 
    32, 71, 69, 84, 32, 107, 101, 121, 32, 124, 32, 68, 69, 76, 32, 107, 
    101, 121, 32, 124, 32, 69, 88, 73, 83, 84, 83, 32, 107, 101, 121, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_45 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_15 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 27, 69, 82, 
    82, 32, 118, 97, 108, 117, 101, 32, 105, 115, 32, 110, 111, 116, 
    32, 97, 110, 32, 105, 110, 116, 101, 103, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_26 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 79, 75, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_17 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 109, 115, 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_94 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_8 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 52, 32, 32, 
    76, 80, 85, 83, 72, 47, 82, 80, 85, 83, 72, 32, 107, 101, 121, 32, 
    118, 32, 124, 32, 76, 80, 79, 80, 47, 82, 80, 79, 80, 32, 107, 101, 
    121, 32, 124, 32, 76, 82, 65, 78, 71, 69, 32, 107, 101, 121, 32, 
    115, 32, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_89 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_100 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 122, 115, 
    101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_66 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 84, 
    82, 76, 69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_59 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_68 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 89, 
    80, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_131 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 82, 80, 
    85, 83, 72, 32, 116, 97, 115, 107, 115, 32, 39, 84, 97, 115, 107, 
    32, 51, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 65, 80, 
    80, 69, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_152 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 65, 68, 
    86, 65, 78, 67, 69, 32, 55, 48, 48, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_47 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_46 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_53 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_36 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 69, 82, 
    82, 32, 118, 97, 108, 117, 101, 32, 105, 115, 32, 110, 111, 116, 
    32, 97, 32, 118, 97, 108, 105, 100, 32, 102, 108, 111, 97, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_114 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_157 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    9989, 32, 21629, 20196, 34892, 20132, 20114, 28436, 31034, 23436, 
    25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_39 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 83, 
    67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_129 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 76, 80, 
    85, 83, 72, 32, 116, 97, 115, 107, 115, 32, 39, 84, 97, 115, 107, 
    32, 49, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_38 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 40, 110, 
    105, 108, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_13 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 65, 68, 
    86, 65, 78, 67, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_87 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_11 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 53, 32, 32, 
    77, 83, 69, 84, 47, 77, 71, 69, 84, 47, 77, 68, 69, 76, 32, 46, 46, 
    46, 32, 124, 32, 68, 66, 83, 73, 90, 69, 32, 124, 32, 70, 76, 85, 
    83, 72, 68, 66, 32, 124, 32, 80, 73, 78, 71, 32, 124, 32, 73, 78, 
    70, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_21 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    70, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_138 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 67, 
    65, 82, 68, 32, 116, 97, 103, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_70 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_69 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 75, 69, 
    89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_118 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 71, 69, 
    84, 32, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_37 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 82, 
    65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_136 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 83, 65, 
    68, 68, 32, 116, 97, 103, 115, 32, 109, 111, 111, 110, 98, 105, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_126 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 32, 72, 83, 
    69, 84, 32, 117, 115, 101, 114, 58, 49, 32, 101, 109, 97, 105, 108, 
    32, 97, 108, 105, 99, 101, 64, 116, 101, 115, 116, 46, 99, 111, 109, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_20 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    109, 105, 99, 114, 111, 115, 101, 99, 111, 110, 100, 115, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_2 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 39, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_40 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_137 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 83, 77, 
    69, 77, 66, 69, 82, 83, 32, 116, 97, 103, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_117 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 69, 
    84, 32, 97, 103, 101, 32, 50, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_128 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 72, 76, 
    69, 78, 32, 117, 115, 101, 114, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_62 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 84, 84, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_120 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 78, 
    67, 82, 32, 97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_125 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 26, 72, 83, 
    69, 84, 32, 117, 115, 101, 114, 58, 49, 32, 117, 115, 101, 114, 110, 
    97, 109, 101, 32, 97, 108, 105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_130 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 76, 80, 
    85, 83, 72, 32, 116, 97, 115, 107, 115, 32, 39, 84, 97, 115, 107, 
    32, 50, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_77 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 100, 98, 
    48, 58, 107, 101, 121, 115, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_43 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 40, 105, 
    110, 116, 101, 103, 101, 114, 41, 32, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_119 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 71, 69, 
    84, 32, 97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_51 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_3 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 69, 
    76, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_106 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 114, 97, 
    100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 98, 101, 
    116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_86 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_16 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 79, 75, 
    32, 40, 97, 100, 118, 97, 110, 99, 101, 100, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_12 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 26, 32, 32, 
    65, 68, 86, 65, 78, 67, 69, 32, 109, 115, 32, 124, 32, 72, 69, 76, 
    80, 32, 124, 32, 81, 85, 73, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_127 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 72, 71, 
    69, 84, 32, 117, 115, 101, 114, 58, 49, 32, 117, 115, 101, 114, 110, 
    97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_122 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 84, 
    82, 76, 69, 78, 32, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_1 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 69, 82, 
    82, 32, 117, 110, 107, 110, 111, 119, 110, 32, 99, 111, 109, 109, 
    97, 110, 100, 32, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_82 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 35, 32, 
    75, 101, 121, 115, 112, 97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_56 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_55 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 76, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_153 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 69, 88, 
    73, 83, 84, 83, 32, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_112 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 44, 32, 
    108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_149 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 84, 89, 
    80, 69, 32, 117, 115, 101, 114, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_109 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 98, 111, 
    117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 97, 105, 
    108, 101, 100, 58, 32, 97, 108, 108, 111, 99, 97, 116, 101, 95, 108, 
    101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_49 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_99 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 115, 101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_41 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_90 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 73, 
    77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_139 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 32, 116, 97, 103, 115, 32, 109, 111, 
    111, 110, 98, 105, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_107 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 
    118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_143 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 90, 82, 
    65, 78, 71, 69, 32, 115, 99, 111, 114, 101, 115, 32, 48, 32, 45, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_76 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 117, 112, 
    116, 105, 109, 101, 95, 105, 110, 95, 109, 115, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_31 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 41, 32, 
    40, 110, 105, 108, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_22 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_23 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_132 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 76, 82, 
    65, 78, 71, 69, 32, 116, 97, 115, 107, 115, 32, 48, 32, 45, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_96 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 115, 116, 
    114, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_35 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 73, 
    78, 67, 82, 66, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_54 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 82, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_145 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 23, 90, 73, 
    78, 67, 82, 66, 89, 32, 115, 99, 111, 114, 101, 115, 32, 53, 48, 
    32, 97, 108, 105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_140 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 90, 65, 
    68, 68, 32, 115, 99, 111, 114, 101, 115, 32, 49, 48, 48, 32, 97, 
    108, 105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_19 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    115, 101, 99, 111, 110, 100, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_115 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 32, 32, 
    77, 111, 111, 110, 66, 105, 116, 68, 66, 32, 45, 32, 21629, 20196, 
    34892, 20132, 20114, 28436, 31034, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_111 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    100, 115, 116, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_108 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 105, 110, 
    118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 111, 105, 
    110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_57 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 71, 
    69, 84, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_29 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_105 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 73, 110, 
    118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 102, 111, 
    114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_24 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 73, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_154 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    55356, 57260, 32, 24320, 22987, 28436, 31034, 21629, 20196, 25191, 
    34892, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_148 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 84, 89, 
    80, 69, 32, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_73 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 83, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_146 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 77, 83, 
    69, 84, 32, 97, 32, 49, 32, 98, 32, 50, 32, 99, 32, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_93 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 57, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_79 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 44, 97, 
    118, 103, 95, 116, 116, 108, 61, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_60 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_28 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 40, 105, 
    110, 116, 101, 103, 101, 114, 41, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_81 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 24, 109, 111, 
    111, 110, 98, 105, 116, 95, 100, 98, 95, 118, 101, 114, 115, 105, 
    111, 110, 58, 49, 46, 48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_52 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 82, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_155 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_141 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 90, 65, 
    68, 68, 32, 115, 99, 111, 114, 101, 115, 32, 50, 48, 48, 32, 98, 
    111, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_142 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 23, 90, 65, 
    68, 68, 32, 115, 99, 111, 114, 101, 115, 32, 49, 53, 48, 32, 99, 
    104, 97, 114, 108, 105, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_63 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_150 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 84, 89, 
    80, 69, 32, 116, 97, 115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_123 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 69, 88, 
    80, 73, 82, 69, 32, 110, 97, 109, 101, 32, 54, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_156 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    62, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_133 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 76, 76, 
    69, 78, 32, 116, 97, 115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_33 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_92 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_32 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 41, 32, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_98 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 108, 105, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_50 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_44 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 40, 105, 
    110, 116, 101, 103, 101, 114, 41, 32, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_71 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 68, 69, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 40, 101, 
    109, 112, 116, 121, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_97 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 104, 97, 
    115, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_116 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 83, 69, 
    84, 32, 110, 97, 109, 101, 32, 65, 108, 105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_151 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 84, 89, 
    80, 69, 32, 115, 99, 111, 114, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_104 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 48, 46, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_25 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 70, 76, 
    85, 83, 72, 68, 66, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_7 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 52, 32, 32, 
    72, 83, 69, 84, 32, 107, 101, 121, 32, 102, 32, 118, 32, 124, 32, 
    72, 71, 69, 84, 32, 107, 101, 121, 32, 102, 32, 124, 32, 72, 71, 
    69, 84, 65, 76, 76, 32, 107, 101, 121, 32, 124, 32, 72, 76, 69, 78, 
    32, 107, 101, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_61 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    82, 83, 73, 83, 84, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0)
  };

struct { int32_t rc; uint32_t meta; struct _M0TWcEb data; 
} const _M0MPC16string6String9to__upperC2839l1791$closure =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0),
    _M0MPC16string6String9to__upperC2839l1791
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWcERPC16string10StringView data;
  
} const _M0MPC16string10StringView5splitC2789l1145$closure =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0),
    _M0MPC16string10StringView5splitC2789l1145
  };

uint32_t const moonbit_layout_table_data[171] =
  {
    sizeof(struct _M0TPB5ArrayGsE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGsE, $0) / 4,
    sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet) / 4, 
    1,
    offsetof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet, $0) / 4,
    sizeof(struct _M0TPB5ArrayGUsfEE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGUsfEE, $0) / 4, sizeof(struct _M0TUsfE) / 4,
    1, offsetof(struct _M0TUsfE, $0) / 4,
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
    sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__)
    / 4, 2,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__, $0)
    / 4,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__)
    / 4, 2,
    offsetof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__, $0)
    / 4,
    offsetof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsiEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsiEE, $0) / 4,
    sizeof(struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__)
    / 4, 2,
    offsetof(struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__, $0)
    / 4,
    offsetof(struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__, $1)
    / 4, sizeof(struct _M0TUsiE) / 4, 1, offsetof(struct _M0TUsiE, $0) / 4,
    sizeof(struct _M0TUsbE) / 4, 1, offsetof(struct _M0TUsbE, $0) / 4,
    sizeof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE) / 4, 
    2,
    offsetof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $0) / 4,
    offsetof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $1) / 4,
    sizeof(struct _M0TUssE) / 4, 2, offsetof(struct _M0TUssE, $0) / 4,
    offsetof(struct _M0TUssE, $1) / 4,
    sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE) / 4,
    3,
    offsetof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $1)
    / 4,
    offsetof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $4)
    / 4,
    offsetof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $5)
    / 4, sizeof(struct _M0TPB5EntryGsiE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsiE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsiE, $4) / 4,
    sizeof(struct _M0TPB5EntryGssE) / 4, 3,
    offsetof(struct _M0TPB5EntryGssE, $1) / 4,
    offsetof(struct _M0TPB5EntryGssE, $4) / 4,
    offsetof(struct _M0TPB5EntryGssE, $5) / 4,
    sizeof(struct _M0TPB5EntryGsbE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsbE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsbE, $4) / 4,
    sizeof(struct _M0TPB5EntryGsfE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsfE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsfE, $4) / 4,
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
    sizeof(struct _M0TPC16string10StringView) / 4, 1,
    offsetof(struct _M0TPC16string10StringView, $0) / 4,
    sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some) / 4,
    1,
    (offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0)
     + offsetof(struct _M0TPC16string10StringView, $0))
    / 4, sizeof(struct _M0TPB8MutLocalGORPC16string10StringViewE) / 4, 
    1, offsetof(struct _M0TPB8MutLocalGORPC16string10StringViewE, $0) / 4,
    sizeof(struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__) / 4, 
    2,
    offsetof(struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__, $0)
    / 4,
    (offsetof(struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__, $1)
     + offsetof(struct _M0TPC16string10StringView, $0))
    / 4,
    sizeof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__)
    / 4, 2,
    offsetof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__, $0)
    / 4,
    offsetof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__, $1)
    / 4, sizeof(struct _M0TPB4IterGRPC16string10StringViewE) / 4, 1,
    offsetof(struct _M0TPB4IterGRPC16string10StringViewE, $0) / 4,
    sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__) / 4, 
    2,
    offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__, $0) / 4,
    (offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__, $2)
     + offsetof(struct _M0TPC16string10StringView, $0))
    / 4, sizeof(struct _M0TPB4IterGUssEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUssEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE) / 4,
    1,
    offsetof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE, $0)
    / 4, sizeof(struct _M0TPB4IterGUsbEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsbEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsfEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsfEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsiEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsiEE, $0) / 4,
    sizeof(struct _M0TPB4IterGcE) / 4, 1,
    offsetof(struct _M0TPB4IterGcE, $0) / 4,
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

int64_t _M0MPB4Iter4nextN6constrS9980GRPC16string10StringViewE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GRPC16string10StringViewE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUssEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUssEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsiEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsiEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GcE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GcE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUssEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GRPC16string10StringViewE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsbEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsfEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsiEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GcE = 0ll;

int64_t _M0FPB28boyer__moore__horspool__findN6constrS9990 = 0ll;

int64_t _M0FPB18brute__force__findN6constrS9991 = 0ll;

struct _M0TPB5ArrayGsE* _M0FP48JIA2JIA29moonbitdb8examples9cli__repl16execute__command(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L2dbS2088,
  moonbit_string_t _M0L3cmdS2086
) {
  struct _M0TPB5ArrayGsE* _M0L5partsS2085;
  int32_t _M0L6_2atmpS4263;
  moonbit_string_t _M0L6_2atmpS4555;
  moonbit_string_t _M0L7commandS2087;
  #line 105 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  #line 106 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L5partsS2085
  = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl14split__command(_M0L3cmdS2086);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6_2atmpS4263 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
  if (_M0L6_2atmpS4263 == 0) {
    moonbit_string_t* _M0L6_2atmpS4264;
    struct _M0TPB5ArrayGsE* _block_5098;
    moonbit_decref(_M0L5partsS2085);
    _M0L6_2atmpS4264 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4264[0] = (moonbit_string_t)moonbit_string_literal_0.data;
    _block_5098
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5098)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5098->$0 = _M0L6_2atmpS4264;
    _block_5098->$1 = 1;
    return _block_5098;
  }
  #line 110 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6_2atmpS4555 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 0);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L7commandS2087 = _M0MPC16string6String9to__upper(_M0L6_2atmpS4555);
  moonbit_decref(_M0L6_2atmpS4555);
  if (
    _M0L7commandS2087 == (moonbit_string_t)moonbit_string_literal_73.data
    || Moonbit_array_length(_M0L7commandS2087) == 3
       && 0
          == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_73.data, 6)
  ) {
    int32_t _M0L6_2atmpS4265;
    moonbit_decref(_M0L7commandS2087);
    #line 113 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4265 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4265 < 3) {
      moonbit_string_t* _M0L6_2atmpS4266;
      struct _M0TPB5ArrayGsE* _block_5233;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4266 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4266[0] = (moonbit_string_t)moonbit_string_literal_74.data;
      _block_5233
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5233)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5233->$0 = _M0L6_2atmpS4266;
      _block_5233->$1 = 1;
      return _block_5233;
    } else {
      moonbit_string_t _M0L6_2atmpS4267;
      moonbit_string_t _M0L6_2atmpS4268;
      moonbit_string_t* _M0L6_2atmpS4269;
      struct _M0TPB5ArrayGsE* _block_5234;
      #line 114 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4267 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 114 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4268 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 114 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MP38JIA2JIA29moonbitdb3lib8Database3set(_M0L2dbS2088, _M0L6_2atmpS4267, _M0L6_2atmpS4268);
      moonbit_decref(_M0L6_2atmpS4267);
      moonbit_decref(_M0L6_2atmpS4268);
      _M0L6_2atmpS4269 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4269[0] = (moonbit_string_t)moonbit_string_literal_26.data;
      _block_5234
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5234)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5234->$0 = _M0L6_2atmpS4269;
      _block_5234->$1 = 1;
      return _block_5234;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_72.data
           || Moonbit_array_length(_M0L7commandS2087) == 3
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_72.data, 6)
         ) {
    int32_t _M0L6_2atmpS4270;
    moonbit_decref(_M0L7commandS2087);
    #line 117 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4270 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4270 < 2) {
      moonbit_string_t* _M0L6_2atmpS4271;
      struct _M0TPB5ArrayGsE* _block_5229;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4271 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4271[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5229
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5229)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5229->$0 = _M0L6_2atmpS4271;
      _block_5229->$1 = 1;
      return _block_5229;
    } else {
      moonbit_string_t _M0L1vS2090;
      moonbit_string_t _M0L6_2atmpS4275;
      moonbit_string_t _M0L7_2abindS2092;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2091;
      moonbit_string_t _M0L6_2atmpS4273;
      moonbit_string_t* _M0L6_2atmpS4272;
      struct _M0TPB5ArrayGsE* _block_5232;
      #line 119 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4275 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2092
      = _M0MP38JIA2JIA29moonbitdb3lib8Database3get(_M0L2dbS2088, _M0L6_2atmpS4275);
      moonbit_decref(_M0L6_2atmpS4275);
      if (_M0L7_2abindS2092 == 0) {
        moonbit_string_t* _M0L6_2atmpS4274;
        struct _M0TPB5ArrayGsE* _block_5231;
        if (_M0L7_2abindS2092) {
          moonbit_decref(_M0L7_2abindS2092);
        }
        _M0L6_2atmpS4274 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4274[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5231
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5231)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5231->$0 = _M0L6_2atmpS4274;
        _block_5231->$1 = 1;
        return _block_5231;
      } else {
        moonbit_string_t _M0L7_2aSomeS2093 = _M0L7_2abindS2092;
        moonbit_string_t _M0L4_2avS2094 = _M0L7_2aSomeS2093;
        _M0L1vS2090 = _M0L4_2avS2094;
        goto join_2089;
      }
      join_2089:;
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2091
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2091, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2091, _M0L1vS2090);
      moonbit_decref(_M0L1vS2090);
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2091, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4273
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2091);
      moonbit_decref(_M0L18_2astring__builderS2091);
      _M0L6_2atmpS4272 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4272[0] = _M0L6_2atmpS4273;
      _block_5232
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5232)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5232->$0 = _M0L6_2atmpS4272;
      _block_5232->$1 = 1;
      return _block_5232;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_71.data
           || Moonbit_array_length(_M0L7commandS2087) == 3
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_71.data, 6)
         ) {
    int32_t _M0L6_2atmpS4276;
    moonbit_decref(_M0L7commandS2087);
    #line 126 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4276 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4276 < 2) {
      moonbit_string_t* _M0L6_2atmpS4277;
      struct _M0TPB5ArrayGsE* _block_5225;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4277 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4277[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5225
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5225)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5225->$0 = _M0L6_2atmpS4277;
      _block_5225->$1 = 1;
      return _block_5225;
    } else {
      moonbit_string_t _M0L6_2atmpS4278;
      int32_t _result_5226;
      #line 127 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4278 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 127 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5226
      = _M0MP38JIA2JIA29moonbitdb3lib8Database3del(_M0L2dbS2088, _M0L6_2atmpS4278);
      moonbit_decref(_M0L6_2atmpS4278);
      if (_result_5226) {
        moonbit_string_t* _M0L6_2atmpS4279 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5227;
        _M0L6_2atmpS4279[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5227
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5227)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5227->$0 = _M0L6_2atmpS4279;
        _block_5227->$1 = 1;
        return _block_5227;
      } else {
        moonbit_string_t* _M0L6_2atmpS4280 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5228;
        _M0L6_2atmpS4280[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5228
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5228)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5228->$0 = _M0L6_2atmpS4280;
        _block_5228->$1 = 1;
        return _block_5228;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_70.data
           || Moonbit_array_length(_M0L7commandS2087) == 6
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_70.data, 12)
         ) {
    int32_t _M0L6_2atmpS4281;
    moonbit_decref(_M0L7commandS2087);
    #line 130 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4281 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4281 < 2) {
      moonbit_string_t* _M0L6_2atmpS4282;
      struct _M0TPB5ArrayGsE* _block_5221;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4282 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4282[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5221
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5221)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5221->$0 = _M0L6_2atmpS4282;
      _block_5221->$1 = 1;
      return _block_5221;
    } else {
      moonbit_string_t _M0L6_2atmpS4283;
      int32_t _result_5222;
      #line 131 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4283 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 131 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5222
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6exists(_M0L2dbS2088, _M0L6_2atmpS4283);
      moonbit_decref(_M0L6_2atmpS4283);
      if (_result_5222) {
        moonbit_string_t* _M0L6_2atmpS4284 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5223;
        _M0L6_2atmpS4284[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5223
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5223)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5223->$0 = _M0L6_2atmpS4284;
        _block_5223->$1 = 1;
        return _block_5223;
      } else {
        moonbit_string_t* _M0L6_2atmpS4285 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5224;
        _M0L6_2atmpS4285[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5224
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5224)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5224->$0 = _M0L6_2atmpS4285;
        _block_5224->$1 = 1;
        return _block_5224;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_69.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_69.data, 8)
         ) {
    struct _M0TPB5ArrayGsE* _M0L4keysS2095;
    moonbit_string_t* _M0L6_2atmpS4291;
    struct _M0TPB5ArrayGsE* _M0L6resultS2096;
    int32_t _M0L1iS2097;
    moonbit_decref(_M0L7commandS2087);
    moonbit_decref(_M0L5partsS2085);
    #line 134 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L4keysS2095
    = _M0MP38JIA2JIA29moonbitdb3lib8Database4keys(_M0L2dbS2088);
    _M0L6_2atmpS4291 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS2096
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS2096)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS2096->$0 = _M0L6_2atmpS4291;
    _M0L6resultS2096->$1 = 0;
    _M0L1iS2097 = 0;
    while (1) {
      int32_t _M0L6_2atmpS4286;
      #line 136 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4286 = _M0MPC15array5Array6lengthGsE(_M0L4keysS2095);
      if (_M0L1iS2097 < _M0L6_2atmpS4286) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2098;
        int32_t _M0L6_2atmpS4288;
        moonbit_string_t _M0L6_2atmpS4289;
        moonbit_string_t _M0L6_2atmpS4287;
        int32_t _M0L6_2atmpS4290;
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L18_2astring__builderS2098
        = _M0MPB13StringBuilder21StringBuilder_2einner(6);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2098, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L6_2atmpS4288 = _M0L1iS2097 + 1;
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2098, _M0L6_2atmpS4288);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2098, (moonbit_string_t)moonbit_string_literal_32.data);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4289
        = _M0MPC15array5Array2atGsE(_M0L4keysS2095, _M0L1iS2097);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2098, _M0L6_2atmpS4289);
        moonbit_decref(_M0L6_2atmpS4289);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2098, (moonbit_string_t)moonbit_string_literal_33.data);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4287
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2098);
        moonbit_decref(_M0L18_2astring__builderS2098);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS2096, _M0L6_2atmpS4287);
        moonbit_decref(_M0L6_2atmpS4287);
        _M0L6_2atmpS4290 = _M0L1iS2097 + 1;
        _M0L1iS2097 = _M0L6_2atmpS4290;
        continue;
      } else {
        moonbit_decref(_M0L4keysS2095);
      }
      break;
    }
    return _M0L6resultS2096;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_68.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_68.data, 8)
         ) {
    int32_t _M0L6_2atmpS4292;
    moonbit_decref(_M0L7commandS2087);
    #line 142 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4292 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4292 < 2) {
      moonbit_string_t* _M0L6_2atmpS4293;
      struct _M0TPB5ArrayGsE* _block_5218;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4293 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4293[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5218
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5218)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5218->$0 = _M0L6_2atmpS4293;
      _block_5218->$1 = 1;
      return _block_5218;
    } else {
      moonbit_string_t _M0L6_2atmpS4296;
      moonbit_string_t _M0L6_2atmpS4295;
      moonbit_string_t* _M0L6_2atmpS4294;
      struct _M0TPB5ArrayGsE* _block_5219;
      #line 143 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4296 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 143 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4295
      = _M0MP38JIA2JIA29moonbitdb3lib8Database8type__of(_M0L2dbS2088, _M0L6_2atmpS4296);
      moonbit_decref(_M0L6_2atmpS4296);
      _M0L6_2atmpS4294 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4294[0] = _M0L6_2atmpS4295;
      _block_5219
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5219)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5219->$0 = _M0L6_2atmpS4294;
      _block_5219->$1 = 1;
      return _block_5219;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_67.data
           || Moonbit_array_length(_M0L7commandS2087) == 6
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_67.data, 12)
         ) {
    int32_t _M0L6_2atmpS4297;
    moonbit_decref(_M0L7commandS2087);
    #line 146 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4297 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4297 < 3) {
      moonbit_string_t* _M0L6_2atmpS4298;
      struct _M0TPB5ArrayGsE* _block_5216;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4298 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4298[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5216
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5216)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5216->$0 = _M0L6_2atmpS4298;
      _block_5216->$1 = 1;
      return _block_5216;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2100;
      moonbit_string_t _M0L6_2atmpS4302;
      moonbit_string_t _M0L6_2atmpS4303;
      int32_t _M0L6_2atmpS4301;
      moonbit_string_t _M0L6_2atmpS4300;
      moonbit_string_t* _M0L6_2atmpS4299;
      struct _M0TPB5ArrayGsE* _block_5217;
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2100
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2100, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4302 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4303 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4301
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6append(_M0L2dbS2088, _M0L6_2atmpS4302, _M0L6_2atmpS4303);
      moonbit_decref(_M0L6_2atmpS4302);
      moonbit_decref(_M0L6_2atmpS4303);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2100, _M0L6_2atmpS4301);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4300
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2100);
      moonbit_decref(_M0L18_2astring__builderS2100);
      _M0L6_2atmpS4299 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4299[0] = _M0L6_2atmpS4300;
      _block_5217
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5217)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5217->$0 = _M0L6_2atmpS4299;
      _block_5217->$1 = 1;
      return _block_5217;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_66.data
           || Moonbit_array_length(_M0L7commandS2087) == 6
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_66.data, 12)
         ) {
    int32_t _M0L6_2atmpS4304;
    moonbit_decref(_M0L7commandS2087);
    #line 150 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4304 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4304 < 2) {
      moonbit_string_t* _M0L6_2atmpS4305;
      struct _M0TPB5ArrayGsE* _block_5214;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4305 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4305[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5214
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5214)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5214->$0 = _M0L6_2atmpS4305;
      _block_5214->$1 = 1;
      return _block_5214;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2101;
      moonbit_string_t _M0L6_2atmpS4309;
      int32_t _M0L6_2atmpS4308;
      moonbit_string_t _M0L6_2atmpS4307;
      moonbit_string_t* _M0L6_2atmpS4306;
      struct _M0TPB5ArrayGsE* _block_5215;
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2101
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2101, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4309 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4308
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6strlen(_M0L2dbS2088, _M0L6_2atmpS4309);
      moonbit_decref(_M0L6_2atmpS4309);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2101, _M0L6_2atmpS4308);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4307
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2101);
      moonbit_decref(_M0L18_2astring__builderS2101);
      _M0L6_2atmpS4306 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4306[0] = _M0L6_2atmpS4307;
      _block_5215
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5215)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5215->$0 = _M0L6_2atmpS4306;
      _block_5215->$1 = 1;
      return _block_5215;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_65.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_65.data, 8)
         ) {
    int32_t _M0L6_2atmpS4310;
    moonbit_decref(_M0L7commandS2087);
    #line 154 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4310 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4310 < 2) {
      moonbit_string_t* _M0L6_2atmpS4311;
      struct _M0TPB5ArrayGsE* _block_5210;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4311 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4311[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5210
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5210)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5210->$0 = _M0L6_2atmpS4311;
      _block_5210->$1 = 1;
      return _block_5210;
    } else {
      int32_t _M0L1vS2103;
      moonbit_string_t _M0L6_2atmpS4315;
      int64_t _M0L7_2abindS2105;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2104;
      moonbit_string_t _M0L6_2atmpS4313;
      moonbit_string_t* _M0L6_2atmpS4312;
      struct _M0TPB5ArrayGsE* _block_5213;
      #line 156 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4315 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2105
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4incr(_M0L2dbS2088, _M0L6_2atmpS4315);
      moonbit_decref(_M0L6_2atmpS4315);
      if (_M0L7_2abindS2105 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4314 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5212;
        _M0L6_2atmpS4314[0]
        = (moonbit_string_t)moonbit_string_literal_15.data;
        _block_5212
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5212)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5212->$0 = _M0L6_2atmpS4314;
        _block_5212->$1 = 1;
        return _block_5212;
      } else {
        int64_t _M0L7_2aSomeS2106 = _M0L7_2abindS2105;
        int32_t _M0L4_2avS2107 = (int32_t)_M0L7_2aSomeS2106;
        _M0L1vS2103 = _M0L4_2avS2107;
        goto join_2102;
      }
      join_2102:;
      #line 157 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2104
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 157 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2104, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 157 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2104, _M0L1vS2103);
      #line 157 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4313
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2104);
      moonbit_decref(_M0L18_2astring__builderS2104);
      _M0L6_2atmpS4312 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4312[0] = _M0L6_2atmpS4313;
      _block_5213
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5213)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5213->$0 = _M0L6_2atmpS4312;
      _block_5213->$1 = 1;
      return _block_5213;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_64.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_64.data, 8)
         ) {
    int32_t _M0L6_2atmpS4316;
    moonbit_decref(_M0L7commandS2087);
    #line 163 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4316 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4316 < 2) {
      moonbit_string_t* _M0L6_2atmpS4317;
      struct _M0TPB5ArrayGsE* _block_5206;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4317 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4317[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5206
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5206)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5206->$0 = _M0L6_2atmpS4317;
      _block_5206->$1 = 1;
      return _block_5206;
    } else {
      int32_t _M0L1vS2109;
      moonbit_string_t _M0L6_2atmpS4321;
      int64_t _M0L7_2abindS2111;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2110;
      moonbit_string_t _M0L6_2atmpS4319;
      moonbit_string_t* _M0L6_2atmpS4318;
      struct _M0TPB5ArrayGsE* _block_5209;
      #line 165 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4321 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 165 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2111
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4decr(_M0L2dbS2088, _M0L6_2atmpS4321);
      moonbit_decref(_M0L6_2atmpS4321);
      if (_M0L7_2abindS2111 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4320 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5208;
        _M0L6_2atmpS4320[0]
        = (moonbit_string_t)moonbit_string_literal_15.data;
        _block_5208
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5208)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5208->$0 = _M0L6_2atmpS4320;
        _block_5208->$1 = 1;
        return _block_5208;
      } else {
        int64_t _M0L7_2aSomeS2112 = _M0L7_2abindS2111;
        int32_t _M0L4_2avS2113 = (int32_t)_M0L7_2aSomeS2112;
        _M0L1vS2109 = _M0L4_2avS2113;
        goto join_2108;
      }
      join_2108:;
      #line 166 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2110
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 166 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2110, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 166 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2110, _M0L1vS2109);
      #line 166 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4319
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2110);
      moonbit_decref(_M0L18_2astring__builderS2110);
      _M0L6_2atmpS4318 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4318[0] = _M0L6_2atmpS4319;
      _block_5209
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5209)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5209->$0 = _M0L6_2atmpS4318;
      _block_5209->$1 = 1;
      return _block_5209;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_63.data
           || Moonbit_array_length(_M0L7commandS2087) == 6
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_63.data, 12)
         ) {
    int32_t _M0L6_2atmpS4322;
    moonbit_decref(_M0L7commandS2087);
    #line 172 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4322 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4322 < 3) {
      moonbit_string_t* _M0L6_2atmpS4323;
      struct _M0TPB5ArrayGsE* _block_5200;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4323 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4323[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5200
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5200)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5200->$0 = _M0L6_2atmpS4323;
      _block_5200->$1 = 1;
      return _block_5200;
    } else {
      int32_t _M0L1sS2115;
      moonbit_string_t _M0L6_2atmpS4328;
      int64_t _M0L7_2abindS2116;
      moonbit_string_t _M0L6_2atmpS4324;
      int32_t _result_5203;
      #line 174 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4328 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      #line 174 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2116
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl10parse__int(_M0L6_2atmpS4328);
      moonbit_decref(_M0L6_2atmpS4328);
      if (_M0L7_2abindS2116 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4327;
        struct _M0TPB5ArrayGsE* _block_5202;
        moonbit_decref(_M0L5partsS2085);
        _M0L6_2atmpS4327 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4327[0]
        = (moonbit_string_t)moonbit_string_literal_15.data;
        _block_5202
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5202)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5202->$0 = _M0L6_2atmpS4327;
        _block_5202->$1 = 1;
        return _block_5202;
      } else {
        int64_t _M0L7_2aSomeS2117 = _M0L7_2abindS2116;
        int32_t _M0L4_2asS2118 = (int32_t)_M0L7_2aSomeS2117;
        _M0L1sS2115 = _M0L4_2asS2118;
        goto join_2114;
      }
      join_2114:;
      #line 175 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4324 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 175 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5203
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(_M0L2dbS2088, _M0L6_2atmpS4324, _M0L1sS2115);
      moonbit_decref(_M0L6_2atmpS4324);
      if (_result_5203) {
        moonbit_string_t* _M0L6_2atmpS4325 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5204;
        _M0L6_2atmpS4325[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5204
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5204)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5204->$0 = _M0L6_2atmpS4325;
        _block_5204->$1 = 1;
        return _block_5204;
      } else {
        moonbit_string_t* _M0L6_2atmpS4326 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5205;
        _M0L6_2atmpS4326[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5205
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5205)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5205->$0 = _M0L6_2atmpS4326;
        _block_5205->$1 = 1;
        return _block_5205;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_62.data
           || Moonbit_array_length(_M0L7commandS2087) == 3
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_62.data, 6)
         ) {
    int32_t _M0L6_2atmpS4329;
    moonbit_decref(_M0L7commandS2087);
    #line 181 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4329 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4329 < 2) {
      moonbit_string_t* _M0L6_2atmpS4330;
      struct _M0TPB5ArrayGsE* _block_5198;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4330 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4330[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5198
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5198)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5198->$0 = _M0L6_2atmpS4330;
      _block_5198->$1 = 1;
      return _block_5198;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2119;
      moonbit_string_t _M0L6_2atmpS4334;
      int32_t _M0L6_2atmpS4333;
      moonbit_string_t _M0L6_2atmpS4332;
      moonbit_string_t* _M0L6_2atmpS4331;
      struct _M0TPB5ArrayGsE* _block_5199;
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2119
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2119, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4334 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4333
      = _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(_M0L2dbS2088, _M0L6_2atmpS4334);
      moonbit_decref(_M0L6_2atmpS4334);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2119, _M0L6_2atmpS4333);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4332
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2119);
      moonbit_decref(_M0L18_2astring__builderS2119);
      _M0L6_2atmpS4331 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4331[0] = _M0L6_2atmpS4332;
      _block_5199
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5199)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5199->$0 = _M0L6_2atmpS4331;
      _block_5199->$1 = 1;
      return _block_5199;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_61.data
           || Moonbit_array_length(_M0L7commandS2087) == 7
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_61.data, 14)
         ) {
    int32_t _M0L6_2atmpS4335;
    moonbit_decref(_M0L7commandS2087);
    #line 185 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4335 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4335 < 2) {
      moonbit_string_t* _M0L6_2atmpS4336;
      struct _M0TPB5ArrayGsE* _block_5194;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4336 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4336[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5194
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5194)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5194->$0 = _M0L6_2atmpS4336;
      _block_5194->$1 = 1;
      return _block_5194;
    } else {
      moonbit_string_t _M0L6_2atmpS4337;
      int32_t _result_5195;
      #line 186 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4337 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 186 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5195
      = _M0MP38JIA2JIA29moonbitdb3lib8Database7persist(_M0L2dbS2088, _M0L6_2atmpS4337);
      moonbit_decref(_M0L6_2atmpS4337);
      if (_result_5195) {
        moonbit_string_t* _M0L6_2atmpS4338 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5196;
        _M0L6_2atmpS4338[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5196
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5196)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5196->$0 = _M0L6_2atmpS4338;
        _block_5196->$1 = 1;
        return _block_5196;
      } else {
        moonbit_string_t* _M0L6_2atmpS4339 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5197;
        _M0L6_2atmpS4339[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5197
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5197)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5197->$0 = _M0L6_2atmpS4339;
        _block_5197->$1 = 1;
        return _block_5197;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_60.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_60.data, 8)
         ) {
    int32_t _M0L6_2atmpS4340;
    moonbit_decref(_M0L7commandS2087);
    #line 189 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4340 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4340 < 4) {
      moonbit_string_t* _M0L6_2atmpS4341;
      struct _M0TPB5ArrayGsE* _block_5192;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4341 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4341[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5192
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5192)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5192->$0 = _M0L6_2atmpS4341;
      _block_5192->$1 = 1;
      return _block_5192;
    } else {
      moonbit_string_t _M0L6_2atmpS4343;
      moonbit_string_t _M0L6_2atmpS4344;
      moonbit_string_t _M0L6_2atmpS4345;
      int32_t _M0L6_2atmpS4342;
      moonbit_string_t* _M0L6_2atmpS4346;
      struct _M0TPB5ArrayGsE* _block_5193;
      #line 190 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4343 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 190 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4344 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      #line 190 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4345 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 3);
      moonbit_decref(_M0L5partsS2085);
      #line 190 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4342
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS2088, _M0L6_2atmpS4343, _M0L6_2atmpS4344, _M0L6_2atmpS4345);
      moonbit_decref(_M0L6_2atmpS4343);
      moonbit_decref(_M0L6_2atmpS4344);
      moonbit_decref(_M0L6_2atmpS4345);
      _M0L6_2atmpS4346 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4346[0] = (moonbit_string_t)moonbit_string_literal_26.data;
      _block_5193
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5193)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5193->$0 = _M0L6_2atmpS4346;
      _block_5193->$1 = 1;
      return _block_5193;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_59.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_59.data, 8)
         ) {
    int32_t _M0L6_2atmpS4347;
    moonbit_decref(_M0L7commandS2087);
    #line 193 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4347 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4347 < 3) {
      moonbit_string_t* _M0L6_2atmpS4348;
      struct _M0TPB5ArrayGsE* _block_5188;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4348 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4348[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5188
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5188)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5188->$0 = _M0L6_2atmpS4348;
      _block_5188->$1 = 1;
      return _block_5188;
    } else {
      moonbit_string_t _M0L1vS2121;
      moonbit_string_t _M0L6_2atmpS4352;
      moonbit_string_t _M0L6_2atmpS4353;
      moonbit_string_t _M0L7_2abindS2123;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2122;
      moonbit_string_t _M0L6_2atmpS4350;
      moonbit_string_t* _M0L6_2atmpS4349;
      struct _M0TPB5ArrayGsE* _block_5191;
      #line 195 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4352 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 195 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4353 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 195 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2123
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS2088, _M0L6_2atmpS4352, _M0L6_2atmpS4353);
      moonbit_decref(_M0L6_2atmpS4352);
      moonbit_decref(_M0L6_2atmpS4353);
      if (_M0L7_2abindS2123 == 0) {
        moonbit_string_t* _M0L6_2atmpS4351;
        struct _M0TPB5ArrayGsE* _block_5190;
        if (_M0L7_2abindS2123) {
          moonbit_decref(_M0L7_2abindS2123);
        }
        _M0L6_2atmpS4351 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4351[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5190
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5190)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5190->$0 = _M0L6_2atmpS4351;
        _block_5190->$1 = 1;
        return _block_5190;
      } else {
        moonbit_string_t _M0L7_2aSomeS2124 = _M0L7_2abindS2123;
        moonbit_string_t _M0L4_2avS2125 = _M0L7_2aSomeS2124;
        _M0L1vS2121 = _M0L4_2avS2125;
        goto join_2120;
      }
      join_2120:;
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2122
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2122, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2122, _M0L1vS2121);
      moonbit_decref(_M0L1vS2121);
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2122, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4350
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2122);
      moonbit_decref(_M0L18_2astring__builderS2122);
      _M0L6_2atmpS4349 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4349[0] = _M0L6_2atmpS4350;
      _block_5191
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5191)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5191->$0 = _M0L6_2atmpS4349;
      _block_5191->$1 = 1;
      return _block_5191;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_58.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_58.data, 8)
         ) {
    int32_t _M0L6_2atmpS4354;
    moonbit_decref(_M0L7commandS2087);
    #line 202 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4354 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4354 < 3) {
      moonbit_string_t* _M0L6_2atmpS4355;
      struct _M0TPB5ArrayGsE* _block_5184;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4355 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4355[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5184
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5184)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5184->$0 = _M0L6_2atmpS4355;
      _block_5184->$1 = 1;
      return _block_5184;
    } else {
      moonbit_string_t _M0L6_2atmpS4356;
      moonbit_string_t _M0L6_2atmpS4357;
      int32_t _result_5185;
      #line 203 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4356 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 203 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4357 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 203 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5185
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hdel(_M0L2dbS2088, _M0L6_2atmpS4356, _M0L6_2atmpS4357);
      moonbit_decref(_M0L6_2atmpS4356);
      moonbit_decref(_M0L6_2atmpS4357);
      if (_result_5185) {
        moonbit_string_t* _M0L6_2atmpS4358 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5186;
        _M0L6_2atmpS4358[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5186
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5186)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5186->$0 = _M0L6_2atmpS4358;
        _block_5186->$1 = 1;
        return _block_5186;
      } else {
        moonbit_string_t* _M0L6_2atmpS4359 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5187;
        _M0L6_2atmpS4359[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5187
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5187)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5187->$0 = _M0L6_2atmpS4359;
        _block_5187->$1 = 1;
        return _block_5187;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_57.data
           || Moonbit_array_length(_M0L7commandS2087) == 7
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_57.data, 14)
         ) {
    int32_t _M0L6_2atmpS4360;
    moonbit_decref(_M0L7commandS2087);
    #line 206 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4360 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4360 < 2) {
      moonbit_string_t* _M0L6_2atmpS4361;
      struct _M0TPB5ArrayGsE* _block_5181;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4361 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4361[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5181
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5181)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5181->$0 = _M0L6_2atmpS4361;
      _block_5181->$1 = 1;
      return _block_5181;
    } else {
      moonbit_string_t _M0L6_2atmpS4370;
      struct _M0TPB3MapGssE* _M0L3allS2126;
      moonbit_string_t* _M0L6_2atmpS4369;
      struct _M0TPB5ArrayGsE* _M0L6resultS2127;
      struct _M0TPB8MutLocalGiE* _M0L3idxS2128;
      struct _M0TPB4IterGUssEE* _M0L5_2aitS2129;
      #line 208 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4370 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 208 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L3allS2126
      = _M0MP38JIA2JIA29moonbitdb3lib8Database7hgetall(_M0L2dbS2088, _M0L6_2atmpS4370);
      moonbit_decref(_M0L6_2atmpS4370);
      _M0L6_2atmpS4369 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L6resultS2127
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L6resultS2127)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _M0L6resultS2127->$0 = _M0L6_2atmpS4369;
      _M0L6resultS2127->$1 = 0;
      _M0L3idxS2128
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L3idxS2128)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L3idxS2128->$0 = 1;
      #line 210 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L5_2aitS2129 = _M0MPB3Map5iter2GssE(_M0L3allS2126);
      moonbit_decref(_M0L3allS2126);
      while (1) {
        moonbit_string_t _M0L1fS2131;
        moonbit_string_t _M0L1vS2132;
        struct _M0TUssE* _M0L7_2abindS2136;
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2133;
        int32_t _M0L3valS4363;
        moonbit_string_t _M0L6_2atmpS4362;
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2134;
        int32_t _M0L3valS4366;
        int32_t _M0L6_2atmpS4365;
        moonbit_string_t _M0L6_2atmpS4364;
        int32_t _M0L3valS4368;
        int32_t _M0L6_2atmpS4367;
        #line 211 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L7_2abindS2136 = _M0MPB5Iter24nextGssE(_M0L5_2aitS2129);
        if (_M0L7_2abindS2136 == 0) {
          if (_M0L7_2abindS2136) {
            moonbit_decref(_M0L7_2abindS2136);
          }
          moonbit_decref(_M0L5_2aitS2129);
          moonbit_decref(_M0L3idxS2128);
        } else {
          struct _M0TUssE* _M0L7_2aSomeS2137 = _M0L7_2abindS2136;
          struct _M0TUssE* _M0L4_2axS2138 = _M0L7_2aSomeS2137;
          moonbit_string_t _M0L4_2afS2139 = _M0L4_2axS2138->$0;
          moonbit_string_t _M0L8_2afieldS4556 = _M0L4_2axS2138->$1;
          int32_t _M0L6_2acntS5015 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2138));
          moonbit_string_t _M0L4_2avS2140;
          if (_M0L6_2acntS5015 > 1) {
            int32_t _M0L11_2anew__cntS5016 = _M0L6_2acntS5015 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2138), _M0L11_2anew__cntS5016);
            moonbit_incref(_M0L8_2afieldS4556);
            moonbit_incref(_M0L4_2afS2139);
          } else if (_M0L6_2acntS5015 == 1) {
            #line 211 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            moonbit_free(_M0L4_2axS2138);
          }
          _M0L4_2avS2140 = _M0L8_2afieldS4556;
          _M0L1fS2131 = _M0L4_2afS2139;
          _M0L1vS2132 = _M0L4_2avS2140;
          goto join_2130;
        }
        goto joinlet_5183;
        join_2130:;
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L18_2astring__builderS2133
        = _M0MPB13StringBuilder21StringBuilder_2einner(6);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2133, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L3valS4363 = _M0L3idxS2128->$0;
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2133, _M0L3valS4363);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2133, (moonbit_string_t)moonbit_string_literal_32.data);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2133, _M0L1fS2131);
        moonbit_decref(_M0L1fS2131);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2133, (moonbit_string_t)moonbit_string_literal_33.data);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4362
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2133);
        moonbit_decref(_M0L18_2astring__builderS2133);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS2127, _M0L6_2atmpS4362);
        moonbit_decref(_M0L6_2atmpS4362);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L18_2astring__builderS2134
        = _M0MPB13StringBuilder21StringBuilder_2einner(6);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2134, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L3valS4366 = _M0L3idxS2128->$0;
        _M0L6_2atmpS4365 = _M0L3valS4366 + 1;
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2134, _M0L6_2atmpS4365);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2134, (moonbit_string_t)moonbit_string_literal_32.data);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2134, _M0L1vS2132);
        moonbit_decref(_M0L1vS2132);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2134, (moonbit_string_t)moonbit_string_literal_33.data);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4364
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2134);
        moonbit_decref(_M0L18_2astring__builderS2134);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS2127, _M0L6_2atmpS4364);
        moonbit_decref(_M0L6_2atmpS4364);
        _M0L3valS4368 = _M0L3idxS2128->$0;
        _M0L6_2atmpS4367 = _M0L3valS4368 + 2;
        _M0L3idxS2128->$0 = _M0L6_2atmpS4367;
        continue;
        joinlet_5183:;
        break;
      }
      return _M0L6resultS2127;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_56.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_56.data, 8)
         ) {
    int32_t _M0L6_2atmpS4371;
    moonbit_decref(_M0L7commandS2087);
    #line 220 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4371 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4371 < 2) {
      moonbit_string_t* _M0L6_2atmpS4372;
      struct _M0TPB5ArrayGsE* _block_5179;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4372 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4372[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5179
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5179)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5179->$0 = _M0L6_2atmpS4372;
      _block_5179->$1 = 1;
      return _block_5179;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2141;
      moonbit_string_t _M0L6_2atmpS4376;
      int32_t _M0L6_2atmpS4375;
      moonbit_string_t _M0L6_2atmpS4374;
      moonbit_string_t* _M0L6_2atmpS4373;
      struct _M0TPB5ArrayGsE* _block_5180;
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2141
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2141, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4376 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4375
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hlen(_M0L2dbS2088, _M0L6_2atmpS4376);
      moonbit_decref(_M0L6_2atmpS4376);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2141, _M0L6_2atmpS4375);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4374
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2141);
      moonbit_decref(_M0L18_2astring__builderS2141);
      _M0L6_2atmpS4373 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4373[0] = _M0L6_2atmpS4374;
      _block_5180
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5180)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5180->$0 = _M0L6_2atmpS4373;
      _block_5180->$1 = 1;
      return _block_5180;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_55.data
           || Moonbit_array_length(_M0L7commandS2087) == 5
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_55.data, 10)
         ) {
    int32_t _M0L6_2atmpS4377;
    moonbit_decref(_M0L7commandS2087);
    #line 224 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4377 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4377 < 3) {
      moonbit_string_t* _M0L6_2atmpS4378;
      struct _M0TPB5ArrayGsE* _block_5177;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4378 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4378[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5177
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5177)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5177->$0 = _M0L6_2atmpS4378;
      _block_5177->$1 = 1;
      return _block_5177;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2142;
      moonbit_string_t _M0L6_2atmpS4382;
      moonbit_string_t _M0L6_2atmpS4383;
      int32_t _M0L6_2atmpS4381;
      moonbit_string_t _M0L6_2atmpS4380;
      moonbit_string_t* _M0L6_2atmpS4379;
      struct _M0TPB5ArrayGsE* _block_5178;
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2142
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2142, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4382 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4383 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4381
      = _M0MP38JIA2JIA29moonbitdb3lib8Database5lpush(_M0L2dbS2088, _M0L6_2atmpS4382, _M0L6_2atmpS4383);
      moonbit_decref(_M0L6_2atmpS4382);
      moonbit_decref(_M0L6_2atmpS4383);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2142, _M0L6_2atmpS4381);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4380
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2142);
      moonbit_decref(_M0L18_2astring__builderS2142);
      _M0L6_2atmpS4379 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4379[0] = _M0L6_2atmpS4380;
      _block_5178
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5178)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5178->$0 = _M0L6_2atmpS4379;
      _block_5178->$1 = 1;
      return _block_5178;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_54.data
           || Moonbit_array_length(_M0L7commandS2087) == 5
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_54.data, 10)
         ) {
    int32_t _M0L6_2atmpS4384;
    moonbit_decref(_M0L7commandS2087);
    #line 228 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4384 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4384 < 3) {
      moonbit_string_t* _M0L6_2atmpS4385;
      struct _M0TPB5ArrayGsE* _block_5175;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4385 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4385[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5175
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5175)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5175->$0 = _M0L6_2atmpS4385;
      _block_5175->$1 = 1;
      return _block_5175;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2143;
      moonbit_string_t _M0L6_2atmpS4389;
      moonbit_string_t _M0L6_2atmpS4390;
      int32_t _M0L6_2atmpS4388;
      moonbit_string_t _M0L6_2atmpS4387;
      moonbit_string_t* _M0L6_2atmpS4386;
      struct _M0TPB5ArrayGsE* _block_5176;
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2143
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2143, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4389 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4390 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4388
      = _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(_M0L2dbS2088, _M0L6_2atmpS4389, _M0L6_2atmpS4390);
      moonbit_decref(_M0L6_2atmpS4389);
      moonbit_decref(_M0L6_2atmpS4390);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2143, _M0L6_2atmpS4388);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4387
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2143);
      moonbit_decref(_M0L18_2astring__builderS2143);
      _M0L6_2atmpS4386 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4386[0] = _M0L6_2atmpS4387;
      _block_5176
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5176)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5176->$0 = _M0L6_2atmpS4386;
      _block_5176->$1 = 1;
      return _block_5176;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_53.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_53.data, 8)
         ) {
    int32_t _M0L6_2atmpS4391;
    moonbit_decref(_M0L7commandS2087);
    #line 232 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4391 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4391 < 2) {
      moonbit_string_t* _M0L6_2atmpS4392;
      struct _M0TPB5ArrayGsE* _block_5171;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4392 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4392[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5171
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5171)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5171->$0 = _M0L6_2atmpS4392;
      _block_5171->$1 = 1;
      return _block_5171;
    } else {
      moonbit_string_t _M0L1vS2145;
      moonbit_string_t _M0L6_2atmpS4396;
      moonbit_string_t _M0L7_2abindS2147;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2146;
      moonbit_string_t _M0L6_2atmpS4394;
      moonbit_string_t* _M0L6_2atmpS4393;
      struct _M0TPB5ArrayGsE* _block_5174;
      #line 234 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4396 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 234 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2147
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4lpop(_M0L2dbS2088, _M0L6_2atmpS4396);
      moonbit_decref(_M0L6_2atmpS4396);
      if (_M0L7_2abindS2147 == 0) {
        moonbit_string_t* _M0L6_2atmpS4395;
        struct _M0TPB5ArrayGsE* _block_5173;
        if (_M0L7_2abindS2147) {
          moonbit_decref(_M0L7_2abindS2147);
        }
        _M0L6_2atmpS4395 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4395[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5173
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5173)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5173->$0 = _M0L6_2atmpS4395;
        _block_5173->$1 = 1;
        return _block_5173;
      } else {
        moonbit_string_t _M0L7_2aSomeS2148 = _M0L7_2abindS2147;
        moonbit_string_t _M0L4_2avS2149 = _M0L7_2aSomeS2148;
        _M0L1vS2145 = _M0L4_2avS2149;
        goto join_2144;
      }
      join_2144:;
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2146
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2146, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2146, _M0L1vS2145);
      moonbit_decref(_M0L1vS2145);
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2146, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4394
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2146);
      moonbit_decref(_M0L18_2astring__builderS2146);
      _M0L6_2atmpS4393 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4393[0] = _M0L6_2atmpS4394;
      _block_5174
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5174)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5174->$0 = _M0L6_2atmpS4393;
      _block_5174->$1 = 1;
      return _block_5174;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_52.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_52.data, 8)
         ) {
    int32_t _M0L6_2atmpS4397;
    moonbit_decref(_M0L7commandS2087);
    #line 241 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4397 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4397 < 2) {
      moonbit_string_t* _M0L6_2atmpS4398;
      struct _M0TPB5ArrayGsE* _block_5167;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4398 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4398[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5167
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5167)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5167->$0 = _M0L6_2atmpS4398;
      _block_5167->$1 = 1;
      return _block_5167;
    } else {
      moonbit_string_t _M0L1vS2151;
      moonbit_string_t _M0L6_2atmpS4402;
      moonbit_string_t _M0L7_2abindS2153;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2152;
      moonbit_string_t _M0L6_2atmpS4400;
      moonbit_string_t* _M0L6_2atmpS4399;
      struct _M0TPB5ArrayGsE* _block_5170;
      #line 243 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4402 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 243 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2153
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4rpop(_M0L2dbS2088, _M0L6_2atmpS4402);
      moonbit_decref(_M0L6_2atmpS4402);
      if (_M0L7_2abindS2153 == 0) {
        moonbit_string_t* _M0L6_2atmpS4401;
        struct _M0TPB5ArrayGsE* _block_5169;
        if (_M0L7_2abindS2153) {
          moonbit_decref(_M0L7_2abindS2153);
        }
        _M0L6_2atmpS4401 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4401[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5169
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5169)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5169->$0 = _M0L6_2atmpS4401;
        _block_5169->$1 = 1;
        return _block_5169;
      } else {
        moonbit_string_t _M0L7_2aSomeS2154 = _M0L7_2abindS2153;
        moonbit_string_t _M0L4_2avS2155 = _M0L7_2aSomeS2154;
        _M0L1vS2151 = _M0L4_2avS2155;
        goto join_2150;
      }
      join_2150:;
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2152
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2152, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2152, _M0L1vS2151);
      moonbit_decref(_M0L1vS2151);
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2152, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4400
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2152);
      moonbit_decref(_M0L18_2astring__builderS2152);
      _M0L6_2atmpS4399 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4399[0] = _M0L6_2atmpS4400;
      _block_5170
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5170)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5170->$0 = _M0L6_2atmpS4399;
      _block_5170->$1 = 1;
      return _block_5170;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_51.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_51.data, 8)
         ) {
    int32_t _M0L6_2atmpS4403;
    moonbit_decref(_M0L7commandS2087);
    #line 250 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4403 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4403 < 2) {
      moonbit_string_t* _M0L6_2atmpS4404;
      struct _M0TPB5ArrayGsE* _block_5165;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4404 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4404[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5165
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5165)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5165->$0 = _M0L6_2atmpS4404;
      _block_5165->$1 = 1;
      return _block_5165;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2156;
      moonbit_string_t _M0L6_2atmpS4408;
      int32_t _M0L6_2atmpS4407;
      moonbit_string_t _M0L6_2atmpS4406;
      moonbit_string_t* _M0L6_2atmpS4405;
      struct _M0TPB5ArrayGsE* _block_5166;
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2156
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2156, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4408 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4407
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(_M0L2dbS2088, _M0L6_2atmpS4408);
      moonbit_decref(_M0L6_2atmpS4408);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2156, _M0L6_2atmpS4407);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4406
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2156);
      moonbit_decref(_M0L18_2astring__builderS2156);
      _M0L6_2atmpS4405 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4405[0] = _M0L6_2atmpS4406;
      _block_5166
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5166)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5166->$0 = _M0L6_2atmpS4405;
      _block_5166->$1 = 1;
      return _block_5166;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_50.data
           || Moonbit_array_length(_M0L7commandS2087) == 6
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_50.data, 12)
         ) {
    int32_t _M0L6_2atmpS4409;
    moonbit_decref(_M0L7commandS2087);
    #line 254 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4409 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4409 < 4) {
      moonbit_string_t* _M0L6_2atmpS4410;
      struct _M0TPB5ArrayGsE* _block_5160;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4410 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4410[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5160
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5160)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5160->$0 = _M0L6_2atmpS4410;
      _block_5160->$1 = 1;
      return _block_5160;
    } else {
      int32_t _M0L2svS2159;
      int32_t _M0L2evS2160;
      moonbit_string_t _M0L6_2atmpS4420;
      int64_t _M0L7_2abindS2166;
      moonbit_string_t _M0L6_2atmpS4419;
      int64_t _M0L7_2abindS2167;
      moonbit_string_t _M0L6_2atmpS4418;
      struct _M0TPB5ArrayGsE* _M0L5itemsS2161;
      moonbit_string_t* _M0L6_2atmpS4417;
      struct _M0TPB5ArrayGsE* _M0L6resultS2162;
      int32_t _M0L1iS2163;
      moonbit_string_t* _M0L6_2atmpS4411;
      struct _M0TPB5ArrayGsE* _block_5164;
      #line 256 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4420 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      #line 256 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2166
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl10parse__int(_M0L6_2atmpS4420);
      moonbit_decref(_M0L6_2atmpS4420);
      #line 256 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4419 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 3);
      #line 256 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2167
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl10parse__int(_M0L6_2atmpS4419);
      moonbit_decref(_M0L6_2atmpS4419);
      if (_M0L7_2abindS2166 == 4294967296ll) {
        moonbit_decref(_M0L5partsS2085);
        goto join_2157;
      } else {
        int64_t _M0L7_2aSomeS2168 = _M0L7_2abindS2166;
        int32_t _M0L5_2asvS2169 = (int32_t)_M0L7_2aSomeS2168;
        if (_M0L7_2abindS2167 == 4294967296ll) {
          moonbit_decref(_M0L5partsS2085);
          goto join_2157;
        } else {
          int64_t _M0L7_2aSomeS2170 = _M0L7_2abindS2167;
          int32_t _M0L5_2aevS2171 = (int32_t)_M0L7_2aSomeS2170;
          _M0L2svS2159 = _M0L5_2asvS2169;
          _M0L2evS2160 = _M0L5_2aevS2171;
          goto join_2158;
        }
      }
      join_2158:;
      #line 258 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4418 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 258 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L5itemsS2161
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6lrange(_M0L2dbS2088, _M0L6_2atmpS4418, _M0L2svS2159, _M0L2evS2160);
      moonbit_decref(_M0L6_2atmpS4418);
      _M0L6_2atmpS4417 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L6resultS2162
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L6resultS2162)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _M0L6resultS2162->$0 = _M0L6_2atmpS4417;
      _M0L6resultS2162->$1 = 0;
      _M0L1iS2163 = 0;
      while (1) {
        int32_t _M0L6_2atmpS4412;
        #line 260 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4412 = _M0MPC15array5Array6lengthGsE(_M0L5itemsS2161);
        if (_M0L1iS2163 < _M0L6_2atmpS4412) {
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2164;
          int32_t _M0L6_2atmpS4414;
          moonbit_string_t _M0L6_2atmpS4415;
          moonbit_string_t _M0L6_2atmpS4413;
          int32_t _M0L6_2atmpS4416;
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2164
          = _M0MPB13StringBuilder21StringBuilder_2einner(6);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2164, (moonbit_string_t)moonbit_string_literal_23.data);
          _M0L6_2atmpS4414 = _M0L1iS2163 + 1;
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2164, _M0L6_2atmpS4414);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2164, (moonbit_string_t)moonbit_string_literal_32.data);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4415
          = _M0MPC15array5Array2atGsE(_M0L5itemsS2161, _M0L1iS2163);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2164, _M0L6_2atmpS4415);
          moonbit_decref(_M0L6_2atmpS4415);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2164, (moonbit_string_t)moonbit_string_literal_33.data);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4413
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2164);
          moonbit_decref(_M0L18_2astring__builderS2164);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPC15array5Array4pushGsE(_M0L6resultS2162, _M0L6_2atmpS4413);
          moonbit_decref(_M0L6_2atmpS4413);
          _M0L6_2atmpS4416 = _M0L1iS2163 + 1;
          _M0L1iS2163 = _M0L6_2atmpS4416;
          continue;
        } else {
          moonbit_decref(_M0L5itemsS2161);
        }
        break;
      }
      return _M0L6resultS2162;
      join_2157:;
      _M0L6_2atmpS4411 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4411[0] = (moonbit_string_t)moonbit_string_literal_15.data;
      _block_5164
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5164)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5164->$0 = _M0L6_2atmpS4411;
      _block_5164->$1 = 1;
      return _block_5164;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_49.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_49.data, 8)
         ) {
    int32_t _M0L6_2atmpS4421;
    moonbit_decref(_M0L7commandS2087);
    #line 270 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4421 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4421 < 3) {
      moonbit_string_t* _M0L6_2atmpS4422;
      struct _M0TPB5ArrayGsE* _block_5156;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4422 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4422[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5156
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5156)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5156->$0 = _M0L6_2atmpS4422;
      _block_5156->$1 = 1;
      return _block_5156;
    } else {
      moonbit_string_t _M0L6_2atmpS4423;
      moonbit_string_t _M0L6_2atmpS4424;
      int32_t _result_5157;
      #line 271 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4423 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 271 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4424 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 271 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5157
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS2088, _M0L6_2atmpS4423, _M0L6_2atmpS4424);
      moonbit_decref(_M0L6_2atmpS4423);
      moonbit_decref(_M0L6_2atmpS4424);
      if (_result_5157) {
        moonbit_string_t* _M0L6_2atmpS4425 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5158;
        _M0L6_2atmpS4425[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5158
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5158)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5158->$0 = _M0L6_2atmpS4425;
        _block_5158->$1 = 1;
        return _block_5158;
      } else {
        moonbit_string_t* _M0L6_2atmpS4426 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5159;
        _M0L6_2atmpS4426[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5159
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5159)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5159->$0 = _M0L6_2atmpS4426;
        _block_5159->$1 = 1;
        return _block_5159;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_48.data
           || Moonbit_array_length(_M0L7commandS2087) == 8
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_48.data, 16)
         ) {
    int32_t _M0L6_2atmpS4427;
    moonbit_decref(_M0L7commandS2087);
    #line 274 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4427 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4427 < 2) {
      moonbit_string_t* _M0L6_2atmpS4428;
      struct _M0TPB5ArrayGsE* _block_5154;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4428 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4428[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5154
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5154)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5154->$0 = _M0L6_2atmpS4428;
      _block_5154->$1 = 1;
      return _block_5154;
    } else {
      moonbit_string_t _M0L6_2atmpS4435;
      struct _M0TPB5ArrayGsE* _M0L7membersS2172;
      moonbit_string_t* _M0L6_2atmpS4434;
      struct _M0TPB5ArrayGsE* _M0L6resultS2173;
      int32_t _M0L1iS2174;
      #line 276 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4435 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 276 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7membersS2172
      = _M0MP38JIA2JIA29moonbitdb3lib8Database8smembers(_M0L2dbS2088, _M0L6_2atmpS4435);
      moonbit_decref(_M0L6_2atmpS4435);
      _M0L6_2atmpS4434 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L6resultS2173
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L6resultS2173)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _M0L6resultS2173->$0 = _M0L6_2atmpS4434;
      _M0L6resultS2173->$1 = 0;
      _M0L1iS2174 = 0;
      while (1) {
        int32_t _M0L6_2atmpS4429;
        #line 278 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4429 = _M0MPC15array5Array6lengthGsE(_M0L7membersS2172);
        if (_M0L1iS2174 < _M0L6_2atmpS4429) {
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2175;
          int32_t _M0L6_2atmpS4431;
          moonbit_string_t _M0L6_2atmpS4432;
          moonbit_string_t _M0L6_2atmpS4430;
          int32_t _M0L6_2atmpS4433;
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2175
          = _M0MPB13StringBuilder21StringBuilder_2einner(6);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2175, (moonbit_string_t)moonbit_string_literal_23.data);
          _M0L6_2atmpS4431 = _M0L1iS2174 + 1;
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2175, _M0L6_2atmpS4431);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2175, (moonbit_string_t)moonbit_string_literal_32.data);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4432
          = _M0MPC15array5Array2atGsE(_M0L7membersS2172, _M0L1iS2174);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2175, _M0L6_2atmpS4432);
          moonbit_decref(_M0L6_2atmpS4432);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2175, (moonbit_string_t)moonbit_string_literal_33.data);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4430
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2175);
          moonbit_decref(_M0L18_2astring__builderS2175);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPC15array5Array4pushGsE(_M0L6resultS2173, _M0L6_2atmpS4430);
          moonbit_decref(_M0L6_2atmpS4430);
          _M0L6_2atmpS4433 = _M0L1iS2174 + 1;
          _M0L1iS2174 = _M0L6_2atmpS4433;
          continue;
        } else {
          moonbit_decref(_M0L7membersS2172);
        }
        break;
      }
      return _M0L6resultS2173;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_47.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_47.data, 8)
         ) {
    int32_t _M0L6_2atmpS4436;
    moonbit_decref(_M0L7commandS2087);
    #line 285 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4436 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4436 < 3) {
      moonbit_string_t* _M0L6_2atmpS4437;
      struct _M0TPB5ArrayGsE* _block_5150;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4437 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4437[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5150
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5150)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5150->$0 = _M0L6_2atmpS4437;
      _block_5150->$1 = 1;
      return _block_5150;
    } else {
      moonbit_string_t _M0L6_2atmpS4438;
      moonbit_string_t _M0L6_2atmpS4439;
      int32_t _result_5151;
      #line 286 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4438 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 286 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4439 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 286 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5151
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4srem(_M0L2dbS2088, _M0L6_2atmpS4438, _M0L6_2atmpS4439);
      moonbit_decref(_M0L6_2atmpS4438);
      moonbit_decref(_M0L6_2atmpS4439);
      if (_result_5151) {
        moonbit_string_t* _M0L6_2atmpS4440 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5152;
        _M0L6_2atmpS4440[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5152
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5152)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5152->$0 = _M0L6_2atmpS4440;
        _block_5152->$1 = 1;
        return _block_5152;
      } else {
        moonbit_string_t* _M0L6_2atmpS4441 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5153;
        _M0L6_2atmpS4441[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5153
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5153)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5153->$0 = _M0L6_2atmpS4441;
        _block_5153->$1 = 1;
        return _block_5153;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_46.data
           || Moonbit_array_length(_M0L7commandS2087) == 5
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_46.data, 10)
         ) {
    int32_t _M0L6_2atmpS4442;
    moonbit_decref(_M0L7commandS2087);
    #line 289 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4442 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4442 < 2) {
      moonbit_string_t* _M0L6_2atmpS4443;
      struct _M0TPB5ArrayGsE* _block_5148;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4443 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4443[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5148
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5148)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5148->$0 = _M0L6_2atmpS4443;
      _block_5148->$1 = 1;
      return _block_5148;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2177;
      moonbit_string_t _M0L6_2atmpS4447;
      int32_t _M0L6_2atmpS4446;
      moonbit_string_t _M0L6_2atmpS4445;
      moonbit_string_t* _M0L6_2atmpS4444;
      struct _M0TPB5ArrayGsE* _block_5149;
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2177
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2177, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4447 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4446
      = _M0MP38JIA2JIA29moonbitdb3lib8Database5scard(_M0L2dbS2088, _M0L6_2atmpS4447);
      moonbit_decref(_M0L6_2atmpS4447);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2177, _M0L6_2atmpS4446);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4445
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2177);
      moonbit_decref(_M0L18_2astring__builderS2177);
      _M0L6_2atmpS4444 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4444[0] = _M0L6_2atmpS4445;
      _block_5149
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5149)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5149->$0 = _M0L6_2atmpS4444;
      _block_5149->$1 = 1;
      return _block_5149;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_45.data
           || Moonbit_array_length(_M0L7commandS2087) == 9
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_45.data, 18)
         ) {
    int32_t _M0L6_2atmpS4448;
    moonbit_decref(_M0L7commandS2087);
    #line 293 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4448 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4448 < 3) {
      moonbit_string_t* _M0L6_2atmpS4449;
      struct _M0TPB5ArrayGsE* _block_5144;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4449 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4449[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5144
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5144)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5144->$0 = _M0L6_2atmpS4449;
      _block_5144->$1 = 1;
      return _block_5144;
    } else {
      moonbit_string_t _M0L6_2atmpS4450;
      moonbit_string_t _M0L6_2atmpS4451;
      int32_t _result_5145;
      #line 294 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4450 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 294 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4451 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 294 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5145
      = _M0MP38JIA2JIA29moonbitdb3lib8Database9sismember(_M0L2dbS2088, _M0L6_2atmpS4450, _M0L6_2atmpS4451);
      moonbit_decref(_M0L6_2atmpS4450);
      moonbit_decref(_M0L6_2atmpS4451);
      if (_result_5145) {
        moonbit_string_t* _M0L6_2atmpS4452 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5146;
        _M0L6_2atmpS4452[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5146
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5146)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5146->$0 = _M0L6_2atmpS4452;
        _block_5146->$1 = 1;
        return _block_5146;
      } else {
        moonbit_string_t* _M0L6_2atmpS4453 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5147;
        _M0L6_2atmpS4453[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5147
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5147)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5147->$0 = _M0L6_2atmpS4453;
        _block_5147->$1 = 1;
        return _block_5147;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_42.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_42.data, 8)
         ) {
    int32_t _M0L6_2atmpS4454;
    moonbit_decref(_M0L7commandS2087);
    #line 297 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4454 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4454 < 4) {
      moonbit_string_t* _M0L6_2atmpS4455;
      struct _M0TPB5ArrayGsE* _block_5138;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4455 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4455[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5138
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5138)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5138->$0 = _M0L6_2atmpS4455;
      _block_5138->$1 = 1;
      return _block_5138;
    } else {
      float _M0L1sS2179;
      moonbit_string_t _M0L6_2atmpS4461;
      void* _M0L7_2abindS2180;
      moonbit_string_t _M0L6_2atmpS4456;
      moonbit_string_t _M0L6_2atmpS4457;
      int32_t _result_5141;
      #line 299 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4461 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      #line 299 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2180
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl12parse__float(_M0L6_2atmpS4461);
      moonbit_decref(_M0L6_2atmpS4461);
      switch (Moonbit_object_tag(_M0L7_2abindS2180)) {
        case 1: {
          struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2181 =
            (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS2180;
          float _M0L4_2asS2182 = _M0L7_2aSomeS2181->$0;
          moonbit_decref(_M0L7_2aSomeS2181);
          _M0L1sS2179 = _M0L4_2asS2182;
          goto join_2178;
          break;
        }
        default: {
          moonbit_string_t* _M0L6_2atmpS4460;
          struct _M0TPB5ArrayGsE* _block_5140;
          moonbit_decref(_M0L7_2abindS2180);
          moonbit_decref(_M0L5partsS2085);
          _M0L6_2atmpS4460 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
          _M0L6_2atmpS4460[0]
          = (moonbit_string_t)moonbit_string_literal_36.data;
          _block_5140
          = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
          Moonbit_object_header(_block_5140)->meta
          = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
          _block_5140->$0 = _M0L6_2atmpS4460;
          _block_5140->$1 = 1;
          return _block_5140;
          break;
        }
      }
      join_2178:;
      #line 300 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4456 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 300 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4457 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 3);
      moonbit_decref(_M0L5partsS2085);
      #line 300 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5141
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS2088, _M0L6_2atmpS4456, _M0L1sS2179, _M0L6_2atmpS4457);
      moonbit_decref(_M0L6_2atmpS4456);
      moonbit_decref(_M0L6_2atmpS4457);
      if (_result_5141) {
        moonbit_string_t* _M0L6_2atmpS4458 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5142;
        _M0L6_2atmpS4458[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5142
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5142)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5142->$0 = _M0L6_2atmpS4458;
        _block_5142->$1 = 1;
        return _block_5142;
      } else {
        moonbit_string_t* _M0L6_2atmpS4459 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5143;
        _M0L6_2atmpS4459[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5143
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5143)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5143->$0 = _M0L6_2atmpS4459;
        _block_5143->$1 = 1;
        return _block_5143;
      }
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_41.data
           || Moonbit_array_length(_M0L7commandS2087) == 6
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_41.data, 12)
         ) {
    int32_t _M0L6_2atmpS4462;
    moonbit_decref(_M0L7commandS2087);
    #line 306 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4462 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4462 < 4) {
      moonbit_string_t* _M0L6_2atmpS4463;
      struct _M0TPB5ArrayGsE* _block_5133;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4463 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4463[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5133
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5133)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5133->$0 = _M0L6_2atmpS4463;
      _block_5133->$1 = 1;
      return _block_5133;
    } else {
      int32_t _M0L2svS2185;
      int32_t _M0L2evS2186;
      moonbit_string_t _M0L6_2atmpS4473;
      int64_t _M0L7_2abindS2192;
      moonbit_string_t _M0L6_2atmpS4472;
      int64_t _M0L7_2abindS2193;
      moonbit_string_t _M0L6_2atmpS4471;
      struct _M0TPB5ArrayGsE* _M0L5itemsS2187;
      moonbit_string_t* _M0L6_2atmpS4470;
      struct _M0TPB5ArrayGsE* _M0L6resultS2188;
      int32_t _M0L1iS2189;
      moonbit_string_t* _M0L6_2atmpS4464;
      struct _M0TPB5ArrayGsE* _block_5137;
      #line 308 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4473 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      #line 308 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2192
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl10parse__int(_M0L6_2atmpS4473);
      moonbit_decref(_M0L6_2atmpS4473);
      #line 308 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4472 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 3);
      #line 308 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2193
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl10parse__int(_M0L6_2atmpS4472);
      moonbit_decref(_M0L6_2atmpS4472);
      if (_M0L7_2abindS2192 == 4294967296ll) {
        moonbit_decref(_M0L5partsS2085);
        goto join_2183;
      } else {
        int64_t _M0L7_2aSomeS2194 = _M0L7_2abindS2192;
        int32_t _M0L5_2asvS2195 = (int32_t)_M0L7_2aSomeS2194;
        if (_M0L7_2abindS2193 == 4294967296ll) {
          moonbit_decref(_M0L5partsS2085);
          goto join_2183;
        } else {
          int64_t _M0L7_2aSomeS2196 = _M0L7_2abindS2193;
          int32_t _M0L5_2aevS2197 = (int32_t)_M0L7_2aSomeS2196;
          _M0L2svS2185 = _M0L5_2asvS2195;
          _M0L2evS2186 = _M0L5_2aevS2197;
          goto join_2184;
        }
      }
      join_2184:;
      #line 310 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4471 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 310 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L5itemsS2187
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6zrange(_M0L2dbS2088, _M0L6_2atmpS4471, _M0L2svS2185, _M0L2evS2186);
      moonbit_decref(_M0L6_2atmpS4471);
      _M0L6_2atmpS4470 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L6resultS2188
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L6resultS2188)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _M0L6resultS2188->$0 = _M0L6_2atmpS4470;
      _M0L6resultS2188->$1 = 0;
      _M0L1iS2189 = 0;
      while (1) {
        int32_t _M0L6_2atmpS4465;
        #line 312 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4465 = _M0MPC15array5Array6lengthGsE(_M0L5itemsS2187);
        if (_M0L1iS2189 < _M0L6_2atmpS4465) {
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2190;
          int32_t _M0L6_2atmpS4467;
          moonbit_string_t _M0L6_2atmpS4468;
          moonbit_string_t _M0L6_2atmpS4466;
          int32_t _M0L6_2atmpS4469;
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2190
          = _M0MPB13StringBuilder21StringBuilder_2einner(6);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2190, (moonbit_string_t)moonbit_string_literal_23.data);
          _M0L6_2atmpS4467 = _M0L1iS2189 + 1;
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2190, _M0L6_2atmpS4467);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2190, (moonbit_string_t)moonbit_string_literal_32.data);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4468
          = _M0MPC15array5Array2atGsE(_M0L5itemsS2187, _M0L1iS2189);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2190, _M0L6_2atmpS4468);
          moonbit_decref(_M0L6_2atmpS4468);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2190, (moonbit_string_t)moonbit_string_literal_33.data);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4466
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2190);
          moonbit_decref(_M0L18_2astring__builderS2190);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPC15array5Array4pushGsE(_M0L6resultS2188, _M0L6_2atmpS4466);
          moonbit_decref(_M0L6_2atmpS4466);
          _M0L6_2atmpS4469 = _M0L1iS2189 + 1;
          _M0L1iS2189 = _M0L6_2atmpS4469;
          continue;
        } else {
          moonbit_decref(_M0L5itemsS2187);
        }
        break;
      }
      return _M0L6resultS2188;
      join_2183:;
      _M0L6_2atmpS4464 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4464[0] = (moonbit_string_t)moonbit_string_literal_15.data;
      _block_5137
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5137)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5137->$0 = _M0L6_2atmpS4464;
      _block_5137->$1 = 1;
      return _block_5137;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_40.data
           || Moonbit_array_length(_M0L7commandS2087) == 5
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_40.data, 10)
         ) {
    int32_t _M0L6_2atmpS4474;
    moonbit_decref(_M0L7commandS2087);
    #line 322 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4474 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4474 < 2) {
      moonbit_string_t* _M0L6_2atmpS4475;
      struct _M0TPB5ArrayGsE* _block_5131;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4475 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4475[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5131
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5131)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5131->$0 = _M0L6_2atmpS4475;
      _block_5131->$1 = 1;
      return _block_5131;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2198;
      moonbit_string_t _M0L6_2atmpS4479;
      int32_t _M0L6_2atmpS4478;
      moonbit_string_t _M0L6_2atmpS4477;
      moonbit_string_t* _M0L6_2atmpS4476;
      struct _M0TPB5ArrayGsE* _block_5132;
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2198
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2198, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4479 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4478
      = _M0MP38JIA2JIA29moonbitdb3lib8Database5zcard(_M0L2dbS2088, _M0L6_2atmpS4479);
      moonbit_decref(_M0L6_2atmpS4479);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2198, _M0L6_2atmpS4478);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4477
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2198);
      moonbit_decref(_M0L18_2astring__builderS2198);
      _M0L6_2atmpS4476 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4476[0] = _M0L6_2atmpS4477;
      _block_5132
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5132)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5132->$0 = _M0L6_2atmpS4476;
      _block_5132->$1 = 1;
      return _block_5132;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_39.data
           || Moonbit_array_length(_M0L7commandS2087) == 6
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_39.data, 12)
         ) {
    int32_t _M0L6_2atmpS4480;
    moonbit_decref(_M0L7commandS2087);
    #line 326 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4480 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4480 < 3) {
      moonbit_string_t* _M0L6_2atmpS4481;
      struct _M0TPB5ArrayGsE* _block_5127;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4481 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4481[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5127
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5127)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5127->$0 = _M0L6_2atmpS4481;
      _block_5127->$1 = 1;
      return _block_5127;
    } else {
      float _M0L1vS2200;
      moonbit_string_t _M0L6_2atmpS4485;
      moonbit_string_t _M0L6_2atmpS4486;
      void* _M0L7_2abindS2202;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2201;
      moonbit_string_t _M0L6_2atmpS4483;
      moonbit_string_t* _M0L6_2atmpS4482;
      struct _M0TPB5ArrayGsE* _block_5130;
      #line 328 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4485 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 328 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4486 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 328 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2202
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(_M0L2dbS2088, _M0L6_2atmpS4485, _M0L6_2atmpS4486);
      moonbit_decref(_M0L6_2atmpS4485);
      moonbit_decref(_M0L6_2atmpS4486);
      switch (Moonbit_object_tag(_M0L7_2abindS2202)) {
        case 1: {
          struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2203 =
            (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS2202;
          float _M0L4_2avS2204 = _M0L7_2aSomeS2203->$0;
          moonbit_decref(_M0L7_2aSomeS2203);
          _M0L1vS2200 = _M0L4_2avS2204;
          goto join_2199;
          break;
        }
        default: {
          moonbit_string_t* _M0L6_2atmpS4484;
          struct _M0TPB5ArrayGsE* _block_5129;
          moonbit_decref(_M0L7_2abindS2202);
          _M0L6_2atmpS4484 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
          _M0L6_2atmpS4484[0]
          = (moonbit_string_t)moonbit_string_literal_38.data;
          _block_5129
          = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
          Moonbit_object_header(_block_5129)->meta
          = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
          _block_5129->$0 = _M0L6_2atmpS4484;
          _block_5129->$1 = 1;
          return _block_5129;
          break;
        }
      }
      join_2199:;
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2201
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2201, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2201, _M0L1vS2200);
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2201, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4483
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2201);
      moonbit_decref(_M0L18_2astring__builderS2201);
      _M0L6_2atmpS4482 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4482[0] = _M0L6_2atmpS4483;
      _block_5130
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5130)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5130->$0 = _M0L6_2atmpS4482;
      _block_5130->$1 = 1;
      return _block_5130;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_37.data
           || Moonbit_array_length(_M0L7commandS2087) == 5
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_37.data, 10)
         ) {
    int32_t _M0L6_2atmpS4487;
    moonbit_decref(_M0L7commandS2087);
    #line 335 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4487 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4487 < 3) {
      moonbit_string_t* _M0L6_2atmpS4488;
      struct _M0TPB5ArrayGsE* _block_5123;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4488 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4488[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5123
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5123)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5123->$0 = _M0L6_2atmpS4488;
      _block_5123->$1 = 1;
      return _block_5123;
    } else {
      int32_t _M0L1vS2206;
      moonbit_string_t _M0L6_2atmpS4492;
      moonbit_string_t _M0L6_2atmpS4493;
      int64_t _M0L7_2abindS2208;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2207;
      moonbit_string_t _M0L6_2atmpS4490;
      moonbit_string_t* _M0L6_2atmpS4489;
      struct _M0TPB5ArrayGsE* _block_5126;
      #line 337 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4492 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 337 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4493 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      moonbit_decref(_M0L5partsS2085);
      #line 337 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2208
      = _M0MP38JIA2JIA29moonbitdb3lib8Database5zrank(_M0L2dbS2088, _M0L6_2atmpS4492, _M0L6_2atmpS4493);
      moonbit_decref(_M0L6_2atmpS4492);
      moonbit_decref(_M0L6_2atmpS4493);
      if (_M0L7_2abindS2208 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4491 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5125;
        _M0L6_2atmpS4491[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5125
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5125)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5125->$0 = _M0L6_2atmpS4491;
        _block_5125->$1 = 1;
        return _block_5125;
      } else {
        int64_t _M0L7_2aSomeS2209 = _M0L7_2abindS2208;
        int32_t _M0L4_2avS2210 = (int32_t)_M0L7_2aSomeS2209;
        _M0L1vS2206 = _M0L4_2avS2210;
        goto join_2205;
      }
      join_2205:;
      #line 338 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2207
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 338 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2207, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 338 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2207, _M0L1vS2206);
      #line 338 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4490
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2207);
      moonbit_decref(_M0L18_2astring__builderS2207);
      _M0L6_2atmpS4489 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4489[0] = _M0L6_2atmpS4490;
      _block_5126
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5126)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5126->$0 = _M0L6_2atmpS4489;
      _block_5126->$1 = 1;
      return _block_5126;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_35.data
           || Moonbit_array_length(_M0L7commandS2087) == 7
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_35.data, 14)
         ) {
    int32_t _M0L6_2atmpS4494;
    moonbit_decref(_M0L7commandS2087);
    #line 344 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4494 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4494 < 4) {
      moonbit_string_t* _M0L6_2atmpS4495;
      struct _M0TPB5ArrayGsE* _block_5119;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4495 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4495[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5119
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5119)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5119->$0 = _M0L6_2atmpS4495;
      _block_5119->$1 = 1;
      return _block_5119;
    } else {
      float _M0L3incS2212;
      moonbit_string_t _M0L6_2atmpS4501;
      void* _M0L7_2abindS2215;
      moonbit_string_t _M0L6_2atmpS4498;
      moonbit_string_t _M0L6_2atmpS4499;
      float _M0L10new__scoreS2213;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2214;
      moonbit_string_t _M0L6_2atmpS4497;
      moonbit_string_t* _M0L6_2atmpS4496;
      struct _M0TPB5ArrayGsE* _block_5122;
      #line 346 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4501 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 2);
      #line 346 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2215
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl12parse__float(_M0L6_2atmpS4501);
      moonbit_decref(_M0L6_2atmpS4501);
      switch (Moonbit_object_tag(_M0L7_2abindS2215)) {
        case 1: {
          struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2216 =
            (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS2215;
          float _M0L6_2aincS2217 = _M0L7_2aSomeS2216->$0;
          moonbit_decref(_M0L7_2aSomeS2216);
          _M0L3incS2212 = _M0L6_2aincS2217;
          goto join_2211;
          break;
        }
        default: {
          moonbit_string_t* _M0L6_2atmpS4500;
          struct _M0TPB5ArrayGsE* _block_5121;
          moonbit_decref(_M0L7_2abindS2215);
          moonbit_decref(_M0L5partsS2085);
          _M0L6_2atmpS4500 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
          _M0L6_2atmpS4500[0]
          = (moonbit_string_t)moonbit_string_literal_36.data;
          _block_5121
          = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
          Moonbit_object_header(_block_5121)->meta
          = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
          _block_5121->$0 = _M0L6_2atmpS4500;
          _block_5121->$1 = 1;
          return _block_5121;
          break;
        }
      }
      join_2211:;
      #line 348 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4498 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      #line 348 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4499 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 3);
      moonbit_decref(_M0L5partsS2085);
      #line 348 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L10new__scoreS2213
      = _M0MP38JIA2JIA29moonbitdb3lib8Database7zincrby(_M0L2dbS2088, _M0L6_2atmpS4498, _M0L3incS2212, _M0L6_2atmpS4499);
      moonbit_decref(_M0L6_2atmpS4498);
      moonbit_decref(_M0L6_2atmpS4499);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2214
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2214, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2214, _M0L10new__scoreS2213);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2214, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4497
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2214);
      moonbit_decref(_M0L18_2astring__builderS2214);
      _M0L6_2atmpS4496 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4496[0] = _M0L6_2atmpS4497;
      _block_5122
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5122)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5122->$0 = _M0L6_2atmpS4496;
      _block_5122->$1 = 1;
      return _block_5122;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_34.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_34.data, 8)
         ) {
    moonbit_string_t* _M0L6_2atmpS4514;
    struct _M0TPB5ArrayGsE* _M0L4keysS2218;
    moonbit_string_t* _M0L6_2atmpS4513;
    struct _M0TPB5ArrayGsE* _M0L4valsS2219;
    struct _M0TPB8MutLocalGiE* _M0L1iS2220;
    moonbit_string_t* _M0L6_2atmpS4512;
    struct _M0TPB5ArrayGsE* _block_5118;
    moonbit_decref(_M0L7commandS2087);
    _M0L6_2atmpS4514 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L4keysS2218
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L4keysS2218)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4keysS2218->$0 = _M0L6_2atmpS4514;
    _M0L4keysS2218->$1 = 0;
    _M0L6_2atmpS4513 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L4valsS2219
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L4valsS2219)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4valsS2219->$0 = _M0L6_2atmpS4513;
    _M0L4valsS2219->$1 = 0;
    _M0L1iS2220
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L1iS2220)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L1iS2220->$0 = 1;
    while (1) {
      int32_t _M0L3valS4504 = _M0L1iS2220->$0;
      int32_t _M0L6_2atmpS4502 = _M0L3valS4504 + 1;
      int32_t _M0L6_2atmpS4503;
      #line 359 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4503 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
      if (_M0L6_2atmpS4502 < _M0L6_2atmpS4503) {
        int32_t _M0L3valS4506 = _M0L1iS2220->$0;
        moonbit_string_t _M0L6_2atmpS4505;
        int32_t _M0L3valS4509;
        int32_t _M0L6_2atmpS4508;
        moonbit_string_t _M0L6_2atmpS4507;
        int32_t _M0L3valS4511;
        int32_t _M0L6_2atmpS4510;
        #line 360 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4505
        = _M0MPC15array5Array2atGsE(_M0L5partsS2085, _M0L3valS4506);
        #line 360 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4keysS2218, _M0L6_2atmpS4505);
        moonbit_decref(_M0L6_2atmpS4505);
        _M0L3valS4509 = _M0L1iS2220->$0;
        _M0L6_2atmpS4508 = _M0L3valS4509 + 1;
        #line 361 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4507
        = _M0MPC15array5Array2atGsE(_M0L5partsS2085, _M0L6_2atmpS4508);
        #line 361 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4valsS2219, _M0L6_2atmpS4507);
        moonbit_decref(_M0L6_2atmpS4507);
        _M0L3valS4511 = _M0L1iS2220->$0;
        _M0L6_2atmpS4510 = _M0L3valS4511 + 2;
        _M0L1iS2220->$0 = _M0L6_2atmpS4510;
        continue;
      } else {
        moonbit_decref(_M0L1iS2220);
        moonbit_decref(_M0L5partsS2085);
      }
      break;
    }
    #line 364 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MP38JIA2JIA29moonbitdb3lib8Database4mset(_M0L2dbS2088, _M0L4keysS2218, _M0L4valsS2219);
    moonbit_decref(_M0L4keysS2218);
    moonbit_decref(_M0L4valsS2219);
    _M0L6_2atmpS4512 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4512[0] = (moonbit_string_t)moonbit_string_literal_26.data;
    _block_5118
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5118)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5118->$0 = _M0L6_2atmpS4512;
    _block_5118->$1 = 1;
    return _block_5118;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_30.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_30.data, 8)
         ) {
    moonbit_string_t* _M0L6_2atmpS4525;
    struct _M0TPB5ArrayGsE* _M0L4keysS2222;
    int32_t _M0L1iS2223;
    struct _M0TPB5ArrayGOsE* _M0L4valsS2225;
    moonbit_string_t* _M0L6_2atmpS4524;
    struct _M0TPB5ArrayGsE* _M0L6resultS2226;
    int32_t _M0L1iS2227;
    moonbit_decref(_M0L7commandS2087);
    _M0L6_2atmpS4525 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L4keysS2222
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L4keysS2222)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4keysS2222->$0 = _M0L6_2atmpS4525;
    _M0L4keysS2222->$1 = 0;
    _M0L1iS2223 = 1;
    while (1) {
      int32_t _M0L6_2atmpS4515;
      #line 369 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4515 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
      if (_M0L1iS2223 < _M0L6_2atmpS4515) {
        moonbit_string_t _M0L6_2atmpS4516;
        int32_t _M0L6_2atmpS4517;
        #line 370 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4516
        = _M0MPC15array5Array2atGsE(_M0L5partsS2085, _M0L1iS2223);
        #line 370 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4keysS2222, _M0L6_2atmpS4516);
        moonbit_decref(_M0L6_2atmpS4516);
        _M0L6_2atmpS4517 = _M0L1iS2223 + 1;
        _M0L1iS2223 = _M0L6_2atmpS4517;
        continue;
      } else {
        moonbit_decref(_M0L5partsS2085);
      }
      break;
    }
    #line 372 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L4valsS2225
    = _M0MP38JIA2JIA29moonbitdb3lib8Database4mget(_M0L2dbS2088, _M0L4keysS2222);
    moonbit_decref(_M0L4keysS2222);
    _M0L6_2atmpS4524 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS2226
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS2226)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS2226->$0 = _M0L6_2atmpS4524;
    _M0L6resultS2226->$1 = 0;
    _M0L1iS2227 = 0;
    while (1) {
      int32_t _M0L6_2atmpS4518;
      #line 374 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4518 = _M0MPC15array5Array6lengthGOsE(_M0L4valsS2225);
      if (_M0L1iS2227 < _M0L6_2atmpS4518) {
        moonbit_string_t _M0L1vS2229;
        moonbit_string_t _M0L7_2abindS2231;
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2230;
        int32_t _M0L6_2atmpS4520;
        moonbit_string_t _M0L6_2atmpS4519;
        int32_t _M0L6_2atmpS4523;
        #line 375 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L7_2abindS2231
        = _M0MPC15array5Array2atGOsE(_M0L4valsS2225, _M0L1iS2227);
        if (_M0L7_2abindS2231 == 0) {
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2234;
          int32_t _M0L6_2atmpS4522;
          moonbit_string_t _M0L6_2atmpS4521;
          if (_M0L7_2abindS2231) {
            moonbit_decref(_M0L7_2abindS2231);
          }
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2234
          = _M0MPB13StringBuilder21StringBuilder_2einner(9);
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2234, (moonbit_string_t)moonbit_string_literal_23.data);
          _M0L6_2atmpS4522 = _M0L1iS2227 + 1;
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2234, _M0L6_2atmpS4522);
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2234, (moonbit_string_t)moonbit_string_literal_31.data);
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4521
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2234);
          moonbit_decref(_M0L18_2astring__builderS2234);
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPC15array5Array4pushGsE(_M0L6resultS2226, _M0L6_2atmpS4521);
          moonbit_decref(_M0L6_2atmpS4521);
        } else {
          moonbit_string_t _M0L7_2aSomeS2232 = _M0L7_2abindS2231;
          moonbit_string_t _M0L4_2avS2233 = _M0L7_2aSomeS2232;
          _M0L1vS2229 = _M0L4_2avS2233;
          goto join_2228;
        }
        goto joinlet_5116;
        join_2228:;
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L18_2astring__builderS2230
        = _M0MPB13StringBuilder21StringBuilder_2einner(6);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2230, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L6_2atmpS4520 = _M0L1iS2227 + 1;
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2230, _M0L6_2atmpS4520);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2230, (moonbit_string_t)moonbit_string_literal_32.data);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2230, _M0L1vS2229);
        moonbit_decref(_M0L1vS2229);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2230, (moonbit_string_t)moonbit_string_literal_33.data);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4519
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2230);
        moonbit_decref(_M0L18_2astring__builderS2230);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS2226, _M0L6_2atmpS4519);
        moonbit_decref(_M0L6_2atmpS4519);
        joinlet_5116:;
        _M0L6_2atmpS4523 = _M0L1iS2227 + 1;
        _M0L1iS2227 = _M0L6_2atmpS4523;
        continue;
      } else {
        moonbit_decref(_M0L4valsS2225);
      }
      break;
    }
    return _M0L6resultS2226;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_29.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_29.data, 8)
         ) {
    moonbit_string_t* _M0L6_2atmpS4532;
    struct _M0TPB5ArrayGsE* _M0L4keysS2236;
    int32_t _M0L1iS2237;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2239;
    int32_t _M0L6_2atmpS4531;
    moonbit_string_t _M0L6_2atmpS4530;
    moonbit_string_t* _M0L6_2atmpS4529;
    struct _M0TPB5ArrayGsE* _block_5113;
    moonbit_decref(_M0L7commandS2087);
    _M0L6_2atmpS4532 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L4keysS2236
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L4keysS2236)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4keysS2236->$0 = _M0L6_2atmpS4532;
    _M0L4keysS2236->$1 = 0;
    _M0L1iS2237 = 1;
    while (1) {
      int32_t _M0L6_2atmpS4526;
      #line 384 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4526 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
      if (_M0L1iS2237 < _M0L6_2atmpS4526) {
        moonbit_string_t _M0L6_2atmpS4527;
        int32_t _M0L6_2atmpS4528;
        #line 385 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4527
        = _M0MPC15array5Array2atGsE(_M0L5partsS2085, _M0L1iS2237);
        #line 385 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4keysS2236, _M0L6_2atmpS4527);
        moonbit_decref(_M0L6_2atmpS4527);
        _M0L6_2atmpS4528 = _M0L1iS2237 + 1;
        _M0L1iS2237 = _M0L6_2atmpS4528;
        continue;
      } else {
        moonbit_decref(_M0L5partsS2085);
      }
      break;
    }
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2239
    = _M0MPB13StringBuilder21StringBuilder_2einner(10);
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2239, (moonbit_string_t)moonbit_string_literal_28.data);
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4531
    = _M0MP38JIA2JIA29moonbitdb3lib8Database4mdel(_M0L2dbS2088, _M0L4keysS2236);
    moonbit_decref(_M0L4keysS2236);
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2239, _M0L6_2atmpS4531);
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4530
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2239);
    moonbit_decref(_M0L18_2astring__builderS2239);
    _M0L6_2atmpS4529 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4529[0] = _M0L6_2atmpS4530;
    _block_5113
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5113)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5113->$0 = _M0L6_2atmpS4529;
    _block_5113->$1 = 1;
    return _block_5113;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_27.data
           || Moonbit_array_length(_M0L7commandS2087) == 6
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_27.data, 12)
         ) {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2240;
    int32_t _M0L6_2atmpS4535;
    moonbit_string_t _M0L6_2atmpS4534;
    moonbit_string_t* _M0L6_2atmpS4533;
    struct _M0TPB5ArrayGsE* _block_5111;
    moonbit_decref(_M0L7commandS2087);
    moonbit_decref(_M0L5partsS2085);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2240
    = _M0MPB13StringBuilder21StringBuilder_2einner(10);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2240, (moonbit_string_t)moonbit_string_literal_28.data);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4535
    = _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(_M0L2dbS2088);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2240, _M0L6_2atmpS4535);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4534
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2240);
    moonbit_decref(_M0L18_2astring__builderS2240);
    _M0L6_2atmpS4533 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4533[0] = _M0L6_2atmpS4534;
    _block_5111
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5111)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5111->$0 = _M0L6_2atmpS4533;
    _block_5111->$1 = 1;
    return _block_5111;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_25.data
           || Moonbit_array_length(_M0L7commandS2087) == 7
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_25.data, 14)
         ) {
    moonbit_string_t* _M0L6_2atmpS4536;
    struct _M0TPB5ArrayGsE* _block_5110;
    moonbit_decref(_M0L7commandS2087);
    moonbit_decref(_M0L5partsS2085);
    #line 390 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MP38JIA2JIA29moonbitdb3lib8Database7flushdb(_M0L2dbS2088);
    _M0L6_2atmpS4536 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4536[0] = (moonbit_string_t)moonbit_string_literal_26.data;
    _block_5110
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5110)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5110->$0 = _M0L6_2atmpS4536;
    _block_5110->$1 = 1;
    return _block_5110;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_24.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_24.data, 8)
         ) {
    moonbit_string_t _M0L6_2atmpS4538;
    moonbit_string_t* _M0L6_2atmpS4537;
    struct _M0TPB5ArrayGsE* _block_5109;
    moonbit_decref(_M0L7commandS2087);
    moonbit_decref(_M0L5partsS2085);
    #line 391 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4538 = _M0MP38JIA2JIA29moonbitdb3lib8Database4ping();
    _M0L6_2atmpS4537 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4537[0] = _M0L6_2atmpS4538;
    _block_5109
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5109)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5109->$0 = _M0L6_2atmpS4537;
    _block_5109->$1 = 1;
    return _block_5109;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_21.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_21.data, 8)
         ) {
    moonbit_string_t _M0L4infoS2241;
    moonbit_string_t _M0L7_2abindS2243;
    int32_t _M0L6_2atmpS4542;
    struct _M0TPC16string10StringView _M0L6_2atmpS4541;
    struct _M0TPB4IterGRPC16string10StringViewE* _M0L5linesS2242;
    moonbit_string_t* _M0L6_2atmpS4540;
    struct _M0TPB5ArrayGsE* _M0L6resultS2244;
    struct _M0TPB4IterGRPC16string10StringViewE* _M0L5_2aitS2245;
    moonbit_decref(_M0L7commandS2087);
    moonbit_decref(_M0L5partsS2085);
    #line 393 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L4infoS2241
    = _M0MP38JIA2JIA29moonbitdb3lib8Database4info(_M0L2dbS2088);
    _M0L7_2abindS2243 = (moonbit_string_t)moonbit_string_literal_22.data;
    _M0L6_2atmpS4542 = Moonbit_array_length(_M0L7_2abindS2243);
    _M0L6_2atmpS4541
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS2243, .$1 = 0, .$2 = _M0L6_2atmpS4542
    };
    #line 394 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L5linesS2242
    = _M0MPC16string6String5split(_M0L4infoS2241, _M0L6_2atmpS4541);
    moonbit_decref(_M0L4infoS2241);
    moonbit_decref(_M0L6_2atmpS4541.$0);
    _M0L6_2atmpS4540 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS2244
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS2244)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS2244->$0 = _M0L6_2atmpS4540;
    _M0L6resultS2244->$1 = 0;
    _M0L5_2aitS2245 = _M0L5linesS2242;
    while (1) {
      struct _M0TPC16string10StringView _M0L4lineS2247;
      void* _M0L7_2abindS2250;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2248;
      moonbit_string_t _M0L6_2atmpS4539;
      #line 396 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2250
      = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L5_2aitS2245);
      switch (Moonbit_object_tag(_M0L7_2abindS2250)) {
        case 1: {
          struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS2251 =
            (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS2250;
          struct _M0TPC16string10StringView _M0L8_2afieldS4558 =
            _M0L7_2aSomeS2251->$0;
          int32_t _M0L6_2acntS5017 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aSomeS2251));
          struct _M0TPC16string10StringView _M0L7_2alineS2252;
          if (_M0L6_2acntS5017 > 1) {
            int32_t _M0L11_2anew__cntS5018 = _M0L6_2acntS5017 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aSomeS2251), _M0L11_2anew__cntS5018);
            moonbit_incref(_M0L8_2afieldS4558.$0);
          } else if (_M0L6_2acntS5017 == 1) {
            #line 396 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            moonbit_free(_M0L7_2aSomeS2251);
          }
          _M0L7_2alineS2252 = _M0L8_2afieldS4558;
          _M0L4lineS2247 = _M0L7_2alineS2252;
          goto join_2246;
          break;
        }
        default: {
          moonbit_decref(_M0L7_2abindS2250);
          moonbit_decref(_M0L5_2aitS2245);
          break;
        }
      }
      goto joinlet_5108;
      join_2246:;
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2248
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2248, (moonbit_string_t)moonbit_string_literal_23.data);
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L18_2astring__builderS2248, _M0L4lineS2247);
      moonbit_decref(_M0L4lineS2247.$0);
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4539
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2248);
      moonbit_decref(_M0L18_2astring__builderS2248);
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS2244, _M0L6_2atmpS4539);
      moonbit_decref(_M0L6_2atmpS4539);
      continue;
      joinlet_5108:;
      break;
    }
    return _M0L6resultS2244;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_18.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_18.data, 8)
         ) {
    int32_t _M0L1sS2254;
    int32_t _M0L2usS2255;
    struct _M0TUiiE* _M0L7_2abindS2258;
    int32_t _M0L4_2asS2259;
    int32_t _M0L5_2ausS2260;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2256;
    moonbit_string_t _M0L6_2atmpS4544;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2257;
    moonbit_string_t _M0L6_2atmpS4545;
    moonbit_string_t* _M0L6_2atmpS4543;
    struct _M0TPB5ArrayGsE* _block_5106;
    moonbit_decref(_M0L7commandS2087);
    moonbit_decref(_M0L5partsS2085);
    #line 402 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L7_2abindS2258
    = _M0MP38JIA2JIA29moonbitdb3lib8Database4time(_M0L2dbS2088);
    _M0L4_2asS2259 = _M0L7_2abindS2258->$0;
    _M0L5_2ausS2260 = _M0L7_2abindS2258->$1;
    moonbit_decref(_M0L7_2abindS2258);
    _M0L1sS2254 = _M0L4_2asS2259;
    _M0L2usS2255 = _M0L5_2ausS2260;
    goto join_2253;
    join_2253:;
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2256
    = _M0MPB13StringBuilder21StringBuilder_2einner(11);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2256, (moonbit_string_t)moonbit_string_literal_19.data);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2256, _M0L1sS2254);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4544
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2256);
    moonbit_decref(_M0L18_2astring__builderS2256);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2257
    = _M0MPB13StringBuilder21StringBuilder_2einner(16);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2257, (moonbit_string_t)moonbit_string_literal_20.data);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2257, _M0L2usS2255);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4545
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2257);
    moonbit_decref(_M0L18_2astring__builderS2257);
    _M0L6_2atmpS4543 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
    _M0L6_2atmpS4543[0] = _M0L6_2atmpS4544;
    _M0L6_2atmpS4543[1] = _M0L6_2atmpS4545;
    _block_5106
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5106)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5106->$0 = _M0L6_2atmpS4543;
    _block_5106->$1 = 2;
    return _block_5106;
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_13.data
           || Moonbit_array_length(_M0L7commandS2087) == 7
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_13.data, 14)
         ) {
    int32_t _M0L6_2atmpS4546;
    moonbit_decref(_M0L7commandS2087);
    #line 406 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4546 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2085);
    if (_M0L6_2atmpS4546 < 2) {
      moonbit_string_t* _M0L6_2atmpS4547;
      struct _M0TPB5ArrayGsE* _block_5101;
      moonbit_decref(_M0L5partsS2085);
      _M0L6_2atmpS4547 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4547[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5101
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5101)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5101->$0 = _M0L6_2atmpS4547;
      _block_5101->$1 = 1;
      return _block_5101;
    } else {
      int32_t _M0L1mS2262;
      moonbit_string_t _M0L6_2atmpS4551;
      int64_t _M0L7_2abindS2264;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2263;
      moonbit_string_t _M0L6_2atmpS4549;
      moonbit_string_t* _M0L6_2atmpS4548;
      struct _M0TPB5ArrayGsE* _block_5104;
      #line 408 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4551 = _M0MPC15array5Array2atGsE(_M0L5partsS2085, 1);
      moonbit_decref(_M0L5partsS2085);
      #line 408 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2264
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl10parse__int(_M0L6_2atmpS4551);
      moonbit_decref(_M0L6_2atmpS4551);
      if (_M0L7_2abindS2264 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4550 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5103;
        _M0L6_2atmpS4550[0]
        = (moonbit_string_t)moonbit_string_literal_15.data;
        _block_5103
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5103)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5103->$0 = _M0L6_2atmpS4550;
        _block_5103->$1 = 1;
        return _block_5103;
      } else {
        int64_t _M0L7_2aSomeS2265 = _M0L7_2abindS2264;
        int32_t _M0L4_2amS2266 = (int32_t)_M0L7_2aSomeS2265;
        _M0L1mS2262 = _M0L4_2amS2266;
        goto join_2261;
      }
      join_2261:;
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(_M0L2dbS2088, _M0L1mS2262);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2263
      = _M0MPB13StringBuilder21StringBuilder_2einner(16);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2263, (moonbit_string_t)moonbit_string_literal_16.data);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2263, _M0L1mS2262);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2263, (moonbit_string_t)moonbit_string_literal_17.data);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4549
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2263);
      moonbit_decref(_M0L18_2astring__builderS2263);
      _M0L6_2atmpS4548 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4548[0] = _M0L6_2atmpS4549;
      _block_5104
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5104)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5104->$0 = _M0L6_2atmpS4548;
      _block_5104->$1 = 1;
      return _block_5104;
    }
  } else if (
           _M0L7commandS2087
           == (moonbit_string_t)moonbit_string_literal_3.data
           || Moonbit_array_length(_M0L7commandS2087) == 4
              && 0
                 == memcmp(_M0L7commandS2087, (moonbit_string_t)moonbit_string_literal_3.data, 8)
         ) {
    moonbit_string_t* _M0L6_2atmpS4552;
    struct _M0TPB5ArrayGsE* _block_5100;
    moonbit_decref(_M0L7commandS2087);
    moonbit_decref(_M0L5partsS2085);
    _M0L6_2atmpS4552 = (moonbit_string_t*)moonbit_make_ref_array_raw(9);
    _M0L6_2atmpS4552[0] = (moonbit_string_t)moonbit_string_literal_4.data;
    _M0L6_2atmpS4552[1] = (moonbit_string_t)moonbit_string_literal_5.data;
    _M0L6_2atmpS4552[2] = (moonbit_string_t)moonbit_string_literal_6.data;
    _M0L6_2atmpS4552[3] = (moonbit_string_t)moonbit_string_literal_7.data;
    _M0L6_2atmpS4552[4] = (moonbit_string_t)moonbit_string_literal_8.data;
    _M0L6_2atmpS4552[5] = (moonbit_string_t)moonbit_string_literal_9.data;
    _M0L6_2atmpS4552[6] = (moonbit_string_t)moonbit_string_literal_10.data;
    _M0L6_2atmpS4552[7] = (moonbit_string_t)moonbit_string_literal_11.data;
    _M0L6_2atmpS4552[8] = (moonbit_string_t)moonbit_string_literal_12.data;
    _block_5100
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5100)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5100->$0 = _M0L6_2atmpS4552;
    _block_5100->$1 = 9;
    return _block_5100;
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2267;
    moonbit_string_t _M0L6_2atmpS4554;
    moonbit_string_t* _M0L6_2atmpS4553;
    struct _M0TPB5ArrayGsE* _block_5099;
    moonbit_decref(_M0L5partsS2085);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2267
    = _M0MPB13StringBuilder21StringBuilder_2einner(22);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2267, (moonbit_string_t)moonbit_string_literal_1.data);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2267, _M0L7commandS2087);
    moonbit_decref(_M0L7commandS2087);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2267, (moonbit_string_t)moonbit_string_literal_2.data);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4554
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2267);
    moonbit_decref(_M0L18_2astring__builderS2267);
    _M0L6_2atmpS4553 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4553[0] = _M0L6_2atmpS4554;
    _block_5099
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5099)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5099->$0 = _M0L6_2atmpS4553;
    _block_5099->$1 = 1;
    return _block_5099;
  }
}

struct _M0TPB5ArrayGsE* _M0FP48JIA2JIA29moonbitdb8examples9cli__repl14split__command(
  moonbit_string_t _M0L3cmdS2082
) {
  moonbit_string_t* _M0L6_2atmpS4262;
  struct _M0TPB5ArrayGsE* _M0L5partsS2078;
  struct _M0TPB8MutLocalGiE* _M0L5startS2079;
  struct _M0TPB8MutLocalGiE* _M0L1iS2080;
  struct _M0TPB8MutLocalGbE* _M0L9in__quoteS2081;
  int32_t _M0L3valS4256;
  int32_t _if__result_5239;
  #line 62 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6_2atmpS4262 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L5partsS2078
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L5partsS2078)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L5partsS2078->$0 = _M0L6_2atmpS4262;
  _M0L5partsS2078->$1 = 0;
  _M0L5startS2079
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5startS2079)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5startS2079->$0 = -1;
  _M0L1iS2080
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS2080)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS2080->$0 = 0;
  _M0L9in__quoteS2081
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L9in__quoteS2081)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9in__quoteS2081->$0 = 0;
  while (1) {
    int32_t _M0L3valS4231 = _M0L1iS2080->$0;
    int32_t _M0L6_2atmpS4232 = Moonbit_array_length(_M0L3cmdS2082);
    if (_M0L3valS4231 < _M0L6_2atmpS4232) {
      int32_t _M0L3valS4253 = _M0L1iS2080->$0;
      int32_t _M0L1cS2083;
      int32_t _if__result_5236;
      int32_t _M0L3valS4252;
      int32_t _M0L6_2atmpS4251;
      if (
        _M0L3valS4253 < 0
        || _M0L3valS4253 >= Moonbit_array_length(_M0L3cmdS2082)
      ) {
        #line 68 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        moonbit_panic();
      }
      _M0L1cS2083 = _M0L3cmdS2082[_M0L3valS4253];
      if (_M0L1cS2083 == 39) {
        _if__result_5236 = 1;
      } else {
        _if__result_5236 = _M0L1cS2083 == 34;
      }
      if (_if__result_5236) {
        if (_M0L9in__quoteS2081->$0) {
          int32_t _M0L3valS4233 = _M0L5startS2079->$0;
          if (_M0L3valS4233 >= 0) {
            int32_t _M0L3valS4236 = _M0L5startS2079->$0;
            int32_t _M0L3valS4238 = _M0L1iS2080->$0;
            int64_t _M0L6_2atmpS4237 = (int64_t)_M0L3valS4238;
            struct _M0TPC16string10StringView _M0L6_2atmpS4235;
            moonbit_string_t _M0L6_2atmpS4234;
            #line 73 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0L6_2atmpS4235
            = _M0MPC16string6String11sub_2einner(_M0L3cmdS2082, _M0L3valS4236, _M0L6_2atmpS4237);
            #line 73 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0L6_2atmpS4234
            = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS4235);
            moonbit_decref(_M0L6_2atmpS4235.$0);
            #line 73 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5partsS2078, _M0L6_2atmpS4234);
            moonbit_decref(_M0L6_2atmpS4234);
            _M0L5startS2079->$0 = -1;
          }
          _M0L9in__quoteS2081->$0 = 0;
        } else {
          int32_t _M0L3valS4240;
          int32_t _M0L6_2atmpS4239;
          _M0L9in__quoteS2081->$0 = 1;
          _M0L3valS4240 = _M0L1iS2080->$0;
          _M0L6_2atmpS4239 = _M0L3valS4240 + 1;
          _M0L5startS2079->$0 = _M0L6_2atmpS4239;
        }
      } else {
        int32_t _if__result_5237;
        if (_M0L1cS2083 == 32) {
          int32_t _M0L3valS4241 = _M0L9in__quoteS2081->$0;
          _if__result_5237 = !_M0L3valS4241;
        } else {
          _if__result_5237 = 0;
        }
        if (_if__result_5237) {
          int32_t _M0L3valS4242 = _M0L5startS2079->$0;
          if (_M0L3valS4242 >= 0) {
            int32_t _M0L3valS4245 = _M0L5startS2079->$0;
            int32_t _M0L3valS4247 = _M0L1iS2080->$0;
            int64_t _M0L6_2atmpS4246 = (int64_t)_M0L3valS4247;
            struct _M0TPC16string10StringView _M0L6_2atmpS4244;
            moonbit_string_t _M0L6_2atmpS4243;
            #line 84 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0L6_2atmpS4244
            = _M0MPC16string6String11sub_2einner(_M0L3cmdS2082, _M0L3valS4245, _M0L6_2atmpS4246);
            #line 84 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0L6_2atmpS4243
            = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS4244);
            moonbit_decref(_M0L6_2atmpS4244.$0);
            #line 84 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5partsS2078, _M0L6_2atmpS4243);
            moonbit_decref(_M0L6_2atmpS4243);
            _M0L5startS2079->$0 = -1;
          }
        } else {
          int32_t _M0L3valS4249 = _M0L5startS2079->$0;
          int32_t _if__result_5238;
          if (_M0L3valS4249 == -1) {
            int32_t _M0L3valS4248 = _M0L9in__quoteS2081->$0;
            _if__result_5238 = !_M0L3valS4248;
          } else {
            _if__result_5238 = 0;
          }
          if (_if__result_5238) {
            int32_t _M0L3valS4250 = _M0L1iS2080->$0;
            _M0L5startS2079->$0 = _M0L3valS4250;
          }
        }
      }
      _M0L3valS4252 = _M0L1iS2080->$0;
      _M0L6_2atmpS4251 = _M0L3valS4252 + 1;
      _M0L1iS2080->$0 = _M0L6_2atmpS4251;
      continue;
    } else {
      moonbit_decref(_M0L9in__quoteS2081);
      moonbit_decref(_M0L1iS2080);
    }
    break;
  }
  _M0L3valS4256 = _M0L5startS2079->$0;
  if (_M0L3valS4256 >= 0) {
    int32_t _M0L3valS4254 = _M0L5startS2079->$0;
    int32_t _M0L6_2atmpS4255 = Moonbit_array_length(_M0L3cmdS2082);
    _if__result_5239 = _M0L3valS4254 <= _M0L6_2atmpS4255;
  } else {
    _if__result_5239 = 0;
  }
  if (_if__result_5239) {
    int32_t _M0L3valS4257 = _M0L5startS2079->$0;
    int32_t _M0L6_2atmpS4258 = Moonbit_array_length(_M0L3cmdS2082);
    if (_M0L3valS4257 == _M0L6_2atmpS4258) {
      moonbit_decref(_M0L5startS2079);
      #line 97 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5partsS2078, (moonbit_string_t)moonbit_string_literal_75.data);
    } else {
      int32_t _M0L3valS4261 = _M0L5startS2079->$0;
      struct _M0TPC16string10StringView _M0L6_2atmpS4260;
      moonbit_string_t _M0L6_2atmpS4259;
      moonbit_decref(_M0L5startS2079);
      #line 99 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4260
      = _M0MPC16string6String11sub_2einner(_M0L3cmdS2082, _M0L3valS4261, 4294967296ll);
      #line 99 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4259
      = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS4260);
      moonbit_decref(_M0L6_2atmpS4260.$0);
      #line 99 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5partsS2078, _M0L6_2atmpS4259);
      moonbit_decref(_M0L6_2atmpS4259);
    }
  } else {
    moonbit_decref(_M0L5startS2079);
  }
  return _M0L5partsS2078;
}

void* _M0FP48JIA2JIA29moonbitdb8examples9cli__repl12parse__float(
  moonbit_string_t _M0L1sS2071
) {
  struct _M0TPB8MutLocalGfE* _M0L6resultS2068;
  struct _M0TPB8MutLocalGiE* _M0L1iS2069;
  struct _M0TPB8MutLocalGbE* _M0L3negS2070;
  int32_t _M0L6_2atmpS4206;
  int32_t _if__result_5240;
  int32_t _M0L3valS4207;
  int32_t _M0L6_2atmpS4208;
  struct _M0TPB8MutLocalGbE* _M0L8has__dotS2072;
  struct _M0TPB8MutLocalGfE* _M0L12decimal__posS2073;
  struct _M0TPB8MutLocalGbE* _M0L10has__digitS2074;
  int32_t _M0L3valS4227;
  int32_t _result_5244;
  #line 24 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6resultS2068
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L6resultS2068)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6resultS2068->$0 = 0x0p+0f;
  _M0L1iS2069
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS2069)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS2069->$0 = 0;
  _M0L3negS2070
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L3negS2070)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L3negS2070->$0 = 0;
  _M0L6_2atmpS4206 = Moonbit_array_length(_M0L1sS2071);
  if (_M0L6_2atmpS4206 > 0) {
    int32_t _M0L6_2atmpS4205;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS2071)) {
      #line 28 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4205 = _M0L1sS2071[0];
    _if__result_5240 = _M0L6_2atmpS4205 == 45;
  } else {
    _if__result_5240 = 0;
  }
  if (_if__result_5240) {
    _M0L3negS2070->$0 = 1;
    _M0L1iS2069->$0 = 1;
  }
  _M0L3valS4207 = _M0L1iS2069->$0;
  _M0L6_2atmpS4208 = Moonbit_array_length(_M0L1sS2071);
  if (_M0L3valS4207 >= _M0L6_2atmpS4208) {
    moonbit_decref(_M0L3negS2070);
    moonbit_decref(_M0L1iS2069);
    moonbit_decref(_M0L6resultS2068);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
  _M0L8has__dotS2072
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L8has__dotS2072)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L8has__dotS2072->$0 = 0;
  _M0L12decimal__posS2073
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L12decimal__posS2073)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L12decimal__posS2073->$0 = 0x1.4p+3f;
  _M0L10has__digitS2074
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L10has__digitS2074)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L10has__digitS2074->$0 = 0;
  while (1) {
    int32_t _M0L3valS4209 = _M0L1iS2069->$0;
    int32_t _M0L6_2atmpS4210 = Moonbit_array_length(_M0L1sS2071);
    if (_M0L3valS4209 < _M0L6_2atmpS4210) {
      int32_t _M0L3valS4226 = _M0L1iS2069->$0;
      int32_t _M0L1cS2075;
      int32_t _if__result_5242;
      int32_t _M0L3valS4225;
      int32_t _M0L6_2atmpS4224;
      if (
        _M0L3valS4226 < 0
        || _M0L3valS4226 >= Moonbit_array_length(_M0L1sS2071)
      ) {
        #line 39 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        moonbit_panic();
      }
      _M0L1cS2075 = _M0L1sS2071[_M0L3valS4226];
      if (_M0L1cS2075 >= 48) {
        _if__result_5242 = _M0L1cS2075 <= 57;
      } else {
        _if__result_5242 = 0;
      }
      if (_if__result_5242) {
        int32_t _M0L6_2atmpS4221;
        int32_t _M0L6_2atmpS4222;
        int32_t _M0L6_2atmpS4220;
        float _M0L5digitS2076;
        _M0L10has__digitS2074->$0 = 1;
        _M0L6_2atmpS4221 = (int32_t)_M0L1cS2075;
        _M0L6_2atmpS4222 = 48;
        _M0L6_2atmpS4220 = _M0L6_2atmpS4221 - _M0L6_2atmpS4222;
        _M0L5digitS2076 = (float)_M0L6_2atmpS4220;
        if (_M0L8has__dotS2072->$0) {
          float _M0L3valS4212 = _M0L6resultS2068->$0;
          float _M0L3valS4214 = _M0L12decimal__posS2073->$0;
          float _M0L6_2atmpS4213 = _M0L5digitS2076 / _M0L3valS4214;
          float _M0L6_2atmpS4211 = _M0L3valS4212 + _M0L6_2atmpS4213;
          float _M0L3valS4216;
          float _M0L6_2atmpS4215;
          _M0L6resultS2068->$0 = _M0L6_2atmpS4211;
          _M0L3valS4216 = _M0L12decimal__posS2073->$0;
          _M0L6_2atmpS4215 = _M0L3valS4216 * 0x1.4p+3f;
          _M0L12decimal__posS2073->$0 = _M0L6_2atmpS4215;
        } else {
          float _M0L3valS4219 = _M0L6resultS2068->$0;
          float _M0L6_2atmpS4218 = _M0L3valS4219 * 0x1.4p+3f;
          float _M0L6_2atmpS4217 = _M0L6_2atmpS4218 + _M0L5digitS2076;
          _M0L6resultS2068->$0 = _M0L6_2atmpS4217;
        }
      } else {
        int32_t _if__result_5243;
        if (_M0L1cS2075 == 46) {
          int32_t _M0L3valS4223 = _M0L8has__dotS2072->$0;
          _if__result_5243 = !_M0L3valS4223;
        } else {
          _if__result_5243 = 0;
        }
        if (_if__result_5243) {
          _M0L8has__dotS2072->$0 = 1;
        } else {
          moonbit_decref(_M0L10has__digitS2074);
          moonbit_decref(_M0L12decimal__posS2073);
          moonbit_decref(_M0L8has__dotS2072);
          moonbit_decref(_M0L3negS2070);
          moonbit_decref(_M0L1iS2069);
          moonbit_decref(_M0L6resultS2068);
          return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        }
      }
      _M0L3valS4225 = _M0L1iS2069->$0;
      _M0L6_2atmpS4224 = _M0L3valS4225 + 1;
      _M0L1iS2069->$0 = _M0L6_2atmpS4224;
      continue;
    } else {
      moonbit_decref(_M0L12decimal__posS2073);
      moonbit_decref(_M0L8has__dotS2072);
      moonbit_decref(_M0L1iS2069);
    }
    break;
  }
  _M0L3valS4227 = _M0L10has__digitS2074->$0;
  moonbit_decref(_M0L10has__digitS2074);
  if (!_M0L3valS4227) {
    moonbit_decref(_M0L3negS2070);
    moonbit_decref(_M0L6resultS2068);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
  _result_5244 = _M0L3negS2070->$0;
  moonbit_decref(_M0L3negS2070);
  if (_result_5244) {
    float _M0L3valS4229 = _M0L6resultS2068->$0;
    float _M0L6_2atmpS4228;
    void* _block_5245;
    moonbit_decref(_M0L6resultS2068);
    _M0L6_2atmpS4228 = -_M0L3valS4229;
    _block_5245
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
    Moonbit_object_header(_block_5245)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
    ((struct _M0DTPC16option6OptionGfE4Some*)_block_5245)->$0
    = _M0L6_2atmpS4228;
    return _block_5245;
  } else {
    float _M0L3valS4230 = _M0L6resultS2068->$0;
    void* _block_5246;
    moonbit_decref(_M0L6resultS2068);
    _block_5246
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
    Moonbit_object_header(_block_5246)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
    ((struct _M0DTPC16option6OptionGfE4Some*)_block_5246)->$0 = _M0L3valS4230;
    return _block_5246;
  }
}

int64_t _M0FP48JIA2JIA29moonbitdb8examples9cli__repl10parse__int(
  moonbit_string_t _M0L1sS2065
) {
  struct _M0TPB8MutLocalGiE* _M0L6resultS2062;
  struct _M0TPB8MutLocalGiE* _M0L1iS2063;
  struct _M0TPB8MutLocalGbE* _M0L3negS2064;
  int32_t _M0L6_2atmpS4188;
  int32_t _if__result_5247;
  int32_t _M0L3valS4189;
  int32_t _M0L6_2atmpS4190;
  int32_t _result_5250;
  #line 1 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6resultS2062
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L6resultS2062)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6resultS2062->$0 = 0;
  _M0L1iS2063
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS2063)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS2063->$0 = 0;
  _M0L3negS2064
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L3negS2064)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L3negS2064->$0 = 0;
  _M0L6_2atmpS4188 = Moonbit_array_length(_M0L1sS2065);
  if (_M0L6_2atmpS4188 > 0) {
    int32_t _M0L6_2atmpS4187;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS2065)) {
      #line 5 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4187 = _M0L1sS2065[0];
    _if__result_5247 = _M0L6_2atmpS4187 == 45;
  } else {
    _if__result_5247 = 0;
  }
  if (_if__result_5247) {
    _M0L3negS2064->$0 = 1;
    _M0L1iS2063->$0 = 1;
  }
  _M0L3valS4189 = _M0L1iS2063->$0;
  _M0L6_2atmpS4190 = Moonbit_array_length(_M0L1sS2065);
  if (_M0L3valS4189 >= _M0L6_2atmpS4190) {
    moonbit_decref(_M0L3negS2064);
    moonbit_decref(_M0L1iS2063);
    moonbit_decref(_M0L6resultS2062);
    return 4294967296ll;
  }
  while (1) {
    int32_t _M0L3valS4191 = _M0L1iS2063->$0;
    int32_t _M0L6_2atmpS4192 = Moonbit_array_length(_M0L1sS2065);
    if (_M0L3valS4191 < _M0L6_2atmpS4192) {
      int32_t _M0L3valS4201 = _M0L1iS2063->$0;
      int32_t _M0L1cS2066;
      int32_t _if__result_5249;
      int32_t _M0L3valS4200;
      int32_t _M0L6_2atmpS4199;
      if (
        _M0L3valS4201 < 0
        || _M0L3valS4201 >= Moonbit_array_length(_M0L1sS2065)
      ) {
        #line 13 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        moonbit_panic();
      }
      _M0L1cS2066 = _M0L1sS2065[_M0L3valS4201];
      if (_M0L1cS2066 >= 48) {
        _if__result_5249 = _M0L1cS2066 <= 57;
      } else {
        _if__result_5249 = 0;
      }
      if (_if__result_5249) {
        int32_t _M0L3valS4198 = _M0L6resultS2062->$0;
        int32_t _M0L6_2atmpS4194 = _M0L3valS4198 * 10;
        int32_t _M0L6_2atmpS4196 = (int32_t)_M0L1cS2066;
        int32_t _M0L6_2atmpS4197 = 48;
        int32_t _M0L6_2atmpS4195 = _M0L6_2atmpS4196 - _M0L6_2atmpS4197;
        int32_t _M0L6_2atmpS4193 = _M0L6_2atmpS4194 + _M0L6_2atmpS4195;
        _M0L6resultS2062->$0 = _M0L6_2atmpS4193;
      } else {
        moonbit_decref(_M0L3negS2064);
        moonbit_decref(_M0L1iS2063);
        moonbit_decref(_M0L6resultS2062);
        return 4294967296ll;
      }
      _M0L3valS4200 = _M0L1iS2063->$0;
      _M0L6_2atmpS4199 = _M0L3valS4200 + 1;
      _M0L1iS2063->$0 = _M0L6_2atmpS4199;
      continue;
    } else {
      moonbit_decref(_M0L1iS2063);
    }
    break;
  }
  _result_5250 = _M0L3negS2064->$0;
  moonbit_decref(_M0L3negS2064);
  if (_result_5250) {
    int32_t _M0L3valS4203 = _M0L6resultS2062->$0;
    int32_t _M0L6_2atmpS4202;
    moonbit_decref(_M0L6resultS2062);
    _M0L6_2atmpS4202 = -_M0L3valS4203;
    return (int64_t)_M0L6_2atmpS4202;
  } else {
    int32_t _M0L3valS4204 = _M0L6resultS2062->$0;
    moonbit_decref(_M0L6resultS2062);
    return (int64_t)_M0L3valS4204;
  }
}

struct _M0TUiiE* _M0MP38JIA2JIA29moonbitdb3lib8Database4time(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS2060
) {
  int32_t _M0L13current__timeS4186;
  int32_t _M0L7secondsS2059;
  int32_t _M0L13current__timeS4185;
  int32_t _M0L6_2atmpS4184;
  int32_t _M0L12microsecondsS2061;
  struct _M0TUiiE* _block_5251;
  #line 1588 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L13current__timeS4186 = _M0L4selfS2060->$2;
  _M0L7secondsS2059 = _M0L13current__timeS4186 / 1000;
  _M0L13current__timeS4185 = _M0L4selfS2060->$2;
  _M0L6_2atmpS4184 = _M0L13current__timeS4185 % 1000;
  _M0L12microsecondsS2061 = _M0L6_2atmpS4184 * 1000;
  _block_5251 = (struct _M0TUiiE*)moonbit_malloc(sizeof(struct _M0TUiiE));
  Moonbit_object_header(_block_5251)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_5251->$0 = _M0L7secondsS2059;
  _block_5251->$1 = _M0L12microsecondsS2061;
  return _block_5251;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4info(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS2039
) {
  struct _M0TPB8MutLocalGiE* _M0L10key__countS2036;
  struct _M0TPB8MutLocalGiE* _M0L13expire__countS2037;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4171;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L5_2aitS2038;
  struct _M0TPB3MapGsiE* _M0L7expiresS4175;
  struct _M0TPB4IterGUsiEE* _M0L5_2aitS2047;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2056;
  int32_t _M0L13current__timeS4183;
  moonbit_string_t _M0L6_2atmpS4179;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2057;
  int32_t _M0L3valS4181;
  int32_t _M0L3valS4182;
  moonbit_string_t _M0L6_2atmpS4180;
  moonbit_string_t* _M0L6_2atmpS4178;
  struct _M0TPB5ArrayGsE* _M0L9info__arrS2055;
  moonbit_string_t _M0L7_2abindS2058;
  int32_t _M0L6_2atmpS4177;
  struct _M0TPC16string10StringView _M0L6_2atmpS4176;
  moonbit_string_t _result_5257;
  #line 1564 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L10key__countS2036
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L10key__countS2036)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L10key__countS2036->$0 = 0;
  _M0L13expire__countS2037
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L13expire__countS2037)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L13expire__countS2037->$0 = 0;
  _M0L4dataS4171 = _M0L4selfS2039->$0;
  moonbit_incref(_M0L4dataS4171);
  #line 1566 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS2038
  = _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4171);
  moonbit_decref(_M0L4dataS4171);
  while (1) {
    moonbit_string_t _M0L3keyS2041;
    struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS2043;
    int32_t _M0L6_2atmpS4168;
    #line 1567 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS2043
    = _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L5_2aitS2038);
    if (_M0L7_2abindS2043 == 0) {
      if (_M0L7_2abindS2043) {
        moonbit_decref(_M0L7_2abindS2043);
      }
      moonbit_decref(_M0L5_2aitS2038);
    } else {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS2044 =
        _M0L7_2abindS2043;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS2045 =
        _M0L7_2aSomeS2044;
      moonbit_string_t _M0L8_2afieldS4562 = _M0L4_2axS2045->$0;
      int32_t _M0L6_2acntS5019 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2045));
      moonbit_string_t _M0L6_2akeyS2046;
      if (_M0L6_2acntS5019 > 1) {
        int32_t _M0L11_2anew__cntS5021 = _M0L6_2acntS5019 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2045), _M0L11_2anew__cntS5021);
        moonbit_incref(_M0L8_2afieldS4562);
      } else if (_M0L6_2acntS5019 == 1) {
        void* _M0L8_2afieldS5020 = _M0L4_2axS2045->$1;
        moonbit_decref(_M0L8_2afieldS5020);
        #line 1567 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS2045);
      }
      _M0L6_2akeyS2046 = _M0L8_2afieldS4562;
      _M0L3keyS2041 = _M0L6_2akeyS2046;
      goto join_2040;
    }
    goto joinlet_5253;
    join_2040:;
    #line 1568 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4168
    = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS2039, _M0L3keyS2041);
    moonbit_decref(_M0L3keyS2041);
    if (!_M0L6_2atmpS4168) {
      int32_t _M0L3valS4170 = _M0L10key__countS2036->$0;
      int32_t _M0L6_2atmpS4169 = _M0L3valS4170 + 1;
      _M0L10key__countS2036->$0 = _M0L6_2atmpS4169;
    }
    continue;
    joinlet_5253:;
    break;
  }
  _M0L7expiresS4175 = _M0L4selfS2039->$1;
  moonbit_incref(_M0L7expiresS4175);
  #line 1566 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS2047 = _M0MPB3Map5iter2GsiE(_M0L7expiresS4175);
  moonbit_decref(_M0L7expiresS4175);
  while (1) {
    moonbit_string_t _M0L3keyS2049;
    struct _M0TUsiE* _M0L7_2abindS2051;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4172;
    int32_t _result_5256;
    #line 1572 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS2051 = _M0MPB5Iter24nextGsiE(_M0L5_2aitS2047);
    if (_M0L7_2abindS2051 == 0) {
      if (_M0L7_2abindS2051) {
        moonbit_decref(_M0L7_2abindS2051);
      }
      moonbit_decref(_M0L5_2aitS2047);
    } else {
      struct _M0TUsiE* _M0L7_2aSomeS2052 = _M0L7_2abindS2051;
      struct _M0TUsiE* _M0L4_2axS2053 = _M0L7_2aSomeS2052;
      moonbit_string_t _M0L8_2afieldS4560 = _M0L4_2axS2053->$0;
      int32_t _M0L6_2acntS5022 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2053));
      moonbit_string_t _M0L6_2akeyS2054;
      if (_M0L6_2acntS5022 > 1) {
        int32_t _M0L11_2anew__cntS5023 = _M0L6_2acntS5022 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2053), _M0L11_2anew__cntS5023);
        moonbit_incref(_M0L8_2afieldS4560);
      } else if (_M0L6_2acntS5022 == 1) {
        #line 1572 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS2053);
      }
      _M0L6_2akeyS2054 = _M0L8_2afieldS4560;
      _M0L3keyS2049 = _M0L6_2akeyS2054;
      goto join_2048;
    }
    goto joinlet_5255;
    join_2048:;
    _M0L4dataS4172 = _M0L4selfS2039->$0;
    moonbit_incref(_M0L4dataS4172);
    #line 1573 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5256
    = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4172, _M0L3keyS2049);
    moonbit_decref(_M0L4dataS4172);
    moonbit_decref(_M0L3keyS2049);
    if (_result_5256) {
      int32_t _M0L3valS4174 = _M0L13expire__countS2037->$0;
      int32_t _M0L6_2atmpS4173 = _M0L3valS4174 + 1;
      _M0L13expire__countS2037->$0 = _M0L6_2atmpS4173;
    }
    continue;
    joinlet_5255:;
    break;
  }
  #line 1580 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L18_2astring__builderS2056
  = _M0MPB13StringBuilder21StringBuilder_2einner(13);
  #line 1580 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2056, (moonbit_string_t)moonbit_string_literal_76.data);
  _M0L13current__timeS4183 = _M0L4selfS2039->$2;
  #line 1580 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2056, _M0L13current__timeS4183);
  #line 1580 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4179
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2056);
  moonbit_decref(_M0L18_2astring__builderS2056);
  #line 1583 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L18_2astring__builderS2057
  = _M0MPB13StringBuilder21StringBuilder_2einner(28);
  #line 1583 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2057, (moonbit_string_t)moonbit_string_literal_77.data);
  _M0L3valS4181 = _M0L10key__countS2036->$0;
  moonbit_decref(_M0L10key__countS2036);
  #line 1583 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2057, _M0L3valS4181);
  #line 1583 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2057, (moonbit_string_t)moonbit_string_literal_78.data);
  _M0L3valS4182 = _M0L13expire__countS2037->$0;
  moonbit_decref(_M0L13expire__countS2037);
  #line 1583 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2057, _M0L3valS4182);
  #line 1583 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2057, (moonbit_string_t)moonbit_string_literal_79.data);
  #line 1583 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4180
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2057);
  moonbit_decref(_M0L18_2astring__builderS2057);
  _M0L6_2atmpS4178 = (moonbit_string_t*)moonbit_make_ref_array_raw(6);
  _M0L6_2atmpS4178[0] = (moonbit_string_t)moonbit_string_literal_80.data;
  _M0L6_2atmpS4178[1] = (moonbit_string_t)moonbit_string_literal_81.data;
  _M0L6_2atmpS4178[2] = _M0L6_2atmpS4179;
  _M0L6_2atmpS4178[3] = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS4178[4] = (moonbit_string_t)moonbit_string_literal_82.data;
  _M0L6_2atmpS4178[5] = _M0L6_2atmpS4180;
  _M0L9info__arrS2055
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9info__arrS2055)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L9info__arrS2055->$0 = _M0L6_2atmpS4178;
  _M0L9info__arrS2055->$1 = 6;
  _M0L7_2abindS2058 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS4177 = Moonbit_array_length(_M0L7_2abindS2058);
  _M0L6_2atmpS4176
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS2058, .$1 = 0, .$2 = _M0L6_2atmpS4177
  };
  #line 1585 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_5257
  = _M0MPC15array5Array4joinGsE(_M0L9info__arrS2055, _M0L6_2atmpS4176);
  moonbit_decref(_M0L9info__arrS2055);
  moonbit_decref(_M0L6_2atmpS4176.$0);
  return _result_5257;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4ping() {
  #line 1556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  return (moonbit_string_t)moonbit_string_literal_83.data;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database7flushdb(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS2033
) {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS2034;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS4164;
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L6_2atmpS4163;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS4162;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4565;
  struct _M0TUsiE** _M0L7_2abindS2035;
  struct _M0TUsiE** _M0L6_2atmpS4167;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS4166;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS4165;
  struct _M0TPB3MapGsiE* _M0L6_2aoldS4564;
  #line 1494 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS2034
  = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4164 = _M0L7_2abindS2034;
  _M0L6_2atmpS4163
  = (struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE){
    .$0 = _M0L6_2atmpS4164, .$1 = 0, .$2 = 0
  };
  #line 1495 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4162
  = _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L6_2atmpS4163, 1000ll);
  moonbit_decref(_M0L6_2atmpS4163.$0);
  _M0L6_2aoldS4565 = _M0L4selfS2033->$0;
  moonbit_decref(_M0L6_2aoldS4565);
  _M0L4selfS2033->$0 = _M0L6_2atmpS4162;
  _M0L7_2abindS2035 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4167 = _M0L7_2abindS2035;
  _M0L6_2atmpS4166
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS4167, .$1 = 0, .$2 = 0
  };
  #line 1496 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4165 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS4166, 1000ll);
  moonbit_decref(_M0L6_2atmpS4166.$0);
  _M0L6_2aoldS4564 = _M0L4selfS2033->$1;
  moonbit_decref(_M0L6_2aoldS4564);
  _M0L4selfS2033->$1 = _M0L6_2atmpS4165;
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS2025
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS2023;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4161;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L5_2aitS2024;
  int32_t _result_5260;
  #line 1484 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5countS2023
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS2023)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS2023->$0 = 0;
  _M0L4dataS4161 = _M0L4selfS2025->$0;
  moonbit_incref(_M0L4dataS4161);
  #line 1485 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS2024
  = _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4161);
  moonbit_decref(_M0L4dataS4161);
  while (1) {
    moonbit_string_t _M0L3keyS2027;
    struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS2029;
    int32_t _M0L6_2atmpS4158;
    #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS2029
    = _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L5_2aitS2024);
    if (_M0L7_2abindS2029 == 0) {
      if (_M0L7_2abindS2029) {
        moonbit_decref(_M0L7_2abindS2029);
      }
      moonbit_decref(_M0L5_2aitS2024);
    } else {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS2030 =
        _M0L7_2abindS2029;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS2031 =
        _M0L7_2aSomeS2030;
      moonbit_string_t _M0L8_2afieldS4566 = _M0L4_2axS2031->$0;
      int32_t _M0L6_2acntS5024 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2031));
      moonbit_string_t _M0L6_2akeyS2032;
      if (_M0L6_2acntS5024 > 1) {
        int32_t _M0L11_2anew__cntS5026 = _M0L6_2acntS5024 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2031), _M0L11_2anew__cntS5026);
        moonbit_incref(_M0L8_2afieldS4566);
      } else if (_M0L6_2acntS5024 == 1) {
        void* _M0L8_2afieldS5025 = _M0L4_2axS2031->$1;
        moonbit_decref(_M0L8_2afieldS5025);
        #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS2031);
      }
      _M0L6_2akeyS2032 = _M0L8_2afieldS4566;
      _M0L3keyS2027 = _M0L6_2akeyS2032;
      goto join_2026;
    }
    goto joinlet_5259;
    join_2026:;
    #line 1487 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4158
    = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS2025, _M0L3keyS2027);
    moonbit_decref(_M0L3keyS2027);
    if (!_M0L6_2atmpS4158) {
      int32_t _M0L3valS4160 = _M0L5countS2023->$0;
      int32_t _M0L6_2atmpS4159 = _M0L3valS4160 + 1;
      _M0L5countS2023->$0 = _M0L6_2atmpS4159;
    }
    continue;
    joinlet_5259:;
    break;
  }
  _result_5260 = _M0L5countS2023->$0;
  moonbit_decref(_M0L5countS2023);
  return _result_5260;
}

float _M0MP38JIA2JIA29moonbitdb3lib8Database7zincrby(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS2002,
  moonbit_string_t _M0L3keyS2003,
  float _M0L9incrementS2022,
  moonbit_string_t _M0L11member__valS2018
) {
  int32_t _M0L6_2atmpS4152;
  struct _M0TPB3MapGsfE* _M0L4zsetS2004;
  struct _M0TPB3MapGsfE* _M0L1zS2008;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4157;
  void* _M0L7_2abindS2009;
  struct _M0TUsfE** _M0L7_2abindS2006;
  struct _M0TUsfE** _M0L6_2atmpS4156;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS4155;
  float _M0L1sS2016;
  float _M0L14current__scoreS2014;
  void* _M0L7_2abindS2017;
  float _M0L10new__scoreS2021;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4153;
  void* _M0L4ZSetS4154;
  #line 1379 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1380 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4152
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS2002, _M0L3keyS2003);
  _M0L4dataS4157 = _M0L4selfS2002->$0;
  moonbit_incref(_M0L4dataS4157);
  #line 1381 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS2009
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4157, _M0L3keyS2003);
  moonbit_decref(_M0L4dataS4157);
  if (_M0L7_2abindS2009 == 0) {
    if (_M0L7_2abindS2009) {
      moonbit_decref(_M0L7_2abindS2009);
    }
    goto join_2005;
  } else {
    void* _M0L7_2aSomeS2010 = _M0L7_2abindS2009;
    void* _M0L4_2axS2011 = _M0L7_2aSomeS2010;
    switch (Moonbit_object_tag(_M0L4_2axS2011)) {
      case 4: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS2012 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS2011;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS4569 = _M0L7_2aZSetS2012->$0;
        int32_t _M0L6_2acntS5027 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS2012));
        struct _M0TPB3MapGsfE* _M0L4_2azS2013;
        if (_M0L6_2acntS5027 > 1) {
          int32_t _M0L11_2anew__cntS5028 = _M0L6_2acntS5027 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS2012), _M0L11_2anew__cntS5028);
          moonbit_incref(_M0L8_2afieldS4569);
        } else if (_M0L6_2acntS5027 == 1) {
          #line 1381 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aZSetS2012);
        }
        _M0L4_2azS2013 = _M0L8_2afieldS4569;
        _M0L1zS2008 = _M0L4_2azS2013;
        goto join_2007;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS2011);
        goto join_2005;
        break;
      }
    }
  }
  goto joinlet_5262;
  join_2007:;
  _M0L4zsetS2004 = _M0L1zS2008;
  joinlet_5262:;
  goto joinlet_5261;
  join_2005:;
  _M0L7_2abindS2006 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4156 = _M0L7_2abindS2006;
  _M0L6_2atmpS4155
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS4156, .$1 = 0, .$2 = 0
  };
  #line 1383 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4zsetS2004 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS4155, 10ll);
  moonbit_decref(_M0L6_2atmpS4155.$0);
  joinlet_5261:;
  #line 1385 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS2017
  = _M0MPB3Map3getGsfE(_M0L4zsetS2004, _M0L11member__valS2018);
  switch (Moonbit_object_tag(_M0L7_2abindS2017)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2019 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS2017;
      float _M0L4_2asS2020 = _M0L7_2aSomeS2019->$0;
      moonbit_decref(_M0L7_2aSomeS2019);
      _M0L1sS2016 = _M0L4_2asS2020;
      goto join_2015;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS2017);
      _M0L14current__scoreS2014 = 0x0p+0f;
      break;
    }
  }
  goto joinlet_5263;
  join_2015:;
  _M0L14current__scoreS2014 = _M0L1sS2016;
  joinlet_5263:;
  _M0L10new__scoreS2021 = _M0L14current__scoreS2014 + _M0L9incrementS2022;
  #line 1390 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS2004, _M0L11member__valS2018, _M0L10new__scoreS2021);
  _M0L4dataS4153 = _M0L4selfS2002->$0;
  _M0L4ZSetS4154
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS4154)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 4);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4ZSetS4154)->$0
  = _M0L4zsetS2004;
  moonbit_incref(_M0L4dataS4153);
  #line 1391 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4153, _M0L3keyS2003, _M0L4ZSetS4154);
  moonbit_decref(_M0L4dataS4153);
  moonbit_decref(_M0L4ZSetS4154);
  return _M0L10new__scoreS2021;
}

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database5zrank(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1987,
  moonbit_string_t _M0L3keyS1988,
  moonbit_string_t _M0L11member__valS1992
) {
  #line 1328 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1329 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1987, _M0L3keyS1988)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1991;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4151 =
      _M0L4selfS1987->$0;
    void* _M0L7_2abindS1997;
    int32_t _M0L6_2atmpS4145;
    moonbit_incref(_M0L4dataS4151);
    #line 1332 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1997
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4151, _M0L3keyS1988);
    moonbit_decref(_M0L4dataS4151);
    if (_M0L7_2abindS1997 == 0) {
      if (_M0L7_2abindS1997) {
        moonbit_decref(_M0L7_2abindS1997);
      }
      goto join_1989;
    } else {
      void* _M0L7_2aSomeS1998 = _M0L7_2abindS1997;
      void* _M0L4_2axS1999 = _M0L7_2aSomeS1998;
      switch (Moonbit_object_tag(_M0L4_2axS1999)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS2000 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1999;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4572 = _M0L7_2aZSetS2000->$0;
          int32_t _M0L6_2acntS5031 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS2000));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS2001;
          if (_M0L6_2acntS5031 > 1) {
            int32_t _M0L11_2anew__cntS5032 = _M0L6_2acntS5031 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS2000), _M0L11_2anew__cntS5032);
            moonbit_incref(_M0L8_2afieldS4572);
          } else if (_M0L6_2acntS5031 == 1) {
            #line 1332 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS2000);
          }
          _M0L7_2azsetS2001 = _M0L8_2afieldS4572;
          _M0L4zsetS1991 = _M0L7_2azsetS2001;
          goto join_1990;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1999);
          goto join_1989;
          break;
        }
      }
    }
    join_1990:;
    #line 1334 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4145
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1991, _M0L11member__valS1992);
    moonbit_decref(_M0L4zsetS1991);
    if (!_M0L6_2atmpS4145) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1993;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1994;
      int32_t _M0L1iS1995;
      int32_t _M0L3valS4150;
      #line 1337 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6sortedS1993
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1987, _M0L3keyS1988);
      _M0L4rankS1994
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1994)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1994->$0 = 0;
      _M0L1iS1995 = 0;
      while (1) {
        int32_t _M0L6_2atmpS4146;
        #line 1339 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS4146 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1993);
        if (_M0L1iS1995 < _M0L6_2atmpS4146) {
          struct _M0TUsfE* _M0L6_2atmpS4148;
          moonbit_string_t _M0L8_2afieldS4571;
          int32_t _M0L6_2acntS5029;
          moonbit_string_t _M0L6_2atmpS4147;
          int32_t _result_5267;
          int32_t _M0L6_2atmpS4149;
          #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0L6_2atmpS4148
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1993, _M0L1iS1995);
          _M0L8_2afieldS4571 = _M0L6_2atmpS4148->$0;
          _M0L6_2acntS5029
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS4148));
          if (_M0L6_2acntS5029 > 1) {
            int32_t _M0L11_2anew__cntS5030 = _M0L6_2acntS5029 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS4148), _M0L11_2anew__cntS5030);
            moonbit_incref(_M0L8_2afieldS4571);
          } else if (_M0L6_2acntS5029 == 1) {
            #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2atmpS4148);
          }
          _M0L6_2atmpS4147 = _M0L8_2afieldS4571;
          #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _result_5267
          = _M0L6_2atmpS4147 == _M0L11member__valS1992
            || Moonbit_array_length(_M0L6_2atmpS4147)
               == Moonbit_array_length(_M0L11member__valS1992)
               && 0
                  == memcmp(_M0L6_2atmpS4147, _M0L11member__valS1992, Moonbit_array_length(_M0L6_2atmpS4147) * 2);
          moonbit_decref(_M0L6_2atmpS4147);
          if (_result_5267) {
            moonbit_decref(_M0L6sortedS1993);
            _M0L4rankS1994->$0 = _M0L1iS1995;
            break;
          }
          _M0L6_2atmpS4149 = _M0L1iS1995 + 1;
          _M0L1iS1995 = _M0L6_2atmpS4149;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1993);
        }
        break;
      }
      _M0L3valS4150 = _M0L4rankS1994->$0;
      moonbit_decref(_M0L4rankS1994);
      return (int64_t)_M0L3valS4150;
    }
    join_1989:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1966,
  moonbit_string_t _M0L3keyS1967
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1966, _M0L3keyS1967)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS4140 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_5268 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_5268)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_5268->$0 = _M0L6_2atmpS4140;
    _block_5268->$1 = 0;
    return _block_5268;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1970;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4144 =
      _M0L4selfS1966->$0;
    void* _M0L7_2abindS1982;
    struct _M0TUsfE** _M0L6_2atmpS4143;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1971;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1972;
    struct _M0TPB5ArrayGUsfEE* _result_5273;
    struct _M0TUsfE** _M0L6_2atmpS4141;
    struct _M0TPB5ArrayGUsfEE* _block_5274;
    moonbit_incref(_M0L4dataS4144);
    #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1982
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4144, _M0L3keyS1967);
    moonbit_decref(_M0L4dataS4144);
    if (_M0L7_2abindS1982 == 0) {
      if (_M0L7_2abindS1982) {
        moonbit_decref(_M0L7_2abindS1982);
      }
      goto join_1968;
    } else {
      void* _M0L7_2aSomeS1983 = _M0L7_2abindS1982;
      void* _M0L4_2axS1984 = _M0L7_2aSomeS1983;
      switch (Moonbit_object_tag(_M0L4_2axS1984)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1985 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1984;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4575 = _M0L7_2aZSetS1985->$0;
          int32_t _M0L6_2acntS5035 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1985));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1986;
          if (_M0L6_2acntS5035 > 1) {
            int32_t _M0L11_2anew__cntS5036 = _M0L6_2acntS5035 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1985), _M0L11_2anew__cntS5036);
            moonbit_incref(_M0L8_2afieldS4575);
          } else if (_M0L6_2acntS5035 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1985);
          }
          _M0L7_2azsetS1986 = _M0L8_2afieldS4575;
          _M0L4zsetS1970 = _M0L7_2azsetS1986;
          goto join_1969;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1984);
          goto join_1968;
          break;
        }
      }
    }
    join_1969:;
    _M0L6_2atmpS4143 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1971
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1971)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L5itemsS1971->$0 = _M0L6_2atmpS4143;
    _M0L5itemsS1971->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1972 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1970);
    moonbit_decref(_M0L4zsetS1970);
    while (1) {
      moonbit_string_t _M0L1mS1974;
      float _M0L1sS1975;
      struct _M0TUsfE* _M0L7_2abindS1977;
      struct _M0TUsfE* _M0L8_2atupleS4142;
      #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1977 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1972);
      if (_M0L7_2abindS1977 == 0) {
        if (_M0L7_2abindS1977) {
          moonbit_decref(_M0L7_2abindS1977);
        }
        moonbit_decref(_M0L5_2aitS1972);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1978 = _M0L7_2abindS1977;
        struct _M0TUsfE* _M0L4_2axS1979 = _M0L7_2aSomeS1978;
        moonbit_string_t _M0L4_2amS1980 = _M0L4_2axS1979->$0;
        float _M0L4_2asS1981 = _M0L4_2axS1979->$1;
        int32_t _M0L6_2acntS5033 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1979));
        if (_M0L6_2acntS5033 > 1) {
          int32_t _M0L11_2anew__cntS5034 = _M0L6_2acntS5033 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1979), _M0L11_2anew__cntS5034);
          moonbit_incref(_M0L4_2amS1980);
        } else if (_M0L6_2acntS5033 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1979);
        }
        _M0L1mS1974 = _M0L4_2amS1980;
        _M0L1sS1975 = _M0L4_2asS1981;
        goto join_1973;
      }
      goto joinlet_5272;
      join_1973:;
      _M0L8_2atupleS4142
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS4142)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 0);
      _M0L8_2atupleS4142->$0 = _M0L1mS1974;
      _M0L8_2atupleS4142->$1 = _M0L1sS1975;
      #line 1271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1971, _M0L8_2atupleS4142);
      moonbit_decref(_M0L8_2atupleS4142);
      continue;
      joinlet_5272:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5273
    = _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(_M0L5itemsS1971);
    moonbit_decref(_M0L5itemsS1971);
    return _result_5273;
    join_1968:;
    _M0L6_2atmpS4141 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_5274
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_5274)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_5274->$0 = _M0L6_2atmpS4141;
    _block_5274->$1 = 0;
    return _block_5274;
  }
}

void* _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1955,
  moonbit_string_t _M0L3keyS1956,
  moonbit_string_t _M0L11member__valS1960
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1955, _M0L3keyS1956)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1959;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4139 =
      _M0L4selfS1955->$0;
    void* _M0L7_2abindS1961;
    void* _result_5277;
    moonbit_incref(_M0L4dataS4139);
    #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1961
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4139, _M0L3keyS1956);
    moonbit_decref(_M0L4dataS4139);
    if (_M0L7_2abindS1961 == 0) {
      if (_M0L7_2abindS1961) {
        moonbit_decref(_M0L7_2abindS1961);
      }
      goto join_1957;
    } else {
      void* _M0L7_2aSomeS1962 = _M0L7_2abindS1961;
      void* _M0L4_2axS1963 = _M0L7_2aSomeS1962;
      switch (Moonbit_object_tag(_M0L4_2axS1963)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1964 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1963;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4577 = _M0L7_2aZSetS1964->$0;
          int32_t _M0L6_2acntS5037 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1964));
          struct _M0TPB3MapGsfE* _M0L4_2azS1965;
          if (_M0L6_2acntS5037 > 1) {
            int32_t _M0L11_2anew__cntS5038 = _M0L6_2acntS5037 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1964), _M0L11_2anew__cntS5038);
            moonbit_incref(_M0L8_2afieldS4577);
          } else if (_M0L6_2acntS5037 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1964);
          }
          _M0L4_2azS1965 = _M0L8_2afieldS4577;
          _M0L1zS1959 = _M0L4_2azS1965;
          goto join_1958;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1963);
          goto join_1957;
          break;
        }
      }
    }
    join_1958:;
    #line 1239 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5277 = _M0MPB3Map3getGsfE(_M0L1zS1959, _M0L11member__valS1960);
    moonbit_decref(_M0L1zS1959);
    return _result_5277;
    join_1957:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5zcard(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1946,
  moonbit_string_t _M0L3keyS1947
) {
  #line 1223 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1224 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1946, _M0L3keyS1947)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1949;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4138 =
      _M0L4selfS1946->$0;
    void* _M0L7_2abindS1950;
    int32_t _result_5279;
    moonbit_incref(_M0L4dataS4138);
    #line 1227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1950
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4138, _M0L3keyS1947);
    moonbit_decref(_M0L4dataS4138);
    if (_M0L7_2abindS1950 == 0) {
      if (_M0L7_2abindS1950) {
        moonbit_decref(_M0L7_2abindS1950);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1951 = _M0L7_2abindS1950;
      void* _M0L4_2axS1952 = _M0L7_2aSomeS1951;
      switch (Moonbit_object_tag(_M0L4_2axS1952)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1953 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1952;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4579 = _M0L7_2aZSetS1953->$0;
          int32_t _M0L6_2acntS5039 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1953));
          struct _M0TPB3MapGsfE* _M0L4_2azS1954;
          if (_M0L6_2acntS5039 > 1) {
            int32_t _M0L11_2anew__cntS5040 = _M0L6_2acntS5039 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1953), _M0L11_2anew__cntS5040);
            moonbit_incref(_M0L8_2afieldS4579);
          } else if (_M0L6_2acntS5039 == 1) {
            #line 1227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1953);
          }
          _M0L4_2azS1954 = _M0L8_2afieldS4579;
          _M0L1zS1949 = _M0L4_2azS1954;
          goto join_1948;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1952);
          return 0;
          break;
        }
      }
    }
    join_1948:;
    #line 1228 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5279 = _M0MPB3Map6lengthGsfE(_M0L1zS1949);
    moonbit_decref(_M0L1zS1949);
    return _result_5279;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6zrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1916,
  moonbit_string_t _M0L3keyS1917,
  int32_t _M0L5startS1936,
  int32_t _M0L3endS1938
) {
  #line 1152 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1153 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1916, _M0L3keyS1917)
  ) {
    moonbit_string_t* _M0L6_2atmpS4129 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_5280 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5280)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5280->$0 = _M0L6_2atmpS4129;
    _block_5280->$1 = 0;
    return _block_5280;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1920;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4137 =
      _M0L4selfS1916->$0;
    void* _M0L7_2abindS1941;
    struct _M0TUsfE** _M0L6_2atmpS4136;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1921;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1922;
    struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1932;
    int32_t _M0L3lenS1933;
    moonbit_string_t* _M0L6_2atmpS4135;
    struct _M0TPB5ArrayGsE* _M0L6resultS1934;
    int32_t _M0L10start__idxS1935;
    int32_t _M0L8end__idxS1937;
    int32_t _M0L1iS1939;
    moonbit_string_t* _M0L6_2atmpS4130;
    struct _M0TPB5ArrayGsE* _block_5287;
    moonbit_incref(_M0L4dataS4137);
    #line 1156 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1941
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4137, _M0L3keyS1917);
    moonbit_decref(_M0L4dataS4137);
    if (_M0L7_2abindS1941 == 0) {
      if (_M0L7_2abindS1941) {
        moonbit_decref(_M0L7_2abindS1941);
      }
      goto join_1918;
    } else {
      void* _M0L7_2aSomeS1942 = _M0L7_2abindS1941;
      void* _M0L4_2axS1943 = _M0L7_2aSomeS1942;
      switch (Moonbit_object_tag(_M0L4_2axS1943)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1944 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1943;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4583 = _M0L7_2aZSetS1944->$0;
          int32_t _M0L6_2acntS5045 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1944));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1945;
          if (_M0L6_2acntS5045 > 1) {
            int32_t _M0L11_2anew__cntS5046 = _M0L6_2acntS5045 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1944), _M0L11_2anew__cntS5046);
            moonbit_incref(_M0L8_2afieldS4583);
          } else if (_M0L6_2acntS5045 == 1) {
            #line 1156 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1944);
          }
          _M0L7_2azsetS1945 = _M0L8_2afieldS4583;
          _M0L4zsetS1920 = _M0L7_2azsetS1945;
          goto join_1919;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1943);
          goto join_1918;
          break;
        }
      }
    }
    join_1919:;
    _M0L6_2atmpS4136 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1921
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1921)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L5itemsS1921->$0 = _M0L6_2atmpS4136;
    _M0L5itemsS1921->$1 = 0;
    #line 1158 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1922 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1920);
    moonbit_decref(_M0L4zsetS1920);
    while (1) {
      moonbit_string_t _M0L1mS1924;
      float _M0L1sS1925;
      struct _M0TUsfE* _M0L7_2abindS1927;
      struct _M0TUsfE* _M0L8_2atupleS4131;
      #line 1159 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1927 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1922);
      if (_M0L7_2abindS1927 == 0) {
        if (_M0L7_2abindS1927) {
          moonbit_decref(_M0L7_2abindS1927);
        }
        moonbit_decref(_M0L5_2aitS1922);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1928 = _M0L7_2abindS1927;
        struct _M0TUsfE* _M0L4_2axS1929 = _M0L7_2aSomeS1928;
        moonbit_string_t _M0L4_2amS1930 = _M0L4_2axS1929->$0;
        float _M0L4_2asS1931 = _M0L4_2axS1929->$1;
        int32_t _M0L6_2acntS5041 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1929));
        if (_M0L6_2acntS5041 > 1) {
          int32_t _M0L11_2anew__cntS5042 = _M0L6_2acntS5041 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1929), _M0L11_2anew__cntS5042);
          moonbit_incref(_M0L4_2amS1930);
        } else if (_M0L6_2acntS5041 == 1) {
          #line 1159 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1929);
        }
        _M0L1mS1924 = _M0L4_2amS1930;
        _M0L1sS1925 = _M0L4_2asS1931;
        goto join_1923;
      }
      goto joinlet_5284;
      join_1923:;
      _M0L8_2atupleS4131
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS4131)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 0);
      _M0L8_2atupleS4131->$0 = _M0L1mS1924;
      _M0L8_2atupleS4131->$1 = _M0L1sS1925;
      #line 1160 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1921, _M0L8_2atupleS4131);
      moonbit_decref(_M0L8_2atupleS4131);
      continue;
      joinlet_5284:;
      break;
    }
    #line 1162 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6sortedS1932
    = _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(_M0L5itemsS1921);
    moonbit_decref(_M0L5itemsS1921);
    #line 1163 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3lenS1933 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1932);
    _M0L6_2atmpS4135 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1934
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1934)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1934->$0 = _M0L6_2atmpS4135;
    _M0L6resultS1934->$1 = 0;
    if (_M0L5startS1936 < 0) {
      _M0L10start__idxS1935 = _M0L3lenS1933 + _M0L5startS1936;
    } else {
      _M0L10start__idxS1935 = _M0L5startS1936;
    }
    if (_M0L3endS1938 < 0) {
      _M0L8end__idxS1937 = _M0L3lenS1933 + _M0L3endS1938;
    } else {
      _M0L8end__idxS1937 = _M0L3endS1938;
    }
    _M0L1iS1939 = _M0L10start__idxS1935;
    while (1) {
      int32_t _if__result_5286;
      if (_M0L1iS1939 <= _M0L8end__idxS1937) {
        _if__result_5286 = _M0L1iS1939 < _M0L3lenS1933;
      } else {
        _if__result_5286 = 0;
      }
      if (_if__result_5286) {
        struct _M0TUsfE* _M0L6_2atmpS4133;
        moonbit_string_t _M0L8_2afieldS4581;
        int32_t _M0L6_2acntS5043;
        moonbit_string_t _M0L6_2atmpS4132;
        int32_t _M0L6_2atmpS4134;
        #line 1168 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS4133
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1932, _M0L1iS1939);
        _M0L8_2afieldS4581 = _M0L6_2atmpS4133->$0;
        _M0L6_2acntS5043
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS4133));
        if (_M0L6_2acntS5043 > 1) {
          int32_t _M0L11_2anew__cntS5044 = _M0L6_2acntS5043 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS4133), _M0L11_2anew__cntS5044);
          moonbit_incref(_M0L8_2afieldS4581);
        } else if (_M0L6_2acntS5043 == 1) {
          #line 1168 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2atmpS4133);
        }
        _M0L6_2atmpS4132 = _M0L8_2afieldS4581;
        #line 1168 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1934, _M0L6_2atmpS4132);
        moonbit_decref(_M0L6_2atmpS4132);
        _M0L6_2atmpS4134 = _M0L1iS1939 + 1;
        _M0L1iS1939 = _M0L6_2atmpS4134;
        continue;
      } else {
        moonbit_decref(_M0L6sortedS1932);
      }
      break;
    }
    return _M0L6resultS1934;
    join_1918:;
    _M0L6_2atmpS4130 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_5287
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5287)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5287->$0 = _M0L6_2atmpS4130;
    _block_5287->$1 = 0;
    return _block_5287;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1915
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  return _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L5itemsS1915);
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1907
) {
  int32_t _M0L3lenS1906;
  #line 1181 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1906 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1907);
  if (_M0L3lenS1906 <= 1) {
    moonbit_incref(_M0L3arrS1907);
    return _M0L3arrS1907;
  } else {
    int32_t _M0L3midS1908 = _M0L3lenS1906 / 2;
    struct _M0TUsfE** _M0L6_2atmpS4128 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1909 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS4127;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1910;
    int32_t _M0L1iS1911;
    int32_t _M0L1iS1913;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS4125;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS4126;
    struct _M0TPB5ArrayGUsfEE* _result_5290;
    Moonbit_object_header(_M0L4leftS1909)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L4leftS1909->$0 = _M0L6_2atmpS4128;
    _M0L4leftS1909->$1 = 0;
    _M0L6_2atmpS4127 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1910
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1910)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L5rightS1910->$0 = _M0L6_2atmpS4127;
    _M0L5rightS1910->$1 = 0;
    _M0L1iS1911 = 0;
    while (1) {
      if (_M0L1iS1911 < _M0L3midS1908) {
        struct _M0TUsfE* _M0L6_2atmpS4121;
        int32_t _M0L6_2atmpS4122;
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS4121
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1907, _M0L1iS1911);
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1909, _M0L6_2atmpS4121);
        moonbit_decref(_M0L6_2atmpS4121);
        _M0L6_2atmpS4122 = _M0L1iS1911 + 1;
        _M0L1iS1911 = _M0L6_2atmpS4122;
        continue;
      }
      break;
    }
    _M0L1iS1913 = _M0L3midS1908;
    while (1) {
      if (_M0L1iS1913 < _M0L3lenS1906) {
        struct _M0TUsfE* _M0L6_2atmpS4123;
        int32_t _M0L6_2atmpS4124;
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS4123
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1907, _M0L1iS1913);
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1910, _M0L6_2atmpS4123);
        moonbit_decref(_M0L6_2atmpS4123);
        _M0L6_2atmpS4124 = _M0L1iS1913 + 1;
        _M0L1iS1913 = _M0L6_2atmpS4124;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4125
    = _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L4leftS1909);
    moonbit_decref(_M0L4leftS1909);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4126
    = _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L5rightS1910);
    moonbit_decref(_M0L5rightS1910);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5290
    = _M0FP38JIA2JIA29moonbitdb3lib5merge(_M0L6_2atmpS4125, _M0L6_2atmpS4126);
    moonbit_decref(_M0L6_2atmpS4125);
    moonbit_decref(_M0L6_2atmpS4126);
    return _result_5290;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1901,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1902
) {
  struct _M0TUsfE** _M0L6_2atmpS4120;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1898;
  struct _M0TPB8MutLocalGiE* _M0L1iS1899;
  struct _M0TPB8MutLocalGiE* _M0L1jS1900;
  #line 1199 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4120 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1898
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1898)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6resultS1898->$0 = _M0L6_2atmpS4120;
  _M0L6resultS1898->$1 = 0;
  _M0L1iS1899
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1899)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1899->$0 = 0;
  _M0L1jS1900
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1900)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1900->$0 = 0;
  while (1) {
    int32_t _M0L3valS4092 = _M0L1iS1899->$0;
    int32_t _M0L6_2atmpS4093;
    int32_t _if__result_5292;
    #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4093 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1901);
    if (_M0L3valS4092 < _M0L6_2atmpS4093) {
      int32_t _M0L3valS4090 = _M0L1jS1900->$0;
      int32_t _M0L6_2atmpS4091;
      #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS4091 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1902);
      _if__result_5292 = _M0L3valS4090 < _M0L6_2atmpS4091;
    } else {
      _if__result_5292 = 0;
    }
    if (_if__result_5292) {
      int32_t _M0L3valS4099 = _M0L1iS1899->$0;
      struct _M0TUsfE* _M0L6_2atmpS4098;
      float _M0L6_2atmpS4094;
      int32_t _M0L3valS4097;
      struct _M0TUsfE* _M0L6_2atmpS4096;
      float _M0L6_2atmpS4095;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS4098
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1901, _M0L3valS4099);
      _M0L6_2atmpS4094 = _M0L6_2atmpS4098->$1;
      moonbit_decref(_M0L6_2atmpS4098);
      _M0L3valS4097 = _M0L1jS1900->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS4096
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1902, _M0L3valS4097);
      _M0L6_2atmpS4095 = _M0L6_2atmpS4096->$1;
      moonbit_decref(_M0L6_2atmpS4096);
      if (_M0L6_2atmpS4094 <= _M0L6_2atmpS4095) {
        int32_t _M0L3valS4101 = _M0L1iS1899->$0;
        struct _M0TUsfE* _M0L6_2atmpS4100;
        int32_t _M0L3valS4103;
        int32_t _M0L6_2atmpS4102;
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS4100
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1901, _M0L3valS4101);
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1898, _M0L6_2atmpS4100);
        moonbit_decref(_M0L6_2atmpS4100);
        _M0L3valS4103 = _M0L1iS1899->$0;
        _M0L6_2atmpS4102 = _M0L3valS4103 + 1;
        _M0L1iS1899->$0 = _M0L6_2atmpS4102;
      } else {
        int32_t _M0L3valS4105 = _M0L1jS1900->$0;
        struct _M0TUsfE* _M0L6_2atmpS4104;
        int32_t _M0L3valS4107;
        int32_t _M0L6_2atmpS4106;
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS4104
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1902, _M0L3valS4105);
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1898, _M0L6_2atmpS4104);
        moonbit_decref(_M0L6_2atmpS4104);
        _M0L3valS4107 = _M0L1jS1900->$0;
        _M0L6_2atmpS4106 = _M0L3valS4107 + 1;
        _M0L1jS1900->$0 = _M0L6_2atmpS4106;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS4108 = _M0L1iS1899->$0;
    int32_t _M0L6_2atmpS4109;
    #line 1212 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4109 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1901);
    if (_M0L3valS4108 < _M0L6_2atmpS4109) {
      int32_t _M0L3valS4111 = _M0L1iS1899->$0;
      struct _M0TUsfE* _M0L6_2atmpS4110;
      int32_t _M0L3valS4113;
      int32_t _M0L6_2atmpS4112;
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS4110
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1901, _M0L3valS4111);
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1898, _M0L6_2atmpS4110);
      moonbit_decref(_M0L6_2atmpS4110);
      _M0L3valS4113 = _M0L1iS1899->$0;
      _M0L6_2atmpS4112 = _M0L3valS4113 + 1;
      _M0L1iS1899->$0 = _M0L6_2atmpS4112;
      continue;
    } else {
      moonbit_decref(_M0L1iS1899);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS4114 = _M0L1jS1900->$0;
    int32_t _M0L6_2atmpS4115;
    #line 1216 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4115 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1902);
    if (_M0L3valS4114 < _M0L6_2atmpS4115) {
      int32_t _M0L3valS4117 = _M0L1jS1900->$0;
      struct _M0TUsfE* _M0L6_2atmpS4116;
      int32_t _M0L3valS4119;
      int32_t _M0L6_2atmpS4118;
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS4116
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1902, _M0L3valS4117);
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1898, _M0L6_2atmpS4116);
      moonbit_decref(_M0L6_2atmpS4116);
      _M0L3valS4119 = _M0L1jS1900->$0;
      _M0L6_2atmpS4118 = _M0L3valS4119 + 1;
      _M0L1jS1900->$0 = _M0L6_2atmpS4118;
      continue;
    } else {
      moonbit_decref(_M0L1jS1900);
    }
    break;
  }
  return _M0L6resultS1898;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1883,
  moonbit_string_t _M0L3keyS1884,
  float _M0L5scoreS1897,
  moonbit_string_t _M0L11member__valS1896
) {
  int32_t _M0L6_2atmpS4084;
  struct _M0TPB3MapGsfE* _M0L4zsetS1885;
  struct _M0TPB3MapGsfE* _M0L1zS1889;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4089;
  void* _M0L7_2abindS1890;
  struct _M0TUsfE** _M0L7_2abindS1887;
  struct _M0TUsfE** _M0L6_2atmpS4088;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS4087;
  int32_t _M0L7existedS1895;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4085;
  void* _M0L4ZSetS4086;
  #line 1140 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4084
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1883, _M0L3keyS1884);
  _M0L4dataS4089 = _M0L4selfS1883->$0;
  moonbit_incref(_M0L4dataS4089);
  #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1890
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4089, _M0L3keyS1884);
  moonbit_decref(_M0L4dataS4089);
  if (_M0L7_2abindS1890 == 0) {
    if (_M0L7_2abindS1890) {
      moonbit_decref(_M0L7_2abindS1890);
    }
    goto join_1886;
  } else {
    void* _M0L7_2aSomeS1891 = _M0L7_2abindS1890;
    void* _M0L4_2axS1892 = _M0L7_2aSomeS1891;
    switch (Moonbit_object_tag(_M0L4_2axS1892)) {
      case 4: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1893 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1892;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS4586 = _M0L7_2aZSetS1893->$0;
        int32_t _M0L6_2acntS5047 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1893));
        struct _M0TPB3MapGsfE* _M0L4_2azS1894;
        if (_M0L6_2acntS5047 > 1) {
          int32_t _M0L11_2anew__cntS5048 = _M0L6_2acntS5047 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1893), _M0L11_2anew__cntS5048);
          moonbit_incref(_M0L8_2afieldS4586);
        } else if (_M0L6_2acntS5047 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aZSetS1893);
        }
        _M0L4_2azS1894 = _M0L8_2afieldS4586;
        _M0L1zS1889 = _M0L4_2azS1894;
        goto join_1888;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1892);
        goto join_1886;
        break;
      }
    }
  }
  goto joinlet_5296;
  join_1888:;
  _M0L4zsetS1885 = _M0L1zS1889;
  joinlet_5296:;
  goto joinlet_5295;
  join_1886:;
  _M0L7_2abindS1887 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4088 = _M0L7_2abindS1887;
  _M0L6_2atmpS4087
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS4088, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4zsetS1885 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS4087, 10ll);
  moonbit_decref(_M0L6_2atmpS4087.$0);
  joinlet_5295:;
  #line 1146 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7existedS1895
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1885, _M0L11member__valS1896);
  #line 1147 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1885, _M0L11member__valS1896, _M0L5scoreS1897);
  _M0L4dataS4085 = _M0L4selfS1883->$0;
  _M0L4ZSetS4086
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS4086)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 4);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4ZSetS4086)->$0
  = _M0L4zsetS1885;
  moonbit_incref(_M0L4dataS4085);
  #line 1148 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4085, _M0L3keyS1884, _M0L4ZSetS4086);
  moonbit_decref(_M0L4dataS4085);
  moonbit_decref(_M0L4ZSetS4086);
  return !_M0L7existedS1895;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database9sismember(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1873,
  moonbit_string_t _M0L3keyS1874,
  moonbit_string_t _M0L5valueS1877
) {
  #line 963 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 964 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1873, _M0L3keyS1874)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1876;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4083 =
      _M0L4selfS1873->$0;
    void* _M0L7_2abindS1878;
    int32_t _result_5298;
    moonbit_incref(_M0L4dataS4083);
    #line 967 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1878
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4083, _M0L3keyS1874);
    moonbit_decref(_M0L4dataS4083);
    if (_M0L7_2abindS1878 == 0) {
      if (_M0L7_2abindS1878) {
        moonbit_decref(_M0L7_2abindS1878);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1879 = _M0L7_2abindS1878;
      void* _M0L4_2axS1880 = _M0L7_2aSomeS1879;
      switch (Moonbit_object_tag(_M0L4_2axS1880)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1881 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1880;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4588 = _M0L6_2aSetS1881->$0;
          int32_t _M0L6_2acntS5049 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1881));
          struct _M0TPB3MapGsbE* _M0L4_2asS1882;
          if (_M0L6_2acntS5049 > 1) {
            int32_t _M0L11_2anew__cntS5050 = _M0L6_2acntS5049 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1881), _M0L11_2anew__cntS5050);
            moonbit_incref(_M0L8_2afieldS4588);
          } else if (_M0L6_2acntS5049 == 1) {
            #line 967 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1881);
          }
          _M0L4_2asS1882 = _M0L8_2afieldS4588;
          _M0L1sS1876 = _M0L4_2asS1882;
          goto join_1875;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1880);
          return 0;
          break;
        }
      }
    }
    join_1875:;
    #line 968 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5298 = _M0MPB3Map8containsGsbE(_M0L1sS1876, _M0L5valueS1877);
    moonbit_decref(_M0L1sS1876);
    return _result_5298;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5scard(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1864,
  moonbit_string_t _M0L3keyS1865
) {
  #line 952 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 953 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1864, _M0L3keyS1865)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1867;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4082 =
      _M0L4selfS1864->$0;
    void* _M0L7_2abindS1868;
    int32_t _result_5300;
    moonbit_incref(_M0L4dataS4082);
    #line 956 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1868
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4082, _M0L3keyS1865);
    moonbit_decref(_M0L4dataS4082);
    if (_M0L7_2abindS1868 == 0) {
      if (_M0L7_2abindS1868) {
        moonbit_decref(_M0L7_2abindS1868);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1869 = _M0L7_2abindS1868;
      void* _M0L4_2axS1870 = _M0L7_2aSomeS1869;
      switch (Moonbit_object_tag(_M0L4_2axS1870)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1871 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1870;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4590 = _M0L6_2aSetS1871->$0;
          int32_t _M0L6_2acntS5051 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1871));
          struct _M0TPB3MapGsbE* _M0L4_2asS1872;
          if (_M0L6_2acntS5051 > 1) {
            int32_t _M0L11_2anew__cntS5052 = _M0L6_2acntS5051 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1871), _M0L11_2anew__cntS5052);
            moonbit_incref(_M0L8_2afieldS4590);
          } else if (_M0L6_2acntS5051 == 1) {
            #line 956 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1871);
          }
          _M0L4_2asS1872 = _M0L8_2afieldS4590;
          _M0L1sS1867 = _M0L4_2asS1872;
          goto join_1866;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1870);
          return 0;
          break;
        }
      }
    }
    join_1866:;
    #line 957 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5300 = _M0MPB3Map6lengthGsbE(_M0L1sS1867);
    moonbit_decref(_M0L1sS1867);
    return _result_5300;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4srem(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1853,
  moonbit_string_t _M0L3keyS1854,
  moonbit_string_t _M0L5valueS1858
) {
  #line 934 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 935 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1853, _M0L3keyS1854)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L3setS1856;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4081 =
      _M0L4selfS1853->$0;
    void* _M0L7_2abindS1859;
    int32_t _M0L7existedS1857;
    moonbit_incref(_M0L4dataS4081);
    #line 938 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1859
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4081, _M0L3keyS1854);
    moonbit_decref(_M0L4dataS4081);
    if (_M0L7_2abindS1859 == 0) {
      if (_M0L7_2abindS1859) {
        moonbit_decref(_M0L7_2abindS1859);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1860 = _M0L7_2abindS1859;
      void* _M0L4_2axS1861 = _M0L7_2aSomeS1860;
      switch (Moonbit_object_tag(_M0L4_2axS1861)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1862 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1861;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4593 = _M0L6_2aSetS1862->$0;
          int32_t _M0L6_2acntS5053 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1862));
          struct _M0TPB3MapGsbE* _M0L6_2asetS1863;
          if (_M0L6_2acntS5053 > 1) {
            int32_t _M0L11_2anew__cntS5054 = _M0L6_2acntS5053 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1862), _M0L11_2anew__cntS5054);
            moonbit_incref(_M0L8_2afieldS4593);
          } else if (_M0L6_2acntS5053 == 1) {
            #line 938 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1862);
          }
          _M0L6_2asetS1863 = _M0L8_2afieldS4593;
          _M0L3setS1856 = _M0L6_2asetS1863;
          goto join_1855;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1861);
          return 0;
          break;
        }
      }
    }
    join_1855:;
    #line 940 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7existedS1857
    = _M0MPB3Map8containsGsbE(_M0L3setS1856, _M0L5valueS1858);
    if (_M0L7existedS1857) {
      struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4079;
      void* _M0L3SetS4080;
      #line 942 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGsbE(_M0L3setS1856, _M0L5valueS1858);
      _M0L4dataS4079 = _M0L4selfS1853->$0;
      _M0L3SetS4080
      = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set));
      Moonbit_object_header(_M0L3SetS4080)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 12, 3);
      ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L3SetS4080)->$0
      = _M0L3setS1856;
      moonbit_incref(_M0L4dataS4079);
      #line 943 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4079, _M0L3keyS1854, _M0L3SetS4080);
      moonbit_decref(_M0L4dataS4079);
      moonbit_decref(_M0L3SetS4080);
    } else {
      moonbit_decref(_M0L3setS1856);
    }
    return _M0L7existedS1857;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database8smembers(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1834,
  moonbit_string_t _M0L3keyS1835
) {
  #line 917 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 918 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1834, _M0L3keyS1835)
  ) {
    moonbit_string_t* _M0L6_2atmpS4075 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_5302 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5302)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5302->$0 = _M0L6_2atmpS4075;
    _block_5302->$1 = 0;
    return _block_5302;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1838;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4078 =
      _M0L4selfS1834->$0;
    void* _M0L7_2abindS1848;
    moonbit_string_t* _M0L6_2atmpS4077;
    struct _M0TPB5ArrayGsE* _M0L6resultS1839;
    struct _M0TPB4IterGUsbEE* _M0L5_2aitS1840;
    moonbit_string_t* _M0L6_2atmpS4076;
    struct _M0TPB5ArrayGsE* _block_5307;
    moonbit_incref(_M0L4dataS4078);
    #line 921 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1848
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4078, _M0L3keyS1835);
    moonbit_decref(_M0L4dataS4078);
    if (_M0L7_2abindS1848 == 0) {
      if (_M0L7_2abindS1848) {
        moonbit_decref(_M0L7_2abindS1848);
      }
      goto join_1836;
    } else {
      void* _M0L7_2aSomeS1849 = _M0L7_2abindS1848;
      void* _M0L4_2axS1850 = _M0L7_2aSomeS1849;
      switch (Moonbit_object_tag(_M0L4_2axS1850)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1851 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1850;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4596 = _M0L6_2aSetS1851->$0;
          int32_t _M0L6_2acntS5057 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1851));
          struct _M0TPB3MapGsbE* _M0L4_2asS1852;
          if (_M0L6_2acntS5057 > 1) {
            int32_t _M0L11_2anew__cntS5058 = _M0L6_2acntS5057 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1851), _M0L11_2anew__cntS5058);
            moonbit_incref(_M0L8_2afieldS4596);
          } else if (_M0L6_2acntS5057 == 1) {
            #line 921 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1851);
          }
          _M0L4_2asS1852 = _M0L8_2afieldS4596;
          _M0L1sS1838 = _M0L4_2asS1852;
          goto join_1837;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1850);
          goto join_1836;
          break;
        }
      }
    }
    join_1837:;
    _M0L6_2atmpS4077 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1839
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1839)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1839->$0 = _M0L6_2atmpS4077;
    _M0L6resultS1839->$1 = 0;
    #line 923 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1840 = _M0MPB3Map5iter2GsbE(_M0L1sS1838);
    moonbit_decref(_M0L1sS1838);
    while (1) {
      moonbit_string_t _M0L1mS1842;
      struct _M0TUsbE* _M0L7_2abindS1844;
      #line 924 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1844 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1840);
      if (_M0L7_2abindS1844 == 0) {
        if (_M0L7_2abindS1844) {
          moonbit_decref(_M0L7_2abindS1844);
        }
        moonbit_decref(_M0L5_2aitS1840);
      } else {
        struct _M0TUsbE* _M0L7_2aSomeS1845 = _M0L7_2abindS1844;
        struct _M0TUsbE* _M0L4_2axS1846 = _M0L7_2aSomeS1845;
        moonbit_string_t _M0L8_2afieldS4595 = _M0L4_2axS1846->$0;
        int32_t _M0L6_2acntS5055 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1846));
        moonbit_string_t _M0L4_2amS1847;
        if (_M0L6_2acntS5055 > 1) {
          int32_t _M0L11_2anew__cntS5056 = _M0L6_2acntS5055 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1846), _M0L11_2anew__cntS5056);
          moonbit_incref(_M0L8_2afieldS4595);
        } else if (_M0L6_2acntS5055 == 1) {
          #line 924 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1846);
        }
        _M0L4_2amS1847 = _M0L8_2afieldS4595;
        _M0L1mS1842 = _M0L4_2amS1847;
        goto join_1841;
      }
      goto joinlet_5306;
      join_1841:;
      #line 925 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1839, _M0L1mS1842);
      moonbit_decref(_M0L1mS1842);
      continue;
      joinlet_5306:;
      break;
    }
    return _M0L6resultS1839;
    join_1836:;
    _M0L6_2atmpS4076 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_5307
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5307)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5307->$0 = _M0L6_2atmpS4076;
    _block_5307->$1 = 0;
    return _block_5307;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1821,
  moonbit_string_t _M0L3keyS1822,
  moonbit_string_t _M0L5valueS1833
) {
  int32_t _M0L6_2atmpS4069;
  struct _M0TPB3MapGsbE* _M0L3setS1823;
  struct _M0TPB3MapGsbE* _M0L1sS1827;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4074;
  void* _M0L7_2abindS1828;
  struct _M0TUsbE** _M0L7_2abindS1825;
  struct _M0TUsbE** _M0L6_2atmpS4073;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS4072;
  #line 902 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4069
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1821, _M0L3keyS1822);
  _M0L4dataS4074 = _M0L4selfS1821->$0;
  moonbit_incref(_M0L4dataS4074);
  #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1828
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4074, _M0L3keyS1822);
  moonbit_decref(_M0L4dataS4074);
  if (_M0L7_2abindS1828 == 0) {
    if (_M0L7_2abindS1828) {
      moonbit_decref(_M0L7_2abindS1828);
    }
    goto join_1824;
  } else {
    void* _M0L7_2aSomeS1829 = _M0L7_2abindS1828;
    void* _M0L4_2axS1830 = _M0L7_2aSomeS1829;
    switch (Moonbit_object_tag(_M0L4_2axS1830)) {
      case 3: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1831 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1830;
        struct _M0TPB3MapGsbE* _M0L8_2afieldS4599 = _M0L6_2aSetS1831->$0;
        int32_t _M0L6_2acntS5059 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1831));
        struct _M0TPB3MapGsbE* _M0L4_2asS1832;
        if (_M0L6_2acntS5059 > 1) {
          int32_t _M0L11_2anew__cntS5060 = _M0L6_2acntS5059 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1831), _M0L11_2anew__cntS5060);
          moonbit_incref(_M0L8_2afieldS4599);
        } else if (_M0L6_2acntS5059 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2aSetS1831);
        }
        _M0L4_2asS1832 = _M0L8_2afieldS4599;
        _M0L1sS1827 = _M0L4_2asS1832;
        goto join_1826;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1830);
        goto join_1824;
        break;
      }
    }
  }
  goto joinlet_5309;
  join_1826:;
  _M0L3setS1823 = _M0L1sS1827;
  joinlet_5309:;
  goto joinlet_5308;
  join_1824:;
  _M0L7_2abindS1825 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4073 = _M0L7_2abindS1825;
  _M0L6_2atmpS4072
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS4073, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3setS1823 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS4072, 10ll);
  moonbit_decref(_M0L6_2atmpS4072.$0);
  joinlet_5308:;
  #line 908 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1823, _M0L5valueS1833)) {
    moonbit_decref(_M0L3setS1823);
    return 0;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4070;
    void* _M0L3SetS4071;
    #line 911 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1823, _M0L5valueS1833, 1);
    _M0L4dataS4070 = _M0L4selfS1821->$0;
    _M0L3SetS4071
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS4071)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 12, 3);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L3SetS4071)->$0
    = _M0L3setS1823;
    moonbit_incref(_M0L4dataS4070);
    #line 912 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4070, _M0L3keyS1822, _M0L3SetS4071);
    moonbit_decref(_M0L4dataS4070);
    moonbit_decref(_M0L3SetS4071);
    return 1;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6lrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1802,
  moonbit_string_t _M0L3keyS1803,
  int32_t _M0L5startS1810,
  int32_t _M0L3endS1812
) {
  #line 720 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 721 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1802, _M0L3keyS1803)
  ) {
    moonbit_string_t* _M0L6_2atmpS4063 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_5310 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5310)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5310->$0 = _M0L6_2atmpS4063;
    _block_5310->$1 = 0;
    return _block_5310;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1806;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4068 =
      _M0L4selfS1802->$0;
    void* _M0L7_2abindS1816;
    struct _M0TPB5ArrayGsE* _M0L3arrS1807;
    int32_t _M0L3lenS1808;
    int32_t _M0L10start__idxS1809;
    int32_t _M0L8end__idxS1811;
    moonbit_string_t* _M0L6_2atmpS4067;
    struct _M0TPB5ArrayGsE* _M0L6resultS1813;
    int32_t _M0L1iS1814;
    moonbit_string_t* _M0L6_2atmpS4064;
    struct _M0TPB5ArrayGsE* _block_5315;
    moonbit_incref(_M0L4dataS4068);
    #line 724 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1816
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4068, _M0L3keyS1803);
    moonbit_decref(_M0L4dataS4068);
    if (_M0L7_2abindS1816 == 0) {
      if (_M0L7_2abindS1816) {
        moonbit_decref(_M0L7_2abindS1816);
      }
      goto join_1804;
    } else {
      void* _M0L7_2aSomeS1817 = _M0L7_2abindS1816;
      void* _M0L4_2axS1818 = _M0L7_2aSomeS1817;
      switch (Moonbit_object_tag(_M0L4_2axS1818)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1819 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1818;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS4601 =
            _M0L7_2aListS1819->$0;
          int32_t _M0L6_2acntS5061 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1819));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2adequeS1820;
          if (_M0L6_2acntS5061 > 1) {
            int32_t _M0L11_2anew__cntS5062 = _M0L6_2acntS5061 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1819), _M0L11_2anew__cntS5062);
            moonbit_incref(_M0L8_2afieldS4601);
          } else if (_M0L6_2acntS5061 == 1) {
            #line 724 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1819);
          }
          _M0L8_2adequeS1820 = _M0L8_2afieldS4601;
          _M0L5dequeS1806 = _M0L8_2adequeS1820;
          goto join_1805;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1818);
          goto join_1804;
          break;
        }
      }
    }
    join_1805:;
    #line 726 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3arrS1807
    = _M0MP38JIA2JIA29moonbitdb3lib5Deque9to__array(_M0L5dequeS1806);
    moonbit_decref(_M0L5dequeS1806);
    #line 727 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3lenS1808 = _M0MPC15array5Array6lengthGsE(_M0L3arrS1807);
    if (_M0L5startS1810 < 0) {
      _M0L10start__idxS1809 = _M0L3lenS1808 + _M0L5startS1810;
    } else {
      _M0L10start__idxS1809 = _M0L5startS1810;
    }
    if (_M0L3endS1812 < 0) {
      _M0L8end__idxS1811 = _M0L3lenS1808 + _M0L3endS1812;
    } else {
      _M0L8end__idxS1811 = _M0L3endS1812;
    }
    _M0L6_2atmpS4067 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1813
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1813)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1813->$0 = _M0L6_2atmpS4067;
    _M0L6resultS1813->$1 = 0;
    _M0L1iS1814 = _M0L10start__idxS1809;
    while (1) {
      int32_t _if__result_5314;
      if (_M0L1iS1814 <= _M0L8end__idxS1811) {
        if (_M0L1iS1814 >= 0) {
          _if__result_5314 = _M0L1iS1814 < _M0L3lenS1808;
        } else {
          _if__result_5314 = 0;
        }
      } else {
        _if__result_5314 = 0;
      }
      if (_if__result_5314) {
        moonbit_string_t _M0L6_2atmpS4065;
        int32_t _M0L6_2atmpS4066;
        #line 732 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS4065
        = _M0MPC15array5Array2atGsE(_M0L3arrS1807, _M0L1iS1814);
        #line 732 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1813, _M0L6_2atmpS4065);
        moonbit_decref(_M0L6_2atmpS4065);
        _M0L6_2atmpS4066 = _M0L1iS1814 + 1;
        _M0L1iS1814 = _M0L6_2atmpS4066;
        continue;
      } else {
        moonbit_decref(_M0L3arrS1807);
      }
      break;
    }
    return _M0L6resultS1813;
    join_1804:;
    _M0L6_2atmpS4064 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_5315
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5315)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5315->$0 = _M0L6_2atmpS4064;
    _block_5315->$1 = 0;
    return _block_5315;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1793,
  moonbit_string_t _M0L3keyS1794
) {
  #line 709 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1793, _M0L3keyS1794)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1796;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4062 =
      _M0L4selfS1793->$0;
    void* _M0L7_2abindS1797;
    int32_t _result_5317;
    moonbit_incref(_M0L4dataS4062);
    #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1797
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4062, _M0L3keyS1794);
    moonbit_decref(_M0L4dataS4062);
    if (_M0L7_2abindS1797 == 0) {
      if (_M0L7_2abindS1797) {
        moonbit_decref(_M0L7_2abindS1797);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1798 = _M0L7_2abindS1797;
      void* _M0L4_2axS1799 = _M0L7_2aSomeS1798;
      switch (Moonbit_object_tag(_M0L4_2axS1799)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1800 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1799;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS4603 =
            _M0L7_2aListS1800->$0;
          int32_t _M0L6_2acntS5063 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1800));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1801;
          if (_M0L6_2acntS5063 > 1) {
            int32_t _M0L11_2anew__cntS5064 = _M0L6_2acntS5063 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1800), _M0L11_2anew__cntS5064);
            moonbit_incref(_M0L8_2afieldS4603);
          } else if (_M0L6_2acntS5063 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1800);
          }
          _M0L4_2adS1801 = _M0L8_2afieldS4603;
          _M0L1dS1796 = _M0L4_2adS1801;
          goto join_1795;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1799);
          return 0;
          break;
        }
      }
    }
    join_1795:;
    #line 714 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5317 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L1dS1796);
    moonbit_decref(_M0L1dS1796);
    return _result_5317;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4rpop(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1782,
  moonbit_string_t _M0L3keyS1783
) {
  #line 694 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 695 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1782, _M0L3keyS1783)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1786;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4061 =
      _M0L4selfS1782->$0;
    void* _M0L7_2abindS1788;
    moonbit_string_t _M0L3valS1787;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4059;
    void* _M0L4ListS4060;
    moonbit_incref(_M0L4dataS4061);
    #line 698 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1788
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4061, _M0L3keyS1783);
    moonbit_decref(_M0L4dataS4061);
    if (_M0L7_2abindS1788 == 0) {
      if (_M0L7_2abindS1788) {
        moonbit_decref(_M0L7_2abindS1788);
      }
      goto join_1784;
    } else {
      void* _M0L7_2aSomeS1789 = _M0L7_2abindS1788;
      void* _M0L4_2axS1790 = _M0L7_2aSomeS1789;
      switch (Moonbit_object_tag(_M0L4_2axS1790)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1791 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1790;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS4606 =
            _M0L7_2aListS1791->$0;
          int32_t _M0L6_2acntS5065 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1791));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2adequeS1792;
          if (_M0L6_2acntS5065 > 1) {
            int32_t _M0L11_2anew__cntS5066 = _M0L6_2acntS5065 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1791), _M0L11_2anew__cntS5066);
            moonbit_incref(_M0L8_2afieldS4606);
          } else if (_M0L6_2acntS5065 == 1) {
            #line 698 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1791);
          }
          _M0L8_2adequeS1792 = _M0L8_2afieldS4606;
          _M0L5dequeS1786 = _M0L8_2adequeS1792;
          goto join_1785;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1790);
          goto join_1784;
          break;
        }
      }
    }
    join_1785:;
    #line 700 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3valS1787
    = _M0MP38JIA2JIA29moonbitdb3lib5Deque9pop__back(_M0L5dequeS1786);
    _M0L4dataS4059 = _M0L4selfS1782->$0;
    _M0L4ListS4060
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
    Moonbit_object_header(_M0L4ListS4060)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS4060)->$0
    = _M0L5dequeS1786;
    moonbit_incref(_M0L4dataS4059);
    #line 701 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4059, _M0L3keyS1783, _M0L4ListS4060);
    moonbit_decref(_M0L4dataS4059);
    moonbit_decref(_M0L4ListS4060);
    return _M0L3valS1787;
    join_1784:;
    return 0;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4lpop(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1771,
  moonbit_string_t _M0L3keyS1772
) {
  #line 679 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 680 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1771, _M0L3keyS1772)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1775;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4058 =
      _M0L4selfS1771->$0;
    void* _M0L7_2abindS1777;
    moonbit_string_t _M0L3valS1776;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4056;
    void* _M0L4ListS4057;
    moonbit_incref(_M0L4dataS4058);
    #line 683 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1777
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4058, _M0L3keyS1772);
    moonbit_decref(_M0L4dataS4058);
    if (_M0L7_2abindS1777 == 0) {
      if (_M0L7_2abindS1777) {
        moonbit_decref(_M0L7_2abindS1777);
      }
      goto join_1773;
    } else {
      void* _M0L7_2aSomeS1778 = _M0L7_2abindS1777;
      void* _M0L4_2axS1779 = _M0L7_2aSomeS1778;
      switch (Moonbit_object_tag(_M0L4_2axS1779)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1780 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1779;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS4609 =
            _M0L7_2aListS1780->$0;
          int32_t _M0L6_2acntS5067 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1780));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2adequeS1781;
          if (_M0L6_2acntS5067 > 1) {
            int32_t _M0L11_2anew__cntS5068 = _M0L6_2acntS5067 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1780), _M0L11_2anew__cntS5068);
            moonbit_incref(_M0L8_2afieldS4609);
          } else if (_M0L6_2acntS5067 == 1) {
            #line 683 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1780);
          }
          _M0L8_2adequeS1781 = _M0L8_2afieldS4609;
          _M0L5dequeS1775 = _M0L8_2adequeS1781;
          goto join_1774;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1779);
          goto join_1773;
          break;
        }
      }
    }
    join_1774:;
    #line 685 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3valS1776
    = _M0MP38JIA2JIA29moonbitdb3lib5Deque10pop__front(_M0L5dequeS1775);
    _M0L4dataS4056 = _M0L4selfS1771->$0;
    _M0L4ListS4057
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
    Moonbit_object_header(_M0L4ListS4057)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS4057)->$0
    = _M0L5dequeS1775;
    moonbit_incref(_M0L4dataS4056);
    #line 686 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4056, _M0L3keyS1772, _M0L4ListS4057);
    moonbit_decref(_M0L4dataS4056);
    moonbit_decref(_M0L4ListS4057);
    return _M0L3valS1776;
    join_1773:;
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1759,
  moonbit_string_t _M0L3keyS1760,
  moonbit_string_t _M0L5valueS1770
) {
  int32_t _M0L6_2atmpS4052;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1761;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1764;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4055;
  void* _M0L7_2abindS1765;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4053;
  void* _M0L4ListS4054;
  int32_t _result_5324;
  #line 668 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 669 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4052
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1759, _M0L3keyS1760);
  _M0L4dataS4055 = _M0L4selfS1759->$0;
  moonbit_incref(_M0L4dataS4055);
  #line 670 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1765
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4055, _M0L3keyS1760);
  moonbit_decref(_M0L4dataS4055);
  if (_M0L7_2abindS1765 == 0) {
    if (_M0L7_2abindS1765) {
      moonbit_decref(_M0L7_2abindS1765);
    }
    goto join_1762;
  } else {
    void* _M0L7_2aSomeS1766 = _M0L7_2abindS1765;
    void* _M0L4_2axS1767 = _M0L7_2aSomeS1766;
    switch (Moonbit_object_tag(_M0L4_2axS1767)) {
      case 2: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1768 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1767;
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS4612 =
          _M0L7_2aListS1768->$0;
        int32_t _M0L6_2acntS5069 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1768));
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1769;
        if (_M0L6_2acntS5069 > 1) {
          int32_t _M0L11_2anew__cntS5070 = _M0L6_2acntS5069 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1768), _M0L11_2anew__cntS5070);
          moonbit_incref(_M0L8_2afieldS4612);
        } else if (_M0L6_2acntS5069 == 1) {
          #line 670 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aListS1768);
        }
        _M0L4_2adS1769 = _M0L8_2afieldS4612;
        _M0L1dS1764 = _M0L4_2adS1769;
        goto join_1763;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1767);
        goto join_1762;
        break;
      }
    }
  }
  goto joinlet_5323;
  join_1763:;
  _M0L5dequeS1761 = _M0L1dS1764;
  joinlet_5323:;
  goto joinlet_5322;
  join_1762:;
  #line 672 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5dequeS1761 = _M0MP38JIA2JIA29moonbitdb3lib5Deque3new();
  joinlet_5322:;
  #line 674 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib5Deque10push__back(_M0L5dequeS1761, _M0L5valueS1770);
  _M0L4dataS4053 = _M0L4selfS1759->$0;
  moonbit_incref(_M0L5dequeS1761);
  _M0L4ListS4054
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
  Moonbit_object_header(_M0L4ListS4054)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS4054)->$0
  = _M0L5dequeS1761;
  moonbit_incref(_M0L4dataS4053);
  #line 675 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4053, _M0L3keyS1760, _M0L4ListS4054);
  moonbit_decref(_M0L4dataS4053);
  moonbit_decref(_M0L4ListS4054);
  #line 676 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_5324 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L5dequeS1761);
  moonbit_decref(_M0L5dequeS1761);
  return _result_5324;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5lpush(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1747,
  moonbit_string_t _M0L3keyS1748,
  moonbit_string_t _M0L5valueS1758
) {
  int32_t _M0L6_2atmpS4048;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1749;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1752;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4051;
  void* _M0L7_2abindS1753;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4049;
  void* _M0L4ListS4050;
  int32_t _result_5327;
  #line 657 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 658 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4048
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1747, _M0L3keyS1748);
  _M0L4dataS4051 = _M0L4selfS1747->$0;
  moonbit_incref(_M0L4dataS4051);
  #line 659 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1753
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4051, _M0L3keyS1748);
  moonbit_decref(_M0L4dataS4051);
  if (_M0L7_2abindS1753 == 0) {
    if (_M0L7_2abindS1753) {
      moonbit_decref(_M0L7_2abindS1753);
    }
    goto join_1750;
  } else {
    void* _M0L7_2aSomeS1754 = _M0L7_2abindS1753;
    void* _M0L4_2axS1755 = _M0L7_2aSomeS1754;
    switch (Moonbit_object_tag(_M0L4_2axS1755)) {
      case 2: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1756 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1755;
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS4615 =
          _M0L7_2aListS1756->$0;
        int32_t _M0L6_2acntS5071 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1756));
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1757;
        if (_M0L6_2acntS5071 > 1) {
          int32_t _M0L11_2anew__cntS5072 = _M0L6_2acntS5071 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1756), _M0L11_2anew__cntS5072);
          moonbit_incref(_M0L8_2afieldS4615);
        } else if (_M0L6_2acntS5071 == 1) {
          #line 659 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aListS1756);
        }
        _M0L4_2adS1757 = _M0L8_2afieldS4615;
        _M0L1dS1752 = _M0L4_2adS1757;
        goto join_1751;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1755);
        goto join_1750;
        break;
      }
    }
  }
  goto joinlet_5326;
  join_1751:;
  _M0L5dequeS1749 = _M0L1dS1752;
  joinlet_5326:;
  goto joinlet_5325;
  join_1750:;
  #line 661 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5dequeS1749 = _M0MP38JIA2JIA29moonbitdb3lib5Deque3new();
  joinlet_5325:;
  #line 663 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib5Deque11push__front(_M0L5dequeS1749, _M0L5valueS1758);
  _M0L4dataS4049 = _M0L4selfS1747->$0;
  moonbit_incref(_M0L5dequeS1749);
  _M0L4ListS4050
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
  Moonbit_object_header(_M0L4ListS4050)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS4050)->$0
  = _M0L5dequeS1749;
  moonbit_incref(_M0L4dataS4049);
  #line 664 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4049, _M0L3keyS1748, _M0L4ListS4050);
  moonbit_decref(_M0L4dataS4049);
  moonbit_decref(_M0L4ListS4050);
  #line 665 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_5327 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L5dequeS1749);
  moonbit_decref(_M0L5dequeS1749);
  return _result_5327;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hlen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1738,
  moonbit_string_t _M0L3keyS1739
) {
  #line 646 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 647 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1738, _M0L3keyS1739)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1741;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4047 =
      _M0L4selfS1738->$0;
    void* _M0L7_2abindS1742;
    int32_t _result_5329;
    moonbit_incref(_M0L4dataS4047);
    #line 650 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1742
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4047, _M0L3keyS1739);
    moonbit_decref(_M0L4dataS4047);
    if (_M0L7_2abindS1742 == 0) {
      if (_M0L7_2abindS1742) {
        moonbit_decref(_M0L7_2abindS1742);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1743 = _M0L7_2abindS1742;
      void* _M0L4_2axS1744 = _M0L7_2aSomeS1743;
      switch (Moonbit_object_tag(_M0L4_2axS1744)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1745 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1744;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4617 = _M0L7_2aHashS1745->$0;
          int32_t _M0L6_2acntS5073 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1745));
          struct _M0TPB3MapGssE* _M0L4_2ahS1746;
          if (_M0L6_2acntS5073 > 1) {
            int32_t _M0L11_2anew__cntS5074 = _M0L6_2acntS5073 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1745), _M0L11_2anew__cntS5074);
            moonbit_incref(_M0L8_2afieldS4617);
          } else if (_M0L6_2acntS5073 == 1) {
            #line 650 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1745);
          }
          _M0L4_2ahS1746 = _M0L8_2afieldS4617;
          _M0L1hS1741 = _M0L4_2ahS1746;
          goto join_1740;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1744);
          return 0;
          break;
        }
      }
    }
    join_1740:;
    #line 651 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5329 = _M0MPB3Map6lengthGssE(_M0L1hS1741);
    moonbit_decref(_M0L1hS1741);
    return _result_5329;
  }
}

struct _M0TPB3MapGssE* _M0MP38JIA2JIA29moonbitdb3lib8Database7hgetall(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1726,
  moonbit_string_t _M0L3keyS1727
) {
  #line 635 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 636 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1726, _M0L3keyS1727)
  ) {
    struct _M0TUssE** _M0L7_2abindS1728 =
      (struct _M0TUssE**)moonbit_empty_ref_array;
    struct _M0TUssE** _M0L6_2atmpS4043 = _M0L7_2abindS1728;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS4042 =
      (struct _M0TPB9ArrayViewGUssEE){.$0 = _M0L6_2atmpS4043,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGssE* _result_5330;
    #line 637 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5330 = _M0MPB3Map3MapGssE(_M0L6_2atmpS4042, 0ll);
    moonbit_decref(_M0L6_2atmpS4042.$0);
    return _result_5330;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1732;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4046 =
      _M0L4selfS1726->$0;
    void* _M0L7_2abindS1733;
    struct _M0TUssE** _M0L7_2abindS1730;
    struct _M0TUssE** _M0L6_2atmpS4045;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS4044;
    struct _M0TPB3MapGssE* _result_5333;
    moonbit_incref(_M0L4dataS4046);
    #line 639 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1733
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4046, _M0L3keyS1727);
    moonbit_decref(_M0L4dataS4046);
    if (_M0L7_2abindS1733 == 0) {
      if (_M0L7_2abindS1733) {
        moonbit_decref(_M0L7_2abindS1733);
      }
      goto join_1729;
    } else {
      void* _M0L7_2aSomeS1734 = _M0L7_2abindS1733;
      void* _M0L4_2axS1735 = _M0L7_2aSomeS1734;
      switch (Moonbit_object_tag(_M0L4_2axS1735)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1736 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1735;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4619 = _M0L7_2aHashS1736->$0;
          int32_t _M0L6_2acntS5075 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1736));
          struct _M0TPB3MapGssE* _M0L4_2ahS1737;
          if (_M0L6_2acntS5075 > 1) {
            int32_t _M0L11_2anew__cntS5076 = _M0L6_2acntS5075 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1736), _M0L11_2anew__cntS5076);
            moonbit_incref(_M0L8_2afieldS4619);
          } else if (_M0L6_2acntS5075 == 1) {
            #line 639 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1736);
          }
          _M0L4_2ahS1737 = _M0L8_2afieldS4619;
          _M0L1hS1732 = _M0L4_2ahS1737;
          goto join_1731;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1735);
          goto join_1729;
          break;
        }
      }
    }
    join_1731:;
    return _M0L1hS1732;
    join_1729:;
    _M0L7_2abindS1730 = (struct _M0TUssE**)moonbit_empty_ref_array;
    _M0L6_2atmpS4045 = _M0L7_2abindS1730;
    _M0L6_2atmpS4044
    = (struct _M0TPB9ArrayViewGUssEE){
      .$0 = _M0L6_2atmpS4045, .$1 = 0, .$2 = 0
    };
    #line 641 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5333 = _M0MPB3Map3MapGssE(_M0L6_2atmpS4044, 0ll);
    moonbit_decref(_M0L6_2atmpS4044.$0);
    return _result_5333;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hdel(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1715,
  moonbit_string_t _M0L3keyS1716,
  moonbit_string_t _M0L5fieldS1720
) {
  #line 617 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 618 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1715, _M0L3keyS1716)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1718;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4041 =
      _M0L4selfS1715->$0;
    void* _M0L7_2abindS1721;
    int32_t _M0L7existedS1719;
    moonbit_incref(_M0L4dataS4041);
    #line 621 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1721
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4041, _M0L3keyS1716);
    moonbit_decref(_M0L4dataS4041);
    if (_M0L7_2abindS1721 == 0) {
      if (_M0L7_2abindS1721) {
        moonbit_decref(_M0L7_2abindS1721);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1722 = _M0L7_2abindS1721;
      void* _M0L4_2axS1723 = _M0L7_2aSomeS1722;
      switch (Moonbit_object_tag(_M0L4_2axS1723)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1724 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1723;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4622 = _M0L7_2aHashS1724->$0;
          int32_t _M0L6_2acntS5077 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1724));
          struct _M0TPB3MapGssE* _M0L4_2ahS1725;
          if (_M0L6_2acntS5077 > 1) {
            int32_t _M0L11_2anew__cntS5078 = _M0L6_2acntS5077 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1724), _M0L11_2anew__cntS5078);
            moonbit_incref(_M0L8_2afieldS4622);
          } else if (_M0L6_2acntS5077 == 1) {
            #line 621 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1724);
          }
          _M0L4_2ahS1725 = _M0L8_2afieldS4622;
          _M0L1hS1718 = _M0L4_2ahS1725;
          goto join_1717;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1723);
          return 0;
          break;
        }
      }
    }
    join_1717:;
    #line 623 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7existedS1719 = _M0MPB3Map8containsGssE(_M0L1hS1718, _M0L5fieldS1720);
    if (_M0L7existedS1719) {
      struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4039;
      void* _M0L4HashS4040;
      #line 625 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGssE(_M0L1hS1718, _M0L5fieldS1720);
      _M0L4dataS4039 = _M0L4selfS1715->$0;
      _M0L4HashS4040
      = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash));
      Moonbit_object_header(_M0L4HashS4040)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 18, 1);
      ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4HashS4040)->$0
      = _M0L1hS1718;
      moonbit_incref(_M0L4dataS4039);
      #line 626 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4039, _M0L3keyS1716, _M0L4HashS4040);
      moonbit_decref(_M0L4dataS4039);
      moonbit_decref(_M0L4HashS4040);
    } else {
      moonbit_decref(_M0L1hS1718);
    }
    return _M0L7existedS1719;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1704,
  moonbit_string_t _M0L3keyS1705,
  moonbit_string_t _M0L5fieldS1709
) {
  #line 606 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1704, _M0L3keyS1705)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1708;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4038 =
      _M0L4selfS1704->$0;
    void* _M0L7_2abindS1710;
    moonbit_string_t _result_5337;
    moonbit_incref(_M0L4dataS4038);
    #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1710
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4038, _M0L3keyS1705);
    moonbit_decref(_M0L4dataS4038);
    if (_M0L7_2abindS1710 == 0) {
      if (_M0L7_2abindS1710) {
        moonbit_decref(_M0L7_2abindS1710);
      }
      goto join_1706;
    } else {
      void* _M0L7_2aSomeS1711 = _M0L7_2abindS1710;
      void* _M0L4_2axS1712 = _M0L7_2aSomeS1711;
      switch (Moonbit_object_tag(_M0L4_2axS1712)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1713 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1712;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4624 = _M0L7_2aHashS1713->$0;
          int32_t _M0L6_2acntS5079 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1713));
          struct _M0TPB3MapGssE* _M0L4_2ahS1714;
          if (_M0L6_2acntS5079 > 1) {
            int32_t _M0L11_2anew__cntS5080 = _M0L6_2acntS5079 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1713), _M0L11_2anew__cntS5080);
            moonbit_incref(_M0L8_2afieldS4624);
          } else if (_M0L6_2acntS5079 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1713);
          }
          _M0L4_2ahS1714 = _M0L8_2afieldS4624;
          _M0L1hS1708 = _M0L4_2ahS1714;
          goto join_1707;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1712);
          goto join_1706;
          break;
        }
      }
    }
    join_1707:;
    #line 611 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5337 = _M0MPB3Map3getGssE(_M0L1hS1708, _M0L5fieldS1709);
    moonbit_decref(_M0L1hS1708);
    return _result_5337;
    join_1706:;
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1690,
  moonbit_string_t _M0L3keyS1691,
  moonbit_string_t _M0L5fieldS1702,
  moonbit_string_t _M0L5valueS1703
) {
  int32_t _M0L6_2atmpS4032;
  struct _M0TPB3MapGssE* _M0L4hashS1692;
  struct _M0TPB3MapGssE* _M0L1hS1696;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4037;
  void* _M0L7_2abindS1697;
  struct _M0TUssE** _M0L7_2abindS1694;
  struct _M0TUssE** _M0L6_2atmpS4036;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS4035;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4033;
  void* _M0L4HashS4034;
  #line 596 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4032
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1690, _M0L3keyS1691);
  _M0L4dataS4037 = _M0L4selfS1690->$0;
  moonbit_incref(_M0L4dataS4037);
  #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1697
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4037, _M0L3keyS1691);
  moonbit_decref(_M0L4dataS4037);
  if (_M0L7_2abindS1697 == 0) {
    if (_M0L7_2abindS1697) {
      moonbit_decref(_M0L7_2abindS1697);
    }
    goto join_1693;
  } else {
    void* _M0L7_2aSomeS1698 = _M0L7_2abindS1697;
    void* _M0L4_2axS1699 = _M0L7_2aSomeS1698;
    switch (Moonbit_object_tag(_M0L4_2axS1699)) {
      case 1: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1700 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1699;
        struct _M0TPB3MapGssE* _M0L8_2afieldS4627 = _M0L7_2aHashS1700->$0;
        int32_t _M0L6_2acntS5081 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1700));
        struct _M0TPB3MapGssE* _M0L4_2ahS1701;
        if (_M0L6_2acntS5081 > 1) {
          int32_t _M0L11_2anew__cntS5082 = _M0L6_2acntS5081 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1700), _M0L11_2anew__cntS5082);
          moonbit_incref(_M0L8_2afieldS4627);
        } else if (_M0L6_2acntS5081 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aHashS1700);
        }
        _M0L4_2ahS1701 = _M0L8_2afieldS4627;
        _M0L1hS1696 = _M0L4_2ahS1701;
        goto join_1695;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1699);
        goto join_1693;
        break;
      }
    }
  }
  goto joinlet_5339;
  join_1695:;
  _M0L4hashS1692 = _M0L1hS1696;
  joinlet_5339:;
  goto joinlet_5338;
  join_1693:;
  _M0L7_2abindS1694 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4036 = _M0L7_2abindS1694;
  _M0L6_2atmpS4035
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS4036, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4hashS1692 = _M0MPB3Map3MapGssE(_M0L6_2atmpS4035, 10ll);
  moonbit_decref(_M0L6_2atmpS4035.$0);
  joinlet_5338:;
  #line 602 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1692, _M0L5fieldS1702, _M0L5valueS1703);
  _M0L4dataS4033 = _M0L4selfS1690->$0;
  _M0L4HashS4034
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS4034)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 18, 1);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4HashS4034)->$0
  = _M0L4hashS1692;
  moonbit_incref(_M0L4dataS4033);
  #line 603 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4033, _M0L3keyS1691, _M0L4HashS4034);
  moonbit_decref(_M0L4dataS4033);
  moonbit_decref(_M0L4HashS4034);
  return 0;
}

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database4decr(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1674,
  moonbit_string_t _M0L3keyS1675
) {
  int32_t _M0L6_2atmpS4025;
  moonbit_string_t _M0L1sS1678;
  moonbit_string_t _M0L7currentS1676;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4031;
  void* _M0L7_2abindS1679;
  int32_t _M0L1nS1685;
  int64_t _M0L7_2abindS1687;
  int32_t _M0L6_2atmpS4030;
  moonbit_string_t _M0L8new__valS1686;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4026;
  void* _M0L6StringS4027;
  struct _M0TPB3MapGsiE* _M0L7expiresS4028;
  int32_t _M0L6_2atmpS4029;
  #line 579 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 580 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4025
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1674, _M0L3keyS1675);
  _M0L4dataS4031 = _M0L4selfS1674->$0;
  moonbit_incref(_M0L4dataS4031);
  #line 581 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1679
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4031, _M0L3keyS1675);
  moonbit_decref(_M0L4dataS4031);
  if (_M0L7_2abindS1679 == 0) {
    if (_M0L7_2abindS1679) {
      moonbit_decref(_M0L7_2abindS1679);
    }
    _M0L7currentS1676 = (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    void* _M0L7_2aSomeS1680 = _M0L7_2abindS1679;
    void* _M0L4_2axS1681 = _M0L7_2aSomeS1680;
    switch (Moonbit_object_tag(_M0L4_2axS1681)) {
      case 0: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1682 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1681;
        moonbit_string_t _M0L8_2afieldS4631 = _M0L9_2aStringS1682->$0;
        int32_t _M0L6_2acntS5083 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1682));
        moonbit_string_t _M0L4_2asS1683;
        if (_M0L6_2acntS5083 > 1) {
          int32_t _M0L11_2anew__cntS5084 = _M0L6_2acntS5083 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1682), _M0L11_2anew__cntS5084);
          moonbit_incref(_M0L8_2afieldS4631);
        } else if (_M0L6_2acntS5083 == 1) {
          #line 581 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L9_2aStringS1682);
        }
        _M0L4_2asS1683 = _M0L8_2afieldS4631;
        _M0L1sS1678 = _M0L4_2asS1683;
        goto join_1677;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1681);
        _M0L7currentS1676 = (moonbit_string_t)moonbit_string_literal_84.data;
        break;
      }
    }
  }
  goto joinlet_5340;
  join_1677:;
  _M0L7currentS1676 = _M0L1sS1678;
  joinlet_5340:;
  #line 585 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1687
  = _M0FP38JIA2JIA29moonbitdb3lib10parse__int(_M0L7currentS1676);
  moonbit_decref(_M0L7currentS1676);
  if (_M0L7_2abindS1687 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1688 = _M0L7_2abindS1687;
    int32_t _M0L4_2anS1689 = (int32_t)_M0L7_2aSomeS1688;
    _M0L1nS1685 = _M0L4_2anS1689;
    goto join_1684;
  }
  join_1684:;
  _M0L6_2atmpS4030 = _M0L1nS1685 - 1;
  #line 587 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L8new__valS1686
  = _M0FP38JIA2JIA29moonbitdb3lib15int__to__string(_M0L6_2atmpS4030);
  _M0L4dataS4026 = _M0L4selfS1674->$0;
  _M0L6StringS4027
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS4027)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS4027)->$0
  = _M0L8new__valS1686;
  moonbit_incref(_M0L4dataS4026);
  #line 588 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4026, _M0L3keyS1675, _M0L6StringS4027);
  moonbit_decref(_M0L4dataS4026);
  moonbit_decref(_M0L6StringS4027);
  _M0L7expiresS4028 = _M0L4selfS1674->$1;
  moonbit_incref(_M0L7expiresS4028);
  #line 589 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS4028, _M0L3keyS1675);
  moonbit_decref(_M0L7expiresS4028);
  _M0L6_2atmpS4029 = _M0L1nS1685 - 1;
  return (int64_t)_M0L6_2atmpS4029;
}

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database4incr(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1658,
  moonbit_string_t _M0L3keyS1659
) {
  int32_t _M0L6_2atmpS4018;
  moonbit_string_t _M0L1sS1662;
  moonbit_string_t _M0L7currentS1660;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4024;
  void* _M0L7_2abindS1663;
  int32_t _M0L1nS1669;
  int64_t _M0L7_2abindS1671;
  int32_t _M0L6_2atmpS4023;
  moonbit_string_t _M0L8new__valS1670;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS4019;
  void* _M0L6StringS4020;
  struct _M0TPB3MapGsiE* _M0L7expiresS4021;
  int32_t _M0L6_2atmpS4022;
  #line 562 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 563 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS4018
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1658, _M0L3keyS1659);
  _M0L4dataS4024 = _M0L4selfS1658->$0;
  moonbit_incref(_M0L4dataS4024);
  #line 564 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1663
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4024, _M0L3keyS1659);
  moonbit_decref(_M0L4dataS4024);
  if (_M0L7_2abindS1663 == 0) {
    if (_M0L7_2abindS1663) {
      moonbit_decref(_M0L7_2abindS1663);
    }
    _M0L7currentS1660 = (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    void* _M0L7_2aSomeS1664 = _M0L7_2abindS1663;
    void* _M0L4_2axS1665 = _M0L7_2aSomeS1664;
    switch (Moonbit_object_tag(_M0L4_2axS1665)) {
      case 0: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1666 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1665;
        moonbit_string_t _M0L8_2afieldS4635 = _M0L9_2aStringS1666->$0;
        int32_t _M0L6_2acntS5085 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1666));
        moonbit_string_t _M0L4_2asS1667;
        if (_M0L6_2acntS5085 > 1) {
          int32_t _M0L11_2anew__cntS5086 = _M0L6_2acntS5085 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1666), _M0L11_2anew__cntS5086);
          moonbit_incref(_M0L8_2afieldS4635);
        } else if (_M0L6_2acntS5085 == 1) {
          #line 564 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L9_2aStringS1666);
        }
        _M0L4_2asS1667 = _M0L8_2afieldS4635;
        _M0L1sS1662 = _M0L4_2asS1667;
        goto join_1661;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1665);
        _M0L7currentS1660 = (moonbit_string_t)moonbit_string_literal_84.data;
        break;
      }
    }
  }
  goto joinlet_5342;
  join_1661:;
  _M0L7currentS1660 = _M0L1sS1662;
  joinlet_5342:;
  #line 568 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1671
  = _M0FP38JIA2JIA29moonbitdb3lib10parse__int(_M0L7currentS1660);
  moonbit_decref(_M0L7currentS1660);
  if (_M0L7_2abindS1671 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1672 = _M0L7_2abindS1671;
    int32_t _M0L4_2anS1673 = (int32_t)_M0L7_2aSomeS1672;
    _M0L1nS1669 = _M0L4_2anS1673;
    goto join_1668;
  }
  join_1668:;
  _M0L6_2atmpS4023 = _M0L1nS1669 + 1;
  #line 570 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L8new__valS1670
  = _M0FP38JIA2JIA29moonbitdb3lib15int__to__string(_M0L6_2atmpS4023);
  _M0L4dataS4019 = _M0L4selfS1658->$0;
  _M0L6StringS4020
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS4020)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS4020)->$0
  = _M0L8new__valS1670;
  moonbit_incref(_M0L4dataS4019);
  #line 571 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS4019, _M0L3keyS1659, _M0L6StringS4020);
  moonbit_decref(_M0L4dataS4019);
  moonbit_decref(_M0L6StringS4020);
  _M0L7expiresS4021 = _M0L4selfS1658->$1;
  moonbit_incref(_M0L7expiresS4021);
  #line 572 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS4021, _M0L3keyS1659);
  moonbit_decref(_M0L7expiresS4021);
  _M0L6_2atmpS4022 = _M0L1nS1669 + 1;
  return (int64_t)_M0L6_2atmpS4022;
}

moonbit_string_t _M0FP38JIA2JIA29moonbitdb3lib15int__to__string(
  int32_t _M0L1nS1649
) {
  #line 526 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (_M0L1nS1649 == 0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L3numS1650 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    moonbit_string_t* _M0L6_2atmpS4017;
    struct _M0TPB5ArrayGsE* _M0L5charsS1651;
    int32_t _M0L3valS4003;
    moonbit_string_t* _M0L6_2atmpS4016;
    struct _M0TPB5ArrayGsE* _M0L6resultS1654;
    int32_t _M0L6_2atmpS4013;
    int32_t _M0L6_2atmpS4012;
    int32_t _M0L1iS1655;
    moonbit_string_t _M0L7_2abindS1657;
    int32_t _M0L6_2atmpS4015;
    struct _M0TPC16string10StringView _M0L6_2atmpS4014;
    moonbit_string_t _result_5346;
    Moonbit_object_header(_M0L3numS1650)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L3numS1650->$0 = _M0L1nS1649;
    _M0L6_2atmpS4017 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L5charsS1651
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L5charsS1651)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L5charsS1651->$0 = _M0L6_2atmpS4017;
    _M0L5charsS1651->$1 = 0;
    _M0L3valS4003 = _M0L3numS1650->$0;
    if (_M0L3valS4003 < 0) {
      int32_t _M0L3valS4005 = _M0L3numS1650->$0;
      int32_t _M0L6_2atmpS4004 = -_M0L3valS4005;
      _M0L3numS1650->$0 = _M0L6_2atmpS4004;
    }
    while (1) {
      int32_t _M0L3valS4006 = _M0L3numS1650->$0;
      if (_M0L3valS4006 > 0) {
        int32_t _M0L3valS4007 = _M0L3numS1650->$0;
        int32_t _M0L7_2abindS1652 = _M0L3valS4007 % 10;
        int32_t _M0L3valS4009;
        int32_t _M0L6_2atmpS4008;
        switch (_M0L7_2abindS1652) {
          case 0: {
            #line 537 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_84.data);
            break;
          }
          
          case 1: {
            #line 538 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_85.data);
            break;
          }
          
          case 2: {
            #line 539 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_86.data);
            break;
          }
          
          case 3: {
            #line 540 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_87.data);
            break;
          }
          
          case 4: {
            #line 541 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_88.data);
            break;
          }
          
          case 5: {
            #line 542 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_89.data);
            break;
          }
          
          case 6: {
            #line 543 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_90.data);
            break;
          }
          
          case 7: {
            #line 544 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_91.data);
            break;
          }
          
          case 8: {
            #line 545 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_92.data);
            break;
          }
          
          case 9: {
            #line 546 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_93.data);
            break;
          }
          default: {
            #line 547 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_84.data);
            break;
          }
        }
        _M0L3valS4009 = _M0L3numS1650->$0;
        _M0L6_2atmpS4008 = _M0L3valS4009 / 10;
        _M0L3numS1650->$0 = _M0L6_2atmpS4008;
        continue;
      } else {
        moonbit_decref(_M0L3numS1650);
      }
      break;
    }
    if (_M0L1nS1649 < 0) {
      #line 552 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5charsS1651, (moonbit_string_t)moonbit_string_literal_94.data);
    }
    _M0L6_2atmpS4016 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1654
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1654)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1654->$0 = _M0L6_2atmpS4016;
    _M0L6resultS1654->$1 = 0;
    #line 555 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS4013 = _M0MPC15array5Array6lengthGsE(_M0L5charsS1651);
    _M0L6_2atmpS4012 = _M0L6_2atmpS4013 - 1;
    _M0L1iS1655 = _M0L6_2atmpS4012;
    while (1) {
      if (_M0L1iS1655 >= 0) {
        moonbit_string_t _M0L6_2atmpS4010;
        int32_t _M0L6_2atmpS4011;
        #line 556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS4010
        = _M0MPC15array5Array2atGsE(_M0L5charsS1651, _M0L1iS1655);
        #line 556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1654, _M0L6_2atmpS4010);
        moonbit_decref(_M0L6_2atmpS4010);
        _M0L6_2atmpS4011 = _M0L1iS1655 - 1;
        _M0L1iS1655 = _M0L6_2atmpS4011;
        continue;
      } else {
        moonbit_decref(_M0L5charsS1651);
      }
      break;
    }
    _M0L7_2abindS1657 = (moonbit_string_t)moonbit_string_literal_75.data;
    _M0L6_2atmpS4015 = Moonbit_array_length(_M0L7_2abindS1657);
    _M0L6_2atmpS4014
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS1657, .$1 = 0, .$2 = _M0L6_2atmpS4015
    };
    #line 558 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5346
    = _M0MPC15array5Array4joinGsE(_M0L6resultS1654, _M0L6_2atmpS4014);
    moonbit_decref(_M0L6resultS1654);
    moonbit_decref(_M0L6_2atmpS4014.$0);
    return _result_5346;
  }
}

int64_t _M0FP38JIA2JIA29moonbitdb3lib10parse__int(
  moonbit_string_t _M0L1sS1638
) {
  int32_t _M0L6_2atmpS3990;
  #line 503 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3990 = Moonbit_array_length(_M0L1sS1638);
  if (_M0L6_2atmpS3990 == 0) {
    return 4294967296ll;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L6resultS1639 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    struct _M0TPB8MutLocalGiE* _M0L4signS1640;
    struct _M0TPB8MutLocalGiE* _M0L5startS1641;
    int32_t _M0L6_2atmpS3991;
    int32_t _M0L3valS3999;
    int32_t _M0L1iS1642;
    int32_t _M0L3valS4001;
    int32_t _M0L3valS4002;
    int32_t _M0L6_2atmpS4000;
    Moonbit_object_header(_M0L6resultS1639)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L6resultS1639->$0 = 0;
    _M0L4signS1640
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L4signS1640)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L4signS1640->$0 = 1;
    _M0L5startS1641
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L5startS1641)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L5startS1641->$0 = 0;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1638)) {
      #line 510 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3991 = _M0L1sS1638[0];
    if (_M0L6_2atmpS3991 == 45) {
      _M0L4signS1640->$0 = -1;
      _M0L5startS1641->$0 = 1;
    } else {
      int32_t _M0L6_2atmpS3992;
      if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1638)) {
        #line 513 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3992 = _M0L1sS1638[0];
      if (_M0L6_2atmpS3992 == 43) {
        _M0L5startS1641->$0 = 1;
      }
    }
    _M0L3valS3999 = _M0L5startS1641->$0;
    moonbit_decref(_M0L5startS1641);
    _M0L1iS1642 = _M0L3valS3999;
    while (1) {
      int32_t _M0L6_2atmpS3993 = Moonbit_array_length(_M0L1sS1638);
      if (_M0L1iS1642 < _M0L6_2atmpS3993) {
        int32_t _M0L5digitS1644;
        int32_t _M0L6_2atmpS3997;
        int64_t _M0L7_2abindS1645;
        int32_t _M0L3valS3996;
        int32_t _M0L6_2atmpS3995;
        int32_t _M0L6_2atmpS3994;
        int32_t _M0L6_2atmpS3998;
        if (
          _M0L1iS1642 < 0 || _M0L1iS1642 >= Moonbit_array_length(_M0L1sS1638)
        ) {
          #line 517 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3997 = _M0L1sS1638[_M0L1iS1642];
        #line 517 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1645
        = _M0FP38JIA2JIA29moonbitdb3lib17uint16__to__digit(_M0L6_2atmpS3997);
        if (_M0L7_2abindS1645 == 4294967296ll) {
          moonbit_decref(_M0L4signS1640);
          moonbit_decref(_M0L6resultS1639);
          return 4294967296ll;
        } else {
          int64_t _M0L7_2aSomeS1646 = _M0L7_2abindS1645;
          int32_t _M0L8_2adigitS1647 = (int32_t)_M0L7_2aSomeS1646;
          _M0L5digitS1644 = _M0L8_2adigitS1647;
          goto join_1643;
        }
        goto joinlet_5348;
        join_1643:;
        _M0L3valS3996 = _M0L6resultS1639->$0;
        _M0L6_2atmpS3995 = _M0L3valS3996 * 10;
        _M0L6_2atmpS3994 = _M0L6_2atmpS3995 + _M0L5digitS1644;
        _M0L6resultS1639->$0 = _M0L6_2atmpS3994;
        joinlet_5348:;
        _M0L6_2atmpS3998 = _M0L1iS1642 + 1;
        _M0L1iS1642 = _M0L6_2atmpS3998;
        continue;
      }
      break;
    }
    _M0L3valS4001 = _M0L6resultS1639->$0;
    moonbit_decref(_M0L6resultS1639);
    _M0L3valS4002 = _M0L4signS1640->$0;
    moonbit_decref(_M0L4signS1640);
    _M0L6_2atmpS4000 = _M0L3valS4001 * _M0L3valS4002;
    return (int64_t)_M0L6_2atmpS4000;
  }
}

int64_t _M0FP38JIA2JIA29moonbitdb3lib17uint16__to__digit(int32_t _M0L1cS1637) {
  #line 471 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  switch (_M0L1cS1637) {
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
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1628,
  moonbit_string_t _M0L3keyS1629
) {
  #line 460 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 461 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1628, _M0L3keyS1629)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1631;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3989 =
      _M0L4selfS1628->$0;
    void* _M0L7_2abindS1632;
    int32_t _result_5350;
    moonbit_incref(_M0L4dataS3989);
    #line 464 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1632
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3989, _M0L3keyS1629);
    moonbit_decref(_M0L4dataS3989);
    if (_M0L7_2abindS1632 == 0) {
      if (_M0L7_2abindS1632) {
        moonbit_decref(_M0L7_2abindS1632);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1633 = _M0L7_2abindS1632;
      void* _M0L4_2axS1634 = _M0L7_2aSomeS1633;
      switch (Moonbit_object_tag(_M0L4_2axS1634)) {
        case 0: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1635 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1634;
          moonbit_string_t _M0L8_2afieldS4637 = _M0L9_2aStringS1635->$0;
          int32_t _M0L6_2acntS5087 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1635));
          moonbit_string_t _M0L4_2asS1636;
          if (_M0L6_2acntS5087 > 1) {
            int32_t _M0L11_2anew__cntS5088 = _M0L6_2acntS5087 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1635), _M0L11_2anew__cntS5088);
            moonbit_incref(_M0L8_2afieldS4637);
          } else if (_M0L6_2acntS5087 == 1) {
            #line 464 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1635);
          }
          _M0L4_2asS1636 = _M0L8_2afieldS4637;
          _M0L1sS1631 = _M0L4_2asS1636;
          goto join_1630;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1634);
          return 0;
          break;
        }
      }
    }
    join_1630:;
    _result_5350 = Moonbit_array_length(_M0L1sS1631);
    moonbit_decref(_M0L1sS1631);
    return _result_5350;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6append(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1615,
  moonbit_string_t _M0L3keyS1616,
  moonbit_string_t _M0L5valueS1618
) {
  #line 444 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 445 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1615, _M0L3keyS1616)
  ) {
    moonbit_string_t _M0L8new__valS1617 = _M0L5valueS1618;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3984 =
      _M0L4selfS1615->$0;
    void* _M0L6StringS3985;
    moonbit_incref(_M0L8new__valS1617);
    _M0L6StringS3985
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
    Moonbit_object_header(_M0L6StringS3985)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3985)->$0
    = _M0L8new__valS1617;
    moonbit_incref(_M0L4dataS3984);
    #line 447 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3984, _M0L3keyS1616, _M0L6StringS3985);
    moonbit_decref(_M0L4dataS3984);
    moonbit_decref(_M0L6StringS3985);
    return Moonbit_array_length(_M0L8new__valS1617);
  } else {
    moonbit_string_t _M0L1sS1621;
    moonbit_string_t _M0L7currentS1619;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3988 =
      _M0L4selfS1615->$0;
    void* _M0L7_2abindS1622;
    moonbit_string_t _M0L8new__valS1627;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3986;
    void* _M0L6StringS3987;
    int32_t _result_5352;
    moonbit_incref(_M0L4dataS3988);
    #line 450 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1622
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3988, _M0L3keyS1616);
    moonbit_decref(_M0L4dataS3988);
    if (_M0L7_2abindS1622 == 0) {
      if (_M0L7_2abindS1622) {
        moonbit_decref(_M0L7_2abindS1622);
      }
      _M0L7currentS1619 = (moonbit_string_t)moonbit_string_literal_75.data;
    } else {
      void* _M0L7_2aSomeS1623 = _M0L7_2abindS1622;
      void* _M0L4_2axS1624 = _M0L7_2aSomeS1623;
      switch (Moonbit_object_tag(_M0L4_2axS1624)) {
        case 0: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1625 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1624;
          moonbit_string_t _M0L8_2afieldS4641 = _M0L9_2aStringS1625->$0;
          int32_t _M0L6_2acntS5089 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1625));
          moonbit_string_t _M0L4_2asS1626;
          if (_M0L6_2acntS5089 > 1) {
            int32_t _M0L11_2anew__cntS5090 = _M0L6_2acntS5089 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1625), _M0L11_2anew__cntS5090);
            moonbit_incref(_M0L8_2afieldS4641);
          } else if (_M0L6_2acntS5089 == 1) {
            #line 450 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1625);
          }
          _M0L4_2asS1626 = _M0L8_2afieldS4641;
          _M0L1sS1621 = _M0L4_2asS1626;
          goto join_1620;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1624);
          _M0L7currentS1619
          = (moonbit_string_t)moonbit_string_literal_75.data;
          break;
        }
      }
    }
    goto joinlet_5351;
    join_1620:;
    _M0L7currentS1619 = _M0L1sS1621;
    joinlet_5351:;
    #line 454 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L8new__valS1627
    = moonbit_add_string(_M0L7currentS1619, _M0L5valueS1618);
    moonbit_decref(_M0L7currentS1619);
    _M0L4dataS3986 = _M0L4selfS1615->$0;
    moonbit_incref(_M0L8new__valS1627);
    _M0L6StringS3987
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
    Moonbit_object_header(_M0L6StringS3987)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3987)->$0
    = _M0L8new__valS1627;
    moonbit_incref(_M0L4dataS3986);
    #line 455 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3986, _M0L3keyS1616, _M0L6StringS3987);
    moonbit_decref(_M0L4dataS3986);
    moonbit_decref(_M0L6StringS3987);
    _result_5352 = Moonbit_array_length(_M0L8new__valS1627);
    moonbit_decref(_M0L8new__valS1627);
    return _result_5352;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database8type__of(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1610,
  moonbit_string_t _M0L3keyS1611
) {
  #line 429 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 430 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1610, _M0L3keyS1611)
  ) {
    return (moonbit_string_t)moonbit_string_literal_95.data;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3983 =
      _M0L4selfS1610->$0;
    void* _M0L7_2abindS1612;
    moonbit_incref(_M0L4dataS3983);
    #line 433 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1612
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3983, _M0L3keyS1611);
    moonbit_decref(_M0L4dataS3983);
    if (_M0L7_2abindS1612 == 0) {
      if (_M0L7_2abindS1612) {
        moonbit_decref(_M0L7_2abindS1612);
      }
      return (moonbit_string_t)moonbit_string_literal_95.data;
    } else {
      void* _M0L7_2aSomeS1613 = _M0L7_2abindS1612;
      void* _M0L4_2axS1614 = _M0L7_2aSomeS1613;
      switch (Moonbit_object_tag(_M0L4_2axS1614)) {
        case 0: {
          moonbit_decref(_M0L4_2axS1614);
          return (moonbit_string_t)moonbit_string_literal_96.data;
          break;
        }
        
        case 1: {
          moonbit_decref(_M0L4_2axS1614);
          return (moonbit_string_t)moonbit_string_literal_97.data;
          break;
        }
        
        case 2: {
          moonbit_decref(_M0L4_2axS1614);
          return (moonbit_string_t)moonbit_string_literal_98.data;
          break;
        }
        
        case 3: {
          moonbit_decref(_M0L4_2axS1614);
          return (moonbit_string_t)moonbit_string_literal_99.data;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1614);
          return (moonbit_string_t)moonbit_string_literal_100.data;
          break;
        }
      }
    }
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database4keys(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1602
) {
  moonbit_string_t* _M0L6_2atmpS3982;
  struct _M0TPB5ArrayGsE* _M0L6resultS1600;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3981;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L5_2aitS1601;
  #line 377 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3982 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1600
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1600)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1600->$0 = _M0L6_2atmpS3982;
  _M0L6resultS1600->$1 = 0;
  _M0L4dataS3981 = _M0L4selfS1602->$0;
  moonbit_incref(_M0L4dataS3981);
  #line 378 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1601
  = _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3981);
  moonbit_decref(_M0L4dataS3981);
  while (1) {
    moonbit_string_t _M0L3keyS1604;
    struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1606;
    int32_t _M0L6_2atmpS3980;
    #line 379 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1606
    = _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L5_2aitS1601);
    if (_M0L7_2abindS1606 == 0) {
      if (_M0L7_2abindS1606) {
        moonbit_decref(_M0L7_2abindS1606);
      }
      moonbit_decref(_M0L5_2aitS1601);
    } else {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1607 =
        _M0L7_2abindS1606;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1608 =
        _M0L7_2aSomeS1607;
      moonbit_string_t _M0L8_2afieldS4644 = _M0L4_2axS1608->$0;
      int32_t _M0L6_2acntS5091 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1608));
      moonbit_string_t _M0L6_2akeyS1609;
      if (_M0L6_2acntS5091 > 1) {
        int32_t _M0L11_2anew__cntS5093 = _M0L6_2acntS5091 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1608), _M0L11_2anew__cntS5093);
        moonbit_incref(_M0L8_2afieldS4644);
      } else if (_M0L6_2acntS5091 == 1) {
        void* _M0L8_2afieldS5092 = _M0L4_2axS1608->$1;
        moonbit_decref(_M0L8_2afieldS5092);
        #line 379 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1608);
      }
      _M0L6_2akeyS1609 = _M0L8_2afieldS4644;
      _M0L3keyS1604 = _M0L6_2akeyS1609;
      goto join_1603;
    }
    goto joinlet_5354;
    join_1603:;
    #line 380 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3980
    = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1602, _M0L3keyS1604);
    if (!_M0L6_2atmpS3980) {
      #line 381 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1600, _M0L3keyS1604);
      moonbit_decref(_M0L3keyS1604);
    } else {
      moonbit_decref(_M0L3keyS1604);
    }
    continue;
    joinlet_5354:;
    break;
  }
  return _M0L6resultS1600;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6exists(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1598,
  moonbit_string_t _M0L3keyS1599
) {
  #line 369 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 370 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1598, _M0L3keyS1599)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3979 =
      _M0L4selfS1598->$0;
    int32_t _result_5355;
    moonbit_incref(_M0L4dataS3979);
    #line 373 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5355
    = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3979, _M0L3keyS1599);
    moonbit_decref(_M0L4dataS3979);
    return _result_5355;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3del(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1595,
  moonbit_string_t _M0L3keyS1596
) {
  #line 356 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 357 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1595, _M0L3keyS1596)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3978 =
      _M0L4selfS1595->$0;
    int32_t _M0L7existedS1597;
    moonbit_incref(_M0L4dataS3978);
    #line 360 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7existedS1597
    = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3978, _M0L3keyS1596);
    moonbit_decref(_M0L4dataS3978);
    if (_M0L7existedS1597) {
      struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3976 =
        _M0L4selfS1595->$0;
      struct _M0TPB3MapGsiE* _M0L7expiresS3977;
      moonbit_incref(_M0L4dataS3976);
      #line 362 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3976, _M0L3keyS1596);
      moonbit_decref(_M0L4dataS3976);
      _M0L7expiresS3977 = _M0L4selfS1595->$1;
      moonbit_incref(_M0L7expiresS3977);
      #line 363 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS3977, _M0L3keyS1596);
      moonbit_decref(_M0L7expiresS3977);
    }
    return _M0L7existedS1597;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database3get(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1585,
  moonbit_string_t _M0L3keyS1586
) {
  #line 345 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 346 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1585, _M0L3keyS1586)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1589;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3975 =
      _M0L4selfS1585->$0;
    void* _M0L7_2abindS1590;
    moonbit_incref(_M0L4dataS3975);
    #line 349 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1590
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3975, _M0L3keyS1586);
    moonbit_decref(_M0L4dataS3975);
    if (_M0L7_2abindS1590 == 0) {
      if (_M0L7_2abindS1590) {
        moonbit_decref(_M0L7_2abindS1590);
      }
      goto join_1587;
    } else {
      void* _M0L7_2aSomeS1591 = _M0L7_2abindS1590;
      void* _M0L4_2axS1592 = _M0L7_2aSomeS1591;
      switch (Moonbit_object_tag(_M0L4_2axS1592)) {
        case 0: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1593 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1592;
          moonbit_string_t _M0L8_2afieldS4650 = _M0L9_2aStringS1593->$0;
          int32_t _M0L6_2acntS5094 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1593));
          moonbit_string_t _M0L4_2asS1594;
          if (_M0L6_2acntS5094 > 1) {
            int32_t _M0L11_2anew__cntS5095 = _M0L6_2acntS5094 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1593), _M0L11_2anew__cntS5095);
            moonbit_incref(_M0L8_2afieldS4650);
          } else if (_M0L6_2acntS5094 == 1) {
            #line 349 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1593);
          }
          _M0L4_2asS1594 = _M0L8_2afieldS4650;
          _M0L1sS1589 = _M0L4_2asS1594;
          goto join_1588;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1592);
          goto join_1587;
          break;
        }
      }
    }
    join_1588:;
    return _M0L1sS1589;
    join_1587:;
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4mdel(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1582,
  struct _M0TPB5ArrayGsE* _M0L4keysS1579
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1577;
  int32_t _M0L7_2abindS1578;
  int32_t _M0L2__S1580;
  int32_t _result_5359;
  #line 291 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5countS1577
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1577)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1577->$0 = 0;
  _M0L7_2abindS1578 = _M0L4keysS1579->$1;
  _M0L2__S1580 = 0;
  while (1) {
    if (_M0L2__S1580 < _M0L7_2abindS1578) {
      moonbit_string_t* _M0L3bufS3974 = _M0L4keysS1579->$0;
      moonbit_string_t _M0L3keyS1581 =
        (moonbit_string_t)_M0L3bufS3974[_M0L2__S1580];
      int32_t _M0L6_2atmpS3967;
      int32_t _M0L6_2atmpS3973;
      moonbit_incref(_M0L3keyS1581);
      #line 294 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3967
      = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1582, _M0L3keyS1581);
      if (!_M0L6_2atmpS3967) {
        struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3972 =
          _M0L4selfS1582->$0;
        int32_t _M0L7existedS1583;
        moonbit_incref(_M0L4dataS3972);
        #line 295 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7existedS1583
        = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3972, _M0L3keyS1581);
        moonbit_decref(_M0L4dataS3972);
        if (_M0L7existedS1583) {
          struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3968 =
            _M0L4selfS1582->$0;
          struct _M0TPB3MapGsiE* _M0L7expiresS3969;
          int32_t _M0L3valS3971;
          int32_t _M0L6_2atmpS3970;
          moonbit_incref(_M0L4dataS3968);
          #line 297 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3968, _M0L3keyS1581);
          moonbit_decref(_M0L4dataS3968);
          _M0L7expiresS3969 = _M0L4selfS1582->$1;
          moonbit_incref(_M0L7expiresS3969);
          #line 298 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPB3Map6removeGsiE(_M0L7expiresS3969, _M0L3keyS1581);
          moonbit_decref(_M0L7expiresS3969);
          moonbit_decref(_M0L3keyS1581);
          _M0L3valS3971 = _M0L5countS1577->$0;
          _M0L6_2atmpS3970 = _M0L3valS3971 + 1;
          _M0L5countS1577->$0 = _M0L6_2atmpS3970;
        } else {
          moonbit_decref(_M0L3keyS1581);
        }
      } else {
        moonbit_decref(_M0L3keyS1581);
      }
      _M0L6_2atmpS3973 = _M0L2__S1580 + 1;
      _M0L2__S1580 = _M0L6_2atmpS3973;
      continue;
    }
    break;
  }
  _result_5359 = _M0L5countS1577->$0;
  moonbit_decref(_M0L5countS1577);
  return _result_5359;
}

struct _M0TPB5ArrayGOsE* _M0MP38JIA2JIA29moonbitdb3lib8Database4mget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1567,
  struct _M0TPB5ArrayGsE* _M0L4keysS1564
) {
  moonbit_string_t* _M0L6_2atmpS3966;
  struct _M0TPB5ArrayGOsE* _M0L6resultS1562;
  int32_t _M0L7_2abindS1563;
  int32_t _M0L2__S1565;
  #line 276 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3966 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1562
  = (struct _M0TPB5ArrayGOsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGOsE));
  Moonbit_object_header(_M0L6resultS1562)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 24, 0);
  _M0L6resultS1562->$0 = _M0L6_2atmpS3966;
  _M0L6resultS1562->$1 = 0;
  _M0L7_2abindS1563 = _M0L4keysS1564->$1;
  _M0L2__S1565 = 0;
  while (1) {
    if (_M0L2__S1565 < _M0L7_2abindS1563) {
      moonbit_string_t* _M0L3bufS3965 = _M0L4keysS1564->$0;
      moonbit_string_t _M0L3keyS1566 =
        (moonbit_string_t)_M0L3bufS3965[_M0L2__S1565];
      int32_t _M0L6_2atmpS3964;
      moonbit_incref(_M0L3keyS1566);
      #line 279 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      if (
        _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1567, _M0L3keyS1566)
      ) {
        moonbit_string_t _M0L6_2atmpS3960;
        moonbit_decref(_M0L3keyS1566);
        _M0L6_2atmpS3960 = 0;
        #line 280 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1562, _M0L6_2atmpS3960);
        if (_M0L6_2atmpS3960) {
          moonbit_decref(_M0L6_2atmpS3960);
        }
      } else {
        moonbit_string_t _M0L1sS1570;
        struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3963 =
          _M0L4selfS1567->$0;
        void* _M0L7_2abindS1571;
        moonbit_string_t _M0L6_2atmpS3962;
        moonbit_string_t _M0L6_2atmpS3961;
        moonbit_incref(_M0L4dataS3963);
        #line 282 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1571
        = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3963, _M0L3keyS1566);
        moonbit_decref(_M0L4dataS3963);
        moonbit_decref(_M0L3keyS1566);
        if (_M0L7_2abindS1571 == 0) {
          if (_M0L7_2abindS1571) {
            moonbit_decref(_M0L7_2abindS1571);
          }
          goto join_1568;
        } else {
          void* _M0L7_2aSomeS1572 = _M0L7_2abindS1571;
          void* _M0L4_2axS1573 = _M0L7_2aSomeS1572;
          switch (Moonbit_object_tag(_M0L4_2axS1573)) {
            case 0: {
              struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1574 =
                (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1573;
              moonbit_string_t _M0L8_2afieldS4657 = _M0L9_2aStringS1574->$0;
              int32_t _M0L6_2acntS5096 =
                Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1574));
              moonbit_string_t _M0L4_2asS1575;
              if (_M0L6_2acntS5096 > 1) {
                int32_t _M0L11_2anew__cntS5097 = _M0L6_2acntS5096 - 1;
                Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1574), _M0L11_2anew__cntS5097);
                moonbit_incref(_M0L8_2afieldS4657);
              } else if (_M0L6_2acntS5096 == 1) {
                #line 282 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
                moonbit_free(_M0L9_2aStringS1574);
              }
              _M0L4_2asS1575 = _M0L8_2afieldS4657;
              _M0L1sS1570 = _M0L4_2asS1575;
              goto join_1569;
              break;
            }
            default: {
              moonbit_decref(_M0L4_2axS1573);
              goto join_1568;
              break;
            }
          }
        }
        goto joinlet_5362;
        join_1569:;
        _M0L6_2atmpS3962 = _M0L1sS1570;
        #line 283 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1562, _M0L6_2atmpS3962);
        if (_M0L6_2atmpS3962) {
          moonbit_decref(_M0L6_2atmpS3962);
        }
        joinlet_5362:;
        goto joinlet_5361;
        join_1568:;
        _M0L6_2atmpS3961 = 0;
        #line 284 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1562, _M0L6_2atmpS3961);
        if (_M0L6_2atmpS3961) {
          moonbit_decref(_M0L6_2atmpS3961);
        }
        joinlet_5361:;
      }
      _M0L6_2atmpS3964 = _M0L2__S1565 + 1;
      _M0L2__S1565 = _M0L6_2atmpS3964;
      continue;
    }
    break;
  }
  return _M0L6resultS1562;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4mset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1560,
  struct _M0TPB5ArrayGsE* _M0L4keysS1558,
  struct _M0TPB5ArrayGsE* _M0L6valuesS1559
) {
  int32_t _M0L1iS1557;
  #line 269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L1iS1557 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3952;
    int32_t _if__result_5364;
    #line 270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3952 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1558);
    if (_M0L1iS1557 < _M0L6_2atmpS3952) {
      int32_t _M0L6_2atmpS3951;
      #line 270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3951 = _M0MPC15array5Array6lengthGsE(_M0L6valuesS1559);
      _if__result_5364 = _M0L1iS1557 < _M0L6_2atmpS3951;
    } else {
      _if__result_5364 = 0;
    }
    if (_if__result_5364) {
      struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3953 =
        _M0L4selfS1560->$0;
      moonbit_string_t _M0L6_2atmpS3954;
      moonbit_string_t _M0L6_2atmpS3956;
      void* _M0L6StringS3955;
      struct _M0TPB3MapGsiE* _M0L7expiresS3957;
      moonbit_string_t _M0L6_2atmpS3958;
      int32_t _M0L6_2atmpS3959;
      moonbit_incref(_M0L4dataS3953);
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3954
      = _M0MPC15array5Array2atGsE(_M0L4keysS1558, _M0L1iS1557);
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3956
      = _M0MPC15array5Array2atGsE(_M0L6valuesS1559, _M0L1iS1557);
      _M0L6StringS3955
      = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
      Moonbit_object_header(_M0L6StringS3955)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
      ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3955)->$0
      = _M0L6_2atmpS3956;
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3953, _M0L6_2atmpS3954, _M0L6StringS3955);
      moonbit_decref(_M0L4dataS3953);
      moonbit_decref(_M0L6_2atmpS3954);
      moonbit_decref(_M0L6StringS3955);
      _M0L7expiresS3957 = _M0L4selfS1560->$1;
      moonbit_incref(_M0L7expiresS3957);
      #line 272 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3958
      = _M0MPC15array5Array2atGsE(_M0L4keysS1558, _M0L1iS1557);
      #line 272 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS3957, _M0L6_2atmpS3958);
      moonbit_decref(_M0L7expiresS3957);
      moonbit_decref(_M0L6_2atmpS3958);
      _M0L6_2atmpS3959 = _M0L1iS1557 + 1;
      _M0L1iS1557 = _M0L6_2atmpS3959;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database7persist(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1555,
  moonbit_string_t _M0L3keyS1556
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3949;
  int32_t _result_5365;
  int32_t _if__result_5366;
  #line 260 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3949 = _M0L4selfS1555->$0;
  moonbit_incref(_M0L4dataS3949);
  #line 261 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_5365
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3949, _M0L3keyS1556);
  moonbit_decref(_M0L4dataS3949);
  if (_result_5365) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3948 = _M0L4selfS1555->$1;
    moonbit_incref(_M0L7expiresS3948);
    #line 261 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _if__result_5366
    = _M0MPB3Map8containsGsiE(_M0L7expiresS3948, _M0L3keyS1556);
    moonbit_decref(_M0L7expiresS3948);
  } else {
    _if__result_5366 = 0;
  }
  if (_if__result_5366) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3950 = _M0L4selfS1555->$1;
    moonbit_incref(_M0L7expiresS3950);
    #line 262 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3950, _M0L3keyS1556);
    moonbit_decref(_M0L7expiresS3950);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1552,
  moonbit_string_t _M0L3keyS1553
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3940;
  int32_t _M0L6_2atmpS3939;
  #line 226 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3940 = _M0L4selfS1552->$0;
  moonbit_incref(_M0L4dataS3940);
  #line 227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3939
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3940, _M0L3keyS1553);
  moonbit_decref(_M0L4dataS3940);
  if (!_M0L6_2atmpS3939) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS3941 = _M0L4selfS1552->$1;
    int32_t _result_5367;
    moonbit_incref(_M0L7expiresS3941);
    #line 229 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_5367 = _M0MPB3Map8containsGsiE(_M0L7expiresS3941, _M0L3keyS1553);
    moonbit_decref(_M0L7expiresS3941);
    if (_result_5367) {
      struct _M0TPB3MapGsiE* _M0L7expiresS3947 = _M0L4selfS1552->$1;
      int64_t _M0L6_2atmpS3946;
      int32_t _M0L6_2atmpS3944;
      int32_t _M0L13current__timeS3945;
      int32_t _M0L9remainingS1554;
      moonbit_incref(_M0L7expiresS3947);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3946 = _M0MPB3Map3getGsiE(_M0L7expiresS3947, _M0L3keyS1553);
      moonbit_decref(_M0L7expiresS3947);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3944 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS3946);
      _M0L13current__timeS3945 = _M0L4selfS1552->$2;
      _M0L9remainingS1554 = _M0L6_2atmpS3944 - _M0L13current__timeS3945;
      if (_M0L9remainingS1554 <= 0) {
        struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3942 =
          _M0L4selfS1552->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS3943;
        moonbit_incref(_M0L4dataS3942);
        #line 232 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3942, _M0L3keyS1553);
        moonbit_decref(_M0L4dataS3942);
        _M0L7expiresS3943 = _M0L4selfS1552->$1;
        moonbit_incref(_M0L7expiresS3943);
        #line 233 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS3943, _M0L3keyS1553);
        moonbit_decref(_M0L7expiresS3943);
        return -2;
      } else {
        return _M0L9remainingS1554 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1549,
  moonbit_string_t _M0L3keyS1550,
  int32_t _M0L7secondsS1551
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3934;
  int32_t _result_5368;
  #line 208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3934 = _M0L4selfS1549->$0;
  moonbit_incref(_M0L4dataS3934);
  #line 209 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_5368
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3934, _M0L3keyS1550);
  moonbit_decref(_M0L4dataS3934);
  if (_result_5368) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3935 = _M0L4selfS1549->$1;
    int32_t _M0L13current__timeS3937 = _M0L4selfS1549->$2;
    int32_t _M0L6_2atmpS3938 = _M0L7secondsS1551 * 1000;
    int32_t _M0L6_2atmpS3936 = _M0L13current__timeS3937 + _M0L6_2atmpS3938;
    moonbit_incref(_M0L7expiresS3935);
    #line 210 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS3935, _M0L3keyS1550, _M0L6_2atmpS3936);
    moonbit_decref(_M0L7expiresS3935);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3set(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1546,
  moonbit_string_t _M0L3keyS1547,
  moonbit_string_t _M0L5valueS1548
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3931;
  void* _M0L6StringS3932;
  struct _M0TPB3MapGsiE* _M0L7expiresS3933;
  #line 203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3931 = _M0L4selfS1546->$0;
  moonbit_incref(_M0L5valueS1548);
  _M0L6StringS3932
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3932)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3932)->$0
  = _M0L5valueS1548;
  moonbit_incref(_M0L4dataS3931);
  #line 204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3931, _M0L3keyS1547, _M0L6StringS3932);
  moonbit_decref(_M0L4dataS3931);
  moonbit_decref(_M0L6StringS3932);
  _M0L7expiresS3933 = _M0L4selfS1546->$1;
  moonbit_incref(_M0L7expiresS3933);
  #line 205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3933, _M0L3keyS1547);
  moonbit_decref(_M0L7expiresS3933);
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1541,
  moonbit_string_t _M0L3keyS1542
) {
  int32_t _M0L12expire__timeS1540;
  struct _M0TPB3MapGsiE* _M0L7expiresS3930;
  int64_t _M0L7_2abindS1543;
  int32_t _M0L13current__timeS3927;
  #line 188 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7expiresS3930 = _M0L4selfS1541->$1;
  moonbit_incref(_M0L7expiresS3930);
  #line 189 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1543 = _M0MPB3Map3getGsiE(_M0L7expiresS3930, _M0L3keyS1542);
  moonbit_decref(_M0L7expiresS3930);
  if (_M0L7_2abindS1543 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1544 = _M0L7_2abindS1543;
    int32_t _M0L15_2aexpire__timeS1545 = (int32_t)_M0L7_2aSomeS1544;
    _M0L12expire__timeS1540 = _M0L15_2aexpire__timeS1545;
    goto join_1539;
  }
  join_1539:;
  _M0L13current__timeS3927 = _M0L4selfS1541->$2;
  if (_M0L12expire__timeS1540 <= _M0L13current__timeS3927) {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3928 =
      _M0L4selfS1541->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS3929;
    moonbit_incref(_M0L4dataS3928);
    #line 192 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3928, _M0L3keyS1542);
    moonbit_decref(_M0L4dataS3928);
    _M0L7expiresS3929 = _M0L4selfS1541->$1;
    moonbit_incref(_M0L7expiresS3929);
    #line 193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3929, _M0L3keyS1542);
    moonbit_decref(_M0L7expiresS3929);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1537,
  int32_t _M0L2msS1538
) {
  int32_t _M0L13current__timeS3926;
  int32_t _M0L6_2atmpS3925;
  #line 184 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L13current__timeS3926 = _M0L4selfS1537->$2;
  _M0L6_2atmpS3925 = _M0L13current__timeS3926 + _M0L2msS1538;
  _M0L4selfS1537->$2 = _M0L6_2atmpS3925;
  return 0;
}

struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0MP38JIA2JIA29moonbitdb3lib8Database3new(
  
) {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS1535;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS3924;
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L6_2atmpS3923;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3919;
  struct _M0TUsiE** _M0L7_2abindS1536;
  struct _M0TUsiE** _M0L6_2atmpS3922;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3921;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3920;
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _block_5370;
  #line 176 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1535
  = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3924 = _M0L7_2abindS1535;
  _M0L6_2atmpS3923
  = (struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE){
    .$0 = _M0L6_2atmpS3924, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3919
  = _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L6_2atmpS3923, 1000ll);
  moonbit_decref(_M0L6_2atmpS3923.$0);
  _M0L7_2abindS1536 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3922 = _M0L7_2abindS1536;
  _M0L6_2atmpS3921
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3922, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3920 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3921, 1000ll);
  moonbit_decref(_M0L6_2atmpS3921.$0);
  _block_5370
  = (struct _M0TP38JIA2JIA29moonbitdb3lib8Database*)moonbit_malloc(sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database));
  Moonbit_object_header(_block_5370)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 27, 0);
  _block_5370->$0 = _M0L6_2atmpS3919;
  _block_5370->$1 = _M0L6_2atmpS3920;
  _block_5370->$2 = 0;
  return _block_5370;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib5Deque9to__array(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1528
) {
  moonbit_string_t* _M0L6_2atmpS3918;
  struct _M0TPB5ArrayGsE* _M0L6resultS1526;
  struct _M0TPB5ArrayGsE* _M0L5frontS3915;
  int32_t _M0L6_2atmpS3914;
  int32_t _M0L6_2atmpS3913;
  int32_t _M0L1iS1527;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1530;
  int32_t _M0L7_2abindS1531;
  int32_t _M0L2__S1532;
  #line 48 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3918 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1526
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1526)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1526->$0 = _M0L6_2atmpS3918;
  _M0L6resultS1526->$1 = 0;
  _M0L5frontS3915 = _M0L4selfS1528->$0;
  moonbit_incref(_M0L5frontS3915);
  #line 50 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3914 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3915);
  moonbit_decref(_M0L5frontS3915);
  _M0L6_2atmpS3913 = _M0L6_2atmpS3914 - 1;
  _M0L1iS1527 = _M0L6_2atmpS3913;
  while (1) {
    if (_M0L1iS1527 >= 0) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3911 = _M0L4selfS1528->$0;
      moonbit_string_t _M0L6_2atmpS3910;
      int32_t _M0L6_2atmpS3912;
      moonbit_incref(_M0L5frontS3911);
      #line 51 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3910
      = _M0MPC15array5Array2atGsE(_M0L5frontS3911, _M0L1iS1527);
      moonbit_decref(_M0L5frontS3911);
      #line 51 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1526, _M0L6_2atmpS3910);
      moonbit_decref(_M0L6_2atmpS3910);
      _M0L6_2atmpS3912 = _M0L1iS1527 - 1;
      _M0L1iS1527 = _M0L6_2atmpS3912;
      continue;
    }
    break;
  }
  _M0L7_2abindS1530 = _M0L4selfS1528->$1;
  _M0L7_2abindS1531 = _M0L7_2abindS1530->$1;
  moonbit_incref(_M0L7_2abindS1530);
  _M0L2__S1532 = 0;
  while (1) {
    if (_M0L2__S1532 < _M0L7_2abindS1531) {
      moonbit_string_t* _M0L3bufS3917 = _M0L7_2abindS1530->$0;
      moonbit_string_t _M0L4itemS1533 =
        (moonbit_string_t)_M0L3bufS3917[_M0L2__S1532];
      int32_t _M0L6_2atmpS3916;
      moonbit_incref(_M0L4itemS1533);
      #line 54 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1526, _M0L4itemS1533);
      moonbit_decref(_M0L4itemS1533);
      _M0L6_2atmpS3916 = _M0L2__S1532 + 1;
      _M0L2__S1532 = _M0L6_2atmpS3916;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1530);
    }
    break;
  }
  return _M0L6resultS1526;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1525
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3909;
  int32_t _M0L6_2atmpS3906;
  struct _M0TPB5ArrayGsE* _M0L4backS3908;
  int32_t _M0L6_2atmpS3907;
  #line 44 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3909 = _M0L4selfS1525->$0;
  moonbit_incref(_M0L5frontS3909);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3906 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3909);
  moonbit_decref(_M0L5frontS3909);
  _M0L4backS3908 = _M0L4selfS1525->$1;
  moonbit_incref(_M0L4backS3908);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3907 = _M0MPC15array5Array6lengthGsE(_M0L4backS3908);
  moonbit_decref(_M0L4backS3908);
  return _M0L6_2atmpS3906 + _M0L6_2atmpS3907;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib5Deque9pop__back(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1518
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3900;
  int32_t _M0L6_2atmpS3899;
  struct _M0TPB5ArrayGsE* _M0L4backS3905;
  moonbit_string_t _result_5375;
  #line 31 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4backS3900 = _M0L4selfS1518->$1;
  moonbit_incref(_M0L4backS3900);
  #line 32 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3899 = _M0MPC15array5Array6lengthGsE(_M0L4backS3900);
  moonbit_decref(_M0L4backS3900);
  if (_M0L6_2atmpS3899 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3902 = _M0L4selfS1518->$0;
      int32_t _M0L6_2atmpS3901;
      moonbit_incref(_M0L5frontS3902);
      #line 33 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3901 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3902);
      moonbit_decref(_M0L5frontS3902);
      if (_M0L6_2atmpS3901 > 0) {
        struct _M0TPB5ArrayGsE* _M0L5frontS3904 = _M0L4selfS1518->$0;
        moonbit_string_t _M0L4itemS1519;
        moonbit_string_t _M0L1vS1521;
        struct _M0TPB5ArrayGsE* _M0L4backS3903;
        moonbit_incref(_M0L5frontS3904);
        #line 34 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L4itemS1519 = _M0MPC15array5Array3popGsE(_M0L5frontS3904);
        moonbit_decref(_M0L5frontS3904);
        if (_M0L4itemS1519 == 0) {
          if (_M0L4itemS1519) {
            moonbit_decref(_M0L4itemS1519);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1522 = _M0L4itemS1519;
          moonbit_string_t _M0L4_2avS1523 = _M0L7_2aSomeS1522;
          _M0L1vS1521 = _M0L4_2avS1523;
          goto join_1520;
        }
        goto joinlet_5374;
        join_1520:;
        _M0L4backS3903 = _M0L4selfS1518->$1;
        moonbit_incref(_M0L4backS3903);
        #line 36 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4backS3903, _M0L1vS1521);
        moonbit_decref(_M0L4backS3903);
        moonbit_decref(_M0L1vS1521);
        joinlet_5374:;
        continue;
      }
      break;
    }
  }
  _M0L4backS3905 = _M0L4selfS1518->$1;
  moonbit_incref(_M0L4backS3905);
  #line 41 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_5375 = _M0MPC15array5Array3popGsE(_M0L4backS3905);
  moonbit_decref(_M0L4backS3905);
  return _result_5375;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib5Deque10pop__front(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1511
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3893;
  int32_t _M0L6_2atmpS3892;
  struct _M0TPB5ArrayGsE* _M0L5frontS3898;
  moonbit_string_t _result_5378;
  #line 18 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3893 = _M0L4selfS1511->$0;
  moonbit_incref(_M0L5frontS3893);
  #line 19 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3892 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3893);
  moonbit_decref(_M0L5frontS3893);
  if (_M0L6_2atmpS3892 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L4backS3895 = _M0L4selfS1511->$1;
      int32_t _M0L6_2atmpS3894;
      moonbit_incref(_M0L4backS3895);
      #line 20 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3894 = _M0MPC15array5Array6lengthGsE(_M0L4backS3895);
      moonbit_decref(_M0L4backS3895);
      if (_M0L6_2atmpS3894 > 0) {
        struct _M0TPB5ArrayGsE* _M0L4backS3897 = _M0L4selfS1511->$1;
        moonbit_string_t _M0L4itemS1512;
        moonbit_string_t _M0L1vS1514;
        struct _M0TPB5ArrayGsE* _M0L5frontS3896;
        moonbit_incref(_M0L4backS3897);
        #line 21 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L4itemS1512 = _M0MPC15array5Array3popGsE(_M0L4backS3897);
        moonbit_decref(_M0L4backS3897);
        if (_M0L4itemS1512 == 0) {
          if (_M0L4itemS1512) {
            moonbit_decref(_M0L4itemS1512);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1515 = _M0L4itemS1512;
          moonbit_string_t _M0L4_2avS1516 = _M0L7_2aSomeS1515;
          _M0L1vS1514 = _M0L4_2avS1516;
          goto join_1513;
        }
        goto joinlet_5377;
        join_1513:;
        _M0L5frontS3896 = _M0L4selfS1511->$0;
        moonbit_incref(_M0L5frontS3896);
        #line 23 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L5frontS3896, _M0L1vS1514);
        moonbit_decref(_M0L5frontS3896);
        moonbit_decref(_M0L1vS1514);
        joinlet_5377:;
        continue;
      }
      break;
    }
  }
  _M0L5frontS3898 = _M0L4selfS1511->$0;
  moonbit_incref(_M0L5frontS3898);
  #line 28 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_5378 = _M0MPC15array5Array3popGsE(_M0L5frontS3898);
  moonbit_decref(_M0L5frontS3898);
  return _result_5378;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque10push__back(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1509,
  moonbit_string_t _M0L5valueS1510
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3891;
  #line 14 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4backS3891 = _M0L4selfS1509->$1;
  moonbit_incref(_M0L4backS3891);
  #line 15 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPC15array5Array4pushGsE(_M0L4backS3891, _M0L5valueS1510);
  moonbit_decref(_M0L4backS3891);
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque11push__front(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1507,
  moonbit_string_t _M0L5valueS1508
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3890;
  #line 10 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3890 = _M0L4selfS1507->$0;
  moonbit_incref(_M0L5frontS3890);
  #line 11 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPC15array5Array4pushGsE(_M0L5frontS3890, _M0L5valueS1508);
  moonbit_decref(_M0L5frontS3890);
  return 0;
}

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0MP38JIA2JIA29moonbitdb3lib5Deque3new(
  
) {
  moonbit_string_t* _M0L6_2atmpS3889;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3886;
  moonbit_string_t* _M0L6_2atmpS3888;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3887;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _block_5379;
  #line 6 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3889 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3886
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3886)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS3886->$0 = _M0L6_2atmpS3889;
  _M0L6_2atmpS3886->$1 = 0;
  _M0L6_2atmpS3888 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3887
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3887)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS3887->$0 = _M0L6_2atmpS3888;
  _M0L6_2atmpS3887->$1 = 0;
  _block_5379
  = (struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*)moonbit_malloc(sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque));
  Moonbit_object_header(_block_5379)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 31, 0);
  _block_5379->$0 = _M0L6_2atmpS3886;
  _block_5379->$1 = _M0L6_2atmpS3887;
  return _block_5379;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1506) {
  double _M0L6_2atmpS3885;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3885 = (double)_M0L4selfS1506;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3885);
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1504,
  struct _M0TPC16string10StringView _M0L9separatorS1505
) {
  moonbit_string_t* _M0L3bufS3883;
  int32_t _M0L3lenS3884;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3882;
  moonbit_string_t _result_5380;
  #line 2184 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS3883 = _M0L4selfS1504->$0;
  _M0L3lenS3884 = _M0L4selfS1504->$1;
  moonbit_incref(_M0L3bufS3883);
  _M0L6_2atmpS3882
  = (struct _M0TPB9ArrayViewGsE){
    .$0 = _M0L3bufS3883, .$1 = 0, .$2 = _M0L3lenS3884
  };
  #line 2188 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _result_5380
  = _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS3882, _M0L9separatorS1505);
  moonbit_decref(_M0L6_2atmpS3882.$0);
  return _result_5380;
}

moonbit_string_t _M0MPC15array5Array3popGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1501
) {
  int32_t _M0L3lenS1500;
  #line 325 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1500 = _M0L4selfS1501->$1;
  if (_M0L3lenS1500 == 0) {
    return 0;
  } else {
    int32_t _M0L5indexS1502 = _M0L3lenS1500 - 1;
    moonbit_string_t* _M0L3bufS3881 = _M0L4selfS1501->$0;
    moonbit_string_t _M0L1vS1503 =
      (moonbit_string_t)_M0L3bufS3881[_M0L5indexS1502];
    moonbit_string_t* _M0L3bufS3880 = _M0L4selfS1501->$0;
    moonbit_string_t _M0L6_2aoldS4698;
    if (
      _M0L5indexS1502 < 0
      || _M0L5indexS1502 >= Moonbit_array_length(_M0L3bufS3880)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4698 = (moonbit_string_t)_M0L3bufS3880[_M0L5indexS1502];
    moonbit_incref(_M0L1vS1503);
    moonbit_decref(_M0L6_2aoldS4698);
    if (
      _M0L5indexS1502 < 0
      || _M0L5indexS1502 >= Moonbit_array_length(_M0L3bufS3880)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3880[_M0L5indexS1502]
    = (moonbit_string_t)moonbit_string_literal_75.data;
    _M0L4selfS1501->$1 = _M0L5indexS1502;
    return _M0L1vS1503;
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1492,
  int32_t _M0L5indexS1493
) {
  int32_t _M0L3lenS1491;
  int32_t _if__result_5381;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1491 = _M0L4selfS1492->$1;
  if (_M0L5indexS1493 >= 0) {
    _if__result_5381 = _M0L5indexS1493 < _M0L3lenS1491;
  } else {
    _if__result_5381 = 0;
  }
  if (_if__result_5381) {
    moonbit_string_t* _M0L6_2atmpS3877;
    moonbit_string_t _M0L6_2atmpS4702;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3877 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1492);
    _M0L6_2atmpS4702 = (moonbit_string_t)_M0L6_2atmpS3877[_M0L5indexS1493];
    moonbit_incref(_M0L6_2atmpS4702);
    moonbit_decref(_M0L6_2atmpS3877);
    return _M0L6_2atmpS4702;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS1495,
  int32_t _M0L5indexS1496
) {
  int32_t _M0L3lenS1494;
  int32_t _if__result_5382;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1494 = _M0L4selfS1495->$1;
  if (_M0L5indexS1496 >= 0) {
    _if__result_5382 = _M0L5indexS1496 < _M0L3lenS1494;
  } else {
    _if__result_5382 = 0;
  }
  if (_if__result_5382) {
    moonbit_string_t* _M0L6_2atmpS3878;
    moonbit_string_t _M0L6_2atmpS4703;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3878 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS1495);
    _M0L6_2atmpS4703 = (moonbit_string_t)_M0L6_2atmpS3878[_M0L5indexS1496];
    if (_M0L6_2atmpS4703) {
      moonbit_incref(_M0L6_2atmpS4703);
    }
    moonbit_decref(_M0L6_2atmpS3878);
    return _M0L6_2atmpS4703;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TUsfE* _M0MPC15array5Array2atGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS1498,
  int32_t _M0L5indexS1499
) {
  int32_t _M0L3lenS1497;
  int32_t _if__result_5383;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1497 = _M0L4selfS1498->$1;
  if (_M0L5indexS1499 >= 0) {
    _if__result_5383 = _M0L5indexS1499 < _M0L3lenS1497;
  } else {
    _if__result_5383 = 0;
  }
  if (_if__result_5383) {
    struct _M0TUsfE** _M0L6_2atmpS3879;
    struct _M0TUsfE* _M0L6_2atmpS4704;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3879 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1498);
    _M0L6_2atmpS4704 = (struct _M0TUsfE*)_M0L6_2atmpS3879[_M0L5indexS1499];
    if (_M0L6_2atmpS4704) {
      moonbit_incref(_M0L6_2atmpS4704);
    }
    moonbit_decref(_M0L6_2atmpS3879);
    return _M0L6_2atmpS4704;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1490) {
  moonbit_string_t _M0L6_2atmpS3876;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3876
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1490);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3876);
  moonbit_decref(_M0L6_2atmpS3876);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1489) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1489);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1476) {
  uint64_t _M0L4bitsS1477;
  uint64_t _M0L6_2atmpS3875;
  uint64_t _M0L6_2atmpS3874;
  int32_t _M0L8ieeeSignS1478;
  uint64_t _M0L12ieeeMantissaS1479;
  uint64_t _M0L6_2atmpS3873;
  uint64_t _M0L6_2atmpS3872;
  int32_t _M0L12ieeeExponentS1480;
  int32_t _if__result_5384;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1481;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1482;
  moonbit_string_t _result_5386;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1476 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  _M0L4bitsS1477 = *(int64_t*)&_M0L3valS1476;
  _M0L6_2atmpS3875 = _M0L4bitsS1477 >> 63;
  _M0L6_2atmpS3874 = _M0L6_2atmpS3875 & 1ull;
  _M0L8ieeeSignS1478 = _M0L6_2atmpS3874 != 0ull;
  _M0L12ieeeMantissaS1479 = _M0L4bitsS1477 & 4503599627370495ull;
  _M0L6_2atmpS3873 = _M0L4bitsS1477 >> 52;
  _M0L6_2atmpS3872 = _M0L6_2atmpS3873 & 2047ull;
  _M0L12ieeeExponentS1480 = (int32_t)_M0L6_2atmpS3872;
  if (_M0L12ieeeExponentS1480 == 2047) {
    _if__result_5384 = 1;
  } else if (_M0L12ieeeExponentS1480 == 0) {
    _if__result_5384 = _M0L12ieeeMantissaS1479 == 0ull;
  } else {
    _if__result_5384 = 0;
  }
  if (_if__result_5384) {
    int32_t _M0L6_2atmpS3863 = _M0L12ieeeExponentS1480 != 0;
    int32_t _M0L6_2atmpS3864 = _M0L12ieeeMantissaS1479 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1478, _M0L6_2atmpS3863, _M0L6_2atmpS3864);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1481
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1479, _M0L12ieeeExponentS1480);
  if (_M0L7_2abindS1481 == 0) {
    uint32_t _M0L6_2atmpS3865;
    if (_M0L7_2abindS1481) {
      moonbit_decref(_M0L7_2abindS1481);
    }
    _M0L6_2atmpS3865 = *(uint32_t*)&_M0L12ieeeExponentS1480;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1482 = _M0FPB3d2d(_M0L12ieeeMantissaS1479, _M0L6_2atmpS3865);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1483 = _M0L7_2abindS1481;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1484 = _M0L7_2aSomeS1483;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1485 = _M0L4_2afS1484;
    while (1) {
      uint64_t _M0L8mantissaS3871 = _M0L1xS1485->$0;
      uint64_t _M0L1qS1486 = _M0L8mantissaS3871 / 10ull;
      uint64_t _M0L8mantissaS3869 = _M0L1xS1485->$0;
      uint64_t _M0L6_2atmpS3870 = 10ull * _M0L1qS1486;
      uint64_t _M0L1rS1487 = _M0L8mantissaS3869 - _M0L6_2atmpS3870;
      int32_t _M0L8exponentS3868;
      int32_t _M0L6_2atmpS3867;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3866;
      if (_M0L1rS1487 != 0ull) {
        _M0L1vS1482 = _M0L1xS1485;
        break;
      }
      _M0L8exponentS3868 = _M0L1xS1485->$1;
      moonbit_decref(_M0L1xS1485);
      _M0L6_2atmpS3867 = _M0L8exponentS3868 + 1;
      _M0L6_2atmpS3866
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3866)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3866->$0 = _M0L1qS1486;
      _M0L6_2atmpS3866->$1 = _M0L6_2atmpS3867;
      _M0L1xS1485 = _M0L6_2atmpS3866;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_5386 = _M0FPB9to__chars(_M0L1vS1482, _M0L8ieeeSignS1478);
  moonbit_decref(_M0L1vS1482);
  return _result_5386;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1471,
  int32_t _M0L12ieeeExponentS1473
) {
  uint64_t _M0L2m2S1470;
  int32_t _M0L6_2atmpS3862;
  int32_t _M0L2e2S1472;
  int32_t _M0L6_2atmpS3861;
  uint64_t _M0L6_2atmpS3860;
  uint64_t _M0L4maskS1474;
  uint64_t _M0L8fractionS1475;
  int32_t _M0L6_2atmpS3859;
  uint64_t _M0L6_2atmpS3858;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3857;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1470 = 4503599627370496ull | _M0L12ieeeMantissaS1471;
  _M0L6_2atmpS3862 = _M0L12ieeeExponentS1473 - 1023;
  _M0L2e2S1472 = _M0L6_2atmpS3862 - 52;
  if (_M0L2e2S1472 > 0) {
    return 0;
  }
  if (_M0L2e2S1472 < -52) {
    return 0;
  }
  _M0L6_2atmpS3861 = -_M0L2e2S1472;
  _M0L6_2atmpS3860 = 1ull << (_M0L6_2atmpS3861 & 63);
  _M0L4maskS1474 = _M0L6_2atmpS3860 - 1ull;
  _M0L8fractionS1475 = _M0L2m2S1470 & _M0L4maskS1474;
  if (_M0L8fractionS1475 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3859 = -_M0L2e2S1472;
  _M0L6_2atmpS3858 = _M0L2m2S1470 >> (_M0L6_2atmpS3859 & 63);
  _M0L6_2atmpS3857
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3857)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3857->$0 = _M0L6_2atmpS3858;
  _M0L6_2atmpS3857->$1 = 0;
  return _M0L6_2atmpS3857;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1438,
  int32_t _M0L4signS1436
) {
  int32_t _M0L6_2atmpS3856;
  moonbit_bytes_t _M0L6resultS1434;
  int32_t _M0Lm5indexS1435;
  uint64_t _M0L6outputS1437;
  int32_t _M0L7olengthS1439;
  int32_t _M0L8exponentS3855;
  int32_t _M0L6_2atmpS3854;
  int32_t _M0Lm3expS1440;
  int32_t _M0L6_2atmpS3853;
  int32_t _M0L6_2atmpS3851;
  int32_t _M0L18scientificNotationS1441;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3856 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1434
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3856);
  _M0Lm5indexS1435 = 0;
  if (_M0L4signS1436) {
    int32_t _M0L6_2atmpS3725 = _M0Lm5indexS1435;
    int32_t _M0L6_2atmpS3726;
    if (
      _M0L6_2atmpS3725 < 0
      || _M0L6_2atmpS3725 >= Moonbit_array_length(_M0L6resultS1434)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1434[_M0L6_2atmpS3725] = 45;
    _M0L6_2atmpS3726 = _M0Lm5indexS1435;
    _M0Lm5indexS1435 = _M0L6_2atmpS3726 + 1;
  }
  _M0L6outputS1437 = _M0L1vS1438->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1439 = _M0FPB17decimal__length17(_M0L6outputS1437);
  _M0L8exponentS3855 = _M0L1vS1438->$1;
  _M0L6_2atmpS3854 = _M0L8exponentS3855 + _M0L7olengthS1439;
  _M0Lm3expS1440 = _M0L6_2atmpS3854 - 1;
  _M0L6_2atmpS3853 = _M0Lm3expS1440;
  if (_M0L6_2atmpS3853 >= -6) {
    int32_t _M0L6_2atmpS3852 = _M0Lm3expS1440;
    _M0L6_2atmpS3851 = _M0L6_2atmpS3852 < 21;
  } else {
    _M0L6_2atmpS3851 = 0;
  }
  _M0L18scientificNotationS1441 = !_M0L6_2atmpS3851;
  if (_M0L18scientificNotationS1441) {
    int32_t _M0L7_2abindS1442 = _M0L7olengthS1439 - 1;
    uint64_t _M0L6outputS1443;
    int32_t _M0L1iS1444 = 0;
    uint64_t _M0L6outputS1445 = _M0L6outputS1437;
    int32_t _M0L6_2atmpS3727;
    int32_t _M0L6_2atmpS3731;
    int32_t _M0L6_2atmpS3730;
    int32_t _M0L6_2atmpS3729;
    int32_t _M0L6_2atmpS3728;
    int32_t _M0L6_2atmpS3735;
    int32_t _M0L6_2atmpS3736;
    int32_t _M0L6_2atmpS3737;
    int32_t _M0L6_2atmpS3738;
    int32_t _M0L6_2atmpS3739;
    int32_t _M0L6_2atmpS3745;
    int32_t _M0L6_2atmpS3778;
    moonbit_string_t _result_5388;
    while (1) {
      if (_M0L1iS1444 < _M0L7_2abindS1442) {
        uint64_t _M0L1cS1446 = _M0L6outputS1445 % 10ull;
        int32_t _M0L6_2atmpS3784 = _M0Lm5indexS1435;
        int32_t _M0L6_2atmpS3783 = _M0L6_2atmpS3784 + _M0L7olengthS1439;
        int32_t _M0L6_2atmpS3779 = _M0L6_2atmpS3783 - _M0L1iS1444;
        int32_t _M0L6_2atmpS3782 = (int32_t)_M0L1cS1446;
        int32_t _M0L6_2atmpS3781 = 48 + _M0L6_2atmpS3782;
        int32_t _M0L6_2atmpS3780 = _M0L6_2atmpS3781 & 0xff;
        int32_t _M0L6_2atmpS3785;
        uint64_t _M0L6_2atmpS3786;
        if (
          _M0L6_2atmpS3779 < 0
          || _M0L6_2atmpS3779 >= Moonbit_array_length(_M0L6resultS1434)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1434[_M0L6_2atmpS3779] = _M0L6_2atmpS3780;
        _M0L6_2atmpS3785 = _M0L1iS1444 + 1;
        _M0L6_2atmpS3786 = _M0L6outputS1445 / 10ull;
        _M0L1iS1444 = _M0L6_2atmpS3785;
        _M0L6outputS1445 = _M0L6_2atmpS3786;
        continue;
      } else {
        _M0L6outputS1443 = _M0L6outputS1445;
      }
      break;
    }
    _M0L6_2atmpS3727 = _M0Lm5indexS1435;
    _M0L6_2atmpS3731 = (int32_t)_M0L6outputS1443;
    _M0L6_2atmpS3730 = _M0L6_2atmpS3731 % 10;
    _M0L6_2atmpS3729 = 48 + _M0L6_2atmpS3730;
    _M0L6_2atmpS3728 = _M0L6_2atmpS3729 & 0xff;
    if (
      _M0L6_2atmpS3727 < 0
      || _M0L6_2atmpS3727 >= Moonbit_array_length(_M0L6resultS1434)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1434[_M0L6_2atmpS3727] = _M0L6_2atmpS3728;
    if (_M0L7olengthS1439 > 1) {
      int32_t _M0L6_2atmpS3733 = _M0Lm5indexS1435;
      int32_t _M0L6_2atmpS3732 = _M0L6_2atmpS3733 + 1;
      if (
        _M0L6_2atmpS3732 < 0
        || _M0L6_2atmpS3732 >= Moonbit_array_length(_M0L6resultS1434)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1434[_M0L6_2atmpS3732] = 46;
    } else {
      int32_t _M0L6_2atmpS3734 = _M0Lm5indexS1435;
      _M0Lm5indexS1435 = _M0L6_2atmpS3734 - 1;
    }
    _M0L6_2atmpS3735 = _M0Lm5indexS1435;
    _M0L6_2atmpS3736 = _M0L7olengthS1439 + 1;
    _M0Lm5indexS1435 = _M0L6_2atmpS3735 + _M0L6_2atmpS3736;
    _M0L6_2atmpS3737 = _M0Lm5indexS1435;
    if (
      _M0L6_2atmpS3737 < 0
      || _M0L6_2atmpS3737 >= Moonbit_array_length(_M0L6resultS1434)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1434[_M0L6_2atmpS3737] = 101;
    _M0L6_2atmpS3738 = _M0Lm5indexS1435;
    _M0Lm5indexS1435 = _M0L6_2atmpS3738 + 1;
    _M0L6_2atmpS3739 = _M0Lm3expS1440;
    if (_M0L6_2atmpS3739 < 0) {
      int32_t _M0L6_2atmpS3740 = _M0Lm5indexS1435;
      int32_t _M0L6_2atmpS3741;
      int32_t _M0L6_2atmpS3742;
      if (
        _M0L6_2atmpS3740 < 0
        || _M0L6_2atmpS3740 >= Moonbit_array_length(_M0L6resultS1434)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1434[_M0L6_2atmpS3740] = 45;
      _M0L6_2atmpS3741 = _M0Lm5indexS1435;
      _M0Lm5indexS1435 = _M0L6_2atmpS3741 + 1;
      _M0L6_2atmpS3742 = _M0Lm3expS1440;
      _M0Lm3expS1440 = -_M0L6_2atmpS3742;
    } else {
      int32_t _M0L6_2atmpS3743 = _M0Lm5indexS1435;
      int32_t _M0L6_2atmpS3744;
      if (
        _M0L6_2atmpS3743 < 0
        || _M0L6_2atmpS3743 >= Moonbit_array_length(_M0L6resultS1434)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1434[_M0L6_2atmpS3743] = 43;
      _M0L6_2atmpS3744 = _M0Lm5indexS1435;
      _M0Lm5indexS1435 = _M0L6_2atmpS3744 + 1;
    }
    _M0L6_2atmpS3745 = _M0Lm3expS1440;
    if (_M0L6_2atmpS3745 >= 100) {
      int32_t _M0L6_2atmpS3761 = _M0Lm3expS1440;
      int32_t _M0L1aS1448 = _M0L6_2atmpS3761 / 100;
      int32_t _M0L6_2atmpS3760 = _M0Lm3expS1440;
      int32_t _M0L6_2atmpS3759 = _M0L6_2atmpS3760 / 10;
      int32_t _M0L1bS1449 = _M0L6_2atmpS3759 % 10;
      int32_t _M0L6_2atmpS3758 = _M0Lm3expS1440;
      int32_t _M0L1cS1450 = _M0L6_2atmpS3758 % 10;
      int32_t _M0L6_2atmpS3746 = _M0Lm5indexS1435;
      int32_t _M0L6_2atmpS3748 = 48 + _M0L1aS1448;
      int32_t _M0L6_2atmpS3747 = _M0L6_2atmpS3748 & 0xff;
      int32_t _M0L6_2atmpS3752;
      int32_t _M0L6_2atmpS3749;
      int32_t _M0L6_2atmpS3751;
      int32_t _M0L6_2atmpS3750;
      int32_t _M0L6_2atmpS3756;
      int32_t _M0L6_2atmpS3753;
      int32_t _M0L6_2atmpS3755;
      int32_t _M0L6_2atmpS3754;
      int32_t _M0L6_2atmpS3757;
      if (
        _M0L6_2atmpS3746 < 0
        || _M0L6_2atmpS3746 >= Moonbit_array_length(_M0L6resultS1434)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1434[_M0L6_2atmpS3746] = _M0L6_2atmpS3747;
      _M0L6_2atmpS3752 = _M0Lm5indexS1435;
      _M0L6_2atmpS3749 = _M0L6_2atmpS3752 + 1;
      _M0L6_2atmpS3751 = 48 + _M0L1bS1449;
      _M0L6_2atmpS3750 = _M0L6_2atmpS3751 & 0xff;
      if (
        _M0L6_2atmpS3749 < 0
        || _M0L6_2atmpS3749 >= Moonbit_array_length(_M0L6resultS1434)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1434[_M0L6_2atmpS3749] = _M0L6_2atmpS3750;
      _M0L6_2atmpS3756 = _M0Lm5indexS1435;
      _M0L6_2atmpS3753 = _M0L6_2atmpS3756 + 2;
      _M0L6_2atmpS3755 = 48 + _M0L1cS1450;
      _M0L6_2atmpS3754 = _M0L6_2atmpS3755 & 0xff;
      if (
        _M0L6_2atmpS3753 < 0
        || _M0L6_2atmpS3753 >= Moonbit_array_length(_M0L6resultS1434)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1434[_M0L6_2atmpS3753] = _M0L6_2atmpS3754;
      _M0L6_2atmpS3757 = _M0Lm5indexS1435;
      _M0Lm5indexS1435 = _M0L6_2atmpS3757 + 3;
    } else {
      int32_t _M0L6_2atmpS3762 = _M0Lm3expS1440;
      if (_M0L6_2atmpS3762 >= 10) {
        int32_t _M0L6_2atmpS3772 = _M0Lm3expS1440;
        int32_t _M0L1aS1451 = _M0L6_2atmpS3772 / 10;
        int32_t _M0L6_2atmpS3771 = _M0Lm3expS1440;
        int32_t _M0L1bS1452 = _M0L6_2atmpS3771 % 10;
        int32_t _M0L6_2atmpS3763 = _M0Lm5indexS1435;
        int32_t _M0L6_2atmpS3765 = 48 + _M0L1aS1451;
        int32_t _M0L6_2atmpS3764 = _M0L6_2atmpS3765 & 0xff;
        int32_t _M0L6_2atmpS3769;
        int32_t _M0L6_2atmpS3766;
        int32_t _M0L6_2atmpS3768;
        int32_t _M0L6_2atmpS3767;
        int32_t _M0L6_2atmpS3770;
        if (
          _M0L6_2atmpS3763 < 0
          || _M0L6_2atmpS3763 >= Moonbit_array_length(_M0L6resultS1434)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1434[_M0L6_2atmpS3763] = _M0L6_2atmpS3764;
        _M0L6_2atmpS3769 = _M0Lm5indexS1435;
        _M0L6_2atmpS3766 = _M0L6_2atmpS3769 + 1;
        _M0L6_2atmpS3768 = 48 + _M0L1bS1452;
        _M0L6_2atmpS3767 = _M0L6_2atmpS3768 & 0xff;
        if (
          _M0L6_2atmpS3766 < 0
          || _M0L6_2atmpS3766 >= Moonbit_array_length(_M0L6resultS1434)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1434[_M0L6_2atmpS3766] = _M0L6_2atmpS3767;
        _M0L6_2atmpS3770 = _M0Lm5indexS1435;
        _M0Lm5indexS1435 = _M0L6_2atmpS3770 + 2;
      } else {
        int32_t _M0L6_2atmpS3773 = _M0Lm5indexS1435;
        int32_t _M0L6_2atmpS3776 = _M0Lm3expS1440;
        int32_t _M0L6_2atmpS3775 = 48 + _M0L6_2atmpS3776;
        int32_t _M0L6_2atmpS3774 = _M0L6_2atmpS3775 & 0xff;
        int32_t _M0L6_2atmpS3777;
        if (
          _M0L6_2atmpS3773 < 0
          || _M0L6_2atmpS3773 >= Moonbit_array_length(_M0L6resultS1434)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1434[_M0L6_2atmpS3773] = _M0L6_2atmpS3774;
        _M0L6_2atmpS3777 = _M0Lm5indexS1435;
        _M0Lm5indexS1435 = _M0L6_2atmpS3777 + 1;
      }
    }
    _M0L6_2atmpS3778 = _M0Lm5indexS1435;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_5388
    = _M0FPB19string__from__bytes(_M0L6resultS1434, 0, _M0L6_2atmpS3778);
    moonbit_decref(_M0L6resultS1434);
    return _result_5388;
  } else {
    int32_t _M0L6_2atmpS3787 = _M0Lm3expS1440;
    int32_t _M0L6_2atmpS3850;
    moonbit_string_t _result_5394;
    if (_M0L6_2atmpS3787 < 0) {
      int32_t _M0L6_2atmpS3788 = _M0Lm5indexS1435;
      int32_t _M0L6_2atmpS3790;
      int32_t _M0L6_2atmpS3789;
      int32_t _M0L6_2atmpS3791;
      int32_t _M0L1iS1453;
      int32_t _M0L6_2atmpS3806;
      int32_t _M0L6_2atmpS3808;
      int32_t _M0L6_2atmpS3807;
      int32_t _M0L7currentS1455;
      int32_t _M0L1iS1456;
      uint64_t _M0L6outputS1457;
      if (
        _M0L6_2atmpS3788 < 0
        || _M0L6_2atmpS3788 >= Moonbit_array_length(_M0L6resultS1434)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1434[_M0L6_2atmpS3788] = 48;
      _M0L6_2atmpS3790 = _M0Lm5indexS1435;
      _M0L6_2atmpS3789 = _M0L6_2atmpS3790 + 1;
      if (
        _M0L6_2atmpS3789 < 0
        || _M0L6_2atmpS3789 >= Moonbit_array_length(_M0L6resultS1434)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1434[_M0L6_2atmpS3789] = 46;
      _M0L6_2atmpS3791 = _M0Lm5indexS1435;
      _M0Lm5indexS1435 = _M0L6_2atmpS3791 + 2;
      _M0L1iS1453 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3792 = _M0Lm3expS1440;
        if (_M0L1iS1453 > _M0L6_2atmpS3792) {
          int32_t _M0L6_2atmpS3795 = _M0Lm5indexS1435;
          int32_t _M0L6_2atmpS3794 = _M0L6_2atmpS3795 - _M0L1iS1453;
          int32_t _M0L6_2atmpS3793 = _M0L6_2atmpS3794 - 1;
          int32_t _M0L6_2atmpS3796;
          if (
            _M0L6_2atmpS3793 < 0
            || _M0L6_2atmpS3793 >= Moonbit_array_length(_M0L6resultS1434)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1434[_M0L6_2atmpS3793] = 48;
          _M0L6_2atmpS3796 = _M0L1iS1453 - 1;
          _M0L1iS1453 = _M0L6_2atmpS3796;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3806 = _M0Lm5indexS1435;
      _M0L6_2atmpS3808 = _M0Lm3expS1440;
      _M0L6_2atmpS3807 = -1 - _M0L6_2atmpS3808;
      _M0L7currentS1455 = _M0L6_2atmpS3806 + _M0L6_2atmpS3807;
      _M0L1iS1456 = 0;
      _M0L6outputS1457 = _M0L6outputS1437;
      while (1) {
        if (_M0L1iS1456 < _M0L7olengthS1439) {
          int32_t _M0L6_2atmpS3803 = _M0L7currentS1455 + _M0L7olengthS1439;
          int32_t _M0L6_2atmpS3802 = _M0L6_2atmpS3803 - _M0L1iS1456;
          int32_t _M0L6_2atmpS3797 = _M0L6_2atmpS3802 - 1;
          uint64_t _M0L6_2atmpS3801 = _M0L6outputS1457 % 10ull;
          int32_t _M0L6_2atmpS3800 = (int32_t)_M0L6_2atmpS3801;
          int32_t _M0L6_2atmpS3799 = 48 + _M0L6_2atmpS3800;
          int32_t _M0L6_2atmpS3798 = _M0L6_2atmpS3799 & 0xff;
          int32_t _M0L6_2atmpS3804;
          uint64_t _M0L6_2atmpS3805;
          if (
            _M0L6_2atmpS3797 < 0
            || _M0L6_2atmpS3797 >= Moonbit_array_length(_M0L6resultS1434)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1434[_M0L6_2atmpS3797] = _M0L6_2atmpS3798;
          _M0L6_2atmpS3804 = _M0L1iS1456 + 1;
          _M0L6_2atmpS3805 = _M0L6outputS1457 / 10ull;
          _M0L1iS1456 = _M0L6_2atmpS3804;
          _M0L6outputS1457 = _M0L6_2atmpS3805;
          continue;
        }
        break;
      }
      _M0Lm5indexS1435 = _M0L7currentS1455 + _M0L7olengthS1439;
    } else {
      int32_t _M0L6_2atmpS3810 = _M0Lm3expS1440;
      int32_t _M0L6_2atmpS3809 = _M0L6_2atmpS3810 + 1;
      if (_M0L6_2atmpS3809 >= _M0L7olengthS1439) {
        int32_t _M0L1iS1459 = 0;
        uint64_t _M0L6outputS1460 = _M0L6outputS1437;
        int32_t _M0L6_2atmpS3821;
        int32_t _M0L6_2atmpS3826;
        int32_t _M0L7_2abindS1462;
        int32_t _M0L1iS1463;
        int32_t _M0L6_2atmpS3827;
        int32_t _M0L6_2atmpS3830;
        int32_t _M0L6_2atmpS3829;
        int32_t _M0L6_2atmpS3828;
        while (1) {
          if (_M0L1iS1459 < _M0L7olengthS1439) {
            int32_t _M0L6_2atmpS3818 = _M0Lm5indexS1435;
            int32_t _M0L6_2atmpS3817 = _M0L6_2atmpS3818 + _M0L7olengthS1439;
            int32_t _M0L6_2atmpS3816 = _M0L6_2atmpS3817 - _M0L1iS1459;
            int32_t _M0L6_2atmpS3811 = _M0L6_2atmpS3816 - 1;
            uint64_t _M0L6_2atmpS3815 = _M0L6outputS1460 % 10ull;
            int32_t _M0L6_2atmpS3814 = (int32_t)_M0L6_2atmpS3815;
            int32_t _M0L6_2atmpS3813 = 48 + _M0L6_2atmpS3814;
            int32_t _M0L6_2atmpS3812 = _M0L6_2atmpS3813 & 0xff;
            int32_t _M0L6_2atmpS3819;
            uint64_t _M0L6_2atmpS3820;
            if (
              _M0L6_2atmpS3811 < 0
              || _M0L6_2atmpS3811 >= Moonbit_array_length(_M0L6resultS1434)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1434[_M0L6_2atmpS3811] = _M0L6_2atmpS3812;
            _M0L6_2atmpS3819 = _M0L1iS1459 + 1;
            _M0L6_2atmpS3820 = _M0L6outputS1460 / 10ull;
            _M0L1iS1459 = _M0L6_2atmpS3819;
            _M0L6outputS1460 = _M0L6_2atmpS3820;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3821 = _M0Lm5indexS1435;
        _M0Lm5indexS1435 = _M0L6_2atmpS3821 + _M0L7olengthS1439;
        _M0L6_2atmpS3826 = _M0Lm3expS1440;
        _M0L7_2abindS1462 = _M0L6_2atmpS3826 + 1;
        _M0L1iS1463 = _M0L7olengthS1439;
        while (1) {
          if (_M0L1iS1463 < _M0L7_2abindS1462) {
            int32_t _M0L6_2atmpS3824 = _M0Lm5indexS1435;
            int32_t _M0L6_2atmpS3823 = _M0L6_2atmpS3824 + _M0L1iS1463;
            int32_t _M0L6_2atmpS3822 = _M0L6_2atmpS3823 - _M0L7olengthS1439;
            int32_t _M0L6_2atmpS3825;
            if (
              _M0L6_2atmpS3822 < 0
              || _M0L6_2atmpS3822 >= Moonbit_array_length(_M0L6resultS1434)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1434[_M0L6_2atmpS3822] = 48;
            _M0L6_2atmpS3825 = _M0L1iS1463 + 1;
            _M0L1iS1463 = _M0L6_2atmpS3825;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3827 = _M0Lm5indexS1435;
        _M0L6_2atmpS3830 = _M0Lm3expS1440;
        _M0L6_2atmpS3829 = _M0L6_2atmpS3830 + 1;
        _M0L6_2atmpS3828 = _M0L6_2atmpS3829 - _M0L7olengthS1439;
        _M0Lm5indexS1435 = _M0L6_2atmpS3827 + _M0L6_2atmpS3828;
      } else {
        int32_t _M0L6_2atmpS3847 = _M0Lm5indexS1435;
        int32_t _M0L6_2atmpS3846 = _M0L6_2atmpS3847 + 1;
        int32_t _M0L1iS1465 = 0;
        int32_t _M0L7currentS1466 = _M0L6_2atmpS3846;
        uint64_t _M0L6outputS1467 = _M0L6outputS1437;
        int32_t _M0L6_2atmpS3848;
        int32_t _M0L6_2atmpS3849;
        while (1) {
          if (_M0L1iS1465 < _M0L7olengthS1439) {
            int32_t _M0L6_2atmpS3842 = _M0L7olengthS1439 - _M0L1iS1465;
            int32_t _M0L6_2atmpS3840 = _M0L6_2atmpS3842 - 1;
            int32_t _M0L6_2atmpS3841 = _M0Lm3expS1440;
            int32_t _M0L7currentS1468;
            int32_t _M0L6_2atmpS3837;
            int32_t _M0L6_2atmpS3836;
            int32_t _M0L6_2atmpS3831;
            uint64_t _M0L6_2atmpS3835;
            int32_t _M0L6_2atmpS3834;
            int32_t _M0L6_2atmpS3833;
            int32_t _M0L6_2atmpS3832;
            int32_t _M0L6_2atmpS3838;
            uint64_t _M0L6_2atmpS3839;
            if (_M0L6_2atmpS3840 == _M0L6_2atmpS3841) {
              int32_t _M0L6_2atmpS3845 =
                _M0L7currentS1466 + _M0L7olengthS1439;
              int32_t _M0L6_2atmpS3844 = _M0L6_2atmpS3845 - _M0L1iS1465;
              int32_t _M0L6_2atmpS3843 = _M0L6_2atmpS3844 - 1;
              if (
                _M0L6_2atmpS3843 < 0
                || _M0L6_2atmpS3843 >= Moonbit_array_length(_M0L6resultS1434)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1434[_M0L6_2atmpS3843] = 46;
              _M0L7currentS1468 = _M0L7currentS1466 - 1;
            } else {
              _M0L7currentS1468 = _M0L7currentS1466;
            }
            _M0L6_2atmpS3837 = _M0L7currentS1468 + _M0L7olengthS1439;
            _M0L6_2atmpS3836 = _M0L6_2atmpS3837 - _M0L1iS1465;
            _M0L6_2atmpS3831 = _M0L6_2atmpS3836 - 1;
            _M0L6_2atmpS3835 = _M0L6outputS1467 % 10ull;
            _M0L6_2atmpS3834 = (int32_t)_M0L6_2atmpS3835;
            _M0L6_2atmpS3833 = 48 + _M0L6_2atmpS3834;
            _M0L6_2atmpS3832 = _M0L6_2atmpS3833 & 0xff;
            if (
              _M0L6_2atmpS3831 < 0
              || _M0L6_2atmpS3831 >= Moonbit_array_length(_M0L6resultS1434)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1434[_M0L6_2atmpS3831] = _M0L6_2atmpS3832;
            _M0L6_2atmpS3838 = _M0L1iS1465 + 1;
            _M0L6_2atmpS3839 = _M0L6outputS1467 / 10ull;
            _M0L1iS1465 = _M0L6_2atmpS3838;
            _M0L7currentS1466 = _M0L7currentS1468;
            _M0L6outputS1467 = _M0L6_2atmpS3839;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3848 = _M0Lm5indexS1435;
        _M0L6_2atmpS3849 = _M0L7olengthS1439 + 1;
        _M0Lm5indexS1435 = _M0L6_2atmpS3848 + _M0L6_2atmpS3849;
      }
    }
    _M0L6_2atmpS3850 = _M0Lm5indexS1435;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_5394
    = _M0FPB19string__from__bytes(_M0L6resultS1434, 0, _M0L6_2atmpS3850);
    moonbit_decref(_M0L6resultS1434);
    return _result_5394;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1380,
  uint32_t _M0L12ieeeExponentS1379
) {
  int32_t _M0Lm2e2S1377;
  uint64_t _M0Lm2m2S1378;
  uint64_t _M0L6_2atmpS3724;
  uint64_t _M0L6_2atmpS3723;
  int32_t _M0L4evenS1381;
  uint64_t _M0L6_2atmpS3722;
  uint64_t _M0L2mvS1382;
  int32_t _M0L7mmShiftS1383;
  uint64_t _M0Lm2vrS1384;
  uint64_t _M0Lm2vpS1385;
  uint64_t _M0Lm2vmS1386;
  int32_t _M0Lm3e10S1387;
  int32_t _M0Lm17vmIsTrailingZerosS1388;
  int32_t _M0Lm17vrIsTrailingZerosS1389;
  int32_t _M0L6_2atmpS3624;
  int32_t _M0Lm7removedS1408;
  int32_t _M0Lm16lastRemovedDigitS1409;
  uint64_t _M0Lm6outputS1410;
  int32_t _M0L6_2atmpS3720;
  int32_t _M0L6_2atmpS3721;
  int32_t _M0L3expS1433;
  uint64_t _M0L6_2atmpS3719;
  struct _M0TPB17FloatingDecimal64* _block_5400;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1377 = 0;
  _M0Lm2m2S1378 = 0ull;
  if (_M0L12ieeeExponentS1379 == 0u) {
    _M0Lm2e2S1377 = -1076;
    _M0Lm2m2S1378 = _M0L12ieeeMantissaS1380;
  } else {
    int32_t _M0L6_2atmpS3623 = *(int32_t*)&_M0L12ieeeExponentS1379;
    int32_t _M0L6_2atmpS3622 = _M0L6_2atmpS3623 - 1023;
    int32_t _M0L6_2atmpS3621 = _M0L6_2atmpS3622 - 52;
    _M0Lm2e2S1377 = _M0L6_2atmpS3621 - 2;
    _M0Lm2m2S1378 = 4503599627370496ull | _M0L12ieeeMantissaS1380;
  }
  _M0L6_2atmpS3724 = _M0Lm2m2S1378;
  _M0L6_2atmpS3723 = _M0L6_2atmpS3724 & 1ull;
  _M0L4evenS1381 = _M0L6_2atmpS3723 == 0ull;
  _M0L6_2atmpS3722 = _M0Lm2m2S1378;
  _M0L2mvS1382 = 4ull * _M0L6_2atmpS3722;
  if (_M0L12ieeeMantissaS1380 != 0ull) {
    _M0L7mmShiftS1383 = 1;
  } else {
    _M0L7mmShiftS1383 = _M0L12ieeeExponentS1379 <= 1u;
  }
  _M0Lm2vrS1384 = 0ull;
  _M0Lm2vpS1385 = 0ull;
  _M0Lm2vmS1386 = 0ull;
  _M0Lm3e10S1387 = 0;
  _M0Lm17vmIsTrailingZerosS1388 = 0;
  _M0Lm17vrIsTrailingZerosS1389 = 0;
  _M0L6_2atmpS3624 = _M0Lm2e2S1377;
  if (_M0L6_2atmpS3624 >= 0) {
    int32_t _M0L6_2atmpS3646 = _M0Lm2e2S1377;
    int32_t _M0L6_2atmpS3642;
    int32_t _M0L6_2atmpS3645;
    int32_t _M0L6_2atmpS3644;
    int32_t _M0L6_2atmpS3643;
    int32_t _M0L1qS1390;
    int32_t _M0L6_2atmpS3641;
    int32_t _M0L6_2atmpS3640;
    int32_t _M0L1kS1391;
    int32_t _M0L6_2atmpS3639;
    int32_t _M0L6_2atmpS3638;
    int32_t _M0L6_2atmpS3637;
    int32_t _M0L1iS1392;
    struct _M0TPB8Pow5Pair _M0L4pow5S1393;
    uint64_t _M0L6_2atmpS3636;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1394;
    uint64_t _M0L8_2avrOutS1395;
    uint64_t _M0L8_2avpOutS1396;
    uint64_t _M0L8_2avmOutS1397;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3642 = _M0FPB9log10Pow2(_M0L6_2atmpS3646);
    _M0L6_2atmpS3645 = _M0Lm2e2S1377;
    _M0L6_2atmpS3644 = _M0L6_2atmpS3645 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3643 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3644);
    _M0L1qS1390 = _M0L6_2atmpS3642 - _M0L6_2atmpS3643;
    _M0Lm3e10S1387 = _M0L1qS1390;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3641 = _M0FPB8pow5bits(_M0L1qS1390);
    _M0L6_2atmpS3640 = 125 + _M0L6_2atmpS3641;
    _M0L1kS1391 = _M0L6_2atmpS3640 - 1;
    _M0L6_2atmpS3639 = _M0Lm2e2S1377;
    _M0L6_2atmpS3638 = -_M0L6_2atmpS3639;
    _M0L6_2atmpS3637 = _M0L6_2atmpS3638 + _M0L1qS1390;
    _M0L1iS1392 = _M0L6_2atmpS3637 + _M0L1kS1391;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1393 = _M0FPB22double__computeInvPow5(_M0L1qS1390);
    _M0L6_2atmpS3636 = _M0Lm2m2S1378;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1394
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3636, _M0L4pow5S1393, _M0L1iS1392, _M0L7mmShiftS1383);
    _M0L8_2avrOutS1395 = _M0L7_2abindS1394.$0;
    _M0L8_2avpOutS1396 = _M0L7_2abindS1394.$1;
    _M0L8_2avmOutS1397 = _M0L7_2abindS1394.$2;
    _M0Lm2vrS1384 = _M0L8_2avrOutS1395;
    _M0Lm2vpS1385 = _M0L8_2avpOutS1396;
    _M0Lm2vmS1386 = _M0L8_2avmOutS1397;
    if (_M0L1qS1390 <= 21) {
      int32_t _M0L6_2atmpS3632 = (int32_t)_M0L2mvS1382;
      uint64_t _M0L6_2atmpS3635 = _M0L2mvS1382 / 5ull;
      int32_t _M0L6_2atmpS3634 = (int32_t)_M0L6_2atmpS3635;
      int32_t _M0L6_2atmpS3633 = 5 * _M0L6_2atmpS3634;
      int32_t _M0L6mvMod5S1398 = _M0L6_2atmpS3632 - _M0L6_2atmpS3633;
      if (_M0L6mvMod5S1398 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1389
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1382, _M0L1qS1390);
      } else if (_M0L4evenS1381) {
        uint64_t _M0L6_2atmpS3626 = _M0L2mvS1382 - 1ull;
        uint64_t _M0L6_2atmpS3627;
        uint64_t _M0L6_2atmpS3625;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3627 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1383);
        _M0L6_2atmpS3625 = _M0L6_2atmpS3626 - _M0L6_2atmpS3627;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1388
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3625, _M0L1qS1390);
      } else {
        uint64_t _M0L6_2atmpS3628 = _M0Lm2vpS1385;
        uint64_t _M0L6_2atmpS3631 = _M0L2mvS1382 + 2ull;
        int32_t _M0L6_2atmpS3630;
        uint64_t _M0L6_2atmpS3629;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3630
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3631, _M0L1qS1390);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3629 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3630);
        _M0Lm2vpS1385 = _M0L6_2atmpS3628 - _M0L6_2atmpS3629;
      }
    }
  } else {
    int32_t _M0L6_2atmpS3660 = _M0Lm2e2S1377;
    int32_t _M0L6_2atmpS3659 = -_M0L6_2atmpS3660;
    int32_t _M0L6_2atmpS3654;
    int32_t _M0L6_2atmpS3658;
    int32_t _M0L6_2atmpS3657;
    int32_t _M0L6_2atmpS3656;
    int32_t _M0L6_2atmpS3655;
    int32_t _M0L1qS1399;
    int32_t _M0L6_2atmpS3647;
    int32_t _M0L6_2atmpS3653;
    int32_t _M0L6_2atmpS3652;
    int32_t _M0L1iS1400;
    int32_t _M0L6_2atmpS3651;
    int32_t _M0L1kS1401;
    int32_t _M0L1jS1402;
    struct _M0TPB8Pow5Pair _M0L4pow5S1403;
    uint64_t _M0L6_2atmpS3650;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1404;
    uint64_t _M0L8_2avrOutS1405;
    uint64_t _M0L8_2avpOutS1406;
    uint64_t _M0L8_2avmOutS1407;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3654 = _M0FPB9log10Pow5(_M0L6_2atmpS3659);
    _M0L6_2atmpS3658 = _M0Lm2e2S1377;
    _M0L6_2atmpS3657 = -_M0L6_2atmpS3658;
    _M0L6_2atmpS3656 = _M0L6_2atmpS3657 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3655 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3656);
    _M0L1qS1399 = _M0L6_2atmpS3654 - _M0L6_2atmpS3655;
    _M0L6_2atmpS3647 = _M0Lm2e2S1377;
    _M0Lm3e10S1387 = _M0L1qS1399 + _M0L6_2atmpS3647;
    _M0L6_2atmpS3653 = _M0Lm2e2S1377;
    _M0L6_2atmpS3652 = -_M0L6_2atmpS3653;
    _M0L1iS1400 = _M0L6_2atmpS3652 - _M0L1qS1399;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3651 = _M0FPB8pow5bits(_M0L1iS1400);
    _M0L1kS1401 = _M0L6_2atmpS3651 - 125;
    _M0L1jS1402 = _M0L1qS1399 - _M0L1kS1401;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1403 = _M0FPB19double__computePow5(_M0L1iS1400);
    _M0L6_2atmpS3650 = _M0Lm2m2S1378;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1404
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3650, _M0L4pow5S1403, _M0L1jS1402, _M0L7mmShiftS1383);
    _M0L8_2avrOutS1405 = _M0L7_2abindS1404.$0;
    _M0L8_2avpOutS1406 = _M0L7_2abindS1404.$1;
    _M0L8_2avmOutS1407 = _M0L7_2abindS1404.$2;
    _M0Lm2vrS1384 = _M0L8_2avrOutS1405;
    _M0Lm2vpS1385 = _M0L8_2avpOutS1406;
    _M0Lm2vmS1386 = _M0L8_2avmOutS1407;
    if (_M0L1qS1399 <= 1) {
      _M0Lm17vrIsTrailingZerosS1389 = 1;
      if (_M0L4evenS1381) {
        int32_t _M0L6_2atmpS3648;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3648 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1383);
        _M0Lm17vmIsTrailingZerosS1388 = _M0L6_2atmpS3648 == 1;
      } else {
        uint64_t _M0L6_2atmpS3649 = _M0Lm2vpS1385;
        _M0Lm2vpS1385 = _M0L6_2atmpS3649 - 1ull;
      }
    } else if (_M0L1qS1399 < 63) {
      #line 437 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1389
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1382, _M0L1qS1399);
    }
  }
  _M0Lm7removedS1408 = 0;
  _M0Lm16lastRemovedDigitS1409 = 0;
  _M0Lm6outputS1410 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1388 || _M0Lm17vrIsTrailingZerosS1389) {
    int32_t _if__result_5397;
    uint64_t _M0L6_2atmpS3690;
    uint64_t _M0L6_2atmpS3696;
    uint64_t _M0L6_2atmpS3697;
    int32_t _if__result_5398;
    int32_t _M0L6_2atmpS3693;
    int64_t _M0L6_2atmpS3692;
    uint64_t _M0L6_2atmpS3691;
    while (1) {
      uint64_t _M0L6_2atmpS3673 = _M0Lm2vpS1385;
      uint64_t _M0L7vpDiv10S1411 = _M0L6_2atmpS3673 / 10ull;
      uint64_t _M0L6_2atmpS3672 = _M0Lm2vmS1386;
      uint64_t _M0L7vmDiv10S1412 = _M0L6_2atmpS3672 / 10ull;
      uint64_t _M0L6_2atmpS3671;
      int32_t _M0L6_2atmpS3668;
      int32_t _M0L6_2atmpS3670;
      int32_t _M0L6_2atmpS3669;
      int32_t _M0L7vmMod10S1414;
      uint64_t _M0L6_2atmpS3667;
      uint64_t _M0L7vrDiv10S1415;
      uint64_t _M0L6_2atmpS3666;
      int32_t _M0L6_2atmpS3663;
      int32_t _M0L6_2atmpS3665;
      int32_t _M0L6_2atmpS3664;
      int32_t _M0L7vrMod10S1416;
      int32_t _M0L6_2atmpS3662;
      if (_M0L7vpDiv10S1411 <= _M0L7vmDiv10S1412) {
        break;
      }
      _M0L6_2atmpS3671 = _M0Lm2vmS1386;
      _M0L6_2atmpS3668 = (int32_t)_M0L6_2atmpS3671;
      _M0L6_2atmpS3670 = (int32_t)_M0L7vmDiv10S1412;
      _M0L6_2atmpS3669 = 10 * _M0L6_2atmpS3670;
      _M0L7vmMod10S1414 = _M0L6_2atmpS3668 - _M0L6_2atmpS3669;
      _M0L6_2atmpS3667 = _M0Lm2vrS1384;
      _M0L7vrDiv10S1415 = _M0L6_2atmpS3667 / 10ull;
      _M0L6_2atmpS3666 = _M0Lm2vrS1384;
      _M0L6_2atmpS3663 = (int32_t)_M0L6_2atmpS3666;
      _M0L6_2atmpS3665 = (int32_t)_M0L7vrDiv10S1415;
      _M0L6_2atmpS3664 = 10 * _M0L6_2atmpS3665;
      _M0L7vrMod10S1416 = _M0L6_2atmpS3663 - _M0L6_2atmpS3664;
      if (_M0Lm17vmIsTrailingZerosS1388) {
        _M0Lm17vmIsTrailingZerosS1388 = _M0L7vmMod10S1414 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1388 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1389) {
        int32_t _M0L6_2atmpS3661 = _M0Lm16lastRemovedDigitS1409;
        _M0Lm17vrIsTrailingZerosS1389 = _M0L6_2atmpS3661 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1389 = 0;
      }
      _M0Lm16lastRemovedDigitS1409 = _M0L7vrMod10S1416;
      _M0Lm2vrS1384 = _M0L7vrDiv10S1415;
      _M0Lm2vpS1385 = _M0L7vpDiv10S1411;
      _M0Lm2vmS1386 = _M0L7vmDiv10S1412;
      _M0L6_2atmpS3662 = _M0Lm7removedS1408;
      _M0Lm7removedS1408 = _M0L6_2atmpS3662 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1388) {
      while (1) {
        uint64_t _M0L6_2atmpS3686 = _M0Lm2vmS1386;
        uint64_t _M0L7vmDiv10S1417 = _M0L6_2atmpS3686 / 10ull;
        uint64_t _M0L6_2atmpS3685 = _M0Lm2vmS1386;
        int32_t _M0L6_2atmpS3682 = (int32_t)_M0L6_2atmpS3685;
        int32_t _M0L6_2atmpS3684 = (int32_t)_M0L7vmDiv10S1417;
        int32_t _M0L6_2atmpS3683 = 10 * _M0L6_2atmpS3684;
        int32_t _M0L7vmMod10S1418 = _M0L6_2atmpS3682 - _M0L6_2atmpS3683;
        uint64_t _M0L6_2atmpS3681;
        uint64_t _M0L7vpDiv10S1420;
        uint64_t _M0L6_2atmpS3680;
        uint64_t _M0L7vrDiv10S1421;
        uint64_t _M0L6_2atmpS3679;
        int32_t _M0L6_2atmpS3676;
        int32_t _M0L6_2atmpS3678;
        int32_t _M0L6_2atmpS3677;
        int32_t _M0L7vrMod10S1422;
        int32_t _M0L6_2atmpS3675;
        if (_M0L7vmMod10S1418 != 0) {
          break;
        }
        _M0L6_2atmpS3681 = _M0Lm2vpS1385;
        _M0L7vpDiv10S1420 = _M0L6_2atmpS3681 / 10ull;
        _M0L6_2atmpS3680 = _M0Lm2vrS1384;
        _M0L7vrDiv10S1421 = _M0L6_2atmpS3680 / 10ull;
        _M0L6_2atmpS3679 = _M0Lm2vrS1384;
        _M0L6_2atmpS3676 = (int32_t)_M0L6_2atmpS3679;
        _M0L6_2atmpS3678 = (int32_t)_M0L7vrDiv10S1421;
        _M0L6_2atmpS3677 = 10 * _M0L6_2atmpS3678;
        _M0L7vrMod10S1422 = _M0L6_2atmpS3676 - _M0L6_2atmpS3677;
        if (_M0Lm17vrIsTrailingZerosS1389) {
          int32_t _M0L6_2atmpS3674 = _M0Lm16lastRemovedDigitS1409;
          _M0Lm17vrIsTrailingZerosS1389 = _M0L6_2atmpS3674 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1389 = 0;
        }
        _M0Lm16lastRemovedDigitS1409 = _M0L7vrMod10S1422;
        _M0Lm2vrS1384 = _M0L7vrDiv10S1421;
        _M0Lm2vpS1385 = _M0L7vpDiv10S1420;
        _M0Lm2vmS1386 = _M0L7vmDiv10S1417;
        _M0L6_2atmpS3675 = _M0Lm7removedS1408;
        _M0Lm7removedS1408 = _M0L6_2atmpS3675 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1389) {
      int32_t _M0L6_2atmpS3689 = _M0Lm16lastRemovedDigitS1409;
      if (_M0L6_2atmpS3689 == 5) {
        uint64_t _M0L6_2atmpS3688 = _M0Lm2vrS1384;
        uint64_t _M0L6_2atmpS3687 = _M0L6_2atmpS3688 % 2ull;
        _if__result_5397 = _M0L6_2atmpS3687 == 0ull;
      } else {
        _if__result_5397 = 0;
      }
    } else {
      _if__result_5397 = 0;
    }
    if (_if__result_5397) {
      _M0Lm16lastRemovedDigitS1409 = 4;
    }
    _M0L6_2atmpS3690 = _M0Lm2vrS1384;
    _M0L6_2atmpS3696 = _M0Lm2vrS1384;
    _M0L6_2atmpS3697 = _M0Lm2vmS1386;
    if (_M0L6_2atmpS3696 == _M0L6_2atmpS3697) {
      if (!_M0L4evenS1381) {
        _if__result_5398 = 1;
      } else {
        int32_t _M0L6_2atmpS3695 = _M0Lm17vmIsTrailingZerosS1388;
        _if__result_5398 = !_M0L6_2atmpS3695;
      }
    } else {
      _if__result_5398 = 0;
    }
    if (_if__result_5398) {
      _M0L6_2atmpS3693 = 1;
    } else {
      int32_t _M0L6_2atmpS3694 = _M0Lm16lastRemovedDigitS1409;
      _M0L6_2atmpS3693 = _M0L6_2atmpS3694 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3692 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3693);
    _M0L6_2atmpS3691 = *(uint64_t*)&_M0L6_2atmpS3692;
    _M0Lm6outputS1410 = _M0L6_2atmpS3690 + _M0L6_2atmpS3691;
  } else {
    int32_t _M0Lm7roundUpS1423 = 0;
    uint64_t _M0L6_2atmpS3718 = _M0Lm2vpS1385;
    uint64_t _M0L8vpDiv100S1424 = _M0L6_2atmpS3718 / 100ull;
    uint64_t _M0L6_2atmpS3717 = _M0Lm2vmS1386;
    uint64_t _M0L8vmDiv100S1425 = _M0L6_2atmpS3717 / 100ull;
    uint64_t _M0L6_2atmpS3712;
    uint64_t _M0L6_2atmpS3715;
    uint64_t _M0L6_2atmpS3716;
    int32_t _M0L6_2atmpS3714;
    uint64_t _M0L6_2atmpS3713;
    if (_M0L8vpDiv100S1424 > _M0L8vmDiv100S1425) {
      uint64_t _M0L6_2atmpS3703 = _M0Lm2vrS1384;
      uint64_t _M0L8vrDiv100S1426 = _M0L6_2atmpS3703 / 100ull;
      uint64_t _M0L6_2atmpS3702 = _M0Lm2vrS1384;
      int32_t _M0L6_2atmpS3699 = (int32_t)_M0L6_2atmpS3702;
      int32_t _M0L6_2atmpS3701 = (int32_t)_M0L8vrDiv100S1426;
      int32_t _M0L6_2atmpS3700 = 100 * _M0L6_2atmpS3701;
      int32_t _M0L8vrMod100S1427 = _M0L6_2atmpS3699 - _M0L6_2atmpS3700;
      int32_t _M0L6_2atmpS3698;
      _M0Lm7roundUpS1423 = _M0L8vrMod100S1427 >= 50;
      _M0Lm2vrS1384 = _M0L8vrDiv100S1426;
      _M0Lm2vpS1385 = _M0L8vpDiv100S1424;
      _M0Lm2vmS1386 = _M0L8vmDiv100S1425;
      _M0L6_2atmpS3698 = _M0Lm7removedS1408;
      _M0Lm7removedS1408 = _M0L6_2atmpS3698 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3711 = _M0Lm2vpS1385;
      uint64_t _M0L7vpDiv10S1428 = _M0L6_2atmpS3711 / 10ull;
      uint64_t _M0L6_2atmpS3710 = _M0Lm2vmS1386;
      uint64_t _M0L7vmDiv10S1429 = _M0L6_2atmpS3710 / 10ull;
      uint64_t _M0L6_2atmpS3709;
      uint64_t _M0L7vrDiv10S1431;
      uint64_t _M0L6_2atmpS3708;
      int32_t _M0L6_2atmpS3705;
      int32_t _M0L6_2atmpS3707;
      int32_t _M0L6_2atmpS3706;
      int32_t _M0L7vrMod10S1432;
      int32_t _M0L6_2atmpS3704;
      if (_M0L7vpDiv10S1428 <= _M0L7vmDiv10S1429) {
        break;
      }
      _M0L6_2atmpS3709 = _M0Lm2vrS1384;
      _M0L7vrDiv10S1431 = _M0L6_2atmpS3709 / 10ull;
      _M0L6_2atmpS3708 = _M0Lm2vrS1384;
      _M0L6_2atmpS3705 = (int32_t)_M0L6_2atmpS3708;
      _M0L6_2atmpS3707 = (int32_t)_M0L7vrDiv10S1431;
      _M0L6_2atmpS3706 = 10 * _M0L6_2atmpS3707;
      _M0L7vrMod10S1432 = _M0L6_2atmpS3705 - _M0L6_2atmpS3706;
      _M0Lm7roundUpS1423 = _M0L7vrMod10S1432 >= 5;
      _M0Lm2vrS1384 = _M0L7vrDiv10S1431;
      _M0Lm2vpS1385 = _M0L7vpDiv10S1428;
      _M0Lm2vmS1386 = _M0L7vmDiv10S1429;
      _M0L6_2atmpS3704 = _M0Lm7removedS1408;
      _M0Lm7removedS1408 = _M0L6_2atmpS3704 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3712 = _M0Lm2vrS1384;
    _M0L6_2atmpS3715 = _M0Lm2vrS1384;
    _M0L6_2atmpS3716 = _M0Lm2vmS1386;
    _M0L6_2atmpS3714
    = _M0L6_2atmpS3715 == _M0L6_2atmpS3716 || _M0Lm7roundUpS1423;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3713 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3714);
    _M0Lm6outputS1410 = _M0L6_2atmpS3712 + _M0L6_2atmpS3713;
  }
  _M0L6_2atmpS3720 = _M0Lm3e10S1387;
  _M0L6_2atmpS3721 = _M0Lm7removedS1408;
  _M0L3expS1433 = _M0L6_2atmpS3720 + _M0L6_2atmpS3721;
  _M0L6_2atmpS3719 = _M0Lm6outputS1410;
  _block_5400
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_5400)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_5400->$0 = _M0L6_2atmpS3719;
  _block_5400->$1 = _M0L3expS1433;
  return _block_5400;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1376) {
  #line 110 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1376) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1375) {
  #line 58 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1375) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1374) {
  #line 32 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1374) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1373) {
  #line 280 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L1vS1373 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1373 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1373 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1373 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1373 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1373 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1373 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1373 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1373 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1373 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1373 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1373 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1373 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1373 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1373 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1373 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1356) {
  int32_t _M0L6_2atmpS3620;
  int32_t _M0L6_2atmpS3619;
  int32_t _M0L4baseS1355;
  int32_t _M0L5base2S1357;
  int32_t _M0L6offsetS1358;
  int32_t _M0L6_2atmpS3618;
  uint64_t _M0L4mul0S1359;
  int32_t _M0L6_2atmpS3617;
  int32_t _M0L6_2atmpS3616;
  uint64_t _M0L4mul1S1360;
  uint64_t _M0L1mS1361;
  struct _M0TPB7Umul128 _M0L7_2abindS1362;
  uint64_t _M0L7_2alow1S1363;
  uint64_t _M0L8_2ahigh1S1364;
  struct _M0TPB7Umul128 _M0L7_2abindS1365;
  uint64_t _M0L7_2alow0S1366;
  uint64_t _M0L8_2ahigh0S1367;
  uint64_t _M0L3sumS1368;
  uint64_t _M0Lm5high1S1369;
  int32_t _M0L6_2atmpS3614;
  int32_t _M0L6_2atmpS3615;
  int32_t _M0L5deltaS1370;
  uint64_t _M0L6_2atmpS3613;
  uint64_t _M0L6_2atmpS3605;
  int32_t _M0L6_2atmpS3612;
  uint32_t _M0L6_2atmpS3609;
  int32_t _M0L6_2atmpS3611;
  int32_t _M0L6_2atmpS3610;
  uint32_t _M0L6_2atmpS3608;
  uint32_t _M0L6_2atmpS3607;
  uint64_t _M0L6_2atmpS3606;
  uint64_t _M0L1aS1371;
  uint64_t _M0L6_2atmpS3604;
  uint64_t _M0L1bS1372;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3620 = _M0L1iS1356 + 26;
  _M0L6_2atmpS3619 = _M0L6_2atmpS3620 - 1;
  _M0L4baseS1355 = _M0L6_2atmpS3619 / 26;
  _M0L5base2S1357 = _M0L4baseS1355 * 26;
  _M0L6offsetS1358 = _M0L5base2S1357 - _M0L1iS1356;
  _M0L6_2atmpS3618 = _M0L4baseS1355 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1359
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3618);
  _M0L6_2atmpS3617 = _M0L4baseS1355 * 2;
  _M0L6_2atmpS3616 = _M0L6_2atmpS3617 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1360
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3616);
  if (_M0L6offsetS1358 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1359,
                                      .$1 = _M0L4mul1S1360};
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1361
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1358);
  #line 249 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1362 = _M0FPB7umul128(_M0L1mS1361, _M0L4mul1S1360);
  _M0L7_2alow1S1363 = _M0L7_2abindS1362.$0;
  _M0L8_2ahigh1S1364 = _M0L7_2abindS1362.$1;
  #line 250 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1365 = _M0FPB7umul128(_M0L1mS1361, _M0L4mul0S1359);
  _M0L7_2alow0S1366 = _M0L7_2abindS1365.$0;
  _M0L8_2ahigh0S1367 = _M0L7_2abindS1365.$1;
  _M0L3sumS1368 = _M0L8_2ahigh0S1367 + _M0L7_2alow1S1363;
  _M0Lm5high1S1369 = _M0L8_2ahigh1S1364;
  if (_M0L3sumS1368 < _M0L8_2ahigh0S1367) {
    uint64_t _M0L6_2atmpS3603 = _M0Lm5high1S1369;
    _M0Lm5high1S1369 = _M0L6_2atmpS3603 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3614 = _M0FPB8pow5bits(_M0L5base2S1357);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3615 = _M0FPB8pow5bits(_M0L1iS1356);
  _M0L5deltaS1370 = _M0L6_2atmpS3614 - _M0L6_2atmpS3615;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3613
  = _M0FPB13shiftright128(_M0L7_2alow0S1366, _M0L3sumS1368, _M0L5deltaS1370);
  _M0L6_2atmpS3605 = _M0L6_2atmpS3613 + 1ull;
  _M0L6_2atmpS3612 = _M0L1iS1356 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3609
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS3612);
  _M0L6_2atmpS3611 = _M0L1iS1356 % 16;
  _M0L6_2atmpS3610 = _M0L6_2atmpS3611 << 1;
  _M0L6_2atmpS3608 = _M0L6_2atmpS3609 >> (_M0L6_2atmpS3610 & 31);
  _M0L6_2atmpS3607 = _M0L6_2atmpS3608 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3606 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3607);
  _M0L1aS1371 = _M0L6_2atmpS3605 + _M0L6_2atmpS3606;
  _M0L6_2atmpS3604 = _M0Lm5high1S1369;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1372
  = _M0FPB13shiftright128(_M0L3sumS1368, _M0L6_2atmpS3604, _M0L5deltaS1370);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1371, .$1 = _M0L1bS1372};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1338) {
  int32_t _M0L4baseS1337;
  int32_t _M0L5base2S1339;
  int32_t _M0L6offsetS1340;
  int32_t _M0L6_2atmpS3602;
  uint64_t _M0L4mul0S1341;
  int32_t _M0L6_2atmpS3601;
  int32_t _M0L6_2atmpS3600;
  uint64_t _M0L4mul1S1342;
  uint64_t _M0L1mS1343;
  struct _M0TPB7Umul128 _M0L7_2abindS1344;
  uint64_t _M0L7_2alow1S1345;
  uint64_t _M0L8_2ahigh1S1346;
  struct _M0TPB7Umul128 _M0L7_2abindS1347;
  uint64_t _M0L7_2alow0S1348;
  uint64_t _M0L8_2ahigh0S1349;
  uint64_t _M0L3sumS1350;
  uint64_t _M0Lm5high1S1351;
  int32_t _M0L6_2atmpS3598;
  int32_t _M0L6_2atmpS3599;
  int32_t _M0L5deltaS1352;
  uint64_t _M0L6_2atmpS3590;
  int32_t _M0L6_2atmpS3597;
  uint32_t _M0L6_2atmpS3594;
  int32_t _M0L6_2atmpS3596;
  int32_t _M0L6_2atmpS3595;
  uint32_t _M0L6_2atmpS3593;
  uint32_t _M0L6_2atmpS3592;
  uint64_t _M0L6_2atmpS3591;
  uint64_t _M0L1aS1353;
  uint64_t _M0L6_2atmpS3589;
  uint64_t _M0L1bS1354;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1337 = _M0L1iS1338 / 26;
  _M0L5base2S1339 = _M0L4baseS1337 * 26;
  _M0L6offsetS1340 = _M0L1iS1338 - _M0L5base2S1339;
  _M0L6_2atmpS3602 = _M0L4baseS1337 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1341
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3602);
  _M0L6_2atmpS3601 = _M0L4baseS1337 * 2;
  _M0L6_2atmpS3600 = _M0L6_2atmpS3601 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1342
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3600);
  if (_M0L6offsetS1340 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1341,
                                      .$1 = _M0L4mul1S1342};
  }
  #line 222 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1343
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1340);
  #line 223 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1344 = _M0FPB7umul128(_M0L1mS1343, _M0L4mul1S1342);
  _M0L7_2alow1S1345 = _M0L7_2abindS1344.$0;
  _M0L8_2ahigh1S1346 = _M0L7_2abindS1344.$1;
  #line 224 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1347 = _M0FPB7umul128(_M0L1mS1343, _M0L4mul0S1341);
  _M0L7_2alow0S1348 = _M0L7_2abindS1347.$0;
  _M0L8_2ahigh0S1349 = _M0L7_2abindS1347.$1;
  _M0L3sumS1350 = _M0L8_2ahigh0S1349 + _M0L7_2alow1S1345;
  _M0Lm5high1S1351 = _M0L8_2ahigh1S1346;
  if (_M0L3sumS1350 < _M0L8_2ahigh0S1349) {
    uint64_t _M0L6_2atmpS3588 = _M0Lm5high1S1351;
    _M0Lm5high1S1351 = _M0L6_2atmpS3588 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3598 = _M0FPB8pow5bits(_M0L1iS1338);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3599 = _M0FPB8pow5bits(_M0L5base2S1339);
  _M0L5deltaS1352 = _M0L6_2atmpS3598 - _M0L6_2atmpS3599;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3590
  = _M0FPB13shiftright128(_M0L7_2alow0S1348, _M0L3sumS1350, _M0L5deltaS1352);
  _M0L6_2atmpS3597 = _M0L1iS1338 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3594
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS3597);
  _M0L6_2atmpS3596 = _M0L1iS1338 % 16;
  _M0L6_2atmpS3595 = _M0L6_2atmpS3596 << 1;
  _M0L6_2atmpS3593 = _M0L6_2atmpS3594 >> (_M0L6_2atmpS3595 & 31);
  _M0L6_2atmpS3592 = _M0L6_2atmpS3593 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3591 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3592);
  _M0L1aS1353 = _M0L6_2atmpS3590 + _M0L6_2atmpS3591;
  _M0L6_2atmpS3589 = _M0Lm5high1S1351;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1354
  = _M0FPB13shiftright128(_M0L3sumS1350, _M0L6_2atmpS3589, _M0L5deltaS1352);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1353, .$1 = _M0L1bS1354};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1311,
  struct _M0TPB8Pow5Pair _M0L3mulS1308,
  int32_t _M0L1jS1324,
  int32_t _M0L7mmShiftS1326
) {
  uint64_t _M0L7_2amul0S1307;
  uint64_t _M0L7_2amul1S1309;
  uint64_t _M0L1mS1310;
  struct _M0TPB7Umul128 _M0L7_2abindS1312;
  uint64_t _M0L5_2aloS1313;
  uint64_t _M0L6_2atmpS1314;
  struct _M0TPB7Umul128 _M0L7_2abindS1315;
  uint64_t _M0L6_2alo2S1316;
  uint64_t _M0L6_2ahi2S1317;
  uint64_t _M0L3midS1318;
  uint64_t _M0L6_2atmpS3587;
  uint64_t _M0L2hiS1319;
  uint64_t _M0L3lo2S1320;
  uint64_t _M0L6_2atmpS3585;
  uint64_t _M0L6_2atmpS3586;
  uint64_t _M0L4mid2S1321;
  uint64_t _M0L6_2atmpS3584;
  uint64_t _M0L3hi2S1322;
  int32_t _M0L6_2atmpS3583;
  int32_t _M0L6_2atmpS3582;
  uint64_t _M0L2vpS1323;
  uint64_t _M0Lm2vmS1325;
  int32_t _M0L6_2atmpS3581;
  int32_t _M0L6_2atmpS3580;
  uint64_t _M0L2vrS1336;
  uint64_t _M0L6_2atmpS3579;
  #line 129 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2amul0S1307 = _M0L3mulS1308.$0;
  _M0L7_2amul1S1309 = _M0L3mulS1308.$1;
  _M0L1mS1310 = _M0L1mS1311 << 1;
  #line 137 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1312 = _M0FPB7umul128(_M0L1mS1310, _M0L7_2amul0S1307);
  _M0L5_2aloS1313 = _M0L7_2abindS1312.$0;
  _M0L6_2atmpS1314 = _M0L7_2abindS1312.$1;
  #line 138 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1315 = _M0FPB7umul128(_M0L1mS1310, _M0L7_2amul1S1309);
  _M0L6_2alo2S1316 = _M0L7_2abindS1315.$0;
  _M0L6_2ahi2S1317 = _M0L7_2abindS1315.$1;
  _M0L3midS1318 = _M0L6_2atmpS1314 + _M0L6_2alo2S1316;
  if (_M0L3midS1318 < _M0L6_2atmpS1314) {
    _M0L6_2atmpS3587 = 1ull;
  } else {
    _M0L6_2atmpS3587 = 0ull;
  }
  _M0L2hiS1319 = _M0L6_2ahi2S1317 + _M0L6_2atmpS3587;
  _M0L3lo2S1320 = _M0L5_2aloS1313 + _M0L7_2amul0S1307;
  _M0L6_2atmpS3585 = _M0L3midS1318 + _M0L7_2amul1S1309;
  if (_M0L3lo2S1320 < _M0L5_2aloS1313) {
    _M0L6_2atmpS3586 = 1ull;
  } else {
    _M0L6_2atmpS3586 = 0ull;
  }
  _M0L4mid2S1321 = _M0L6_2atmpS3585 + _M0L6_2atmpS3586;
  if (_M0L4mid2S1321 < _M0L3midS1318) {
    _M0L6_2atmpS3584 = 1ull;
  } else {
    _M0L6_2atmpS3584 = 0ull;
  }
  _M0L3hi2S1322 = _M0L2hiS1319 + _M0L6_2atmpS3584;
  _M0L6_2atmpS3583 = _M0L1jS1324 - 64;
  _M0L6_2atmpS3582 = _M0L6_2atmpS3583 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1323
  = _M0FPB13shiftright128(_M0L4mid2S1321, _M0L3hi2S1322, _M0L6_2atmpS3582);
  _M0Lm2vmS1325 = 0ull;
  if (_M0L7mmShiftS1326) {
    uint64_t _M0L3lo3S1327 = _M0L5_2aloS1313 - _M0L7_2amul0S1307;
    uint64_t _M0L6_2atmpS3569 = _M0L3midS1318 - _M0L7_2amul1S1309;
    uint64_t _M0L6_2atmpS3570;
    uint64_t _M0L4mid3S1328;
    uint64_t _M0L6_2atmpS3568;
    uint64_t _M0L3hi3S1329;
    int32_t _M0L6_2atmpS3567;
    int32_t _M0L6_2atmpS3566;
    if (_M0L5_2aloS1313 < _M0L3lo3S1327) {
      _M0L6_2atmpS3570 = 1ull;
    } else {
      _M0L6_2atmpS3570 = 0ull;
    }
    _M0L4mid3S1328 = _M0L6_2atmpS3569 - _M0L6_2atmpS3570;
    if (_M0L3midS1318 < _M0L4mid3S1328) {
      _M0L6_2atmpS3568 = 1ull;
    } else {
      _M0L6_2atmpS3568 = 0ull;
    }
    _M0L3hi3S1329 = _M0L2hiS1319 - _M0L6_2atmpS3568;
    _M0L6_2atmpS3567 = _M0L1jS1324 - 64;
    _M0L6_2atmpS3566 = _M0L6_2atmpS3567 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1325
    = _M0FPB13shiftright128(_M0L4mid3S1328, _M0L3hi3S1329, _M0L6_2atmpS3566);
  } else {
    uint64_t _M0L3lo3S1330 = _M0L5_2aloS1313 + _M0L5_2aloS1313;
    uint64_t _M0L6_2atmpS3577 = _M0L3midS1318 + _M0L3midS1318;
    uint64_t _M0L6_2atmpS3578;
    uint64_t _M0L4mid3S1331;
    uint64_t _M0L6_2atmpS3575;
    uint64_t _M0L6_2atmpS3576;
    uint64_t _M0L3hi3S1332;
    uint64_t _M0L3lo4S1333;
    uint64_t _M0L6_2atmpS3573;
    uint64_t _M0L6_2atmpS3574;
    uint64_t _M0L4mid4S1334;
    uint64_t _M0L6_2atmpS3572;
    uint64_t _M0L3hi4S1335;
    int32_t _M0L6_2atmpS3571;
    if (_M0L3lo3S1330 < _M0L5_2aloS1313) {
      _M0L6_2atmpS3578 = 1ull;
    } else {
      _M0L6_2atmpS3578 = 0ull;
    }
    _M0L4mid3S1331 = _M0L6_2atmpS3577 + _M0L6_2atmpS3578;
    _M0L6_2atmpS3575 = _M0L2hiS1319 + _M0L2hiS1319;
    if (_M0L4mid3S1331 < _M0L3midS1318) {
      _M0L6_2atmpS3576 = 1ull;
    } else {
      _M0L6_2atmpS3576 = 0ull;
    }
    _M0L3hi3S1332 = _M0L6_2atmpS3575 + _M0L6_2atmpS3576;
    _M0L3lo4S1333 = _M0L3lo3S1330 - _M0L7_2amul0S1307;
    _M0L6_2atmpS3573 = _M0L4mid3S1331 - _M0L7_2amul1S1309;
    if (_M0L3lo3S1330 < _M0L3lo4S1333) {
      _M0L6_2atmpS3574 = 1ull;
    } else {
      _M0L6_2atmpS3574 = 0ull;
    }
    _M0L4mid4S1334 = _M0L6_2atmpS3573 - _M0L6_2atmpS3574;
    if (_M0L4mid3S1331 < _M0L4mid4S1334) {
      _M0L6_2atmpS3572 = 1ull;
    } else {
      _M0L6_2atmpS3572 = 0ull;
    }
    _M0L3hi4S1335 = _M0L3hi3S1332 - _M0L6_2atmpS3572;
    _M0L6_2atmpS3571 = _M0L1jS1324 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1325
    = _M0FPB13shiftright128(_M0L4mid4S1334, _M0L3hi4S1335, _M0L6_2atmpS3571);
  }
  _M0L6_2atmpS3581 = _M0L1jS1324 - 64;
  _M0L6_2atmpS3580 = _M0L6_2atmpS3581 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1336
  = _M0FPB13shiftright128(_M0L3midS1318, _M0L2hiS1319, _M0L6_2atmpS3580);
  _M0L6_2atmpS3579 = _M0Lm2vmS1325;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1336,
                                                .$1 = _M0L2vpS1323,
                                                .$2 = _M0L6_2atmpS3579};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1305,
  int32_t _M0L1pS1306
) {
  uint64_t _M0L6_2atmpS3565;
  uint64_t _M0L6_2atmpS3564;
  uint64_t _M0L6_2atmpS3563;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3565 = 1ull << (_M0L1pS1306 & 63);
  _M0L6_2atmpS3564 = _M0L6_2atmpS3565 - 1ull;
  _M0L6_2atmpS3563 = _M0L5valueS1305 & _M0L6_2atmpS3564;
  return _M0L6_2atmpS3563 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1303,
  int32_t _M0L1pS1304
) {
  int32_t _M0L6_2atmpS3562;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3562 = _M0FPB10pow5Factor(_M0L5valueS1303);
  return _M0L6_2atmpS3562 >= _M0L1pS1304;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1298) {
  uint64_t _M0L6_2atmpS3553;
  uint64_t _M0L6_2atmpS3554;
  uint64_t _M0L6_2atmpS3555;
  uint64_t _M0L6_2atmpS3556;
  uint64_t _M0L6_2atmpS3561;
  int32_t _M0L5countS1299;
  uint64_t _M0L1vS1300;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3553 = _M0L5valueS1298 % 5ull;
  if (_M0L6_2atmpS3553 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3554 = _M0L5valueS1298 % 25ull;
  if (_M0L6_2atmpS3554 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS3555 = _M0L5valueS1298 % 125ull;
  if (_M0L6_2atmpS3555 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS3556 = _M0L5valueS1298 % 625ull;
  if (_M0L6_2atmpS3556 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS3561 = _M0L5valueS1298 / 625ull;
  _M0L5countS1299 = 4;
  _M0L1vS1300 = _M0L6_2atmpS3561;
  while (1) {
    if (_M0L1vS1300 > 0ull) {
      uint64_t _M0L6_2atmpS3557 = _M0L1vS1300 % 5ull;
      int32_t _M0L6_2atmpS3558;
      uint64_t _M0L6_2atmpS3559;
      if (_M0L6_2atmpS3557 != 0ull) {
        return _M0L5countS1299;
      }
      _M0L6_2atmpS3558 = _M0L5countS1299 + 1;
      _M0L6_2atmpS3559 = _M0L1vS1300 / 5ull;
      _M0L5countS1299 = _M0L6_2atmpS3558;
      _M0L1vS1300 = _M0L6_2atmpS3559;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1302;
      moonbit_string_t _M0L6_2atmpS3560;
      int32_t _result_5402;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1302
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1302, (moonbit_string_t)moonbit_string_literal_101.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1302, _M0L5valueS1298);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS3560
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1302);
      moonbit_decref(_M0L18_2astring__builderS1302);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_5402 = _M0FPC15abort5abortGiE(_M0L6_2atmpS3560);
      moonbit_decref(_M0L6_2atmpS3560);
      return _result_5402;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1297,
  uint64_t _M0L2hiS1295,
  int32_t _M0L4distS1296
) {
  int32_t _M0L6_2atmpS3552;
  uint64_t _M0L6_2atmpS3550;
  uint64_t _M0L6_2atmpS3551;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3552 = 64 - _M0L4distS1296;
  _M0L6_2atmpS3550 = _M0L2hiS1295 << (_M0L6_2atmpS3552 & 63);
  _M0L6_2atmpS3551 = _M0L2loS1297 >> (_M0L4distS1296 & 63);
  return _M0L6_2atmpS3550 | _M0L6_2atmpS3551;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS1285,
  uint64_t _M0L1bS1288
) {
  uint64_t _M0L3aLoS1284;
  uint64_t _M0L3aHiS1286;
  uint64_t _M0L3bLoS1287;
  uint64_t _M0L3bHiS1289;
  uint64_t _M0L1xS1290;
  uint64_t _M0L6_2atmpS3548;
  uint64_t _M0L6_2atmpS3549;
  uint64_t _M0L1yS1291;
  uint64_t _M0L6_2atmpS3546;
  uint64_t _M0L6_2atmpS3547;
  uint64_t _M0L1zS1292;
  uint64_t _M0L6_2atmpS3544;
  uint64_t _M0L6_2atmpS3545;
  uint64_t _M0L6_2atmpS3542;
  uint64_t _M0L6_2atmpS3543;
  uint64_t _M0L1wS1293;
  uint64_t _M0L2loS1294;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1284 = _M0L1aS1285 & 4294967295ull;
  _M0L3aHiS1286 = _M0L1aS1285 >> 32;
  _M0L3bLoS1287 = _M0L1bS1288 & 4294967295ull;
  _M0L3bHiS1289 = _M0L1bS1288 >> 32;
  _M0L1xS1290 = _M0L3aLoS1284 * _M0L3bLoS1287;
  _M0L6_2atmpS3548 = _M0L3aHiS1286 * _M0L3bLoS1287;
  _M0L6_2atmpS3549 = _M0L1xS1290 >> 32;
  _M0L1yS1291 = _M0L6_2atmpS3548 + _M0L6_2atmpS3549;
  _M0L6_2atmpS3546 = _M0L3aLoS1284 * _M0L3bHiS1289;
  _M0L6_2atmpS3547 = _M0L1yS1291 & 4294967295ull;
  _M0L1zS1292 = _M0L6_2atmpS3546 + _M0L6_2atmpS3547;
  _M0L6_2atmpS3544 = _M0L3aHiS1286 * _M0L3bHiS1289;
  _M0L6_2atmpS3545 = _M0L1yS1291 >> 32;
  _M0L6_2atmpS3542 = _M0L6_2atmpS3544 + _M0L6_2atmpS3545;
  _M0L6_2atmpS3543 = _M0L1zS1292 >> 32;
  _M0L1wS1293 = _M0L6_2atmpS3542 + _M0L6_2atmpS3543;
  _M0L2loS1294 = _M0L1aS1285 * _M0L1bS1288;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1294, .$1 = _M0L1wS1293};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1282,
  int32_t _M0L4fromS1279,
  int32_t _M0L2toS1278
) {
  int32_t _M0L3lenS1277;
  int32_t _M0L6_2atmpS3541;
  uint16_t* _M0L6bufferS1280;
  int32_t _M0L1iS1281;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1277 = _M0L2toS1278 - _M0L4fromS1279;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3541 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1280
  = (uint16_t*)moonbit_make_string(_M0L3lenS1277, _M0L6_2atmpS3541);
  _M0L1iS1281 = 0;
  while (1) {
    if (_M0L1iS1281 < _M0L3lenS1277) {
      int32_t _M0L6_2atmpS3539 = _M0L4fromS1279 + _M0L1iS1281;
      int32_t _M0L6_2atmpS3538;
      int32_t _M0L6_2atmpS3537;
      int32_t _M0L6_2atmpS3540;
      if (
        _M0L6_2atmpS3539 < 0
        || _M0L6_2atmpS3539 >= Moonbit_array_length(_M0L5bytesS1282)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3538 = (int32_t)_M0L5bytesS1282[_M0L6_2atmpS3539];
      _M0L6_2atmpS3537 = (uint16_t)_M0L6_2atmpS3538;
      if (
        _M0L1iS1281 < 0
        || _M0L1iS1281 >= Moonbit_array_length(_M0L6bufferS1280)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1280[_M0L1iS1281] = _M0L6_2atmpS3537;
      _M0L6_2atmpS3540 = _M0L1iS1281 + 1;
      _M0L1iS1281 = _M0L6_2atmpS3540;
      continue;
    }
    break;
  }
  return _M0L6bufferS1280;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1276) {
  int32_t _M0L6_2atmpS3536;
  uint32_t _M0L6_2atmpS3535;
  uint32_t _M0L6_2atmpS3534;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3536 = _M0L1eS1276 * 78913;
  _M0L6_2atmpS3535 = *(uint32_t*)&_M0L6_2atmpS3536;
  _M0L6_2atmpS3534 = _M0L6_2atmpS3535 >> 18;
  return *(int32_t*)&_M0L6_2atmpS3534;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1275) {
  int32_t _M0L6_2atmpS3533;
  uint32_t _M0L6_2atmpS3532;
  uint32_t _M0L6_2atmpS3531;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3533 = _M0L1eS1275 * 732923;
  _M0L6_2atmpS3532 = *(uint32_t*)&_M0L6_2atmpS3533;
  _M0L6_2atmpS3531 = _M0L6_2atmpS3532 >> 20;
  return *(int32_t*)&_M0L6_2atmpS3531;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1273,
  int32_t _M0L8exponentS1274,
  int32_t _M0L8mantissaS1271
) {
  moonbit_string_t _M0L1sS1272;
  moonbit_string_t _result_5405;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1271) {
    return (moonbit_string_t)moonbit_string_literal_102.data;
  }
  if (_M0L4signS1273) {
    _M0L1sS1272 = (moonbit_string_t)moonbit_string_literal_94.data;
  } else {
    _M0L1sS1272 = (moonbit_string_t)moonbit_string_literal_75.data;
  }
  if (_M0L8exponentS1274) {
    moonbit_string_t _result_5404;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_5404
    = moonbit_add_string(_M0L1sS1272, (moonbit_string_t)moonbit_string_literal_103.data);
    moonbit_decref(_M0L1sS1272);
    return _result_5404;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_5405
  = moonbit_add_string(_M0L1sS1272, (moonbit_string_t)moonbit_string_literal_104.data);
  moonbit_decref(_M0L1sS1272);
  return _result_5405;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1270) {
  int32_t _M0L6_2atmpS3530;
  uint32_t _M0L6_2atmpS3529;
  uint32_t _M0L6_2atmpS3528;
  int32_t _M0L6_2atmpS3527;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3530 = _M0L1eS1270 * 1217359;
  _M0L6_2atmpS3529 = *(uint32_t*)&_M0L6_2atmpS3530;
  _M0L6_2atmpS3528 = _M0L6_2atmpS3529 >> 19;
  _M0L6_2atmpS3527 = *(int32_t*)&_M0L6_2atmpS3528;
  return _M0L6_2atmpS3527 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1266) {
  int32_t _tmp_5406;
  uint32_t _M0L6_2atmpS3526;
  uint32_t _M0Lm3accS1264;
  int32_t _M0L7_2abindS1265;
  int32_t _M0L1iS1267;
  uint32_t _M0L6_2atmpS3525;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_5406 = 0;
  _M0L6_2atmpS3526 = *(uint32_t*)&_tmp_5406;
  _M0Lm3accS1264 = _M0L6_2atmpS3526 + 374761393u;
  _M0L7_2abindS1265 = Moonbit_array_length(_M0L4selfS1266);
  _M0L1iS1267 = 0;
  while (1) {
    if (_M0L1iS1267 < _M0L7_2abindS1265) {
      uint32_t _M0L6_2atmpS3520 = _M0Lm3accS1264;
      int32_t _M0L6_2atmpS3523;
      int32_t _M0L6_2atmpS3522;
      uint32_t _M0L1vS1268;
      uint32_t _M0L6_2atmpS3521;
      int32_t _M0L6_2atmpS3524;
      _M0Lm3accS1264 = _M0L6_2atmpS3520 + 4u;
      _M0L6_2atmpS3523 = _M0L4selfS1266[_M0L1iS1267];
      _M0L6_2atmpS3522 = (int32_t)_M0L6_2atmpS3523;
      _M0L1vS1268 = *(uint32_t*)&_M0L6_2atmpS3522;
      _M0L6_2atmpS3521 = _M0Lm3accS1264;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1264 = _M0FPB13consume4__acc(_M0L6_2atmpS3521, _M0L1vS1268);
      _M0L6_2atmpS3524 = _M0L1iS1267 + 1;
      _M0L1iS1267 = _M0L6_2atmpS3524;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3525 = _M0Lm3accS1264;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS3525);
}

struct _M0TUssE* _M0MPB5Iter24nextGssE(
  struct _M0TPB4IterGUssEE* _M0L4selfS1259
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUssEE(_M0L4selfS1259);
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L4selfS1260
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L4selfS1260);
}

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS1261
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsbEE(_M0L4selfS1261);
}

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS1262
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsfEE(_M0L4selfS1262);
}

struct _M0TUsiE* _M0MPB5Iter24nextGsiE(
  struct _M0TPB4IterGUsiEE* _M0L4selfS1263
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsiEE(_M0L4selfS1263);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(
  struct _M0TPB3MapGssE* _M0L4selfS1254
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGssE(_M0L4selfS1254);
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1255
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS1255);
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1256
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsbE(_M0L4selfS1256);
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1257
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsfE(_M0L4selfS1257);
}

struct _M0TPB4IterGUsiEE* _M0MPB3Map5iter2GsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1258
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsiE(_M0L4selfS1258);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1200
) {
  struct _M0TPB5EntryGssE* _M0L4headS3479;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS1199;
  int32_t _M0L3lenS1201;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1202;
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__* _closure_5408;
  struct _M0TWEOUssE* _M0L6_2atmpS3470;
  int64_t _M0L6_2atmpS3471;
  struct _M0TPB4IterGUssEE* _result_5409;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3479 = _M0L4selfS1200->$5;
  if (_M0L4headS3479) {
    moonbit_incref(_M0L4headS3479);
  }
  _M0L11curr__entryS1199
  = (struct _M0TPB8MutLocalGORPB5EntryGssEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE));
  Moonbit_object_header(_M0L11curr__entryS1199)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 35, 0);
  _M0L11curr__entryS1199->$0 = _M0L4headS3479;
  _M0L3lenS1201 = _M0L4selfS1200->$1;
  _M0L9remainingS1202
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1202)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1202->$0 = _M0L3lenS1201;
  _closure_5408
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__*)moonbit_malloc(sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__));
  Moonbit_object_header(_closure_5408)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 38, 0);
  _closure_5408->code = &_M0MPB3Map4iterGssEC3472l711;
  _closure_5408->$0 = _M0L9remainingS1202;
  _closure_5408->$1 = _M0L11curr__entryS1199;
  _M0L6_2atmpS3470 = (struct _M0TWEOUssE*)_closure_5408;
  _M0L6_2atmpS3471 = (int64_t)_M0L3lenS1201;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5409 = _M0MPB4Iter3newGUssEE(_M0L6_2atmpS3470, _M0L6_2atmpS3471);
  moonbit_decref(_M0L6_2atmpS3470);
  return _result_5409;
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1211
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4headS3489;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L11curr__entryS1210;
  int32_t _M0L3lenS1212;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1213;
  struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__* _closure_5410;
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3480;
  int64_t _M0L6_2atmpS3481;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _result_5411;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3489 = _M0L4selfS1211->$5;
  if (_M0L4headS3489) {
    moonbit_incref(_M0L4headS3489);
  }
  _M0L11curr__entryS1210
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS1210)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 42, 0);
  _M0L11curr__entryS1210->$0 = _M0L4headS3489;
  _M0L3lenS1212 = _M0L4selfS1211->$1;
  _M0L9remainingS1213
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1213)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1213->$0 = _M0L3lenS1212;
  _closure_5410
  = (struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__*)moonbit_malloc(sizeof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__));
  Moonbit_object_header(_closure_5410)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 45, 0);
  _closure_5410->code
  = &_M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC3482l711;
  _closure_5410->$0 = _M0L9remainingS1213;
  _closure_5410->$1 = _M0L11curr__entryS1210;
  _M0L6_2atmpS3480
  = (struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_closure_5410;
  _M0L6_2atmpS3481 = (int64_t)_M0L3lenS1212;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5411
  = _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS3480, _M0L6_2atmpS3481);
  moonbit_decref(_M0L6_2atmpS3480);
  return _result_5411;
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1222
) {
  struct _M0TPB5EntryGsbE* _M0L4headS3499;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS1221;
  int32_t _M0L3lenS1223;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1224;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__* _closure_5412;
  struct _M0TWEOUsbE* _M0L6_2atmpS3490;
  int64_t _M0L6_2atmpS3491;
  struct _M0TPB4IterGUsbEE* _result_5413;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3499 = _M0L4selfS1222->$5;
  if (_M0L4headS3499) {
    moonbit_incref(_M0L4headS3499);
  }
  _M0L11curr__entryS1221
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS1221)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 49, 0);
  _M0L11curr__entryS1221->$0 = _M0L4headS3499;
  _M0L3lenS1223 = _M0L4selfS1222->$1;
  _M0L9remainingS1224
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1224)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1224->$0 = _M0L3lenS1223;
  _closure_5412
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__));
  Moonbit_object_header(_closure_5412)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 52, 0);
  _closure_5412->code = &_M0MPB3Map4iterGsbEC3492l711;
  _closure_5412->$0 = _M0L9remainingS1224;
  _closure_5412->$1 = _M0L11curr__entryS1221;
  _M0L6_2atmpS3490 = (struct _M0TWEOUsbE*)_closure_5412;
  _M0L6_2atmpS3491 = (int64_t)_M0L3lenS1223;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5413 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS3490, _M0L6_2atmpS3491);
  moonbit_decref(_M0L6_2atmpS3490);
  return _result_5413;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1233
) {
  struct _M0TPB5EntryGsfE* _M0L4headS3509;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1232;
  int32_t _M0L3lenS1234;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1235;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__* _closure_5414;
  struct _M0TWEOUsfE* _M0L6_2atmpS3500;
  int64_t _M0L6_2atmpS3501;
  struct _M0TPB4IterGUsfEE* _result_5415;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3509 = _M0L4selfS1233->$5;
  if (_M0L4headS3509) {
    moonbit_incref(_M0L4headS3509);
  }
  _M0L11curr__entryS1232
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS1232)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 56, 0);
  _M0L11curr__entryS1232->$0 = _M0L4headS3509;
  _M0L3lenS1234 = _M0L4selfS1233->$1;
  _M0L9remainingS1235
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1235)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1235->$0 = _M0L3lenS1234;
  _closure_5414
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__));
  Moonbit_object_header(_closure_5414)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 59, 0);
  _closure_5414->code = &_M0MPB3Map4iterGsfEC3502l711;
  _closure_5414->$0 = _M0L9remainingS1235;
  _closure_5414->$1 = _M0L11curr__entryS1232;
  _M0L6_2atmpS3500 = (struct _M0TWEOUsfE*)_closure_5414;
  _M0L6_2atmpS3501 = (int64_t)_M0L3lenS1234;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5415 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS3500, _M0L6_2atmpS3501);
  moonbit_decref(_M0L6_2atmpS3500);
  return _result_5415;
}

struct _M0TPB4IterGUsiEE* _M0MPB3Map4iterGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1244
) {
  struct _M0TPB5EntryGsiE* _M0L4headS3519;
  struct _M0TPB8MutLocalGORPB5EntryGsiEE* _M0L11curr__entryS1243;
  int32_t _M0L3lenS1245;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1246;
  struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__* _closure_5416;
  struct _M0TWEOUsiE* _M0L6_2atmpS3510;
  int64_t _M0L6_2atmpS3511;
  struct _M0TPB4IterGUsiEE* _result_5417;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3519 = _M0L4selfS1244->$5;
  if (_M0L4headS3519) {
    moonbit_incref(_M0L4headS3519);
  }
  _M0L11curr__entryS1243
  = (struct _M0TPB8MutLocalGORPB5EntryGsiEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsiEE));
  Moonbit_object_header(_M0L11curr__entryS1243)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 63, 0);
  _M0L11curr__entryS1243->$0 = _M0L4headS3519;
  _M0L3lenS1245 = _M0L4selfS1244->$1;
  _M0L9remainingS1246
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1246)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1246->$0 = _M0L3lenS1245;
  _closure_5416
  = (struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__*)moonbit_malloc(sizeof(struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__));
  Moonbit_object_header(_closure_5416)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 66, 0);
  _closure_5416->code = &_M0MPB3Map4iterGsiEC3512l711;
  _closure_5416->$0 = _M0L9remainingS1246;
  _closure_5416->$1 = _M0L11curr__entryS1243;
  _M0L6_2atmpS3510 = (struct _M0TWEOUsiE*)_closure_5416;
  _M0L6_2atmpS3511 = (int64_t)_M0L3lenS1245;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5417 = _M0MPB4Iter3newGUsiEE(_M0L6_2atmpS3510, _M0L6_2atmpS3511);
  moonbit_decref(_M0L6_2atmpS3510);
  return _result_5417;
}

struct _M0TUsiE* _M0MPB3Map4iterGsiEC3512l711(
  struct _M0TWEOUsiE* _M0L6_2aenvS3513
) {
  struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__* _M0L14_2acasted__envS3514;
  struct _M0TPB8MutLocalGORPB5EntryGsiEE* _M0L11curr__entryS1243;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1246;
  int32_t _M0L3valS3515;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3514
  = (struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3512__l711__*)_M0L6_2aenvS3513;
  _M0L11curr__entryS1243 = _M0L14_2acasted__envS3514->$1;
  _M0L9remainingS1246 = _M0L14_2acasted__envS3514->$0;
  _M0L3valS3515 = _M0L9remainingS1246->$0;
  if (_M0L3valS3515 > 0) {
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1248 = _M0L11curr__entryS1243->$0;
    if (_M0L7_2abindS1248 == 0) {
      goto join_1247;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1249 = _M0L7_2abindS1248;
      struct _M0TPB5EntryGsiE* _M0L4_2axS1250 = _M0L7_2aSomeS1249;
      moonbit_string_t _M0L6_2akeyS1251 = _M0L4_2axS1250->$4;
      int32_t _M0L8_2avalueS1252 = _M0L4_2axS1250->$5;
      struct _M0TPB5EntryGsiE* _M0L7_2anextS1253 = _M0L4_2axS1250->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4710 = _M0L11curr__entryS1243->$0;
      int32_t _M0L3valS3517;
      int32_t _M0L6_2atmpS3516;
      struct _M0TUsiE* _M0L8_2atupleS3518;
      if (_M0L7_2anextS1253) {
        moonbit_incref(_M0L7_2anextS1253);
      }
      moonbit_incref(_M0L6_2akeyS1251);
      if (_M0L6_2aoldS4710) {
        moonbit_decref(_M0L6_2aoldS4710);
      }
      _M0L11curr__entryS1243->$0 = _M0L7_2anextS1253;
      _M0L3valS3517 = _M0L9remainingS1246->$0;
      _M0L6_2atmpS3516 = _M0L3valS3517 - 1;
      _M0L9remainingS1246->$0 = _M0L6_2atmpS3516;
      _M0L8_2atupleS3518
      = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
      Moonbit_object_header(_M0L8_2atupleS3518)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 70, 0);
      _M0L8_2atupleS3518->$0 = _M0L6_2akeyS1251;
      _M0L8_2atupleS3518->$1 = _M0L8_2avalueS1252;
      return _M0L8_2atupleS3518;
    }
  } else {
    goto join_1247;
  }
  join_1247:;
  return 0;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC3502l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS3503
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__* _M0L14_2acasted__envS3504;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1232;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1235;
  int32_t _M0L3valS3505;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3504
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3502__l711__*)_M0L6_2aenvS3503;
  _M0L11curr__entryS1232 = _M0L14_2acasted__envS3504->$1;
  _M0L9remainingS1235 = _M0L14_2acasted__envS3504->$0;
  _M0L3valS3505 = _M0L9remainingS1235->$0;
  if (_M0L3valS3505 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1237 = _M0L11curr__entryS1232->$0;
    if (_M0L7_2abindS1237 == 0) {
      goto join_1236;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1238 = _M0L7_2abindS1237;
      struct _M0TPB5EntryGsfE* _M0L4_2axS1239 = _M0L7_2aSomeS1238;
      moonbit_string_t _M0L6_2akeyS1240 = _M0L4_2axS1239->$4;
      float _M0L8_2avalueS1241 = _M0L4_2axS1239->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1242 = _M0L4_2axS1239->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4714 = _M0L11curr__entryS1232->$0;
      int32_t _M0L3valS3507;
      int32_t _M0L6_2atmpS3506;
      struct _M0TUsfE* _M0L8_2atupleS3508;
      if (_M0L7_2anextS1242) {
        moonbit_incref(_M0L7_2anextS1242);
      }
      moonbit_incref(_M0L6_2akeyS1240);
      if (_M0L6_2aoldS4714) {
        moonbit_decref(_M0L6_2aoldS4714);
      }
      _M0L11curr__entryS1232->$0 = _M0L7_2anextS1242;
      _M0L3valS3507 = _M0L9remainingS1235->$0;
      _M0L6_2atmpS3506 = _M0L3valS3507 - 1;
      _M0L9remainingS1235->$0 = _M0L6_2atmpS3506;
      _M0L8_2atupleS3508
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3508)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 0);
      _M0L8_2atupleS3508->$0 = _M0L6_2akeyS1240;
      _M0L8_2atupleS3508->$1 = _M0L8_2avalueS1241;
      return _M0L8_2atupleS3508;
    }
  } else {
    goto join_1236;
  }
  join_1236:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC3492l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS3493
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__* _M0L14_2acasted__envS3494;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS1221;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1224;
  int32_t _M0L3valS3495;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3494
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3492__l711__*)_M0L6_2aenvS3493;
  _M0L11curr__entryS1221 = _M0L14_2acasted__envS3494->$1;
  _M0L9remainingS1224 = _M0L14_2acasted__envS3494->$0;
  _M0L3valS3495 = _M0L9remainingS1224->$0;
  if (_M0L3valS3495 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1226 = _M0L11curr__entryS1221->$0;
    if (_M0L7_2abindS1226 == 0) {
      goto join_1225;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1227 = _M0L7_2abindS1226;
      struct _M0TPB5EntryGsbE* _M0L4_2axS1228 = _M0L7_2aSomeS1227;
      moonbit_string_t _M0L6_2akeyS1229 = _M0L4_2axS1228->$4;
      int32_t _M0L8_2avalueS1230 = _M0L4_2axS1228->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS1231 = _M0L4_2axS1228->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4718 = _M0L11curr__entryS1221->$0;
      int32_t _M0L3valS3497;
      int32_t _M0L6_2atmpS3496;
      struct _M0TUsbE* _M0L8_2atupleS3498;
      if (_M0L7_2anextS1231) {
        moonbit_incref(_M0L7_2anextS1231);
      }
      moonbit_incref(_M0L6_2akeyS1229);
      if (_M0L6_2aoldS4718) {
        moonbit_decref(_M0L6_2aoldS4718);
      }
      _M0L11curr__entryS1221->$0 = _M0L7_2anextS1231;
      _M0L3valS3497 = _M0L9remainingS1224->$0;
      _M0L6_2atmpS3496 = _M0L3valS3497 - 1;
      _M0L9remainingS1224->$0 = _M0L6_2atmpS3496;
      _M0L8_2atupleS3498
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS3498)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 73, 0);
      _M0L8_2atupleS3498->$0 = _M0L6_2akeyS1229;
      _M0L8_2atupleS3498->$1 = _M0L8_2avalueS1230;
      return _M0L8_2atupleS3498;
    }
  } else {
    goto join_1225;
  }
  join_1225:;
  return 0;
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC3482l711(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aenvS3483
) {
  struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__* _M0L14_2acasted__envS3484;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L11curr__entryS1210;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1213;
  int32_t _M0L3valS3485;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3484
  = (struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u3482__l711__*)_M0L6_2aenvS3483;
  _M0L11curr__entryS1210 = _M0L14_2acasted__envS3484->$1;
  _M0L9remainingS1213 = _M0L14_2acasted__envS3484->$0;
  _M0L3valS3485 = _M0L9remainingS1213->$0;
  if (_M0L3valS3485 > 0) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1215 =
      _M0L11curr__entryS1210->$0;
    if (_M0L7_2abindS1215 == 0) {
      goto join_1214;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1216 =
        _M0L7_2abindS1215;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1217 =
        _M0L7_2aSomeS1216;
      moonbit_string_t _M0L6_2akeyS1218 = _M0L4_2axS1217->$4;
      void* _M0L8_2avalueS1219 = _M0L4_2axS1217->$5;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS1220 =
        _M0L4_2axS1217->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4722 =
        _M0L11curr__entryS1210->$0;
      int32_t _M0L3valS3487;
      int32_t _M0L6_2atmpS3486;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2atupleS3488;
      if (_M0L7_2anextS1220) {
        moonbit_incref(_M0L7_2anextS1220);
      }
      moonbit_incref(_M0L8_2avalueS1219);
      moonbit_incref(_M0L6_2akeyS1218);
      if (_M0L6_2aoldS4722) {
        moonbit_decref(_M0L6_2aoldS4722);
      }
      _M0L11curr__entryS1210->$0 = _M0L7_2anextS1220;
      _M0L3valS3487 = _M0L9remainingS1213->$0;
      _M0L6_2atmpS3486 = _M0L3valS3487 - 1;
      _M0L9remainingS1213->$0 = _M0L6_2atmpS3486;
      _M0L8_2atupleS3488
      = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS3488)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 76, 0);
      _M0L8_2atupleS3488->$0 = _M0L6_2akeyS1218;
      _M0L8_2atupleS3488->$1 = _M0L8_2avalueS1219;
      return _M0L8_2atupleS3488;
    }
  } else {
    goto join_1214;
  }
  join_1214:;
  return 0;
}

struct _M0TUssE* _M0MPB3Map4iterGssEC3472l711(
  struct _M0TWEOUssE* _M0L6_2aenvS3473
) {
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__* _M0L14_2acasted__envS3474;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS1199;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1202;
  int32_t _M0L3valS3475;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3474
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3472__l711__*)_M0L6_2aenvS3473;
  _M0L11curr__entryS1199 = _M0L14_2acasted__envS3474->$1;
  _M0L9remainingS1202 = _M0L14_2acasted__envS3474->$0;
  _M0L3valS3475 = _M0L9remainingS1202->$0;
  if (_M0L3valS3475 > 0) {
    struct _M0TPB5EntryGssE* _M0L7_2abindS1204 = _M0L11curr__entryS1199->$0;
    if (_M0L7_2abindS1204 == 0) {
      goto join_1203;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1205 = _M0L7_2abindS1204;
      struct _M0TPB5EntryGssE* _M0L4_2axS1206 = _M0L7_2aSomeS1205;
      moonbit_string_t _M0L6_2akeyS1207 = _M0L4_2axS1206->$4;
      moonbit_string_t _M0L8_2avalueS1208 = _M0L4_2axS1206->$5;
      struct _M0TPB5EntryGssE* _M0L7_2anextS1209 = _M0L4_2axS1206->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4727 = _M0L11curr__entryS1199->$0;
      int32_t _M0L3valS3477;
      int32_t _M0L6_2atmpS3476;
      struct _M0TUssE* _M0L8_2atupleS3478;
      if (_M0L7_2anextS1209) {
        moonbit_incref(_M0L7_2anextS1209);
      }
      moonbit_incref(_M0L8_2avalueS1208);
      moonbit_incref(_M0L6_2akeyS1207);
      if (_M0L6_2aoldS4727) {
        moonbit_decref(_M0L6_2aoldS4727);
      }
      _M0L11curr__entryS1199->$0 = _M0L7_2anextS1209;
      _M0L3valS3477 = _M0L9remainingS1202->$0;
      _M0L6_2atmpS3476 = _M0L3valS3477 - 1;
      _M0L9remainingS1202->$0 = _M0L6_2atmpS3476;
      _M0L8_2atupleS3478
      = (struct _M0TUssE*)moonbit_malloc(sizeof(struct _M0TUssE));
      Moonbit_object_header(_M0L8_2atupleS3478)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 80, 0);
      _M0L8_2atupleS3478->$0 = _M0L6_2akeyS1207;
      _M0L8_2atupleS3478->$1 = _M0L8_2avalueS1208;
      return _M0L8_2atupleS3478;
    }
  } else {
    goto join_1203;
  }
  join_1203:;
  return 0;
}

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE* _M0L4selfS1196) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1196->$1;
}

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE* _M0L4selfS1197) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1197->$1;
}

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE* _M0L4selfS1198) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1198->$1;
}

int32_t _M0MPB3Map6removeGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1188,
  moonbit_string_t _M0L3keyS1189
) {
  int32_t _M0L6_2atmpS3466;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3466 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1189);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS1188, _M0L3keyS1189, _M0L6_2atmpS3466);
  return 0;
}

int32_t _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1190,
  moonbit_string_t _M0L3keyS1191
) {
  int32_t _M0L6_2atmpS3467;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3467 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1191);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS1190, _M0L3keyS1191, _M0L6_2atmpS3467);
  return 0;
}

int32_t _M0MPB3Map6removeGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1192,
  moonbit_string_t _M0L3keyS1193
) {
  int32_t _M0L6_2atmpS3468;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3468 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1193);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGssE(_M0L4selfS1192, _M0L3keyS1193, _M0L6_2atmpS3468);
  return 0;
}

int32_t _M0MPB3Map6removeGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1194,
  moonbit_string_t _M0L3keyS1195
) {
  int32_t _M0L6_2atmpS3469;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3469 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1195);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsbE(_M0L4selfS1194, _M0L3keyS1195, _M0L6_2atmpS3469);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1155,
  moonbit_string_t _M0L3keyS1159,
  int32_t _M0L4hashS1158
) {
  int32_t _M0L14capacity__maskS3429;
  int32_t _M0L6_2atmpS3428;
  int32_t _M0L1iS1152;
  int32_t _M0L3idxS1153;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3429 = _M0L4selfS1155->$3;
  _M0L6_2atmpS3428 = _M0L4hashS1158 & _M0L14capacity__maskS3429;
  _M0L1iS1152 = 0;
  _M0L3idxS1153 = _M0L6_2atmpS3428;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3427 = _M0L4selfS1155->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1154;
    if (
      _M0L3idxS1153 < 0
      || _M0L3idxS1153 >= Moonbit_array_length(_M0L7entriesS3427)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1154
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3427[_M0L3idxS1153];
    if (_M0L7_2abindS1154 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1156 = _M0L7_2abindS1154;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS1157 = _M0L7_2aSomeS1156;
      int32_t _M0L4hashS3419 = _M0L8_2aentryS1157->$3;
      int32_t _if__result_5424;
      int32_t _M0L3pslS3422;
      int32_t _M0L6_2atmpS3423;
      int32_t _M0L6_2atmpS3425;
      int32_t _M0L14capacity__maskS3426;
      int32_t _M0L6_2atmpS3424;
      if (_M0L4hashS3419 == _M0L4hashS1158) {
        moonbit_string_t _M0L3keyS3418 = _M0L8_2aentryS1157->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5424
        = _M0L3keyS3418 == _M0L3keyS1159
          || Moonbit_array_length(_M0L3keyS3418)
             == Moonbit_array_length(_M0L3keyS1159)
             && 0
                == memcmp(_M0L3keyS3418, _M0L3keyS1159, Moonbit_array_length(_M0L3keyS3418) * 2);
      } else {
        _if__result_5424 = 0;
      }
      if (_if__result_5424) {
        int32_t _M0L4sizeS3421;
        int32_t _M0L6_2atmpS3420;
        moonbit_incref(_M0L8_2aentryS1157);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS1155, _M0L8_2aentryS1157);
        moonbit_decref(_M0L8_2aentryS1157);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS1155, _M0L3idxS1153);
        _M0L4sizeS3421 = _M0L4selfS1155->$1;
        _M0L6_2atmpS3420 = _M0L4sizeS3421 - 1;
        _M0L4selfS1155->$1 = _M0L6_2atmpS3420;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1157);
      }
      _M0L3pslS3422 = _M0L8_2aentryS1157->$2;
      moonbit_decref(_M0L8_2aentryS1157);
      if (_M0L1iS1152 > _M0L3pslS3422) {
        break;
      }
      _M0L6_2atmpS3423 = _M0L1iS1152 + 1;
      _M0L6_2atmpS3425 = _M0L3idxS1153 + 1;
      _M0L14capacity__maskS3426 = _M0L4selfS1155->$3;
      _M0L6_2atmpS3424 = _M0L6_2atmpS3425 & _M0L14capacity__maskS3426;
      _M0L1iS1152 = _M0L6_2atmpS3423;
      _M0L3idxS1153 = _M0L6_2atmpS3424;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1164,
  moonbit_string_t _M0L3keyS1168,
  int32_t _M0L4hashS1167
) {
  int32_t _M0L14capacity__maskS3441;
  int32_t _M0L6_2atmpS3440;
  int32_t _M0L1iS1161;
  int32_t _M0L3idxS1162;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3441 = _M0L4selfS1164->$3;
  _M0L6_2atmpS3440 = _M0L4hashS1167 & _M0L14capacity__maskS3441;
  _M0L1iS1161 = 0;
  _M0L3idxS1162 = _M0L6_2atmpS3440;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS3439 =
      _M0L4selfS1164->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1163;
    if (
      _M0L3idxS1162 < 0
      || _M0L3idxS1162 >= Moonbit_array_length(_M0L7entriesS3439)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1163
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS3439[
        _M0L3idxS1162
      ];
    if (_M0L7_2abindS1163 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1165 =
        _M0L7_2abindS1163;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS1166 =
        _M0L7_2aSomeS1165;
      int32_t _M0L4hashS3431 = _M0L8_2aentryS1166->$3;
      int32_t _if__result_5426;
      int32_t _M0L3pslS3434;
      int32_t _M0L6_2atmpS3435;
      int32_t _M0L6_2atmpS3437;
      int32_t _M0L14capacity__maskS3438;
      int32_t _M0L6_2atmpS3436;
      if (_M0L4hashS3431 == _M0L4hashS1167) {
        moonbit_string_t _M0L3keyS3430 = _M0L8_2aentryS1166->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5426
        = _M0L3keyS3430 == _M0L3keyS1168
          || Moonbit_array_length(_M0L3keyS3430)
             == Moonbit_array_length(_M0L3keyS1168)
             && 0
                == memcmp(_M0L3keyS3430, _M0L3keyS1168, Moonbit_array_length(_M0L3keyS3430) * 2);
      } else {
        _if__result_5426 = 0;
      }
      if (_if__result_5426) {
        int32_t _M0L4sizeS3433;
        int32_t _M0L6_2atmpS3432;
        moonbit_incref(_M0L8_2aentryS1166);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS1164, _M0L8_2aentryS1166);
        moonbit_decref(_M0L8_2aentryS1166);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS1164, _M0L3idxS1162);
        _M0L4sizeS3433 = _M0L4selfS1164->$1;
        _M0L6_2atmpS3432 = _M0L4sizeS3433 - 1;
        _M0L4selfS1164->$1 = _M0L6_2atmpS3432;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1166);
      }
      _M0L3pslS3434 = _M0L8_2aentryS1166->$2;
      moonbit_decref(_M0L8_2aentryS1166);
      if (_M0L1iS1161 > _M0L3pslS3434) {
        break;
      }
      _M0L6_2atmpS3435 = _M0L1iS1161 + 1;
      _M0L6_2atmpS3437 = _M0L3idxS1162 + 1;
      _M0L14capacity__maskS3438 = _M0L4selfS1164->$3;
      _M0L6_2atmpS3436 = _M0L6_2atmpS3437 & _M0L14capacity__maskS3438;
      _M0L1iS1161 = _M0L6_2atmpS3435;
      _M0L3idxS1162 = _M0L6_2atmpS3436;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1173,
  moonbit_string_t _M0L3keyS1177,
  int32_t _M0L4hashS1176
) {
  int32_t _M0L14capacity__maskS3453;
  int32_t _M0L6_2atmpS3452;
  int32_t _M0L1iS1170;
  int32_t _M0L3idxS1171;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3453 = _M0L4selfS1173->$3;
  _M0L6_2atmpS3452 = _M0L4hashS1176 & _M0L14capacity__maskS3453;
  _M0L1iS1170 = 0;
  _M0L3idxS1171 = _M0L6_2atmpS3452;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3451 = _M0L4selfS1173->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS1172;
    if (
      _M0L3idxS1171 < 0
      || _M0L3idxS1171 >= Moonbit_array_length(_M0L7entriesS3451)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1172
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3451[_M0L3idxS1171];
    if (_M0L7_2abindS1172 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1174 = _M0L7_2abindS1172;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS1175 = _M0L7_2aSomeS1174;
      int32_t _M0L4hashS3443 = _M0L8_2aentryS1175->$3;
      int32_t _if__result_5428;
      int32_t _M0L3pslS3446;
      int32_t _M0L6_2atmpS3447;
      int32_t _M0L6_2atmpS3449;
      int32_t _M0L14capacity__maskS3450;
      int32_t _M0L6_2atmpS3448;
      if (_M0L4hashS3443 == _M0L4hashS1176) {
        moonbit_string_t _M0L3keyS3442 = _M0L8_2aentryS1175->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5428
        = _M0L3keyS3442 == _M0L3keyS1177
          || Moonbit_array_length(_M0L3keyS3442)
             == Moonbit_array_length(_M0L3keyS1177)
             && 0
                == memcmp(_M0L3keyS3442, _M0L3keyS1177, Moonbit_array_length(_M0L3keyS3442) * 2);
      } else {
        _if__result_5428 = 0;
      }
      if (_if__result_5428) {
        int32_t _M0L4sizeS3445;
        int32_t _M0L6_2atmpS3444;
        moonbit_incref(_M0L8_2aentryS1175);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGssE(_M0L4selfS1173, _M0L8_2aentryS1175);
        moonbit_decref(_M0L8_2aentryS1175);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGssE(_M0L4selfS1173, _M0L3idxS1171);
        _M0L4sizeS3445 = _M0L4selfS1173->$1;
        _M0L6_2atmpS3444 = _M0L4sizeS3445 - 1;
        _M0L4selfS1173->$1 = _M0L6_2atmpS3444;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1175);
      }
      _M0L3pslS3446 = _M0L8_2aentryS1175->$2;
      moonbit_decref(_M0L8_2aentryS1175);
      if (_M0L1iS1170 > _M0L3pslS3446) {
        break;
      }
      _M0L6_2atmpS3447 = _M0L1iS1170 + 1;
      _M0L6_2atmpS3449 = _M0L3idxS1171 + 1;
      _M0L14capacity__maskS3450 = _M0L4selfS1173->$3;
      _M0L6_2atmpS3448 = _M0L6_2atmpS3449 & _M0L14capacity__maskS3450;
      _M0L1iS1170 = _M0L6_2atmpS3447;
      _M0L3idxS1171 = _M0L6_2atmpS3448;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1182,
  moonbit_string_t _M0L3keyS1186,
  int32_t _M0L4hashS1185
) {
  int32_t _M0L14capacity__maskS3465;
  int32_t _M0L6_2atmpS3464;
  int32_t _M0L1iS1179;
  int32_t _M0L3idxS1180;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3465 = _M0L4selfS1182->$3;
  _M0L6_2atmpS3464 = _M0L4hashS1185 & _M0L14capacity__maskS3465;
  _M0L1iS1179 = 0;
  _M0L3idxS1180 = _M0L6_2atmpS3464;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3463 = _M0L4selfS1182->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1181;
    if (
      _M0L3idxS1180 < 0
      || _M0L3idxS1180 >= Moonbit_array_length(_M0L7entriesS3463)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1181
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3463[_M0L3idxS1180];
    if (_M0L7_2abindS1181 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1183 = _M0L7_2abindS1181;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS1184 = _M0L7_2aSomeS1183;
      int32_t _M0L4hashS3455 = _M0L8_2aentryS1184->$3;
      int32_t _if__result_5430;
      int32_t _M0L3pslS3458;
      int32_t _M0L6_2atmpS3459;
      int32_t _M0L6_2atmpS3461;
      int32_t _M0L14capacity__maskS3462;
      int32_t _M0L6_2atmpS3460;
      if (_M0L4hashS3455 == _M0L4hashS1185) {
        moonbit_string_t _M0L3keyS3454 = _M0L8_2aentryS1184->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5430
        = _M0L3keyS3454 == _M0L3keyS1186
          || Moonbit_array_length(_M0L3keyS3454)
             == Moonbit_array_length(_M0L3keyS1186)
             && 0
                == memcmp(_M0L3keyS3454, _M0L3keyS1186, Moonbit_array_length(_M0L3keyS3454) * 2);
      } else {
        _if__result_5430 = 0;
      }
      if (_if__result_5430) {
        int32_t _M0L4sizeS3457;
        int32_t _M0L6_2atmpS3456;
        moonbit_incref(_M0L8_2aentryS1184);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsbE(_M0L4selfS1182, _M0L8_2aentryS1184);
        moonbit_decref(_M0L8_2aentryS1184);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsbE(_M0L4selfS1182, _M0L3idxS1180);
        _M0L4sizeS3457 = _M0L4selfS1182->$1;
        _M0L6_2atmpS3456 = _M0L4sizeS3457 - 1;
        _M0L4selfS1182->$1 = _M0L6_2atmpS3456;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1184);
      }
      _M0L3pslS3458 = _M0L8_2aentryS1184->$2;
      moonbit_decref(_M0L8_2aentryS1184);
      if (_M0L1iS1179 > _M0L3pslS3458) {
        break;
      }
      _M0L6_2atmpS3459 = _M0L1iS1179 + 1;
      _M0L6_2atmpS3461 = _M0L3idxS1180 + 1;
      _M0L14capacity__maskS3462 = _M0L4selfS1182->$3;
      _M0L6_2atmpS3460 = _M0L6_2atmpS3461 & _M0L14capacity__maskS3462;
      _M0L1iS1179 = _M0L6_2atmpS3459;
      _M0L3idxS1180 = _M0L6_2atmpS3460;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1114,
  int32_t _M0L3idxS1121
) {
  int32_t _M0L3curS1112;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS1112 = _M0L3idxS1121;
  _2afor_1116:;
  while (1) {
    int32_t _M0L6_2atmpS3395 = _M0L3curS1112 + 1;
    int32_t _M0L14capacity__maskS3396 = _M0L4selfS1114->$3;
    int32_t _M0L4nextS1113 = _M0L6_2atmpS3395 & _M0L14capacity__maskS3396;
    struct _M0TPB5EntryGsiE** _M0L7entriesS3394 = _M0L4selfS1114->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1117;
    struct _M0TPB5EntryGsiE** _M0L7entriesS3390;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS3391;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS4744;
    int32_t _tmp_5433;
    if (
      _M0L4nextS1113 < 0
      || _M0L4nextS1113 >= Moonbit_array_length(_M0L7entriesS3394)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1117
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3394[_M0L4nextS1113];
    if (_M0L7_2abindS1117 == 0) {
      goto join_1115;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1118 = _M0L7_2abindS1117;
      struct _M0TPB5EntryGsiE* _M0L4_2axS1119 = _M0L7_2aSomeS1118;
      int32_t _M0L4_2axS1120 = _M0L4_2axS1119->$2;
      switch (_M0L4_2axS1120) {
        case 0: {
          goto join_1115;
          break;
        }
        default: {
          int32_t _M0L3pslS3393 = _M0L4_2axS1119->$2;
          int32_t _M0L6_2atmpS3392 = _M0L3pslS3393 - 1;
          _M0L4_2axS1119->$2 = _M0L6_2atmpS3392;
          moonbit_incref(_M0L4_2axS1119);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsiE(_M0L4selfS1114, _M0L4_2axS1119, _M0L3curS1112);
          moonbit_decref(_M0L4_2axS1119);
          _M0L3curS1112 = _M0L4nextS1113;
          goto _2afor_1116;
          break;
        }
      }
    }
    goto joinlet_5432;
    join_1115:;
    _M0L7entriesS3390 = _M0L4selfS1114->$0;
    _M0L6_2atmpS3391 = 0;
    if (
      _M0L3curS1112 < 0
      || _M0L3curS1112 >= Moonbit_array_length(_M0L7entriesS3390)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4744
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3390[_M0L3curS1112];
    if (_M0L6_2aoldS4744) {
      moonbit_decref(_M0L6_2aoldS4744);
    }
    _M0L7entriesS3390[_M0L3curS1112] = _M0L6_2atmpS3391;
    break;
    joinlet_5432:;
    _tmp_5433 = _M0L3curS1112;
    _M0L3curS1112 = _tmp_5433;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1124,
  int32_t _M0L3idxS1131
) {
  int32_t _M0L3curS1122;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS1122 = _M0L3idxS1131;
  _2afor_1126:;
  while (1) {
    int32_t _M0L6_2atmpS3402 = _M0L3curS1122 + 1;
    int32_t _M0L14capacity__maskS3403 = _M0L4selfS1124->$3;
    int32_t _M0L4nextS1123 = _M0L6_2atmpS3402 & _M0L14capacity__maskS3403;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS3401 =
      _M0L4selfS1124->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1127;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS3397;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3398;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4748;
    int32_t _tmp_5436;
    if (
      _M0L4nextS1123 < 0
      || _M0L4nextS1123 >= Moonbit_array_length(_M0L7entriesS3401)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1127
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS3401[
        _M0L4nextS1123
      ];
    if (_M0L7_2abindS1127 == 0) {
      goto join_1125;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1128 =
        _M0L7_2abindS1127;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1129 =
        _M0L7_2aSomeS1128;
      int32_t _M0L4_2axS1130 = _M0L4_2axS1129->$2;
      switch (_M0L4_2axS1130) {
        case 0: {
          goto join_1125;
          break;
        }
        default: {
          int32_t _M0L3pslS3400 = _M0L4_2axS1129->$2;
          int32_t _M0L6_2atmpS3399 = _M0L3pslS3400 - 1;
          _M0L4_2axS1129->$2 = _M0L6_2atmpS3399;
          moonbit_incref(_M0L4_2axS1129);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS1124, _M0L4_2axS1129, _M0L3curS1122);
          moonbit_decref(_M0L4_2axS1129);
          _M0L3curS1122 = _M0L4nextS1123;
          goto _2afor_1126;
          break;
        }
      }
    }
    goto joinlet_5435;
    join_1125:;
    _M0L7entriesS3397 = _M0L4selfS1124->$0;
    _M0L6_2atmpS3398 = 0;
    if (
      _M0L3curS1122 < 0
      || _M0L3curS1122 >= Moonbit_array_length(_M0L7entriesS3397)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4748
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS3397[
        _M0L3curS1122
      ];
    if (_M0L6_2aoldS4748) {
      moonbit_decref(_M0L6_2aoldS4748);
    }
    _M0L7entriesS3397[_M0L3curS1122] = _M0L6_2atmpS3398;
    break;
    joinlet_5435:;
    _tmp_5436 = _M0L3curS1122;
    _M0L3curS1122 = _tmp_5436;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1134,
  int32_t _M0L3idxS1141
) {
  int32_t _M0L3curS1132;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS1132 = _M0L3idxS1141;
  _2afor_1136:;
  while (1) {
    int32_t _M0L6_2atmpS3409 = _M0L3curS1132 + 1;
    int32_t _M0L14capacity__maskS3410 = _M0L4selfS1134->$3;
    int32_t _M0L4nextS1133 = _M0L6_2atmpS3409 & _M0L14capacity__maskS3410;
    struct _M0TPB5EntryGssE** _M0L7entriesS3408 = _M0L4selfS1134->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS1137;
    struct _M0TPB5EntryGssE** _M0L7entriesS3404;
    struct _M0TPB5EntryGssE* _M0L6_2atmpS3405;
    struct _M0TPB5EntryGssE* _M0L6_2aoldS4752;
    int32_t _tmp_5439;
    if (
      _M0L4nextS1133 < 0
      || _M0L4nextS1133 >= Moonbit_array_length(_M0L7entriesS3408)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1137
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3408[_M0L4nextS1133];
    if (_M0L7_2abindS1137 == 0) {
      goto join_1135;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1138 = _M0L7_2abindS1137;
      struct _M0TPB5EntryGssE* _M0L4_2axS1139 = _M0L7_2aSomeS1138;
      int32_t _M0L4_2axS1140 = _M0L4_2axS1139->$2;
      switch (_M0L4_2axS1140) {
        case 0: {
          goto join_1135;
          break;
        }
        default: {
          int32_t _M0L3pslS3407 = _M0L4_2axS1139->$2;
          int32_t _M0L6_2atmpS3406 = _M0L3pslS3407 - 1;
          _M0L4_2axS1139->$2 = _M0L6_2atmpS3406;
          moonbit_incref(_M0L4_2axS1139);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGssE(_M0L4selfS1134, _M0L4_2axS1139, _M0L3curS1132);
          moonbit_decref(_M0L4_2axS1139);
          _M0L3curS1132 = _M0L4nextS1133;
          goto _2afor_1136;
          break;
        }
      }
    }
    goto joinlet_5438;
    join_1135:;
    _M0L7entriesS3404 = _M0L4selfS1134->$0;
    _M0L6_2atmpS3405 = 0;
    if (
      _M0L3curS1132 < 0
      || _M0L3curS1132 >= Moonbit_array_length(_M0L7entriesS3404)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4752
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3404[_M0L3curS1132];
    if (_M0L6_2aoldS4752) {
      moonbit_decref(_M0L6_2aoldS4752);
    }
    _M0L7entriesS3404[_M0L3curS1132] = _M0L6_2atmpS3405;
    break;
    joinlet_5438:;
    _tmp_5439 = _M0L3curS1132;
    _M0L3curS1132 = _tmp_5439;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1144,
  int32_t _M0L3idxS1151
) {
  int32_t _M0L3curS1142;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS1142 = _M0L3idxS1151;
  _2afor_1146:;
  while (1) {
    int32_t _M0L6_2atmpS3416 = _M0L3curS1142 + 1;
    int32_t _M0L14capacity__maskS3417 = _M0L4selfS1144->$3;
    int32_t _M0L4nextS1143 = _M0L6_2atmpS3416 & _M0L14capacity__maskS3417;
    struct _M0TPB5EntryGsbE** _M0L7entriesS3415 = _M0L4selfS1144->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1147;
    struct _M0TPB5EntryGsbE** _M0L7entriesS3411;
    struct _M0TPB5EntryGsbE* _M0L6_2atmpS3412;
    struct _M0TPB5EntryGsbE* _M0L6_2aoldS4756;
    int32_t _tmp_5442;
    if (
      _M0L4nextS1143 < 0
      || _M0L4nextS1143 >= Moonbit_array_length(_M0L7entriesS3415)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1147
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3415[_M0L4nextS1143];
    if (_M0L7_2abindS1147 == 0) {
      goto join_1145;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1148 = _M0L7_2abindS1147;
      struct _M0TPB5EntryGsbE* _M0L4_2axS1149 = _M0L7_2aSomeS1148;
      int32_t _M0L4_2axS1150 = _M0L4_2axS1149->$2;
      switch (_M0L4_2axS1150) {
        case 0: {
          goto join_1145;
          break;
        }
        default: {
          int32_t _M0L3pslS3414 = _M0L4_2axS1149->$2;
          int32_t _M0L6_2atmpS3413 = _M0L3pslS3414 - 1;
          _M0L4_2axS1149->$2 = _M0L6_2atmpS3413;
          moonbit_incref(_M0L4_2axS1149);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsbE(_M0L4selfS1144, _M0L4_2axS1149, _M0L3curS1142);
          moonbit_decref(_M0L4_2axS1149);
          _M0L3curS1142 = _M0L4nextS1143;
          goto _2afor_1146;
          break;
        }
      }
    }
    goto joinlet_5441;
    join_1145:;
    _M0L7entriesS3411 = _M0L4selfS1144->$0;
    _M0L6_2atmpS3412 = 0;
    if (
      _M0L3curS1142 < 0
      || _M0L3curS1142 >= Moonbit_array_length(_M0L7entriesS3411)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4756
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3411[_M0L3curS1142];
    if (_M0L6_2aoldS4756) {
      moonbit_decref(_M0L6_2aoldS4756);
    }
    _M0L7entriesS3411[_M0L3curS1142] = _M0L6_2atmpS3412;
    break;
    joinlet_5441:;
    _tmp_5442 = _M0L3curS1142;
    _M0L3curS1142 = _tmp_5442;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1090,
  struct _M0TPB5EntryGsiE* _M0L5entryS1089
) {
  int32_t _M0L7_2abindS1088;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS1091;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS1088 = _M0L5entryS1089->$0;
  switch (_M0L7_2abindS1088) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L4nextS3362 = _M0L5entryS1089->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4761 = _M0L4selfS1090->$5;
      if (_M0L4nextS3362) {
        moonbit_incref(_M0L4nextS3362);
      }
      if (_M0L6_2aoldS4761) {
        moonbit_decref(_M0L6_2aoldS4761);
      }
      _M0L4selfS1090->$5 = _M0L4nextS3362;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS3366 = _M0L4selfS1090->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3365;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3363;
      struct _M0TPB5EntryGsiE* _M0L4nextS3364;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4763;
      if (
        _M0L7_2abindS1088 < 0
        || _M0L7_2abindS1088 >= Moonbit_array_length(_M0L7entriesS3366)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3365
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3366[_M0L7_2abindS1088];
      if (_M0L6_2atmpS3365) {
        moonbit_incref(_M0L6_2atmpS3365);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3363
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS3365);
      if (_M0L6_2atmpS3365) {
        moonbit_decref(_M0L6_2atmpS3365);
      }
      _M0L4nextS3364 = _M0L5entryS1089->$1;
      _M0L6_2aoldS4763 = _M0L6_2atmpS3363->$1;
      if (_M0L4nextS3364) {
        moonbit_incref(_M0L4nextS3364);
      }
      if (_M0L6_2aoldS4763) {
        moonbit_decref(_M0L6_2aoldS4763);
      }
      _M0L6_2atmpS3363->$1 = _M0L4nextS3364;
      moonbit_decref(_M0L6_2atmpS3363);
      break;
    }
  }
  _M0L7_2abindS1091 = _M0L5entryS1089->$1;
  if (_M0L7_2abindS1091 == 0) {
    int32_t _M0L4prevS3367 = _M0L5entryS1089->$0;
    _M0L4selfS1090->$6 = _M0L4prevS3367;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1092 = _M0L7_2abindS1091;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS1093 = _M0L7_2aSomeS1092;
    int32_t _M0L4prevS3368 = _M0L5entryS1089->$0;
    _M0L7_2anextS1093->$0 = _M0L4prevS3368;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1096,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS1095
) {
  int32_t _M0L7_2abindS1094;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1097;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS1094 = _M0L5entryS1095->$0;
  switch (_M0L7_2abindS1094) {
    case -1: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4nextS3369 =
        _M0L5entryS1095->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4768 =
        _M0L4selfS1096->$5;
      if (_M0L4nextS3369) {
        moonbit_incref(_M0L4nextS3369);
      }
      if (_M0L6_2aoldS4768) {
        moonbit_decref(_M0L6_2aoldS4768);
      }
      _M0L4selfS1096->$5 = _M0L4nextS3369;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS3373 =
        _M0L4selfS1096->$0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3372;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3370;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4nextS3371;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4770;
      if (
        _M0L7_2abindS1094 < 0
        || _M0L7_2abindS1094 >= Moonbit_array_length(_M0L7entriesS3373)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3372
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS3373[
          _M0L7_2abindS1094
        ];
      if (_M0L6_2atmpS3372) {
        moonbit_incref(_M0L6_2atmpS3372);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3370
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS3372);
      if (_M0L6_2atmpS3372) {
        moonbit_decref(_M0L6_2atmpS3372);
      }
      _M0L4nextS3371 = _M0L5entryS1095->$1;
      _M0L6_2aoldS4770 = _M0L6_2atmpS3370->$1;
      if (_M0L4nextS3371) {
        moonbit_incref(_M0L4nextS3371);
      }
      if (_M0L6_2aoldS4770) {
        moonbit_decref(_M0L6_2aoldS4770);
      }
      _M0L6_2atmpS3370->$1 = _M0L4nextS3371;
      moonbit_decref(_M0L6_2atmpS3370);
      break;
    }
  }
  _M0L7_2abindS1097 = _M0L5entryS1095->$1;
  if (_M0L7_2abindS1097 == 0) {
    int32_t _M0L4prevS3374 = _M0L5entryS1095->$0;
    _M0L4selfS1096->$6 = _M0L4prevS3374;
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1098 =
      _M0L7_2abindS1097;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS1099 =
      _M0L7_2aSomeS1098;
    int32_t _M0L4prevS3375 = _M0L5entryS1095->$0;
    _M0L7_2anextS1099->$0 = _M0L4prevS3375;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1102,
  struct _M0TPB5EntryGssE* _M0L5entryS1101
) {
  int32_t _M0L7_2abindS1100;
  struct _M0TPB5EntryGssE* _M0L7_2abindS1103;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS1100 = _M0L5entryS1101->$0;
  switch (_M0L7_2abindS1100) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L4nextS3376 = _M0L5entryS1101->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4775 = _M0L4selfS1102->$5;
      if (_M0L4nextS3376) {
        moonbit_incref(_M0L4nextS3376);
      }
      if (_M0L6_2aoldS4775) {
        moonbit_decref(_M0L6_2aoldS4775);
      }
      _M0L4selfS1102->$5 = _M0L4nextS3376;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS3380 = _M0L4selfS1102->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3379;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3377;
      struct _M0TPB5EntryGssE* _M0L4nextS3378;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4777;
      if (
        _M0L7_2abindS1100 < 0
        || _M0L7_2abindS1100 >= Moonbit_array_length(_M0L7entriesS3380)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3379
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS3380[_M0L7_2abindS1100];
      if (_M0L6_2atmpS3379) {
        moonbit_incref(_M0L6_2atmpS3379);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3377
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS3379);
      if (_M0L6_2atmpS3379) {
        moonbit_decref(_M0L6_2atmpS3379);
      }
      _M0L4nextS3378 = _M0L5entryS1101->$1;
      _M0L6_2aoldS4777 = _M0L6_2atmpS3377->$1;
      if (_M0L4nextS3378) {
        moonbit_incref(_M0L4nextS3378);
      }
      if (_M0L6_2aoldS4777) {
        moonbit_decref(_M0L6_2aoldS4777);
      }
      _M0L6_2atmpS3377->$1 = _M0L4nextS3378;
      moonbit_decref(_M0L6_2atmpS3377);
      break;
    }
  }
  _M0L7_2abindS1103 = _M0L5entryS1101->$1;
  if (_M0L7_2abindS1103 == 0) {
    int32_t _M0L4prevS3381 = _M0L5entryS1101->$0;
    _M0L4selfS1102->$6 = _M0L4prevS3381;
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS1104 = _M0L7_2abindS1103;
    struct _M0TPB5EntryGssE* _M0L7_2anextS1105 = _M0L7_2aSomeS1104;
    int32_t _M0L4prevS3382 = _M0L5entryS1101->$0;
    _M0L7_2anextS1105->$0 = _M0L4prevS3382;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1108,
  struct _M0TPB5EntryGsbE* _M0L5entryS1107
) {
  int32_t _M0L7_2abindS1106;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS1109;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS1106 = _M0L5entryS1107->$0;
  switch (_M0L7_2abindS1106) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L4nextS3383 = _M0L5entryS1107->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4782 = _M0L4selfS1108->$5;
      if (_M0L4nextS3383) {
        moonbit_incref(_M0L4nextS3383);
      }
      if (_M0L6_2aoldS4782) {
        moonbit_decref(_M0L6_2aoldS4782);
      }
      _M0L4selfS1108->$5 = _M0L4nextS3383;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS3387 = _M0L4selfS1108->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3386;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3384;
      struct _M0TPB5EntryGsbE* _M0L4nextS3385;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4784;
      if (
        _M0L7_2abindS1106 < 0
        || _M0L7_2abindS1106 >= Moonbit_array_length(_M0L7entriesS3387)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3386
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3387[_M0L7_2abindS1106];
      if (_M0L6_2atmpS3386) {
        moonbit_incref(_M0L6_2atmpS3386);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3384
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS3386);
      if (_M0L6_2atmpS3386) {
        moonbit_decref(_M0L6_2atmpS3386);
      }
      _M0L4nextS3385 = _M0L5entryS1107->$1;
      _M0L6_2aoldS4784 = _M0L6_2atmpS3384->$1;
      if (_M0L4nextS3385) {
        moonbit_incref(_M0L4nextS3385);
      }
      if (_M0L6_2aoldS4784) {
        moonbit_decref(_M0L6_2aoldS4784);
      }
      _M0L6_2atmpS3384->$1 = _M0L4nextS3385;
      moonbit_decref(_M0L6_2atmpS3384);
      break;
    }
  }
  _M0L7_2abindS1109 = _M0L5entryS1107->$1;
  if (_M0L7_2abindS1109 == 0) {
    int32_t _M0L4prevS3388 = _M0L5entryS1107->$0;
    _M0L4selfS1108->$6 = _M0L4prevS3388;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1110 = _M0L7_2abindS1109;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS1111 = _M0L7_2aSomeS1110;
    int32_t _M0L4prevS3389 = _M0L5entryS1107->$0;
    _M0L7_2anextS1111->$0 = _M0L4prevS3389;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1048,
  moonbit_string_t _M0L3keyS1044
) {
  int32_t _M0L4hashS1043;
  int32_t _M0L14capacity__maskS3321;
  int32_t _M0L6_2atmpS3320;
  int32_t _M0L1iS1045;
  int32_t _M0L3idxS1046;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1043 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1044);
  _M0L14capacity__maskS3321 = _M0L4selfS1048->$3;
  _M0L6_2atmpS3320 = _M0L4hashS1043 & _M0L14capacity__maskS3321;
  _M0L1iS1045 = 0;
  _M0L3idxS1046 = _M0L6_2atmpS3320;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS3319 =
      _M0L4selfS1048->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1047;
    if (
      _M0L3idxS1046 < 0
      || _M0L3idxS1046 >= Moonbit_array_length(_M0L7entriesS3319)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1047
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS3319[
        _M0L3idxS1046
      ];
    if (_M0L7_2abindS1047 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1049 =
        _M0L7_2abindS1047;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS1050 =
        _M0L7_2aSomeS1049;
      int32_t _M0L4hashS3313 = _M0L8_2aentryS1050->$3;
      int32_t _if__result_5444;
      int32_t _M0L3pslS3314;
      int32_t _M0L6_2atmpS3315;
      int32_t _M0L6_2atmpS3317;
      int32_t _M0L14capacity__maskS3318;
      int32_t _M0L6_2atmpS3316;
      if (_M0L4hashS3313 == _M0L4hashS1043) {
        moonbit_string_t _M0L3keyS3312 = _M0L8_2aentryS1050->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5444
        = _M0L3keyS3312 == _M0L3keyS1044
          || Moonbit_array_length(_M0L3keyS3312)
             == Moonbit_array_length(_M0L3keyS1044)
             && 0
                == memcmp(_M0L3keyS3312, _M0L3keyS1044, Moonbit_array_length(_M0L3keyS3312) * 2);
      } else {
        _if__result_5444 = 0;
      }
      if (_if__result_5444) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1050);
      }
      _M0L3pslS3314 = _M0L8_2aentryS1050->$2;
      moonbit_decref(_M0L8_2aentryS1050);
      if (_M0L1iS1045 > _M0L3pslS3314) {
        return 0;
      }
      _M0L6_2atmpS3315 = _M0L1iS1045 + 1;
      _M0L6_2atmpS3317 = _M0L3idxS1046 + 1;
      _M0L14capacity__maskS3318 = _M0L4selfS1048->$3;
      _M0L6_2atmpS3316 = _M0L6_2atmpS3317 & _M0L14capacity__maskS3318;
      _M0L1iS1045 = _M0L6_2atmpS3315;
      _M0L3idxS1046 = _M0L6_2atmpS3316;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1057,
  moonbit_string_t _M0L3keyS1053
) {
  int32_t _M0L4hashS1052;
  int32_t _M0L14capacity__maskS3331;
  int32_t _M0L6_2atmpS3330;
  int32_t _M0L1iS1054;
  int32_t _M0L3idxS1055;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1052 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1053);
  _M0L14capacity__maskS3331 = _M0L4selfS1057->$3;
  _M0L6_2atmpS3330 = _M0L4hashS1052 & _M0L14capacity__maskS3331;
  _M0L1iS1054 = 0;
  _M0L3idxS1055 = _M0L6_2atmpS3330;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3329 = _M0L4selfS1057->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1056;
    if (
      _M0L3idxS1055 < 0
      || _M0L3idxS1055 >= Moonbit_array_length(_M0L7entriesS3329)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1056
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3329[_M0L3idxS1055];
    if (_M0L7_2abindS1056 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1058 = _M0L7_2abindS1056;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS1059 = _M0L7_2aSomeS1058;
      int32_t _M0L4hashS3323 = _M0L8_2aentryS1059->$3;
      int32_t _if__result_5446;
      int32_t _M0L3pslS3324;
      int32_t _M0L6_2atmpS3325;
      int32_t _M0L6_2atmpS3327;
      int32_t _M0L14capacity__maskS3328;
      int32_t _M0L6_2atmpS3326;
      if (_M0L4hashS3323 == _M0L4hashS1052) {
        moonbit_string_t _M0L3keyS3322 = _M0L8_2aentryS1059->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5446
        = _M0L3keyS3322 == _M0L3keyS1053
          || Moonbit_array_length(_M0L3keyS3322)
             == Moonbit_array_length(_M0L3keyS1053)
             && 0
                == memcmp(_M0L3keyS3322, _M0L3keyS1053, Moonbit_array_length(_M0L3keyS3322) * 2);
      } else {
        _if__result_5446 = 0;
      }
      if (_if__result_5446) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1059);
      }
      _M0L3pslS3324 = _M0L8_2aentryS1059->$2;
      moonbit_decref(_M0L8_2aentryS1059);
      if (_M0L1iS1054 > _M0L3pslS3324) {
        return 0;
      }
      _M0L6_2atmpS3325 = _M0L1iS1054 + 1;
      _M0L6_2atmpS3327 = _M0L3idxS1055 + 1;
      _M0L14capacity__maskS3328 = _M0L4selfS1057->$3;
      _M0L6_2atmpS3326 = _M0L6_2atmpS3327 & _M0L14capacity__maskS3328;
      _M0L1iS1054 = _M0L6_2atmpS3325;
      _M0L3idxS1055 = _M0L6_2atmpS3326;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1066,
  moonbit_string_t _M0L3keyS1062
) {
  int32_t _M0L4hashS1061;
  int32_t _M0L14capacity__maskS3341;
  int32_t _M0L6_2atmpS3340;
  int32_t _M0L1iS1063;
  int32_t _M0L3idxS1064;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1061 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1062);
  _M0L14capacity__maskS3341 = _M0L4selfS1066->$3;
  _M0L6_2atmpS3340 = _M0L4hashS1061 & _M0L14capacity__maskS3341;
  _M0L1iS1063 = 0;
  _M0L3idxS1064 = _M0L6_2atmpS3340;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3339 = _M0L4selfS1066->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS1065;
    if (
      _M0L3idxS1064 < 0
      || _M0L3idxS1064 >= Moonbit_array_length(_M0L7entriesS3339)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1065
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3339[_M0L3idxS1064];
    if (_M0L7_2abindS1065 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1067 = _M0L7_2abindS1065;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS1068 = _M0L7_2aSomeS1067;
      int32_t _M0L4hashS3333 = _M0L8_2aentryS1068->$3;
      int32_t _if__result_5448;
      int32_t _M0L3pslS3334;
      int32_t _M0L6_2atmpS3335;
      int32_t _M0L6_2atmpS3337;
      int32_t _M0L14capacity__maskS3338;
      int32_t _M0L6_2atmpS3336;
      if (_M0L4hashS3333 == _M0L4hashS1061) {
        moonbit_string_t _M0L3keyS3332 = _M0L8_2aentryS1068->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5448
        = _M0L3keyS3332 == _M0L3keyS1062
          || Moonbit_array_length(_M0L3keyS3332)
             == Moonbit_array_length(_M0L3keyS1062)
             && 0
                == memcmp(_M0L3keyS3332, _M0L3keyS1062, Moonbit_array_length(_M0L3keyS3332) * 2);
      } else {
        _if__result_5448 = 0;
      }
      if (_if__result_5448) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1068);
      }
      _M0L3pslS3334 = _M0L8_2aentryS1068->$2;
      moonbit_decref(_M0L8_2aentryS1068);
      if (_M0L1iS1063 > _M0L3pslS3334) {
        return 0;
      }
      _M0L6_2atmpS3335 = _M0L1iS1063 + 1;
      _M0L6_2atmpS3337 = _M0L3idxS1064 + 1;
      _M0L14capacity__maskS3338 = _M0L4selfS1066->$3;
      _M0L6_2atmpS3336 = _M0L6_2atmpS3337 & _M0L14capacity__maskS3338;
      _M0L1iS1063 = _M0L6_2atmpS3335;
      _M0L3idxS1064 = _M0L6_2atmpS3336;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1075,
  moonbit_string_t _M0L3keyS1071
) {
  int32_t _M0L4hashS1070;
  int32_t _M0L14capacity__maskS3351;
  int32_t _M0L6_2atmpS3350;
  int32_t _M0L1iS1072;
  int32_t _M0L3idxS1073;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1070 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1071);
  _M0L14capacity__maskS3351 = _M0L4selfS1075->$3;
  _M0L6_2atmpS3350 = _M0L4hashS1070 & _M0L14capacity__maskS3351;
  _M0L1iS1072 = 0;
  _M0L3idxS1073 = _M0L6_2atmpS3350;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3349 = _M0L4selfS1075->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1074;
    if (
      _M0L3idxS1073 < 0
      || _M0L3idxS1073 >= Moonbit_array_length(_M0L7entriesS3349)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1074
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3349[_M0L3idxS1073];
    if (_M0L7_2abindS1074 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1076 = _M0L7_2abindS1074;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS1077 = _M0L7_2aSomeS1076;
      int32_t _M0L4hashS3343 = _M0L8_2aentryS1077->$3;
      int32_t _if__result_5450;
      int32_t _M0L3pslS3344;
      int32_t _M0L6_2atmpS3345;
      int32_t _M0L6_2atmpS3347;
      int32_t _M0L14capacity__maskS3348;
      int32_t _M0L6_2atmpS3346;
      if (_M0L4hashS3343 == _M0L4hashS1070) {
        moonbit_string_t _M0L3keyS3342 = _M0L8_2aentryS1077->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5450
        = _M0L3keyS3342 == _M0L3keyS1071
          || Moonbit_array_length(_M0L3keyS3342)
             == Moonbit_array_length(_M0L3keyS1071)
             && 0
                == memcmp(_M0L3keyS3342, _M0L3keyS1071, Moonbit_array_length(_M0L3keyS3342) * 2);
      } else {
        _if__result_5450 = 0;
      }
      if (_if__result_5450) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1077);
      }
      _M0L3pslS3344 = _M0L8_2aentryS1077->$2;
      moonbit_decref(_M0L8_2aentryS1077);
      if (_M0L1iS1072 > _M0L3pslS3344) {
        return 0;
      }
      _M0L6_2atmpS3345 = _M0L1iS1072 + 1;
      _M0L6_2atmpS3347 = _M0L3idxS1073 + 1;
      _M0L14capacity__maskS3348 = _M0L4selfS1075->$3;
      _M0L6_2atmpS3346 = _M0L6_2atmpS3347 & _M0L14capacity__maskS3348;
      _M0L1iS1072 = _M0L6_2atmpS3345;
      _M0L3idxS1073 = _M0L6_2atmpS3346;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1084,
  moonbit_string_t _M0L3keyS1080
) {
  int32_t _M0L4hashS1079;
  int32_t _M0L14capacity__maskS3361;
  int32_t _M0L6_2atmpS3360;
  int32_t _M0L1iS1081;
  int32_t _M0L3idxS1082;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1079 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1080);
  _M0L14capacity__maskS3361 = _M0L4selfS1084->$3;
  _M0L6_2atmpS3360 = _M0L4hashS1079 & _M0L14capacity__maskS3361;
  _M0L1iS1081 = 0;
  _M0L3idxS1082 = _M0L6_2atmpS3360;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3359 = _M0L4selfS1084->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1083;
    if (
      _M0L3idxS1082 < 0
      || _M0L3idxS1082 >= Moonbit_array_length(_M0L7entriesS3359)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1083
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3359[_M0L3idxS1082];
    if (_M0L7_2abindS1083 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1085 = _M0L7_2abindS1083;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS1086 = _M0L7_2aSomeS1085;
      int32_t _M0L4hashS3353 = _M0L8_2aentryS1086->$3;
      int32_t _if__result_5452;
      int32_t _M0L3pslS3354;
      int32_t _M0L6_2atmpS3355;
      int32_t _M0L6_2atmpS3357;
      int32_t _M0L14capacity__maskS3358;
      int32_t _M0L6_2atmpS3356;
      if (_M0L4hashS3353 == _M0L4hashS1079) {
        moonbit_string_t _M0L3keyS3352 = _M0L8_2aentryS1086->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5452
        = _M0L3keyS3352 == _M0L3keyS1080
          || Moonbit_array_length(_M0L3keyS3352)
             == Moonbit_array_length(_M0L3keyS1080)
             && 0
                == memcmp(_M0L3keyS3352, _M0L3keyS1080, Moonbit_array_length(_M0L3keyS3352) * 2);
      } else {
        _if__result_5452 = 0;
      }
      if (_if__result_5452) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1086);
      }
      _M0L3pslS3354 = _M0L8_2aentryS1086->$2;
      moonbit_decref(_M0L8_2aentryS1086);
      if (_M0L1iS1081 > _M0L3pslS3354) {
        return 0;
      }
      _M0L6_2atmpS3355 = _M0L1iS1081 + 1;
      _M0L6_2atmpS3357 = _M0L3idxS1082 + 1;
      _M0L14capacity__maskS3358 = _M0L4selfS1084->$3;
      _M0L6_2atmpS3356 = _M0L6_2atmpS3357 & _M0L14capacity__maskS3358;
      _M0L1iS1081 = _M0L6_2atmpS3355;
      _M0L3idxS1082 = _M0L6_2atmpS3356;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1012,
  moonbit_string_t _M0L3keyS1008
) {
  int32_t _M0L4hashS1007;
  int32_t _M0L14capacity__maskS3271;
  int32_t _M0L6_2atmpS3270;
  int32_t _M0L1iS1009;
  int32_t _M0L3idxS1010;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1007 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1008);
  _M0L14capacity__maskS3271 = _M0L4selfS1012->$3;
  _M0L6_2atmpS3270 = _M0L4hashS1007 & _M0L14capacity__maskS3271;
  _M0L1iS1009 = 0;
  _M0L3idxS1010 = _M0L6_2atmpS3270;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS3269 =
      _M0L4selfS1012->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1011;
    if (
      _M0L3idxS1010 < 0
      || _M0L3idxS1010 >= Moonbit_array_length(_M0L7entriesS3269)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1011
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS3269[
        _M0L3idxS1010
      ];
    if (_M0L7_2abindS1011 == 0) {
      void* _M0L6_2atmpS3258 = 0;
      return _M0L6_2atmpS3258;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1013 =
        _M0L7_2abindS1011;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS1014 =
        _M0L7_2aSomeS1013;
      int32_t _M0L4hashS3260 = _M0L8_2aentryS1014->$3;
      int32_t _if__result_5454;
      int32_t _M0L3pslS3263;
      int32_t _M0L6_2atmpS3265;
      int32_t _M0L6_2atmpS3267;
      int32_t _M0L14capacity__maskS3268;
      int32_t _M0L6_2atmpS3266;
      if (_M0L4hashS3260 == _M0L4hashS1007) {
        moonbit_string_t _M0L3keyS3259 = _M0L8_2aentryS1014->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5454
        = _M0L3keyS3259 == _M0L3keyS1008
          || Moonbit_array_length(_M0L3keyS3259)
             == Moonbit_array_length(_M0L3keyS1008)
             && 0
                == memcmp(_M0L3keyS3259, _M0L3keyS1008, Moonbit_array_length(_M0L3keyS3259) * 2);
      } else {
        _if__result_5454 = 0;
      }
      if (_if__result_5454) {
        void* _M0L5valueS3262 = _M0L8_2aentryS1014->$5;
        void* _M0L6_2atmpS3261;
        moonbit_incref(_M0L5valueS3262);
        _M0L6_2atmpS3261 = _M0L5valueS3262;
        return _M0L6_2atmpS3261;
      } else {
        moonbit_incref(_M0L8_2aentryS1014);
      }
      _M0L3pslS3263 = _M0L8_2aentryS1014->$2;
      moonbit_decref(_M0L8_2aentryS1014);
      if (_M0L1iS1009 > _M0L3pslS3263) {
        void* _M0L6_2atmpS3264 = 0;
        return _M0L6_2atmpS3264;
      }
      _M0L6_2atmpS3265 = _M0L1iS1009 + 1;
      _M0L6_2atmpS3267 = _M0L3idxS1010 + 1;
      _M0L14capacity__maskS3268 = _M0L4selfS1012->$3;
      _M0L6_2atmpS3266 = _M0L6_2atmpS3267 & _M0L14capacity__maskS3268;
      _M0L1iS1009 = _M0L6_2atmpS3265;
      _M0L3idxS1010 = _M0L6_2atmpS3266;
      continue;
    }
    break;
  }
}

int64_t _M0MPB3Map3getGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1021,
  moonbit_string_t _M0L3keyS1017
) {
  int32_t _M0L4hashS1016;
  int32_t _M0L14capacity__maskS3283;
  int32_t _M0L6_2atmpS3282;
  int32_t _M0L1iS1018;
  int32_t _M0L3idxS1019;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1016 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1017);
  _M0L14capacity__maskS3283 = _M0L4selfS1021->$3;
  _M0L6_2atmpS3282 = _M0L4hashS1016 & _M0L14capacity__maskS3283;
  _M0L1iS1018 = 0;
  _M0L3idxS1019 = _M0L6_2atmpS3282;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3281 = _M0L4selfS1021->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1020;
    if (
      _M0L3idxS1019 < 0
      || _M0L3idxS1019 >= Moonbit_array_length(_M0L7entriesS3281)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1020
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3281[_M0L3idxS1019];
    if (_M0L7_2abindS1020 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1022 = _M0L7_2abindS1020;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS1023 = _M0L7_2aSomeS1022;
      int32_t _M0L4hashS3273 = _M0L8_2aentryS1023->$3;
      int32_t _if__result_5456;
      int32_t _M0L3pslS3276;
      int32_t _M0L6_2atmpS3277;
      int32_t _M0L6_2atmpS3279;
      int32_t _M0L14capacity__maskS3280;
      int32_t _M0L6_2atmpS3278;
      if (_M0L4hashS3273 == _M0L4hashS1016) {
        moonbit_string_t _M0L3keyS3272 = _M0L8_2aentryS1023->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5456
        = _M0L3keyS3272 == _M0L3keyS1017
          || Moonbit_array_length(_M0L3keyS3272)
             == Moonbit_array_length(_M0L3keyS1017)
             && 0
                == memcmp(_M0L3keyS3272, _M0L3keyS1017, Moonbit_array_length(_M0L3keyS3272) * 2);
      } else {
        _if__result_5456 = 0;
      }
      if (_if__result_5456) {
        int32_t _M0L5valueS3275 = _M0L8_2aentryS1023->$5;
        int64_t _M0L6_2atmpS3274 = (int64_t)_M0L5valueS3275;
        return _M0L6_2atmpS3274;
      } else {
        moonbit_incref(_M0L8_2aentryS1023);
      }
      _M0L3pslS3276 = _M0L8_2aentryS1023->$2;
      moonbit_decref(_M0L8_2aentryS1023);
      if (_M0L1iS1018 > _M0L3pslS3276) {
        return 4294967296ll;
      }
      _M0L6_2atmpS3277 = _M0L1iS1018 + 1;
      _M0L6_2atmpS3279 = _M0L3idxS1019 + 1;
      _M0L14capacity__maskS3280 = _M0L4selfS1021->$3;
      _M0L6_2atmpS3278 = _M0L6_2atmpS3279 & _M0L14capacity__maskS3280;
      _M0L1iS1018 = _M0L6_2atmpS3277;
      _M0L3idxS1019 = _M0L6_2atmpS3278;
      continue;
    }
    break;
  }
}

moonbit_string_t _M0MPB3Map3getGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1030,
  moonbit_string_t _M0L3keyS1026
) {
  int32_t _M0L4hashS1025;
  int32_t _M0L14capacity__maskS3297;
  int32_t _M0L6_2atmpS3296;
  int32_t _M0L1iS1027;
  int32_t _M0L3idxS1028;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1025 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1026);
  _M0L14capacity__maskS3297 = _M0L4selfS1030->$3;
  _M0L6_2atmpS3296 = _M0L4hashS1025 & _M0L14capacity__maskS3297;
  _M0L1iS1027 = 0;
  _M0L3idxS1028 = _M0L6_2atmpS3296;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3295 = _M0L4selfS1030->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS1029;
    if (
      _M0L3idxS1028 < 0
      || _M0L3idxS1028 >= Moonbit_array_length(_M0L7entriesS3295)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1029
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3295[_M0L3idxS1028];
    if (_M0L7_2abindS1029 == 0) {
      moonbit_string_t _M0L6_2atmpS3284 = 0;
      return _M0L6_2atmpS3284;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1031 = _M0L7_2abindS1029;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS1032 = _M0L7_2aSomeS1031;
      int32_t _M0L4hashS3286 = _M0L8_2aentryS1032->$3;
      int32_t _if__result_5458;
      int32_t _M0L3pslS3289;
      int32_t _M0L6_2atmpS3291;
      int32_t _M0L6_2atmpS3293;
      int32_t _M0L14capacity__maskS3294;
      int32_t _M0L6_2atmpS3292;
      if (_M0L4hashS3286 == _M0L4hashS1025) {
        moonbit_string_t _M0L3keyS3285 = _M0L8_2aentryS1032->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5458
        = _M0L3keyS3285 == _M0L3keyS1026
          || Moonbit_array_length(_M0L3keyS3285)
             == Moonbit_array_length(_M0L3keyS1026)
             && 0
                == memcmp(_M0L3keyS3285, _M0L3keyS1026, Moonbit_array_length(_M0L3keyS3285) * 2);
      } else {
        _if__result_5458 = 0;
      }
      if (_if__result_5458) {
        moonbit_string_t _M0L5valueS3288 = _M0L8_2aentryS1032->$5;
        moonbit_string_t _M0L6_2atmpS3287;
        moonbit_incref(_M0L5valueS3288);
        _M0L6_2atmpS3287 = _M0L5valueS3288;
        return _M0L6_2atmpS3287;
      } else {
        moonbit_incref(_M0L8_2aentryS1032);
      }
      _M0L3pslS3289 = _M0L8_2aentryS1032->$2;
      moonbit_decref(_M0L8_2aentryS1032);
      if (_M0L1iS1027 > _M0L3pslS3289) {
        moonbit_string_t _M0L6_2atmpS3290 = 0;
        return _M0L6_2atmpS3290;
      }
      _M0L6_2atmpS3291 = _M0L1iS1027 + 1;
      _M0L6_2atmpS3293 = _M0L3idxS1028 + 1;
      _M0L14capacity__maskS3294 = _M0L4selfS1030->$3;
      _M0L6_2atmpS3292 = _M0L6_2atmpS3293 & _M0L14capacity__maskS3294;
      _M0L1iS1027 = _M0L6_2atmpS3291;
      _M0L3idxS1028 = _M0L6_2atmpS3292;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1039,
  moonbit_string_t _M0L3keyS1035
) {
  int32_t _M0L4hashS1034;
  int32_t _M0L14capacity__maskS3311;
  int32_t _M0L6_2atmpS3310;
  int32_t _M0L1iS1036;
  int32_t _M0L3idxS1037;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1034 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1035);
  _M0L14capacity__maskS3311 = _M0L4selfS1039->$3;
  _M0L6_2atmpS3310 = _M0L4hashS1034 & _M0L14capacity__maskS3311;
  _M0L1iS1036 = 0;
  _M0L3idxS1037 = _M0L6_2atmpS3310;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3309 = _M0L4selfS1039->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1038;
    if (
      _M0L3idxS1037 < 0
      || _M0L3idxS1037 >= Moonbit_array_length(_M0L7entriesS3309)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1038
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3309[_M0L3idxS1037];
    if (_M0L7_2abindS1038 == 0) {
      void* _M0L4NoneS3298 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS3298;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1040 = _M0L7_2abindS1038;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS1041 = _M0L7_2aSomeS1040;
      int32_t _M0L4hashS3300 = _M0L8_2aentryS1041->$3;
      int32_t _if__result_5460;
      int32_t _M0L3pslS3303;
      int32_t _M0L6_2atmpS3305;
      int32_t _M0L6_2atmpS3307;
      int32_t _M0L14capacity__maskS3308;
      int32_t _M0L6_2atmpS3306;
      if (_M0L4hashS3300 == _M0L4hashS1034) {
        moonbit_string_t _M0L3keyS3299 = _M0L8_2aentryS1041->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5460
        = _M0L3keyS3299 == _M0L3keyS1035
          || Moonbit_array_length(_M0L3keyS3299)
             == Moonbit_array_length(_M0L3keyS1035)
             && 0
                == memcmp(_M0L3keyS3299, _M0L3keyS1035, Moonbit_array_length(_M0L3keyS3299) * 2);
      } else {
        _if__result_5460 = 0;
      }
      if (_if__result_5460) {
        float _M0L5valueS3302 = _M0L8_2aentryS1041->$5;
        void* _M0L4SomeS3301 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS3301)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS3301)->$0
        = _M0L5valueS3302;
        return _M0L4SomeS3301;
      } else {
        moonbit_incref(_M0L8_2aentryS1041);
      }
      _M0L3pslS3303 = _M0L8_2aentryS1041->$2;
      moonbit_decref(_M0L8_2aentryS1041);
      if (_M0L1iS1036 > _M0L3pslS3303) {
        void* _M0L4NoneS3304 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS3304;
      }
      _M0L6_2atmpS3305 = _M0L1iS1036 + 1;
      _M0L6_2atmpS3307 = _M0L3idxS1037 + 1;
      _M0L14capacity__maskS3308 = _M0L4selfS1039->$3;
      _M0L6_2atmpS3306 = _M0L6_2atmpS3307 & _M0L14capacity__maskS3308;
      _M0L1iS1036 = _M0L6_2atmpS3305;
      _M0L3idxS1037 = _M0L6_2atmpS3306;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L3arrS953,
  int64_t _M0L8capacityS955
) {
  int32_t _M0L3endS3212;
  int32_t _M0L5startS3213;
  int32_t _M0L6lengthS952;
  int32_t _M0L8capacityS954;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1mS958;
  int32_t _M0L3endS3209;
  int32_t _M0L5startS3210;
  int32_t _M0L7_2abindS959;
  int32_t _M0L2__S960;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3212 = _M0L3arrS953.$2;
  _M0L5startS3213 = _M0L3arrS953.$1;
  _M0L6lengthS952 = _M0L3endS3212 - _M0L5startS3213;
  if (_M0L8capacityS955 == 4294967296ll) {
    if (_M0L6lengthS952 == 0) {
      _M0L8capacityS954 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS954 = _M0FPB21capacity__for__length(_M0L6lengthS952);
    }
  } else {
    int64_t _M0L7_2aSomeS956 = _M0L8capacityS955;
    int32_t _M0L11_2acapacityS957 = (int32_t)_M0L7_2aSomeS956;
    int32_t _M0L6_2atmpS3211;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3211 = _M0FPB21capacity__for__length(_M0L6lengthS952);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS954
    = _M0MPC13int3Int3max(_M0L11_2acapacityS957, _M0L6_2atmpS3211);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS958
  = _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L8capacityS954);
  _M0L3endS3209 = _M0L3arrS953.$2;
  _M0L5startS3210 = _M0L3arrS953.$1;
  _M0L7_2abindS959 = _M0L3endS3209 - _M0L5startS3210;
  _M0L2__S960 = 0;
  while (1) {
    if (_M0L2__S960 < _M0L7_2abindS959) {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L3bufS3206 =
        _M0L3arrS953.$0;
      int32_t _M0L5startS3208 = _M0L3arrS953.$1;
      int32_t _M0L6_2atmpS3207 = _M0L5startS3208 + _M0L2__S960;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1eS961 =
        (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L3bufS3206[
          _M0L6_2atmpS3207
        ];
      moonbit_string_t _M0L6_2atmpS3203 = _M0L1eS961->$0;
      void* _M0L6_2atmpS3204 = _M0L1eS961->$1;
      int32_t _M0L6_2atmpS3205;
      moonbit_incref(_M0L6_2atmpS3204);
      moonbit_incref(_M0L6_2atmpS3203);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L1mS958, _M0L6_2atmpS3203, _M0L6_2atmpS3204);
      moonbit_decref(_M0L6_2atmpS3203);
      moonbit_decref(_M0L6_2atmpS3204);
      _M0L6_2atmpS3205 = _M0L2__S960 + 1;
      _M0L2__S960 = _M0L6_2atmpS3205;
      continue;
    }
    break;
  }
  return _M0L1mS958;
}

struct _M0TPB3MapGsiE* _M0MPB3Map3MapGsiE(
  struct _M0TPB9ArrayViewGUsiEE _M0L3arrS964,
  int64_t _M0L8capacityS966
) {
  int32_t _M0L3endS3223;
  int32_t _M0L5startS3224;
  int32_t _M0L6lengthS963;
  int32_t _M0L8capacityS965;
  struct _M0TPB3MapGsiE* _M0L1mS969;
  int32_t _M0L3endS3220;
  int32_t _M0L5startS3221;
  int32_t _M0L7_2abindS970;
  int32_t _M0L2__S971;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3223 = _M0L3arrS964.$2;
  _M0L5startS3224 = _M0L3arrS964.$1;
  _M0L6lengthS963 = _M0L3endS3223 - _M0L5startS3224;
  if (_M0L8capacityS966 == 4294967296ll) {
    if (_M0L6lengthS963 == 0) {
      _M0L8capacityS965 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS965 = _M0FPB21capacity__for__length(_M0L6lengthS963);
    }
  } else {
    int64_t _M0L7_2aSomeS967 = _M0L8capacityS966;
    int32_t _M0L11_2acapacityS968 = (int32_t)_M0L7_2aSomeS967;
    int32_t _M0L6_2atmpS3222;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3222 = _M0FPB21capacity__for__length(_M0L6lengthS963);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS965
    = _M0MPC13int3Int3max(_M0L11_2acapacityS968, _M0L6_2atmpS3222);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS969 = _M0FPB8new__mapGsiE(_M0L8capacityS965);
  _M0L3endS3220 = _M0L3arrS964.$2;
  _M0L5startS3221 = _M0L3arrS964.$1;
  _M0L7_2abindS970 = _M0L3endS3220 - _M0L5startS3221;
  _M0L2__S971 = 0;
  while (1) {
    if (_M0L2__S971 < _M0L7_2abindS970) {
      struct _M0TUsiE** _M0L3bufS3217 = _M0L3arrS964.$0;
      int32_t _M0L5startS3219 = _M0L3arrS964.$1;
      int32_t _M0L6_2atmpS3218 = _M0L5startS3219 + _M0L2__S971;
      struct _M0TUsiE* _M0L1eS972 =
        (struct _M0TUsiE*)_M0L3bufS3217[_M0L6_2atmpS3218];
      moonbit_string_t _M0L6_2atmpS3214 = _M0L1eS972->$0;
      int32_t _M0L6_2atmpS3215 = _M0L1eS972->$1;
      int32_t _M0L6_2atmpS3216;
      moonbit_incref(_M0L6_2atmpS3214);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS969, _M0L6_2atmpS3214, _M0L6_2atmpS3215);
      moonbit_decref(_M0L6_2atmpS3214);
      _M0L6_2atmpS3216 = _M0L2__S971 + 1;
      _M0L2__S971 = _M0L6_2atmpS3216;
      continue;
    }
    break;
  }
  return _M0L1mS969;
}

struct _M0TPB3MapGssE* _M0MPB3Map3MapGssE(
  struct _M0TPB9ArrayViewGUssEE _M0L3arrS975,
  int64_t _M0L8capacityS977
) {
  int32_t _M0L3endS3234;
  int32_t _M0L5startS3235;
  int32_t _M0L6lengthS974;
  int32_t _M0L8capacityS976;
  struct _M0TPB3MapGssE* _M0L1mS980;
  int32_t _M0L3endS3231;
  int32_t _M0L5startS3232;
  int32_t _M0L7_2abindS981;
  int32_t _M0L2__S982;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3234 = _M0L3arrS975.$2;
  _M0L5startS3235 = _M0L3arrS975.$1;
  _M0L6lengthS974 = _M0L3endS3234 - _M0L5startS3235;
  if (_M0L8capacityS977 == 4294967296ll) {
    if (_M0L6lengthS974 == 0) {
      _M0L8capacityS976 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS976 = _M0FPB21capacity__for__length(_M0L6lengthS974);
    }
  } else {
    int64_t _M0L7_2aSomeS978 = _M0L8capacityS977;
    int32_t _M0L11_2acapacityS979 = (int32_t)_M0L7_2aSomeS978;
    int32_t _M0L6_2atmpS3233;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3233 = _M0FPB21capacity__for__length(_M0L6lengthS974);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS976
    = _M0MPC13int3Int3max(_M0L11_2acapacityS979, _M0L6_2atmpS3233);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS980 = _M0FPB8new__mapGssE(_M0L8capacityS976);
  _M0L3endS3231 = _M0L3arrS975.$2;
  _M0L5startS3232 = _M0L3arrS975.$1;
  _M0L7_2abindS981 = _M0L3endS3231 - _M0L5startS3232;
  _M0L2__S982 = 0;
  while (1) {
    if (_M0L2__S982 < _M0L7_2abindS981) {
      struct _M0TUssE** _M0L3bufS3228 = _M0L3arrS975.$0;
      int32_t _M0L5startS3230 = _M0L3arrS975.$1;
      int32_t _M0L6_2atmpS3229 = _M0L5startS3230 + _M0L2__S982;
      struct _M0TUssE* _M0L1eS983 =
        (struct _M0TUssE*)_M0L3bufS3228[_M0L6_2atmpS3229];
      moonbit_string_t _M0L6_2atmpS3225 = _M0L1eS983->$0;
      moonbit_string_t _M0L6_2atmpS3226 = _M0L1eS983->$1;
      int32_t _M0L6_2atmpS3227;
      moonbit_incref(_M0L6_2atmpS3226);
      moonbit_incref(_M0L6_2atmpS3225);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS980, _M0L6_2atmpS3225, _M0L6_2atmpS3226);
      moonbit_decref(_M0L6_2atmpS3225);
      moonbit_decref(_M0L6_2atmpS3226);
      _M0L6_2atmpS3227 = _M0L2__S982 + 1;
      _M0L2__S982 = _M0L6_2atmpS3227;
      continue;
    }
    break;
  }
  return _M0L1mS980;
}

struct _M0TPB3MapGsbE* _M0MPB3Map3MapGsbE(
  struct _M0TPB9ArrayViewGUsbEE _M0L3arrS986,
  int64_t _M0L8capacityS988
) {
  int32_t _M0L3endS3245;
  int32_t _M0L5startS3246;
  int32_t _M0L6lengthS985;
  int32_t _M0L8capacityS987;
  struct _M0TPB3MapGsbE* _M0L1mS991;
  int32_t _M0L3endS3242;
  int32_t _M0L5startS3243;
  int32_t _M0L7_2abindS992;
  int32_t _M0L2__S993;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3245 = _M0L3arrS986.$2;
  _M0L5startS3246 = _M0L3arrS986.$1;
  _M0L6lengthS985 = _M0L3endS3245 - _M0L5startS3246;
  if (_M0L8capacityS988 == 4294967296ll) {
    if (_M0L6lengthS985 == 0) {
      _M0L8capacityS987 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS987 = _M0FPB21capacity__for__length(_M0L6lengthS985);
    }
  } else {
    int64_t _M0L7_2aSomeS989 = _M0L8capacityS988;
    int32_t _M0L11_2acapacityS990 = (int32_t)_M0L7_2aSomeS989;
    int32_t _M0L6_2atmpS3244;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3244 = _M0FPB21capacity__for__length(_M0L6lengthS985);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS987
    = _M0MPC13int3Int3max(_M0L11_2acapacityS990, _M0L6_2atmpS3244);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS991 = _M0FPB8new__mapGsbE(_M0L8capacityS987);
  _M0L3endS3242 = _M0L3arrS986.$2;
  _M0L5startS3243 = _M0L3arrS986.$1;
  _M0L7_2abindS992 = _M0L3endS3242 - _M0L5startS3243;
  _M0L2__S993 = 0;
  while (1) {
    if (_M0L2__S993 < _M0L7_2abindS992) {
      struct _M0TUsbE** _M0L3bufS3239 = _M0L3arrS986.$0;
      int32_t _M0L5startS3241 = _M0L3arrS986.$1;
      int32_t _M0L6_2atmpS3240 = _M0L5startS3241 + _M0L2__S993;
      struct _M0TUsbE* _M0L1eS994 =
        (struct _M0TUsbE*)_M0L3bufS3239[_M0L6_2atmpS3240];
      moonbit_string_t _M0L6_2atmpS3236 = _M0L1eS994->$0;
      int32_t _M0L6_2atmpS3237 = _M0L1eS994->$1;
      int32_t _M0L6_2atmpS3238;
      moonbit_incref(_M0L6_2atmpS3236);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS991, _M0L6_2atmpS3236, _M0L6_2atmpS3237);
      moonbit_decref(_M0L6_2atmpS3236);
      _M0L6_2atmpS3238 = _M0L2__S993 + 1;
      _M0L2__S993 = _M0L6_2atmpS3238;
      continue;
    }
    break;
  }
  return _M0L1mS991;
}

struct _M0TPB3MapGsfE* _M0MPB3Map3MapGsfE(
  struct _M0TPB9ArrayViewGUsfEE _M0L3arrS997,
  int64_t _M0L8capacityS999
) {
  int32_t _M0L3endS3256;
  int32_t _M0L5startS3257;
  int32_t _M0L6lengthS996;
  int32_t _M0L8capacityS998;
  struct _M0TPB3MapGsfE* _M0L1mS1002;
  int32_t _M0L3endS3253;
  int32_t _M0L5startS3254;
  int32_t _M0L7_2abindS1003;
  int32_t _M0L2__S1004;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3256 = _M0L3arrS997.$2;
  _M0L5startS3257 = _M0L3arrS997.$1;
  _M0L6lengthS996 = _M0L3endS3256 - _M0L5startS3257;
  if (_M0L8capacityS999 == 4294967296ll) {
    if (_M0L6lengthS996 == 0) {
      _M0L8capacityS998 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS998 = _M0FPB21capacity__for__length(_M0L6lengthS996);
    }
  } else {
    int64_t _M0L7_2aSomeS1000 = _M0L8capacityS999;
    int32_t _M0L11_2acapacityS1001 = (int32_t)_M0L7_2aSomeS1000;
    int32_t _M0L6_2atmpS3255;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3255 = _M0FPB21capacity__for__length(_M0L6lengthS996);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS998
    = _M0MPC13int3Int3max(_M0L11_2acapacityS1001, _M0L6_2atmpS3255);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS1002 = _M0FPB8new__mapGsfE(_M0L8capacityS998);
  _M0L3endS3253 = _M0L3arrS997.$2;
  _M0L5startS3254 = _M0L3arrS997.$1;
  _M0L7_2abindS1003 = _M0L3endS3253 - _M0L5startS3254;
  _M0L2__S1004 = 0;
  while (1) {
    if (_M0L2__S1004 < _M0L7_2abindS1003) {
      struct _M0TUsfE** _M0L3bufS3250 = _M0L3arrS997.$0;
      int32_t _M0L5startS3252 = _M0L3arrS997.$1;
      int32_t _M0L6_2atmpS3251 = _M0L5startS3252 + _M0L2__S1004;
      struct _M0TUsfE* _M0L1eS1005 =
        (struct _M0TUsfE*)_M0L3bufS3250[_M0L6_2atmpS3251];
      moonbit_string_t _M0L6_2atmpS3247 = _M0L1eS1005->$0;
      float _M0L6_2atmpS3248 = _M0L1eS1005->$1;
      int32_t _M0L6_2atmpS3249;
      moonbit_incref(_M0L6_2atmpS3247);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS1002, _M0L6_2atmpS3247, _M0L6_2atmpS3248);
      moonbit_decref(_M0L6_2atmpS3247);
      _M0L6_2atmpS3249 = _M0L2__S1004 + 1;
      _M0L2__S1004 = _M0L6_2atmpS3249;
      continue;
    }
    break;
  }
  return _M0L1mS1002;
}

int32_t _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS937,
  moonbit_string_t _M0L3keyS938,
  void* _M0L5valueS939
) {
  int32_t _M0L6_2atmpS3198;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3198 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS938);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS937, _M0L3keyS938, _M0L5valueS939, _M0L6_2atmpS3198);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS940,
  moonbit_string_t _M0L3keyS941,
  int32_t _M0L5valueS942
) {
  int32_t _M0L6_2atmpS3199;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3199 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS941);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS940, _M0L3keyS941, _M0L5valueS942, _M0L6_2atmpS3199);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS943,
  moonbit_string_t _M0L3keyS944,
  moonbit_string_t _M0L5valueS945
) {
  int32_t _M0L6_2atmpS3200;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3200 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS944);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS943, _M0L3keyS944, _M0L5valueS945, _M0L6_2atmpS3200);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS946,
  moonbit_string_t _M0L3keyS947,
  int32_t _M0L5valueS948
) {
  int32_t _M0L6_2atmpS3201;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3201 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS947);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS946, _M0L3keyS947, _M0L5valueS948, _M0L6_2atmpS3201);
  return 0;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS949,
  moonbit_string_t _M0L3keyS950,
  float _M0L5valueS951
) {
  int32_t _M0L6_2atmpS3202;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3202 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS950);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS949, _M0L3keyS950, _M0L5valueS951, _M0L6_2atmpS3202);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS860,
  moonbit_string_t _M0L3keyS866,
  void* _M0L5valueS867,
  int32_t _M0L4hashS862
) {
  int32_t _M0L14capacity__maskS3125;
  int32_t _M0L6_2atmpS3124;
  int32_t _M0L3pslS857;
  int32_t _M0L3idxS858;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3125 = _M0L4selfS860->$3;
  _M0L6_2atmpS3124 = _M0L4hashS862 & _M0L14capacity__maskS3125;
  _M0L3pslS857 = 0;
  _M0L3idxS858 = _M0L6_2atmpS3124;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS3123 =
      _M0L4selfS860->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS859;
    if (
      _M0L3idxS858 < 0
      || _M0L3idxS858 >= Moonbit_array_length(_M0L7entriesS3123)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS859
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS3123[
        _M0L3idxS858
      ];
    if (_M0L7_2abindS859 == 0) {
      int32_t _M0L4sizeS3108 = _M0L4selfS860->$1;
      int32_t _M0L8grow__atS3109 = _M0L4selfS860->$4;
      int32_t _M0L7_2abindS863;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS864;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS865;
      if (_M0L4sizeS3108 >= _M0L8grow__atS3109) {
        int32_t _M0L14capacity__maskS3111;
        int32_t _M0L6_2atmpS3110;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS860);
        _M0L14capacity__maskS3111 = _M0L4selfS860->$3;
        _M0L6_2atmpS3110 = _M0L4hashS862 & _M0L14capacity__maskS3111;
        _M0L3pslS857 = 0;
        _M0L3idxS858 = _M0L6_2atmpS3110;
        continue;
      }
      _M0L7_2abindS863 = _M0L4selfS860->$6;
      _M0L7_2abindS864 = 0;
      moonbit_incref(_M0L3keyS866);
      moonbit_incref(_M0L5valueS867);
      _M0L5entryS865
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L5entryS865)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 84, 0);
      _M0L5entryS865->$0 = _M0L7_2abindS863;
      _M0L5entryS865->$1 = _M0L7_2abindS864;
      _M0L5entryS865->$2 = _M0L3pslS857;
      _M0L5entryS865->$3 = _M0L4hashS862;
      _M0L5entryS865->$4 = _M0L3keyS866;
      _M0L5entryS865->$5 = _M0L5valueS867;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS860, _M0L3idxS858, _M0L5entryS865);
      moonbit_decref(_M0L5entryS865);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS868 =
        _M0L7_2abindS859;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS869 =
        _M0L7_2aSomeS868;
      int32_t _M0L4hashS3113 = _M0L14_2acurr__entryS869->$3;
      int32_t _if__result_5467;
      int32_t _M0L3pslS3114;
      int32_t _M0L6_2atmpS3119;
      int32_t _M0L6_2atmpS3121;
      int32_t _M0L14capacity__maskS3122;
      int32_t _M0L6_2atmpS3120;
      if (_M0L4hashS3113 == _M0L4hashS862) {
        moonbit_string_t _M0L3keyS3112 = _M0L14_2acurr__entryS869->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5467
        = _M0L3keyS3112 == _M0L3keyS866
          || Moonbit_array_length(_M0L3keyS3112)
             == Moonbit_array_length(_M0L3keyS866)
             && 0
                == memcmp(_M0L3keyS3112, _M0L3keyS866, Moonbit_array_length(_M0L3keyS3112) * 2);
      } else {
        _if__result_5467 = 0;
      }
      if (_if__result_5467) {
        void* _M0L6_2aoldS4834 = _M0L14_2acurr__entryS869->$5;
        moonbit_incref(_M0L5valueS867);
        moonbit_decref(_M0L6_2aoldS4834);
        _M0L14_2acurr__entryS869->$5 = _M0L5valueS867;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS869);
      }
      _M0L3pslS3114 = _M0L14_2acurr__entryS869->$2;
      if (_M0L3pslS857 > _M0L3pslS3114) {
        int32_t _M0L4sizeS3115 = _M0L4selfS860->$1;
        int32_t _M0L8grow__atS3116 = _M0L4selfS860->$4;
        int32_t _M0L7_2abindS870;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS871;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS872;
        if (_M0L4sizeS3115 >= _M0L8grow__atS3116) {
          int32_t _M0L14capacity__maskS3118;
          int32_t _M0L6_2atmpS3117;
          moonbit_decref(_M0L14_2acurr__entryS869);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS860);
          _M0L14capacity__maskS3118 = _M0L4selfS860->$3;
          _M0L6_2atmpS3117 = _M0L4hashS862 & _M0L14capacity__maskS3118;
          _M0L3pslS857 = 0;
          _M0L3idxS858 = _M0L6_2atmpS3117;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS860, _M0L3idxS858, _M0L14_2acurr__entryS869);
        moonbit_decref(_M0L14_2acurr__entryS869);
        _M0L7_2abindS870 = _M0L4selfS860->$6;
        _M0L7_2abindS871 = 0;
        moonbit_incref(_M0L3keyS866);
        moonbit_incref(_M0L5valueS867);
        _M0L5entryS872
        = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
        Moonbit_object_header(_M0L5entryS872)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 84, 0);
        _M0L5entryS872->$0 = _M0L7_2abindS870;
        _M0L5entryS872->$1 = _M0L7_2abindS871;
        _M0L5entryS872->$2 = _M0L3pslS857;
        _M0L5entryS872->$3 = _M0L4hashS862;
        _M0L5entryS872->$4 = _M0L3keyS866;
        _M0L5entryS872->$5 = _M0L5valueS867;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS860, _M0L3idxS858, _M0L5entryS872);
        moonbit_decref(_M0L5entryS872);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS869);
      }
      _M0L6_2atmpS3119 = _M0L3pslS857 + 1;
      _M0L6_2atmpS3121 = _M0L3idxS858 + 1;
      _M0L14capacity__maskS3122 = _M0L4selfS860->$3;
      _M0L6_2atmpS3120 = _M0L6_2atmpS3121 & _M0L14capacity__maskS3122;
      _M0L3pslS857 = _M0L6_2atmpS3119;
      _M0L3idxS858 = _M0L6_2atmpS3120;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS876,
  moonbit_string_t _M0L3keyS882,
  int32_t _M0L5valueS883,
  int32_t _M0L4hashS878
) {
  int32_t _M0L14capacity__maskS3143;
  int32_t _M0L6_2atmpS3142;
  int32_t _M0L3pslS873;
  int32_t _M0L3idxS874;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3143 = _M0L4selfS876->$3;
  _M0L6_2atmpS3142 = _M0L4hashS878 & _M0L14capacity__maskS3143;
  _M0L3pslS873 = 0;
  _M0L3idxS874 = _M0L6_2atmpS3142;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3141 = _M0L4selfS876->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS875;
    if (
      _M0L3idxS874 < 0
      || _M0L3idxS874 >= Moonbit_array_length(_M0L7entriesS3141)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS875
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3141[_M0L3idxS874];
    if (_M0L7_2abindS875 == 0) {
      int32_t _M0L4sizeS3126 = _M0L4selfS876->$1;
      int32_t _M0L8grow__atS3127 = _M0L4selfS876->$4;
      int32_t _M0L7_2abindS879;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS880;
      struct _M0TPB5EntryGsiE* _M0L5entryS881;
      if (_M0L4sizeS3126 >= _M0L8grow__atS3127) {
        int32_t _M0L14capacity__maskS3129;
        int32_t _M0L6_2atmpS3128;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS876);
        _M0L14capacity__maskS3129 = _M0L4selfS876->$3;
        _M0L6_2atmpS3128 = _M0L4hashS878 & _M0L14capacity__maskS3129;
        _M0L3pslS873 = 0;
        _M0L3idxS874 = _M0L6_2atmpS3128;
        continue;
      }
      _M0L7_2abindS879 = _M0L4selfS876->$6;
      _M0L7_2abindS880 = 0;
      moonbit_incref(_M0L3keyS882);
      _M0L5entryS881
      = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
      Moonbit_object_header(_M0L5entryS881)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 89, 0);
      _M0L5entryS881->$0 = _M0L7_2abindS879;
      _M0L5entryS881->$1 = _M0L7_2abindS880;
      _M0L5entryS881->$2 = _M0L3pslS873;
      _M0L5entryS881->$3 = _M0L4hashS878;
      _M0L5entryS881->$4 = _M0L3keyS882;
      _M0L5entryS881->$5 = _M0L5valueS883;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS876, _M0L3idxS874, _M0L5entryS881);
      moonbit_decref(_M0L5entryS881);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS884 = _M0L7_2abindS875;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS885 = _M0L7_2aSomeS884;
      int32_t _M0L4hashS3131 = _M0L14_2acurr__entryS885->$3;
      int32_t _if__result_5469;
      int32_t _M0L3pslS3132;
      int32_t _M0L6_2atmpS3137;
      int32_t _M0L6_2atmpS3139;
      int32_t _M0L14capacity__maskS3140;
      int32_t _M0L6_2atmpS3138;
      if (_M0L4hashS3131 == _M0L4hashS878) {
        moonbit_string_t _M0L3keyS3130 = _M0L14_2acurr__entryS885->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5469
        = _M0L3keyS3130 == _M0L3keyS882
          || Moonbit_array_length(_M0L3keyS3130)
             == Moonbit_array_length(_M0L3keyS882)
             && 0
                == memcmp(_M0L3keyS3130, _M0L3keyS882, Moonbit_array_length(_M0L3keyS3130) * 2);
      } else {
        _if__result_5469 = 0;
      }
      if (_if__result_5469) {
        _M0L14_2acurr__entryS885->$5 = _M0L5valueS883;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS885);
      }
      _M0L3pslS3132 = _M0L14_2acurr__entryS885->$2;
      if (_M0L3pslS873 > _M0L3pslS3132) {
        int32_t _M0L4sizeS3133 = _M0L4selfS876->$1;
        int32_t _M0L8grow__atS3134 = _M0L4selfS876->$4;
        int32_t _M0L7_2abindS886;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS887;
        struct _M0TPB5EntryGsiE* _M0L5entryS888;
        if (_M0L4sizeS3133 >= _M0L8grow__atS3134) {
          int32_t _M0L14capacity__maskS3136;
          int32_t _M0L6_2atmpS3135;
          moonbit_decref(_M0L14_2acurr__entryS885);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS876);
          _M0L14capacity__maskS3136 = _M0L4selfS876->$3;
          _M0L6_2atmpS3135 = _M0L4hashS878 & _M0L14capacity__maskS3136;
          _M0L3pslS873 = 0;
          _M0L3idxS874 = _M0L6_2atmpS3135;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS876, _M0L3idxS874, _M0L14_2acurr__entryS885);
        moonbit_decref(_M0L14_2acurr__entryS885);
        _M0L7_2abindS886 = _M0L4selfS876->$6;
        _M0L7_2abindS887 = 0;
        moonbit_incref(_M0L3keyS882);
        _M0L5entryS888
        = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
        Moonbit_object_header(_M0L5entryS888)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 89, 0);
        _M0L5entryS888->$0 = _M0L7_2abindS886;
        _M0L5entryS888->$1 = _M0L7_2abindS887;
        _M0L5entryS888->$2 = _M0L3pslS873;
        _M0L5entryS888->$3 = _M0L4hashS878;
        _M0L5entryS888->$4 = _M0L3keyS882;
        _M0L5entryS888->$5 = _M0L5valueS883;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS876, _M0L3idxS874, _M0L5entryS888);
        moonbit_decref(_M0L5entryS888);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS885);
      }
      _M0L6_2atmpS3137 = _M0L3pslS873 + 1;
      _M0L6_2atmpS3139 = _M0L3idxS874 + 1;
      _M0L14capacity__maskS3140 = _M0L4selfS876->$3;
      _M0L6_2atmpS3138 = _M0L6_2atmpS3139 & _M0L14capacity__maskS3140;
      _M0L3pslS873 = _M0L6_2atmpS3137;
      _M0L3idxS874 = _M0L6_2atmpS3138;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS892,
  moonbit_string_t _M0L3keyS898,
  moonbit_string_t _M0L5valueS899,
  int32_t _M0L4hashS894
) {
  int32_t _M0L14capacity__maskS3161;
  int32_t _M0L6_2atmpS3160;
  int32_t _M0L3pslS889;
  int32_t _M0L3idxS890;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3161 = _M0L4selfS892->$3;
  _M0L6_2atmpS3160 = _M0L4hashS894 & _M0L14capacity__maskS3161;
  _M0L3pslS889 = 0;
  _M0L3idxS890 = _M0L6_2atmpS3160;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3159 = _M0L4selfS892->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS891;
    if (
      _M0L3idxS890 < 0
      || _M0L3idxS890 >= Moonbit_array_length(_M0L7entriesS3159)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS891
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3159[_M0L3idxS890];
    if (_M0L7_2abindS891 == 0) {
      int32_t _M0L4sizeS3144 = _M0L4selfS892->$1;
      int32_t _M0L8grow__atS3145 = _M0L4selfS892->$4;
      int32_t _M0L7_2abindS895;
      struct _M0TPB5EntryGssE* _M0L7_2abindS896;
      struct _M0TPB5EntryGssE* _M0L5entryS897;
      if (_M0L4sizeS3144 >= _M0L8grow__atS3145) {
        int32_t _M0L14capacity__maskS3147;
        int32_t _M0L6_2atmpS3146;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS892);
        _M0L14capacity__maskS3147 = _M0L4selfS892->$3;
        _M0L6_2atmpS3146 = _M0L4hashS894 & _M0L14capacity__maskS3147;
        _M0L3pslS889 = 0;
        _M0L3idxS890 = _M0L6_2atmpS3146;
        continue;
      }
      _M0L7_2abindS895 = _M0L4selfS892->$6;
      _M0L7_2abindS896 = 0;
      moonbit_incref(_M0L3keyS898);
      moonbit_incref(_M0L5valueS899);
      _M0L5entryS897
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS897)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 93, 0);
      _M0L5entryS897->$0 = _M0L7_2abindS895;
      _M0L5entryS897->$1 = _M0L7_2abindS896;
      _M0L5entryS897->$2 = _M0L3pslS889;
      _M0L5entryS897->$3 = _M0L4hashS894;
      _M0L5entryS897->$4 = _M0L3keyS898;
      _M0L5entryS897->$5 = _M0L5valueS899;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS892, _M0L3idxS890, _M0L5entryS897);
      moonbit_decref(_M0L5entryS897);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS900 = _M0L7_2abindS891;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS901 = _M0L7_2aSomeS900;
      int32_t _M0L4hashS3149 = _M0L14_2acurr__entryS901->$3;
      int32_t _if__result_5471;
      int32_t _M0L3pslS3150;
      int32_t _M0L6_2atmpS3155;
      int32_t _M0L6_2atmpS3157;
      int32_t _M0L14capacity__maskS3158;
      int32_t _M0L6_2atmpS3156;
      if (_M0L4hashS3149 == _M0L4hashS894) {
        moonbit_string_t _M0L3keyS3148 = _M0L14_2acurr__entryS901->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5471
        = _M0L3keyS3148 == _M0L3keyS898
          || Moonbit_array_length(_M0L3keyS3148)
             == Moonbit_array_length(_M0L3keyS898)
             && 0
                == memcmp(_M0L3keyS3148, _M0L3keyS898, Moonbit_array_length(_M0L3keyS3148) * 2);
      } else {
        _if__result_5471 = 0;
      }
      if (_if__result_5471) {
        moonbit_string_t _M0L6_2aoldS4841 = _M0L14_2acurr__entryS901->$5;
        moonbit_incref(_M0L5valueS899);
        moonbit_decref(_M0L6_2aoldS4841);
        _M0L14_2acurr__entryS901->$5 = _M0L5valueS899;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS901);
      }
      _M0L3pslS3150 = _M0L14_2acurr__entryS901->$2;
      if (_M0L3pslS889 > _M0L3pslS3150) {
        int32_t _M0L4sizeS3151 = _M0L4selfS892->$1;
        int32_t _M0L8grow__atS3152 = _M0L4selfS892->$4;
        int32_t _M0L7_2abindS902;
        struct _M0TPB5EntryGssE* _M0L7_2abindS903;
        struct _M0TPB5EntryGssE* _M0L5entryS904;
        if (_M0L4sizeS3151 >= _M0L8grow__atS3152) {
          int32_t _M0L14capacity__maskS3154;
          int32_t _M0L6_2atmpS3153;
          moonbit_decref(_M0L14_2acurr__entryS901);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS892);
          _M0L14capacity__maskS3154 = _M0L4selfS892->$3;
          _M0L6_2atmpS3153 = _M0L4hashS894 & _M0L14capacity__maskS3154;
          _M0L3pslS889 = 0;
          _M0L3idxS890 = _M0L6_2atmpS3153;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS892, _M0L3idxS890, _M0L14_2acurr__entryS901);
        moonbit_decref(_M0L14_2acurr__entryS901);
        _M0L7_2abindS902 = _M0L4selfS892->$6;
        _M0L7_2abindS903 = 0;
        moonbit_incref(_M0L3keyS898);
        moonbit_incref(_M0L5valueS899);
        _M0L5entryS904
        = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
        Moonbit_object_header(_M0L5entryS904)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 93, 0);
        _M0L5entryS904->$0 = _M0L7_2abindS902;
        _M0L5entryS904->$1 = _M0L7_2abindS903;
        _M0L5entryS904->$2 = _M0L3pslS889;
        _M0L5entryS904->$3 = _M0L4hashS894;
        _M0L5entryS904->$4 = _M0L3keyS898;
        _M0L5entryS904->$5 = _M0L5valueS899;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS892, _M0L3idxS890, _M0L5entryS904);
        moonbit_decref(_M0L5entryS904);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS901);
      }
      _M0L6_2atmpS3155 = _M0L3pslS889 + 1;
      _M0L6_2atmpS3157 = _M0L3idxS890 + 1;
      _M0L14capacity__maskS3158 = _M0L4selfS892->$3;
      _M0L6_2atmpS3156 = _M0L6_2atmpS3157 & _M0L14capacity__maskS3158;
      _M0L3pslS889 = _M0L6_2atmpS3155;
      _M0L3idxS890 = _M0L6_2atmpS3156;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS908,
  moonbit_string_t _M0L3keyS914,
  int32_t _M0L5valueS915,
  int32_t _M0L4hashS910
) {
  int32_t _M0L14capacity__maskS3179;
  int32_t _M0L6_2atmpS3178;
  int32_t _M0L3pslS905;
  int32_t _M0L3idxS906;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3179 = _M0L4selfS908->$3;
  _M0L6_2atmpS3178 = _M0L4hashS910 & _M0L14capacity__maskS3179;
  _M0L3pslS905 = 0;
  _M0L3idxS906 = _M0L6_2atmpS3178;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3177 = _M0L4selfS908->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS907;
    if (
      _M0L3idxS906 < 0
      || _M0L3idxS906 >= Moonbit_array_length(_M0L7entriesS3177)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS907
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3177[_M0L3idxS906];
    if (_M0L7_2abindS907 == 0) {
      int32_t _M0L4sizeS3162 = _M0L4selfS908->$1;
      int32_t _M0L8grow__atS3163 = _M0L4selfS908->$4;
      int32_t _M0L7_2abindS911;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS912;
      struct _M0TPB5EntryGsbE* _M0L5entryS913;
      if (_M0L4sizeS3162 >= _M0L8grow__atS3163) {
        int32_t _M0L14capacity__maskS3165;
        int32_t _M0L6_2atmpS3164;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS908);
        _M0L14capacity__maskS3165 = _M0L4selfS908->$3;
        _M0L6_2atmpS3164 = _M0L4hashS910 & _M0L14capacity__maskS3165;
        _M0L3pslS905 = 0;
        _M0L3idxS906 = _M0L6_2atmpS3164;
        continue;
      }
      _M0L7_2abindS911 = _M0L4selfS908->$6;
      _M0L7_2abindS912 = 0;
      moonbit_incref(_M0L3keyS914);
      _M0L5entryS913
      = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
      Moonbit_object_header(_M0L5entryS913)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 98, 0);
      _M0L5entryS913->$0 = _M0L7_2abindS911;
      _M0L5entryS913->$1 = _M0L7_2abindS912;
      _M0L5entryS913->$2 = _M0L3pslS905;
      _M0L5entryS913->$3 = _M0L4hashS910;
      _M0L5entryS913->$4 = _M0L3keyS914;
      _M0L5entryS913->$5 = _M0L5valueS915;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS908, _M0L3idxS906, _M0L5entryS913);
      moonbit_decref(_M0L5entryS913);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS916 = _M0L7_2abindS907;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS917 = _M0L7_2aSomeS916;
      int32_t _M0L4hashS3167 = _M0L14_2acurr__entryS917->$3;
      int32_t _if__result_5473;
      int32_t _M0L3pslS3168;
      int32_t _M0L6_2atmpS3173;
      int32_t _M0L6_2atmpS3175;
      int32_t _M0L14capacity__maskS3176;
      int32_t _M0L6_2atmpS3174;
      if (_M0L4hashS3167 == _M0L4hashS910) {
        moonbit_string_t _M0L3keyS3166 = _M0L14_2acurr__entryS917->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5473
        = _M0L3keyS3166 == _M0L3keyS914
          || Moonbit_array_length(_M0L3keyS3166)
             == Moonbit_array_length(_M0L3keyS914)
             && 0
                == memcmp(_M0L3keyS3166, _M0L3keyS914, Moonbit_array_length(_M0L3keyS3166) * 2);
      } else {
        _if__result_5473 = 0;
      }
      if (_if__result_5473) {
        _M0L14_2acurr__entryS917->$5 = _M0L5valueS915;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS917);
      }
      _M0L3pslS3168 = _M0L14_2acurr__entryS917->$2;
      if (_M0L3pslS905 > _M0L3pslS3168) {
        int32_t _M0L4sizeS3169 = _M0L4selfS908->$1;
        int32_t _M0L8grow__atS3170 = _M0L4selfS908->$4;
        int32_t _M0L7_2abindS918;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS919;
        struct _M0TPB5EntryGsbE* _M0L5entryS920;
        if (_M0L4sizeS3169 >= _M0L8grow__atS3170) {
          int32_t _M0L14capacity__maskS3172;
          int32_t _M0L6_2atmpS3171;
          moonbit_decref(_M0L14_2acurr__entryS917);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS908);
          _M0L14capacity__maskS3172 = _M0L4selfS908->$3;
          _M0L6_2atmpS3171 = _M0L4hashS910 & _M0L14capacity__maskS3172;
          _M0L3pslS905 = 0;
          _M0L3idxS906 = _M0L6_2atmpS3171;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS908, _M0L3idxS906, _M0L14_2acurr__entryS917);
        moonbit_decref(_M0L14_2acurr__entryS917);
        _M0L7_2abindS918 = _M0L4selfS908->$6;
        _M0L7_2abindS919 = 0;
        moonbit_incref(_M0L3keyS914);
        _M0L5entryS920
        = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
        Moonbit_object_header(_M0L5entryS920)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 98, 0);
        _M0L5entryS920->$0 = _M0L7_2abindS918;
        _M0L5entryS920->$1 = _M0L7_2abindS919;
        _M0L5entryS920->$2 = _M0L3pslS905;
        _M0L5entryS920->$3 = _M0L4hashS910;
        _M0L5entryS920->$4 = _M0L3keyS914;
        _M0L5entryS920->$5 = _M0L5valueS915;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS908, _M0L3idxS906, _M0L5entryS920);
        moonbit_decref(_M0L5entryS920);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS917);
      }
      _M0L6_2atmpS3173 = _M0L3pslS905 + 1;
      _M0L6_2atmpS3175 = _M0L3idxS906 + 1;
      _M0L14capacity__maskS3176 = _M0L4selfS908->$3;
      _M0L6_2atmpS3174 = _M0L6_2atmpS3175 & _M0L14capacity__maskS3176;
      _M0L3pslS905 = _M0L6_2atmpS3173;
      _M0L3idxS906 = _M0L6_2atmpS3174;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS924,
  moonbit_string_t _M0L3keyS930,
  float _M0L5valueS931,
  int32_t _M0L4hashS926
) {
  int32_t _M0L14capacity__maskS3197;
  int32_t _M0L6_2atmpS3196;
  int32_t _M0L3pslS921;
  int32_t _M0L3idxS922;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3197 = _M0L4selfS924->$3;
  _M0L6_2atmpS3196 = _M0L4hashS926 & _M0L14capacity__maskS3197;
  _M0L3pslS921 = 0;
  _M0L3idxS922 = _M0L6_2atmpS3196;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3195 = _M0L4selfS924->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS923;
    if (
      _M0L3idxS922 < 0
      || _M0L3idxS922 >= Moonbit_array_length(_M0L7entriesS3195)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS923
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3195[_M0L3idxS922];
    if (_M0L7_2abindS923 == 0) {
      int32_t _M0L4sizeS3180 = _M0L4selfS924->$1;
      int32_t _M0L8grow__atS3181 = _M0L4selfS924->$4;
      int32_t _M0L7_2abindS927;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS928;
      struct _M0TPB5EntryGsfE* _M0L5entryS929;
      if (_M0L4sizeS3180 >= _M0L8grow__atS3181) {
        int32_t _M0L14capacity__maskS3183;
        int32_t _M0L6_2atmpS3182;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS924);
        _M0L14capacity__maskS3183 = _M0L4selfS924->$3;
        _M0L6_2atmpS3182 = _M0L4hashS926 & _M0L14capacity__maskS3183;
        _M0L3pslS921 = 0;
        _M0L3idxS922 = _M0L6_2atmpS3182;
        continue;
      }
      _M0L7_2abindS927 = _M0L4selfS924->$6;
      _M0L7_2abindS928 = 0;
      moonbit_incref(_M0L3keyS930);
      _M0L5entryS929
      = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
      Moonbit_object_header(_M0L5entryS929)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 102, 0);
      _M0L5entryS929->$0 = _M0L7_2abindS927;
      _M0L5entryS929->$1 = _M0L7_2abindS928;
      _M0L5entryS929->$2 = _M0L3pslS921;
      _M0L5entryS929->$3 = _M0L4hashS926;
      _M0L5entryS929->$4 = _M0L3keyS930;
      _M0L5entryS929->$5 = _M0L5valueS931;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS924, _M0L3idxS922, _M0L5entryS929);
      moonbit_decref(_M0L5entryS929);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS932 = _M0L7_2abindS923;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS933 = _M0L7_2aSomeS932;
      int32_t _M0L4hashS3185 = _M0L14_2acurr__entryS933->$3;
      int32_t _if__result_5475;
      int32_t _M0L3pslS3186;
      int32_t _M0L6_2atmpS3191;
      int32_t _M0L6_2atmpS3193;
      int32_t _M0L14capacity__maskS3194;
      int32_t _M0L6_2atmpS3192;
      if (_M0L4hashS3185 == _M0L4hashS926) {
        moonbit_string_t _M0L3keyS3184 = _M0L14_2acurr__entryS933->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5475
        = _M0L3keyS3184 == _M0L3keyS930
          || Moonbit_array_length(_M0L3keyS3184)
             == Moonbit_array_length(_M0L3keyS930)
             && 0
                == memcmp(_M0L3keyS3184, _M0L3keyS930, Moonbit_array_length(_M0L3keyS3184) * 2);
      } else {
        _if__result_5475 = 0;
      }
      if (_if__result_5475) {
        _M0L14_2acurr__entryS933->$5 = _M0L5valueS931;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS933);
      }
      _M0L3pslS3186 = _M0L14_2acurr__entryS933->$2;
      if (_M0L3pslS921 > _M0L3pslS3186) {
        int32_t _M0L4sizeS3187 = _M0L4selfS924->$1;
        int32_t _M0L8grow__atS3188 = _M0L4selfS924->$4;
        int32_t _M0L7_2abindS934;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS935;
        struct _M0TPB5EntryGsfE* _M0L5entryS936;
        if (_M0L4sizeS3187 >= _M0L8grow__atS3188) {
          int32_t _M0L14capacity__maskS3190;
          int32_t _M0L6_2atmpS3189;
          moonbit_decref(_M0L14_2acurr__entryS933);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS924);
          _M0L14capacity__maskS3190 = _M0L4selfS924->$3;
          _M0L6_2atmpS3189 = _M0L4hashS926 & _M0L14capacity__maskS3190;
          _M0L3pslS921 = 0;
          _M0L3idxS922 = _M0L6_2atmpS3189;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS924, _M0L3idxS922, _M0L14_2acurr__entryS933);
        moonbit_decref(_M0L14_2acurr__entryS933);
        _M0L7_2abindS934 = _M0L4selfS924->$6;
        _M0L7_2abindS935 = 0;
        moonbit_incref(_M0L3keyS930);
        _M0L5entryS936
        = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
        Moonbit_object_header(_M0L5entryS936)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 102, 0);
        _M0L5entryS936->$0 = _M0L7_2abindS934;
        _M0L5entryS936->$1 = _M0L7_2abindS935;
        _M0L5entryS936->$2 = _M0L3pslS921;
        _M0L5entryS936->$3 = _M0L4hashS926;
        _M0L5entryS936->$4 = _M0L3keyS930;
        _M0L5entryS936->$5 = _M0L5valueS931;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS924, _M0L3idxS922, _M0L5entryS936);
        moonbit_decref(_M0L5entryS936);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS933);
      }
      _M0L6_2atmpS3191 = _M0L3pslS921 + 1;
      _M0L6_2atmpS3193 = _M0L3idxS922 + 1;
      _M0L14capacity__maskS3194 = _M0L4selfS924->$3;
      _M0L6_2atmpS3192 = _M0L6_2atmpS3193 & _M0L14capacity__maskS3194;
      _M0L3pslS921 = _M0L6_2atmpS3191;
      _M0L3idxS922 = _M0L6_2atmpS3192;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS818
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L9old__headS817;
  int32_t _M0L8capacityS3075;
  int32_t _M0L13new__capacityS819;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3069;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS3068;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2aoldS4854;
  int32_t _M0L6_2atmpS3070;
  int32_t _M0L8capacityS3072;
  int32_t _M0L6_2atmpS3071;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3073;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4853;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1xS820;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS817 = _M0L4selfS818->$5;
  _M0L8capacityS3075 = _M0L4selfS818->$2;
  _M0L13new__capacityS819 = _M0L8capacityS3075 << 1;
  _M0L6_2atmpS3069 = 0;
  _M0L6_2atmpS3068
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS819, _M0L6_2atmpS3069);
  _M0L6_2aoldS4854 = _M0L4selfS818->$0;
  if (_M0L9old__headS817) {
    moonbit_incref(_M0L9old__headS817);
  }
  moonbit_decref(_M0L6_2aoldS4854);
  _M0L4selfS818->$0 = _M0L6_2atmpS3068;
  _M0L4selfS818->$2 = _M0L13new__capacityS819;
  _M0L6_2atmpS3070 = _M0L13new__capacityS819 - 1;
  _M0L4selfS818->$3 = _M0L6_2atmpS3070;
  _M0L8capacityS3072 = _M0L4selfS818->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3071 = _M0FPB21calc__grow__threshold(_M0L8capacityS3072);
  _M0L4selfS818->$4 = _M0L6_2atmpS3071;
  _M0L4selfS818->$1 = 0;
  _M0L6_2atmpS3073 = 0;
  _M0L6_2aoldS4853 = _M0L4selfS818->$5;
  if (_M0L6_2aoldS4853) {
    moonbit_decref(_M0L6_2aoldS4853);
  }
  _M0L4selfS818->$5 = _M0L6_2atmpS3073;
  _M0L4selfS818->$6 = -1;
  _M0L1xS820 = _M0L9old__headS817;
  while (1) {
    if (_M0L1xS820 == 0) {
      if (_M0L1xS820) {
        moonbit_decref(_M0L1xS820);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS822 =
        _M0L1xS820;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2aeS823 =
        _M0L7_2aSomeS822;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L15next__in__chainS824 =
        _M0L4_2aeS823->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3074 =
        0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4851 =
        _M0L4_2aeS823->$1;
      if (_M0L15next__in__chainS824) {
        moonbit_incref(_M0L15next__in__chainS824);
      }
      if (_M0L6_2aoldS4851) {
        moonbit_decref(_M0L6_2aoldS4851);
      }
      _M0L4_2aeS823->$1 = _M0L6_2atmpS3074;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS818, _M0L4_2aeS823);
      moonbit_decref(_M0L4_2aeS823);
      _M0L1xS820 = _M0L15next__in__chainS824;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE* _M0L4selfS826) {
  struct _M0TPB5EntryGsiE* _M0L9old__headS825;
  int32_t _M0L8capacityS3083;
  int32_t _M0L13new__capacityS827;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS3077;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS3076;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS4859;
  int32_t _M0L6_2atmpS3078;
  int32_t _M0L8capacityS3080;
  int32_t _M0L6_2atmpS3079;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS3081;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4858;
  struct _M0TPB5EntryGsiE* _M0L1xS828;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS825 = _M0L4selfS826->$5;
  _M0L8capacityS3083 = _M0L4selfS826->$2;
  _M0L13new__capacityS827 = _M0L8capacityS3083 << 1;
  _M0L6_2atmpS3077 = 0;
  _M0L6_2atmpS3076
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS827, _M0L6_2atmpS3077);
  _M0L6_2aoldS4859 = _M0L4selfS826->$0;
  if (_M0L9old__headS825) {
    moonbit_incref(_M0L9old__headS825);
  }
  moonbit_decref(_M0L6_2aoldS4859);
  _M0L4selfS826->$0 = _M0L6_2atmpS3076;
  _M0L4selfS826->$2 = _M0L13new__capacityS827;
  _M0L6_2atmpS3078 = _M0L13new__capacityS827 - 1;
  _M0L4selfS826->$3 = _M0L6_2atmpS3078;
  _M0L8capacityS3080 = _M0L4selfS826->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3079 = _M0FPB21calc__grow__threshold(_M0L8capacityS3080);
  _M0L4selfS826->$4 = _M0L6_2atmpS3079;
  _M0L4selfS826->$1 = 0;
  _M0L6_2atmpS3081 = 0;
  _M0L6_2aoldS4858 = _M0L4selfS826->$5;
  if (_M0L6_2aoldS4858) {
    moonbit_decref(_M0L6_2aoldS4858);
  }
  _M0L4selfS826->$5 = _M0L6_2atmpS3081;
  _M0L4selfS826->$6 = -1;
  _M0L1xS828 = _M0L9old__headS825;
  while (1) {
    if (_M0L1xS828 == 0) {
      if (_M0L1xS828) {
        moonbit_decref(_M0L1xS828);
      }
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS830 = _M0L1xS828;
      struct _M0TPB5EntryGsiE* _M0L4_2aeS831 = _M0L7_2aSomeS830;
      struct _M0TPB5EntryGsiE* _M0L15next__in__chainS832 = _M0L4_2aeS831->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3082 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4856 = _M0L4_2aeS831->$1;
      if (_M0L15next__in__chainS832) {
        moonbit_incref(_M0L15next__in__chainS832);
      }
      if (_M0L6_2aoldS4856) {
        moonbit_decref(_M0L6_2aoldS4856);
      }
      _M0L4_2aeS831->$1 = _M0L6_2atmpS3082;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsiE(_M0L4selfS826, _M0L4_2aeS831);
      moonbit_decref(_M0L4_2aeS831);
      _M0L1xS828 = _M0L15next__in__chainS832;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE* _M0L4selfS834) {
  struct _M0TPB5EntryGssE* _M0L9old__headS833;
  int32_t _M0L8capacityS3091;
  int32_t _M0L13new__capacityS835;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS3085;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS3084;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS4864;
  int32_t _M0L6_2atmpS3086;
  int32_t _M0L8capacityS3088;
  int32_t _M0L6_2atmpS3087;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS3089;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4863;
  struct _M0TPB5EntryGssE* _M0L1xS836;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS833 = _M0L4selfS834->$5;
  _M0L8capacityS3091 = _M0L4selfS834->$2;
  _M0L13new__capacityS835 = _M0L8capacityS3091 << 1;
  _M0L6_2atmpS3085 = 0;
  _M0L6_2atmpS3084
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS835, _M0L6_2atmpS3085);
  _M0L6_2aoldS4864 = _M0L4selfS834->$0;
  if (_M0L9old__headS833) {
    moonbit_incref(_M0L9old__headS833);
  }
  moonbit_decref(_M0L6_2aoldS4864);
  _M0L4selfS834->$0 = _M0L6_2atmpS3084;
  _M0L4selfS834->$2 = _M0L13new__capacityS835;
  _M0L6_2atmpS3086 = _M0L13new__capacityS835 - 1;
  _M0L4selfS834->$3 = _M0L6_2atmpS3086;
  _M0L8capacityS3088 = _M0L4selfS834->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3087 = _M0FPB21calc__grow__threshold(_M0L8capacityS3088);
  _M0L4selfS834->$4 = _M0L6_2atmpS3087;
  _M0L4selfS834->$1 = 0;
  _M0L6_2atmpS3089 = 0;
  _M0L6_2aoldS4863 = _M0L4selfS834->$5;
  if (_M0L6_2aoldS4863) {
    moonbit_decref(_M0L6_2aoldS4863);
  }
  _M0L4selfS834->$5 = _M0L6_2atmpS3089;
  _M0L4selfS834->$6 = -1;
  _M0L1xS836 = _M0L9old__headS833;
  while (1) {
    if (_M0L1xS836 == 0) {
      if (_M0L1xS836) {
        moonbit_decref(_M0L1xS836);
      }
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS838 = _M0L1xS836;
      struct _M0TPB5EntryGssE* _M0L4_2aeS839 = _M0L7_2aSomeS838;
      struct _M0TPB5EntryGssE* _M0L15next__in__chainS840 = _M0L4_2aeS839->$1;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3090 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4861 = _M0L4_2aeS839->$1;
      if (_M0L15next__in__chainS840) {
        moonbit_incref(_M0L15next__in__chainS840);
      }
      if (_M0L6_2aoldS4861) {
        moonbit_decref(_M0L6_2aoldS4861);
      }
      _M0L4_2aeS839->$1 = _M0L6_2atmpS3090;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGssE(_M0L4selfS834, _M0L4_2aeS839);
      moonbit_decref(_M0L4_2aeS839);
      _M0L1xS836 = _M0L15next__in__chainS840;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE* _M0L4selfS842) {
  struct _M0TPB5EntryGsbE* _M0L9old__headS841;
  int32_t _M0L8capacityS3099;
  int32_t _M0L13new__capacityS843;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS3093;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS3092;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS4869;
  int32_t _M0L6_2atmpS3094;
  int32_t _M0L8capacityS3096;
  int32_t _M0L6_2atmpS3095;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS3097;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4868;
  struct _M0TPB5EntryGsbE* _M0L1xS844;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS841 = _M0L4selfS842->$5;
  _M0L8capacityS3099 = _M0L4selfS842->$2;
  _M0L13new__capacityS843 = _M0L8capacityS3099 << 1;
  _M0L6_2atmpS3093 = 0;
  _M0L6_2atmpS3092
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS843, _M0L6_2atmpS3093);
  _M0L6_2aoldS4869 = _M0L4selfS842->$0;
  if (_M0L9old__headS841) {
    moonbit_incref(_M0L9old__headS841);
  }
  moonbit_decref(_M0L6_2aoldS4869);
  _M0L4selfS842->$0 = _M0L6_2atmpS3092;
  _M0L4selfS842->$2 = _M0L13new__capacityS843;
  _M0L6_2atmpS3094 = _M0L13new__capacityS843 - 1;
  _M0L4selfS842->$3 = _M0L6_2atmpS3094;
  _M0L8capacityS3096 = _M0L4selfS842->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3095 = _M0FPB21calc__grow__threshold(_M0L8capacityS3096);
  _M0L4selfS842->$4 = _M0L6_2atmpS3095;
  _M0L4selfS842->$1 = 0;
  _M0L6_2atmpS3097 = 0;
  _M0L6_2aoldS4868 = _M0L4selfS842->$5;
  if (_M0L6_2aoldS4868) {
    moonbit_decref(_M0L6_2aoldS4868);
  }
  _M0L4selfS842->$5 = _M0L6_2atmpS3097;
  _M0L4selfS842->$6 = -1;
  _M0L1xS844 = _M0L9old__headS841;
  while (1) {
    if (_M0L1xS844 == 0) {
      if (_M0L1xS844) {
        moonbit_decref(_M0L1xS844);
      }
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS846 = _M0L1xS844;
      struct _M0TPB5EntryGsbE* _M0L4_2aeS847 = _M0L7_2aSomeS846;
      struct _M0TPB5EntryGsbE* _M0L15next__in__chainS848 = _M0L4_2aeS847->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3098 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4866 = _M0L4_2aeS847->$1;
      if (_M0L15next__in__chainS848) {
        moonbit_incref(_M0L15next__in__chainS848);
      }
      if (_M0L6_2aoldS4866) {
        moonbit_decref(_M0L6_2aoldS4866);
      }
      _M0L4_2aeS847->$1 = _M0L6_2atmpS3098;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsbE(_M0L4selfS842, _M0L4_2aeS847);
      moonbit_decref(_M0L4_2aeS847);
      _M0L1xS844 = _M0L15next__in__chainS848;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE* _M0L4selfS850) {
  struct _M0TPB5EntryGsfE* _M0L9old__headS849;
  int32_t _M0L8capacityS3107;
  int32_t _M0L13new__capacityS851;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS3101;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS3100;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS4874;
  int32_t _M0L6_2atmpS3102;
  int32_t _M0L8capacityS3104;
  int32_t _M0L6_2atmpS3103;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS3105;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4873;
  struct _M0TPB5EntryGsfE* _M0L1xS852;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS849 = _M0L4selfS850->$5;
  _M0L8capacityS3107 = _M0L4selfS850->$2;
  _M0L13new__capacityS851 = _M0L8capacityS3107 << 1;
  _M0L6_2atmpS3101 = 0;
  _M0L6_2atmpS3100
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS851, _M0L6_2atmpS3101);
  _M0L6_2aoldS4874 = _M0L4selfS850->$0;
  if (_M0L9old__headS849) {
    moonbit_incref(_M0L9old__headS849);
  }
  moonbit_decref(_M0L6_2aoldS4874);
  _M0L4selfS850->$0 = _M0L6_2atmpS3100;
  _M0L4selfS850->$2 = _M0L13new__capacityS851;
  _M0L6_2atmpS3102 = _M0L13new__capacityS851 - 1;
  _M0L4selfS850->$3 = _M0L6_2atmpS3102;
  _M0L8capacityS3104 = _M0L4selfS850->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3103 = _M0FPB21calc__grow__threshold(_M0L8capacityS3104);
  _M0L4selfS850->$4 = _M0L6_2atmpS3103;
  _M0L4selfS850->$1 = 0;
  _M0L6_2atmpS3105 = 0;
  _M0L6_2aoldS4873 = _M0L4selfS850->$5;
  if (_M0L6_2aoldS4873) {
    moonbit_decref(_M0L6_2aoldS4873);
  }
  _M0L4selfS850->$5 = _M0L6_2atmpS3105;
  _M0L4selfS850->$6 = -1;
  _M0L1xS852 = _M0L9old__headS849;
  while (1) {
    if (_M0L1xS852 == 0) {
      if (_M0L1xS852) {
        moonbit_decref(_M0L1xS852);
      }
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS854 = _M0L1xS852;
      struct _M0TPB5EntryGsfE* _M0L4_2aeS855 = _M0L7_2aSomeS854;
      struct _M0TPB5EntryGsfE* _M0L15next__in__chainS856 = _M0L4_2aeS855->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS3106 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4871 = _M0L4_2aeS855->$1;
      if (_M0L15next__in__chainS856) {
        moonbit_incref(_M0L15next__in__chainS856);
      }
      if (_M0L6_2aoldS4871) {
        moonbit_decref(_M0L6_2aoldS4871);
      }
      _M0L4_2aeS855->$1 = _M0L6_2atmpS3106;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsfE(_M0L4selfS850, _M0L4_2aeS855);
      moonbit_decref(_M0L4_2aeS855);
      _M0L1xS852 = _M0L15next__in__chainS856;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS777,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5outerS773
) {
  int32_t _M0L4hashS772;
  int32_t _M0L14capacity__maskS3027;
  int32_t _M0L6_2atmpS3026;
  int32_t _M0L3pslS774;
  int32_t _M0L3idxS775;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS772 = _M0L5outerS773->$3;
  _M0L14capacity__maskS3027 = _M0L4selfS777->$3;
  _M0L6_2atmpS3026 = _M0L4hashS772 & _M0L14capacity__maskS3027;
  _M0L3pslS774 = 0;
  _M0L3idxS775 = _M0L6_2atmpS3026;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS3025 =
      _M0L4selfS777->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS776;
    if (
      _M0L3idxS775 < 0
      || _M0L3idxS775 >= Moonbit_array_length(_M0L7entriesS3025)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS776
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS3025[
        _M0L3idxS775
      ];
    if (_M0L7_2abindS776 == 0) {
      int32_t _M0L4tailS3018;
      _M0L5outerS773->$2 = _M0L3pslS774;
      _M0L4tailS3018 = _M0L4selfS777->$6;
      _M0L5outerS773->$0 = _M0L4tailS3018;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS777, _M0L3idxS775, _M0L5outerS773);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS778 =
        _M0L7_2abindS776;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2acurrS779 =
        _M0L7_2aSomeS778;
      int32_t _M0L3pslS3019 = _M0L7_2acurrS779->$2;
      if (_M0L3pslS774 > _M0L3pslS3019) {
        int32_t _M0L4tailS3020;
        moonbit_incref(_M0L7_2acurrS779);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS777, _M0L3idxS775, _M0L7_2acurrS779);
        moonbit_decref(_M0L7_2acurrS779);
        _M0L5outerS773->$2 = _M0L3pslS774;
        _M0L4tailS3020 = _M0L4selfS777->$6;
        _M0L5outerS773->$0 = _M0L4tailS3020;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS777, _M0L3idxS775, _M0L5outerS773);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3021 = _M0L3pslS774 + 1;
        int32_t _M0L6_2atmpS3023 = _M0L3idxS775 + 1;
        int32_t _M0L14capacity__maskS3024 = _M0L4selfS777->$3;
        int32_t _M0L6_2atmpS3022 =
          _M0L6_2atmpS3023 & _M0L14capacity__maskS3024;
        _M0L3pslS774 = _M0L6_2atmpS3021;
        _M0L3idxS775 = _M0L6_2atmpS3022;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS786,
  struct _M0TPB5EntryGsiE* _M0L5outerS782
) {
  int32_t _M0L4hashS781;
  int32_t _M0L14capacity__maskS3037;
  int32_t _M0L6_2atmpS3036;
  int32_t _M0L3pslS783;
  int32_t _M0L3idxS784;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS781 = _M0L5outerS782->$3;
  _M0L14capacity__maskS3037 = _M0L4selfS786->$3;
  _M0L6_2atmpS3036 = _M0L4hashS781 & _M0L14capacity__maskS3037;
  _M0L3pslS783 = 0;
  _M0L3idxS784 = _M0L6_2atmpS3036;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3035 = _M0L4selfS786->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS785;
    if (
      _M0L3idxS784 < 0
      || _M0L3idxS784 >= Moonbit_array_length(_M0L7entriesS3035)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS785
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3035[_M0L3idxS784];
    if (_M0L7_2abindS785 == 0) {
      int32_t _M0L4tailS3028;
      _M0L5outerS782->$2 = _M0L3pslS783;
      _M0L4tailS3028 = _M0L4selfS786->$6;
      _M0L5outerS782->$0 = _M0L4tailS3028;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS786, _M0L3idxS784, _M0L5outerS782);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS787 = _M0L7_2abindS785;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS788 = _M0L7_2aSomeS787;
      int32_t _M0L3pslS3029 = _M0L7_2acurrS788->$2;
      if (_M0L3pslS783 > _M0L3pslS3029) {
        int32_t _M0L4tailS3030;
        moonbit_incref(_M0L7_2acurrS788);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS786, _M0L3idxS784, _M0L7_2acurrS788);
        moonbit_decref(_M0L7_2acurrS788);
        _M0L5outerS782->$2 = _M0L3pslS783;
        _M0L4tailS3030 = _M0L4selfS786->$6;
        _M0L5outerS782->$0 = _M0L4tailS3030;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS786, _M0L3idxS784, _M0L5outerS782);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3031 = _M0L3pslS783 + 1;
        int32_t _M0L6_2atmpS3033 = _M0L3idxS784 + 1;
        int32_t _M0L14capacity__maskS3034 = _M0L4selfS786->$3;
        int32_t _M0L6_2atmpS3032 =
          _M0L6_2atmpS3033 & _M0L14capacity__maskS3034;
        _M0L3pslS783 = _M0L6_2atmpS3031;
        _M0L3idxS784 = _M0L6_2atmpS3032;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS795,
  struct _M0TPB5EntryGssE* _M0L5outerS791
) {
  int32_t _M0L4hashS790;
  int32_t _M0L14capacity__maskS3047;
  int32_t _M0L6_2atmpS3046;
  int32_t _M0L3pslS792;
  int32_t _M0L3idxS793;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS790 = _M0L5outerS791->$3;
  _M0L14capacity__maskS3047 = _M0L4selfS795->$3;
  _M0L6_2atmpS3046 = _M0L4hashS790 & _M0L14capacity__maskS3047;
  _M0L3pslS792 = 0;
  _M0L3idxS793 = _M0L6_2atmpS3046;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3045 = _M0L4selfS795->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS794;
    if (
      _M0L3idxS793 < 0
      || _M0L3idxS793 >= Moonbit_array_length(_M0L7entriesS3045)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS794
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3045[_M0L3idxS793];
    if (_M0L7_2abindS794 == 0) {
      int32_t _M0L4tailS3038;
      _M0L5outerS791->$2 = _M0L3pslS792;
      _M0L4tailS3038 = _M0L4selfS795->$6;
      _M0L5outerS791->$0 = _M0L4tailS3038;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS795, _M0L3idxS793, _M0L5outerS791);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS796 = _M0L7_2abindS794;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS797 = _M0L7_2aSomeS796;
      int32_t _M0L3pslS3039 = _M0L7_2acurrS797->$2;
      if (_M0L3pslS792 > _M0L3pslS3039) {
        int32_t _M0L4tailS3040;
        moonbit_incref(_M0L7_2acurrS797);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS795, _M0L3idxS793, _M0L7_2acurrS797);
        moonbit_decref(_M0L7_2acurrS797);
        _M0L5outerS791->$2 = _M0L3pslS792;
        _M0L4tailS3040 = _M0L4selfS795->$6;
        _M0L5outerS791->$0 = _M0L4tailS3040;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS795, _M0L3idxS793, _M0L5outerS791);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3041 = _M0L3pslS792 + 1;
        int32_t _M0L6_2atmpS3043 = _M0L3idxS793 + 1;
        int32_t _M0L14capacity__maskS3044 = _M0L4selfS795->$3;
        int32_t _M0L6_2atmpS3042 =
          _M0L6_2atmpS3043 & _M0L14capacity__maskS3044;
        _M0L3pslS792 = _M0L6_2atmpS3041;
        _M0L3idxS793 = _M0L6_2atmpS3042;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS804,
  struct _M0TPB5EntryGsbE* _M0L5outerS800
) {
  int32_t _M0L4hashS799;
  int32_t _M0L14capacity__maskS3057;
  int32_t _M0L6_2atmpS3056;
  int32_t _M0L3pslS801;
  int32_t _M0L3idxS802;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS799 = _M0L5outerS800->$3;
  _M0L14capacity__maskS3057 = _M0L4selfS804->$3;
  _M0L6_2atmpS3056 = _M0L4hashS799 & _M0L14capacity__maskS3057;
  _M0L3pslS801 = 0;
  _M0L3idxS802 = _M0L6_2atmpS3056;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3055 = _M0L4selfS804->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS803;
    if (
      _M0L3idxS802 < 0
      || _M0L3idxS802 >= Moonbit_array_length(_M0L7entriesS3055)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS803
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3055[_M0L3idxS802];
    if (_M0L7_2abindS803 == 0) {
      int32_t _M0L4tailS3048;
      _M0L5outerS800->$2 = _M0L3pslS801;
      _M0L4tailS3048 = _M0L4selfS804->$6;
      _M0L5outerS800->$0 = _M0L4tailS3048;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS804, _M0L3idxS802, _M0L5outerS800);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS805 = _M0L7_2abindS803;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS806 = _M0L7_2aSomeS805;
      int32_t _M0L3pslS3049 = _M0L7_2acurrS806->$2;
      if (_M0L3pslS801 > _M0L3pslS3049) {
        int32_t _M0L4tailS3050;
        moonbit_incref(_M0L7_2acurrS806);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS804, _M0L3idxS802, _M0L7_2acurrS806);
        moonbit_decref(_M0L7_2acurrS806);
        _M0L5outerS800->$2 = _M0L3pslS801;
        _M0L4tailS3050 = _M0L4selfS804->$6;
        _M0L5outerS800->$0 = _M0L4tailS3050;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS804, _M0L3idxS802, _M0L5outerS800);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3051 = _M0L3pslS801 + 1;
        int32_t _M0L6_2atmpS3053 = _M0L3idxS802 + 1;
        int32_t _M0L14capacity__maskS3054 = _M0L4selfS804->$3;
        int32_t _M0L6_2atmpS3052 =
          _M0L6_2atmpS3053 & _M0L14capacity__maskS3054;
        _M0L3pslS801 = _M0L6_2atmpS3051;
        _M0L3idxS802 = _M0L6_2atmpS3052;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS813,
  struct _M0TPB5EntryGsfE* _M0L5outerS809
) {
  int32_t _M0L4hashS808;
  int32_t _M0L14capacity__maskS3067;
  int32_t _M0L6_2atmpS3066;
  int32_t _M0L3pslS810;
  int32_t _M0L3idxS811;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS808 = _M0L5outerS809->$3;
  _M0L14capacity__maskS3067 = _M0L4selfS813->$3;
  _M0L6_2atmpS3066 = _M0L4hashS808 & _M0L14capacity__maskS3067;
  _M0L3pslS810 = 0;
  _M0L3idxS811 = _M0L6_2atmpS3066;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3065 = _M0L4selfS813->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS812;
    if (
      _M0L3idxS811 < 0
      || _M0L3idxS811 >= Moonbit_array_length(_M0L7entriesS3065)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS812
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3065[_M0L3idxS811];
    if (_M0L7_2abindS812 == 0) {
      int32_t _M0L4tailS3058;
      _M0L5outerS809->$2 = _M0L3pslS810;
      _M0L4tailS3058 = _M0L4selfS813->$6;
      _M0L5outerS809->$0 = _M0L4tailS3058;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS813, _M0L3idxS811, _M0L5outerS809);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS814 = _M0L7_2abindS812;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS815 = _M0L7_2aSomeS814;
      int32_t _M0L3pslS3059 = _M0L7_2acurrS815->$2;
      if (_M0L3pslS810 > _M0L3pslS3059) {
        int32_t _M0L4tailS3060;
        moonbit_incref(_M0L7_2acurrS815);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS813, _M0L3idxS811, _M0L7_2acurrS815);
        moonbit_decref(_M0L7_2acurrS815);
        _M0L5outerS809->$2 = _M0L3pslS810;
        _M0L4tailS3060 = _M0L4selfS813->$6;
        _M0L5outerS809->$0 = _M0L4tailS3060;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS813, _M0L3idxS811, _M0L5outerS809);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3061 = _M0L3pslS810 + 1;
        int32_t _M0L6_2atmpS3063 = _M0L3idxS811 + 1;
        int32_t _M0L14capacity__maskS3064 = _M0L4selfS813->$3;
        int32_t _M0L6_2atmpS3062 =
          _M0L6_2atmpS3063 & _M0L14capacity__maskS3064;
        _M0L3pslS810 = _M0L6_2atmpS3061;
        _M0L3idxS811 = _M0L6_2atmpS3062;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS726,
  int32_t _M0L3idxS731,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS730
) {
  int32_t _M0L3pslS2953;
  int32_t _M0L6_2atmpS2949;
  int32_t _M0L6_2atmpS2951;
  int32_t _M0L14capacity__maskS2952;
  int32_t _M0L6_2atmpS2950;
  int32_t _M0L3pslS722;
  int32_t _M0L3idxS723;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS724;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2953 = _M0L5entryS730->$2;
  _M0L6_2atmpS2949 = _M0L3pslS2953 + 1;
  _M0L6_2atmpS2951 = _M0L3idxS731 + 1;
  _M0L14capacity__maskS2952 = _M0L4selfS726->$3;
  _M0L6_2atmpS2950 = _M0L6_2atmpS2951 & _M0L14capacity__maskS2952;
  moonbit_incref(_M0L5entryS730);
  _M0L3pslS722 = _M0L6_2atmpS2949;
  _M0L3idxS723 = _M0L6_2atmpS2950;
  _M0L5entryS724 = _M0L5entryS730;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2948 =
      _M0L4selfS726->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS725;
    if (
      _M0L3idxS723 < 0
      || _M0L3idxS723 >= Moonbit_array_length(_M0L7entriesS2948)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS725
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2948[
        _M0L3idxS723
      ];
    if (_M0L7_2abindS725 == 0) {
      _M0L5entryS724->$2 = _M0L3pslS722;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS726, _M0L5entryS724, _M0L3idxS723);
      moonbit_decref(_M0L5entryS724);
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS728 =
        _M0L7_2abindS725;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS729 =
        _M0L7_2aSomeS728;
      int32_t _M0L3pslS2938 = _M0L14_2acurr__entryS729->$2;
      if (_M0L3pslS722 > _M0L3pslS2938) {
        int32_t _M0L3pslS2943;
        int32_t _M0L6_2atmpS2939;
        int32_t _M0L6_2atmpS2941;
        int32_t _M0L14capacity__maskS2942;
        int32_t _M0L6_2atmpS2940;
        _M0L5entryS724->$2 = _M0L3pslS722;
        moonbit_incref(_M0L14_2acurr__entryS729);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS726, _M0L5entryS724, _M0L3idxS723);
        moonbit_decref(_M0L5entryS724);
        _M0L3pslS2943 = _M0L14_2acurr__entryS729->$2;
        _M0L6_2atmpS2939 = _M0L3pslS2943 + 1;
        _M0L6_2atmpS2941 = _M0L3idxS723 + 1;
        _M0L14capacity__maskS2942 = _M0L4selfS726->$3;
        _M0L6_2atmpS2940 = _M0L6_2atmpS2941 & _M0L14capacity__maskS2942;
        _M0L3pslS722 = _M0L6_2atmpS2939;
        _M0L3idxS723 = _M0L6_2atmpS2940;
        _M0L5entryS724 = _M0L14_2acurr__entryS729;
        continue;
      } else {
        int32_t _M0L6_2atmpS2944 = _M0L3pslS722 + 1;
        int32_t _M0L6_2atmpS2946 = _M0L3idxS723 + 1;
        int32_t _M0L14capacity__maskS2947 = _M0L4selfS726->$3;
        int32_t _M0L6_2atmpS2945 =
          _M0L6_2atmpS2946 & _M0L14capacity__maskS2947;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _tmp_5487 =
          _M0L5entryS724;
        _M0L3pslS722 = _M0L6_2atmpS2944;
        _M0L3idxS723 = _M0L6_2atmpS2945;
        _M0L5entryS724 = _tmp_5487;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS736,
  int32_t _M0L3idxS741,
  struct _M0TPB5EntryGsiE* _M0L5entryS740
) {
  int32_t _M0L3pslS2969;
  int32_t _M0L6_2atmpS2965;
  int32_t _M0L6_2atmpS2967;
  int32_t _M0L14capacity__maskS2968;
  int32_t _M0L6_2atmpS2966;
  int32_t _M0L3pslS732;
  int32_t _M0L3idxS733;
  struct _M0TPB5EntryGsiE* _M0L5entryS734;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2969 = _M0L5entryS740->$2;
  _M0L6_2atmpS2965 = _M0L3pslS2969 + 1;
  _M0L6_2atmpS2967 = _M0L3idxS741 + 1;
  _M0L14capacity__maskS2968 = _M0L4selfS736->$3;
  _M0L6_2atmpS2966 = _M0L6_2atmpS2967 & _M0L14capacity__maskS2968;
  moonbit_incref(_M0L5entryS740);
  _M0L3pslS732 = _M0L6_2atmpS2965;
  _M0L3idxS733 = _M0L6_2atmpS2966;
  _M0L5entryS734 = _M0L5entryS740;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2964 = _M0L4selfS736->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS735;
    if (
      _M0L3idxS733 < 0
      || _M0L3idxS733 >= Moonbit_array_length(_M0L7entriesS2964)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS735
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2964[_M0L3idxS733];
    if (_M0L7_2abindS735 == 0) {
      _M0L5entryS734->$2 = _M0L3pslS732;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS736, _M0L5entryS734, _M0L3idxS733);
      moonbit_decref(_M0L5entryS734);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS738 = _M0L7_2abindS735;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS739 = _M0L7_2aSomeS738;
      int32_t _M0L3pslS2954 = _M0L14_2acurr__entryS739->$2;
      if (_M0L3pslS732 > _M0L3pslS2954) {
        int32_t _M0L3pslS2959;
        int32_t _M0L6_2atmpS2955;
        int32_t _M0L6_2atmpS2957;
        int32_t _M0L14capacity__maskS2958;
        int32_t _M0L6_2atmpS2956;
        _M0L5entryS734->$2 = _M0L3pslS732;
        moonbit_incref(_M0L14_2acurr__entryS739);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS736, _M0L5entryS734, _M0L3idxS733);
        moonbit_decref(_M0L5entryS734);
        _M0L3pslS2959 = _M0L14_2acurr__entryS739->$2;
        _M0L6_2atmpS2955 = _M0L3pslS2959 + 1;
        _M0L6_2atmpS2957 = _M0L3idxS733 + 1;
        _M0L14capacity__maskS2958 = _M0L4selfS736->$3;
        _M0L6_2atmpS2956 = _M0L6_2atmpS2957 & _M0L14capacity__maskS2958;
        _M0L3pslS732 = _M0L6_2atmpS2955;
        _M0L3idxS733 = _M0L6_2atmpS2956;
        _M0L5entryS734 = _M0L14_2acurr__entryS739;
        continue;
      } else {
        int32_t _M0L6_2atmpS2960 = _M0L3pslS732 + 1;
        int32_t _M0L6_2atmpS2962 = _M0L3idxS733 + 1;
        int32_t _M0L14capacity__maskS2963 = _M0L4selfS736->$3;
        int32_t _M0L6_2atmpS2961 =
          _M0L6_2atmpS2962 & _M0L14capacity__maskS2963;
        struct _M0TPB5EntryGsiE* _tmp_5489 = _M0L5entryS734;
        _M0L3pslS732 = _M0L6_2atmpS2960;
        _M0L3idxS733 = _M0L6_2atmpS2961;
        _M0L5entryS734 = _tmp_5489;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE* _M0L4selfS746,
  int32_t _M0L3idxS751,
  struct _M0TPB5EntryGssE* _M0L5entryS750
) {
  int32_t _M0L3pslS2985;
  int32_t _M0L6_2atmpS2981;
  int32_t _M0L6_2atmpS2983;
  int32_t _M0L14capacity__maskS2984;
  int32_t _M0L6_2atmpS2982;
  int32_t _M0L3pslS742;
  int32_t _M0L3idxS743;
  struct _M0TPB5EntryGssE* _M0L5entryS744;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2985 = _M0L5entryS750->$2;
  _M0L6_2atmpS2981 = _M0L3pslS2985 + 1;
  _M0L6_2atmpS2983 = _M0L3idxS751 + 1;
  _M0L14capacity__maskS2984 = _M0L4selfS746->$3;
  _M0L6_2atmpS2982 = _M0L6_2atmpS2983 & _M0L14capacity__maskS2984;
  moonbit_incref(_M0L5entryS750);
  _M0L3pslS742 = _M0L6_2atmpS2981;
  _M0L3idxS743 = _M0L6_2atmpS2982;
  _M0L5entryS744 = _M0L5entryS750;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2980 = _M0L4selfS746->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS745;
    if (
      _M0L3idxS743 < 0
      || _M0L3idxS743 >= Moonbit_array_length(_M0L7entriesS2980)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS745
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2980[_M0L3idxS743];
    if (_M0L7_2abindS745 == 0) {
      _M0L5entryS744->$2 = _M0L3pslS742;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS746, _M0L5entryS744, _M0L3idxS743);
      moonbit_decref(_M0L5entryS744);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS748 = _M0L7_2abindS745;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS749 = _M0L7_2aSomeS748;
      int32_t _M0L3pslS2970 = _M0L14_2acurr__entryS749->$2;
      if (_M0L3pslS742 > _M0L3pslS2970) {
        int32_t _M0L3pslS2975;
        int32_t _M0L6_2atmpS2971;
        int32_t _M0L6_2atmpS2973;
        int32_t _M0L14capacity__maskS2974;
        int32_t _M0L6_2atmpS2972;
        _M0L5entryS744->$2 = _M0L3pslS742;
        moonbit_incref(_M0L14_2acurr__entryS749);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS746, _M0L5entryS744, _M0L3idxS743);
        moonbit_decref(_M0L5entryS744);
        _M0L3pslS2975 = _M0L14_2acurr__entryS749->$2;
        _M0L6_2atmpS2971 = _M0L3pslS2975 + 1;
        _M0L6_2atmpS2973 = _M0L3idxS743 + 1;
        _M0L14capacity__maskS2974 = _M0L4selfS746->$3;
        _M0L6_2atmpS2972 = _M0L6_2atmpS2973 & _M0L14capacity__maskS2974;
        _M0L3pslS742 = _M0L6_2atmpS2971;
        _M0L3idxS743 = _M0L6_2atmpS2972;
        _M0L5entryS744 = _M0L14_2acurr__entryS749;
        continue;
      } else {
        int32_t _M0L6_2atmpS2976 = _M0L3pslS742 + 1;
        int32_t _M0L6_2atmpS2978 = _M0L3idxS743 + 1;
        int32_t _M0L14capacity__maskS2979 = _M0L4selfS746->$3;
        int32_t _M0L6_2atmpS2977 =
          _M0L6_2atmpS2978 & _M0L14capacity__maskS2979;
        struct _M0TPB5EntryGssE* _tmp_5491 = _M0L5entryS744;
        _M0L3pslS742 = _M0L6_2atmpS2976;
        _M0L3idxS743 = _M0L6_2atmpS2977;
        _M0L5entryS744 = _tmp_5491;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS756,
  int32_t _M0L3idxS761,
  struct _M0TPB5EntryGsbE* _M0L5entryS760
) {
  int32_t _M0L3pslS3001;
  int32_t _M0L6_2atmpS2997;
  int32_t _M0L6_2atmpS2999;
  int32_t _M0L14capacity__maskS3000;
  int32_t _M0L6_2atmpS2998;
  int32_t _M0L3pslS752;
  int32_t _M0L3idxS753;
  struct _M0TPB5EntryGsbE* _M0L5entryS754;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS3001 = _M0L5entryS760->$2;
  _M0L6_2atmpS2997 = _M0L3pslS3001 + 1;
  _M0L6_2atmpS2999 = _M0L3idxS761 + 1;
  _M0L14capacity__maskS3000 = _M0L4selfS756->$3;
  _M0L6_2atmpS2998 = _M0L6_2atmpS2999 & _M0L14capacity__maskS3000;
  moonbit_incref(_M0L5entryS760);
  _M0L3pslS752 = _M0L6_2atmpS2997;
  _M0L3idxS753 = _M0L6_2atmpS2998;
  _M0L5entryS754 = _M0L5entryS760;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2996 = _M0L4selfS756->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS755;
    if (
      _M0L3idxS753 < 0
      || _M0L3idxS753 >= Moonbit_array_length(_M0L7entriesS2996)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS755
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2996[_M0L3idxS753];
    if (_M0L7_2abindS755 == 0) {
      _M0L5entryS754->$2 = _M0L3pslS752;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS756, _M0L5entryS754, _M0L3idxS753);
      moonbit_decref(_M0L5entryS754);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS758 = _M0L7_2abindS755;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS759 = _M0L7_2aSomeS758;
      int32_t _M0L3pslS2986 = _M0L14_2acurr__entryS759->$2;
      if (_M0L3pslS752 > _M0L3pslS2986) {
        int32_t _M0L3pslS2991;
        int32_t _M0L6_2atmpS2987;
        int32_t _M0L6_2atmpS2989;
        int32_t _M0L14capacity__maskS2990;
        int32_t _M0L6_2atmpS2988;
        _M0L5entryS754->$2 = _M0L3pslS752;
        moonbit_incref(_M0L14_2acurr__entryS759);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS756, _M0L5entryS754, _M0L3idxS753);
        moonbit_decref(_M0L5entryS754);
        _M0L3pslS2991 = _M0L14_2acurr__entryS759->$2;
        _M0L6_2atmpS2987 = _M0L3pslS2991 + 1;
        _M0L6_2atmpS2989 = _M0L3idxS753 + 1;
        _M0L14capacity__maskS2990 = _M0L4selfS756->$3;
        _M0L6_2atmpS2988 = _M0L6_2atmpS2989 & _M0L14capacity__maskS2990;
        _M0L3pslS752 = _M0L6_2atmpS2987;
        _M0L3idxS753 = _M0L6_2atmpS2988;
        _M0L5entryS754 = _M0L14_2acurr__entryS759;
        continue;
      } else {
        int32_t _M0L6_2atmpS2992 = _M0L3pslS752 + 1;
        int32_t _M0L6_2atmpS2994 = _M0L3idxS753 + 1;
        int32_t _M0L14capacity__maskS2995 = _M0L4selfS756->$3;
        int32_t _M0L6_2atmpS2993 =
          _M0L6_2atmpS2994 & _M0L14capacity__maskS2995;
        struct _M0TPB5EntryGsbE* _tmp_5493 = _M0L5entryS754;
        _M0L3pslS752 = _M0L6_2atmpS2992;
        _M0L3idxS753 = _M0L6_2atmpS2993;
        _M0L5entryS754 = _tmp_5493;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS766,
  int32_t _M0L3idxS771,
  struct _M0TPB5EntryGsfE* _M0L5entryS770
) {
  int32_t _M0L3pslS3017;
  int32_t _M0L6_2atmpS3013;
  int32_t _M0L6_2atmpS3015;
  int32_t _M0L14capacity__maskS3016;
  int32_t _M0L6_2atmpS3014;
  int32_t _M0L3pslS762;
  int32_t _M0L3idxS763;
  struct _M0TPB5EntryGsfE* _M0L5entryS764;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS3017 = _M0L5entryS770->$2;
  _M0L6_2atmpS3013 = _M0L3pslS3017 + 1;
  _M0L6_2atmpS3015 = _M0L3idxS771 + 1;
  _M0L14capacity__maskS3016 = _M0L4selfS766->$3;
  _M0L6_2atmpS3014 = _M0L6_2atmpS3015 & _M0L14capacity__maskS3016;
  moonbit_incref(_M0L5entryS770);
  _M0L3pslS762 = _M0L6_2atmpS3013;
  _M0L3idxS763 = _M0L6_2atmpS3014;
  _M0L5entryS764 = _M0L5entryS770;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3012 = _M0L4selfS766->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS765;
    if (
      _M0L3idxS763 < 0
      || _M0L3idxS763 >= Moonbit_array_length(_M0L7entriesS3012)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS765
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3012[_M0L3idxS763];
    if (_M0L7_2abindS765 == 0) {
      _M0L5entryS764->$2 = _M0L3pslS762;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS766, _M0L5entryS764, _M0L3idxS763);
      moonbit_decref(_M0L5entryS764);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS768 = _M0L7_2abindS765;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS769 = _M0L7_2aSomeS768;
      int32_t _M0L3pslS3002 = _M0L14_2acurr__entryS769->$2;
      if (_M0L3pslS762 > _M0L3pslS3002) {
        int32_t _M0L3pslS3007;
        int32_t _M0L6_2atmpS3003;
        int32_t _M0L6_2atmpS3005;
        int32_t _M0L14capacity__maskS3006;
        int32_t _M0L6_2atmpS3004;
        _M0L5entryS764->$2 = _M0L3pslS762;
        moonbit_incref(_M0L14_2acurr__entryS769);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS766, _M0L5entryS764, _M0L3idxS763);
        moonbit_decref(_M0L5entryS764);
        _M0L3pslS3007 = _M0L14_2acurr__entryS769->$2;
        _M0L6_2atmpS3003 = _M0L3pslS3007 + 1;
        _M0L6_2atmpS3005 = _M0L3idxS763 + 1;
        _M0L14capacity__maskS3006 = _M0L4selfS766->$3;
        _M0L6_2atmpS3004 = _M0L6_2atmpS3005 & _M0L14capacity__maskS3006;
        _M0L3pslS762 = _M0L6_2atmpS3003;
        _M0L3idxS763 = _M0L6_2atmpS3004;
        _M0L5entryS764 = _M0L14_2acurr__entryS769;
        continue;
      } else {
        int32_t _M0L6_2atmpS3008 = _M0L3pslS762 + 1;
        int32_t _M0L6_2atmpS3010 = _M0L3idxS763 + 1;
        int32_t _M0L14capacity__maskS3011 = _M0L4selfS766->$3;
        int32_t _M0L6_2atmpS3009 =
          _M0L6_2atmpS3010 & _M0L14capacity__maskS3011;
        struct _M0TPB5EntryGsfE* _tmp_5495 = _M0L5entryS764;
        _M0L3pslS762 = _M0L6_2atmpS3008;
        _M0L3idxS763 = _M0L6_2atmpS3009;
        _M0L5entryS764 = _tmp_5495;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS692,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS694,
  int32_t _M0L8new__idxS693
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2928;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2929;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4897;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS695;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2928 = _M0L4selfS692->$0;
  _M0L6_2atmpS2929 = _M0L5entryS694;
  if (
    _M0L8new__idxS693 < 0
    || _M0L8new__idxS693 >= Moonbit_array_length(_M0L7entriesS2928)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4897
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2928[
      _M0L8new__idxS693
    ];
  if (_M0L6_2atmpS2929) {
    moonbit_incref(_M0L6_2atmpS2929);
  }
  if (_M0L6_2aoldS4897) {
    moonbit_decref(_M0L6_2aoldS4897);
  }
  _M0L7entriesS2928[_M0L8new__idxS693] = _M0L6_2atmpS2929;
  _M0L7_2abindS695 = _M0L5entryS694->$1;
  if (_M0L7_2abindS695 == 0) {
    _M0L4selfS692->$6 = _M0L8new__idxS693;
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS696 =
      _M0L7_2abindS695;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS697 =
      _M0L7_2aSomeS696;
    _M0L7_2anextS697->$0 = _M0L8new__idxS693;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS698,
  struct _M0TPB5EntryGsiE* _M0L5entryS700,
  int32_t _M0L8new__idxS699
) {
  struct _M0TPB5EntryGsiE** _M0L7entriesS2930;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2931;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4900;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS701;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2930 = _M0L4selfS698->$0;
  _M0L6_2atmpS2931 = _M0L5entryS700;
  if (
    _M0L8new__idxS699 < 0
    || _M0L8new__idxS699 >= Moonbit_array_length(_M0L7entriesS2930)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4900
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2930[_M0L8new__idxS699];
  if (_M0L6_2atmpS2931) {
    moonbit_incref(_M0L6_2atmpS2931);
  }
  if (_M0L6_2aoldS4900) {
    moonbit_decref(_M0L6_2aoldS4900);
  }
  _M0L7entriesS2930[_M0L8new__idxS699] = _M0L6_2atmpS2931;
  _M0L7_2abindS701 = _M0L5entryS700->$1;
  if (_M0L7_2abindS701 == 0) {
    _M0L4selfS698->$6 = _M0L8new__idxS699;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS702 = _M0L7_2abindS701;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS703 = _M0L7_2aSomeS702;
    _M0L7_2anextS703->$0 = _M0L8new__idxS699;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS704,
  struct _M0TPB5EntryGssE* _M0L5entryS706,
  int32_t _M0L8new__idxS705
) {
  struct _M0TPB5EntryGssE** _M0L7entriesS2932;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2933;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4903;
  struct _M0TPB5EntryGssE* _M0L7_2abindS707;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2932 = _M0L4selfS704->$0;
  _M0L6_2atmpS2933 = _M0L5entryS706;
  if (
    _M0L8new__idxS705 < 0
    || _M0L8new__idxS705 >= Moonbit_array_length(_M0L7entriesS2932)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4903
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2932[_M0L8new__idxS705];
  if (_M0L6_2atmpS2933) {
    moonbit_incref(_M0L6_2atmpS2933);
  }
  if (_M0L6_2aoldS4903) {
    moonbit_decref(_M0L6_2aoldS4903);
  }
  _M0L7entriesS2932[_M0L8new__idxS705] = _M0L6_2atmpS2933;
  _M0L7_2abindS707 = _M0L5entryS706->$1;
  if (_M0L7_2abindS707 == 0) {
    _M0L4selfS704->$6 = _M0L8new__idxS705;
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS708 = _M0L7_2abindS707;
    struct _M0TPB5EntryGssE* _M0L7_2anextS709 = _M0L7_2aSomeS708;
    _M0L7_2anextS709->$0 = _M0L8new__idxS705;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS710,
  struct _M0TPB5EntryGsbE* _M0L5entryS712,
  int32_t _M0L8new__idxS711
) {
  struct _M0TPB5EntryGsbE** _M0L7entriesS2934;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2935;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4906;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS713;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2934 = _M0L4selfS710->$0;
  _M0L6_2atmpS2935 = _M0L5entryS712;
  if (
    _M0L8new__idxS711 < 0
    || _M0L8new__idxS711 >= Moonbit_array_length(_M0L7entriesS2934)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4906
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2934[_M0L8new__idxS711];
  if (_M0L6_2atmpS2935) {
    moonbit_incref(_M0L6_2atmpS2935);
  }
  if (_M0L6_2aoldS4906) {
    moonbit_decref(_M0L6_2aoldS4906);
  }
  _M0L7entriesS2934[_M0L8new__idxS711] = _M0L6_2atmpS2935;
  _M0L7_2abindS713 = _M0L5entryS712->$1;
  if (_M0L7_2abindS713 == 0) {
    _M0L4selfS710->$6 = _M0L8new__idxS711;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS714 = _M0L7_2abindS713;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS715 = _M0L7_2aSomeS714;
    _M0L7_2anextS715->$0 = _M0L8new__idxS711;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS716,
  struct _M0TPB5EntryGsfE* _M0L5entryS718,
  int32_t _M0L8new__idxS717
) {
  struct _M0TPB5EntryGsfE** _M0L7entriesS2936;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2937;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4909;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS719;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2936 = _M0L4selfS716->$0;
  _M0L6_2atmpS2937 = _M0L5entryS718;
  if (
    _M0L8new__idxS717 < 0
    || _M0L8new__idxS717 >= Moonbit_array_length(_M0L7entriesS2936)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4909
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2936[_M0L8new__idxS717];
  if (_M0L6_2atmpS2937) {
    moonbit_incref(_M0L6_2atmpS2937);
  }
  if (_M0L6_2aoldS4909) {
    moonbit_decref(_M0L6_2aoldS4909);
  }
  _M0L7entriesS2936[_M0L8new__idxS717] = _M0L6_2atmpS2937;
  _M0L7_2abindS719 = _M0L5entryS718->$1;
  if (_M0L7_2abindS719 == 0) {
    _M0L4selfS716->$6 = _M0L8new__idxS717;
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS720 = _M0L7_2abindS719;
    struct _M0TPB5EntryGsfE* _M0L7_2anextS721 = _M0L7_2aSomeS720;
    _M0L7_2anextS721->$0 = _M0L8new__idxS717;
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS673,
  int32_t _M0L3idxS675,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS674
) {
  int32_t _M0L7_2abindS672;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2888;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2889;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4911;
  int32_t _M0L4sizeS2891;
  int32_t _M0L6_2atmpS2890;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS672 = _M0L4selfS673->$6;
  switch (_M0L7_2abindS672) {
    case -1: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2883 =
        _M0L5entryS674;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4913 =
        _M0L4selfS673->$5;
      if (_M0L6_2atmpS2883) {
        moonbit_incref(_M0L6_2atmpS2883);
      }
      if (_M0L6_2aoldS4913) {
        moonbit_decref(_M0L6_2aoldS4913);
      }
      _M0L4selfS673->$5 = _M0L6_2atmpS2883;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2887 =
        _M0L4selfS673->$0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2886;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2884;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2885;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS4914;
      if (
        _M0L7_2abindS672 < 0
        || _M0L7_2abindS672 >= Moonbit_array_length(_M0L7entriesS2887)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2886
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2887[
          _M0L7_2abindS672
        ];
      if (_M0L6_2atmpS2886) {
        moonbit_incref(_M0L6_2atmpS2886);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2884
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2886);
      if (_M0L6_2atmpS2886) {
        moonbit_decref(_M0L6_2atmpS2886);
      }
      _M0L6_2atmpS2885 = _M0L5entryS674;
      _M0L6_2aoldS4914 = _M0L6_2atmpS2884->$1;
      if (_M0L6_2atmpS2885) {
        moonbit_incref(_M0L6_2atmpS2885);
      }
      if (_M0L6_2aoldS4914) {
        moonbit_decref(_M0L6_2aoldS4914);
      }
      _M0L6_2atmpS2884->$1 = _M0L6_2atmpS2885;
      moonbit_decref(_M0L6_2atmpS2884);
      break;
    }
  }
  _M0L4selfS673->$6 = _M0L3idxS675;
  _M0L7entriesS2888 = _M0L4selfS673->$0;
  _M0L6_2atmpS2889 = _M0L5entryS674;
  if (
    _M0L3idxS675 < 0
    || _M0L3idxS675 >= Moonbit_array_length(_M0L7entriesS2888)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4911
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2888[
      _M0L3idxS675
    ];
  if (_M0L6_2atmpS2889) {
    moonbit_incref(_M0L6_2atmpS2889);
  }
  if (_M0L6_2aoldS4911) {
    moonbit_decref(_M0L6_2aoldS4911);
  }
  _M0L7entriesS2888[_M0L3idxS675] = _M0L6_2atmpS2889;
  _M0L4sizeS2891 = _M0L4selfS673->$1;
  _M0L6_2atmpS2890 = _M0L4sizeS2891 + 1;
  _M0L4selfS673->$1 = _M0L6_2atmpS2890;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS677,
  int32_t _M0L3idxS679,
  struct _M0TPB5EntryGsiE* _M0L5entryS678
) {
  int32_t _M0L7_2abindS676;
  struct _M0TPB5EntryGsiE** _M0L7entriesS2897;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2898;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4917;
  int32_t _M0L4sizeS2900;
  int32_t _M0L6_2atmpS2899;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS676 = _M0L4selfS677->$6;
  switch (_M0L7_2abindS676) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2892 = _M0L5entryS678;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4919 = _M0L4selfS677->$5;
      if (_M0L6_2atmpS2892) {
        moonbit_incref(_M0L6_2atmpS2892);
      }
      if (_M0L6_2aoldS4919) {
        moonbit_decref(_M0L6_2aoldS4919);
      }
      _M0L4selfS677->$5 = _M0L6_2atmpS2892;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2896 = _M0L4selfS677->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2895;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2893;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2894;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4920;
      if (
        _M0L7_2abindS676 < 0
        || _M0L7_2abindS676 >= Moonbit_array_length(_M0L7entriesS2896)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2895
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2896[_M0L7_2abindS676];
      if (_M0L6_2atmpS2895) {
        moonbit_incref(_M0L6_2atmpS2895);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2893
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2895);
      if (_M0L6_2atmpS2895) {
        moonbit_decref(_M0L6_2atmpS2895);
      }
      _M0L6_2atmpS2894 = _M0L5entryS678;
      _M0L6_2aoldS4920 = _M0L6_2atmpS2893->$1;
      if (_M0L6_2atmpS2894) {
        moonbit_incref(_M0L6_2atmpS2894);
      }
      if (_M0L6_2aoldS4920) {
        moonbit_decref(_M0L6_2aoldS4920);
      }
      _M0L6_2atmpS2893->$1 = _M0L6_2atmpS2894;
      moonbit_decref(_M0L6_2atmpS2893);
      break;
    }
  }
  _M0L4selfS677->$6 = _M0L3idxS679;
  _M0L7entriesS2897 = _M0L4selfS677->$0;
  _M0L6_2atmpS2898 = _M0L5entryS678;
  if (
    _M0L3idxS679 < 0
    || _M0L3idxS679 >= Moonbit_array_length(_M0L7entriesS2897)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4917
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2897[_M0L3idxS679];
  if (_M0L6_2atmpS2898) {
    moonbit_incref(_M0L6_2atmpS2898);
  }
  if (_M0L6_2aoldS4917) {
    moonbit_decref(_M0L6_2aoldS4917);
  }
  _M0L7entriesS2897[_M0L3idxS679] = _M0L6_2atmpS2898;
  _M0L4sizeS2900 = _M0L4selfS677->$1;
  _M0L6_2atmpS2899 = _M0L4sizeS2900 + 1;
  _M0L4selfS677->$1 = _M0L6_2atmpS2899;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS681,
  int32_t _M0L3idxS683,
  struct _M0TPB5EntryGssE* _M0L5entryS682
) {
  int32_t _M0L7_2abindS680;
  struct _M0TPB5EntryGssE** _M0L7entriesS2906;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2907;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4923;
  int32_t _M0L4sizeS2909;
  int32_t _M0L6_2atmpS2908;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS680 = _M0L4selfS681->$6;
  switch (_M0L7_2abindS680) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2901 = _M0L5entryS682;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4925 = _M0L4selfS681->$5;
      if (_M0L6_2atmpS2901) {
        moonbit_incref(_M0L6_2atmpS2901);
      }
      if (_M0L6_2aoldS4925) {
        moonbit_decref(_M0L6_2aoldS4925);
      }
      _M0L4selfS681->$5 = _M0L6_2atmpS2901;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS2905 = _M0L4selfS681->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2904;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2902;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2903;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4926;
      if (
        _M0L7_2abindS680 < 0
        || _M0L7_2abindS680 >= Moonbit_array_length(_M0L7entriesS2905)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2904
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS2905[_M0L7_2abindS680];
      if (_M0L6_2atmpS2904) {
        moonbit_incref(_M0L6_2atmpS2904);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2902
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS2904);
      if (_M0L6_2atmpS2904) {
        moonbit_decref(_M0L6_2atmpS2904);
      }
      _M0L6_2atmpS2903 = _M0L5entryS682;
      _M0L6_2aoldS4926 = _M0L6_2atmpS2902->$1;
      if (_M0L6_2atmpS2903) {
        moonbit_incref(_M0L6_2atmpS2903);
      }
      if (_M0L6_2aoldS4926) {
        moonbit_decref(_M0L6_2aoldS4926);
      }
      _M0L6_2atmpS2902->$1 = _M0L6_2atmpS2903;
      moonbit_decref(_M0L6_2atmpS2902);
      break;
    }
  }
  _M0L4selfS681->$6 = _M0L3idxS683;
  _M0L7entriesS2906 = _M0L4selfS681->$0;
  _M0L6_2atmpS2907 = _M0L5entryS682;
  if (
    _M0L3idxS683 < 0
    || _M0L3idxS683 >= Moonbit_array_length(_M0L7entriesS2906)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4923
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2906[_M0L3idxS683];
  if (_M0L6_2atmpS2907) {
    moonbit_incref(_M0L6_2atmpS2907);
  }
  if (_M0L6_2aoldS4923) {
    moonbit_decref(_M0L6_2aoldS4923);
  }
  _M0L7entriesS2906[_M0L3idxS683] = _M0L6_2atmpS2907;
  _M0L4sizeS2909 = _M0L4selfS681->$1;
  _M0L6_2atmpS2908 = _M0L4sizeS2909 + 1;
  _M0L4selfS681->$1 = _M0L6_2atmpS2908;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS685,
  int32_t _M0L3idxS687,
  struct _M0TPB5EntryGsbE* _M0L5entryS686
) {
  int32_t _M0L7_2abindS684;
  struct _M0TPB5EntryGsbE** _M0L7entriesS2915;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2916;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4929;
  int32_t _M0L4sizeS2918;
  int32_t _M0L6_2atmpS2917;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS684 = _M0L4selfS685->$6;
  switch (_M0L7_2abindS684) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2910 = _M0L5entryS686;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4931 = _M0L4selfS685->$5;
      if (_M0L6_2atmpS2910) {
        moonbit_incref(_M0L6_2atmpS2910);
      }
      if (_M0L6_2aoldS4931) {
        moonbit_decref(_M0L6_2aoldS4931);
      }
      _M0L4selfS685->$5 = _M0L6_2atmpS2910;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2914 = _M0L4selfS685->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2913;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2911;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2912;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4932;
      if (
        _M0L7_2abindS684 < 0
        || _M0L7_2abindS684 >= Moonbit_array_length(_M0L7entriesS2914)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2913
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2914[_M0L7_2abindS684];
      if (_M0L6_2atmpS2913) {
        moonbit_incref(_M0L6_2atmpS2913);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2911
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2913);
      if (_M0L6_2atmpS2913) {
        moonbit_decref(_M0L6_2atmpS2913);
      }
      _M0L6_2atmpS2912 = _M0L5entryS686;
      _M0L6_2aoldS4932 = _M0L6_2atmpS2911->$1;
      if (_M0L6_2atmpS2912) {
        moonbit_incref(_M0L6_2atmpS2912);
      }
      if (_M0L6_2aoldS4932) {
        moonbit_decref(_M0L6_2aoldS4932);
      }
      _M0L6_2atmpS2911->$1 = _M0L6_2atmpS2912;
      moonbit_decref(_M0L6_2atmpS2911);
      break;
    }
  }
  _M0L4selfS685->$6 = _M0L3idxS687;
  _M0L7entriesS2915 = _M0L4selfS685->$0;
  _M0L6_2atmpS2916 = _M0L5entryS686;
  if (
    _M0L3idxS687 < 0
    || _M0L3idxS687 >= Moonbit_array_length(_M0L7entriesS2915)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4929
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2915[_M0L3idxS687];
  if (_M0L6_2atmpS2916) {
    moonbit_incref(_M0L6_2atmpS2916);
  }
  if (_M0L6_2aoldS4929) {
    moonbit_decref(_M0L6_2aoldS4929);
  }
  _M0L7entriesS2915[_M0L3idxS687] = _M0L6_2atmpS2916;
  _M0L4sizeS2918 = _M0L4selfS685->$1;
  _M0L6_2atmpS2917 = _M0L4sizeS2918 + 1;
  _M0L4selfS685->$1 = _M0L6_2atmpS2917;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS689,
  int32_t _M0L3idxS691,
  struct _M0TPB5EntryGsfE* _M0L5entryS690
) {
  int32_t _M0L7_2abindS688;
  struct _M0TPB5EntryGsfE** _M0L7entriesS2924;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2925;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4935;
  int32_t _M0L4sizeS2927;
  int32_t _M0L6_2atmpS2926;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS688 = _M0L4selfS689->$6;
  switch (_M0L7_2abindS688) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2919 = _M0L5entryS690;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4937 = _M0L4selfS689->$5;
      if (_M0L6_2atmpS2919) {
        moonbit_incref(_M0L6_2atmpS2919);
      }
      if (_M0L6_2aoldS4937) {
        moonbit_decref(_M0L6_2aoldS4937);
      }
      _M0L4selfS689->$5 = _M0L6_2atmpS2919;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2923 = _M0L4selfS689->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2922;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2920;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2921;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4938;
      if (
        _M0L7_2abindS688 < 0
        || _M0L7_2abindS688 >= Moonbit_array_length(_M0L7entriesS2923)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2922
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2923[_M0L7_2abindS688];
      if (_M0L6_2atmpS2922) {
        moonbit_incref(_M0L6_2atmpS2922);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2920
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2922);
      if (_M0L6_2atmpS2922) {
        moonbit_decref(_M0L6_2atmpS2922);
      }
      _M0L6_2atmpS2921 = _M0L5entryS690;
      _M0L6_2aoldS4938 = _M0L6_2atmpS2920->$1;
      if (_M0L6_2atmpS2921) {
        moonbit_incref(_M0L6_2atmpS2921);
      }
      if (_M0L6_2aoldS4938) {
        moonbit_decref(_M0L6_2aoldS4938);
      }
      _M0L6_2atmpS2920->$1 = _M0L6_2atmpS2921;
      moonbit_decref(_M0L6_2atmpS2920);
      break;
    }
  }
  _M0L4selfS689->$6 = _M0L3idxS691;
  _M0L7entriesS2924 = _M0L4selfS689->$0;
  _M0L6_2atmpS2925 = _M0L5entryS690;
  if (
    _M0L3idxS691 < 0
    || _M0L3idxS691 >= Moonbit_array_length(_M0L7entriesS2924)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4935
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2924[_M0L3idxS691];
  if (_M0L6_2atmpS2925) {
    moonbit_incref(_M0L6_2atmpS2925);
  }
  if (_M0L6_2aoldS4935) {
    moonbit_decref(_M0L6_2aoldS4935);
  }
  _M0L7entriesS2924[_M0L3idxS691] = _M0L6_2atmpS2925;
  _M0L4sizeS2927 = _M0L4selfS689->$1;
  _M0L6_2atmpS2926 = _M0L4sizeS2927 + 1;
  _M0L4selfS689->$1 = _M0L6_2atmpS2926;
  return 0;
}

int32_t _M0MPC13int3Int3max(int32_t _M0L4selfS670, int32_t _M0L5otherS671) {
  #line 75 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS670 > _M0L5otherS671) {
    return _M0L4selfS670;
  } else {
    return _M0L5otherS671;
  }
}

int32_t _M0FPB21capacity__for__length(int32_t _M0L6lengthS669) {
  int32_t _M0Lm8capacityS668;
  int32_t _M0L6_2atmpS2881;
  int32_t _M0L6_2atmpS2880;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS668 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS669);
  _M0L6_2atmpS2881 = _M0Lm8capacityS668;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2880 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2881);
  if (_M0L6lengthS669 > _M0L6_2atmpS2880) {
    int32_t _M0L6_2atmpS2882 = _M0Lm8capacityS668;
    _M0Lm8capacityS668 = _M0L6_2atmpS2882 * 2;
  }
  return _M0Lm8capacityS668;
}

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  int32_t _M0L8capacityS639
) {
  int32_t _M0L8capacityS638;
  int32_t _M0L7_2abindS640;
  int32_t _M0L7_2abindS641;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2875;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS642;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS643;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _block_5496;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS638
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS639);
  _M0L7_2abindS640 = _M0L8capacityS638 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS641 = _M0FPB21calc__grow__threshold(_M0L8capacityS638);
  _M0L6_2atmpS2875 = 0;
  _M0L7_2abindS642
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS638, _M0L6_2atmpS2875);
  _M0L7_2abindS643 = 0;
  _block_5496
  = (struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
  Moonbit_object_header(_block_5496)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 106, 0);
  _block_5496->$0 = _M0L7_2abindS642;
  _block_5496->$1 = 0;
  _block_5496->$2 = _M0L8capacityS638;
  _block_5496->$3 = _M0L7_2abindS640;
  _block_5496->$4 = _M0L7_2abindS641;
  _block_5496->$5 = _M0L7_2abindS643;
  _block_5496->$6 = -1;
  return _block_5496;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS645) {
  int32_t _M0L8capacityS644;
  int32_t _M0L7_2abindS646;
  int32_t _M0L7_2abindS647;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2876;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS648;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS649;
  struct _M0TPB3MapGsiE* _block_5497;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS644
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS645);
  _M0L7_2abindS646 = _M0L8capacityS644 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS647 = _M0FPB21calc__grow__threshold(_M0L8capacityS644);
  _M0L6_2atmpS2876 = 0;
  _M0L7_2abindS648
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS644, _M0L6_2atmpS2876);
  _M0L7_2abindS649 = 0;
  _block_5497
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_5497)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 110, 0);
  _block_5497->$0 = _M0L7_2abindS648;
  _block_5497->$1 = 0;
  _block_5497->$2 = _M0L8capacityS644;
  _block_5497->$3 = _M0L7_2abindS646;
  _block_5497->$4 = _M0L7_2abindS647;
  _block_5497->$5 = _M0L7_2abindS649;
  _block_5497->$6 = -1;
  return _block_5497;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS651) {
  int32_t _M0L8capacityS650;
  int32_t _M0L7_2abindS652;
  int32_t _M0L7_2abindS653;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2877;
  struct _M0TPB5EntryGssE** _M0L7_2abindS654;
  struct _M0TPB5EntryGssE* _M0L7_2abindS655;
  struct _M0TPB3MapGssE* _block_5498;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS650
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS651);
  _M0L7_2abindS652 = _M0L8capacityS650 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS653 = _M0FPB21calc__grow__threshold(_M0L8capacityS650);
  _M0L6_2atmpS2877 = 0;
  _M0L7_2abindS654
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS650, _M0L6_2atmpS2877);
  _M0L7_2abindS655 = 0;
  _block_5498
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_5498)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 114, 0);
  _block_5498->$0 = _M0L7_2abindS654;
  _block_5498->$1 = 0;
  _block_5498->$2 = _M0L8capacityS650;
  _block_5498->$3 = _M0L7_2abindS652;
  _block_5498->$4 = _M0L7_2abindS653;
  _block_5498->$5 = _M0L7_2abindS655;
  _block_5498->$6 = -1;
  return _block_5498;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS657) {
  int32_t _M0L8capacityS656;
  int32_t _M0L7_2abindS658;
  int32_t _M0L7_2abindS659;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2878;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS660;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS661;
  struct _M0TPB3MapGsbE* _block_5499;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS656
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS657);
  _M0L7_2abindS658 = _M0L8capacityS656 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS659 = _M0FPB21calc__grow__threshold(_M0L8capacityS656);
  _M0L6_2atmpS2878 = 0;
  _M0L7_2abindS660
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS656, _M0L6_2atmpS2878);
  _M0L7_2abindS661 = 0;
  _block_5499
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_5499)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 118, 0);
  _block_5499->$0 = _M0L7_2abindS660;
  _block_5499->$1 = 0;
  _block_5499->$2 = _M0L8capacityS656;
  _block_5499->$3 = _M0L7_2abindS658;
  _block_5499->$4 = _M0L7_2abindS659;
  _block_5499->$5 = _M0L7_2abindS661;
  _block_5499->$6 = -1;
  return _block_5499;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS663) {
  int32_t _M0L8capacityS662;
  int32_t _M0L7_2abindS664;
  int32_t _M0L7_2abindS665;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2879;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS666;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS667;
  struct _M0TPB3MapGsfE* _block_5500;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS662
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS663);
  _M0L7_2abindS664 = _M0L8capacityS662 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS665 = _M0FPB21calc__grow__threshold(_M0L8capacityS662);
  _M0L6_2atmpS2879 = 0;
  _M0L7_2abindS666
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS662, _M0L6_2atmpS2879);
  _M0L7_2abindS667 = 0;
  _block_5500
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_5500)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 122, 0);
  _block_5500->$0 = _M0L7_2abindS666;
  _block_5500->$1 = 0;
  _block_5500->$2 = _M0L8capacityS662;
  _block_5500->$3 = _M0L7_2abindS664;
  _block_5500->$4 = _M0L7_2abindS665;
  _block_5500->$5 = _M0L7_2abindS667;
  _block_5500->$6 = -1;
  return _block_5500;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS637) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS637 >= 0) {
    int32_t _M0L6_2atmpS2874;
    int32_t _M0L6_2atmpS2873;
    int32_t _M0L6_2atmpS2872;
    int32_t _M0L6_2atmpS2871;
    if (_M0L4selfS637 <= 1) {
      return 1;
    }
    if (_M0L4selfS637 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2874 = _M0L4selfS637 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2873 = moonbit_clz32(_M0L6_2atmpS2874);
    _M0L6_2atmpS2872 = _M0L6_2atmpS2873 - 1;
    _M0L6_2atmpS2871 = 2147483647 >> (_M0L6_2atmpS2872 & 31);
    return _M0L6_2atmpS2871 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS636) {
  int32_t _M0L6_2atmpS2870;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2870 = _M0L8capacityS636 * 13;
  return _M0L6_2atmpS2870 / 16;
}

int32_t _M0MPC16option6Option6unwrapGiE(int64_t _M0L4selfS624) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS624 == 4294967296ll) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    int64_t _M0L7_2aSomeS625 = _M0L4selfS624;
    return (int32_t)_M0L7_2aSomeS625;
  }
}

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS626
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS626 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS627 =
      _M0L4selfS626;
    if (_M0L7_2aSomeS627) {
      moonbit_incref(_M0L7_2aSomeS627);
    }
    return _M0L7_2aSomeS627;
  }
}

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE* _M0L4selfS628
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS628 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS629 = _M0L4selfS628;
    if (_M0L7_2aSomeS629) {
      moonbit_incref(_M0L7_2aSomeS629);
    }
    return _M0L7_2aSomeS629;
  }
}

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE* _M0L4selfS630
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS630 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS631 = _M0L4selfS630;
    if (_M0L7_2aSomeS631) {
      moonbit_incref(_M0L7_2aSomeS631);
    }
    return _M0L7_2aSomeS631;
  }
}

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE* _M0L4selfS632
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS632 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS633 = _M0L4selfS632;
    if (_M0L7_2aSomeS633) {
      moonbit_incref(_M0L7_2aSomeS633);
    }
    return _M0L7_2aSomeS633;
  }
}

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE* _M0L4selfS634
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS634 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS635 = _M0L4selfS634;
    if (_M0L7_2aSomeS635) {
      moonbit_incref(_M0L7_2aSomeS635);
    }
    return _M0L7_2aSomeS635;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS598,
  struct _M0TPC16string10StringView _M0L9separatorS611
) {
  int32_t _M0L3endS2845;
  int32_t _M0L5startS2846;
  int32_t _M0L6_2atmpS2844;
  #line 1497 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2845 = _M0L4selfS598.$2;
  _M0L5startS2846 = _M0L4selfS598.$1;
  _M0L6_2atmpS2844 = _M0L3endS2845 - _M0L5startS2846;
  if (_M0L6_2atmpS2844 == 0) {
    return (moonbit_string_t)moonbit_string_literal_75.data;
  } else {
    moonbit_string_t* _M0L3bufS2868 = _M0L4selfS598.$0;
    int32_t _M0L5startS2869 = _M0L4selfS598.$1;
    moonbit_string_t _M0L5_2ahdS599 =
      (moonbit_string_t)_M0L3bufS2868[_M0L5startS2869];
    moonbit_string_t* _M0L9_2ax__bufS600 = _M0L4selfS598.$0;
    int32_t _M0L5startS2867 = _M0L4selfS598.$1;
    int32_t _M0L11_2ax__startS601 = 1 + _M0L5startS2867;
    int32_t _M0L9_2ax__endS602 = _M0L4selfS598.$2;
    struct _M0TPC16string10StringView _M0L2hdS603;
    int32_t _M0L7_2abindS604;
    int32_t _M0L3endS2865;
    int32_t _M0L5startS2866;
    int32_t _M0L6_2atmpS2864;
    int32_t _M0L10size__hintS605;
    int32_t _M0L2__S606;
    int32_t _M0L10size__hintS607;
    int32_t _M0L10size__hintS612;
    struct _M0TPB13StringBuilder* _M0L3bufS613;
    int32_t _M0L3endS2848;
    int32_t _M0L5startS2849;
    int32_t _M0L6_2atmpS2847;
    moonbit_string_t _result_5504;
    moonbit_incref(_M0L9_2ax__bufS600);
    moonbit_incref(_M0L5_2ahdS599);
    #line 1504 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS603
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS599);
    moonbit_decref(_M0L5_2ahdS599);
    _M0L7_2abindS604 = _M0L9_2ax__endS602 - _M0L11_2ax__startS601;
    _M0L3endS2865 = _M0L2hdS603.$2;
    _M0L5startS2866 = _M0L2hdS603.$1;
    _M0L6_2atmpS2864 = _M0L3endS2865 - _M0L5startS2866;
    _M0L2__S606 = 0;
    _M0L10size__hintS607 = _M0L6_2atmpS2864;
    while (1) {
      if (_M0L2__S606 < _M0L7_2abindS604) {
        int32_t _M0L6_2atmpS2863 = _M0L11_2ax__startS601 + _M0L2__S606;
        moonbit_string_t _M0L1sS608 =
          (moonbit_string_t)_M0L9_2ax__bufS600[_M0L6_2atmpS2863];
        int32_t _M0L6_2atmpS2854 = _M0L2__S606 + 1;
        struct _M0TPC16string10StringView _M0L7_2abindS610;
        int32_t _M0L3endS2861;
        int32_t _M0L5startS2862;
        int32_t _M0L6_2atmpS2860;
        int32_t _M0L6_2atmpS2856;
        int32_t _M0L3endS2858;
        int32_t _M0L5startS2859;
        int32_t _M0L6_2atmpS2857;
        int32_t _M0L6_2atmpS2855;
        moonbit_incref(_M0L1sS608);
        #line 1506 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
        _M0L7_2abindS610
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS608);
        moonbit_decref(_M0L1sS608);
        _M0L3endS2861 = _M0L7_2abindS610.$2;
        _M0L5startS2862 = _M0L7_2abindS610.$1;
        moonbit_decref(_M0L7_2abindS610.$0);
        _M0L6_2atmpS2860 = _M0L3endS2861 - _M0L5startS2862;
        _M0L6_2atmpS2856 = _M0L10size__hintS607 + _M0L6_2atmpS2860;
        _M0L3endS2858 = _M0L9separatorS611.$2;
        _M0L5startS2859 = _M0L9separatorS611.$1;
        _M0L6_2atmpS2857 = _M0L3endS2858 - _M0L5startS2859;
        _M0L6_2atmpS2855 = _M0L6_2atmpS2856 + _M0L6_2atmpS2857;
        _M0L2__S606 = _M0L6_2atmpS2854;
        _M0L10size__hintS607 = _M0L6_2atmpS2855;
        continue;
      } else {
        _M0L10size__hintS605 = _M0L10size__hintS607;
      }
      break;
    }
    _M0L10size__hintS612 = _M0L10size__hintS605 << 1;
    #line 1511 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS613
    = _M0MPB13StringBuilder21StringBuilder_2einner(_M0L10size__hintS612);
    #line 1513 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS613, _M0L2hdS603);
    moonbit_decref(_M0L2hdS603.$0);
    _M0L3endS2848 = _M0L9separatorS611.$2;
    _M0L5startS2849 = _M0L9separatorS611.$1;
    _M0L6_2atmpS2847 = _M0L3endS2848 - _M0L5startS2849;
    if (_M0L6_2atmpS2847 == 0) {
      int32_t _M0L7_2abindS614 = _M0L9_2ax__endS602 - _M0L11_2ax__startS601;
      int32_t _M0L2__S615 = 0;
      while (1) {
        if (_M0L2__S615 < _M0L7_2abindS614) {
          int32_t _M0L6_2atmpS2851 = _M0L11_2ax__startS601 + _M0L2__S615;
          moonbit_string_t _M0L1sS616 =
            (moonbit_string_t)_M0L9_2ax__bufS600[_M0L6_2atmpS2851];
          struct _M0TPC16string10StringView _M0L1sS617;
          int32_t _M0L6_2atmpS2850;
          moonbit_incref(_M0L1sS616);
          #line 1517 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS617
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS616);
          moonbit_decref(_M0L1sS616);
          #line 1518 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS613, _M0L1sS617);
          moonbit_decref(_M0L1sS617.$0);
          _M0L6_2atmpS2850 = _M0L2__S615 + 1;
          _M0L2__S615 = _M0L6_2atmpS2850;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS600);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS619 = _M0L9_2ax__endS602 - _M0L11_2ax__startS601;
      int32_t _M0L2__S620 = 0;
      while (1) {
        if (_M0L2__S620 < _M0L7_2abindS619) {
          int32_t _M0L6_2atmpS2853 = _M0L11_2ax__startS601 + _M0L2__S620;
          moonbit_string_t _M0L1sS621 =
            (moonbit_string_t)_M0L9_2ax__bufS600[_M0L6_2atmpS2853];
          struct _M0TPC16string10StringView _M0L1sS622;
          int32_t _M0L6_2atmpS2852;
          moonbit_incref(_M0L1sS621);
          #line 1522 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS622
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS621);
          moonbit_decref(_M0L1sS621);
          #line 1523 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS613, _M0L9separatorS611);
          #line 1525 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS613, _M0L1sS622);
          moonbit_decref(_M0L1sS622.$0);
          _M0L6_2atmpS2852 = _M0L2__S620 + 1;
          _M0L2__S620 = _M0L6_2atmpS2852;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS600);
        }
        break;
      }
    }
    #line 1528 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _result_5504 = _M0MPB13StringBuilder10to__string(_M0L3bufS613);
    moonbit_decref(_M0L3bufS613);
    return _result_5504;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS594,
  int32_t _M0L5indexS595
) {
  uint64_t* _M0L6_2atmpS2842;
  uint64_t _result_5505;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS594);
  _M0L6_2atmpS2842 = _M0L4selfS594;
  if (
    _M0L5indexS595 < 0
    || _M0L5indexS595 >= Moonbit_array_length(_M0L6_2atmpS2842)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_5505 = (uint64_t)_M0L6_2atmpS2842[_M0L5indexS595];
  moonbit_decref(_M0L6_2atmpS2842);
  return _result_5505;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS596,
  int32_t _M0L5indexS597
) {
  uint32_t* _M0L6_2atmpS2843;
  uint32_t _result_5506;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS596);
  _M0L6_2atmpS2843 = _M0L4selfS596;
  if (
    _M0L5indexS597 < 0
    || _M0L5indexS597 >= Moonbit_array_length(_M0L6_2atmpS2843)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_5506 = (uint32_t)_M0L6_2atmpS2843[_M0L5indexS597];
  moonbit_decref(_M0L6_2atmpS2843);
  return _result_5506;
}

moonbit_string_t _M0IPC16uint646UInt64PB4Show10to__string(
  uint64_t _M0L4selfS593
) {
  #line 50 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 51 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS593, 10);
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS592) {
  #line 35 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 36 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS592, 10);
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS591) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS591;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS590
) {
  int32_t _M0L6_2atmpS2841;
  #line 24 "/home/developer/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2841 = Moonbit_array_length(_M0L4selfS590);
  moonbit_incref(_M0L4selfS590);
  return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS590,
                                               .$1 = 0,
                                               .$2 = _M0L6_2atmpS2841};
}

moonbit_string_t _M0MPC16string6String9to__upper(
  moonbit_string_t _M0L4selfS573
) {
  struct _M0TWcEb* _M0L6_2atmpS2838;
  int64_t _M0L7_2abindS572;
  #line 1789 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS2838
  = (struct _M0TWcEb*)&_M0MPC16string6String9to__upperC2839l1791$closure.data;
  #line 1791 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L7_2abindS572
  = _M0MPC16string6String8find__by(_M0L4selfS573, _M0L6_2atmpS2838);
  moonbit_decref(_M0L6_2atmpS2838);
  if (_M0L7_2abindS572 == 4294967296ll) {
    moonbit_incref(_M0L4selfS573);
    return _M0L4selfS573;
  } else {
    int64_t _M0L7_2aSomeS575 = _M0L7_2abindS572;
    int32_t _M0L6_2aidxS576 = (int32_t)_M0L7_2aSomeS575;
    int32_t _M0L6_2atmpS2837 = Moonbit_array_length(_M0L4selfS573);
    struct _M0TPB13StringBuilder* _M0L3bufS577;
    int64_t _M0L6_2atmpS2836;
    struct _M0TPC16string10StringView _M0L4headS578;
    moonbit_string_t _M0L6_2atmpS2807;
    int32_t _M0L6_2atmpS2808;
    int32_t _M0L3endS2810;
    int32_t _M0L5startS2811;
    int32_t _M0L6_2atmpS2809;
    struct _M0TPC16string10StringView _M0L7_2abindS579;
    moonbit_string_t _M0L7_2abindS580;
    int32_t _M0L7_2abindS581;
    int32_t _M0L7_2abindS582;
    int32_t _M0L16_2astring__indexS583;
    moonbit_string_t _result_5512;
    #line 1794 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L3bufS577
    = _M0MPB13StringBuilder21StringBuilder_2einner(_M0L6_2atmpS2837);
    _M0L6_2atmpS2836 = (int64_t)_M0L6_2aidxS576;
    #line 1795 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L4headS578
    = _M0MPC16string6String12view_2einner(_M0L4selfS573, 0, _M0L6_2atmpS2836);
    #line 1796 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS2807 = _M0MPC16string10StringView4data(_M0L4headS578);
    #line 1796 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS2808
    = _M0MPC16string10StringView13start__offset(_M0L4headS578);
    _M0L3endS2810 = _M0L4headS578.$2;
    _M0L5startS2811 = _M0L4headS578.$1;
    moonbit_decref(_M0L4headS578.$0);
    _M0L6_2atmpS2809 = _M0L3endS2810 - _M0L5startS2811;
    #line 1796 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L3bufS577, _M0L6_2atmpS2807, _M0L6_2atmpS2808, _M0L6_2atmpS2809);
    moonbit_decref(_M0L6_2atmpS2807);
    #line 1797 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L7_2abindS579
    = _M0MPC16string6String12view_2einner(_M0L4selfS573, _M0L6_2aidxS576, 4294967296ll);
    _M0L7_2abindS580 = _M0L7_2abindS579.$0;
    _M0L7_2abindS581 = _M0L7_2abindS579.$1;
    _M0L7_2abindS582 = _M0L7_2abindS579.$2;
    _M0L16_2astring__indexS583 = _M0L7_2abindS581;
    while (1) {
      if (_M0L16_2astring__indexS583 < _M0L7_2abindS582) {
        int32_t _M0L31_2adecoded__next__string__indexS585;
        int32_t _M0L16_2adecoded__charS586;
        int32_t _M0L7_2abindS588 =
          _M0L7_2abindS580[_M0L16_2astring__indexS583];
        int32_t _M0L6_2atmpS2817 = (int32_t)_M0L7_2abindS588;
        int32_t _if__result_5509;
        int32_t _if__result_5510;
        if (_M0L6_2atmpS2817 >= 55296) {
          int32_t _M0L6_2atmpS2816 = (int32_t)_M0L7_2abindS588;
          _if__result_5509 = _M0L6_2atmpS2816 <= 56319;
        } else {
          _if__result_5509 = 0;
        }
        if (_if__result_5509) {
          int32_t _M0L6_2atmpS2815 = _M0L16_2astring__indexS583 + 1;
          _if__result_5510 = _M0L6_2atmpS2815 < _M0L7_2abindS582;
        } else {
          _if__result_5510 = 0;
        }
        if (_if__result_5510) {
          int32_t _M0L6_2atmpS2832 = _M0L16_2astring__indexS583 + 1;
          int32_t _M0L7_2abindS589 = _M0L7_2abindS580[_M0L6_2atmpS2832];
          int32_t _M0L6_2atmpS2819 = (int32_t)_M0L7_2abindS589;
          int32_t _if__result_5511;
          if (_M0L6_2atmpS2819 >= 56320) {
            int32_t _M0L6_2atmpS2818 = (int32_t)_M0L7_2abindS589;
            _if__result_5511 = _M0L6_2atmpS2818 <= 57343;
          } else {
            _if__result_5511 = 0;
          }
          if (_if__result_5511) {
            int32_t _M0L6_2atmpS2820 = _M0L16_2astring__indexS583 + 2;
            int32_t _M0L6_2atmpS2828 = (int32_t)_M0L7_2abindS588;
            int32_t _M0L6_2atmpS2827 = _M0L6_2atmpS2828 - 55296;
            int32_t _M0L6_2atmpS2825 = _M0L6_2atmpS2827 * 1024;
            int32_t _M0L6_2atmpS2826 = (int32_t)_M0L7_2abindS589;
            int32_t _M0L6_2atmpS2824 = _M0L6_2atmpS2825 + _M0L6_2atmpS2826;
            int32_t _M0L6_2atmpS2823 = _M0L6_2atmpS2824 - 56320;
            int32_t _M0L6_2atmpS2822 = _M0L6_2atmpS2823 + 65536;
            int32_t _M0L6_2atmpS2821;
            #line 1797 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
            _M0L6_2atmpS2821
            = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2822);
            _M0L31_2adecoded__next__string__indexS585 = _M0L6_2atmpS2820;
            _M0L16_2adecoded__charS586 = _M0L6_2atmpS2821;
            goto join_584;
          } else {
            int32_t _M0L6_2atmpS2829 = _M0L16_2astring__indexS583 + 1;
            int32_t _M0L6_2atmpS2831 = (int32_t)_M0L7_2abindS588;
            int32_t _M0L6_2atmpS2830;
            #line 1797 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
            _M0L6_2atmpS2830
            = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2831);
            _M0L31_2adecoded__next__string__indexS585 = _M0L6_2atmpS2829;
            _M0L16_2adecoded__charS586 = _M0L6_2atmpS2830;
            goto join_584;
          }
        } else {
          int32_t _M0L6_2atmpS2833 = _M0L16_2astring__indexS583 + 1;
          int32_t _M0L6_2atmpS2835 = (int32_t)_M0L7_2abindS588;
          int32_t _M0L6_2atmpS2834;
          #line 1797 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS2834
          = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2835);
          _M0L31_2adecoded__next__string__indexS585 = _M0L6_2atmpS2833;
          _M0L16_2adecoded__charS586 = _M0L6_2atmpS2834;
          goto join_584;
        }
        goto joinlet_5508;
        join_584:;
        #line 1798 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
        if (
          _M0MPC14char4Char20is__ascii__lowercase(_M0L16_2adecoded__charS586)
        ) {
          int32_t _M0L6_2atmpS2814 = _M0L16_2adecoded__charS586;
          int32_t _M0L6_2atmpS2813 = _M0L6_2atmpS2814 - 32;
          int32_t _M0L6_2atmpS2812 = _M0L6_2atmpS2813;
          #line 1799 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS577, _M0L6_2atmpS2812);
        } else {
          #line 1801 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS577, _M0L16_2adecoded__charS586);
        }
        _M0L16_2astring__indexS583
        = _M0L31_2adecoded__next__string__indexS585;
        continue;
        joinlet_5508:;
      } else {
        moonbit_decref(_M0L7_2abindS580);
      }
      break;
    }
    #line 1804 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _result_5512 = _M0MPB13StringBuilder10to__string(_M0L3bufS577);
    moonbit_decref(_M0L3bufS577);
    return _result_5512;
  }
}

int32_t _M0MPC16string6String9to__upperC2839l1791(
  struct _M0TWcEb* _M0L6_2aenvS2840,
  int32_t _M0L1cS574
) {
  #line 1791 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  #line 1791 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  return _M0MPC14char4Char20is__ascii__lowercase(_M0L1cS574);
}

int32_t _M0MPC14char4Char20is__ascii__lowercase(int32_t _M0L4selfS571) {
  #line 103 "/home/developer/.moon/lib/core/builtin/char.mbt"
  return _M0L4selfS571 >= 97 && _M0L4selfS571 <= 122 || 0;
}

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t _M0L4selfS569,
  struct _M0TPC16string10StringView _M0L3sepS570
) {
  int32_t _M0L6_2atmpS2806;
  struct _M0TPC16string10StringView _M0L6_2atmpS2805;
  struct _M0TPB4IterGRPC16string10StringViewE* _result_5513;
  #line 1168 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS2806 = Moonbit_array_length(_M0L4selfS569);
  moonbit_incref(_M0L4selfS569);
  _M0L6_2atmpS2805
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L4selfS569, .$1 = 0, .$2 = _M0L6_2atmpS2806
  };
  #line 1169 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _result_5513
  = _M0MPC16string10StringView5split(_M0L6_2atmpS2805, _M0L3sepS570);
  moonbit_decref(_M0L6_2atmpS2805.$0);
  return _result_5513;
}

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView _M0L4selfS560,
  struct _M0TPC16string10StringView _M0L3sepS559
) {
  int32_t _M0L3endS2803;
  int32_t _M0L5startS2804;
  int32_t _M0L8sep__lenS558;
  void* _M0L4SomeS2802;
  struct _M0TPB8MutLocalGORPC16string10StringViewE* _M0L9remainingS562;
  struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__* _closure_5515;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2792;
  struct _M0TPB4IterGRPC16string10StringViewE* _result_5516;
  #line 1139 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L3endS2803 = _M0L3sepS559.$2;
  _M0L5startS2804 = _M0L3sepS559.$1;
  _M0L8sep__lenS558 = _M0L3endS2803 - _M0L5startS2804;
  if (_M0L8sep__lenS558 == 0) {
    struct _M0TPB4IterGcE* _M0L6_2atmpS2787;
    struct _M0TWcERPC16string10StringView* _M0L6_2atmpS2788;
    struct _M0TPB4IterGRPC16string10StringViewE* _result_5514;
    #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS2787 = _M0MPC16string10StringView4iter(_M0L4selfS560);
    _M0L6_2atmpS2788
    = (struct _M0TWcERPC16string10StringView*)&_M0MPC16string10StringView5splitC2789l1145$closure.data;
    #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _result_5514
    = _M0MPB4Iter3mapGcRPC16string10StringViewE(_M0L6_2atmpS2787, _M0L6_2atmpS2788);
    moonbit_decref(_M0L6_2atmpS2787);
    moonbit_decref(_M0L6_2atmpS2788);
    return _result_5514;
  }
  moonbit_incref(_M0L4selfS560.$0);
  _M0L4SomeS2802
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS2802)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2802)->$0
  = _M0L4selfS560;
  _M0L9remainingS562
  = (struct _M0TPB8MutLocalGORPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPC16string10StringViewE));
  Moonbit_object_header(_M0L9remainingS562)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 132, 0);
  _M0L9remainingS562->$0 = _M0L4SomeS2802;
  moonbit_incref(_M0L3sepS559.$0);
  _closure_5515
  = (struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__*)moonbit_malloc(sizeof(struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__));
  Moonbit_object_header(_closure_5515)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 135, 0);
  _closure_5515->code = &_M0MPC16string10StringView5splitC2793l1148;
  _closure_5515->$0 = _M0L9remainingS562;
  _closure_5515->$1 = _M0L3sepS559;
  _closure_5515->$2 = _M0L8sep__lenS558;
  _M0L6_2atmpS2792
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_5515;
  #line 1148 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _result_5516
  = _M0MPB4Iter3newGRPC16string10StringViewE(_M0L6_2atmpS2792, 4294967296ll);
  moonbit_decref(_M0L6_2atmpS2792);
  return _result_5516;
}

void* _M0MPC16string10StringView5splitC2793l1148(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2794
) {
  struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__* _M0L14_2acasted__envS2795;
  int32_t _M0L8sep__lenS558;
  struct _M0TPC16string10StringView _M0L3sepS559;
  struct _M0TPB8MutLocalGORPC16string10StringViewE* _M0L9remainingS562;
  void* _M0L7_2abindS563;
  #line 1148 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L14_2acasted__envS2795
  = (struct _M0R44StringView_3a_3asplit_2eanon__u2793__l1148__*)_M0L6_2aenvS2794;
  _M0L8sep__lenS558 = _M0L14_2acasted__envS2795->$2;
  _M0L3sepS559 = _M0L14_2acasted__envS2795->$1;
  _M0L9remainingS562 = _M0L14_2acasted__envS2795->$0;
  _M0L7_2abindS563 = _M0L9remainingS562->$0;
  switch (Moonbit_object_tag(_M0L7_2abindS563)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS564 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS563;
      struct _M0TPC16string10StringView _M0L7_2aviewS565 =
        _M0L7_2aSomeS564->$0;
      int64_t _M0L7_2abindS566;
      moonbit_incref(_M0L7_2aviewS565.$0);
      #line 1150 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
      _M0L7_2abindS566
      = _M0MPC16string10StringView4find(_M0L7_2aviewS565, _M0L3sepS559);
      if (_M0L7_2abindS566 == 4294967296ll) {
        void* _M0L4NoneS2796 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        void* _M0L6_2aoldS4948 = _M0L9remainingS562->$0;
        void* _block_5517;
        moonbit_decref(_M0L6_2aoldS4948);
        _M0L9remainingS562->$0 = _M0L4NoneS2796;
        _block_5517
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_5517)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5517)->$0
        = _M0L7_2aviewS565;
        return _block_5517;
      } else {
        int64_t _M0L7_2aSomeS567 = _M0L7_2abindS566;
        int32_t _M0L6_2aendS568 = (int32_t)_M0L7_2aSomeS567;
        int32_t _M0L6_2atmpS2799 = _M0L6_2aendS568 + _M0L8sep__lenS558;
        struct _M0TPC16string10StringView _M0L6_2atmpS2798;
        void* _M0L4SomeS2797;
        void* _M0L6_2aoldS4949;
        int64_t _M0L6_2atmpS2801;
        struct _M0TPC16string10StringView _M0L6_2atmpS2800;
        void* _block_5518;
        #line 1154 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
        _M0L6_2atmpS2798
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS565, _M0L6_2atmpS2799, 4294967296ll);
        _M0L4SomeS2797
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_M0L4SomeS2797)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2797)->$0
        = _M0L6_2atmpS2798;
        _M0L6_2aoldS4949 = _M0L9remainingS562->$0;
        moonbit_decref(_M0L6_2aoldS4949);
        _M0L9remainingS562->$0 = _M0L4SomeS2797;
        _M0L6_2atmpS2801 = (int64_t)_M0L6_2aendS568;
        #line 1155 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
        _M0L6_2atmpS2800
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS565, 0, _M0L6_2atmpS2801);
        moonbit_decref(_M0L7_2aviewS565.$0);
        _block_5518
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_5518)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5518)->$0
        = _M0L6_2atmpS2800;
        return _block_5518;
      }
      break;
    }
    default: {
      return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      break;
    }
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC2789l1145(
  struct _M0TWcERPC16string10StringView* _M0L6_2aenvS2790,
  int32_t _M0L1cS561
) {
  moonbit_string_t _M0L6_2atmpS2791;
  struct _M0TPC16string10StringView _result_5519;
  #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS2791 = _M0IPC14char4CharPB4Show10to__string(_M0L1cS561);
  #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _result_5519
  = _M0MPC16string6String12view_2einner(_M0L6_2atmpS2791, 0, 4294967296ll);
  moonbit_decref(_M0L6_2atmpS2791);
  return _result_5519;
}

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t _M0L4selfS557) {
  #line 446 "/home/developer/.moon/lib/core/builtin/char.mbt"
  #line 447 "/home/developer/.moon/lib/core/builtin/char.mbt"
  return _M0FPB16char__to__string(_M0L4selfS557);
}

moonbit_string_t _M0FPB16char__to__string(int32_t _M0L4charS556) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS555;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2786;
  moonbit_string_t _result_5520;
  #line 452 "/home/developer/.moon/lib/core/builtin/char.mbt"
  #line 454 "/home/developer/.moon/lib/core/builtin/char.mbt"
  _M0L7_2aselfS555 = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 454 "/home/developer/.moon/lib/core/builtin/char.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS555, _M0L4charS556);
  _M0L6_2atmpS2786 = _M0L7_2aselfS555;
  #line 454 "/home/developer/.moon/lib/core/builtin/char.mbt"
  _result_5520 = _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2786);
  moonbit_decref(_M0L6_2atmpS2786);
  return _result_5520;
}

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TPB4IterGcE* _M0L4selfS551,
  struct _M0TWcERPC16string10StringView* _M0L1fS554
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__* _closure_5521;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2780;
  int64_t _M0L10size__hintS2781;
  struct _M0TPB4IterGRPC16string10StringViewE* _block_5522;
  #line 389 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  moonbit_incref(_M0L1fS554);
  moonbit_incref(_M0L4selfS551);
  _closure_5521
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__*)moonbit_malloc(sizeof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__));
  Moonbit_object_header(_closure_5521)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 139, 0);
  _closure_5521->code = &_M0MPB4Iter3mapGcRPC16string10StringViewEC2782l391;
  _closure_5521->$0 = _M0L1fS554;
  _closure_5521->$1 = _M0L4selfS551;
  _M0L6_2atmpS2780
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_5521;
  _M0L10size__hintS2781 = _M0L4selfS551->$1;
  _block_5522
  = (struct _M0TPB4IterGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB4IterGRPC16string10StringViewE));
  Moonbit_object_header(_block_5522)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 143, 0);
  _block_5522->$0 = _M0L6_2atmpS2780;
  _block_5522->$1 = _M0L10size__hintS2781;
  return _block_5522;
}

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC2782l391(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2783
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__* _M0L14_2acasted__envS2784;
  struct _M0TPB4IterGcE* _M0L4selfS551;
  struct _M0TWcERPC16string10StringView* _M0L1fS554;
  int32_t _M0L7_2abindS550;
  #line 391 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L14_2acasted__envS2784
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2782__l391__*)_M0L6_2aenvS2783;
  _M0L4selfS551 = _M0L14_2acasted__envS2784->$1;
  _M0L1fS554 = _M0L14_2acasted__envS2784->$0;
  #line 392 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2abindS550 = _M0MPB4Iter4nextGcE(_M0L4selfS551);
  if (_M0L7_2abindS550 == -1) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    int32_t _M0L7_2aSomeS552 = _M0L7_2abindS550;
    int32_t _M0L4_2axS553 = _M0L7_2aSomeS552;
    struct _M0TPC16string10StringView _M0L6_2atmpS2785;
    void* _block_5523;
    #line 393 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
    _M0L6_2atmpS2785 = _M0L1fS554->code(_M0L1fS554, _M0L4_2axS553);
    _block_5523
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_block_5523)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5523)->$0
    = _M0L6_2atmpS2785;
    return _block_5523;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS541,
  moonbit_string_t _M0L5valueS543
) {
  int32_t _M0L3lenS2765;
  moonbit_string_t* _M0L6_2atmpS2767;
  int32_t _M0L6_2atmpS2766;
  int32_t _M0L6lengthS542;
  moonbit_string_t* _M0L3bufS2768;
  moonbit_string_t _M0L6_2aoldS4952;
  int32_t _M0L6_2atmpS2769;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2765 = _M0L4selfS541->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2767 = _M0MPC15array5Array6bufferGsE(_M0L4selfS541);
  _M0L6_2atmpS2766 = Moonbit_array_length(_M0L6_2atmpS2767);
  moonbit_decref(_M0L6_2atmpS2767);
  if (_M0L3lenS2765 == _M0L6_2atmpS2766) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS541);
  }
  _M0L6lengthS542 = _M0L4selfS541->$1;
  _M0L3bufS2768 = _M0L4selfS541->$0;
  _M0L6_2aoldS4952 = (moonbit_string_t)_M0L3bufS2768[_M0L6lengthS542];
  moonbit_incref(_M0L5valueS543);
  moonbit_decref(_M0L6_2aoldS4952);
  _M0L3bufS2768[_M0L6lengthS542] = _M0L5valueS543;
  _M0L6_2atmpS2769 = _M0L6lengthS542 + 1;
  _M0L4selfS541->$1 = _M0L6_2atmpS2769;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS544,
  struct _M0TUsfE* _M0L5valueS546
) {
  int32_t _M0L3lenS2770;
  struct _M0TUsfE** _M0L6_2atmpS2772;
  int32_t _M0L6_2atmpS2771;
  int32_t _M0L6lengthS545;
  struct _M0TUsfE** _M0L3bufS2773;
  struct _M0TUsfE* _M0L6_2aoldS4954;
  int32_t _M0L6_2atmpS2774;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2770 = _M0L4selfS544->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2772 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS544);
  _M0L6_2atmpS2771 = Moonbit_array_length(_M0L6_2atmpS2772);
  moonbit_decref(_M0L6_2atmpS2772);
  if (_M0L3lenS2770 == _M0L6_2atmpS2771) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS544);
  }
  _M0L6lengthS545 = _M0L4selfS544->$1;
  _M0L3bufS2773 = _M0L4selfS544->$0;
  _M0L6_2aoldS4954 = (struct _M0TUsfE*)_M0L3bufS2773[_M0L6lengthS545];
  moonbit_incref(_M0L5valueS546);
  if (_M0L6_2aoldS4954) {
    moonbit_decref(_M0L6_2aoldS4954);
  }
  _M0L3bufS2773[_M0L6lengthS545] = _M0L5valueS546;
  _M0L6_2atmpS2774 = _M0L6lengthS545 + 1;
  _M0L4selfS544->$1 = _M0L6_2atmpS2774;
  return 0;
}

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS547,
  moonbit_string_t _M0L5valueS549
) {
  int32_t _M0L3lenS2775;
  moonbit_string_t* _M0L6_2atmpS2777;
  int32_t _M0L6_2atmpS2776;
  int32_t _M0L6lengthS548;
  moonbit_string_t* _M0L3bufS2778;
  moonbit_string_t _M0L6_2aoldS4956;
  int32_t _M0L6_2atmpS2779;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2775 = _M0L4selfS547->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2777 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS547);
  _M0L6_2atmpS2776 = Moonbit_array_length(_M0L6_2atmpS2777);
  moonbit_decref(_M0L6_2atmpS2777);
  if (_M0L3lenS2775 == _M0L6_2atmpS2776) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGOsE(_M0L4selfS547);
  }
  _M0L6lengthS548 = _M0L4selfS547->$1;
  _M0L3bufS2778 = _M0L4selfS547->$0;
  _M0L6_2aoldS4956 = (moonbit_string_t)_M0L3bufS2778[_M0L6lengthS548];
  if (_M0L5valueS549) {
    moonbit_incref(_M0L5valueS549);
  }
  if (_M0L6_2aoldS4956) {
    moonbit_decref(_M0L6_2aoldS4956);
  }
  _M0L3bufS2778[_M0L6lengthS548] = _M0L5valueS549;
  _M0L6_2atmpS2779 = _M0L6lengthS548 + 1;
  _M0L4selfS547->$1 = _M0L6_2atmpS2779;
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS533) {
  int32_t _M0L8old__capS532;
  int32_t _M0L8new__capS534;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS532 = _M0L4selfS533->$1;
  if (_M0L8old__capS532 == 0) {
    _M0L8new__capS534 = 8;
  } else {
    _M0L8new__capS534 = _M0L8old__capS532 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS533, _M0L8new__capS534);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS536
) {
  int32_t _M0L8old__capS535;
  int32_t _M0L8new__capS537;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS535 = _M0L4selfS536->$1;
  if (_M0L8old__capS535 == 0) {
    _M0L8new__capS537 = 8;
  } else {
    _M0L8new__capS537 = _M0L8old__capS535 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsfEE(_M0L4selfS536, _M0L8new__capS537);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS539
) {
  int32_t _M0L8old__capS538;
  int32_t _M0L8new__capS540;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS538 = _M0L4selfS539->$1;
  if (_M0L8old__capS538 == 0) {
    _M0L8new__capS540 = 8;
  } else {
    _M0L8new__capS540 = _M0L8old__capS538 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGOsE(_M0L4selfS539, _M0L8new__capS540);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS515,
  int32_t _M0L13new__capacityS518
) {
  moonbit_string_t* _M0L8old__bufS514;
  int32_t _M0L8old__capS516;
  int32_t _M0L9copy__lenS517;
  moonbit_string_t* _M0L8new__bufS519;
  moonbit_string_t* _M0L6_2aoldS4958;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS514 = _M0L4selfS515->$0;
  _M0L8old__capS516 = Moonbit_array_length(_M0L8old__bufS514);
  if (_M0L8old__capS516 < _M0L13new__capacityS518) {
    _M0L9copy__lenS517 = _M0L8old__capS516;
  } else {
    _M0L9copy__lenS517 = _M0L13new__capacityS518;
  }
  moonbit_incref(_M0L8old__bufS514);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS519
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(_M0L8old__bufS514, _M0L13new__capacityS518, _M0L9copy__lenS517, 0, 0);
  moonbit_decref(_M0L8old__bufS514);
  _M0L6_2aoldS4958 = _M0L4selfS515->$0;
  moonbit_decref(_M0L6_2aoldS4958);
  _M0L4selfS515->$0 = _M0L8new__bufS519;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS521,
  int32_t _M0L13new__capacityS524
) {
  struct _M0TUsfE** _M0L8old__bufS520;
  int32_t _M0L8old__capS522;
  int32_t _M0L9copy__lenS523;
  struct _M0TUsfE** _M0L8new__bufS525;
  struct _M0TUsfE** _M0L6_2aoldS4960;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS520 = _M0L4selfS521->$0;
  _M0L8old__capS522 = Moonbit_array_length(_M0L8old__bufS520);
  if (_M0L8old__capS522 < _M0L13new__capacityS524) {
    _M0L9copy__lenS523 = _M0L8old__capS522;
  } else {
    _M0L9copy__lenS523 = _M0L13new__capacityS524;
  }
  moonbit_incref(_M0L8old__bufS520);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS525
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(_M0L8old__bufS520, _M0L13new__capacityS524, _M0L9copy__lenS523, 0, 0);
  moonbit_decref(_M0L8old__bufS520);
  _M0L6_2aoldS4960 = _M0L4selfS521->$0;
  moonbit_decref(_M0L6_2aoldS4960);
  _M0L4selfS521->$0 = _M0L8new__bufS525;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS527,
  int32_t _M0L13new__capacityS530
) {
  moonbit_string_t* _M0L8old__bufS526;
  int32_t _M0L8old__capS528;
  int32_t _M0L9copy__lenS529;
  moonbit_string_t* _M0L8new__bufS531;
  moonbit_string_t* _M0L6_2aoldS4962;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS526 = _M0L4selfS527->$0;
  _M0L8old__capS528 = Moonbit_array_length(_M0L8old__bufS526);
  if (_M0L8old__capS528 < _M0L13new__capacityS530) {
    _M0L9copy__lenS529 = _M0L8old__capS528;
  } else {
    _M0L9copy__lenS529 = _M0L13new__capacityS530;
  }
  moonbit_incref(_M0L8old__bufS526);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS531
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(_M0L8old__bufS526, _M0L13new__capacityS530, _M0L9copy__lenS529, 0, 0);
  moonbit_decref(_M0L8old__bufS526);
  _M0L6_2aoldS4962 = _M0L4selfS527->$0;
  moonbit_decref(_M0L6_2aoldS4962);
  _M0L4selfS527->$0 = _M0L8new__bufS531;
  return 0;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS511) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS511->$1;
}

int32_t _M0MPC15array5Array6lengthGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS512
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS512->$1;
}

int32_t _M0MPC15array5Array6lengthGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS513
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS513->$1;
}

int64_t _M0MPC16string6String8find__by(
  moonbit_string_t _M0L4selfS509,
  struct _M0TWcEb* _M0L4predS510
) {
  int32_t _M0L6_2atmpS2764;
  struct _M0TPC16string10StringView _M0L6_2atmpS2763;
  int64_t _result_5524;
  #line 144 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS2764 = Moonbit_array_length(_M0L4selfS509);
  moonbit_incref(_M0L4selfS509);
  _M0L6_2atmpS2763
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L4selfS509, .$1 = 0, .$2 = _M0L6_2atmpS2764
  };
  #line 145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _result_5524
  = _M0MPC16string10StringView8find__by(_M0L6_2atmpS2763, _M0L4predS510);
  moonbit_decref(_M0L6_2atmpS2763.$0);
  return _result_5524;
}

int64_t _M0MPC16string10StringView8find__by(
  struct _M0TPC16string10StringView _M0L4selfS496,
  struct _M0TWcEb* _M0L4predS505
) {
  moonbit_string_t _M0L7_2abindS495;
  int32_t _M0L7_2abindS497;
  int32_t _M0L7_2abindS498;
  int32_t _M0L16_2astring__indexS499;
  int32_t _M0L1iS500;
  #line 131 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L7_2abindS495 = _M0L4selfS496.$0;
  _M0L7_2abindS497 = _M0L4selfS496.$1;
  _M0L7_2abindS498 = _M0L4selfS496.$2;
  moonbit_incref(_M0L7_2abindS495);
  _M0L16_2astring__indexS499 = _M0L7_2abindS497;
  _M0L1iS500 = 0;
  while (1) {
    if (_M0L16_2astring__indexS499 < _M0L7_2abindS498) {
      int32_t _M0L31_2adecoded__next__string__indexS502;
      int32_t _M0L16_2adecoded__charS503;
      int32_t _M0L7_2abindS507 = _M0L7_2abindS495[_M0L16_2astring__indexS499];
      int32_t _M0L6_2atmpS2744 = (int32_t)_M0L7_2abindS507;
      int32_t _if__result_5527;
      int32_t _if__result_5528;
      int32_t _M0L20_2anext__char__indexS504;
      if (_M0L6_2atmpS2744 >= 55296) {
        int32_t _M0L6_2atmpS2743 = (int32_t)_M0L7_2abindS507;
        _if__result_5527 = _M0L6_2atmpS2743 <= 56319;
      } else {
        _if__result_5527 = 0;
      }
      if (_if__result_5527) {
        int32_t _M0L6_2atmpS2742 = _M0L16_2astring__indexS499 + 1;
        _if__result_5528 = _M0L6_2atmpS2742 < _M0L7_2abindS498;
      } else {
        _if__result_5528 = 0;
      }
      if (_if__result_5528) {
        int32_t _M0L6_2atmpS2759 = _M0L16_2astring__indexS499 + 1;
        int32_t _M0L7_2abindS508 = _M0L7_2abindS495[_M0L6_2atmpS2759];
        int32_t _M0L6_2atmpS2746 = (int32_t)_M0L7_2abindS508;
        int32_t _if__result_5529;
        if (_M0L6_2atmpS2746 >= 56320) {
          int32_t _M0L6_2atmpS2745 = (int32_t)_M0L7_2abindS508;
          _if__result_5529 = _M0L6_2atmpS2745 <= 57343;
        } else {
          _if__result_5529 = 0;
        }
        if (_if__result_5529) {
          int32_t _M0L6_2atmpS2747 = _M0L16_2astring__indexS499 + 2;
          int32_t _M0L6_2atmpS2755 = (int32_t)_M0L7_2abindS507;
          int32_t _M0L6_2atmpS2754 = _M0L6_2atmpS2755 - 55296;
          int32_t _M0L6_2atmpS2752 = _M0L6_2atmpS2754 * 1024;
          int32_t _M0L6_2atmpS2753 = (int32_t)_M0L7_2abindS508;
          int32_t _M0L6_2atmpS2751 = _M0L6_2atmpS2752 + _M0L6_2atmpS2753;
          int32_t _M0L6_2atmpS2750 = _M0L6_2atmpS2751 - 56320;
          int32_t _M0L6_2atmpS2749 = _M0L6_2atmpS2750 + 65536;
          int32_t _M0L6_2atmpS2748;
          #line 133 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS2748
          = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2749);
          _M0L31_2adecoded__next__string__indexS502 = _M0L6_2atmpS2747;
          _M0L16_2adecoded__charS503 = _M0L6_2atmpS2748;
          goto join_501;
        } else {
          int32_t _M0L6_2atmpS2756 = _M0L16_2astring__indexS499 + 1;
          int32_t _M0L6_2atmpS2758 = (int32_t)_M0L7_2abindS507;
          int32_t _M0L6_2atmpS2757;
          #line 133 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS2757
          = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2758);
          _M0L31_2adecoded__next__string__indexS502 = _M0L6_2atmpS2756;
          _M0L16_2adecoded__charS503 = _M0L6_2atmpS2757;
          goto join_501;
        }
      } else {
        int32_t _M0L6_2atmpS2760 = _M0L16_2astring__indexS499 + 1;
        int32_t _M0L6_2atmpS2762 = (int32_t)_M0L7_2abindS507;
        int32_t _M0L6_2atmpS2761;
        #line 133 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
        _M0L6_2atmpS2761
        = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2762);
        _M0L31_2adecoded__next__string__indexS502 = _M0L6_2atmpS2760;
        _M0L16_2adecoded__charS503 = _M0L6_2atmpS2761;
        goto join_501;
      }
      goto joinlet_5526;
      join_501:;
      _M0L20_2anext__char__indexS504 = _M0L1iS500 + 1;
      #line 134 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
      if (_M0L4predS505->code(_M0L4predS505, _M0L16_2adecoded__charS503)) {
        moonbit_decref(_M0L7_2abindS495);
        return (int64_t)_M0L1iS500;
      }
      _M0L16_2astring__indexS499 = _M0L31_2adecoded__next__string__indexS502;
      _M0L1iS500 = _M0L20_2anext__char__indexS504;
      continue;
      joinlet_5526:;
    } else {
      moonbit_decref(_M0L7_2abindS495);
    }
    break;
  }
  return 4294967296ll;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS494
) {
  #line 222 "/home/developer/.moon/lib/core/builtin/show.mbt"
  moonbit_incref(_M0L4selfS494);
  return _M0L4selfS494;
}

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView _M0L4selfS493,
  struct _M0TPC16string10StringView _M0L3strS492
) {
  int32_t _M0L3endS2740;
  int32_t _M0L5startS2741;
  int32_t _M0L6_2atmpS2739;
  #line 18 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L3endS2740 = _M0L3strS492.$2;
  _M0L5startS2741 = _M0L3strS492.$1;
  _M0L6_2atmpS2739 = _M0L3endS2740 - _M0L5startS2741;
  if (_M0L6_2atmpS2739 <= 4) {
    #line 20 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB18brute__force__find(_M0L4selfS493, _M0L3strS492);
  } else {
    #line 22 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB28boyer__moore__horspool__find(_M0L4selfS493, _M0L3strS492);
  }
}

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView _M0L8haystackS482,
  struct _M0TPC16string10StringView _M0L6needleS484
) {
  int32_t _M0L3endS2737;
  int32_t _M0L5startS2738;
  int32_t _M0L13haystack__lenS481;
  int32_t _M0L3endS2735;
  int32_t _M0L5startS2736;
  int32_t _M0L11needle__lenS483;
  #line 31 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L3endS2737 = _M0L8haystackS482.$2;
  _M0L5startS2738 = _M0L8haystackS482.$1;
  _M0L13haystack__lenS481 = _M0L3endS2737 - _M0L5startS2738;
  _M0L3endS2735 = _M0L6needleS484.$2;
  _M0L5startS2736 = _M0L6needleS484.$1;
  _M0L11needle__lenS483 = _M0L3endS2735 - _M0L5startS2736;
  if (_M0L11needle__lenS483 > 0) {
    if (_M0L13haystack__lenS481 >= _M0L11needle__lenS483) {
      moonbit_string_t _M0L3strS2733 = _M0L6needleS484.$0;
      int32_t _M0L5startS2734 = _M0L6needleS484.$1;
      int32_t _M0L13needle__firstS485 = _M0L3strS2733[_M0L5startS2734];
      int32_t _M0L12forward__lenS486 =
        _M0L13haystack__lenS481 - _M0L11needle__lenS483;
      int32_t _M0L1iS487 = 0;
      while (1) {
        if (_M0L1iS487 <= _M0L12forward__lenS486) {
          moonbit_string_t _M0L3strS2720 = _M0L8haystackS482.$0;
          int32_t _M0L5startS2722 = _M0L8haystackS482.$1;
          int32_t _M0L6_2atmpS2721 = _M0L5startS2722 + _M0L1iS487;
          int32_t _M0L6_2atmpS2719 = _M0L3strS2720[_M0L6_2atmpS2721];
          int32_t _M0L1jS490;
          int32_t _M0L6_2atmpS2718;
          if (_M0L6_2atmpS2719 != _M0L13needle__firstS485) {
            goto join_488;
          }
          _M0L1jS490 = 1;
          while (1) {
            if (_M0L1jS490 < _M0L11needle__lenS483) {
              moonbit_string_t _M0L3strS2728 = _M0L8haystackS482.$0;
              int32_t _M0L5startS2730 = _M0L8haystackS482.$1;
              int32_t _M0L6_2atmpS2731 = _M0L1iS487 + _M0L1jS490;
              int32_t _M0L6_2atmpS2729 = _M0L5startS2730 + _M0L6_2atmpS2731;
              int32_t _M0L6_2atmpS2723 = _M0L3strS2728[_M0L6_2atmpS2729];
              moonbit_string_t _M0L3strS2725 = _M0L6needleS484.$0;
              int32_t _M0L5startS2727 = _M0L6needleS484.$1;
              int32_t _M0L6_2atmpS2726 = _M0L5startS2727 + _M0L1jS490;
              int32_t _M0L6_2atmpS2724 = _M0L3strS2725[_M0L6_2atmpS2726];
              int32_t _M0L6_2atmpS2732;
              if (_M0L6_2atmpS2723 != _M0L6_2atmpS2724) {
                break;
              }
              _M0L6_2atmpS2732 = _M0L1jS490 + 1;
              _M0L1jS490 = _M0L6_2atmpS2732;
              continue;
            } else {
              return (int64_t)_M0L1iS487;
            }
            break;
          }
          goto join_488;
          goto joinlet_5531;
          join_488:;
          _M0L6_2atmpS2718 = _M0L1iS487 + 1;
          _M0L1iS487 = _M0L6_2atmpS2718;
          continue;
          joinlet_5531:;
        }
        break;
      }
      return 4294967296ll;
    } else {
      return 4294967296ll;
    }
  } else {
    return _M0FPB18brute__force__findN6constrS9991;
  }
}

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView _M0L8haystackS469,
  struct _M0TPC16string10StringView _M0L6needleS471
) {
  int32_t _M0L3endS2716;
  int32_t _M0L5startS2717;
  int32_t _M0L13haystack__lenS468;
  int32_t _M0L3endS2714;
  int32_t _M0L5startS2715;
  int32_t _M0L11needle__lenS470;
  #line 57 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L3endS2716 = _M0L8haystackS469.$2;
  _M0L5startS2717 = _M0L8haystackS469.$1;
  _M0L13haystack__lenS468 = _M0L3endS2716 - _M0L5startS2717;
  _M0L3endS2714 = _M0L6needleS471.$2;
  _M0L5startS2715 = _M0L6needleS471.$1;
  _M0L11needle__lenS470 = _M0L3endS2714 - _M0L5startS2715;
  if (_M0L11needle__lenS470 > 0) {
    if (_M0L13haystack__lenS468 >= _M0L11needle__lenS470) {
      int32_t* _M0L11skip__tableS472 =
        (int32_t*)moonbit_make_int32_array(256, _M0L11needle__lenS470);
      int32_t _M0L7_2abindS473 = _M0L11needle__lenS470 - 1;
      int32_t _M0L1iS474 = 0;
      int32_t _M0L1iS476;
      while (1) {
        if (_M0L1iS474 < _M0L7_2abindS473) {
          moonbit_string_t _M0L3strS2689 = _M0L6needleS471.$0;
          int32_t _M0L5startS2691 = _M0L6needleS471.$1;
          int32_t _M0L6_2atmpS2690 = _M0L5startS2691 + _M0L1iS474;
          int32_t _M0L6_2atmpS2688 = _M0L3strS2689[_M0L6_2atmpS2690];
          int32_t _M0L6_2atmpS2687 = (int32_t)_M0L6_2atmpS2688;
          int32_t _M0L6_2atmpS2684 = _M0L6_2atmpS2687 & 255;
          int32_t _M0L6_2atmpS2686 = _M0L11needle__lenS470 - 1;
          int32_t _M0L6_2atmpS2685 = _M0L6_2atmpS2686 - _M0L1iS474;
          int32_t _M0L6_2atmpS2692;
          if (
            _M0L6_2atmpS2684 < 0
            || _M0L6_2atmpS2684
               >= Moonbit_array_length(_M0L11skip__tableS472)
          ) {
            #line 68 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L11skip__tableS472[_M0L6_2atmpS2684] = _M0L6_2atmpS2685;
          _M0L6_2atmpS2692 = _M0L1iS474 + 1;
          _M0L1iS474 = _M0L6_2atmpS2692;
          continue;
        }
        break;
      }
      _M0L1iS476 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2693 =
          _M0L13haystack__lenS468 - _M0L11needle__lenS470;
        if (_M0L1iS476 <= _M0L6_2atmpS2693) {
          int32_t _M0L7_2abindS477 = _M0L11needle__lenS470 - 1;
          int32_t _M0L1jS478 = 0;
          moonbit_string_t _M0L3strS2709;
          int32_t _M0L5startS2711;
          int32_t _M0L6_2atmpS2713;
          int32_t _M0L6_2atmpS2712;
          int32_t _M0L6_2atmpS2710;
          int32_t _M0L6_2atmpS2708;
          int32_t _M0L6_2atmpS2707;
          int32_t _M0L6_2atmpS2706;
          int32_t _M0L6_2atmpS2705;
          int32_t _M0L6_2atmpS2704;
          while (1) {
            if (_M0L1jS478 <= _M0L7_2abindS477) {
              moonbit_string_t _M0L3strS2699 = _M0L8haystackS469.$0;
              int32_t _M0L5startS2701 = _M0L8haystackS469.$1;
              int32_t _M0L6_2atmpS2702 = _M0L1iS476 + _M0L1jS478;
              int32_t _M0L6_2atmpS2700 = _M0L5startS2701 + _M0L6_2atmpS2702;
              int32_t _M0L6_2atmpS2694 = _M0L3strS2699[_M0L6_2atmpS2700];
              moonbit_string_t _M0L3strS2696 = _M0L6needleS471.$0;
              int32_t _M0L5startS2698 = _M0L6needleS471.$1;
              int32_t _M0L6_2atmpS2697 = _M0L5startS2698 + _M0L1jS478;
              int32_t _M0L6_2atmpS2695 = _M0L3strS2696[_M0L6_2atmpS2697];
              int32_t _M0L6_2atmpS2703;
              if (_M0L6_2atmpS2694 != _M0L6_2atmpS2695) {
                break;
              }
              _M0L6_2atmpS2703 = _M0L1jS478 + 1;
              _M0L1jS478 = _M0L6_2atmpS2703;
              continue;
            } else {
              moonbit_decref(_M0L11skip__tableS472);
              return (int64_t)_M0L1iS476;
            }
            break;
          }
          _M0L3strS2709 = _M0L8haystackS469.$0;
          _M0L5startS2711 = _M0L8haystackS469.$1;
          _M0L6_2atmpS2713 = _M0L1iS476 + _M0L11needle__lenS470;
          _M0L6_2atmpS2712 = _M0L6_2atmpS2713 - 1;
          _M0L6_2atmpS2710 = _M0L5startS2711 + _M0L6_2atmpS2712;
          _M0L6_2atmpS2708 = _M0L3strS2709[_M0L6_2atmpS2710];
          _M0L6_2atmpS2707 = (int32_t)_M0L6_2atmpS2708;
          _M0L6_2atmpS2706 = _M0L6_2atmpS2707 & 255;
          if (
            _M0L6_2atmpS2706 < 0
            || _M0L6_2atmpS2706
               >= Moonbit_array_length(_M0L11skip__tableS472)
          ) {
            #line 73 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2705 = (int32_t)_M0L11skip__tableS472[_M0L6_2atmpS2706];
          _M0L6_2atmpS2704 = _M0L1iS476 + _M0L6_2atmpS2705;
          _M0L1iS476 = _M0L6_2atmpS2704;
          continue;
        } else {
          moonbit_decref(_M0L11skip__tableS472);
        }
        break;
      }
      return 4294967296ll;
    } else {
      return 4294967296ll;
    }
  } else {
    return _M0FPB28boyer__moore__horspool__findN6constrS9990;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS467,
  struct _M0TPC16string10StringView _M0L3strS466
) {
  int32_t _M0L3endS2682;
  int32_t _M0L5startS2683;
  int32_t _M0L8str__lenS465;
  int32_t _M0L3lenS2675;
  int32_t _M0L6_2atmpS2674;
  uint16_t* _M0L4dataS2676;
  int32_t _M0L3lenS2677;
  moonbit_string_t _M0L6_2atmpS2678;
  int32_t _M0L6_2atmpS2679;
  int32_t _M0L3lenS2681;
  int32_t _M0L6_2atmpS2680;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2682 = _M0L3strS466.$2;
  _M0L5startS2683 = _M0L3strS466.$1;
  _M0L8str__lenS465 = _M0L3endS2682 - _M0L5startS2683;
  _M0L3lenS2675 = _M0L4selfS467->$1;
  _M0L6_2atmpS2674 = _M0L3lenS2675 + _M0L8str__lenS465;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS467, _M0L6_2atmpS2674);
  _M0L4dataS2676 = _M0L4selfS467->$0;
  _M0L3lenS2677 = _M0L4selfS467->$1;
  moonbit_incref(_M0L4dataS2676);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2678 = _M0MPC16string10StringView4data(_M0L3strS466);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2679 = _M0MPC16string10StringView13start__offset(_M0L3strS466);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2676, _M0L3lenS2677, _M0L6_2atmpS2678, _M0L6_2atmpS2679, _M0L8str__lenS465);
  moonbit_decref(_M0L4dataS2676);
  moonbit_decref(_M0L6_2atmpS2678);
  _M0L3lenS2681 = _M0L4selfS467->$1;
  _M0L6_2atmpS2680 = _M0L3lenS2681 + _M0L8str__lenS465;
  _M0L4selfS467->$1 = _M0L6_2atmpS2680;
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS463,
  int32_t _M0L13start__offsetS464,
  int64_t _M0L11end__offsetS461
) {
  int32_t _M0L11end__offsetS460;
  int32_t _if__result_5536;
  #line 614 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS461 == 4294967296ll) {
    _M0L11end__offsetS460 = Moonbit_array_length(_M0L4selfS463);
  } else {
    int64_t _M0L7_2aSomeS462 = _M0L11end__offsetS461;
    _M0L11end__offsetS460 = (int32_t)_M0L7_2aSomeS462;
  }
  if (_M0L13start__offsetS464 >= 0) {
    if (_M0L13start__offsetS464 <= _M0L11end__offsetS460) {
      int32_t _M0L6_2atmpS2673 = Moonbit_array_length(_M0L4selfS463);
      _if__result_5536 = _M0L11end__offsetS460 <= _M0L6_2atmpS2673;
    } else {
      _if__result_5536 = 0;
    }
  } else {
    _if__result_5536 = 0;
  }
  if (_if__result_5536) {
    moonbit_incref(_M0L4selfS463);
    return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS463,
                                                 .$1 = _M0L13start__offsetS464,
                                                 .$2 = _M0L11end__offsetS460};
  } else {
    #line 623 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_105.data);
  }
}

struct _M0TPB4IterGcE* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS455
) {
  int32_t _M0L5startS454;
  int32_t _M0L3endS456;
  struct _M0TPB8MutLocalGiE* _M0L5indexS457;
  struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__* _closure_5537;
  struct _M0TWEOc* _M0L6_2atmpS2652;
  struct _M0TPB4IterGcE* _result_5538;
  #line 214 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L5startS454 = _M0L4selfS455.$1;
  _M0L3endS456 = _M0L4selfS455.$2;
  _M0L5indexS457
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5indexS457)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5indexS457->$0 = _M0L5startS454;
  moonbit_incref(_M0L4selfS455.$0);
  _closure_5537
  = (struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__));
  Moonbit_object_header(_closure_5537)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 146, 0);
  _closure_5537->code = &_M0MPC16string10StringView4iterC2653l219;
  _closure_5537->$0 = _M0L5indexS457;
  _closure_5537->$1 = _M0L3endS456;
  _closure_5537->$2 = _M0L4selfS455;
  _M0L6_2atmpS2652 = (struct _M0TWEOc*)_closure_5537;
  #line 219 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _result_5538 = _M0MPB4Iter3newGcE(_M0L6_2atmpS2652, 4294967296ll);
  moonbit_decref(_M0L6_2atmpS2652);
  return _result_5538;
}

int32_t _M0MPC16string10StringView4iterC2653l219(
  struct _M0TWEOc* _M0L6_2aenvS2654
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__* _M0L14_2acasted__envS2655;
  struct _M0TPC16string10StringView _M0L4selfS455;
  int32_t _M0L3endS456;
  struct _M0TPB8MutLocalGiE* _M0L5indexS457;
  int32_t _M0L3valS2656;
  #line 219 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L14_2acasted__envS2655
  = (struct _M0R42StringView_3a_3aiter_2eanon__u2653__l219__*)_M0L6_2aenvS2654;
  _M0L4selfS455 = _M0L14_2acasted__envS2655->$2;
  _M0L3endS456 = _M0L14_2acasted__envS2655->$1;
  _M0L5indexS457 = _M0L14_2acasted__envS2655->$0;
  _M0L3valS2656 = _M0L5indexS457->$0;
  if (_M0L3valS2656 < _M0L3endS456) {
    moonbit_string_t _M0L3strS2671 = _M0L4selfS455.$0;
    int32_t _M0L3valS2672 = _M0L5indexS457->$0;
    int32_t _M0L2c1S458 = _M0L3strS2671[_M0L3valS2672];
    int32_t _if__result_5539;
    int32_t _M0L3valS2669;
    int32_t _M0L6_2atmpS2668;
    int32_t _M0L6_2atmpS2670;
    #line 222 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S458)) {
      int32_t _M0L3valS2659 = _M0L5indexS457->$0;
      int32_t _M0L6_2atmpS2657 = _M0L3valS2659 + 1;
      int32_t _M0L3endS2658 = _M0L4selfS455.$2;
      _if__result_5539 = _M0L6_2atmpS2657 < _M0L3endS2658;
    } else {
      _if__result_5539 = 0;
    }
    if (_if__result_5539) {
      moonbit_string_t _M0L3strS2665 = _M0L4selfS455.$0;
      int32_t _M0L3valS2667 = _M0L5indexS457->$0;
      int32_t _M0L6_2atmpS2666 = _M0L3valS2667 + 1;
      int32_t _M0L2c2S459 = _M0L3strS2665[_M0L6_2atmpS2666];
      #line 224 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S459)) {
        int32_t _M0L3valS2661 = _M0L5indexS457->$0;
        int32_t _M0L6_2atmpS2660 = _M0L3valS2661 + 2;
        int32_t _M0L6_2atmpS2663;
        int32_t _M0L6_2atmpS2664;
        int32_t _M0L6_2atmpS2662;
        _M0L5indexS457->$0 = _M0L6_2atmpS2660;
        _M0L6_2atmpS2663 = (int32_t)_M0L2c1S458;
        _M0L6_2atmpS2664 = (int32_t)_M0L2c2S459;
        #line 226 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        _M0L6_2atmpS2662
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2663, _M0L6_2atmpS2664);
        return _M0L6_2atmpS2662;
      }
    }
    _M0L3valS2669 = _M0L5indexS457->$0;
    _M0L6_2atmpS2668 = _M0L3valS2669 + 1;
    _M0L5indexS457->$0 = _M0L6_2atmpS2668;
    #line 230 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    _M0L6_2atmpS2670 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S458);
    return _M0L6_2atmpS2670;
  } else {
    return -1;
  }
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS453,
  struct _M0TPB6Logger _M0L6loggerS452
) {
  #line 203 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  #line 204 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L6loggerS452.$0->$method_2(_M0L6loggerS452.$1, _M0L4selfS453);
  return 0;
}

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView _M0L4selfS451
) {
  moonbit_string_t _M0L3strS2649;
  int32_t _M0L5startS2650;
  int32_t _M0L3endS2651;
  moonbit_string_t _result_5540;
  #line 196 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS2649 = _M0L4selfS451.$0;
  _M0L5startS2650 = _M0L4selfS451.$1;
  _M0L3endS2651 = _M0L4selfS451.$2;
  moonbit_incref(_M0L3strS2649);
  #line 199 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _result_5540
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2649, _M0L5startS2650, _M0L3endS2651);
  moonbit_decref(_M0L3strS2649);
  return _result_5540;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS448,
  int32_t _M0L5startS446,
  int32_t _M0L3endS447
) {
  int32_t _if__result_5541;
  int32_t _M0L3lenS449;
  int32_t _M0L6_2atmpS2647;
  int32_t _M0L6_2atmpS2648;
  moonbit_bytes_t _M0L5bytesS450;
  moonbit_bytes_t _M0L6_2atmpS2646;
  moonbit_string_t _result_5542;
  #line 91 "/home/developer/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS446 == 0) {
    int32_t _M0L6_2atmpS2645 = Moonbit_array_length(_M0L3strS448);
    _if__result_5541 = _M0L3endS447 == _M0L6_2atmpS2645;
  } else {
    _if__result_5541 = 0;
  }
  if (_if__result_5541) {
    moonbit_incref(_M0L3strS448);
    return _M0L3strS448;
  }
  _M0L3lenS449 = _M0L3endS447 - _M0L5startS446;
  _M0L6_2atmpS2647 = _M0L3lenS449 * 2;
  #line 101 "/home/developer/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS2648 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS450
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2647, _M0L6_2atmpS2648);
  #line 102 "/home/developer/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS450, 0, _M0L3strS448, _M0L5startS446, _M0L3lenS449);
  _M0L6_2atmpS2646 = _M0L5bytesS450;
  #line 103 "/home/developer/.moon/lib/core/builtin/string.mbt"
  _result_5542
  = _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2646, 0, 4294967296ll);
  moonbit_decref(_M0L6_2atmpS2646);
  return _result_5542;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS441,
  int32_t _M0L6offsetS445,
  int64_t _M0L6lengthS443
) {
  int32_t _M0L3lenS440;
  int32_t _M0L6lengthS442;
  int32_t _if__result_5543;
  #line 77 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS440 = Moonbit_array_length(_M0L4selfS441);
  if (_M0L6lengthS443 == 4294967296ll) {
    _M0L6lengthS442 = _M0L3lenS440 - _M0L6offsetS445;
  } else {
    int64_t _M0L7_2aSomeS444 = _M0L6lengthS443;
    _M0L6lengthS442 = (int32_t)_M0L7_2aSomeS444;
  }
  if (_M0L6offsetS445 >= 0) {
    if (_M0L6lengthS442 >= 0) {
      int32_t _M0L6_2atmpS2644 = _M0L6offsetS445 + _M0L6lengthS442;
      _if__result_5543 = _M0L6_2atmpS2644 <= _M0L3lenS440;
    } else {
      _if__result_5543 = 0;
    }
  } else {
    _if__result_5543 = 0;
  }
  if (_if__result_5543) {
    moonbit_incref(_M0L4selfS441);
    #line 85 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS441, _M0L6offsetS445, _M0L6lengthS442);
  } else {
    #line 84 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS432,
  int32_t _M0L13bytes__offsetS427,
  moonbit_string_t _M0L3strS434,
  int32_t _M0L11str__offsetS430,
  int32_t _M0L6lengthS428
) {
  int32_t _M0L6_2atmpS2643;
  int32_t _M0L6_2atmpS2642;
  int32_t _M0L2e1S426;
  int32_t _M0L6_2atmpS2641;
  int32_t _M0L2e2S429;
  int32_t _M0L4len1S431;
  int32_t _M0L4len2S433;
  int32_t _if__result_5544;
  #line 125 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS2643 = _M0L6lengthS428 * 2;
  _M0L6_2atmpS2642 = _M0L13bytes__offsetS427 + _M0L6_2atmpS2643;
  _M0L2e1S426 = _M0L6_2atmpS2642 - 1;
  _M0L6_2atmpS2641 = _M0L11str__offsetS430 + _M0L6lengthS428;
  _M0L2e2S429 = _M0L6_2atmpS2641 - 1;
  _M0L4len1S431 = Moonbit_array_length(_M0L4selfS432);
  _M0L4len2S433 = Moonbit_array_length(_M0L3strS434);
  if (_M0L6lengthS428 >= 0) {
    if (_M0L13bytes__offsetS427 >= 0) {
      if (_M0L2e1S426 < _M0L4len1S431) {
        if (_M0L11str__offsetS430 >= 0) {
          _if__result_5544 = _M0L2e2S429 < _M0L4len2S433;
        } else {
          _if__result_5544 = 0;
        }
      } else {
        _if__result_5544 = 0;
      }
    } else {
      _if__result_5544 = 0;
    }
  } else {
    _if__result_5544 = 0;
  }
  if (_if__result_5544) {
    int32_t _M0L16end__str__offsetS435 =
      _M0L11str__offsetS430 + _M0L6lengthS428;
    int32_t _M0L1iS436 = _M0L11str__offsetS430;
    int32_t _M0L1jS437 = _M0L13bytes__offsetS427;
    while (1) {
      if (_M0L1iS436 < _M0L16end__str__offsetS435) {
        int32_t _M0L6_2atmpS2638 = _M0L3strS434[_M0L1iS436];
        int32_t _M0L6_2atmpS2637 = (int32_t)_M0L6_2atmpS2638;
        uint32_t _M0L1cS438 = *(uint32_t*)&_M0L6_2atmpS2637;
        uint32_t _M0L6_2atmpS2633 = _M0L1cS438 & 255u;
        int32_t _M0L6_2atmpS2632;
        int32_t _M0L6_2atmpS2634;
        uint32_t _M0L6_2atmpS2636;
        int32_t _M0L6_2atmpS2635;
        int32_t _M0L6_2atmpS2639;
        int32_t _M0L6_2atmpS2640;
        #line 142 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS2632 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2633);
        if (
          _M0L1jS437 < 0 || _M0L1jS437 >= Moonbit_array_length(_M0L4selfS432)
        ) {
          #line 142 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS432[_M0L1jS437] = _M0L6_2atmpS2632;
        _M0L6_2atmpS2634 = _M0L1jS437 + 1;
        _M0L6_2atmpS2636 = _M0L1cS438 >> 8;
        #line 143 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS2635 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2636);
        if (
          _M0L6_2atmpS2634 < 0
          || _M0L6_2atmpS2634 >= Moonbit_array_length(_M0L4selfS432)
        ) {
          #line 143 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS432[_M0L6_2atmpS2634] = _M0L6_2atmpS2635;
        _M0L6_2atmpS2639 = _M0L1iS436 + 1;
        _M0L6_2atmpS2640 = _M0L1jS437 + 2;
        _M0L1iS436 = _M0L6_2atmpS2639;
        _M0L1jS437 = _M0L6_2atmpS2640;
        continue;
      }
      break;
    }
  } else {
    #line 138 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS425) {
  int32_t _M0L6_2atmpS2631;
  #line 2519 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2631 = *(int32_t*)&_M0L4selfS425;
  return _M0L6_2atmpS2631 & 0xff;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS422
) {
  moonbit_string_t* _M0L8_2afieldS4977;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4977 = _M0L4selfS422->$0;
  moonbit_incref(_M0L8_2afieldS4977);
  return _M0L8_2afieldS4977;
}

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS423
) {
  moonbit_string_t* _M0L8_2afieldS4978;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4978 = _M0L4selfS423->$0;
  moonbit_incref(_M0L8_2afieldS4978);
  return _M0L8_2afieldS4978;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS424
) {
  struct _M0TUsfE** _M0L8_2afieldS4979;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4979 = _M0L4selfS424->$0;
  moonbit_incref(_M0L8_2afieldS4979);
  return _M0L8_2afieldS4979;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView _M0L4selfS420,
  int32_t _M0L13start__offsetS421,
  int64_t _M0L11end__offsetS418
) {
  int32_t _M0L11end__offsetS417;
  int32_t _if__result_5546;
  #line 105 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS418 == 4294967296ll) {
    int32_t _M0L3endS2629 = _M0L4selfS420.$2;
    int32_t _M0L5startS2630 = _M0L4selfS420.$1;
    _M0L11end__offsetS417 = _M0L3endS2629 - _M0L5startS2630;
  } else {
    int64_t _M0L7_2aSomeS419 = _M0L11end__offsetS418;
    _M0L11end__offsetS417 = (int32_t)_M0L7_2aSomeS419;
  }
  if (_M0L13start__offsetS421 >= 0) {
    if (_M0L13start__offsetS421 <= _M0L11end__offsetS417) {
      int32_t _M0L3endS2622 = _M0L4selfS420.$2;
      int32_t _M0L5startS2623 = _M0L4selfS420.$1;
      int32_t _M0L6_2atmpS2621 = _M0L3endS2622 - _M0L5startS2623;
      _if__result_5546 = _M0L11end__offsetS417 <= _M0L6_2atmpS2621;
    } else {
      _if__result_5546 = 0;
    }
  } else {
    _if__result_5546 = 0;
  }
  if (_if__result_5546) {
    moonbit_string_t _M0L3strS2624 = _M0L4selfS420.$0;
    int32_t _M0L5startS2628 = _M0L4selfS420.$1;
    int32_t _M0L6_2atmpS2625 = _M0L5startS2628 + _M0L13start__offsetS421;
    int32_t _M0L5startS2627 = _M0L4selfS420.$1;
    int32_t _M0L6_2atmpS2626 = _M0L5startS2627 + _M0L11end__offsetS417;
    moonbit_incref(_M0L3strS2624);
    return (struct _M0TPC16string10StringView){.$0 = _M0L3strS2624,
                                                 .$1 = _M0L6_2atmpS2625,
                                                 .$2 = _M0L6_2atmpS2626};
  } else {
    #line 114 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_105.data);
  }
}

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(
  struct _M0TWEOUssE* _M0L1fS386,
  int64_t _M0L10size__hintS383
) {
  int64_t _M0L10size__hintS382;
  struct _M0TPB4IterGUssEE* _block_5547;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS383 == 4294967296ll) {
    _M0L10size__hintS382 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS384 = _M0L10size__hintS383;
    int32_t _M0L4_2anS385 = (int32_t)_M0L7_2aSomeS384;
    if (_M0L4_2anS385 > 0) {
      _M0L10size__hintS382 = (int64_t)_M0L4_2anS385;
    } else {
      _M0L10size__hintS382 = _M0MPB4Iter3newN6constrS9988GUssEE;
    }
  }
  moonbit_incref(_M0L1fS386);
  _block_5547
  = (struct _M0TPB4IterGUssEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUssEE));
  Moonbit_object_header(_block_5547)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 150, 0);
  _block_5547->$0 = _M0L1fS386;
  _block_5547->$1 = _M0L10size__hintS382;
  return _block_5547;
}

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L1fS391,
  int64_t _M0L10size__hintS388
) {
  int64_t _M0L10size__hintS387;
  struct _M0TPB4IterGRPC16string10StringViewE* _block_5548;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS388 == 4294967296ll) {
    _M0L10size__hintS387 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS389 = _M0L10size__hintS388;
    int32_t _M0L4_2anS390 = (int32_t)_M0L7_2aSomeS389;
    if (_M0L4_2anS390 > 0) {
      _M0L10size__hintS387 = (int64_t)_M0L4_2anS390;
    } else {
      _M0L10size__hintS387
      = _M0MPB4Iter3newN6constrS9988GRPC16string10StringViewE;
    }
  }
  moonbit_incref(_M0L1fS391);
  _block_5548
  = (struct _M0TPB4IterGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB4IterGRPC16string10StringViewE));
  Moonbit_object_header(_block_5548)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 143, 0);
  _block_5548->$0 = _M0L1fS391;
  _block_5548->$1 = _M0L10size__hintS387;
  return _block_5548;
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1fS396,
  int64_t _M0L10size__hintS393
) {
  int64_t _M0L10size__hintS392;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _block_5549;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS393 == 4294967296ll) {
    _M0L10size__hintS392 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS394 = _M0L10size__hintS393;
    int32_t _M0L4_2anS395 = (int32_t)_M0L7_2aSomeS394;
    if (_M0L4_2anS395 > 0) {
      _M0L10size__hintS392 = (int64_t)_M0L4_2anS395;
    } else {
      _M0L10size__hintS392
      = _M0MPB4Iter3newN6constrS9988GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
    }
  }
  moonbit_incref(_M0L1fS396);
  _block_5549
  = (struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_block_5549)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 153, 0);
  _block_5549->$0 = _M0L1fS396;
  _block_5549->$1 = _M0L10size__hintS392;
  return _block_5549;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS401,
  int64_t _M0L10size__hintS398
) {
  int64_t _M0L10size__hintS397;
  struct _M0TPB4IterGUsbEE* _block_5550;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS398 == 4294967296ll) {
    _M0L10size__hintS397 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS399 = _M0L10size__hintS398;
    int32_t _M0L4_2anS400 = (int32_t)_M0L7_2aSomeS399;
    if (_M0L4_2anS400 > 0) {
      _M0L10size__hintS397 = (int64_t)_M0L4_2anS400;
    } else {
      _M0L10size__hintS397 = _M0MPB4Iter3newN6constrS9988GUsbEE;
    }
  }
  moonbit_incref(_M0L1fS401);
  _block_5550
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_5550)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 156, 0);
  _block_5550->$0 = _M0L1fS401;
  _block_5550->$1 = _M0L10size__hintS397;
  return _block_5550;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS406,
  int64_t _M0L10size__hintS403
) {
  int64_t _M0L10size__hintS402;
  struct _M0TPB4IterGUsfEE* _block_5551;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS403 == 4294967296ll) {
    _M0L10size__hintS402 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS404 = _M0L10size__hintS403;
    int32_t _M0L4_2anS405 = (int32_t)_M0L7_2aSomeS404;
    if (_M0L4_2anS405 > 0) {
      _M0L10size__hintS402 = (int64_t)_M0L4_2anS405;
    } else {
      _M0L10size__hintS402 = _M0MPB4Iter3newN6constrS9988GUsfEE;
    }
  }
  moonbit_incref(_M0L1fS406);
  _block_5551
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_5551)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 159, 0);
  _block_5551->$0 = _M0L1fS406;
  _block_5551->$1 = _M0L10size__hintS402;
  return _block_5551;
}

struct _M0TPB4IterGUsiEE* _M0MPB4Iter3newGUsiEE(
  struct _M0TWEOUsiE* _M0L1fS411,
  int64_t _M0L10size__hintS408
) {
  int64_t _M0L10size__hintS407;
  struct _M0TPB4IterGUsiEE* _block_5552;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS408 == 4294967296ll) {
    _M0L10size__hintS407 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS409 = _M0L10size__hintS408;
    int32_t _M0L4_2anS410 = (int32_t)_M0L7_2aSomeS409;
    if (_M0L4_2anS410 > 0) {
      _M0L10size__hintS407 = (int64_t)_M0L4_2anS410;
    } else {
      _M0L10size__hintS407 = _M0MPB4Iter3newN6constrS9988GUsiEE;
    }
  }
  moonbit_incref(_M0L1fS411);
  _block_5552
  = (struct _M0TPB4IterGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsiEE));
  Moonbit_object_header(_block_5552)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 162, 0);
  _block_5552->$0 = _M0L1fS411;
  _block_5552->$1 = _M0L10size__hintS407;
  return _block_5552;
}

struct _M0TPB4IterGcE* _M0MPB4Iter3newGcE(
  struct _M0TWEOc* _M0L1fS416,
  int64_t _M0L10size__hintS413
) {
  int64_t _M0L10size__hintS412;
  struct _M0TPB4IterGcE* _block_5553;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS413 == 4294967296ll) {
    _M0L10size__hintS412 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS414 = _M0L10size__hintS413;
    int32_t _M0L4_2anS415 = (int32_t)_M0L7_2aSomeS414;
    if (_M0L4_2anS415 > 0) {
      _M0L10size__hintS412 = (int64_t)_M0L4_2anS415;
    } else {
      _M0L10size__hintS412 = _M0MPB4Iter3newN6constrS9988GcE;
    }
  }
  moonbit_incref(_M0L1fS416);
  _block_5553
  = (struct _M0TPB4IterGcE*)moonbit_malloc(sizeof(struct _M0TPB4IterGcE));
  Moonbit_object_header(_block_5553)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 165, 0);
  _block_5553->$0 = _M0L1fS416;
  _block_5553->$1 = _M0L10size__hintS412;
  return _block_5553;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS374,
  int32_t _M0L5radixS373
) {
  int32_t _if__result_5554;
  uint16_t* _M0L6bufferS375;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS373 < 2) {
    _if__result_5554 = 1;
  } else {
    _if__result_5554 = _M0L5radixS373 > 36;
  }
  if (_if__result_5554) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_106.data);
  }
  if (_M0L4selfS374 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  switch (_M0L5radixS373) {
    case 10: {
      int32_t _M0L3lenS376;
      uint16_t* _M0L6bufferS377;
      #line 622 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS376 = _M0FPB12dec__count64(_M0L4selfS374);
      _M0L6bufferS377 = (uint16_t*)moonbit_make_string(_M0L3lenS376, 0);
      #line 624 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS377, _M0L4selfS374, 0, _M0L3lenS376);
      _M0L6bufferS375 = _M0L6bufferS377;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS378;
      uint16_t* _M0L6bufferS379;
      #line 628 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS378 = _M0FPB12hex__count64(_M0L4selfS374);
      _M0L6bufferS379 = (uint16_t*)moonbit_make_string(_M0L3lenS378, 0);
      #line 630 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS379, _M0L4selfS374, 0, _M0L3lenS378);
      _M0L6bufferS375 = _M0L6bufferS379;
      break;
    }
    default: {
      int32_t _M0L3lenS380;
      uint16_t* _M0L6bufferS381;
      #line 634 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS380 = _M0FPB14radix__count64(_M0L4selfS374, _M0L5radixS373);
      _M0L6bufferS381 = (uint16_t*)moonbit_make_string(_M0L3lenS380, 0);
      #line 636 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS381, _M0L4selfS374, 0, _M0L3lenS380, _M0L5radixS373);
      _M0L6bufferS375 = _M0L6bufferS381;
      break;
    }
  }
  return _M0L6bufferS375;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS359,
  uint64_t _M0L3numS371,
  int32_t _M0L12digit__startS360,
  int32_t _M0L10total__lenS372
) {
  int32_t _M0L6_2atmpS2620;
  uint64_t _M0L3numS349;
  int32_t _M0L6offsetS350;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2620 = _M0L10total__lenS372 - _M0L12digit__startS360;
  _M0L3numS349 = _M0L3numS371;
  _M0L6offsetS350 = _M0L6_2atmpS2620;
  while (1) {
    if (_M0L3numS349 >= 10000ull) {
      uint64_t _M0L1tS351 = _M0L3numS349 / 10000ull;
      uint64_t _M0L6_2atmpS2597 = _M0L3numS349 % 10000ull;
      int32_t _M0L1rS352 = (int32_t)_M0L6_2atmpS2597;
      int32_t _M0L2d1S353 = _M0L1rS352 / 100;
      int32_t _M0L2d2S354 = _M0L1rS352 % 100;
      int32_t _M0L6_2atmpS2596 = _M0L2d1S353 / 10;
      int32_t _M0L6_2atmpS2595 = 48 + _M0L6_2atmpS2596;
      int32_t _M0L6d1__hiS355 = (uint16_t)_M0L6_2atmpS2595;
      int32_t _M0L6_2atmpS2594 = _M0L2d1S353 % 10;
      int32_t _M0L6_2atmpS2593 = 48 + _M0L6_2atmpS2594;
      int32_t _M0L6d1__loS356 = (uint16_t)_M0L6_2atmpS2593;
      int32_t _M0L6_2atmpS2592 = _M0L2d2S354 / 10;
      int32_t _M0L6_2atmpS2591 = 48 + _M0L6_2atmpS2592;
      int32_t _M0L6d2__hiS357 = (uint16_t)_M0L6_2atmpS2591;
      int32_t _M0L6_2atmpS2590 = _M0L2d2S354 % 10;
      int32_t _M0L6_2atmpS2589 = 48 + _M0L6_2atmpS2590;
      int32_t _M0L6d2__loS358 = (uint16_t)_M0L6_2atmpS2589;
      int32_t _M0L6_2atmpS2581 = _M0L12digit__startS360 + _M0L6offsetS350;
      int32_t _M0L6_2atmpS2580 = _M0L6_2atmpS2581 - 4;
      int32_t _M0L6_2atmpS2583;
      int32_t _M0L6_2atmpS2582;
      int32_t _M0L6_2atmpS2585;
      int32_t _M0L6_2atmpS2584;
      int32_t _M0L6_2atmpS2587;
      int32_t _M0L6_2atmpS2586;
      int32_t _M0L6_2atmpS2588;
      _M0L6bufferS359[_M0L6_2atmpS2580] = _M0L6d1__hiS355;
      _M0L6_2atmpS2583 = _M0L12digit__startS360 + _M0L6offsetS350;
      _M0L6_2atmpS2582 = _M0L6_2atmpS2583 - 3;
      _M0L6bufferS359[_M0L6_2atmpS2582] = _M0L6d1__loS356;
      _M0L6_2atmpS2585 = _M0L12digit__startS360 + _M0L6offsetS350;
      _M0L6_2atmpS2584 = _M0L6_2atmpS2585 - 2;
      _M0L6bufferS359[_M0L6_2atmpS2584] = _M0L6d2__hiS357;
      _M0L6_2atmpS2587 = _M0L12digit__startS360 + _M0L6offsetS350;
      _M0L6_2atmpS2586 = _M0L6_2atmpS2587 - 1;
      _M0L6bufferS359[_M0L6_2atmpS2586] = _M0L6d2__loS358;
      _M0L6_2atmpS2588 = _M0L6offsetS350 - 4;
      _M0L3numS349 = _M0L1tS351;
      _M0L6offsetS350 = _M0L6_2atmpS2588;
      continue;
    } else {
      int32_t _M0L6_2atmpS2619 = (int32_t)_M0L3numS349;
      int32_t _M0L9remainingS362 = _M0L6_2atmpS2619;
      int32_t _M0L6offsetS363 = _M0L6offsetS350;
      while (1) {
        if (_M0L9remainingS362 >= 100) {
          int32_t _M0L1tS364 = _M0L9remainingS362 / 100;
          int32_t _M0L1dS365 = _M0L9remainingS362 % 100;
          int32_t _M0L6_2atmpS2606 = _M0L1dS365 / 10;
          int32_t _M0L6_2atmpS2605 = 48 + _M0L6_2atmpS2606;
          int32_t _M0L5d__hiS366 = (uint16_t)_M0L6_2atmpS2605;
          int32_t _M0L6_2atmpS2604 = _M0L1dS365 % 10;
          int32_t _M0L6_2atmpS2603 = 48 + _M0L6_2atmpS2604;
          int32_t _M0L5d__loS367 = (uint16_t)_M0L6_2atmpS2603;
          int32_t _M0L6_2atmpS2599 = _M0L12digit__startS360 + _M0L6offsetS363;
          int32_t _M0L6_2atmpS2598 = _M0L6_2atmpS2599 - 2;
          int32_t _M0L6_2atmpS2601;
          int32_t _M0L6_2atmpS2600;
          int32_t _M0L6_2atmpS2602;
          _M0L6bufferS359[_M0L6_2atmpS2598] = _M0L5d__hiS366;
          _M0L6_2atmpS2601 = _M0L12digit__startS360 + _M0L6offsetS363;
          _M0L6_2atmpS2600 = _M0L6_2atmpS2601 - 1;
          _M0L6bufferS359[_M0L6_2atmpS2600] = _M0L5d__loS367;
          _M0L6_2atmpS2602 = _M0L6offsetS363 - 2;
          _M0L9remainingS362 = _M0L1tS364;
          _M0L6offsetS363 = _M0L6_2atmpS2602;
          continue;
        } else if (_M0L9remainingS362 >= 10) {
          int32_t _M0L6_2atmpS2614 = _M0L9remainingS362 / 10;
          int32_t _M0L6_2atmpS2613 = 48 + _M0L6_2atmpS2614;
          int32_t _M0L5d__hiS369 = (uint16_t)_M0L6_2atmpS2613;
          int32_t _M0L6_2atmpS2612 = _M0L9remainingS362 % 10;
          int32_t _M0L6_2atmpS2611 = 48 + _M0L6_2atmpS2612;
          int32_t _M0L5d__loS370 = (uint16_t)_M0L6_2atmpS2611;
          int32_t _M0L6_2atmpS2608 = _M0L12digit__startS360 + _M0L6offsetS363;
          int32_t _M0L6_2atmpS2607 = _M0L6_2atmpS2608 - 2;
          int32_t _M0L6_2atmpS2610;
          int32_t _M0L6_2atmpS2609;
          _M0L6bufferS359[_M0L6_2atmpS2607] = _M0L5d__hiS369;
          _M0L6_2atmpS2610 = _M0L12digit__startS360 + _M0L6offsetS363;
          _M0L6_2atmpS2609 = _M0L6_2atmpS2610 - 1;
          _M0L6bufferS359[_M0L6_2atmpS2609] = _M0L5d__loS370;
        } else {
          int32_t _M0L6_2atmpS2618 = _M0L12digit__startS360 + _M0L6offsetS363;
          int32_t _M0L6_2atmpS2615 = _M0L6_2atmpS2618 - 1;
          int32_t _M0L6_2atmpS2617 = 48 + _M0L9remainingS362;
          int32_t _M0L6_2atmpS2616 = (uint16_t)_M0L6_2atmpS2617;
          _M0L6bufferS359[_M0L6_2atmpS2615] = _M0L6_2atmpS2616;
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS339,
  uint64_t _M0L3numS343,
  int32_t _M0L12digit__startS340,
  int32_t _M0L10total__lenS342,
  int32_t _M0L5radixS333
) {
  uint64_t _M0L4baseS332;
  int32_t _M0L6_2atmpS2565;
  int32_t _M0L6_2atmpS2564;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS332 = _M0MPC13int3Int10to__uint64(_M0L5radixS333);
  _M0L6_2atmpS2565 = _M0L5radixS333 - 1;
  _M0L6_2atmpS2564 = _M0L5radixS333 & _M0L6_2atmpS2565;
  if (_M0L6_2atmpS2564 == 0) {
    int32_t _M0L5shiftS334;
    uint64_t _M0L4maskS335;
    int32_t _M0L6_2atmpS2572;
    int32_t _M0L6offsetS336;
    uint64_t _M0L1nS337;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS334 = moonbit_ctz32(_M0L5radixS333);
    _M0L4maskS335 = _M0L4baseS332 - 1ull;
    _M0L6_2atmpS2572 = _M0L10total__lenS342 - _M0L12digit__startS340;
    _M0L6offsetS336 = _M0L6_2atmpS2572;
    _M0L1nS337 = _M0L3numS343;
    while (1) {
      if (_M0L1nS337 > 0ull) {
        uint64_t _M0L6_2atmpS2571 = _M0L1nS337 & _M0L4maskS335;
        int32_t _M0L5digitS338 = (int32_t)_M0L6_2atmpS2571;
        int32_t _M0L6_2atmpS2568 = _M0L12digit__startS340 + _M0L6offsetS336;
        int32_t _M0L6_2atmpS2566 = _M0L6_2atmpS2568 - 1;
        int32_t _M0L6_2atmpS2567 =
          ((moonbit_string_t)moonbit_string_literal_107.data)[_M0L5digitS338];
        int32_t _M0L6_2atmpS2569;
        uint64_t _M0L6_2atmpS2570;
        _M0L6bufferS339[_M0L6_2atmpS2566] = _M0L6_2atmpS2567;
        _M0L6_2atmpS2569 = _M0L6offsetS336 - 1;
        _M0L6_2atmpS2570 = _M0L1nS337 >> (_M0L5shiftS334 & 63);
        _M0L6offsetS336 = _M0L6_2atmpS2569;
        _M0L1nS337 = _M0L6_2atmpS2570;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2579 = _M0L10total__lenS342 - _M0L12digit__startS340;
    int32_t _M0L6offsetS344 = _M0L6_2atmpS2579;
    uint64_t _M0L1nS345 = _M0L3numS343;
    while (1) {
      if (_M0L1nS345 > 0ull) {
        uint64_t _M0L1qS346 = _M0L1nS345 / _M0L4baseS332;
        uint64_t _M0L6_2atmpS2578 = _M0L1qS346 * _M0L4baseS332;
        uint64_t _M0L6_2atmpS2577 = _M0L1nS345 - _M0L6_2atmpS2578;
        int32_t _M0L5digitS347 = (int32_t)_M0L6_2atmpS2577;
        int32_t _M0L6_2atmpS2575 = _M0L12digit__startS340 + _M0L6offsetS344;
        int32_t _M0L6_2atmpS2573 = _M0L6_2atmpS2575 - 1;
        int32_t _M0L6_2atmpS2574 =
          ((moonbit_string_t)moonbit_string_literal_107.data)[_M0L5digitS347];
        int32_t _M0L6_2atmpS2576;
        _M0L6bufferS339[_M0L6_2atmpS2573] = _M0L6_2atmpS2574;
        _M0L6_2atmpS2576 = _M0L6offsetS344 - 1;
        _M0L6offsetS344 = _M0L6_2atmpS2576;
        _M0L1nS345 = _M0L1qS346;
        continue;
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS326,
  uint64_t _M0L3numS331,
  int32_t _M0L12digit__startS327,
  int32_t _M0L10total__lenS330
) {
  int32_t _M0L6_2atmpS2563;
  int32_t _M0L6offsetS321;
  uint64_t _M0L1nS322;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2563 = _M0L10total__lenS330 - _M0L12digit__startS327;
  _M0L6offsetS321 = _M0L6_2atmpS2563;
  _M0L1nS322 = _M0L3numS331;
  while (1) {
    if (_M0L6offsetS321 >= 2) {
      uint64_t _M0L6_2atmpS2560 = _M0L1nS322 & 255ull;
      int32_t _M0L9byte__valS323 = (int32_t)_M0L6_2atmpS2560;
      int32_t _M0L2hiS324 = _M0L9byte__valS323 / 16;
      int32_t _M0L2loS325 = _M0L9byte__valS323 % 16;
      int32_t _M0L6_2atmpS2554 = _M0L12digit__startS327 + _M0L6offsetS321;
      int32_t _M0L6_2atmpS2552 = _M0L6_2atmpS2554 - 2;
      int32_t _M0L6_2atmpS2553 =
        ((moonbit_string_t)moonbit_string_literal_107.data)[_M0L2hiS324];
      int32_t _M0L6_2atmpS2557;
      int32_t _M0L6_2atmpS2555;
      int32_t _M0L6_2atmpS2556;
      int32_t _M0L6_2atmpS2558;
      uint64_t _M0L6_2atmpS2559;
      _M0L6bufferS326[_M0L6_2atmpS2552] = _M0L6_2atmpS2553;
      _M0L6_2atmpS2557 = _M0L12digit__startS327 + _M0L6offsetS321;
      _M0L6_2atmpS2555 = _M0L6_2atmpS2557 - 1;
      _M0L6_2atmpS2556
      = ((moonbit_string_t)moonbit_string_literal_107.data)[
        _M0L2loS325
      ];
      _M0L6bufferS326[_M0L6_2atmpS2555] = _M0L6_2atmpS2556;
      _M0L6_2atmpS2558 = _M0L6offsetS321 - 2;
      _M0L6_2atmpS2559 = _M0L1nS322 >> 8;
      _M0L6offsetS321 = _M0L6_2atmpS2558;
      _M0L1nS322 = _M0L6_2atmpS2559;
      continue;
    } else if (_M0L6offsetS321 == 1) {
      uint64_t _M0L6_2atmpS2562 = _M0L1nS322 & 15ull;
      int32_t _M0L6nibbleS329 = (int32_t)_M0L6_2atmpS2562;
      int32_t _M0L6_2atmpS2561 =
        ((moonbit_string_t)moonbit_string_literal_107.data)[_M0L6nibbleS329];
      _M0L6bufferS326[_M0L12digit__startS327] = _M0L6_2atmpS2561;
    }
    break;
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS315,
  int32_t _M0L5radixS317
) {
  uint64_t _M0L4baseS316;
  uint64_t _M0L3numS318;
  int32_t _M0L5countS319;
  #line 419 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS315 == 0ull) {
    return 1;
  }
  #line 424 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS316 = _M0MPC13int3Int10to__uint64(_M0L5radixS317);
  _M0L3numS318 = _M0L5valueS315;
  _M0L5countS319 = 0;
  while (1) {
    if (_M0L3numS318 > 0ull) {
      uint64_t _M0L6_2atmpS2550 = _M0L3numS318 / _M0L4baseS316;
      int32_t _M0L6_2atmpS2551 = _M0L5countS319 + 1;
      _M0L3numS318 = _M0L6_2atmpS2550;
      _M0L5countS319 = _M0L6_2atmpS2551;
      continue;
    } else {
      return _M0L5countS319;
    }
    break;
  }
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS313) {
  #line 407 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS313 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS314;
    int32_t _M0L6_2atmpS2549;
    int32_t _M0L6_2atmpS2548;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS314 = moonbit_clz64(_M0L5valueS313);
    _M0L6_2atmpS2549 = 63 - _M0L14leading__zerosS314;
    _M0L6_2atmpS2548 = _M0L6_2atmpS2549 / 4;
    return _M0L6_2atmpS2548 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS312) {
  #line 343 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS312 >= 10000000000ull) {
    if (_M0L5valueS312 >= 100000000000000ull) {
      if (_M0L5valueS312 >= 10000000000000000ull) {
        if (_M0L5valueS312 >= 1000000000000000000ull) {
          if (_M0L5valueS312 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS312 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS312 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS312 >= 1000000000000ull) {
      if (_M0L5valueS312 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS312 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS312 >= 100000ull) {
    if (_M0L5valueS312 >= 10000000ull) {
      if (_M0L5valueS312 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS312 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS312 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS312 >= 1000ull) {
    if (_M0L5valueS312 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS312 >= 100ull) {
    return 3;
  } else if (_M0L5valueS312 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS296,
  int32_t _M0L5radixS295
) {
  int32_t _if__result_5561;
  int32_t _M0L12is__negativeS297;
  uint32_t _M0L3numS298;
  uint16_t* _M0L6bufferS299;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS295 < 2) {
    _if__result_5561 = 1;
  } else {
    _if__result_5561 = _M0L5radixS295 > 36;
  }
  if (_if__result_5561) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_106.data);
  }
  if (_M0L4selfS296 == 0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  _M0L12is__negativeS297 = _M0L4selfS296 < 0;
  if (_M0L12is__negativeS297) {
    int32_t _M0L6_2atmpS2547 = -_M0L4selfS296;
    _M0L3numS298 = *(uint32_t*)&_M0L6_2atmpS2547;
  } else {
    _M0L3numS298 = *(uint32_t*)&_M0L4selfS296;
  }
  switch (_M0L5radixS295) {
    case 10: {
      int32_t _M0L10digit__lenS300;
      int32_t _M0L6_2atmpS2544;
      int32_t _M0L10total__lenS301;
      uint16_t* _M0L6bufferS302;
      int32_t _M0L12digit__startS303;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS300 = _M0FPB12dec__count32(_M0L3numS298);
      if (_M0L12is__negativeS297) {
        _M0L6_2atmpS2544 = 1;
      } else {
        _M0L6_2atmpS2544 = 0;
      }
      _M0L10total__lenS301 = _M0L10digit__lenS300 + _M0L6_2atmpS2544;
      _M0L6bufferS302
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS301, 0);
      if (_M0L12is__negativeS297) {
        _M0L12digit__startS303 = 1;
      } else {
        _M0L12digit__startS303 = 0;
      }
      #line 239 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS302, _M0L3numS298, _M0L12digit__startS303, _M0L10total__lenS301);
      _M0L6bufferS299 = _M0L6bufferS302;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS304;
      int32_t _M0L6_2atmpS2545;
      int32_t _M0L10total__lenS305;
      uint16_t* _M0L6bufferS306;
      int32_t _M0L12digit__startS307;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS304 = _M0FPB12hex__count32(_M0L3numS298);
      if (_M0L12is__negativeS297) {
        _M0L6_2atmpS2545 = 1;
      } else {
        _M0L6_2atmpS2545 = 0;
      }
      _M0L10total__lenS305 = _M0L10digit__lenS304 + _M0L6_2atmpS2545;
      _M0L6bufferS306
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS305, 0);
      if (_M0L12is__negativeS297) {
        _M0L12digit__startS307 = 1;
      } else {
        _M0L12digit__startS307 = 0;
      }
      #line 247 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS306, _M0L3numS298, _M0L12digit__startS307, _M0L10total__lenS305);
      _M0L6bufferS299 = _M0L6bufferS306;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS308;
      int32_t _M0L6_2atmpS2546;
      int32_t _M0L10total__lenS309;
      uint16_t* _M0L6bufferS310;
      int32_t _M0L12digit__startS311;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS308
      = _M0FPB14radix__count32(_M0L3numS298, _M0L5radixS295);
      if (_M0L12is__negativeS297) {
        _M0L6_2atmpS2546 = 1;
      } else {
        _M0L6_2atmpS2546 = 0;
      }
      _M0L10total__lenS309 = _M0L10digit__lenS308 + _M0L6_2atmpS2546;
      _M0L6bufferS310
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS309, 0);
      if (_M0L12is__negativeS297) {
        _M0L12digit__startS311 = 1;
      } else {
        _M0L12digit__startS311 = 0;
      }
      #line 255 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS310, _M0L3numS298, _M0L12digit__startS311, _M0L10total__lenS309, _M0L5radixS295);
      _M0L6bufferS299 = _M0L6bufferS310;
      break;
    }
  }
  if (_M0L12is__negativeS297) {
    _M0L6bufferS299[0] = 45;
  }
  return _M0L6bufferS299;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS289,
  int32_t _M0L5radixS291
) {
  uint32_t _M0L4baseS290;
  uint32_t _M0L3numS292;
  int32_t _M0L5countS293;
  #line 189 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS289 == 0u) {
    return 1;
  }
  _M0L4baseS290 = *(uint32_t*)&_M0L5radixS291;
  _M0L3numS292 = _M0L5valueS289;
  _M0L5countS293 = 0;
  while (1) {
    if (_M0L3numS292 > 0u) {
      uint32_t _M0L6_2atmpS2542 = _M0L3numS292 / _M0L4baseS290;
      int32_t _M0L6_2atmpS2543 = _M0L5countS293 + 1;
      _M0L3numS292 = _M0L6_2atmpS2542;
      _M0L5countS293 = _M0L6_2atmpS2543;
      continue;
    } else {
      return _M0L5countS293;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS287) {
  #line 177 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS287 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS288;
    int32_t _M0L6_2atmpS2541;
    int32_t _M0L6_2atmpS2540;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS288 = moonbit_clz32(_M0L5valueS287);
    _M0L6_2atmpS2541 = 31 - _M0L14leading__zerosS288;
    _M0L6_2atmpS2540 = _M0L6_2atmpS2541 / 4;
    return _M0L6_2atmpS2540 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS286) {
  #line 143 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS286 >= 100000u) {
    if (_M0L5valueS286 >= 10000000u) {
      if (_M0L5valueS286 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS286 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS286 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS286 >= 1000u) {
    if (_M0L5valueS286 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS286 >= 100u) {
    return 3;
  } else if (_M0L5valueS286 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS272,
  uint32_t _M0L3numS284,
  int32_t _M0L12digit__startS273,
  int32_t _M0L10total__lenS285
) {
  int32_t _M0L6_2atmpS2539;
  uint32_t _M0L3numS262;
  int32_t _M0L6offsetS263;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2539 = _M0L10total__lenS285 - _M0L12digit__startS273;
  _M0L3numS262 = _M0L3numS284;
  _M0L6offsetS263 = _M0L6_2atmpS2539;
  while (1) {
    if (_M0L3numS262 >= 10000u) {
      uint32_t _M0L1tS264 = _M0L3numS262 / 10000u;
      uint32_t _M0L6_2atmpS2516 = _M0L3numS262 % 10000u;
      int32_t _M0L1rS265 = *(int32_t*)&_M0L6_2atmpS2516;
      int32_t _M0L2d1S266 = _M0L1rS265 / 100;
      int32_t _M0L2d2S267 = _M0L1rS265 % 100;
      int32_t _M0L6_2atmpS2515 = _M0L2d1S266 / 10;
      int32_t _M0L6_2atmpS2514 = 48 + _M0L6_2atmpS2515;
      int32_t _M0L6d1__hiS268 = (uint16_t)_M0L6_2atmpS2514;
      int32_t _M0L6_2atmpS2513 = _M0L2d1S266 % 10;
      int32_t _M0L6_2atmpS2512 = 48 + _M0L6_2atmpS2513;
      int32_t _M0L6d1__loS269 = (uint16_t)_M0L6_2atmpS2512;
      int32_t _M0L6_2atmpS2511 = _M0L2d2S267 / 10;
      int32_t _M0L6_2atmpS2510 = 48 + _M0L6_2atmpS2511;
      int32_t _M0L6d2__hiS270 = (uint16_t)_M0L6_2atmpS2510;
      int32_t _M0L6_2atmpS2509 = _M0L2d2S267 % 10;
      int32_t _M0L6_2atmpS2508 = 48 + _M0L6_2atmpS2509;
      int32_t _M0L6d2__loS271 = (uint16_t)_M0L6_2atmpS2508;
      int32_t _M0L6_2atmpS2500 = _M0L12digit__startS273 + _M0L6offsetS263;
      int32_t _M0L6_2atmpS2499 = _M0L6_2atmpS2500 - 4;
      int32_t _M0L6_2atmpS2502;
      int32_t _M0L6_2atmpS2501;
      int32_t _M0L6_2atmpS2504;
      int32_t _M0L6_2atmpS2503;
      int32_t _M0L6_2atmpS2506;
      int32_t _M0L6_2atmpS2505;
      int32_t _M0L6_2atmpS2507;
      _M0L6bufferS272[_M0L6_2atmpS2499] = _M0L6d1__hiS268;
      _M0L6_2atmpS2502 = _M0L12digit__startS273 + _M0L6offsetS263;
      _M0L6_2atmpS2501 = _M0L6_2atmpS2502 - 3;
      _M0L6bufferS272[_M0L6_2atmpS2501] = _M0L6d1__loS269;
      _M0L6_2atmpS2504 = _M0L12digit__startS273 + _M0L6offsetS263;
      _M0L6_2atmpS2503 = _M0L6_2atmpS2504 - 2;
      _M0L6bufferS272[_M0L6_2atmpS2503] = _M0L6d2__hiS270;
      _M0L6_2atmpS2506 = _M0L12digit__startS273 + _M0L6offsetS263;
      _M0L6_2atmpS2505 = _M0L6_2atmpS2506 - 1;
      _M0L6bufferS272[_M0L6_2atmpS2505] = _M0L6d2__loS271;
      _M0L6_2atmpS2507 = _M0L6offsetS263 - 4;
      _M0L3numS262 = _M0L1tS264;
      _M0L6offsetS263 = _M0L6_2atmpS2507;
      continue;
    } else {
      int32_t _M0L6_2atmpS2538 = *(int32_t*)&_M0L3numS262;
      int32_t _M0L9remainingS275 = _M0L6_2atmpS2538;
      int32_t _M0L6offsetS276 = _M0L6offsetS263;
      while (1) {
        if (_M0L9remainingS275 >= 100) {
          int32_t _M0L1tS277 = _M0L9remainingS275 / 100;
          int32_t _M0L1dS278 = _M0L9remainingS275 % 100;
          int32_t _M0L6_2atmpS2525 = _M0L1dS278 / 10;
          int32_t _M0L6_2atmpS2524 = 48 + _M0L6_2atmpS2525;
          int32_t _M0L5d__hiS279 = (uint16_t)_M0L6_2atmpS2524;
          int32_t _M0L6_2atmpS2523 = _M0L1dS278 % 10;
          int32_t _M0L6_2atmpS2522 = 48 + _M0L6_2atmpS2523;
          int32_t _M0L5d__loS280 = (uint16_t)_M0L6_2atmpS2522;
          int32_t _M0L6_2atmpS2518 = _M0L12digit__startS273 + _M0L6offsetS276;
          int32_t _M0L6_2atmpS2517 = _M0L6_2atmpS2518 - 2;
          int32_t _M0L6_2atmpS2520;
          int32_t _M0L6_2atmpS2519;
          int32_t _M0L6_2atmpS2521;
          _M0L6bufferS272[_M0L6_2atmpS2517] = _M0L5d__hiS279;
          _M0L6_2atmpS2520 = _M0L12digit__startS273 + _M0L6offsetS276;
          _M0L6_2atmpS2519 = _M0L6_2atmpS2520 - 1;
          _M0L6bufferS272[_M0L6_2atmpS2519] = _M0L5d__loS280;
          _M0L6_2atmpS2521 = _M0L6offsetS276 - 2;
          _M0L9remainingS275 = _M0L1tS277;
          _M0L6offsetS276 = _M0L6_2atmpS2521;
          continue;
        } else if (_M0L9remainingS275 >= 10) {
          int32_t _M0L6_2atmpS2533 = _M0L9remainingS275 / 10;
          int32_t _M0L6_2atmpS2532 = 48 + _M0L6_2atmpS2533;
          int32_t _M0L5d__hiS282 = (uint16_t)_M0L6_2atmpS2532;
          int32_t _M0L6_2atmpS2531 = _M0L9remainingS275 % 10;
          int32_t _M0L6_2atmpS2530 = 48 + _M0L6_2atmpS2531;
          int32_t _M0L5d__loS283 = (uint16_t)_M0L6_2atmpS2530;
          int32_t _M0L6_2atmpS2527 = _M0L12digit__startS273 + _M0L6offsetS276;
          int32_t _M0L6_2atmpS2526 = _M0L6_2atmpS2527 - 2;
          int32_t _M0L6_2atmpS2529;
          int32_t _M0L6_2atmpS2528;
          _M0L6bufferS272[_M0L6_2atmpS2526] = _M0L5d__hiS282;
          _M0L6_2atmpS2529 = _M0L12digit__startS273 + _M0L6offsetS276;
          _M0L6_2atmpS2528 = _M0L6_2atmpS2529 - 1;
          _M0L6bufferS272[_M0L6_2atmpS2528] = _M0L5d__loS283;
        } else {
          int32_t _M0L6_2atmpS2537 = _M0L12digit__startS273 + _M0L6offsetS276;
          int32_t _M0L6_2atmpS2534 = _M0L6_2atmpS2537 - 1;
          int32_t _M0L6_2atmpS2536 = 48 + _M0L9remainingS275;
          int32_t _M0L6_2atmpS2535 = (uint16_t)_M0L6_2atmpS2536;
          _M0L6bufferS272[_M0L6_2atmpS2534] = _M0L6_2atmpS2535;
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS252,
  uint32_t _M0L3numS256,
  int32_t _M0L12digit__startS253,
  int32_t _M0L10total__lenS255,
  int32_t _M0L5radixS246
) {
  uint32_t _M0L4baseS245;
  int32_t _M0L6_2atmpS2484;
  int32_t _M0L6_2atmpS2483;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS245 = *(uint32_t*)&_M0L5radixS246;
  _M0L6_2atmpS2484 = _M0L5radixS246 - 1;
  _M0L6_2atmpS2483 = _M0L5radixS246 & _M0L6_2atmpS2484;
  if (_M0L6_2atmpS2483 == 0) {
    int32_t _M0L5shiftS247;
    uint32_t _M0L4maskS248;
    int32_t _M0L6_2atmpS2491;
    int32_t _M0L6offsetS249;
    uint32_t _M0L1nS250;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS247 = moonbit_ctz32(_M0L5radixS246);
    _M0L4maskS248 = _M0L4baseS245 - 1u;
    _M0L6_2atmpS2491 = _M0L10total__lenS255 - _M0L12digit__startS253;
    _M0L6offsetS249 = _M0L6_2atmpS2491;
    _M0L1nS250 = _M0L3numS256;
    while (1) {
      if (_M0L1nS250 > 0u) {
        uint32_t _M0L6_2atmpS2490 = _M0L1nS250 & _M0L4maskS248;
        int32_t _M0L5digitS251 = *(int32_t*)&_M0L6_2atmpS2490;
        int32_t _M0L6_2atmpS2487 = _M0L12digit__startS253 + _M0L6offsetS249;
        int32_t _M0L6_2atmpS2485 = _M0L6_2atmpS2487 - 1;
        int32_t _M0L6_2atmpS2486 =
          ((moonbit_string_t)moonbit_string_literal_107.data)[_M0L5digitS251];
        int32_t _M0L6_2atmpS2488;
        uint32_t _M0L6_2atmpS2489;
        _M0L6bufferS252[_M0L6_2atmpS2485] = _M0L6_2atmpS2486;
        _M0L6_2atmpS2488 = _M0L6offsetS249 - 1;
        _M0L6_2atmpS2489 = _M0L1nS250 >> (_M0L5shiftS247 & 31);
        _M0L6offsetS249 = _M0L6_2atmpS2488;
        _M0L1nS250 = _M0L6_2atmpS2489;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2498 = _M0L10total__lenS255 - _M0L12digit__startS253;
    int32_t _M0L6offsetS257 = _M0L6_2atmpS2498;
    uint32_t _M0L1nS258 = _M0L3numS256;
    while (1) {
      if (_M0L1nS258 > 0u) {
        uint32_t _M0L1qS259 = _M0L1nS258 / _M0L4baseS245;
        uint32_t _M0L6_2atmpS2497 = _M0L1qS259 * _M0L4baseS245;
        uint32_t _M0L6_2atmpS2496 = _M0L1nS258 - _M0L6_2atmpS2497;
        int32_t _M0L5digitS260 = *(int32_t*)&_M0L6_2atmpS2496;
        int32_t _M0L6_2atmpS2494 = _M0L12digit__startS253 + _M0L6offsetS257;
        int32_t _M0L6_2atmpS2492 = _M0L6_2atmpS2494 - 1;
        int32_t _M0L6_2atmpS2493 =
          ((moonbit_string_t)moonbit_string_literal_107.data)[_M0L5digitS260];
        int32_t _M0L6_2atmpS2495;
        _M0L6bufferS252[_M0L6_2atmpS2492] = _M0L6_2atmpS2493;
        _M0L6_2atmpS2495 = _M0L6offsetS257 - 1;
        _M0L6offsetS257 = _M0L6_2atmpS2495;
        _M0L1nS258 = _M0L1qS259;
        continue;
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS239,
  uint32_t _M0L3numS244,
  int32_t _M0L12digit__startS240,
  int32_t _M0L10total__lenS243
) {
  int32_t _M0L6_2atmpS2482;
  int32_t _M0L6offsetS234;
  uint32_t _M0L1nS235;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2482 = _M0L10total__lenS243 - _M0L12digit__startS240;
  _M0L6offsetS234 = _M0L6_2atmpS2482;
  _M0L1nS235 = _M0L3numS244;
  while (1) {
    if (_M0L6offsetS234 >= 2) {
      uint32_t _M0L6_2atmpS2479 = _M0L1nS235 & 255u;
      int32_t _M0L9byte__valS236 = *(int32_t*)&_M0L6_2atmpS2479;
      int32_t _M0L2hiS237 = _M0L9byte__valS236 / 16;
      int32_t _M0L2loS238 = _M0L9byte__valS236 % 16;
      int32_t _M0L6_2atmpS2473 = _M0L12digit__startS240 + _M0L6offsetS234;
      int32_t _M0L6_2atmpS2471 = _M0L6_2atmpS2473 - 2;
      int32_t _M0L6_2atmpS2472 =
        ((moonbit_string_t)moonbit_string_literal_107.data)[_M0L2hiS237];
      int32_t _M0L6_2atmpS2476;
      int32_t _M0L6_2atmpS2474;
      int32_t _M0L6_2atmpS2475;
      int32_t _M0L6_2atmpS2477;
      uint32_t _M0L6_2atmpS2478;
      _M0L6bufferS239[_M0L6_2atmpS2471] = _M0L6_2atmpS2472;
      _M0L6_2atmpS2476 = _M0L12digit__startS240 + _M0L6offsetS234;
      _M0L6_2atmpS2474 = _M0L6_2atmpS2476 - 1;
      _M0L6_2atmpS2475
      = ((moonbit_string_t)moonbit_string_literal_107.data)[
        _M0L2loS238
      ];
      _M0L6bufferS239[_M0L6_2atmpS2474] = _M0L6_2atmpS2475;
      _M0L6_2atmpS2477 = _M0L6offsetS234 - 2;
      _M0L6_2atmpS2478 = _M0L1nS235 >> 8;
      _M0L6offsetS234 = _M0L6_2atmpS2477;
      _M0L1nS235 = _M0L6_2atmpS2478;
      continue;
    } else if (_M0L6offsetS234 == 1) {
      uint32_t _M0L6_2atmpS2481 = _M0L1nS235 & 15u;
      int32_t _M0L6nibbleS242 = *(int32_t*)&_M0L6_2atmpS2481;
      int32_t _M0L6_2atmpS2480 =
        ((moonbit_string_t)moonbit_string_literal_107.data)[_M0L6nibbleS242];
      _M0L6bufferS239[_M0L12digit__startS240] = _M0L6_2atmpS2480;
    }
    break;
  }
  return 0;
}

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TPB4IterGRPC16string10StringViewE* _M0L4selfS193
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L7_2afuncS192;
  void* _M0L6resultS194;
  int64_t _M0L7_2abindS195;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS192 = _M0L4selfS193->$0;
  moonbit_incref(_M0L7_2afuncS192);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS194 = _M0L7_2afuncS192->code(_M0L7_2afuncS192);
  moonbit_decref(_M0L7_2afuncS192);
  _M0L7_2abindS195 = _M0L4selfS193->$1;
  switch (Moonbit_object_tag(_M0L6resultS194)) {
    case 1: {
      if (_M0L7_2abindS195 == 4294967296ll) {
        
      } else {
        int64_t _M0L7_2aSomeS196 = _M0L7_2abindS195;
        int32_t _M0L4_2anS197 = (int32_t)_M0L7_2aSomeS196;
        int64_t _M0L6_2atmpS2457;
        if (_M0L4_2anS197 > 0) {
          int32_t _M0L6_2atmpS2458 = _M0L4_2anS197 - 1;
          _M0L6_2atmpS2457 = (int64_t)_M0L6_2atmpS2458;
        } else {
          _M0L6_2atmpS2457
          = _M0MPB4Iter4nextN6constrS9980GRPC16string10StringViewE;
        }
        _M0L4selfS193->$1 = _M0L6_2atmpS2457;
      }
      break;
    }
    default: {
      _M0L4selfS193->$1
      = _M0MPB4Iter4nextN6constrS9981GRPC16string10StringViewE;
      break;
    }
  }
  return _M0L6resultS194;
}

struct _M0TUssE* _M0MPB4Iter4nextGUssEE(
  struct _M0TPB4IterGUssEE* _M0L4selfS199
) {
  struct _M0TWEOUssE* _M0L7_2afuncS198;
  struct _M0TUssE* _M0L6resultS200;
  int64_t _M0L7_2abindS201;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS198 = _M0L4selfS199->$0;
  moonbit_incref(_M0L7_2afuncS198);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS200 = _M0L7_2afuncS198->code(_M0L7_2afuncS198);
  moonbit_decref(_M0L7_2afuncS198);
  _M0L7_2abindS201 = _M0L4selfS199->$1;
  if (_M0L6resultS200 == 0) {
    _M0L4selfS199->$1 = _M0MPB4Iter4nextN6constrS9981GUssEE;
  } else if (_M0L7_2abindS201 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS202 = _M0L7_2abindS201;
    int32_t _M0L4_2anS203 = (int32_t)_M0L7_2aSomeS202;
    int64_t _M0L6_2atmpS2459;
    if (_M0L4_2anS203 > 0) {
      int32_t _M0L6_2atmpS2460 = _M0L4_2anS203 - 1;
      _M0L6_2atmpS2459 = (int64_t)_M0L6_2atmpS2460;
    } else {
      _M0L6_2atmpS2459 = _M0MPB4Iter4nextN6constrS9980GUssEE;
    }
    _M0L4selfS199->$1 = _M0L6_2atmpS2459;
  }
  return _M0L6resultS200;
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L4selfS205
) {
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2afuncS204;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6resultS206;
  int64_t _M0L7_2abindS207;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS204 = _M0L4selfS205->$0;
  moonbit_incref(_M0L7_2afuncS204);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS206 = _M0L7_2afuncS204->code(_M0L7_2afuncS204);
  moonbit_decref(_M0L7_2afuncS204);
  _M0L7_2abindS207 = _M0L4selfS205->$1;
  if (_M0L6resultS206 == 0) {
    _M0L4selfS205->$1
    = _M0MPB4Iter4nextN6constrS9981GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
  } else if (_M0L7_2abindS207 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS208 = _M0L7_2abindS207;
    int32_t _M0L4_2anS209 = (int32_t)_M0L7_2aSomeS208;
    int64_t _M0L6_2atmpS2461;
    if (_M0L4_2anS209 > 0) {
      int32_t _M0L6_2atmpS2462 = _M0L4_2anS209 - 1;
      _M0L6_2atmpS2461 = (int64_t)_M0L6_2atmpS2462;
    } else {
      _M0L6_2atmpS2461
      = _M0MPB4Iter4nextN6constrS9980GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
    }
    _M0L4selfS205->$1 = _M0L6_2atmpS2461;
  }
  return _M0L6resultS206;
}

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS211
) {
  struct _M0TWEOUsbE* _M0L7_2afuncS210;
  struct _M0TUsbE* _M0L6resultS212;
  int64_t _M0L7_2abindS213;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS210 = _M0L4selfS211->$0;
  moonbit_incref(_M0L7_2afuncS210);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS212 = _M0L7_2afuncS210->code(_M0L7_2afuncS210);
  moonbit_decref(_M0L7_2afuncS210);
  _M0L7_2abindS213 = _M0L4selfS211->$1;
  if (_M0L6resultS212 == 0) {
    _M0L4selfS211->$1 = _M0MPB4Iter4nextN6constrS9981GUsbEE;
  } else if (_M0L7_2abindS213 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS214 = _M0L7_2abindS213;
    int32_t _M0L4_2anS215 = (int32_t)_M0L7_2aSomeS214;
    int64_t _M0L6_2atmpS2463;
    if (_M0L4_2anS215 > 0) {
      int32_t _M0L6_2atmpS2464 = _M0L4_2anS215 - 1;
      _M0L6_2atmpS2463 = (int64_t)_M0L6_2atmpS2464;
    } else {
      _M0L6_2atmpS2463 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS211->$1 = _M0L6_2atmpS2463;
  }
  return _M0L6resultS212;
}

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS217
) {
  struct _M0TWEOUsfE* _M0L7_2afuncS216;
  struct _M0TUsfE* _M0L6resultS218;
  int64_t _M0L7_2abindS219;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS216 = _M0L4selfS217->$0;
  moonbit_incref(_M0L7_2afuncS216);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS218 = _M0L7_2afuncS216->code(_M0L7_2afuncS216);
  moonbit_decref(_M0L7_2afuncS216);
  _M0L7_2abindS219 = _M0L4selfS217->$1;
  if (_M0L6resultS218 == 0) {
    _M0L4selfS217->$1 = _M0MPB4Iter4nextN6constrS9981GUsfEE;
  } else if (_M0L7_2abindS219 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS220 = _M0L7_2abindS219;
    int32_t _M0L4_2anS221 = (int32_t)_M0L7_2aSomeS220;
    int64_t _M0L6_2atmpS2465;
    if (_M0L4_2anS221 > 0) {
      int32_t _M0L6_2atmpS2466 = _M0L4_2anS221 - 1;
      _M0L6_2atmpS2465 = (int64_t)_M0L6_2atmpS2466;
    } else {
      _M0L6_2atmpS2465 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS217->$1 = _M0L6_2atmpS2465;
  }
  return _M0L6resultS218;
}

struct _M0TUsiE* _M0MPB4Iter4nextGUsiEE(
  struct _M0TPB4IterGUsiEE* _M0L4selfS223
) {
  struct _M0TWEOUsiE* _M0L7_2afuncS222;
  struct _M0TUsiE* _M0L6resultS224;
  int64_t _M0L7_2abindS225;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS222 = _M0L4selfS223->$0;
  moonbit_incref(_M0L7_2afuncS222);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS224 = _M0L7_2afuncS222->code(_M0L7_2afuncS222);
  moonbit_decref(_M0L7_2afuncS222);
  _M0L7_2abindS225 = _M0L4selfS223->$1;
  if (_M0L6resultS224 == 0) {
    _M0L4selfS223->$1 = _M0MPB4Iter4nextN6constrS9981GUsiEE;
  } else if (_M0L7_2abindS225 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS226 = _M0L7_2abindS225;
    int32_t _M0L4_2anS227 = (int32_t)_M0L7_2aSomeS226;
    int64_t _M0L6_2atmpS2467;
    if (_M0L4_2anS227 > 0) {
      int32_t _M0L6_2atmpS2468 = _M0L4_2anS227 - 1;
      _M0L6_2atmpS2467 = (int64_t)_M0L6_2atmpS2468;
    } else {
      _M0L6_2atmpS2467 = _M0MPB4Iter4nextN6constrS9980GUsiEE;
    }
    _M0L4selfS223->$1 = _M0L6_2atmpS2467;
  }
  return _M0L6resultS224;
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TPB4IterGcE* _M0L4selfS229) {
  struct _M0TWEOc* _M0L7_2afuncS228;
  int32_t _M0L6resultS230;
  int64_t _M0L7_2abindS231;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS228 = _M0L4selfS229->$0;
  moonbit_incref(_M0L7_2afuncS228);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS230 = _M0L7_2afuncS228->code(_M0L7_2afuncS228);
  moonbit_decref(_M0L7_2afuncS228);
  _M0L7_2abindS231 = _M0L4selfS229->$1;
  if (_M0L6resultS230 == -1) {
    _M0L4selfS229->$1 = _M0MPB4Iter4nextN6constrS9981GcE;
  } else if (_M0L7_2abindS231 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS232 = _M0L7_2abindS231;
    int32_t _M0L4_2anS233 = (int32_t)_M0L7_2aSomeS232;
    int64_t _M0L6_2atmpS2469;
    if (_M0L4_2anS233 > 0) {
      int32_t _M0L6_2atmpS2470 = _M0L4_2anS233 - 1;
      _M0L6_2atmpS2469 = (int64_t)_M0L6_2atmpS2470;
    } else {
      _M0L6_2atmpS2469 = _M0MPB4Iter4nextN6constrS9980GcE;
    }
    _M0L4selfS229->$1 = _M0L6_2atmpS2469;
  }
  return _M0L6resultS230;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS185,
  struct _M0TPB6Logger _M0L6loggerS184
) {
  moonbit_string_t _M0L6_2atmpS2453;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2453 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS185);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS184.$0->$method_0(_M0L6loggerS184.$1, _M0L6_2atmpS2453);
  moonbit_decref(_M0L6_2atmpS2453);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS187,
  struct _M0TPB6Logger _M0L6loggerS186
) {
  moonbit_string_t _M0L6_2atmpS2454;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2454 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS187);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS186.$0->$method_0(_M0L6loggerS186.$1, _M0L6_2atmpS2454);
  moonbit_decref(_M0L6_2atmpS2454);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS189,
  struct _M0TPB6Logger _M0L6loggerS188
) {
  moonbit_string_t _M0L6_2atmpS2455;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2455 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS189);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS188.$0->$method_0(_M0L6loggerS188.$1, _M0L6_2atmpS2455);
  moonbit_decref(_M0L6_2atmpS2455);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS191,
  struct _M0TPB6Logger _M0L6loggerS190
) {
  moonbit_string_t _M0L6_2atmpS2456;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2456 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS191);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS190.$0->$method_0(_M0L6loggerS190.$1, _M0L6_2atmpS2456);
  moonbit_decref(_M0L6_2atmpS2456);
  return 0;
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS183
) {
  #line 99 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  return _M0L4selfS183.$1;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS182
) {
  moonbit_string_t _M0L8_2afieldS4988;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS4988 = _M0L4selfS182.$0;
  moonbit_incref(_M0L8_2afieldS4988);
  return _M0L8_2afieldS4988;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS178,
  moonbit_string_t _M0L5valueS179,
  int32_t _M0L5startS180,
  int32_t _M0L3lenS181
) {
  int32_t _M0L6_2atmpS2452;
  int64_t _M0L6_2atmpS2451;
  struct _M0TPC16string10StringView _M0L6_2atmpS2450;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2452 = _M0L5startS180 + _M0L3lenS181;
  _M0L6_2atmpS2451 = (int64_t)_M0L6_2atmpS2452;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2450
  = _M0MPC16string6String11sub_2einner(_M0L5valueS179, _M0L5startS180, _M0L6_2atmpS2451);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS178, _M0L6_2atmpS2450);
  moonbit_decref(_M0L6_2atmpS2450.$0);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS171,
  int32_t _M0L5startS177,
  int64_t _M0L3endS173
) {
  int32_t _M0L3lenS170;
  int32_t _M0L3endS172;
  int32_t _M0L5startS176;
  int32_t _if__result_5568;
  #line 755 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS170 = Moonbit_array_length(_M0L4selfS171);
  if (_M0L3endS173 == 4294967296ll) {
    _M0L3endS172 = _M0L3lenS170;
  } else {
    int64_t _M0L7_2aSomeS174 = _M0L3endS173;
    int32_t _M0L6_2aendS175 = (int32_t)_M0L7_2aSomeS174;
    if (_M0L6_2aendS175 < 0) {
      _M0L3endS172 = _M0L3lenS170 + _M0L6_2aendS175;
    } else {
      _M0L3endS172 = _M0L6_2aendS175;
    }
  }
  if (_M0L5startS177 < 0) {
    _M0L5startS176 = _M0L3lenS170 + _M0L5startS177;
  } else {
    _M0L5startS176 = _M0L5startS177;
  }
  if (_M0L5startS176 >= 0) {
    if (_M0L5startS176 <= _M0L3endS172) {
      _if__result_5568 = _M0L3endS172 <= _M0L3lenS170;
    } else {
      _if__result_5568 = 0;
    }
  } else {
    _if__result_5568 = 0;
  }
  if (_if__result_5568) {
    if (_M0L5startS176 < _M0L3lenS170) {
      int32_t _M0L6_2atmpS2447 = _M0L4selfS171[_M0L5startS176];
      int32_t _M0L6_2atmpS2446;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2446
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2447);
      if (!_M0L6_2atmpS2446) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS172 < _M0L3lenS170) {
      int32_t _M0L6_2atmpS2449 = _M0L4selfS171[_M0L3endS172];
      int32_t _M0L6_2atmpS2448;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2448
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2449);
      if (!_M0L6_2atmpS2448) {
        
      } else {
        #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    moonbit_incref(_M0L4selfS171);
    return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS171,
                                                 .$1 = _M0L5startS176,
                                                 .$2 = _M0L3endS172};
  } else {
    #line 763 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS169,
  struct _M0TPB4Show _M0L4showS168
) {
  struct _M0TPB6Logger _M0L6_2atmpS2445;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS169);
  _M0L6_2atmpS2445
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS169
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS168.$0->$method_0(_M0L4showS168.$1, _M0L6_2atmpS2445);
  if (_M0L6_2atmpS2445.$1) {
    moonbit_decref(_M0L6_2atmpS2445.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS167,
  struct _M0TPB4Show _M0L4showS166
) {
  struct _M0TPB6Logger _M0L6_2atmpS2444;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS167);
  _M0L6_2atmpS2444
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS167
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS166.$0->$method_0(_M0L4showS166.$1, _M0L6_2atmpS2444);
  if (_M0L6_2atmpS2444.$1) {
    moonbit_decref(_M0L6_2atmpS2444.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS165) {
  uint32_t _M0L6_2atmpS2443;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2443 = _M0FPB14avalanche__acc(_M0L3accS165);
  return *(int32_t*)&_M0L6_2atmpS2443;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS164) {
  uint32_t _M0Lm3accS163;
  uint32_t _M0L6_2atmpS2432;
  uint32_t _M0L6_2atmpS2434;
  uint32_t _M0L6_2atmpS2433;
  uint32_t _M0L6_2atmpS2435;
  uint32_t _M0L6_2atmpS2436;
  uint32_t _M0L6_2atmpS2438;
  uint32_t _M0L6_2atmpS2437;
  uint32_t _M0L6_2atmpS2439;
  uint32_t _M0L6_2atmpS2440;
  uint32_t _M0L6_2atmpS2442;
  uint32_t _M0L6_2atmpS2441;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS163 = _M0L3accS164;
  _M0L6_2atmpS2432 = _M0Lm3accS163;
  _M0L6_2atmpS2434 = _M0Lm3accS163;
  _M0L6_2atmpS2433 = _M0L6_2atmpS2434 >> 15;
  _M0Lm3accS163 = _M0L6_2atmpS2432 ^ _M0L6_2atmpS2433;
  _M0L6_2atmpS2435 = _M0Lm3accS163;
  _M0Lm3accS163 = _M0L6_2atmpS2435 * 2246822519u;
  _M0L6_2atmpS2436 = _M0Lm3accS163;
  _M0L6_2atmpS2438 = _M0Lm3accS163;
  _M0L6_2atmpS2437 = _M0L6_2atmpS2438 >> 13;
  _M0Lm3accS163 = _M0L6_2atmpS2436 ^ _M0L6_2atmpS2437;
  _M0L6_2atmpS2439 = _M0Lm3accS163;
  _M0Lm3accS163 = _M0L6_2atmpS2439 * 3266489917u;
  _M0L6_2atmpS2440 = _M0Lm3accS163;
  _M0L6_2atmpS2442 = _M0Lm3accS163;
  _M0L6_2atmpS2441 = _M0L6_2atmpS2442 >> 16;
  _M0Lm3accS163 = _M0L6_2atmpS2440 ^ _M0L6_2atmpS2441;
  return _M0Lm3accS163;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS162) {
  int64_t _M0L6_2atmpS2431;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2431 = (int64_t)_M0L4selfS162;
  return *(uint64_t*)&_M0L6_2atmpS2431;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS161,
  moonbit_string_t _M0L3strS160
) {
  int32_t _M0L8str__lenS159;
  int32_t _M0L3lenS2426;
  int32_t _M0L6_2atmpS2425;
  uint16_t* _M0L4dataS2427;
  int32_t _M0L3lenS2428;
  int32_t _M0L3lenS2430;
  int32_t _M0L6_2atmpS2429;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS159 = Moonbit_array_length(_M0L3strS160);
  _M0L3lenS2426 = _M0L4selfS161->$1;
  _M0L6_2atmpS2425 = _M0L3lenS2426 + _M0L8str__lenS159;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS161, _M0L6_2atmpS2425);
  _M0L4dataS2427 = _M0L4selfS161->$0;
  _M0L3lenS2428 = _M0L4selfS161->$1;
  moonbit_incref(_M0L4dataS2427);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2427, _M0L3lenS2428, _M0L3strS160, 0, _M0L8str__lenS159);
  moonbit_decref(_M0L4dataS2427);
  _M0L3lenS2430 = _M0L4selfS161->$1;
  _M0L6_2atmpS2429 = _M0L3lenS2430 + _M0L8str__lenS159;
  _M0L4selfS161->$1 = _M0L6_2atmpS2429;
  return 0;
}

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t* _M0L4selfS155,
  int32_t _M0L11dst__offsetS158,
  moonbit_string_t _M0L3strS156,
  int32_t _M0L11str__offsetS151,
  int32_t _M0L3lenS152
) {
  int32_t _M0L16end__str__offsetS150;
  int32_t _M0L1iS153;
  int32_t _M0L1jS154;
  #line 71 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L16end__str__offsetS150 = _M0L11str__offsetS151 + _M0L3lenS152;
  _M0L1iS153 = _M0L11str__offsetS151;
  _M0L1jS154 = _M0L11dst__offsetS158;
  while (1) {
    if (_M0L1iS153 < _M0L16end__str__offsetS150) {
      int32_t _M0L6_2atmpS2422 = _M0L3strS156[_M0L1iS153];
      int32_t _M0L6_2atmpS2423;
      int32_t _M0L6_2atmpS2424;
      if (
        _M0L1jS154 < 0 || _M0L1jS154 >= Moonbit_array_length(_M0L4selfS155)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS155[_M0L1jS154] = _M0L6_2atmpS2422;
      _M0L6_2atmpS2423 = _M0L1iS153 + 1;
      _M0L6_2atmpS2424 = _M0L1jS154 + 1;
      _M0L1iS153 = _M0L6_2atmpS2423;
      _M0L1jS154 = _M0L6_2atmpS2424;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS149) {
  int32_t _M0L6_2atmpS2421;
  #line 68 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS2421 = (int32_t)_M0L4selfS149;
  return _M0L6_2atmpS2421;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS147,
  int32_t _M0L8trailingS148
) {
  int32_t _M0L6_2atmpS2420;
  int32_t _M0L6_2atmpS2419;
  int32_t _M0L6_2atmpS2418;
  int32_t _M0L6_2atmpS2417;
  int32_t _M0L6_2atmpS2416;
  #line 40 "/home/developer/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS2420 = _M0L7leadingS147 - 55296;
  _M0L6_2atmpS2419 = _M0L6_2atmpS2420 * 1024;
  _M0L6_2atmpS2418 = _M0L6_2atmpS2419 + _M0L8trailingS148;
  _M0L6_2atmpS2417 = _M0L6_2atmpS2418 - 56320;
  _M0L6_2atmpS2416 = _M0L6_2atmpS2417 + 65536;
  return _M0L6_2atmpS2416;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS146) {
  #line 45 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS146 >= 56320) {
    return _M0L4selfS146 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS145) {
  #line 28 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS145 >= 55296) {
    return _M0L4selfS145 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS143,
  int32_t _M0L2chS142
) {
  uint32_t _M0L4codeS141;
  #line 95 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  #line 96 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4codeS141 = _M0MPC14char4Char8to__uint(_M0L2chS142);
  if (_M0L4codeS141 <= 65535u) {
    int32_t _M0L3lenS2395 = _M0L4selfS143->$1;
    int32_t _M0L6_2atmpS2394 = _M0L3lenS2395 + 1;
    uint16_t* _M0L4dataS2396;
    int32_t _M0L3lenS2397;
    int32_t _M0L6_2atmpS2398;
    int32_t _M0L3lenS2400;
    int32_t _M0L6_2atmpS2399;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS143, _M0L6_2atmpS2394);
    _M0L4dataS2396 = _M0L4selfS143->$0;
    _M0L3lenS2397 = _M0L4selfS143->$1;
    moonbit_incref(_M0L4dataS2396);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2398 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS141);
    if (
      _M0L3lenS2397 < 0
      || _M0L3lenS2397 >= Moonbit_array_length(_M0L4dataS2396)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2396[_M0L3lenS2397] = _M0L6_2atmpS2398;
    moonbit_decref(_M0L4dataS2396);
    _M0L3lenS2400 = _M0L4selfS143->$1;
    _M0L6_2atmpS2399 = _M0L3lenS2400 + 1;
    _M0L4selfS143->$1 = _M0L6_2atmpS2399;
  } else if (_M0L4codeS141 <= 1114111u) {
    int32_t _M0L3lenS2402 = _M0L4selfS143->$1;
    int32_t _M0L6_2atmpS2401 = _M0L3lenS2402 + 2;
    uint32_t _M0L4codeS144;
    uint16_t* _M0L4dataS2403;
    int32_t _M0L3lenS2404;
    uint32_t _M0L6_2atmpS2407;
    uint32_t _M0L6_2atmpS2406;
    int32_t _M0L6_2atmpS2405;
    uint16_t* _M0L4dataS2408;
    int32_t _M0L3lenS2413;
    int32_t _M0L6_2atmpS2409;
    uint32_t _M0L6_2atmpS2412;
    uint32_t _M0L6_2atmpS2411;
    int32_t _M0L6_2atmpS2410;
    int32_t _M0L3lenS2415;
    int32_t _M0L6_2atmpS2414;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS143, _M0L6_2atmpS2401);
    _M0L4codeS144 = _M0L4codeS141 - 65536u;
    _M0L4dataS2403 = _M0L4selfS143->$0;
    _M0L3lenS2404 = _M0L4selfS143->$1;
    _M0L6_2atmpS2407 = _M0L4codeS144 >> 10;
    _M0L6_2atmpS2406 = 55296u + _M0L6_2atmpS2407;
    moonbit_incref(_M0L4dataS2403);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2405 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2406);
    if (
      _M0L3lenS2404 < 0
      || _M0L3lenS2404 >= Moonbit_array_length(_M0L4dataS2403)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2403[_M0L3lenS2404] = _M0L6_2atmpS2405;
    moonbit_decref(_M0L4dataS2403);
    _M0L4dataS2408 = _M0L4selfS143->$0;
    _M0L3lenS2413 = _M0L4selfS143->$1;
    _M0L6_2atmpS2409 = _M0L3lenS2413 + 1;
    _M0L6_2atmpS2412 = _M0L4codeS144 & 1023u;
    _M0L6_2atmpS2411 = 56320u + _M0L6_2atmpS2412;
    moonbit_incref(_M0L4dataS2408);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2410 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2411);
    if (
      _M0L6_2atmpS2409 < 0
      || _M0L6_2atmpS2409 >= Moonbit_array_length(_M0L4dataS2408)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2408[_M0L6_2atmpS2409] = _M0L6_2atmpS2410;
    moonbit_decref(_M0L4dataS2408);
    _M0L3lenS2415 = _M0L4selfS143->$1;
    _M0L6_2atmpS2414 = _M0L3lenS2415 + 2;
    _M0L4selfS143->$1 = _M0L6_2atmpS2414;
  } else {
    #line 108 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_108.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS135,
  int32_t _M0L8requiredS136
) {
  uint16_t* _M0L4dataS2393;
  int32_t _M0L12current__lenS134;
  int32_t _M0L13enough__spaceS137;
  int32_t _M0L13enough__spaceS138;
  uint16_t* _M0L4dataS2389;
  int32_t _M0L6_2atmpS2390;
  int32_t _M0L3lenS2391;
  uint16_t* _M0L9new__dataS140;
  uint16_t* _M0L6_2aoldS4993;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS2393 = _M0L4selfS135->$0;
  _M0L12current__lenS134 = Moonbit_array_length(_M0L4dataS2393);
  if (_M0L8requiredS136 <= _M0L12current__lenS134) {
    return 0;
  }
  _M0L13enough__spaceS138 = _M0L12current__lenS134;
  while (1) {
    if (_M0L13enough__spaceS138 < _M0L8requiredS136) {
      int32_t _M0L6_2atmpS2392 = _M0L13enough__spaceS138 * 2;
      _M0L13enough__spaceS138 = _M0L6_2atmpS2392;
      continue;
    } else {
      _M0L13enough__spaceS137 = _M0L13enough__spaceS138;
    }
    break;
  }
  _M0L4dataS2389 = _M0L4selfS135->$0;
  moonbit_incref(_M0L4dataS2389);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2390 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS2391 = _M0L4selfS135->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS140
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2389, _M0L13enough__spaceS137, _M0L6_2atmpS2390, _M0L3lenS2391, 0, 0);
  moonbit_decref(_M0L4dataS2389);
  _M0L6_2aoldS4993 = _M0L4selfS135->$0;
  moonbit_decref(_M0L6_2aoldS4993);
  _M0L4selfS135->$0 = _M0L9new__dataS140;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS133) {
  int32_t _M0L6_2atmpS2388;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2388 = *(int32_t*)&_M0L4selfS133;
  return (uint16_t)_M0L6_2atmpS2388;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS132) {
  int32_t _M0L6_2atmpS2387;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2387 = _M0L4selfS132;
  return *(uint32_t*)&_M0L6_2atmpS2387;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS130
) {
  int32_t _M0L3lenS2379;
  uint16_t* _M0L4dataS2381;
  int32_t _M0L6_2atmpS2380;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS2379 = _M0L4selfS130->$1;
  _M0L4dataS2381 = _M0L4selfS130->$0;
  _M0L6_2atmpS2380 = Moonbit_array_length(_M0L4dataS2381);
  if (_M0L3lenS2379 == _M0L6_2atmpS2380) {
    uint16_t* _M0L4dataS2382 = _M0L4selfS130->$0;
    moonbit_incref(_M0L4dataS2382);
    return _M0L4dataS2382;
  } else {
    uint16_t* _M0L4dataS2383 = _M0L4selfS130->$0;
    int32_t _M0L3lenS2384 = _M0L4selfS130->$1;
    int32_t _M0L6_2atmpS2385;
    int32_t _M0L3lenS2386;
    uint16_t* _M0L4dataS131;
    moonbit_incref(_M0L4dataS2383);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2385 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS2386 = _M0L4selfS130->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS131
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2383, _M0L3lenS2384, _M0L6_2atmpS2385, _M0L3lenS2386, 0, 0);
    moonbit_decref(_M0L4dataS2383);
    return _M0L4dataS131;
  }
}

int32_t _M0IPC16uint166UInt16PB7Default7default() {
  #line 176 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  return 0;
}

uint16_t* _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(
  uint16_t* _M0L3srcS127,
  int32_t _M0L13allocate__lenS123,
  int32_t _M0L4initS128,
  int32_t _M0L3lenS124,
  int32_t _M0L11src__offsetS125,
  int32_t _M0L11dst__offsetS126
) {
  int32_t _if__result_5571;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS123 >= 0) {
    if (_M0L3lenS124 >= 0) {
      if (_M0L11src__offsetS125 >= 0) {
        if (_M0L11dst__offsetS126 >= 0) {
          int32_t _M0L6_2atmpS2375 = _M0L11src__offsetS125 + _M0L3lenS124;
          int32_t _M0L6_2atmpS2376 = Moonbit_array_length(_M0L3srcS127);
          if (_M0L6_2atmpS2375 <= _M0L6_2atmpS2376) {
            int32_t _M0L6_2atmpS2374 = _M0L11dst__offsetS126 + _M0L3lenS124;
            _if__result_5571 = _M0L6_2atmpS2374 <= _M0L13allocate__lenS123;
          } else {
            _if__result_5571 = 0;
          }
        } else {
          _if__result_5571 = 0;
        }
      } else {
        _if__result_5571 = 0;
      }
    } else {
      _if__result_5571 = 0;
    }
  } else {
    _if__result_5571 = 0;
  }
  if (_if__result_5571) {
    moonbit_incref(_M0L3srcS127);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS127, _M0L13allocate__lenS123, _M0L4initS128, _M0L11src__offsetS125, _M0L11dst__offsetS126, _M0L3lenS124);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS129;
    int32_t _M0L6_2atmpS2378;
    moonbit_string_t _M0L6_2atmpS2377;
    uint16_t* _result_5572;
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L18_2astring__builderS129
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS129, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS129, _M0L13allocate__lenS123);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS129, (moonbit_string_t)moonbit_string_literal_110.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS129, _M0L11src__offsetS125);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS129, (moonbit_string_t)moonbit_string_literal_111.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS129, _M0L11dst__offsetS126);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS129, (moonbit_string_t)moonbit_string_literal_112.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS129, _M0L3lenS124);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS129, (moonbit_string_t)moonbit_string_literal_113.data);
    _M0L6_2atmpS2378 = Moonbit_array_length(_M0L3srcS127);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS129, _M0L6_2atmpS2378);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS2377
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS129);
    moonbit_decref(_M0L18_2astring__builderS129);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_5572 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS2377);
    moonbit_decref(_M0L6_2atmpS2377);
    return _result_5572;
  }
}

uint16_t* _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(
  uint16_t* _M0L3srcS120,
  int32_t _M0L13allocate__lenS117,
  int32_t _M0L4initS118,
  int32_t _M0L11src__offsetS121,
  int32_t _M0L11dst__offsetS119,
  int32_t _M0L9blit__lenS122
) {
  uint16_t* _M0L3dstS116;
  #line 79 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  _M0L3dstS116
  = (uint16_t*)moonbit_make_string(_M0L13allocate__lenS117, _M0L4initS118);
  moonbit_incref(_M0L3dstS116);
  #line 90 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  moonbit_unsafe_val_array_blit(_M0L3dstS116, _M0L11dst__offsetS119, _M0L3srcS120, _M0L11src__offsetS121, _M0L9blit__lenS122, sizeof(uint16_t));
  return _M0L3dstS116;
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder21StringBuilder_2einner(
  int32_t _M0L10size__hintS114
) {
  int32_t _M0L7initialS113;
  uint16_t* _M0L4dataS115;
  struct _M0TPB13StringBuilder* _block_5573;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS114 < 1) {
    _M0L7initialS113 = 1;
  } else {
    int32_t _M0L6_2atmpS2373 = _M0L10size__hintS114 + 1;
    _M0L7initialS113 = _M0L6_2atmpS2373 / 2;
  }
  _M0L4dataS115 = (uint16_t*)moonbit_make_string(_M0L7initialS113, 0);
  _block_5573
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_5573)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 168, 0);
  _block_5573->$0 = _M0L4dataS115;
  _block_5573->$1 = 0;
  return _block_5573;
}

int32_t _M0MPC13int3Int16unsafe__to__char(int32_t _M0L4selfS112) {
  #line 1532 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return _M0L4selfS112;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS98,
  int32_t _M0L13allocate__lenS94,
  int32_t _M0L3lenS95,
  int32_t _M0L11src__offsetS96,
  int32_t _M0L11dst__offsetS97
) {
  int32_t _if__result_5574;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS94 >= 0) {
    if (_M0L3lenS95 >= 0) {
      if (_M0L11src__offsetS96 >= 0) {
        if (_M0L11dst__offsetS97 >= 0) {
          int32_t _M0L6_2atmpS2359 = _M0L11src__offsetS96 + _M0L3lenS95;
          int32_t _M0L6_2atmpS2360;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2360
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS98);
          if (_M0L6_2atmpS2359 <= _M0L6_2atmpS2360) {
            int32_t _M0L6_2atmpS2358 = _M0L11dst__offsetS97 + _M0L3lenS95;
            _if__result_5574 = _M0L6_2atmpS2358 <= _M0L13allocate__lenS94;
          } else {
            _if__result_5574 = 0;
          }
        } else {
          _if__result_5574 = 0;
        }
      } else {
        _if__result_5574 = 0;
      }
    } else {
      _if__result_5574 = 0;
    }
  } else {
    _if__result_5574 = 0;
  }
  if (_if__result_5574) {
    moonbit_incref(_M0L3srcS98);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS94, (moonbit_string_t)moonbit_string_literal_75.data, _M0L3srcS98, _M0L11src__offsetS96, _M0L11dst__offsetS97, _M0L3lenS95);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS99;
    int32_t _M0L6_2atmpS2362;
    moonbit_string_t _M0L6_2atmpS2361;
    moonbit_string_t* _result_5575;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS99
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS99, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS99, _M0L13allocate__lenS94);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS99, (moonbit_string_t)moonbit_string_literal_110.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS99, _M0L11src__offsetS96);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS99, (moonbit_string_t)moonbit_string_literal_111.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS99, _M0L11dst__offsetS97);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS99, (moonbit_string_t)moonbit_string_literal_112.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS99, _M0L3lenS95);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS99, (moonbit_string_t)moonbit_string_literal_113.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2362 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS98);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS99, _M0L6_2atmpS2362);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2361
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS99);
    moonbit_decref(_M0L18_2astring__builderS99);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_5575
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS2361);
    moonbit_decref(_M0L6_2atmpS2361);
    return _result_5575;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS104,
  int32_t _M0L13allocate__lenS100,
  int32_t _M0L3lenS101,
  int32_t _M0L11src__offsetS102,
  int32_t _M0L11dst__offsetS103
) {
  int32_t _if__result_5576;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS100 >= 0) {
    if (_M0L3lenS101 >= 0) {
      if (_M0L11src__offsetS102 >= 0) {
        if (_M0L11dst__offsetS103 >= 0) {
          int32_t _M0L6_2atmpS2364 = _M0L11src__offsetS102 + _M0L3lenS101;
          int32_t _M0L6_2atmpS2365;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2365
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS104);
          if (_M0L6_2atmpS2364 <= _M0L6_2atmpS2365) {
            int32_t _M0L6_2atmpS2363 = _M0L11dst__offsetS103 + _M0L3lenS101;
            _if__result_5576 = _M0L6_2atmpS2363 <= _M0L13allocate__lenS100;
          } else {
            _if__result_5576 = 0;
          }
        } else {
          _if__result_5576 = 0;
        }
      } else {
        _if__result_5576 = 0;
      }
    } else {
      _if__result_5576 = 0;
    }
  } else {
    _if__result_5576 = 0;
  }
  if (_if__result_5576) {
    moonbit_incref(_M0L3srcS104);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS100, 0, _M0L3srcS104, _M0L11src__offsetS102, _M0L11dst__offsetS103, _M0L3lenS101);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS105;
    int32_t _M0L6_2atmpS2367;
    moonbit_string_t _M0L6_2atmpS2366;
    struct _M0TUsfE** _result_5577;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS105
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS105, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS105, _M0L13allocate__lenS100);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS105, (moonbit_string_t)moonbit_string_literal_110.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS105, _M0L11src__offsetS102);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS105, (moonbit_string_t)moonbit_string_literal_111.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS105, _M0L11dst__offsetS103);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS105, (moonbit_string_t)moonbit_string_literal_112.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS105, _M0L3lenS101);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS105, (moonbit_string_t)moonbit_string_literal_113.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2367 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS104);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS105, _M0L6_2atmpS2367);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2366
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS105);
    moonbit_decref(_M0L18_2astring__builderS105);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_5577
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS2366);
    moonbit_decref(_M0L6_2atmpS2366);
    return _result_5577;
  }
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t* _M0L3srcS110,
  int32_t _M0L13allocate__lenS106,
  int32_t _M0L3lenS107,
  int32_t _M0L11src__offsetS108,
  int32_t _M0L11dst__offsetS109
) {
  int32_t _if__result_5578;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS106 >= 0) {
    if (_M0L3lenS107 >= 0) {
      if (_M0L11src__offsetS108 >= 0) {
        if (_M0L11dst__offsetS109 >= 0) {
          int32_t _M0L6_2atmpS2369 = _M0L11src__offsetS108 + _M0L3lenS107;
          int32_t _M0L6_2atmpS2370;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2370
          = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS110);
          if (_M0L6_2atmpS2369 <= _M0L6_2atmpS2370) {
            int32_t _M0L6_2atmpS2368 = _M0L11dst__offsetS109 + _M0L3lenS107;
            _if__result_5578 = _M0L6_2atmpS2368 <= _M0L13allocate__lenS106;
          } else {
            _if__result_5578 = 0;
          }
        } else {
          _if__result_5578 = 0;
        }
      } else {
        _if__result_5578 = 0;
      }
    } else {
      _if__result_5578 = 0;
    }
  } else {
    _if__result_5578 = 0;
  }
  if (_if__result_5578) {
    moonbit_incref(_M0L3srcS110);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS106, 0, _M0L3srcS110, _M0L11src__offsetS108, _M0L11dst__offsetS109, _M0L3lenS107);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS111;
    int32_t _M0L6_2atmpS2372;
    moonbit_string_t _M0L6_2atmpS2371;
    moonbit_string_t* _result_5579;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS111
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS111, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS111, _M0L13allocate__lenS106);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS111, (moonbit_string_t)moonbit_string_literal_110.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS111, _M0L11src__offsetS108);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS111, (moonbit_string_t)moonbit_string_literal_111.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS111, _M0L11dst__offsetS109);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS111, (moonbit_string_t)moonbit_string_literal_112.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS111, _M0L3lenS107);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS111, (moonbit_string_t)moonbit_string_literal_113.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2372 = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS110);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS111, _M0L6_2atmpS2372);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2371
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS111);
    moonbit_decref(_M0L18_2astring__builderS111);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_5579
    = _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(_M0L6_2atmpS2371);
    moonbit_decref(_M0L6_2atmpS2371);
    return _result_5579;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS85,
  moonbit_string_t _M0L3objS84
) {
  struct _M0TPB6Logger _M0L6_2atmpS2353;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS85);
  _M0L6_2atmpS2353
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS85
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS84, _M0L6_2atmpS2353);
  if (_M0L6_2atmpS2353.$1) {
    moonbit_decref(_M0L6_2atmpS2353.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS87,
  int32_t _M0L3objS86
) {
  struct _M0TPB6Logger _M0L6_2atmpS2354;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS87);
  _M0L6_2atmpS2354
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS87
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS86, _M0L6_2atmpS2354);
  if (_M0L6_2atmpS2354.$1) {
    moonbit_decref(_M0L6_2atmpS2354.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS89,
  float _M0L3objS88
) {
  struct _M0TPB6Logger _M0L6_2atmpS2355;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS89);
  _M0L6_2atmpS2355
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS89
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS88, _M0L6_2atmpS2355);
  if (_M0L6_2atmpS2355.$1) {
    moonbit_decref(_M0L6_2atmpS2355.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS91,
  struct _M0TPC16string10StringView _M0L3objS90
) {
  struct _M0TPB6Logger _M0L6_2atmpS2356;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS91);
  _M0L6_2atmpS2356
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS91
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS90, _M0L6_2atmpS2356);
  if (_M0L6_2atmpS2356.$1) {
    moonbit_decref(_M0L6_2atmpS2356.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS93,
  uint64_t _M0L3objS92
) {
  struct _M0TPB6Logger _M0L6_2atmpS2357;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS93);
  _M0L6_2atmpS2357
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS93
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS92, _M0L6_2atmpS2357);
  if (_M0L6_2atmpS2357.$1) {
    moonbit_decref(_M0L6_2atmpS2357.$1);
  }
  return 0;
}

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGsE(
  moonbit_string_t* _M0L3srcS69,
  int32_t _M0L13allocate__lenS67,
  int32_t _M0L11src__offsetS70,
  int32_t _M0L11dst__offsetS68,
  int32_t _M0L9blit__lenS71
) {
  moonbit_string_t* _M0L3dstS66;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS66
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS67, (moonbit_string_t)moonbit_string_literal_75.data);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L3dstS66, _M0L11dst__offsetS68, _M0L3srcS69, _M0L11src__offsetS70, _M0L9blit__lenS71);
  moonbit_decref(_M0L3srcS69);
  return _M0L3dstS66;
}

struct _M0TUsfE** _M0MPB18UninitializedArray23unsafe__make__and__blitGUsfEE(
  struct _M0TUsfE** _M0L3srcS75,
  int32_t _M0L13allocate__lenS73,
  int32_t _M0L11src__offsetS76,
  int32_t _M0L11dst__offsetS74,
  int32_t _M0L9blit__lenS77
) {
  struct _M0TUsfE** _M0L3dstS72;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS72
  = (struct _M0TUsfE**)moonbit_make_ref_array(_M0L13allocate__lenS73, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsfEE(_M0L3dstS72, _M0L11dst__offsetS74, _M0L3srcS75, _M0L11src__offsetS76, _M0L9blit__lenS77);
  moonbit_decref(_M0L3srcS75);
  return _M0L3dstS72;
}

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGOsE(
  moonbit_string_t* _M0L3srcS81,
  int32_t _M0L13allocate__lenS79,
  int32_t _M0L11src__offsetS82,
  int32_t _M0L11dst__offsetS80,
  int32_t _M0L9blit__lenS83
) {
  moonbit_string_t* _M0L3dstS78;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS78
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS79, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGOsE(_M0L3dstS78, _M0L11dst__offsetS80, _M0L3srcS81, _M0L11src__offsetS82, _M0L9blit__lenS83);
  moonbit_decref(_M0L3srcS81);
  return _M0L3dstS78;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS51,
  int32_t _M0L11dst__offsetS52,
  moonbit_string_t* _M0L3srcS53,
  int32_t _M0L11src__offsetS54,
  int32_t _M0L3lenS55
) {
  #line 119 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_incref(_M0L3srcS53);
  moonbit_incref(_M0L3dstS51);
  #line 128 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_unsafe_ref_array_blit(_M0L3dstS51, _M0L11dst__offsetS52, _M0L3srcS53, _M0L11src__offsetS54, _M0L3lenS55);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsfEE(
  struct _M0TUsfE** _M0L3dstS56,
  int32_t _M0L11dst__offsetS57,
  struct _M0TUsfE** _M0L3srcS58,
  int32_t _M0L11src__offsetS59,
  int32_t _M0L3lenS60
) {
  #line 119 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_incref(_M0L3srcS58);
  moonbit_incref(_M0L3dstS56);
  #line 128 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_unsafe_ref_array_blit(_M0L3dstS56, _M0L11dst__offsetS57, _M0L3srcS58, _M0L11src__offsetS59, _M0L3lenS60);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGOsE(
  moonbit_string_t* _M0L3dstS61,
  int32_t _M0L11dst__offsetS62,
  moonbit_string_t* _M0L3srcS63,
  int32_t _M0L11src__offsetS64,
  int32_t _M0L3lenS65
) {
  #line 119 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_incref(_M0L3srcS63);
  moonbit_incref(_M0L3dstS61);
  #line 128 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_unsafe_ref_array_blit(_M0L3dstS61, _M0L11dst__offsetS62, _M0L3srcS63, _M0L11src__offsetS64, _M0L3lenS65);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t* _M0L3dstS15,
  int32_t _M0L11dst__offsetS17,
  uint16_t* _M0L3srcS16,
  int32_t _M0L11src__offsetS18,
  int32_t _M0L3lenS20
) {
  int32_t _if__result_5580;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS15 == _M0L3srcS16) {
    _if__result_5580 = _M0L11dst__offsetS17 < _M0L11src__offsetS18;
  } else {
    _if__result_5580 = 0;
  }
  if (_if__result_5580) {
    int32_t _M0L1iS19 = 0;
    while (1) {
      if (_M0L1iS19 < _M0L3lenS20) {
        int32_t _M0L6_2atmpS2317 = _M0L11dst__offsetS17 + _M0L1iS19;
        int32_t _M0L6_2atmpS2319 = _M0L11src__offsetS18 + _M0L1iS19;
        int32_t _M0L6_2atmpS2318;
        int32_t _M0L6_2atmpS2320;
        if (
          _M0L6_2atmpS2319 < 0
          || _M0L6_2atmpS2319 >= Moonbit_array_length(_M0L3srcS16)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2318 = (int32_t)_M0L3srcS16[_M0L6_2atmpS2319];
        if (
          _M0L6_2atmpS2317 < 0
          || _M0L6_2atmpS2317 >= Moonbit_array_length(_M0L3dstS15)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS15[_M0L6_2atmpS2317] = _M0L6_2atmpS2318;
        _M0L6_2atmpS2320 = _M0L1iS19 + 1;
        _M0L1iS19 = _M0L6_2atmpS2320;
        continue;
      } else {
        moonbit_decref(_M0L3srcS16);
        moonbit_decref(_M0L3dstS15);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2325 = _M0L3lenS20 - 1;
    int32_t _M0L1iS22 = _M0L6_2atmpS2325;
    while (1) {
      if (_M0L1iS22 >= 0) {
        int32_t _M0L6_2atmpS2321 = _M0L11dst__offsetS17 + _M0L1iS22;
        int32_t _M0L6_2atmpS2323 = _M0L11src__offsetS18 + _M0L1iS22;
        int32_t _M0L6_2atmpS2322;
        int32_t _M0L6_2atmpS2324;
        if (
          _M0L6_2atmpS2323 < 0
          || _M0L6_2atmpS2323 >= Moonbit_array_length(_M0L3srcS16)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2322 = (int32_t)_M0L3srcS16[_M0L6_2atmpS2323];
        if (
          _M0L6_2atmpS2321 < 0
          || _M0L6_2atmpS2321 >= Moonbit_array_length(_M0L3dstS15)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS15[_M0L6_2atmpS2321] = _M0L6_2atmpS2322;
        _M0L6_2atmpS2324 = _M0L1iS22 - 1;
        _M0L1iS22 = _M0L6_2atmpS2324;
        continue;
      } else {
        moonbit_decref(_M0L3srcS16);
        moonbit_decref(_M0L3dstS15);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS24,
  int32_t _M0L11dst__offsetS26,
  moonbit_string_t* _M0L3srcS25,
  int32_t _M0L11src__offsetS27,
  int32_t _M0L3lenS29
) {
  int32_t _if__result_5583;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS24 == _M0L3srcS25) {
    _if__result_5583 = _M0L11dst__offsetS26 < _M0L11src__offsetS27;
  } else {
    _if__result_5583 = 0;
  }
  if (_if__result_5583) {
    int32_t _M0L1iS28 = 0;
    while (1) {
      if (_M0L1iS28 < _M0L3lenS29) {
        int32_t _M0L6_2atmpS2326 = _M0L11dst__offsetS26 + _M0L1iS28;
        int32_t _M0L6_2atmpS2328 = _M0L11src__offsetS27 + _M0L1iS28;
        moonbit_string_t _M0L6_2atmpS2327;
        moonbit_string_t _M0L6_2aoldS4999;
        int32_t _M0L6_2atmpS2329;
        if (
          _M0L6_2atmpS2328 < 0
          || _M0L6_2atmpS2328 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2327 = (moonbit_string_t)_M0L3srcS25[_M0L6_2atmpS2328];
        if (
          _M0L6_2atmpS2326 < 0
          || _M0L6_2atmpS2326 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4999 = (moonbit_string_t)_M0L3dstS24[_M0L6_2atmpS2326];
        moonbit_incref(_M0L6_2atmpS2327);
        moonbit_decref(_M0L6_2aoldS4999);
        _M0L3dstS24[_M0L6_2atmpS2326] = _M0L6_2atmpS2327;
        _M0L6_2atmpS2329 = _M0L1iS28 + 1;
        _M0L1iS28 = _M0L6_2atmpS2329;
        continue;
      } else {
        moonbit_decref(_M0L3srcS25);
        moonbit_decref(_M0L3dstS24);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2334 = _M0L3lenS29 - 1;
    int32_t _M0L1iS31 = _M0L6_2atmpS2334;
    while (1) {
      if (_M0L1iS31 >= 0) {
        int32_t _M0L6_2atmpS2330 = _M0L11dst__offsetS26 + _M0L1iS31;
        int32_t _M0L6_2atmpS2332 = _M0L11src__offsetS27 + _M0L1iS31;
        moonbit_string_t _M0L6_2atmpS2331;
        moonbit_string_t _M0L6_2aoldS5001;
        int32_t _M0L6_2atmpS2333;
        if (
          _M0L6_2atmpS2332 < 0
          || _M0L6_2atmpS2332 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2331 = (moonbit_string_t)_M0L3srcS25[_M0L6_2atmpS2332];
        if (
          _M0L6_2atmpS2330 < 0
          || _M0L6_2atmpS2330 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5001 = (moonbit_string_t)_M0L3dstS24[_M0L6_2atmpS2330];
        moonbit_incref(_M0L6_2atmpS2331);
        moonbit_decref(_M0L6_2aoldS5001);
        _M0L3dstS24[_M0L6_2atmpS2330] = _M0L6_2atmpS2331;
        _M0L6_2atmpS2333 = _M0L1iS31 - 1;
        _M0L1iS31 = _M0L6_2atmpS2333;
        continue;
      } else {
        moonbit_decref(_M0L3srcS25);
        moonbit_decref(_M0L3dstS24);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsfEEE(
  struct _M0TUsfE** _M0L3dstS33,
  int32_t _M0L11dst__offsetS35,
  struct _M0TUsfE** _M0L3srcS34,
  int32_t _M0L11src__offsetS36,
  int32_t _M0L3lenS38
) {
  int32_t _if__result_5586;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS33 == _M0L3srcS34) {
    _if__result_5586 = _M0L11dst__offsetS35 < _M0L11src__offsetS36;
  } else {
    _if__result_5586 = 0;
  }
  if (_if__result_5586) {
    int32_t _M0L1iS37 = 0;
    while (1) {
      if (_M0L1iS37 < _M0L3lenS38) {
        int32_t _M0L6_2atmpS2335 = _M0L11dst__offsetS35 + _M0L1iS37;
        int32_t _M0L6_2atmpS2337 = _M0L11src__offsetS36 + _M0L1iS37;
        struct _M0TUsfE* _M0L6_2atmpS2336;
        struct _M0TUsfE* _M0L6_2aoldS5003;
        int32_t _M0L6_2atmpS2338;
        if (
          _M0L6_2atmpS2337 < 0
          || _M0L6_2atmpS2337 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2336 = (struct _M0TUsfE*)_M0L3srcS34[_M0L6_2atmpS2337];
        if (
          _M0L6_2atmpS2335 < 0
          || _M0L6_2atmpS2335 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5003 = (struct _M0TUsfE*)_M0L3dstS33[_M0L6_2atmpS2335];
        if (_M0L6_2atmpS2336) {
          moonbit_incref(_M0L6_2atmpS2336);
        }
        if (_M0L6_2aoldS5003) {
          moonbit_decref(_M0L6_2aoldS5003);
        }
        _M0L3dstS33[_M0L6_2atmpS2335] = _M0L6_2atmpS2336;
        _M0L6_2atmpS2338 = _M0L1iS37 + 1;
        _M0L1iS37 = _M0L6_2atmpS2338;
        continue;
      } else {
        moonbit_decref(_M0L3srcS34);
        moonbit_decref(_M0L3dstS33);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2343 = _M0L3lenS38 - 1;
    int32_t _M0L1iS40 = _M0L6_2atmpS2343;
    while (1) {
      if (_M0L1iS40 >= 0) {
        int32_t _M0L6_2atmpS2339 = _M0L11dst__offsetS35 + _M0L1iS40;
        int32_t _M0L6_2atmpS2341 = _M0L11src__offsetS36 + _M0L1iS40;
        struct _M0TUsfE* _M0L6_2atmpS2340;
        struct _M0TUsfE* _M0L6_2aoldS5005;
        int32_t _M0L6_2atmpS2342;
        if (
          _M0L6_2atmpS2341 < 0
          || _M0L6_2atmpS2341 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2340 = (struct _M0TUsfE*)_M0L3srcS34[_M0L6_2atmpS2341];
        if (
          _M0L6_2atmpS2339 < 0
          || _M0L6_2atmpS2339 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5005 = (struct _M0TUsfE*)_M0L3dstS33[_M0L6_2atmpS2339];
        if (_M0L6_2atmpS2340) {
          moonbit_incref(_M0L6_2atmpS2340);
        }
        if (_M0L6_2aoldS5005) {
          moonbit_decref(_M0L6_2aoldS5005);
        }
        _M0L3dstS33[_M0L6_2atmpS2339] = _M0L6_2atmpS2340;
        _M0L6_2atmpS2342 = _M0L1iS40 - 1;
        _M0L1iS40 = _M0L6_2atmpS2342;
        continue;
      } else {
        moonbit_decref(_M0L3srcS34);
        moonbit_decref(_M0L3dstS33);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGOsEE(
  moonbit_string_t* _M0L3dstS42,
  int32_t _M0L11dst__offsetS44,
  moonbit_string_t* _M0L3srcS43,
  int32_t _M0L11src__offsetS45,
  int32_t _M0L3lenS47
) {
  int32_t _if__result_5589;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS42 == _M0L3srcS43) {
    _if__result_5589 = _M0L11dst__offsetS44 < _M0L11src__offsetS45;
  } else {
    _if__result_5589 = 0;
  }
  if (_if__result_5589) {
    int32_t _M0L1iS46 = 0;
    while (1) {
      if (_M0L1iS46 < _M0L3lenS47) {
        int32_t _M0L6_2atmpS2344 = _M0L11dst__offsetS44 + _M0L1iS46;
        int32_t _M0L6_2atmpS2346 = _M0L11src__offsetS45 + _M0L1iS46;
        moonbit_string_t _M0L6_2atmpS2345;
        moonbit_string_t _M0L6_2aoldS5007;
        int32_t _M0L6_2atmpS2347;
        if (
          _M0L6_2atmpS2346 < 0
          || _M0L6_2atmpS2346 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2345 = (moonbit_string_t)_M0L3srcS43[_M0L6_2atmpS2346];
        if (
          _M0L6_2atmpS2344 < 0
          || _M0L6_2atmpS2344 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5007 = (moonbit_string_t)_M0L3dstS42[_M0L6_2atmpS2344];
        if (_M0L6_2atmpS2345) {
          moonbit_incref(_M0L6_2atmpS2345);
        }
        if (_M0L6_2aoldS5007) {
          moonbit_decref(_M0L6_2aoldS5007);
        }
        _M0L3dstS42[_M0L6_2atmpS2344] = _M0L6_2atmpS2345;
        _M0L6_2atmpS2347 = _M0L1iS46 + 1;
        _M0L1iS46 = _M0L6_2atmpS2347;
        continue;
      } else {
        moonbit_decref(_M0L3srcS43);
        moonbit_decref(_M0L3dstS42);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2352 = _M0L3lenS47 - 1;
    int32_t _M0L1iS49 = _M0L6_2atmpS2352;
    while (1) {
      if (_M0L1iS49 >= 0) {
        int32_t _M0L6_2atmpS2348 = _M0L11dst__offsetS44 + _M0L1iS49;
        int32_t _M0L6_2atmpS2350 = _M0L11src__offsetS45 + _M0L1iS49;
        moonbit_string_t _M0L6_2atmpS2349;
        moonbit_string_t _M0L6_2aoldS5009;
        int32_t _M0L6_2atmpS2351;
        if (
          _M0L6_2atmpS2350 < 0
          || _M0L6_2atmpS2350 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2349 = (moonbit_string_t)_M0L3srcS43[_M0L6_2atmpS2350];
        if (
          _M0L6_2atmpS2348 < 0
          || _M0L6_2atmpS2348 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5009 = (moonbit_string_t)_M0L3dstS42[_M0L6_2atmpS2348];
        if (_M0L6_2atmpS2349) {
          moonbit_incref(_M0L6_2atmpS2349);
        }
        if (_M0L6_2aoldS5009) {
          moonbit_decref(_M0L6_2aoldS5009);
        }
        _M0L3dstS42[_M0L6_2atmpS2348] = _M0L6_2atmpS2349;
        _M0L6_2atmpS2351 = _M0L1iS49 - 1;
        _M0L1iS49 = _M0L6_2atmpS2351;
        continue;
      } else {
        moonbit_decref(_M0L3srcS43);
        moonbit_decref(_M0L3dstS42);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPB18UninitializedArray6lengthGsE(moonbit_string_t* _M0L4selfS12) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS12);
}

int32_t _M0MPB18UninitializedArray6lengthGUsfEE(
  struct _M0TUsfE** _M0L4selfS13
) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS13);
}

int32_t _M0MPB18UninitializedArray6lengthGOsE(moonbit_string_t* _M0L4selfS14) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS14);
}

uint32_t _M0FPB13consume4__acc(uint32_t _M0L3accS10, uint32_t _M0L5inputS11) {
  uint32_t _M0L6_2atmpS2316;
  uint32_t _M0L6_2atmpS2315;
  uint32_t _M0L6_2atmpS2314;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2316 = _M0L5inputS11 * 3266489917u;
  _M0L6_2atmpS2315 = _M0L3accS10 + _M0L6_2atmpS2316;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2314 = _M0FPB4rotl(_M0L6_2atmpS2315, 17);
  return _M0L6_2atmpS2314 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS8, int32_t _M0L1rS9) {
  uint32_t _M0L6_2atmpS2311;
  int32_t _M0L6_2atmpS2313;
  uint32_t _M0L6_2atmpS2312;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2311 = _M0L1xS8 << (_M0L1rS9 & 31);
  _M0L6_2atmpS2313 = 32 - _M0L1rS9;
  _M0L6_2atmpS2312 = _M0L1xS8 >> (_M0L6_2atmpS2313 & 31);
  return _M0L6_2atmpS2311 | _M0L6_2atmpS2312;
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

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS3);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(
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

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(
  moonbit_string_t _M0L3msgS6
) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS6);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS7) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS7);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

int32_t _M0IP016_24default__implPB6Logger61write_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2303,
  struct _M0TPB4Show _M0L8_2aparamS2302
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2301 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2303;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS2301, _M0L8_2aparamS2302);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2300,
  struct _M0TPB4Show _M0L8_2aparamS2299
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2298 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2300;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS2298, _M0L8_2aparamS2299);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2297,
  int32_t _M0L8_2aparamS2296
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2295 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2297;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS2295, _M0L8_2aparamS2296);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2294,
  struct _M0TPC16string10StringView _M0L8_2aparamS2293
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2292 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2294;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS2292, _M0L8_2aparamS2293);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2291,
  moonbit_string_t _M0L8_2aparamS2288,
  int32_t _M0L8_2aparamS2289,
  int32_t _M0L8_2aparamS2290
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2287 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2291;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS2287, _M0L8_2aparamS2288, _M0L8_2aparamS2289, _M0L8_2aparamS2290);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2286,
  moonbit_string_t _M0L8_2aparamS2285
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2284 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2286;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS2284, _M0L8_2aparamS2285);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L2dbS2268;
  moonbit_string_t* _M0L6_2atmpS2310;
  struct _M0TPB5ArrayGsE* _M0L14demo__commandsS2269;
  int32_t _M0L7_2abindS2270;
  int32_t _M0L2__S2271;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 432 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_114.data);
  #line 433 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_115.data);
  #line 434 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_114.data);
  #line 435 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_75.data);
  #line 437 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L2dbS2268 = _M0MP38JIA2JIA29moonbitdb3lib8Database3new();
  _M0L6_2atmpS2310 = (moonbit_string_t*)moonbit_make_ref_array_raw(46);
  _M0L6_2atmpS2310[0] = (moonbit_string_t)moonbit_string_literal_116.data;
  _M0L6_2atmpS2310[1] = (moonbit_string_t)moonbit_string_literal_117.data;
  _M0L6_2atmpS2310[2] = (moonbit_string_t)moonbit_string_literal_118.data;
  _M0L6_2atmpS2310[3] = (moonbit_string_t)moonbit_string_literal_119.data;
  _M0L6_2atmpS2310[4] = (moonbit_string_t)moonbit_string_literal_120.data;
  _M0L6_2atmpS2310[5] = (moonbit_string_t)moonbit_string_literal_119.data;
  _M0L6_2atmpS2310[6] = (moonbit_string_t)moonbit_string_literal_121.data;
  _M0L6_2atmpS2310[7] = (moonbit_string_t)moonbit_string_literal_118.data;
  _M0L6_2atmpS2310[8] = (moonbit_string_t)moonbit_string_literal_122.data;
  _M0L6_2atmpS2310[9] = (moonbit_string_t)moonbit_string_literal_123.data;
  _M0L6_2atmpS2310[10] = (moonbit_string_t)moonbit_string_literal_124.data;
  _M0L6_2atmpS2310[11] = (moonbit_string_t)moonbit_string_literal_125.data;
  _M0L6_2atmpS2310[12] = (moonbit_string_t)moonbit_string_literal_126.data;
  _M0L6_2atmpS2310[13] = (moonbit_string_t)moonbit_string_literal_127.data;
  _M0L6_2atmpS2310[14] = (moonbit_string_t)moonbit_string_literal_128.data;
  _M0L6_2atmpS2310[15] = (moonbit_string_t)moonbit_string_literal_129.data;
  _M0L6_2atmpS2310[16] = (moonbit_string_t)moonbit_string_literal_130.data;
  _M0L6_2atmpS2310[17] = (moonbit_string_t)moonbit_string_literal_131.data;
  _M0L6_2atmpS2310[18] = (moonbit_string_t)moonbit_string_literal_132.data;
  _M0L6_2atmpS2310[19] = (moonbit_string_t)moonbit_string_literal_133.data;
  _M0L6_2atmpS2310[20] = (moonbit_string_t)moonbit_string_literal_134.data;
  _M0L6_2atmpS2310[21] = (moonbit_string_t)moonbit_string_literal_135.data;
  _M0L6_2atmpS2310[22] = (moonbit_string_t)moonbit_string_literal_136.data;
  _M0L6_2atmpS2310[23] = (moonbit_string_t)moonbit_string_literal_137.data;
  _M0L6_2atmpS2310[24] = (moonbit_string_t)moonbit_string_literal_138.data;
  _M0L6_2atmpS2310[25] = (moonbit_string_t)moonbit_string_literal_139.data;
  _M0L6_2atmpS2310[26] = (moonbit_string_t)moonbit_string_literal_140.data;
  _M0L6_2atmpS2310[27] = (moonbit_string_t)moonbit_string_literal_141.data;
  _M0L6_2atmpS2310[28] = (moonbit_string_t)moonbit_string_literal_142.data;
  _M0L6_2atmpS2310[29] = (moonbit_string_t)moonbit_string_literal_143.data;
  _M0L6_2atmpS2310[30] = (moonbit_string_t)moonbit_string_literal_144.data;
  _M0L6_2atmpS2310[31] = (moonbit_string_t)moonbit_string_literal_145.data;
  _M0L6_2atmpS2310[32] = (moonbit_string_t)moonbit_string_literal_143.data;
  _M0L6_2atmpS2310[33] = (moonbit_string_t)moonbit_string_literal_146.data;
  _M0L6_2atmpS2310[34] = (moonbit_string_t)moonbit_string_literal_147.data;
  _M0L6_2atmpS2310[35] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS2310[36] = (moonbit_string_t)moonbit_string_literal_148.data;
  _M0L6_2atmpS2310[37] = (moonbit_string_t)moonbit_string_literal_149.data;
  _M0L6_2atmpS2310[38] = (moonbit_string_t)moonbit_string_literal_150.data;
  _M0L6_2atmpS2310[39] = (moonbit_string_t)moonbit_string_literal_151.data;
  _M0L6_2atmpS2310[40] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS2310[41] = (moonbit_string_t)moonbit_string_literal_152.data;
  _M0L6_2atmpS2310[42] = (moonbit_string_t)moonbit_string_literal_153.data;
  _M0L6_2atmpS2310[43] = (moonbit_string_t)moonbit_string_literal_124.data;
  _M0L6_2atmpS2310[44] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS2310[45] = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L14demo__commandsS2269
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L14demo__commandsS2269)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L14demo__commandsS2269->$0 = _M0L6_2atmpS2310;
  _M0L14demo__commandsS2269->$1 = 46;
  #line 488 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_154.data);
  #line 489 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_155.data);
  _M0L7_2abindS2270 = _M0L14demo__commandsS2269->$1;
  _M0L2__S2271 = 0;
  while (1) {
    if (_M0L2__S2271 < _M0L7_2abindS2270) {
      moonbit_string_t* _M0L3bufS2309 = _M0L14demo__commandsS2269->$0;
      moonbit_string_t _M0L3cmdS2272 =
        (moonbit_string_t)_M0L3bufS2309[_M0L2__S2271];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2273;
      moonbit_string_t _M0L6_2atmpS2304;
      struct _M0TPB5ArrayGsE* _M0L6resultS2274;
      int32_t _M0L7_2abindS2275;
      int32_t _M0L2__S2276;
      int32_t _M0L6_2atmpS2308;
      moonbit_incref(_M0L3cmdS2272);
      #line 491 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_75.data);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2273
      = _M0MPB13StringBuilder21StringBuilder_2einner(4);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2273, (moonbit_string_t)moonbit_string_literal_156.data);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2273, _M0L3cmdS2272);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS2304
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2273);
      moonbit_decref(_M0L18_2astring__builderS2273);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2304);
      moonbit_decref(_M0L6_2atmpS2304);
      #line 493 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6resultS2274
      = _M0FP48JIA2JIA29moonbitdb8examples9cli__repl16execute__command(_M0L2dbS2268, _M0L3cmdS2272);
      moonbit_decref(_M0L3cmdS2272);
      _M0L7_2abindS2275 = _M0L6resultS2274->$1;
      _M0L2__S2276 = 0;
      while (1) {
        if (_M0L2__S2276 < _M0L7_2abindS2275) {
          moonbit_string_t* _M0L3bufS2307 = _M0L6resultS2274->$0;
          moonbit_string_t _M0L4lineS2277 =
            (moonbit_string_t)_M0L3bufS2307[_M0L2__S2276];
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2278;
          moonbit_string_t _M0L6_2atmpS2305;
          int32_t _M0L6_2atmpS2306;
          moonbit_incref(_M0L4lineS2277);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2278
          = _M0MPB13StringBuilder21StringBuilder_2einner(2);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2278, (moonbit_string_t)moonbit_string_literal_23.data);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2278, _M0L4lineS2277);
          moonbit_decref(_M0L4lineS2277);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS2305
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2278);
          moonbit_decref(_M0L18_2astring__builderS2278);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0FPB7printlnGsE(_M0L6_2atmpS2305);
          moonbit_decref(_M0L6_2atmpS2305);
          _M0L6_2atmpS2306 = _M0L2__S2276 + 1;
          _M0L2__S2276 = _M0L6_2atmpS2306;
          continue;
        } else {
          moonbit_decref(_M0L6resultS2274);
        }
        break;
      }
      _M0L6_2atmpS2308 = _M0L2__S2271 + 1;
      _M0L2__S2271 = _M0L6_2atmpS2308;
      continue;
    } else {
      moonbit_decref(_M0L14demo__commandsS2269);
      moonbit_decref(_M0L2dbS2268);
    }
    break;
  }
  #line 499 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_75.data);
  #line 500 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_114.data);
  #line 501 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_157.data);
  #line 502 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_114.data);
  return 0;
}