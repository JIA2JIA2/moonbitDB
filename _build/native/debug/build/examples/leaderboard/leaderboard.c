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
struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0TPB9ArrayViewGUsfEE;

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

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

struct _M0DTPC16option6OptionGfE4Some;

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque;

struct _M0TPB19MulShiftAll64Result;

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set;

struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0TWEOUsbE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet;

struct _M0TPB5EntryGsfE;

struct _M0TPB8MutLocalGiE;

struct _M0TPB5ArrayGUsfEE;

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0TPB3MapGssE;

struct _M0TPB4Show;

struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash;

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player;

struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0BTPB4Show;

struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB8MutLocalGORPB5EntryGsbEE;

struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE;

struct _M0TPB3MapGsbE;

struct _M0TPB5ArrayGsE;

struct _M0TPB3MapGsiE;

struct _M0TPB9ArrayViewGUssEE;

struct _M0TPB9ArrayViewGUsbEE;

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__;

struct _M0TPB9ArrayViewGsE;

struct _M0TPB4IterGUsbEE;

struct _M0TPB5EntryGsiE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

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

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
};

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set {
  struct _M0TPB3MapGsbE* $0;
  
};

struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** $0;
  int32_t $1;
  int32_t $2;
  
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

struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* $0;
  
};

struct _M0BTPB4Show {
  int32_t(* $method_0)(void*, struct _M0TPB6Logger);
  moonbit_string_t(* $method_1)(void*);
  
};

struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__ {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*(* code)(
    struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* $1;
  
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

struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE {
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player** $0;
  int32_t $1;
  
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

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
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

int32_t _M0FP48JIA2JIA29moonbitdb8examples11leaderboard19find__player__index(
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE*,
  moonbit_string_t
);

struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new(
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0FP48JIA2JIA29moonbitdb8examples11leaderboard16show__opt__float(
  void*
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database7command();

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

struct _M0TUsfE* _M0MP38JIA2JIA29moonbitdb3lib8Database7zpopmax(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

float _M0MP38JIA2JIA29moonbitdb3lib8Database7zincrby(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  float,
  moonbit_string_t
);

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database8zrevrank(
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6zcount(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  float,
  float
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database13zrangebyscore(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  float,
  float
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

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database5sdiff(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6sunion(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6sinter(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB3MapGsbE* _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0MP38JIA2JIA29moonbitdb3lib8Database3new(
  
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(
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

struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0MPC15array5Array2atGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE*,
  int32_t
);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

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

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(struct _M0TPB4IterGUsbEE*);

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*
);

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(struct _M0TPB4IterGUsfEE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(struct _M0TPB3MapGsfE*);

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2815l711(struct _M0TWEOUsfE*);

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2805l711(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2795l711(struct _M0TWEOUsbE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

int32_t _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map18remove__with__hashGsfE(
  struct _M0TPB3MapGsfE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map11shift__backGsiE(struct _M0TPB3MapGsiE*, int32_t);

int32_t _M0MPB3Map11shift__backGsfE(struct _M0TPB3MapGsfE*, int32_t);

int32_t _M0MPB3Map11shift__backGsbE(struct _M0TPB3MapGsbE*, int32_t);

int32_t _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsfE(
  struct _M0TPB3MapGsfE*,
  struct _M0TPB5EntryGsfE*
);

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map8containsGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map8containsGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

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

struct _M0TPB3MapGsfE* _M0MPB3Map3MapGsfE(
  struct _M0TPB9ArrayViewGUsfEE,
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

int32_t _M0MPB3Map3setGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t, float);

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

int32_t _M0MPB3Map3setGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t, int32_t);

int32_t _M0MPB3Map3setGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t, int32_t);

int32_t _M0MPB3Map15set__with__hashGsfE(
  struct _M0TPB3MapGsfE*,
  moonbit_string_t,
  float,
  int32_t
);

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

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsbE(
  struct _M0TPB3MapGsbE*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE*);

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE*,
  struct _M0TPB5EntryGsfE*
);

int32_t _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*
);

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map10push__awayGsfE(
  struct _M0TPB3MapGsfE*,
  int32_t,
  struct _M0TPB5EntryGsfE*
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

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE*,
  int32_t,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map10push__awayGsbE(
  struct _M0TPB3MapGsbE*,
  int32_t,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map10set__entryGsfE(
  struct _M0TPB3MapGsfE*,
  struct _M0TPB5EntryGsfE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*,
  int32_t
);

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE*,
  int32_t,
  struct _M0TPB5EntryGsfE*
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

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE*,
  int32_t,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE*,
  int32_t,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPC13int3Int3max(int32_t, int32_t);

int32_t _M0FPB21capacity__for__length(int32_t);

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  int32_t
);

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t);

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t);

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t);

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

int32_t _M0MPC16option6Option6unwrapGiE(int64_t);

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE*
);

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE*
);

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE*
);

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE*
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

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE*,
  struct _M0TUsfE*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsfEE(struct _M0TPB5ArrayGUsfEE*);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE*
);

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array6lengthGUsfEE(struct _M0TPB5ArrayGUsfEE*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

int32_t _M0IPC14byte4BytePB7Default7default();

struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player** _M0MPC15array5Array6bufferGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE*
);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE*
);

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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsfEEE(
  struct _M0TUsfE**,
  int32_t,
  struct _M0TUsfE**,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray6lengthGsE(moonbit_string_t*);

int32_t _M0MPB18UninitializedArray6lengthGUsfEE(struct _M0TUsfE**);

uint32_t _M0FPB13consume4__acc(uint32_t, uint32_t);

uint32_t _M0FPB4rotl(uint32_t, int32_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

uint16_t* _M0FPC15abort5abortGAkE(moonbit_string_t);

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(
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

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_82 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 0, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_63 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_9 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_135 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 22686,
    21152, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_10 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 68, 69, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_60 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 90, 82, 
    65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_71 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 68, 66, 
    83, 73, 90, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_124 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 20998, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_87 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 110, 
    102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_85 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 78, 97, 78, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_43 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 83, 77, 
    69, 77, 66, 69, 82, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_21 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_120 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 115, 99, 
    111, 114, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_109 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_162 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 102, 114, 
    105, 101, 110, 100, 115, 58, 112, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_121 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 32, 32, 
    29609, 23478, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_83 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_52 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 68, 
    73, 70, 70, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_17 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_177 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    38431, 21015, 21097, 20313, 38271, 24230, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_39 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_35 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 73, 
    78, 68, 69, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_55 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_170 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 26032, 29256,
    26412, 21457, 24067, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_62 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_165 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    20849, 21516, 22909, 21451, 73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_142 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 8595, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_96 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_14 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 84, 
    84, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_93 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_141 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 8593, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_2 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 71, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_69 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 73, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_84 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 73, 108, 
    108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 116, 69, 
    120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_175 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    36880, 26465, 28040, 36153, 28040, 24687, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_147 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 32, 32, 
    50, 48, 48, 45, 52, 48, 48, 32, 20998, 21306, 38388, 29609, 23478, 
    25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_115 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 10, 55357,
    56522, 32, 38454, 27573, 49, 65306, 21021, 22987, 21270, 29609, 23478,
    20998, 25968, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_133 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55357,
    56580, 32, 38454, 27573, 51, 65306, 27169, 25311, 28216, 25103, 23545,
    23616, 26356, 26032, 20998, 25968, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_53 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 77, 
    79, 86, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_154 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 31186, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_136 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 20998,
    65292, 26032, 24635, 20998, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_46 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_179 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    24635, 32, 75, 101, 121, 32, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_157 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 32, 21517, 
    58, 32, 26080, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_86 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_26 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 86, 
    65, 76, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_8 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 84, 
    82, 76, 69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_128 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 85, 110, 
    107, 110, 111, 119, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_103 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_36 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_6 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 89, 
    80, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_74 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 65, 
    78, 68, 79, 77, 75, 69, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_12 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    88, 80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_7 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 65, 80, 
    80, 69, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_125 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55356,
    57286, 32, 38454, 27573, 50, 65306, 26597, 35810, 25490, 34892, 27036,
    32, 84, 79, 80, 32, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_114 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 101, 
    110, 114, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_45 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_44 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_31 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_138 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 32, 32, 
    25490, 21517, 32, 124, 32, 29609, 23478, 73, 68, 32, 124, 32, 29609,
    23478, 21517, 31216, 32, 124, 32, 20998, 25968, 32, 124, 32, 25490,
    21517, 21464, 21270, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_105 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_97 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_65 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 90, 82, 
    69, 86, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_168 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 10, 55357,
    56523, 32, 38454, 27573, 57, 65306, 20351, 29992, 76, 105, 115, 116,
    31649, 29702, 28040, 24687, 38431, 21015, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_163 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 102, 114, 
    105, 101, 110, 100, 115, 58, 112, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_151 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 108, 101, 
    97, 100, 101, 114, 98, 111, 97, 114, 100, 58, 100, 97, 105, 108, 
    121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_75 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 69, 
    78, 65, 77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_58 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 83, 
    67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_156 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    31532, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_169 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 31995, 32479,
    32500, 25252, 36890, 30693, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_129 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    32, 35, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_122 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 40, 
    73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_110 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 70, 114, 
    97, 110, 107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_101 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_54 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_139 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 45, 32, 32, 
    45, 45, 45, 45, 45, 124, 45, 45, 45, 45, 45, 45, 45, 45, 124, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 124, 45, 45, 45, 45, 45, 45, 
    124, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_130 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    124, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_24 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 69, 
    88, 73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_174 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    28040, 24687, 38431, 21015, 38271, 24230, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_160 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 41, 32, 
    45, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_102 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 66, 111, 98, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_79 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    70, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_5 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 75, 69, 
    89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_4 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_144 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 32, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_64 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 82, 
    65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_166 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    20004, 20154, 22909, 21451, 24635, 25968, 65288, 21435, 37325, 65289, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_81 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 67, 79, 
    77, 77, 65, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_171 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 27963, 21160,
    22870, 21169, 21457, 25918, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_57 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_112 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 71, 114, 
    97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_149 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 44, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_13 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 84, 84, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_180 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    25903, 25345, 21629, 20196, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_127 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 34, 32, 32, 
    45, 45, 45, 45, 45, 124, 45, 45, 45, 45, 45, 45, 45, 45, 124, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 124, 45, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_113 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_182 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    9989, 32, 25490, 34892, 27036, 31995, 32479, 31034, 20363, 36816, 
    34892, 23436, 25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_155 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 10, 55357,
    56401, 32, 38454, 27573, 55, 65306, 24377, 20986, 27599, 26085, 21069,
    19977, 21517, 39046, 22870, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_33 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_89 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 114, 97, 
    100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 98, 101, 
    116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_172 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 36187, 23395,
    26356, 26032, 39044, 21578, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_0 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 78, 47, 65, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_29 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 76, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_23 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_132 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 32, 
    32, 32, 124, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_95 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 44, 32, 
    108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_92 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 98, 111, 
    117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 97, 105, 
    108, 101, 100, 58, 32, 97, 108, 108, 111, 99, 97, 116, 101, 95, 108, 
    101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_181 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 41, 10, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_59 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_42 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_70 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 65, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_28 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    71, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_56 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_80 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 73, 
    77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_90 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 
    118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_51 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 68, 
    73, 70, 70, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_41 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 80, 
    79, 80, 76, 80, 85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_159 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 40, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_143 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 8212, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_123 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 41, 58, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_134 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_117 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 108, 101, 
    97, 100, 101, 114, 98, 111, 97, 114, 100, 58, 103, 108, 111, 98, 
    97, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_100 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 65, 108, 
    105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_66 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 73, 
    78, 67, 82, 66, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_108 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 69, 118, 101, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_30 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 82, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_176 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    32, 32, 45, 62, 32, 22788, 29702, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_68 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_178 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 55357,
    56522, 32, 38454, 27573, 49, 48, 65306, 26381, 21153, 22120, 29366,
    24577, 32479, 35745, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_173 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 110, 111, 
    116, 105, 102, 105, 99, 97, 116, 105, 111, 110, 115, 58, 103, 108, 
    111, 98, 97, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_73 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 70, 76, 
    85, 83, 72, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_140 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 35, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_48 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 73, 
    78, 84, 69, 82, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_47 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 73, 
    78, 84, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_50 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 85, 
    78, 73, 79, 78, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_27 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    83, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_148 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 32, 32, 
    49, 48, 48, 45, 51, 48, 48, 32, 20998, 21306, 38388, 29609, 23478, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_94 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    100, 115, 116, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_91 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 105, 110, 
    118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 111, 105, 
    110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_49 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 85, 
    78, 73, 79, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_37 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_22 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 71, 
    69, 84, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_126 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 23, 32, 32, 
    25490, 21517, 32, 124, 32, 29609, 23478, 73, 68, 32, 124, 32, 29609,
    23478, 21517, 31216, 32, 124, 32, 20998, 25968, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_111 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 55, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_77 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 73, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_1 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 83, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_164 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 32, 32, 
    65, 108, 105, 99, 101, 32, 21644, 32, 66, 111, 98, 32, 30340, 20849,
    21516, 22909, 21451, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_19 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_131 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 32, 32, 
    32, 32, 32, 124, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_25 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 75, 
    69, 89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_106 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 68, 97, 
    118, 105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_32 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 82, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_116 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_38 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 76, 73, 
    78, 83, 69, 82, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_11 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_119 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 97, 
    109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_145 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 124, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_146 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 10, 55357,
    56520, 32, 38454, 27573, 53, 65306, 20998, 25968, 21306, 38388, 32479,
    35745, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_16 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_158 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 21517, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_152 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 32, 32, 
    27599, 26085, 25490, 34892, 27036, 24050, 21019, 24314, 65292, 50, 
    52, 23567, 26102, 21518, 33258, 21160, 36807, 26399, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_107 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_167 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    20165, 32, 65, 108, 105, 99, 101, 32, 26377, 30340, 22909, 21451, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_137 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 10, 55356,
    57286, 32, 38454, 27573, 52, 65306, 26356, 26032, 21518, 30340, 23436,
    25972, 25490, 34892, 27036, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_118 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 112, 108, 
    97, 121, 101, 114, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_40 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_153 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 32, 32, 
    84, 84, 76, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_61 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 67, 
    79, 85, 78, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_104 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 67, 104, 
    97, 114, 108, 105, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_34 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_76 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 82, 69, 
    78, 65, 77, 69, 78, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_3 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 68, 69, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_161 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55357,
    56550, 32, 38454, 27573, 56, 65306, 20351, 29992, 83, 101, 116, 31649,
    29702, 22909, 21451, 20851, 31995, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_99 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_98 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    28216, 25103, 25490, 34892, 27036, 31995, 32479, 32, 45, 32, 37096,
    32626, 31034, 20363, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 69, 67, 
    72, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_150 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55356,
    57262, 32, 38454, 27573, 54, 65306, 27599, 26085, 25490, 34892, 27036,
    65288, 24102, 36807, 26399, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_88 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 48, 46, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_72 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 70, 76, 
    85, 83, 72, 68, 66, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_15 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    82, 83, 73, 83, 84, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0)
  };

uint32_t const moonbit_layout_table_data[118] =
  {
    sizeof(struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player) / 4,
    2,
    offsetof(struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player, $0)
    / 4,
    offsetof(struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player, $1)
    / 4, sizeof(struct _M0TPB5ArrayGsE) / 4, 1,
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
    sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database) / 4, 2,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database, $0) / 4,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database, $1) / 4,
    sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque) / 4, 2,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque, $0) / 4,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque, $1) / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__)
    / 4, 2,
    offsetof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__, $0)
    / 4,
    offsetof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__, $1)
    / 4, sizeof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE) / 4, 
    2,
    offsetof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $0) / 4,
    offsetof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $1) / 4,
    sizeof(struct _M0TUsbE) / 4, 1, offsetof(struct _M0TUsbE, $0) / 4,
    sizeof(struct _M0TPB5EntryGsfE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsfE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsfE, $4) / 4,
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
    sizeof(struct _M0TPB5EntryGsiE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsiE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsiE, $4) / 4,
    sizeof(struct _M0TPB5EntryGsbE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsbE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsbE, $4) / 4,
    sizeof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE) / 4,
    2,
    offsetof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $0)
    / 4,
    offsetof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $5)
    / 4, sizeof(struct _M0TPB3MapGsiE) / 4, 2,
    offsetof(struct _M0TPB3MapGsiE, $0) / 4,
    offsetof(struct _M0TPB3MapGsiE, $5) / 4,
    sizeof(struct _M0TPB3MapGsfE) / 4, 2,
    offsetof(struct _M0TPB3MapGsfE, $0) / 4,
    offsetof(struct _M0TPB3MapGsfE, $5) / 4,
    sizeof(struct _M0TPB3MapGssE) / 4, 2,
    offsetof(struct _M0TPB3MapGssE, $0) / 4,
    offsetof(struct _M0TPB3MapGssE, $5) / 4,
    sizeof(struct _M0TPB3MapGsbE) / 4, 2,
    offsetof(struct _M0TPB3MapGsbE, $0) / 4,
    offsetof(struct _M0TPB3MapGsbE, $5) / 4,
    sizeof(struct _M0TPB4IterGUsbEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsbEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE) / 4,
    1,
    offsetof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE, $0)
    / 4, sizeof(struct _M0TPB4IterGUsfEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsfEE, $0) / 4,
    sizeof(struct _M0TPB13StringBuilder) / 4, 1,
    offsetof(struct _M0TPB13StringBuilder, $0) / 4,
    sizeof(struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE)
    / 4, 1,
    offsetof(struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE, $0)
    / 4
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

int64_t _M0MPB4Iter4nextN6constrS9980GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsfEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsbEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsfEE = 0ll;

int32_t _M0FP48JIA2JIA29moonbitdb8examples11leaderboard19find__player__index(
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE* _M0L7playersS1626,
  moonbit_string_t _M0L2idS1627
) {
  int32_t _M0L1iS1625;
  #line 172 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L1iS1625 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3383;
    #line 173 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS3383
    = _M0MPC15array5Array6lengthGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(_M0L7playersS1626);
    if (_M0L1iS1625 < _M0L6_2atmpS3383) {
      struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS3385;
      moonbit_string_t _M0L8_2afieldS3387;
      int32_t _M0L6_2acntS3744;
      moonbit_string_t _M0L2idS3384;
      int32_t _result_3806;
      int32_t _M0L6_2atmpS3386;
      #line 174 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS3385
      = _M0MPC15array5Array2atGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(_M0L7playersS1626, _M0L1iS1625);
      _M0L8_2afieldS3387 = _M0L6_2atmpS3385->$0;
      _M0L6_2acntS3744
      = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3385));
      if (_M0L6_2acntS3744 > 1) {
        int32_t _M0L11_2anew__cntS3746 = _M0L6_2acntS3744 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3385), _M0L11_2anew__cntS3746);
        moonbit_incref(_M0L8_2afieldS3387);
      } else if (_M0L6_2acntS3744 == 1) {
        moonbit_string_t _M0L8_2afieldS3745 = _M0L6_2atmpS3385->$1;
        moonbit_decref(_M0L8_2afieldS3745);
        #line 174 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        moonbit_free(_M0L6_2atmpS3385);
      }
      _M0L2idS3384 = _M0L8_2afieldS3387;
      #line 174 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _result_3806
      = _M0L2idS3384 == _M0L2idS1627
        || Moonbit_array_length(_M0L2idS3384)
           == Moonbit_array_length(_M0L2idS1627)
           && 0
              == memcmp(_M0L2idS3384, _M0L2idS1627, Moonbit_array_length(_M0L2idS3384) * 2);
      moonbit_decref(_M0L2idS3384);
      if (_result_3806) {
        return _M0L1iS1625;
      }
      _M0L6_2atmpS3386 = _M0L1iS1625 + 1;
      _M0L1iS1625 = _M0L6_2atmpS3386;
      continue;
    }
    break;
  }
  return -1;
}

struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new(
  moonbit_string_t _M0L2idS1623,
  moonbit_string_t _M0L4nameS1624
) {
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _block_3807;
  #line 13 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  moonbit_incref(_M0L2idS1623);
  moonbit_incref(_M0L4nameS1624);
  _block_3807
  = (struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player*)moonbit_malloc(sizeof(struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player));
  Moonbit_object_header(_block_3807)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _block_3807->$0 = _M0L2idS1623;
  _block_3807->$1 = _M0L4nameS1624;
  return _block_3807;
}

moonbit_string_t _M0FP48JIA2JIA29moonbitdb8examples11leaderboard16show__opt__float(
  void* _M0L3optS1620
) {
  float _M0L1vS1618;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1619;
  moonbit_string_t _result_3809;
  #line 1 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  switch (Moonbit_object_tag(_M0L3optS1620)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1621 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1620;
      float _M0L4_2avS1622 = _M0L7_2aSomeS1621->$0;
      _M0L1vS1618 = _M0L4_2avS1622;
      goto join_1617;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_0.data;
      break;
    }
  }
  join_1617:;
  #line 3 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1619
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1619, _M0L1vS1618);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _result_3809
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1619);
  moonbit_decref(_M0L18_2astring__builderS1619);
  return _result_3809;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database7command() {
  moonbit_string_t* _M0L6_2atmpS3382;
  struct _M0TPB5ArrayGsE* _block_3810;
  #line 1594 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3382 = (moonbit_string_t*)moonbit_make_ref_array_raw(81);
  _M0L6_2atmpS3382[0] = (moonbit_string_t)moonbit_string_literal_1.data;
  _M0L6_2atmpS3382[1] = (moonbit_string_t)moonbit_string_literal_2.data;
  _M0L6_2atmpS3382[2] = (moonbit_string_t)moonbit_string_literal_3.data;
  _M0L6_2atmpS3382[3] = (moonbit_string_t)moonbit_string_literal_4.data;
  _M0L6_2atmpS3382[4] = (moonbit_string_t)moonbit_string_literal_5.data;
  _M0L6_2atmpS3382[5] = (moonbit_string_t)moonbit_string_literal_6.data;
  _M0L6_2atmpS3382[6] = (moonbit_string_t)moonbit_string_literal_7.data;
  _M0L6_2atmpS3382[7] = (moonbit_string_t)moonbit_string_literal_8.data;
  _M0L6_2atmpS3382[8] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3382[9] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3382[10] = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3382[11] = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3382[12] = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3382[13] = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3382[14] = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS3382[15] = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3382[16] = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS3382[17] = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3382[18] = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3382[19] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS3382[20] = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3382[21] = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3382[22] = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS3382[23] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS3382[24] = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3382[25] = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS3382[26] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS3382[27] = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS3382[28] = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3382[29] = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3382[30] = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3382[31] = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS3382[32] = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS3382[33] = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3382[34] = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS3382[35] = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS3382[36] = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS3382[37] = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3382[38] = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS3382[39] = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS3382[40] = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS3382[41] = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3382[42] = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3382[43] = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS3382[44] = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS3382[45] = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS3382[46] = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS3382[47] = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS3382[48] = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS3382[49] = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS3382[50] = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3382[51] = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS3382[52] = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L6_2atmpS3382[53] = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS3382[54] = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS3382[55] = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS3382[56] = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS3382[57] = (moonbit_string_t)moonbit_string_literal_58.data;
  _M0L6_2atmpS3382[58] = (moonbit_string_t)moonbit_string_literal_59.data;
  _M0L6_2atmpS3382[59] = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS3382[60] = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L6_2atmpS3382[61] = (moonbit_string_t)moonbit_string_literal_62.data;
  _M0L6_2atmpS3382[62] = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS3382[63] = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS3382[64] = (moonbit_string_t)moonbit_string_literal_65.data;
  _M0L6_2atmpS3382[65] = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L6_2atmpS3382[66] = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS3382[67] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS3382[68] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS3382[69] = (moonbit_string_t)moonbit_string_literal_70.data;
  _M0L6_2atmpS3382[70] = (moonbit_string_t)moonbit_string_literal_71.data;
  _M0L6_2atmpS3382[71] = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS3382[72] = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS3382[73] = (moonbit_string_t)moonbit_string_literal_74.data;
  _M0L6_2atmpS3382[74] = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS3382[75] = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS3382[76] = (moonbit_string_t)moonbit_string_literal_77.data;
  _M0L6_2atmpS3382[77] = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L6_2atmpS3382[78] = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L6_2atmpS3382[79] = (moonbit_string_t)moonbit_string_literal_80.data;
  _M0L6_2atmpS3382[80] = (moonbit_string_t)moonbit_string_literal_81.data;
  _block_3810
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_3810)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _block_3810->$0 = _M0L6_2atmpS3382;
  _block_3810->$1 = 81;
  return _block_3810;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1609
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1607;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3381;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L5_2aitS1608;
  int32_t _result_3813;
  #line 1484 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5countS1607
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1607)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1607->$0 = 0;
  _M0L4dataS3381 = _M0L4selfS1609->$0;
  moonbit_incref(_M0L4dataS3381);
  #line 1485 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1608
  = _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3381);
  moonbit_decref(_M0L4dataS3381);
  while (1) {
    moonbit_string_t _M0L3keyS1611;
    struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1613;
    int32_t _M0L6_2atmpS3378;
    #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1613
    = _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L5_2aitS1608);
    if (_M0L7_2abindS1613 == 0) {
      if (_M0L7_2abindS1613) {
        moonbit_decref(_M0L7_2abindS1613);
      }
      moonbit_decref(_M0L5_2aitS1608);
    } else {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1614 =
        _M0L7_2abindS1613;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1615 =
        _M0L7_2aSomeS1614;
      moonbit_string_t _M0L8_2afieldS3388 = _M0L4_2axS1615->$0;
      int32_t _M0L6_2acntS3747 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1615));
      moonbit_string_t _M0L6_2akeyS1616;
      if (_M0L6_2acntS3747 > 1) {
        int32_t _M0L11_2anew__cntS3749 = _M0L6_2acntS3747 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1615), _M0L11_2anew__cntS3749);
        moonbit_incref(_M0L8_2afieldS3388);
      } else if (_M0L6_2acntS3747 == 1) {
        void* _M0L8_2afieldS3748 = _M0L4_2axS1615->$1;
        moonbit_decref(_M0L8_2afieldS3748);
        #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1615);
      }
      _M0L6_2akeyS1616 = _M0L8_2afieldS3388;
      _M0L3keyS1611 = _M0L6_2akeyS1616;
      goto join_1610;
    }
    goto joinlet_3812;
    join_1610:;
    #line 1487 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3378
    = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1609, _M0L3keyS1611);
    moonbit_decref(_M0L3keyS1611);
    if (!_M0L6_2atmpS3378) {
      int32_t _M0L3valS3380 = _M0L5countS1607->$0;
      int32_t _M0L6_2atmpS3379 = _M0L3valS3380 + 1;
      _M0L5countS1607->$0 = _M0L6_2atmpS3379;
    }
    continue;
    joinlet_3812:;
    break;
  }
  _result_3813 = _M0L5countS1607->$0;
  moonbit_decref(_M0L5countS1607);
  return _result_3813;
}

struct _M0TUsfE* _M0MP38JIA2JIA29moonbitdb3lib8Database7zpopmax(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1594,
  moonbit_string_t _M0L3keyS1595
) {
  #line 1462 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1463 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1594, _M0L3keyS1595)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1598;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3377 =
      _M0L4selfS1594->$0;
    void* _M0L7_2abindS1602;
    struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1599;
    int32_t _M0L3lenS1600;
    moonbit_incref(_M0L4dataS3377);
    #line 1466 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1602
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3377, _M0L3keyS1595);
    moonbit_decref(_M0L4dataS3377);
    if (_M0L7_2abindS1602 == 0) {
      if (_M0L7_2abindS1602) {
        moonbit_decref(_M0L7_2abindS1602);
      }
      goto join_1596;
    } else {
      void* _M0L7_2aSomeS1603 = _M0L7_2abindS1602;
      void* _M0L4_2axS1604 = _M0L7_2aSomeS1603;
      switch (Moonbit_object_tag(_M0L4_2axS1604)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1605 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1604;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3392 = _M0L7_2aZSetS1605->$0;
          int32_t _M0L6_2acntS3750 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1605));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1606;
          if (_M0L6_2acntS3750 > 1) {
            int32_t _M0L11_2anew__cntS3751 = _M0L6_2acntS3750 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1605), _M0L11_2anew__cntS3751);
            moonbit_incref(_M0L8_2afieldS3392);
          } else if (_M0L6_2acntS3750 == 1) {
            #line 1466 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1605);
          }
          _M0L7_2azsetS1606 = _M0L8_2afieldS3392;
          _M0L4zsetS1598 = _M0L7_2azsetS1606;
          goto join_1597;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1604);
          goto join_1596;
          break;
        }
      }
    }
    join_1597:;
    #line 1468 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6sortedS1599
    = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1594, _M0L3keyS1595);
    #line 1469 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3lenS1600 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1599);
    if (_M0L3lenS1600 == 0) {
      moonbit_decref(_M0L6sortedS1599);
      moonbit_decref(_M0L4zsetS1598);
      return 0;
    } else {
      int32_t _M0L6_2atmpS3376 = _M0L3lenS1600 - 1;
      struct _M0TUsfE* _M0L9max__itemS1601;
      moonbit_string_t _M0L6_2atmpS3373;
      struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3374;
      void* _M0L4ZSetS3375;
      #line 1473 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L9max__itemS1601
      = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1599, _M0L6_2atmpS3376);
      moonbit_decref(_M0L6sortedS1599);
      _M0L6_2atmpS3373 = _M0L9max__itemS1601->$0;
      moonbit_incref(_M0L6_2atmpS3373);
      #line 1474 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGsfE(_M0L4zsetS1598, _M0L6_2atmpS3373);
      moonbit_decref(_M0L6_2atmpS3373);
      _M0L4dataS3374 = _M0L4selfS1594->$0;
      _M0L4ZSetS3375
      = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet));
      Moonbit_object_header(_M0L4ZSetS3375)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 4);
      ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4ZSetS3375)->$0
      = _M0L4zsetS1598;
      moonbit_incref(_M0L4dataS3374);
      #line 1475 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3374, _M0L3keyS1595, _M0L4ZSetS3375);
      moonbit_decref(_M0L4dataS3374);
      moonbit_decref(_M0L4ZSetS3375);
      return _M0L9max__itemS1601;
    }
    join_1596:;
    return 0;
  }
}

float _M0MP38JIA2JIA29moonbitdb3lib8Database7zincrby(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1573,
  moonbit_string_t _M0L3keyS1574,
  float _M0L9incrementS1593,
  moonbit_string_t _M0L11member__valS1589
) {
  int32_t _M0L6_2atmpS3367;
  struct _M0TPB3MapGsfE* _M0L4zsetS1575;
  struct _M0TPB3MapGsfE* _M0L1zS1579;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3372;
  void* _M0L7_2abindS1580;
  struct _M0TUsfE** _M0L7_2abindS1577;
  struct _M0TUsfE** _M0L6_2atmpS3371;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3370;
  float _M0L1sS1587;
  float _M0L14current__scoreS1585;
  void* _M0L7_2abindS1588;
  float _M0L10new__scoreS1592;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3368;
  void* _M0L4ZSetS3369;
  #line 1379 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1380 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3367
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1573, _M0L3keyS1574);
  _M0L4dataS3372 = _M0L4selfS1573->$0;
  moonbit_incref(_M0L4dataS3372);
  #line 1381 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1580
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3372, _M0L3keyS1574);
  moonbit_decref(_M0L4dataS3372);
  if (_M0L7_2abindS1580 == 0) {
    if (_M0L7_2abindS1580) {
      moonbit_decref(_M0L7_2abindS1580);
    }
    goto join_1576;
  } else {
    void* _M0L7_2aSomeS1581 = _M0L7_2abindS1580;
    void* _M0L4_2axS1582 = _M0L7_2aSomeS1581;
    switch (Moonbit_object_tag(_M0L4_2axS1582)) {
      case 4: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1583 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1582;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3395 = _M0L7_2aZSetS1583->$0;
        int32_t _M0L6_2acntS3752 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1583));
        struct _M0TPB3MapGsfE* _M0L4_2azS1584;
        if (_M0L6_2acntS3752 > 1) {
          int32_t _M0L11_2anew__cntS3753 = _M0L6_2acntS3752 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1583), _M0L11_2anew__cntS3753);
          moonbit_incref(_M0L8_2afieldS3395);
        } else if (_M0L6_2acntS3752 == 1) {
          #line 1381 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aZSetS1583);
        }
        _M0L4_2azS1584 = _M0L8_2afieldS3395;
        _M0L1zS1579 = _M0L4_2azS1584;
        goto join_1578;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1582);
        goto join_1576;
        break;
      }
    }
  }
  goto joinlet_3817;
  join_1578:;
  _M0L4zsetS1575 = _M0L1zS1579;
  joinlet_3817:;
  goto joinlet_3816;
  join_1576:;
  _M0L7_2abindS1577 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3371 = _M0L7_2abindS1577;
  _M0L6_2atmpS3370
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3371, .$1 = 0, .$2 = 0
  };
  #line 1383 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4zsetS1575 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3370, 10ll);
  moonbit_decref(_M0L6_2atmpS3370.$0);
  joinlet_3816:;
  #line 1385 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1588
  = _M0MPB3Map3getGsfE(_M0L4zsetS1575, _M0L11member__valS1589);
  switch (Moonbit_object_tag(_M0L7_2abindS1588)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1590 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS1588;
      float _M0L4_2asS1591 = _M0L7_2aSomeS1590->$0;
      moonbit_decref(_M0L7_2aSomeS1590);
      _M0L1sS1587 = _M0L4_2asS1591;
      goto join_1586;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS1588);
      _M0L14current__scoreS1585 = 0x0p+0f;
      break;
    }
  }
  goto joinlet_3818;
  join_1586:;
  _M0L14current__scoreS1585 = _M0L1sS1587;
  joinlet_3818:;
  _M0L10new__scoreS1592 = _M0L14current__scoreS1585 + _M0L9incrementS1593;
  #line 1390 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1575, _M0L11member__valS1589, _M0L10new__scoreS1592);
  _M0L4dataS3368 = _M0L4selfS1573->$0;
  _M0L4ZSetS3369
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3369)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 4);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4ZSetS3369)->$0
  = _M0L4zsetS1575;
  moonbit_incref(_M0L4dataS3368);
  #line 1391 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3368, _M0L3keyS1574, _M0L4ZSetS3369);
  moonbit_decref(_M0L4dataS3368);
  moonbit_decref(_M0L4ZSetS3369);
  return _M0L10new__scoreS1592;
}

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database8zrevrank(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1557,
  moonbit_string_t _M0L3keyS1558,
  moonbit_string_t _M0L11member__valS1562
) {
  #line 1353 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1354 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1557, _M0L3keyS1558)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1561;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3366 =
      _M0L4selfS1557->$0;
    void* _M0L7_2abindS1568;
    int32_t _M0L6_2atmpS3359;
    moonbit_incref(_M0L4dataS3366);
    #line 1357 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1568
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3366, _M0L3keyS1558);
    moonbit_decref(_M0L4dataS3366);
    if (_M0L7_2abindS1568 == 0) {
      if (_M0L7_2abindS1568) {
        moonbit_decref(_M0L7_2abindS1568);
      }
      goto join_1559;
    } else {
      void* _M0L7_2aSomeS1569 = _M0L7_2abindS1568;
      void* _M0L4_2axS1570 = _M0L7_2aSomeS1569;
      switch (Moonbit_object_tag(_M0L4_2axS1570)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1571 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1570;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3398 = _M0L7_2aZSetS1571->$0;
          int32_t _M0L6_2acntS3756 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1571));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1572;
          if (_M0L6_2acntS3756 > 1) {
            int32_t _M0L11_2anew__cntS3757 = _M0L6_2acntS3756 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1571), _M0L11_2anew__cntS3757);
            moonbit_incref(_M0L8_2afieldS3398);
          } else if (_M0L6_2acntS3756 == 1) {
            #line 1357 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1571);
          }
          _M0L7_2azsetS1572 = _M0L8_2afieldS3398;
          _M0L4zsetS1561 = _M0L7_2azsetS1572;
          goto join_1560;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1570);
          goto join_1559;
          break;
        }
      }
    }
    join_1560:;
    #line 1359 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3359
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1561, _M0L11member__valS1562);
    moonbit_decref(_M0L4zsetS1561);
    if (!_M0L6_2atmpS3359) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1563;
      int32_t _M0L3lenS1564;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1565;
      int32_t _M0L1iS1566;
      int32_t _M0L3valS3365;
      #line 1362 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6sortedS1563
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1557, _M0L3keyS1558);
      #line 1363 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L3lenS1564 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1563);
      _M0L4rankS1565
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1565)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1565->$0 = 0;
      _M0L1iS1566 = 0;
      while (1) {
        if (_M0L1iS1566 < _M0L3lenS1564) {
          struct _M0TUsfE* _M0L6_2atmpS3361;
          moonbit_string_t _M0L8_2afieldS3397;
          int32_t _M0L6_2acntS3754;
          moonbit_string_t _M0L6_2atmpS3360;
          int32_t _result_3822;
          int32_t _M0L6_2atmpS3364;
          #line 1366 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0L6_2atmpS3361
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1563, _M0L1iS1566);
          _M0L8_2afieldS3397 = _M0L6_2atmpS3361->$0;
          _M0L6_2acntS3754
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3361));
          if (_M0L6_2acntS3754 > 1) {
            int32_t _M0L11_2anew__cntS3755 = _M0L6_2acntS3754 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3361), _M0L11_2anew__cntS3755);
            moonbit_incref(_M0L8_2afieldS3397);
          } else if (_M0L6_2acntS3754 == 1) {
            #line 1366 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2atmpS3361);
          }
          _M0L6_2atmpS3360 = _M0L8_2afieldS3397;
          #line 1366 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _result_3822
          = _M0L6_2atmpS3360 == _M0L11member__valS1562
            || Moonbit_array_length(_M0L6_2atmpS3360)
               == Moonbit_array_length(_M0L11member__valS1562)
               && 0
                  == memcmp(_M0L6_2atmpS3360, _M0L11member__valS1562, Moonbit_array_length(_M0L6_2atmpS3360) * 2);
          moonbit_decref(_M0L6_2atmpS3360);
          if (_result_3822) {
            int32_t _M0L6_2atmpS3363;
            int32_t _M0L6_2atmpS3362;
            moonbit_decref(_M0L6sortedS1563);
            _M0L6_2atmpS3363 = _M0L3lenS1564 - 1;
            _M0L6_2atmpS3362 = _M0L6_2atmpS3363 - _M0L1iS1566;
            _M0L4rankS1565->$0 = _M0L6_2atmpS3362;
            break;
          }
          _M0L6_2atmpS3364 = _M0L1iS1566 + 1;
          _M0L1iS1566 = _M0L6_2atmpS3364;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1563);
        }
        break;
      }
      _M0L3valS3365 = _M0L4rankS1565->$0;
      moonbit_decref(_M0L4rankS1565);
      return (int64_t)_M0L3valS3365;
    }
    join_1559:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database9zrevrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1546,
  moonbit_string_t _M0L3keyS1547,
  int32_t _M0L5startS1551,
  int32_t _M0L3endS1553
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1545;
  int32_t _M0L3lenS1548;
  moonbit_string_t* _M0L6_2atmpS3358;
  struct _M0TPB5ArrayGsE* _M0L6resultS1549;
  int32_t _M0L10start__idxS1550;
  int32_t _M0L8end__idxS1552;
  int32_t _M0L1iS1554;
  #line 1302 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1303 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6sortedS1545
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1546, _M0L3keyS1547);
  #line 1304 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1548 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1545);
  _M0L6_2atmpS3358 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1549
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1549)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1549->$0 = _M0L6_2atmpS3358;
  _M0L6resultS1549->$1 = 0;
  if (_M0L5startS1551 < 0) {
    _M0L10start__idxS1550 = _M0L3lenS1548 + _M0L5startS1551;
  } else {
    _M0L10start__idxS1550 = _M0L5startS1551;
  }
  if (_M0L3endS1553 < 0) {
    _M0L8end__idxS1552 = _M0L3lenS1548 + _M0L3endS1553;
  } else {
    _M0L8end__idxS1552 = _M0L3endS1553;
  }
  _M0L1iS1554 = _M0L10start__idxS1550;
  while (1) {
    int32_t _if__result_3824;
    if (_M0L1iS1554 <= _M0L8end__idxS1552) {
      _if__result_3824 = _M0L1iS1554 < _M0L3lenS1548;
    } else {
      _if__result_3824 = 0;
    }
    if (_if__result_3824) {
      int32_t _M0L6_2atmpS3356 = _M0L3lenS1548 - 1;
      int32_t _M0L8rev__idxS1555 = _M0L6_2atmpS3356 - _M0L1iS1554;
      int32_t _M0L6_2atmpS3357;
      if (_M0L8rev__idxS1555 >= 0) {
        struct _M0TUsfE* _M0L6_2atmpS3355;
        moonbit_string_t _M0L8_2afieldS3400;
        int32_t _M0L6_2acntS3758;
        moonbit_string_t _M0L6_2atmpS3354;
        #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3355
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1545, _M0L8rev__idxS1555);
        _M0L8_2afieldS3400 = _M0L6_2atmpS3355->$0;
        _M0L6_2acntS3758
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3355));
        if (_M0L6_2acntS3758 > 1) {
          int32_t _M0L11_2anew__cntS3759 = _M0L6_2acntS3758 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3355), _M0L11_2anew__cntS3759);
          moonbit_incref(_M0L8_2afieldS3400);
        } else if (_M0L6_2acntS3758 == 1) {
          #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2atmpS3355);
        }
        _M0L6_2atmpS3354 = _M0L8_2afieldS3400;
        #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1549, _M0L6_2atmpS3354);
        moonbit_decref(_M0L6_2atmpS3354);
      }
      _M0L6_2atmpS3357 = _M0L1iS1554 + 1;
      _M0L1iS1554 = _M0L6_2atmpS3357;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1545);
    }
    break;
  }
  return _M0L6resultS1549;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6zcount(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1536,
  moonbit_string_t _M0L3keyS1537,
  float _M0L10min__scoreS1542,
  float _M0L10max__scoreS1543
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1535;
  struct _M0TPB8MutLocalGiE* _M0L5countS1538;
  int32_t _M0L7_2abindS1539;
  int32_t _M0L2__S1540;
  int32_t _result_3827;
  #line 1291 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1292 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6sortedS1535
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1536, _M0L3keyS1537);
  _M0L5countS1538
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1538)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1538->$0 = 0;
  _M0L7_2abindS1539 = _M0L6sortedS1535->$1;
  _M0L2__S1540 = 0;
  while (1) {
    if (_M0L2__S1540 < _M0L7_2abindS1539) {
      struct _M0TUsfE** _M0L3bufS3353 = _M0L6sortedS1535->$0;
      struct _M0TUsfE* _M0L4itemS1541 =
        (struct _M0TUsfE*)_M0L3bufS3353[_M0L2__S1540];
      float _M0L6_2atmpS3349 = _M0L4itemS1541->$1;
      int32_t _if__result_3826;
      int32_t _M0L6_2atmpS3352;
      if (_M0L6_2atmpS3349 >= _M0L10min__scoreS1542) {
        float _M0L6_2atmpS3348 = _M0L4itemS1541->$1;
        _if__result_3826 = _M0L6_2atmpS3348 <= _M0L10max__scoreS1543;
      } else {
        _if__result_3826 = 0;
      }
      if (_if__result_3826) {
        int32_t _M0L3valS3351 = _M0L5countS1538->$0;
        int32_t _M0L6_2atmpS3350 = _M0L3valS3351 + 1;
        _M0L5countS1538->$0 = _M0L6_2atmpS3350;
      }
      _M0L6_2atmpS3352 = _M0L2__S1540 + 1;
      _M0L2__S1540 = _M0L6_2atmpS3352;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1535);
    }
    break;
  }
  _result_3827 = _M0L5countS1538->$0;
  moonbit_decref(_M0L5countS1538);
  return _result_3827;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database13zrangebyscore(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1526,
  moonbit_string_t _M0L3keyS1527,
  float _M0L10min__scoreS1532,
  float _M0L10max__scoreS1533
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1525;
  moonbit_string_t* _M0L6_2atmpS3347;
  struct _M0TPB5ArrayGsE* _M0L6resultS1528;
  int32_t _M0L7_2abindS1529;
  int32_t _M0L2__S1530;
  #line 1280 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1281 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6sortedS1525
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1526, _M0L3keyS1527);
  _M0L6_2atmpS3347 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1528
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1528)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1528->$0 = _M0L6_2atmpS3347;
  _M0L6resultS1528->$1 = 0;
  _M0L7_2abindS1529 = _M0L6sortedS1525->$1;
  _M0L2__S1530 = 0;
  while (1) {
    if (_M0L2__S1530 < _M0L7_2abindS1529) {
      struct _M0TUsfE** _M0L3bufS3346 = _M0L6sortedS1525->$0;
      struct _M0TUsfE* _M0L4itemS1531 =
        (struct _M0TUsfE*)_M0L3bufS3346[_M0L2__S1530];
      float _M0L6_2atmpS3343 = _M0L4itemS1531->$1;
      int32_t _if__result_3829;
      int32_t _M0L6_2atmpS3345;
      if (_M0L6_2atmpS3343 >= _M0L10min__scoreS1532) {
        float _M0L6_2atmpS3342 = _M0L4itemS1531->$1;
        _if__result_3829 = _M0L6_2atmpS3342 <= _M0L10max__scoreS1533;
      } else {
        _if__result_3829 = 0;
      }
      if (_if__result_3829) {
        moonbit_string_t _M0L6_2atmpS3344 = _M0L4itemS1531->$0;
        moonbit_incref(_M0L6_2atmpS3344);
        #line 1285 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1528, _M0L6_2atmpS3344);
        moonbit_decref(_M0L6_2atmpS3344);
      }
      _M0L6_2atmpS3345 = _M0L2__S1530 + 1;
      _M0L2__S1530 = _M0L6_2atmpS3345;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1525);
    }
    break;
  }
  return _M0L6resultS1528;
}

struct _M0TPB5ArrayGUsfEE* _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1504,
  moonbit_string_t _M0L3keyS1505
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1504, _M0L3keyS1505)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS3337 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_3830 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_3830)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _block_3830->$0 = _M0L6_2atmpS3337;
    _block_3830->$1 = 0;
    return _block_3830;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1508;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3341 =
      _M0L4selfS1504->$0;
    void* _M0L7_2abindS1520;
    struct _M0TUsfE** _M0L6_2atmpS3340;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1509;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1510;
    struct _M0TPB5ArrayGUsfEE* _result_3835;
    struct _M0TUsfE** _M0L6_2atmpS3338;
    struct _M0TPB5ArrayGUsfEE* _block_3836;
    moonbit_incref(_M0L4dataS3341);
    #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1520
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3341, _M0L3keyS1505);
    moonbit_decref(_M0L4dataS3341);
    if (_M0L7_2abindS1520 == 0) {
      if (_M0L7_2abindS1520) {
        moonbit_decref(_M0L7_2abindS1520);
      }
      goto join_1506;
    } else {
      void* _M0L7_2aSomeS1521 = _M0L7_2abindS1520;
      void* _M0L4_2axS1522 = _M0L7_2aSomeS1521;
      switch (Moonbit_object_tag(_M0L4_2axS1522)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1523 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1522;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3407 = _M0L7_2aZSetS1523->$0;
          int32_t _M0L6_2acntS3762 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1523));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1524;
          if (_M0L6_2acntS3762 > 1) {
            int32_t _M0L11_2anew__cntS3763 = _M0L6_2acntS3762 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1523), _M0L11_2anew__cntS3763);
            moonbit_incref(_M0L8_2afieldS3407);
          } else if (_M0L6_2acntS3762 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1523);
          }
          _M0L7_2azsetS1524 = _M0L8_2afieldS3407;
          _M0L4zsetS1508 = _M0L7_2azsetS1524;
          goto join_1507;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1522);
          goto join_1506;
          break;
        }
      }
    }
    join_1507:;
    _M0L6_2atmpS3340 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1509
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1509)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _M0L5itemsS1509->$0 = _M0L6_2atmpS3340;
    _M0L5itemsS1509->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1510 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1508);
    moonbit_decref(_M0L4zsetS1508);
    while (1) {
      moonbit_string_t _M0L1mS1512;
      float _M0L1sS1513;
      struct _M0TUsfE* _M0L7_2abindS1515;
      struct _M0TUsfE* _M0L8_2atupleS3339;
      #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1515 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1510);
      if (_M0L7_2abindS1515 == 0) {
        if (_M0L7_2abindS1515) {
          moonbit_decref(_M0L7_2abindS1515);
        }
        moonbit_decref(_M0L5_2aitS1510);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1516 = _M0L7_2abindS1515;
        struct _M0TUsfE* _M0L4_2axS1517 = _M0L7_2aSomeS1516;
        moonbit_string_t _M0L4_2amS1518 = _M0L4_2axS1517->$0;
        float _M0L4_2asS1519 = _M0L4_2axS1517->$1;
        int32_t _M0L6_2acntS3760 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1517));
        if (_M0L6_2acntS3760 > 1) {
          int32_t _M0L11_2anew__cntS3761 = _M0L6_2acntS3760 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1517), _M0L11_2anew__cntS3761);
          moonbit_incref(_M0L4_2amS1518);
        } else if (_M0L6_2acntS3760 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1517);
        }
        _M0L1mS1512 = _M0L4_2amS1518;
        _M0L1sS1513 = _M0L4_2asS1519;
        goto join_1511;
      }
      goto joinlet_3834;
      join_1511:;
      _M0L8_2atupleS3339
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3339)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
      _M0L8_2atupleS3339->$0 = _M0L1mS1512;
      _M0L8_2atupleS3339->$1 = _M0L1sS1513;
      #line 1271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1509, _M0L8_2atupleS3339);
      moonbit_decref(_M0L8_2atupleS3339);
      continue;
      joinlet_3834:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3835
    = _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(_M0L5itemsS1509);
    moonbit_decref(_M0L5itemsS1509);
    return _result_3835;
    join_1506:;
    _M0L6_2atmpS3338 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_3836
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_3836)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _block_3836->$0 = _M0L6_2atmpS3338;
    _block_3836->$1 = 0;
    return _block_3836;
  }
}

void* _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1493,
  moonbit_string_t _M0L3keyS1494,
  moonbit_string_t _M0L11member__valS1498
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1493, _M0L3keyS1494)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1497;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3336 =
      _M0L4selfS1493->$0;
    void* _M0L7_2abindS1499;
    void* _result_3839;
    moonbit_incref(_M0L4dataS3336);
    #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1499
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3336, _M0L3keyS1494);
    moonbit_decref(_M0L4dataS3336);
    if (_M0L7_2abindS1499 == 0) {
      if (_M0L7_2abindS1499) {
        moonbit_decref(_M0L7_2abindS1499);
      }
      goto join_1495;
    } else {
      void* _M0L7_2aSomeS1500 = _M0L7_2abindS1499;
      void* _M0L4_2axS1501 = _M0L7_2aSomeS1500;
      switch (Moonbit_object_tag(_M0L4_2axS1501)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1502 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1501;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3409 = _M0L7_2aZSetS1502->$0;
          int32_t _M0L6_2acntS3764 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1502));
          struct _M0TPB3MapGsfE* _M0L4_2azS1503;
          if (_M0L6_2acntS3764 > 1) {
            int32_t _M0L11_2anew__cntS3765 = _M0L6_2acntS3764 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1502), _M0L11_2anew__cntS3765);
            moonbit_incref(_M0L8_2afieldS3409);
          } else if (_M0L6_2acntS3764 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1502);
          }
          _M0L4_2azS1503 = _M0L8_2afieldS3409;
          _M0L1zS1497 = _M0L4_2azS1503;
          goto join_1496;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1501);
          goto join_1495;
          break;
        }
      }
    }
    join_1496:;
    #line 1239 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3839 = _M0MPB3Map3getGsfE(_M0L1zS1497, _M0L11member__valS1498);
    moonbit_decref(_M0L1zS1497);
    return _result_3839;
    join_1495:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1492
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  return _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L5itemsS1492);
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1484
) {
  int32_t _M0L3lenS1483;
  #line 1181 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1483 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1484);
  if (_M0L3lenS1483 <= 1) {
    moonbit_incref(_M0L3arrS1484);
    return _M0L3arrS1484;
  } else {
    int32_t _M0L3midS1485 = _M0L3lenS1483 / 2;
    struct _M0TUsfE** _M0L6_2atmpS3335 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1486 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS3334;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1487;
    int32_t _M0L1iS1488;
    int32_t _M0L1iS1490;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3332;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3333;
    struct _M0TPB5ArrayGUsfEE* _result_3842;
    Moonbit_object_header(_M0L4leftS1486)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _M0L4leftS1486->$0 = _M0L6_2atmpS3335;
    _M0L4leftS1486->$1 = 0;
    _M0L6_2atmpS3334 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1487
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1487)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _M0L5rightS1487->$0 = _M0L6_2atmpS3334;
    _M0L5rightS1487->$1 = 0;
    _M0L1iS1488 = 0;
    while (1) {
      if (_M0L1iS1488 < _M0L3midS1485) {
        struct _M0TUsfE* _M0L6_2atmpS3328;
        int32_t _M0L6_2atmpS3329;
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3328
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1484, _M0L1iS1488);
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1486, _M0L6_2atmpS3328);
        moonbit_decref(_M0L6_2atmpS3328);
        _M0L6_2atmpS3329 = _M0L1iS1488 + 1;
        _M0L1iS1488 = _M0L6_2atmpS3329;
        continue;
      }
      break;
    }
    _M0L1iS1490 = _M0L3midS1485;
    while (1) {
      if (_M0L1iS1490 < _M0L3lenS1483) {
        struct _M0TUsfE* _M0L6_2atmpS3330;
        int32_t _M0L6_2atmpS3331;
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3330
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1484, _M0L1iS1490);
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1487, _M0L6_2atmpS3330);
        moonbit_decref(_M0L6_2atmpS3330);
        _M0L6_2atmpS3331 = _M0L1iS1490 + 1;
        _M0L1iS1490 = _M0L6_2atmpS3331;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3332
    = _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L4leftS1486);
    moonbit_decref(_M0L4leftS1486);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3333
    = _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L5rightS1487);
    moonbit_decref(_M0L5rightS1487);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3842
    = _M0FP38JIA2JIA29moonbitdb3lib5merge(_M0L6_2atmpS3332, _M0L6_2atmpS3333);
    moonbit_decref(_M0L6_2atmpS3332);
    moonbit_decref(_M0L6_2atmpS3333);
    return _result_3842;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1478,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1479
) {
  struct _M0TUsfE** _M0L6_2atmpS3327;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1475;
  struct _M0TPB8MutLocalGiE* _M0L1iS1476;
  struct _M0TPB8MutLocalGiE* _M0L1jS1477;
  #line 1199 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3327 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1475
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1475)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
  _M0L6resultS1475->$0 = _M0L6_2atmpS3327;
  _M0L6resultS1475->$1 = 0;
  _M0L1iS1476
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1476)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1476->$0 = 0;
  _M0L1jS1477
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1477)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1477->$0 = 0;
  while (1) {
    int32_t _M0L3valS3299 = _M0L1iS1476->$0;
    int32_t _M0L6_2atmpS3300;
    int32_t _if__result_3844;
    #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3300 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1478);
    if (_M0L3valS3299 < _M0L6_2atmpS3300) {
      int32_t _M0L3valS3297 = _M0L1jS1477->$0;
      int32_t _M0L6_2atmpS3298;
      #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3298 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1479);
      _if__result_3844 = _M0L3valS3297 < _M0L6_2atmpS3298;
    } else {
      _if__result_3844 = 0;
    }
    if (_if__result_3844) {
      int32_t _M0L3valS3306 = _M0L1iS1476->$0;
      struct _M0TUsfE* _M0L6_2atmpS3305;
      float _M0L6_2atmpS3301;
      int32_t _M0L3valS3304;
      struct _M0TUsfE* _M0L6_2atmpS3303;
      float _M0L6_2atmpS3302;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3305
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1478, _M0L3valS3306);
      _M0L6_2atmpS3301 = _M0L6_2atmpS3305->$1;
      moonbit_decref(_M0L6_2atmpS3305);
      _M0L3valS3304 = _M0L1jS1477->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3303
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1479, _M0L3valS3304);
      _M0L6_2atmpS3302 = _M0L6_2atmpS3303->$1;
      moonbit_decref(_M0L6_2atmpS3303);
      if (_M0L6_2atmpS3301 <= _M0L6_2atmpS3302) {
        int32_t _M0L3valS3308 = _M0L1iS1476->$0;
        struct _M0TUsfE* _M0L6_2atmpS3307;
        int32_t _M0L3valS3310;
        int32_t _M0L6_2atmpS3309;
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3307
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1478, _M0L3valS3308);
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1475, _M0L6_2atmpS3307);
        moonbit_decref(_M0L6_2atmpS3307);
        _M0L3valS3310 = _M0L1iS1476->$0;
        _M0L6_2atmpS3309 = _M0L3valS3310 + 1;
        _M0L1iS1476->$0 = _M0L6_2atmpS3309;
      } else {
        int32_t _M0L3valS3312 = _M0L1jS1477->$0;
        struct _M0TUsfE* _M0L6_2atmpS3311;
        int32_t _M0L3valS3314;
        int32_t _M0L6_2atmpS3313;
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3311
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1479, _M0L3valS3312);
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1475, _M0L6_2atmpS3311);
        moonbit_decref(_M0L6_2atmpS3311);
        _M0L3valS3314 = _M0L1jS1477->$0;
        _M0L6_2atmpS3313 = _M0L3valS3314 + 1;
        _M0L1jS1477->$0 = _M0L6_2atmpS3313;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3315 = _M0L1iS1476->$0;
    int32_t _M0L6_2atmpS3316;
    #line 1212 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3316 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1478);
    if (_M0L3valS3315 < _M0L6_2atmpS3316) {
      int32_t _M0L3valS3318 = _M0L1iS1476->$0;
      struct _M0TUsfE* _M0L6_2atmpS3317;
      int32_t _M0L3valS3320;
      int32_t _M0L6_2atmpS3319;
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3317
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1478, _M0L3valS3318);
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1475, _M0L6_2atmpS3317);
      moonbit_decref(_M0L6_2atmpS3317);
      _M0L3valS3320 = _M0L1iS1476->$0;
      _M0L6_2atmpS3319 = _M0L3valS3320 + 1;
      _M0L1iS1476->$0 = _M0L6_2atmpS3319;
      continue;
    } else {
      moonbit_decref(_M0L1iS1476);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3321 = _M0L1jS1477->$0;
    int32_t _M0L6_2atmpS3322;
    #line 1216 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3322 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1479);
    if (_M0L3valS3321 < _M0L6_2atmpS3322) {
      int32_t _M0L3valS3324 = _M0L1jS1477->$0;
      struct _M0TUsfE* _M0L6_2atmpS3323;
      int32_t _M0L3valS3326;
      int32_t _M0L6_2atmpS3325;
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3323
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1479, _M0L3valS3324);
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1475, _M0L6_2atmpS3323);
      moonbit_decref(_M0L6_2atmpS3323);
      _M0L3valS3326 = _M0L1jS1477->$0;
      _M0L6_2atmpS3325 = _M0L3valS3326 + 1;
      _M0L1jS1477->$0 = _M0L6_2atmpS3325;
      continue;
    } else {
      moonbit_decref(_M0L1jS1477);
    }
    break;
  }
  return _M0L6resultS1475;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1460,
  moonbit_string_t _M0L3keyS1461,
  float _M0L5scoreS1474,
  moonbit_string_t _M0L11member__valS1473
) {
  int32_t _M0L6_2atmpS3291;
  struct _M0TPB3MapGsfE* _M0L4zsetS1462;
  struct _M0TPB3MapGsfE* _M0L1zS1466;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3296;
  void* _M0L7_2abindS1467;
  struct _M0TUsfE** _M0L7_2abindS1464;
  struct _M0TUsfE** _M0L6_2atmpS3295;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3294;
  int32_t _M0L7existedS1472;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3292;
  void* _M0L4ZSetS3293;
  #line 1140 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3291
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1460, _M0L3keyS1461);
  _M0L4dataS3296 = _M0L4selfS1460->$0;
  moonbit_incref(_M0L4dataS3296);
  #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1467
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3296, _M0L3keyS1461);
  moonbit_decref(_M0L4dataS3296);
  if (_M0L7_2abindS1467 == 0) {
    if (_M0L7_2abindS1467) {
      moonbit_decref(_M0L7_2abindS1467);
    }
    goto join_1463;
  } else {
    void* _M0L7_2aSomeS1468 = _M0L7_2abindS1467;
    void* _M0L4_2axS1469 = _M0L7_2aSomeS1468;
    switch (Moonbit_object_tag(_M0L4_2axS1469)) {
      case 4: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1470 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1469;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3412 = _M0L7_2aZSetS1470->$0;
        int32_t _M0L6_2acntS3766 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1470));
        struct _M0TPB3MapGsfE* _M0L4_2azS1471;
        if (_M0L6_2acntS3766 > 1) {
          int32_t _M0L11_2anew__cntS3767 = _M0L6_2acntS3766 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1470), _M0L11_2anew__cntS3767);
          moonbit_incref(_M0L8_2afieldS3412);
        } else if (_M0L6_2acntS3766 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aZSetS1470);
        }
        _M0L4_2azS1471 = _M0L8_2afieldS3412;
        _M0L1zS1466 = _M0L4_2azS1471;
        goto join_1465;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1469);
        goto join_1463;
        break;
      }
    }
  }
  goto joinlet_3848;
  join_1465:;
  _M0L4zsetS1462 = _M0L1zS1466;
  joinlet_3848:;
  goto joinlet_3847;
  join_1463:;
  _M0L7_2abindS1464 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3295 = _M0L7_2abindS1464;
  _M0L6_2atmpS3294
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3295, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4zsetS1462 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3294, 10ll);
  moonbit_decref(_M0L6_2atmpS3294.$0);
  joinlet_3847:;
  #line 1146 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7existedS1472
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1462, _M0L11member__valS1473);
  #line 1147 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1462, _M0L11member__valS1473, _M0L5scoreS1474);
  _M0L4dataS3292 = _M0L4selfS1460->$0;
  _M0L4ZSetS3293
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3293)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 4);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4ZSetS3293)->$0
  = _M0L4zsetS1462;
  moonbit_incref(_M0L4dataS3292);
  #line 1148 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3292, _M0L3keyS1461, _M0L4ZSetS3293);
  moonbit_decref(_M0L4dataS3292);
  moonbit_decref(_M0L4ZSetS3293);
  return !_M0L7existedS1472;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database5sdiff(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1424,
  struct _M0TPB5ArrayGsE* _M0L4keysS1422
) {
  int32_t _M0L6_2atmpS3277;
  moonbit_string_t _M0L6_2atmpS3290;
  struct _M0TPB3MapGsbE* _M0L10first__setS1423;
  struct _M0TUsbE** _M0L7_2abindS1426;
  struct _M0TUsbE** _M0L6_2atmpS3289;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3286;
  int32_t _M0L6_2atmpS3288;
  int64_t _M0L6_2atmpS3287;
  struct _M0TPB3MapGsbE* _M0L6resultS1425;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1427;
  int32_t _M0L1iS1435;
  moonbit_string_t* _M0L6_2atmpS3285;
  struct _M0TPB5ArrayGsE* _M0L3arrS1451;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1452;
  #line 1051 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1052 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3277 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1422);
  if (_M0L6_2atmpS3277 == 0) {
    moonbit_string_t* _M0L6_2atmpS3278 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3849 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3849)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3849->$0 = _M0L6_2atmpS3278;
    _block_3849->$1 = 0;
    return _block_3849;
  }
  #line 1055 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3290 = _M0MPC15array5Array2atGsE(_M0L4keysS1422, 0);
  #line 1055 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L10first__setS1423
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(_M0L4selfS1424, _M0L6_2atmpS3290);
  moonbit_decref(_M0L6_2atmpS3290);
  _M0L7_2abindS1426 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3289 = _M0L7_2abindS1426;
  _M0L6_2atmpS3286
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3289, .$1 = 0, .$2 = 0
  };
  #line 1056 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3288 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1423);
  _M0L6_2atmpS3287 = (int64_t)_M0L6_2atmpS3288;
  #line 1056 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6resultS1425 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3286, _M0L6_2atmpS3287);
  moonbit_decref(_M0L6_2atmpS3286.$0);
  #line 1056 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1427 = _M0MPB3Map5iter2GsbE(_M0L10first__setS1423);
  moonbit_decref(_M0L10first__setS1423);
  while (1) {
    moonbit_string_t _M0L1mS1429;
    struct _M0TUsbE* _M0L7_2abindS1431;
    #line 1057 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1431 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1427);
    if (_M0L7_2abindS1431 == 0) {
      if (_M0L7_2abindS1431) {
        moonbit_decref(_M0L7_2abindS1431);
      }
      moonbit_decref(_M0L5_2aitS1427);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1432 = _M0L7_2abindS1431;
      struct _M0TUsbE* _M0L4_2axS1433 = _M0L7_2aSomeS1432;
      moonbit_string_t _M0L8_2afieldS3418 = _M0L4_2axS1433->$0;
      int32_t _M0L6_2acntS3768 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1433));
      moonbit_string_t _M0L4_2amS1434;
      if (_M0L6_2acntS3768 > 1) {
        int32_t _M0L11_2anew__cntS3769 = _M0L6_2acntS3768 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1433), _M0L11_2anew__cntS3769);
        moonbit_incref(_M0L8_2afieldS3418);
      } else if (_M0L6_2acntS3768 == 1) {
        #line 1057 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1433);
      }
      _M0L4_2amS1434 = _M0L8_2afieldS3418;
      _M0L1mS1429 = _M0L4_2amS1434;
      goto join_1428;
    }
    goto joinlet_3851;
    join_1428:;
    #line 1058 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsbE(_M0L6resultS1425, _M0L1mS1429, 1);
    moonbit_decref(_M0L1mS1429);
    continue;
    joinlet_3851:;
    break;
  }
  _M0L1iS1435 = 1;
  while (1) {
    int32_t _M0L6_2atmpS3279;
    #line 1060 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3279 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1422);
    if (_M0L1iS1435 < _M0L6_2atmpS3279) {
      moonbit_string_t _M0L6_2atmpS3283;
      struct _M0TPB3MapGsbE* _M0L12current__setS1436;
      moonbit_string_t* _M0L6_2atmpS3282;
      struct _M0TPB5ArrayGsE* _M0L10to__removeS1437;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1438;
      int32_t _M0L7_2abindS1446;
      int32_t _M0L2__S1447;
      int32_t _M0L6_2atmpS3284;
      #line 1061 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3283
      = _M0MPC15array5Array2atGsE(_M0L4keysS1422, _M0L1iS1435);
      #line 1061 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L12current__setS1436
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(_M0L4selfS1424, _M0L6_2atmpS3283);
      moonbit_decref(_M0L6_2atmpS3283);
      _M0L6_2atmpS3282 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L10to__removeS1437
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L10to__removeS1437)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
      _M0L10to__removeS1437->$0 = _M0L6_2atmpS3282;
      _M0L10to__removeS1437->$1 = 0;
      #line 1062 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L5_2aitS1438 = _M0MPB3Map5iter2GsbE(_M0L6resultS1425);
      while (1) {
        moonbit_string_t _M0L1mS1440;
        struct _M0TUsbE* _M0L7_2abindS1442;
        #line 1063 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1442 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1438);
        if (_M0L7_2abindS1442 == 0) {
          if (_M0L7_2abindS1442) {
            moonbit_decref(_M0L7_2abindS1442);
          }
          moonbit_decref(_M0L5_2aitS1438);
          moonbit_decref(_M0L12current__setS1436);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1443 = _M0L7_2abindS1442;
          struct _M0TUsbE* _M0L4_2axS1444 = _M0L7_2aSomeS1443;
          moonbit_string_t _M0L8_2afieldS3417 = _M0L4_2axS1444->$0;
          int32_t _M0L6_2acntS3770 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1444));
          moonbit_string_t _M0L4_2amS1445;
          if (_M0L6_2acntS3770 > 1) {
            int32_t _M0L11_2anew__cntS3771 = _M0L6_2acntS3770 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1444), _M0L11_2anew__cntS3771);
            moonbit_incref(_M0L8_2afieldS3417);
          } else if (_M0L6_2acntS3770 == 1) {
            #line 1063 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L4_2axS1444);
          }
          _M0L4_2amS1445 = _M0L8_2afieldS3417;
          _M0L1mS1440 = _M0L4_2amS1445;
          goto join_1439;
        }
        goto joinlet_3854;
        join_1439:;
        #line 1064 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        if (_M0MPB3Map8containsGsbE(_M0L12current__setS1436, _M0L1mS1440)) {
          #line 1065 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPC15array5Array4pushGsE(_M0L10to__removeS1437, _M0L1mS1440);
          moonbit_decref(_M0L1mS1440);
        } else {
          moonbit_decref(_M0L1mS1440);
        }
        continue;
        joinlet_3854:;
        break;
      }
      _M0L7_2abindS1446 = _M0L10to__removeS1437->$1;
      _M0L2__S1447 = 0;
      while (1) {
        if (_M0L2__S1447 < _M0L7_2abindS1446) {
          moonbit_string_t* _M0L3bufS3281 = _M0L10to__removeS1437->$0;
          moonbit_string_t _M0L1mS1448 =
            (moonbit_string_t)_M0L3bufS3281[_M0L2__S1447];
          int32_t _M0L6_2atmpS3280;
          moonbit_incref(_M0L1mS1448);
          #line 1069 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPB3Map6removeGsbE(_M0L6resultS1425, _M0L1mS1448);
          moonbit_decref(_M0L1mS1448);
          _M0L6_2atmpS3280 = _M0L2__S1447 + 1;
          _M0L2__S1447 = _M0L6_2atmpS3280;
          continue;
        } else {
          moonbit_decref(_M0L10to__removeS1437);
        }
        break;
      }
      _M0L6_2atmpS3284 = _M0L1iS1435 + 1;
      _M0L1iS1435 = _M0L6_2atmpS3284;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3285 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1451
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1451)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1451->$0 = _M0L6_2atmpS3285;
  _M0L3arrS1451->$1 = 0;
  #line 1072 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1452 = _M0MPB3Map5iter2GsbE(_M0L6resultS1425);
  moonbit_decref(_M0L6resultS1425);
  while (1) {
    moonbit_string_t _M0L1mS1454;
    struct _M0TUsbE* _M0L7_2abindS1456;
    #line 1073 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1456 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1452);
    if (_M0L7_2abindS1456 == 0) {
      if (_M0L7_2abindS1456) {
        moonbit_decref(_M0L7_2abindS1456);
      }
      moonbit_decref(_M0L5_2aitS1452);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1457 = _M0L7_2abindS1456;
      struct _M0TUsbE* _M0L4_2axS1458 = _M0L7_2aSomeS1457;
      moonbit_string_t _M0L8_2afieldS3414 = _M0L4_2axS1458->$0;
      int32_t _M0L6_2acntS3772 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1458));
      moonbit_string_t _M0L4_2amS1459;
      if (_M0L6_2acntS3772 > 1) {
        int32_t _M0L11_2anew__cntS3773 = _M0L6_2acntS3772 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1458), _M0L11_2anew__cntS3773);
        moonbit_incref(_M0L8_2afieldS3414);
      } else if (_M0L6_2acntS3772 == 1) {
        #line 1073 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1458);
      }
      _M0L4_2amS1459 = _M0L8_2afieldS3414;
      _M0L1mS1454 = _M0L4_2amS1459;
      goto join_1453;
    }
    goto joinlet_3857;
    join_1453:;
    #line 1074 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1451, _M0L1mS1454);
    moonbit_decref(_M0L1mS1454);
    continue;
    joinlet_3857:;
    break;
  }
  return _M0L3arrS1451;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6sunion(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1403,
  struct _M0TPB5ArrayGsE* _M0L4keysS1399
) {
  struct _M0TUsbE** _M0L7_2abindS1397;
  struct _M0TUsbE** _M0L6_2atmpS3276;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3275;
  struct _M0TPB3MapGsbE* _M0L6resultS1396;
  int32_t _M0L7_2abindS1398;
  int32_t _M0L2__S1400;
  moonbit_string_t* _M0L6_2atmpS3274;
  struct _M0TPB5ArrayGsE* _M0L3arrS1413;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1414;
  #line 1026 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1397 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3276 = _M0L7_2abindS1397;
  _M0L6_2atmpS3275
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3276, .$1 = 0, .$2 = 0
  };
  #line 1027 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6resultS1396 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3275, 10ll);
  moonbit_decref(_M0L6_2atmpS3275.$0);
  _M0L7_2abindS1398 = _M0L4keysS1399->$1;
  _M0L2__S1400 = 0;
  while (1) {
    if (_M0L2__S1400 < _M0L7_2abindS1398) {
      moonbit_string_t* _M0L3bufS3273 = _M0L4keysS1399->$0;
      moonbit_string_t _M0L3keyS1401 =
        (moonbit_string_t)_M0L3bufS3273[_M0L2__S1400];
      struct _M0TPB3MapGsbE* _M0L12current__setS1402;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1404;
      int32_t _M0L6_2atmpS3272;
      moonbit_incref(_M0L3keyS1401);
      #line 1029 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L12current__setS1402
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(_M0L4selfS1403, _M0L3keyS1401);
      moonbit_decref(_M0L3keyS1401);
      #line 1029 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L5_2aitS1404 = _M0MPB3Map5iter2GsbE(_M0L12current__setS1402);
      moonbit_decref(_M0L12current__setS1402);
      while (1) {
        moonbit_string_t _M0L1mS1406;
        struct _M0TUsbE* _M0L7_2abindS1408;
        #line 1030 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1408 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1404);
        if (_M0L7_2abindS1408 == 0) {
          if (_M0L7_2abindS1408) {
            moonbit_decref(_M0L7_2abindS1408);
          }
          moonbit_decref(_M0L5_2aitS1404);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1409 = _M0L7_2abindS1408;
          struct _M0TUsbE* _M0L4_2axS1410 = _M0L7_2aSomeS1409;
          moonbit_string_t _M0L8_2afieldS3420 = _M0L4_2axS1410->$0;
          int32_t _M0L6_2acntS3774 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1410));
          moonbit_string_t _M0L4_2amS1411;
          if (_M0L6_2acntS3774 > 1) {
            int32_t _M0L11_2anew__cntS3775 = _M0L6_2acntS3774 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1410), _M0L11_2anew__cntS3775);
            moonbit_incref(_M0L8_2afieldS3420);
          } else if (_M0L6_2acntS3774 == 1) {
            #line 1030 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L4_2axS1410);
          }
          _M0L4_2amS1411 = _M0L8_2afieldS3420;
          _M0L1mS1406 = _M0L4_2amS1411;
          goto join_1405;
        }
        goto joinlet_3860;
        join_1405:;
        #line 1031 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map3setGsbE(_M0L6resultS1396, _M0L1mS1406, 1);
        moonbit_decref(_M0L1mS1406);
        continue;
        joinlet_3860:;
        break;
      }
      _M0L6_2atmpS3272 = _M0L2__S1400 + 1;
      _M0L2__S1400 = _M0L6_2atmpS3272;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3274 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1413
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1413)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1413->$0 = _M0L6_2atmpS3274;
  _M0L3arrS1413->$1 = 0;
  #line 1034 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1414 = _M0MPB3Map5iter2GsbE(_M0L6resultS1396);
  moonbit_decref(_M0L6resultS1396);
  while (1) {
    moonbit_string_t _M0L1mS1416;
    struct _M0TUsbE* _M0L7_2abindS1418;
    #line 1035 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1418 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1414);
    if (_M0L7_2abindS1418 == 0) {
      if (_M0L7_2abindS1418) {
        moonbit_decref(_M0L7_2abindS1418);
      }
      moonbit_decref(_M0L5_2aitS1414);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1419 = _M0L7_2abindS1418;
      struct _M0TUsbE* _M0L4_2axS1420 = _M0L7_2aSomeS1419;
      moonbit_string_t _M0L8_2afieldS3419 = _M0L4_2axS1420->$0;
      int32_t _M0L6_2acntS3776 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1420));
      moonbit_string_t _M0L4_2amS1421;
      if (_M0L6_2acntS3776 > 1) {
        int32_t _M0L11_2anew__cntS3777 = _M0L6_2acntS3776 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1420), _M0L11_2anew__cntS3777);
        moonbit_incref(_M0L8_2afieldS3419);
      } else if (_M0L6_2acntS3776 == 1) {
        #line 1035 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1420);
      }
      _M0L4_2amS1421 = _M0L8_2afieldS3419;
      _M0L1mS1416 = _M0L4_2amS1421;
      goto join_1415;
    }
    goto joinlet_3862;
    join_1415:;
    #line 1036 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1413, _M0L1mS1416);
    moonbit_decref(_M0L1mS1416);
    continue;
    joinlet_3862:;
    break;
  }
  return _M0L3arrS1413;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6sinter(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1360,
  struct _M0TPB5ArrayGsE* _M0L4keysS1358
) {
  int32_t _M0L6_2atmpS3255;
  moonbit_string_t _M0L6_2atmpS3271;
  struct _M0TPB3MapGsbE* _M0L10first__setS1359;
  int32_t _M0L6_2atmpS3257;
  struct _M0TUsbE** _M0L7_2abindS1362;
  struct _M0TUsbE** _M0L6_2atmpS3270;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3267;
  int32_t _M0L6_2atmpS3269;
  int64_t _M0L6_2atmpS3268;
  struct _M0TPB3MapGsbE* _M0L6resultS1361;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1363;
  int32_t _M0L1iS1371;
  moonbit_string_t* _M0L6_2atmpS3266;
  struct _M0TPB5ArrayGsE* _M0L3arrS1387;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1388;
  #line 985 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 986 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3255 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1358);
  if (_M0L6_2atmpS3255 == 0) {
    moonbit_string_t* _M0L6_2atmpS3256 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3863 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3863)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3863->$0 = _M0L6_2atmpS3256;
    _block_3863->$1 = 0;
    return _block_3863;
  }
  #line 989 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3271 = _M0MPC15array5Array2atGsE(_M0L4keysS1358, 0);
  #line 989 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L10first__setS1359
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(_M0L4selfS1360, _M0L6_2atmpS3271);
  moonbit_decref(_M0L6_2atmpS3271);
  #line 990 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3257 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1359);
  if (_M0L6_2atmpS3257 == 0) {
    moonbit_string_t* _M0L6_2atmpS3258;
    struct _M0TPB5ArrayGsE* _block_3864;
    moonbit_decref(_M0L10first__setS1359);
    _M0L6_2atmpS3258 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_3864
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3864)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3864->$0 = _M0L6_2atmpS3258;
    _block_3864->$1 = 0;
    return _block_3864;
  }
  _M0L7_2abindS1362 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3270 = _M0L7_2abindS1362;
  _M0L6_2atmpS3267
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3270, .$1 = 0, .$2 = 0
  };
  #line 993 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3269 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1359);
  _M0L6_2atmpS3268 = (int64_t)_M0L6_2atmpS3269;
  #line 993 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6resultS1361 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3267, _M0L6_2atmpS3268);
  moonbit_decref(_M0L6_2atmpS3267.$0);
  #line 993 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1363 = _M0MPB3Map5iter2GsbE(_M0L10first__setS1359);
  moonbit_decref(_M0L10first__setS1359);
  while (1) {
    moonbit_string_t _M0L1mS1365;
    struct _M0TUsbE* _M0L7_2abindS1367;
    #line 994 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1367 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1363);
    if (_M0L7_2abindS1367 == 0) {
      if (_M0L7_2abindS1367) {
        moonbit_decref(_M0L7_2abindS1367);
      }
      moonbit_decref(_M0L5_2aitS1363);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1368 = _M0L7_2abindS1367;
      struct _M0TUsbE* _M0L4_2axS1369 = _M0L7_2aSomeS1368;
      moonbit_string_t _M0L8_2afieldS3427 = _M0L4_2axS1369->$0;
      int32_t _M0L6_2acntS3778 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1369));
      moonbit_string_t _M0L4_2amS1370;
      if (_M0L6_2acntS3778 > 1) {
        int32_t _M0L11_2anew__cntS3779 = _M0L6_2acntS3778 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1369), _M0L11_2anew__cntS3779);
        moonbit_incref(_M0L8_2afieldS3427);
      } else if (_M0L6_2acntS3778 == 1) {
        #line 994 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1369);
      }
      _M0L4_2amS1370 = _M0L8_2afieldS3427;
      _M0L1mS1365 = _M0L4_2amS1370;
      goto join_1364;
    }
    goto joinlet_3866;
    join_1364:;
    #line 995 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsbE(_M0L6resultS1361, _M0L1mS1365, 1);
    moonbit_decref(_M0L1mS1365);
    continue;
    joinlet_3866:;
    break;
  }
  _M0L1iS1371 = 1;
  while (1) {
    int32_t _M0L6_2atmpS3259;
    #line 997 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3259 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1358);
    if (_M0L1iS1371 < _M0L6_2atmpS3259) {
      moonbit_string_t _M0L6_2atmpS3264;
      struct _M0TPB3MapGsbE* _M0L12current__setS1372;
      moonbit_string_t* _M0L6_2atmpS3263;
      struct _M0TPB5ArrayGsE* _M0L10to__removeS1373;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1374;
      int32_t _M0L7_2abindS1382;
      int32_t _M0L2__S1383;
      int32_t _M0L6_2atmpS3265;
      #line 998 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3264
      = _M0MPC15array5Array2atGsE(_M0L4keysS1358, _M0L1iS1371);
      #line 998 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L12current__setS1372
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(_M0L4selfS1360, _M0L6_2atmpS3264);
      moonbit_decref(_M0L6_2atmpS3264);
      _M0L6_2atmpS3263 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L10to__removeS1373
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L10to__removeS1373)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
      _M0L10to__removeS1373->$0 = _M0L6_2atmpS3263;
      _M0L10to__removeS1373->$1 = 0;
      #line 999 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L5_2aitS1374 = _M0MPB3Map5iter2GsbE(_M0L6resultS1361);
      while (1) {
        moonbit_string_t _M0L1mS1376;
        struct _M0TUsbE* _M0L7_2abindS1378;
        int32_t _M0L6_2atmpS3260;
        #line 1000 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1378 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1374);
        if (_M0L7_2abindS1378 == 0) {
          if (_M0L7_2abindS1378) {
            moonbit_decref(_M0L7_2abindS1378);
          }
          moonbit_decref(_M0L5_2aitS1374);
          moonbit_decref(_M0L12current__setS1372);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1379 = _M0L7_2abindS1378;
          struct _M0TUsbE* _M0L4_2axS1380 = _M0L7_2aSomeS1379;
          moonbit_string_t _M0L8_2afieldS3426 = _M0L4_2axS1380->$0;
          int32_t _M0L6_2acntS3780 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1380));
          moonbit_string_t _M0L4_2amS1381;
          if (_M0L6_2acntS3780 > 1) {
            int32_t _M0L11_2anew__cntS3781 = _M0L6_2acntS3780 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1380), _M0L11_2anew__cntS3781);
            moonbit_incref(_M0L8_2afieldS3426);
          } else if (_M0L6_2acntS3780 == 1) {
            #line 1000 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L4_2axS1380);
          }
          _M0L4_2amS1381 = _M0L8_2afieldS3426;
          _M0L1mS1376 = _M0L4_2amS1381;
          goto join_1375;
        }
        goto joinlet_3869;
        join_1375:;
        #line 1001 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3260
        = _M0MPB3Map8containsGsbE(_M0L12current__setS1372, _M0L1mS1376);
        if (!_M0L6_2atmpS3260) {
          #line 1002 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPC15array5Array4pushGsE(_M0L10to__removeS1373, _M0L1mS1376);
          moonbit_decref(_M0L1mS1376);
        } else {
          moonbit_decref(_M0L1mS1376);
        }
        continue;
        joinlet_3869:;
        break;
      }
      _M0L7_2abindS1382 = _M0L10to__removeS1373->$1;
      _M0L2__S1383 = 0;
      while (1) {
        if (_M0L2__S1383 < _M0L7_2abindS1382) {
          moonbit_string_t* _M0L3bufS3262 = _M0L10to__removeS1373->$0;
          moonbit_string_t _M0L1mS1384 =
            (moonbit_string_t)_M0L3bufS3262[_M0L2__S1383];
          int32_t _M0L6_2atmpS3261;
          moonbit_incref(_M0L1mS1384);
          #line 1006 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPB3Map6removeGsbE(_M0L6resultS1361, _M0L1mS1384);
          moonbit_decref(_M0L1mS1384);
          _M0L6_2atmpS3261 = _M0L2__S1383 + 1;
          _M0L2__S1383 = _M0L6_2atmpS3261;
          continue;
        } else {
          moonbit_decref(_M0L10to__removeS1373);
        }
        break;
      }
      _M0L6_2atmpS3265 = _M0L1iS1371 + 1;
      _M0L1iS1371 = _M0L6_2atmpS3265;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3266 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1387
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1387)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1387->$0 = _M0L6_2atmpS3266;
  _M0L3arrS1387->$1 = 0;
  #line 1009 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1388 = _M0MPB3Map5iter2GsbE(_M0L6resultS1361);
  moonbit_decref(_M0L6resultS1361);
  while (1) {
    moonbit_string_t _M0L1mS1390;
    struct _M0TUsbE* _M0L7_2abindS1392;
    #line 1010 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1392 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1388);
    if (_M0L7_2abindS1392 == 0) {
      if (_M0L7_2abindS1392) {
        moonbit_decref(_M0L7_2abindS1392);
      }
      moonbit_decref(_M0L5_2aitS1388);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1393 = _M0L7_2abindS1392;
      struct _M0TUsbE* _M0L4_2axS1394 = _M0L7_2aSomeS1393;
      moonbit_string_t _M0L8_2afieldS3423 = _M0L4_2axS1394->$0;
      int32_t _M0L6_2acntS3782 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1394));
      moonbit_string_t _M0L4_2amS1395;
      if (_M0L6_2acntS3782 > 1) {
        int32_t _M0L11_2anew__cntS3783 = _M0L6_2acntS3782 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1394), _M0L11_2anew__cntS3783);
        moonbit_incref(_M0L8_2afieldS3423);
      } else if (_M0L6_2acntS3782 == 1) {
        #line 1010 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1394);
      }
      _M0L4_2amS1395 = _M0L8_2afieldS3423;
      _M0L1mS1390 = _M0L4_2amS1395;
      goto join_1389;
    }
    goto joinlet_3872;
    join_1389:;
    #line 1011 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1387, _M0L1mS1390);
    moonbit_decref(_M0L1mS1390);
    continue;
    joinlet_3872:;
    break;
  }
  return _M0L3arrS1387;
}

struct _M0TPB3MapGsbE* _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1346,
  moonbit_string_t _M0L3keyS1347
) {
  #line 974 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 975 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1346, _M0L3keyS1347)
  ) {
    struct _M0TUsbE** _M0L7_2abindS1348 =
      (struct _M0TUsbE**)moonbit_empty_ref_array;
    struct _M0TUsbE** _M0L6_2atmpS3251 = _M0L7_2abindS1348;
    struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3250 =
      (struct _M0TPB9ArrayViewGUsbEE){.$0 = _M0L6_2atmpS3251,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGsbE* _result_3873;
    #line 976 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3873 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3250, 0ll);
    moonbit_decref(_M0L6_2atmpS3250.$0);
    return _result_3873;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1352;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3254 =
      _M0L4selfS1346->$0;
    void* _M0L7_2abindS1353;
    struct _M0TUsbE** _M0L7_2abindS1350;
    struct _M0TUsbE** _M0L6_2atmpS3253;
    struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3252;
    struct _M0TPB3MapGsbE* _result_3876;
    moonbit_incref(_M0L4dataS3254);
    #line 978 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1353
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3254, _M0L3keyS1347);
    moonbit_decref(_M0L4dataS3254);
    if (_M0L7_2abindS1353 == 0) {
      if (_M0L7_2abindS1353) {
        moonbit_decref(_M0L7_2abindS1353);
      }
      goto join_1349;
    } else {
      void* _M0L7_2aSomeS1354 = _M0L7_2abindS1353;
      void* _M0L4_2axS1355 = _M0L7_2aSomeS1354;
      switch (Moonbit_object_tag(_M0L4_2axS1355)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1356 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1355;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3428 = _M0L6_2aSetS1356->$0;
          int32_t _M0L6_2acntS3784 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1356));
          struct _M0TPB3MapGsbE* _M0L4_2asS1357;
          if (_M0L6_2acntS3784 > 1) {
            int32_t _M0L11_2anew__cntS3785 = _M0L6_2acntS3784 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1356), _M0L11_2anew__cntS3785);
            moonbit_incref(_M0L8_2afieldS3428);
          } else if (_M0L6_2acntS3784 == 1) {
            #line 978 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1356);
          }
          _M0L4_2asS1357 = _M0L8_2afieldS3428;
          _M0L1sS1352 = _M0L4_2asS1357;
          goto join_1351;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1355);
          goto join_1349;
          break;
        }
      }
    }
    join_1351:;
    return _M0L1sS1352;
    join_1349:;
    _M0L7_2abindS1350 = (struct _M0TUsbE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3253 = _M0L7_2abindS1350;
    _M0L6_2atmpS3252
    = (struct _M0TPB9ArrayViewGUsbEE){
      .$0 = _M0L6_2atmpS3253, .$1 = 0, .$2 = 0
    };
    #line 980 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3876 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3252, 0ll);
    moonbit_decref(_M0L6_2atmpS3252.$0);
    return _result_3876;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1333,
  moonbit_string_t _M0L3keyS1334,
  moonbit_string_t _M0L5valueS1345
) {
  int32_t _M0L6_2atmpS3244;
  struct _M0TPB3MapGsbE* _M0L3setS1335;
  struct _M0TPB3MapGsbE* _M0L1sS1339;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3249;
  void* _M0L7_2abindS1340;
  struct _M0TUsbE** _M0L7_2abindS1337;
  struct _M0TUsbE** _M0L6_2atmpS3248;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3247;
  #line 902 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3244
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1333, _M0L3keyS1334);
  _M0L4dataS3249 = _M0L4selfS1333->$0;
  moonbit_incref(_M0L4dataS3249);
  #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1340
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3249, _M0L3keyS1334);
  moonbit_decref(_M0L4dataS3249);
  if (_M0L7_2abindS1340 == 0) {
    if (_M0L7_2abindS1340) {
      moonbit_decref(_M0L7_2abindS1340);
    }
    goto join_1336;
  } else {
    void* _M0L7_2aSomeS1341 = _M0L7_2abindS1340;
    void* _M0L4_2axS1342 = _M0L7_2aSomeS1341;
    switch (Moonbit_object_tag(_M0L4_2axS1342)) {
      case 3: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1343 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1342;
        struct _M0TPB3MapGsbE* _M0L8_2afieldS3431 = _M0L6_2aSetS1343->$0;
        int32_t _M0L6_2acntS3786 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1343));
        struct _M0TPB3MapGsbE* _M0L4_2asS1344;
        if (_M0L6_2acntS3786 > 1) {
          int32_t _M0L11_2anew__cntS3787 = _M0L6_2acntS3786 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1343), _M0L11_2anew__cntS3787);
          moonbit_incref(_M0L8_2afieldS3431);
        } else if (_M0L6_2acntS3786 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2aSetS1343);
        }
        _M0L4_2asS1344 = _M0L8_2afieldS3431;
        _M0L1sS1339 = _M0L4_2asS1344;
        goto join_1338;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1342);
        goto join_1336;
        break;
      }
    }
  }
  goto joinlet_3878;
  join_1338:;
  _M0L3setS1335 = _M0L1sS1339;
  joinlet_3878:;
  goto joinlet_3877;
  join_1336:;
  _M0L7_2abindS1337 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3248 = _M0L7_2abindS1337;
  _M0L6_2atmpS3247
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3248, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3setS1335 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3247, 10ll);
  moonbit_decref(_M0L6_2atmpS3247.$0);
  joinlet_3877:;
  #line 908 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1335, _M0L5valueS1345)) {
    moonbit_decref(_M0L3setS1335);
    return 0;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3245;
    void* _M0L3SetS3246;
    #line 911 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1335, _M0L5valueS1345, 1);
    _M0L4dataS3245 = _M0L4selfS1333->$0;
    _M0L3SetS3246
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS3246)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 16, 3);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L3SetS3246)->$0
    = _M0L3setS1335;
    moonbit_incref(_M0L4dataS3245);
    #line 912 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3245, _M0L3keyS1334, _M0L3SetS3246);
    moonbit_decref(_M0L4dataS3245);
    moonbit_decref(_M0L3SetS3246);
    return 1;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1324,
  moonbit_string_t _M0L3keyS1325
) {
  #line 709 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1324, _M0L3keyS1325)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1327;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3243 =
      _M0L4selfS1324->$0;
    void* _M0L7_2abindS1328;
    int32_t _result_3880;
    moonbit_incref(_M0L4dataS3243);
    #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1328
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3243, _M0L3keyS1325);
    moonbit_decref(_M0L4dataS3243);
    if (_M0L7_2abindS1328 == 0) {
      if (_M0L7_2abindS1328) {
        moonbit_decref(_M0L7_2abindS1328);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1329 = _M0L7_2abindS1328;
      void* _M0L4_2axS1330 = _M0L7_2aSomeS1329;
      switch (Moonbit_object_tag(_M0L4_2axS1330)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1331 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1330;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3433 =
            _M0L7_2aListS1331->$0;
          int32_t _M0L6_2acntS3788 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1331));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1332;
          if (_M0L6_2acntS3788 > 1) {
            int32_t _M0L11_2anew__cntS3789 = _M0L6_2acntS3788 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1331), _M0L11_2anew__cntS3789);
            moonbit_incref(_M0L8_2afieldS3433);
          } else if (_M0L6_2acntS3788 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1331);
          }
          _M0L4_2adS1332 = _M0L8_2afieldS3433;
          _M0L1dS1327 = _M0L4_2adS1332;
          goto join_1326;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1330);
          return 0;
          break;
        }
      }
    }
    join_1326:;
    #line 714 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3880 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L1dS1327);
    moonbit_decref(_M0L1dS1327);
    return _result_3880;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4lpop(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1313,
  moonbit_string_t _M0L3keyS1314
) {
  #line 679 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 680 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1313, _M0L3keyS1314)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1317;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3242 =
      _M0L4selfS1313->$0;
    void* _M0L7_2abindS1319;
    moonbit_string_t _M0L3valS1318;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3240;
    void* _M0L4ListS3241;
    moonbit_incref(_M0L4dataS3242);
    #line 683 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1319
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3242, _M0L3keyS1314);
    moonbit_decref(_M0L4dataS3242);
    if (_M0L7_2abindS1319 == 0) {
      if (_M0L7_2abindS1319) {
        moonbit_decref(_M0L7_2abindS1319);
      }
      goto join_1315;
    } else {
      void* _M0L7_2aSomeS1320 = _M0L7_2abindS1319;
      void* _M0L4_2axS1321 = _M0L7_2aSomeS1320;
      switch (Moonbit_object_tag(_M0L4_2axS1321)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1322 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1321;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3436 =
            _M0L7_2aListS1322->$0;
          int32_t _M0L6_2acntS3790 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1322));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2adequeS1323;
          if (_M0L6_2acntS3790 > 1) {
            int32_t _M0L11_2anew__cntS3791 = _M0L6_2acntS3790 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1322), _M0L11_2anew__cntS3791);
            moonbit_incref(_M0L8_2afieldS3436);
          } else if (_M0L6_2acntS3790 == 1) {
            #line 683 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1322);
          }
          _M0L8_2adequeS1323 = _M0L8_2afieldS3436;
          _M0L5dequeS1317 = _M0L8_2adequeS1323;
          goto join_1316;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1321);
          goto join_1315;
          break;
        }
      }
    }
    join_1316:;
    #line 685 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3valS1318
    = _M0MP38JIA2JIA29moonbitdb3lib5Deque10pop__front(_M0L5dequeS1317);
    _M0L4dataS3240 = _M0L4selfS1313->$0;
    _M0L4ListS3241
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3241)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS3241)->$0
    = _M0L5dequeS1317;
    moonbit_incref(_M0L4dataS3240);
    #line 686 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3240, _M0L3keyS1314, _M0L4ListS3241);
    moonbit_decref(_M0L4dataS3240);
    moonbit_decref(_M0L4ListS3241);
    return _M0L3valS1318;
    join_1315:;
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1301,
  moonbit_string_t _M0L3keyS1302,
  moonbit_string_t _M0L5valueS1312
) {
  int32_t _M0L6_2atmpS3236;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1303;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1306;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3239;
  void* _M0L7_2abindS1307;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3237;
  void* _M0L4ListS3238;
  int32_t _result_3885;
  #line 668 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 669 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3236
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1301, _M0L3keyS1302);
  _M0L4dataS3239 = _M0L4selfS1301->$0;
  moonbit_incref(_M0L4dataS3239);
  #line 670 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1307
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3239, _M0L3keyS1302);
  moonbit_decref(_M0L4dataS3239);
  if (_M0L7_2abindS1307 == 0) {
    if (_M0L7_2abindS1307) {
      moonbit_decref(_M0L7_2abindS1307);
    }
    goto join_1304;
  } else {
    void* _M0L7_2aSomeS1308 = _M0L7_2abindS1307;
    void* _M0L4_2axS1309 = _M0L7_2aSomeS1308;
    switch (Moonbit_object_tag(_M0L4_2axS1309)) {
      case 2: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1310 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1309;
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3439 =
          _M0L7_2aListS1310->$0;
        int32_t _M0L6_2acntS3792 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1310));
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1311;
        if (_M0L6_2acntS3792 > 1) {
          int32_t _M0L11_2anew__cntS3793 = _M0L6_2acntS3792 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1310), _M0L11_2anew__cntS3793);
          moonbit_incref(_M0L8_2afieldS3439);
        } else if (_M0L6_2acntS3792 == 1) {
          #line 670 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aListS1310);
        }
        _M0L4_2adS1311 = _M0L8_2afieldS3439;
        _M0L1dS1306 = _M0L4_2adS1311;
        goto join_1305;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1309);
        goto join_1304;
        break;
      }
    }
  }
  goto joinlet_3884;
  join_1305:;
  _M0L5dequeS1303 = _M0L1dS1306;
  joinlet_3884:;
  goto joinlet_3883;
  join_1304:;
  #line 672 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5dequeS1303 = _M0MP38JIA2JIA29moonbitdb3lib5Deque3new();
  joinlet_3883:;
  #line 674 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib5Deque10push__back(_M0L5dequeS1303, _M0L5valueS1312);
  _M0L4dataS3237 = _M0L4selfS1301->$0;
  moonbit_incref(_M0L5dequeS1303);
  _M0L4ListS3238
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
  Moonbit_object_header(_M0L4ListS3238)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS3238)->$0
  = _M0L5dequeS1303;
  moonbit_incref(_M0L4dataS3237);
  #line 675 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3237, _M0L3keyS1302, _M0L4ListS3238);
  moonbit_decref(_M0L4dataS3237);
  moonbit_decref(_M0L4ListS3238);
  #line 676 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3885 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L5dequeS1303);
  moonbit_decref(_M0L5dequeS1303);
  return _result_3885;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1290,
  moonbit_string_t _M0L3keyS1291,
  moonbit_string_t _M0L5fieldS1295
) {
  #line 606 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1290, _M0L3keyS1291)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1294;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3235 =
      _M0L4selfS1290->$0;
    void* _M0L7_2abindS1296;
    moonbit_string_t _result_3888;
    moonbit_incref(_M0L4dataS3235);
    #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1296
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3235, _M0L3keyS1291);
    moonbit_decref(_M0L4dataS3235);
    if (_M0L7_2abindS1296 == 0) {
      if (_M0L7_2abindS1296) {
        moonbit_decref(_M0L7_2abindS1296);
      }
      goto join_1292;
    } else {
      void* _M0L7_2aSomeS1297 = _M0L7_2abindS1296;
      void* _M0L4_2axS1298 = _M0L7_2aSomeS1297;
      switch (Moonbit_object_tag(_M0L4_2axS1298)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1299 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1298;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3441 = _M0L7_2aHashS1299->$0;
          int32_t _M0L6_2acntS3794 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1299));
          struct _M0TPB3MapGssE* _M0L4_2ahS1300;
          if (_M0L6_2acntS3794 > 1) {
            int32_t _M0L11_2anew__cntS3795 = _M0L6_2acntS3794 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1299), _M0L11_2anew__cntS3795);
            moonbit_incref(_M0L8_2afieldS3441);
          } else if (_M0L6_2acntS3794 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1299);
          }
          _M0L4_2ahS1300 = _M0L8_2afieldS3441;
          _M0L1hS1294 = _M0L4_2ahS1300;
          goto join_1293;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1298);
          goto join_1292;
          break;
        }
      }
    }
    join_1293:;
    #line 611 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3888 = _M0MPB3Map3getGssE(_M0L1hS1294, _M0L5fieldS1295);
    moonbit_decref(_M0L1hS1294);
    return _result_3888;
    join_1292:;
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1276,
  moonbit_string_t _M0L3keyS1277,
  moonbit_string_t _M0L5fieldS1288,
  moonbit_string_t _M0L5valueS1289
) {
  int32_t _M0L6_2atmpS3229;
  struct _M0TPB3MapGssE* _M0L4hashS1278;
  struct _M0TPB3MapGssE* _M0L1hS1282;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3234;
  void* _M0L7_2abindS1283;
  struct _M0TUssE** _M0L7_2abindS1280;
  struct _M0TUssE** _M0L6_2atmpS3233;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3232;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3230;
  void* _M0L4HashS3231;
  #line 596 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3229
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1276, _M0L3keyS1277);
  _M0L4dataS3234 = _M0L4selfS1276->$0;
  moonbit_incref(_M0L4dataS3234);
  #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1283
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3234, _M0L3keyS1277);
  moonbit_decref(_M0L4dataS3234);
  if (_M0L7_2abindS1283 == 0) {
    if (_M0L7_2abindS1283) {
      moonbit_decref(_M0L7_2abindS1283);
    }
    goto join_1279;
  } else {
    void* _M0L7_2aSomeS1284 = _M0L7_2abindS1283;
    void* _M0L4_2axS1285 = _M0L7_2aSomeS1284;
    switch (Moonbit_object_tag(_M0L4_2axS1285)) {
      case 1: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1286 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1285;
        struct _M0TPB3MapGssE* _M0L8_2afieldS3444 = _M0L7_2aHashS1286->$0;
        int32_t _M0L6_2acntS3796 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1286));
        struct _M0TPB3MapGssE* _M0L4_2ahS1287;
        if (_M0L6_2acntS3796 > 1) {
          int32_t _M0L11_2anew__cntS3797 = _M0L6_2acntS3796 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1286), _M0L11_2anew__cntS3797);
          moonbit_incref(_M0L8_2afieldS3444);
        } else if (_M0L6_2acntS3796 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aHashS1286);
        }
        _M0L4_2ahS1287 = _M0L8_2afieldS3444;
        _M0L1hS1282 = _M0L4_2ahS1287;
        goto join_1281;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1285);
        goto join_1279;
        break;
      }
    }
  }
  goto joinlet_3890;
  join_1281:;
  _M0L4hashS1278 = _M0L1hS1282;
  joinlet_3890:;
  goto joinlet_3889;
  join_1279:;
  _M0L7_2abindS1280 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3233 = _M0L7_2abindS1280;
  _M0L6_2atmpS3232
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS3233, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4hashS1278 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3232, 10ll);
  moonbit_decref(_M0L6_2atmpS3232.$0);
  joinlet_3889:;
  #line 602 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1278, _M0L5fieldS1288, _M0L5valueS1289);
  _M0L4dataS3230 = _M0L4selfS1276->$0;
  _M0L4HashS3231
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS3231)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 22, 1);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4HashS3231)->$0
  = _M0L4hashS1278;
  moonbit_incref(_M0L4dataS3230);
  #line 603 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3230, _M0L3keyS1277, _M0L4HashS3231);
  moonbit_decref(_M0L4dataS3230);
  moonbit_decref(_M0L4HashS3231);
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1273,
  moonbit_string_t _M0L3keyS1274
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3221;
  int32_t _M0L6_2atmpS3220;
  #line 226 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3221 = _M0L4selfS1273->$0;
  moonbit_incref(_M0L4dataS3221);
  #line 227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3220
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3221, _M0L3keyS1274);
  moonbit_decref(_M0L4dataS3221);
  if (!_M0L6_2atmpS3220) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS3222 = _M0L4selfS1273->$1;
    int32_t _result_3891;
    moonbit_incref(_M0L7expiresS3222);
    #line 229 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3891 = _M0MPB3Map8containsGsiE(_M0L7expiresS3222, _M0L3keyS1274);
    moonbit_decref(_M0L7expiresS3222);
    if (_result_3891) {
      struct _M0TPB3MapGsiE* _M0L7expiresS3228 = _M0L4selfS1273->$1;
      int64_t _M0L6_2atmpS3227;
      int32_t _M0L6_2atmpS3225;
      int32_t _M0L13current__timeS3226;
      int32_t _M0L9remainingS1275;
      moonbit_incref(_M0L7expiresS3228);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3227 = _M0MPB3Map3getGsiE(_M0L7expiresS3228, _M0L3keyS1274);
      moonbit_decref(_M0L7expiresS3228);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3225 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS3227);
      _M0L13current__timeS3226 = _M0L4selfS1273->$2;
      _M0L9remainingS1275 = _M0L6_2atmpS3225 - _M0L13current__timeS3226;
      if (_M0L9remainingS1275 <= 0) {
        struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3223 =
          _M0L4selfS1273->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS3224;
        moonbit_incref(_M0L4dataS3223);
        #line 232 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3223, _M0L3keyS1274);
        moonbit_decref(_M0L4dataS3223);
        _M0L7expiresS3224 = _M0L4selfS1273->$1;
        moonbit_incref(_M0L7expiresS3224);
        #line 233 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS3224, _M0L3keyS1274);
        moonbit_decref(_M0L7expiresS3224);
        return -2;
      } else {
        return _M0L9remainingS1275 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1270,
  moonbit_string_t _M0L3keyS1271,
  int32_t _M0L7secondsS1272
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3215;
  int32_t _result_3892;
  #line 208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3215 = _M0L4selfS1270->$0;
  moonbit_incref(_M0L4dataS3215);
  #line 209 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3892
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3215, _M0L3keyS1271);
  moonbit_decref(_M0L4dataS3215);
  if (_result_3892) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3216 = _M0L4selfS1270->$1;
    int32_t _M0L13current__timeS3218 = _M0L4selfS1270->$2;
    int32_t _M0L6_2atmpS3219 = _M0L7secondsS1272 * 1000;
    int32_t _M0L6_2atmpS3217 = _M0L13current__timeS3218 + _M0L6_2atmpS3219;
    moonbit_incref(_M0L7expiresS3216);
    #line 210 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS3216, _M0L3keyS1271, _M0L6_2atmpS3217);
    moonbit_decref(_M0L7expiresS3216);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1265,
  moonbit_string_t _M0L3keyS1266
) {
  int32_t _M0L12expire__timeS1264;
  struct _M0TPB3MapGsiE* _M0L7expiresS3214;
  int64_t _M0L7_2abindS1267;
  int32_t _M0L13current__timeS3211;
  #line 188 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7expiresS3214 = _M0L4selfS1265->$1;
  moonbit_incref(_M0L7expiresS3214);
  #line 189 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1267 = _M0MPB3Map3getGsiE(_M0L7expiresS3214, _M0L3keyS1266);
  moonbit_decref(_M0L7expiresS3214);
  if (_M0L7_2abindS1267 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1268 = _M0L7_2abindS1267;
    int32_t _M0L15_2aexpire__timeS1269 = (int32_t)_M0L7_2aSomeS1268;
    _M0L12expire__timeS1264 = _M0L15_2aexpire__timeS1269;
    goto join_1263;
  }
  join_1263:;
  _M0L13current__timeS3211 = _M0L4selfS1265->$2;
  if (_M0L12expire__timeS1264 <= _M0L13current__timeS3211) {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3212 =
      _M0L4selfS1265->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS3213;
    moonbit_incref(_M0L4dataS3212);
    #line 192 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3212, _M0L3keyS1266);
    moonbit_decref(_M0L4dataS3212);
    _M0L7expiresS3213 = _M0L4selfS1265->$1;
    moonbit_incref(_M0L7expiresS3213);
    #line 193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3213, _M0L3keyS1266);
    moonbit_decref(_M0L7expiresS3213);
    return 1;
  } else {
    return 0;
  }
}

struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0MP38JIA2JIA29moonbitdb3lib8Database3new(
  
) {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS1261;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS3210;
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L6_2atmpS3209;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3205;
  struct _M0TUsiE** _M0L7_2abindS1262;
  struct _M0TUsiE** _M0L6_2atmpS3208;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3207;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3206;
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _block_3894;
  #line 176 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1261
  = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3210 = _M0L7_2abindS1261;
  _M0L6_2atmpS3209
  = (struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE){
    .$0 = _M0L6_2atmpS3210, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3205
  = _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L6_2atmpS3209, 1000ll);
  moonbit_decref(_M0L6_2atmpS3209.$0);
  _M0L7_2abindS1262 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3208 = _M0L7_2abindS1262;
  _M0L6_2atmpS3207
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3208, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3206 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3207, 1000ll);
  moonbit_decref(_M0L6_2atmpS3207.$0);
  _block_3894
  = (struct _M0TP38JIA2JIA29moonbitdb3lib8Database*)moonbit_malloc(sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database));
  Moonbit_object_header(_block_3894)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
  _block_3894->$0 = _M0L6_2atmpS3205;
  _block_3894->$1 = _M0L6_2atmpS3206;
  _block_3894->$2 = 0;
  return _block_3894;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1260
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3204;
  int32_t _M0L6_2atmpS3201;
  struct _M0TPB5ArrayGsE* _M0L4backS3203;
  int32_t _M0L6_2atmpS3202;
  #line 44 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3204 = _M0L4selfS1260->$0;
  moonbit_incref(_M0L5frontS3204);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3201 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3204);
  moonbit_decref(_M0L5frontS3204);
  _M0L4backS3203 = _M0L4selfS1260->$1;
  moonbit_incref(_M0L4backS3203);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3202 = _M0MPC15array5Array6lengthGsE(_M0L4backS3203);
  moonbit_decref(_M0L4backS3203);
  return _M0L6_2atmpS3201 + _M0L6_2atmpS3202;
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib5Deque10pop__front(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1253
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3195;
  int32_t _M0L6_2atmpS3194;
  struct _M0TPB5ArrayGsE* _M0L5frontS3200;
  moonbit_string_t _result_3897;
  #line 18 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3195 = _M0L4selfS1253->$0;
  moonbit_incref(_M0L5frontS3195);
  #line 19 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3194 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3195);
  moonbit_decref(_M0L5frontS3195);
  if (_M0L6_2atmpS3194 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L4backS3197 = _M0L4selfS1253->$1;
      int32_t _M0L6_2atmpS3196;
      moonbit_incref(_M0L4backS3197);
      #line 20 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3196 = _M0MPC15array5Array6lengthGsE(_M0L4backS3197);
      moonbit_decref(_M0L4backS3197);
      if (_M0L6_2atmpS3196 > 0) {
        struct _M0TPB5ArrayGsE* _M0L4backS3199 = _M0L4selfS1253->$1;
        moonbit_string_t _M0L4itemS1254;
        moonbit_string_t _M0L1vS1256;
        struct _M0TPB5ArrayGsE* _M0L5frontS3198;
        moonbit_incref(_M0L4backS3199);
        #line 21 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L4itemS1254 = _M0MPC15array5Array3popGsE(_M0L4backS3199);
        moonbit_decref(_M0L4backS3199);
        if (_M0L4itemS1254 == 0) {
          if (_M0L4itemS1254) {
            moonbit_decref(_M0L4itemS1254);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1257 = _M0L4itemS1254;
          moonbit_string_t _M0L4_2avS1258 = _M0L7_2aSomeS1257;
          _M0L1vS1256 = _M0L4_2avS1258;
          goto join_1255;
        }
        goto joinlet_3896;
        join_1255:;
        _M0L5frontS3198 = _M0L4selfS1253->$0;
        moonbit_incref(_M0L5frontS3198);
        #line 23 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L5frontS3198, _M0L1vS1256);
        moonbit_decref(_M0L5frontS3198);
        moonbit_decref(_M0L1vS1256);
        joinlet_3896:;
        continue;
      }
      break;
    }
  }
  _M0L5frontS3200 = _M0L4selfS1253->$0;
  moonbit_incref(_M0L5frontS3200);
  #line 28 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3897 = _M0MPC15array5Array3popGsE(_M0L5frontS3200);
  moonbit_decref(_M0L5frontS3200);
  return _result_3897;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque10push__back(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1251,
  moonbit_string_t _M0L5valueS1252
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3193;
  #line 14 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4backS3193 = _M0L4selfS1251->$1;
  moonbit_incref(_M0L4backS3193);
  #line 15 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPC15array5Array4pushGsE(_M0L4backS3193, _M0L5valueS1252);
  moonbit_decref(_M0L4backS3193);
  return 0;
}

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0MP38JIA2JIA29moonbitdb3lib5Deque3new(
  
) {
  moonbit_string_t* _M0L6_2atmpS3192;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3189;
  moonbit_string_t* _M0L6_2atmpS3191;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3190;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _block_3898;
  #line 6 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3192 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3189
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3189)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS3189->$0 = _M0L6_2atmpS3192;
  _M0L6_2atmpS3189->$1 = 0;
  _M0L6_2atmpS3191 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3190
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3190)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS3190->$0 = _M0L6_2atmpS3191;
  _M0L6_2atmpS3190->$1 = 0;
  _block_3898
  = (struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*)moonbit_malloc(sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque));
  Moonbit_object_header(_block_3898)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 29, 0);
  _block_3898->$0 = _M0L6_2atmpS3189;
  _block_3898->$1 = _M0L6_2atmpS3190;
  return _block_3898;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1250) {
  double _M0L6_2atmpS3188;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3188 = (double)_M0L4selfS1250;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3188);
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1248,
  struct _M0TPC16string10StringView _M0L9separatorS1249
) {
  moonbit_string_t* _M0L3bufS3186;
  int32_t _M0L3lenS3187;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3185;
  moonbit_string_t _result_3899;
  #line 2184 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS3186 = _M0L4selfS1248->$0;
  _M0L3lenS3187 = _M0L4selfS1248->$1;
  moonbit_incref(_M0L3bufS3186);
  _M0L6_2atmpS3185
  = (struct _M0TPB9ArrayViewGsE){
    .$0 = _M0L3bufS3186, .$1 = 0, .$2 = _M0L3lenS3187
  };
  #line 2188 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _result_3899
  = _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS3185, _M0L9separatorS1249);
  moonbit_decref(_M0L6_2atmpS3185.$0);
  return _result_3899;
}

moonbit_string_t _M0MPC15array5Array3popGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1245
) {
  int32_t _M0L3lenS1244;
  #line 325 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1244 = _M0L4selfS1245->$1;
  if (_M0L3lenS1244 == 0) {
    return 0;
  } else {
    int32_t _M0L5indexS1246 = _M0L3lenS1244 - 1;
    moonbit_string_t* _M0L3bufS3184 = _M0L4selfS1245->$0;
    moonbit_string_t _M0L1vS1247 =
      (moonbit_string_t)_M0L3bufS3184[_M0L5indexS1246];
    moonbit_string_t* _M0L3bufS3183 = _M0L4selfS1245->$0;
    moonbit_string_t _M0L6_2aoldS3465;
    if (
      _M0L5indexS1246 < 0
      || _M0L5indexS1246 >= Moonbit_array_length(_M0L3bufS3183)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3465 = (moonbit_string_t)_M0L3bufS3183[_M0L5indexS1246];
    moonbit_incref(_M0L1vS1247);
    moonbit_decref(_M0L6_2aoldS3465);
    if (
      _M0L5indexS1246 < 0
      || _M0L5indexS1246 >= Moonbit_array_length(_M0L3bufS3183)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3183[_M0L5indexS1246]
    = (moonbit_string_t)moonbit_string_literal_82.data;
    _M0L4selfS1245->$1 = _M0L5indexS1246;
    return _M0L1vS1247;
  }
}

struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0MPC15array5Array2atGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE* _M0L4selfS1236,
  int32_t _M0L5indexS1237
) {
  int32_t _M0L3lenS1235;
  int32_t _if__result_3900;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1235 = _M0L4selfS1236->$1;
  if (_M0L5indexS1237 >= 0) {
    _if__result_3900 = _M0L5indexS1237 < _M0L3lenS1235;
  } else {
    _if__result_3900 = 0;
  }
  if (_if__result_3900) {
    struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player** _M0L6_2atmpS3180;
    struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS3469;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3180
    = _M0MPC15array5Array6bufferGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(_M0L4selfS1236);
    _M0L6_2atmpS3469
    = (struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player*)_M0L6_2atmpS3180[
        _M0L5indexS1237
      ];
    if (_M0L6_2atmpS3469) {
      moonbit_incref(_M0L6_2atmpS3469);
    }
    moonbit_decref(_M0L6_2atmpS3180);
    return _M0L6_2atmpS3469;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1239,
  int32_t _M0L5indexS1240
) {
  int32_t _M0L3lenS1238;
  int32_t _if__result_3901;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1238 = _M0L4selfS1239->$1;
  if (_M0L5indexS1240 >= 0) {
    _if__result_3901 = _M0L5indexS1240 < _M0L3lenS1238;
  } else {
    _if__result_3901 = 0;
  }
  if (_if__result_3901) {
    moonbit_string_t* _M0L6_2atmpS3181;
    moonbit_string_t _M0L6_2atmpS3470;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3181 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1239);
    _M0L6_2atmpS3470 = (moonbit_string_t)_M0L6_2atmpS3181[_M0L5indexS1240];
    moonbit_incref(_M0L6_2atmpS3470);
    moonbit_decref(_M0L6_2atmpS3181);
    return _M0L6_2atmpS3470;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TUsfE* _M0MPC15array5Array2atGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS1242,
  int32_t _M0L5indexS1243
) {
  int32_t _M0L3lenS1241;
  int32_t _if__result_3902;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1241 = _M0L4selfS1242->$1;
  if (_M0L5indexS1243 >= 0) {
    _if__result_3902 = _M0L5indexS1243 < _M0L3lenS1241;
  } else {
    _if__result_3902 = 0;
  }
  if (_if__result_3902) {
    struct _M0TUsfE** _M0L6_2atmpS3182;
    struct _M0TUsfE* _M0L6_2atmpS3471;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3182 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1242);
    _M0L6_2atmpS3471 = (struct _M0TUsfE*)_M0L6_2atmpS3182[_M0L5indexS1243];
    if (_M0L6_2atmpS3471) {
      moonbit_incref(_M0L6_2atmpS3471);
    }
    moonbit_decref(_M0L6_2atmpS3182);
    return _M0L6_2atmpS3471;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1234) {
  moonbit_string_t _M0L6_2atmpS3179;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3179
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1234);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3179);
  moonbit_decref(_M0L6_2atmpS3179);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1233) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1233);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1220) {
  uint64_t _M0L4bitsS1221;
  uint64_t _M0L6_2atmpS3178;
  uint64_t _M0L6_2atmpS3177;
  int32_t _M0L8ieeeSignS1222;
  uint64_t _M0L12ieeeMantissaS1223;
  uint64_t _M0L6_2atmpS3176;
  uint64_t _M0L6_2atmpS3175;
  int32_t _M0L12ieeeExponentS1224;
  int32_t _if__result_3903;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1225;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1226;
  moonbit_string_t _result_3905;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1220 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_83.data;
  }
  _M0L4bitsS1221 = *(int64_t*)&_M0L3valS1220;
  _M0L6_2atmpS3178 = _M0L4bitsS1221 >> 63;
  _M0L6_2atmpS3177 = _M0L6_2atmpS3178 & 1ull;
  _M0L8ieeeSignS1222 = _M0L6_2atmpS3177 != 0ull;
  _M0L12ieeeMantissaS1223 = _M0L4bitsS1221 & 4503599627370495ull;
  _M0L6_2atmpS3176 = _M0L4bitsS1221 >> 52;
  _M0L6_2atmpS3175 = _M0L6_2atmpS3176 & 2047ull;
  _M0L12ieeeExponentS1224 = (int32_t)_M0L6_2atmpS3175;
  if (_M0L12ieeeExponentS1224 == 2047) {
    _if__result_3903 = 1;
  } else if (_M0L12ieeeExponentS1224 == 0) {
    _if__result_3903 = _M0L12ieeeMantissaS1223 == 0ull;
  } else {
    _if__result_3903 = 0;
  }
  if (_if__result_3903) {
    int32_t _M0L6_2atmpS3166 = _M0L12ieeeExponentS1224 != 0;
    int32_t _M0L6_2atmpS3167 = _M0L12ieeeMantissaS1223 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1222, _M0L6_2atmpS3166, _M0L6_2atmpS3167);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1225
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1223, _M0L12ieeeExponentS1224);
  if (_M0L7_2abindS1225 == 0) {
    uint32_t _M0L6_2atmpS3168;
    if (_M0L7_2abindS1225) {
      moonbit_decref(_M0L7_2abindS1225);
    }
    _M0L6_2atmpS3168 = *(uint32_t*)&_M0L12ieeeExponentS1224;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1226 = _M0FPB3d2d(_M0L12ieeeMantissaS1223, _M0L6_2atmpS3168);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1227 = _M0L7_2abindS1225;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1228 = _M0L7_2aSomeS1227;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1229 = _M0L4_2afS1228;
    while (1) {
      uint64_t _M0L8mantissaS3174 = _M0L1xS1229->$0;
      uint64_t _M0L1qS1230 = _M0L8mantissaS3174 / 10ull;
      uint64_t _M0L8mantissaS3172 = _M0L1xS1229->$0;
      uint64_t _M0L6_2atmpS3173 = 10ull * _M0L1qS1230;
      uint64_t _M0L1rS1231 = _M0L8mantissaS3172 - _M0L6_2atmpS3173;
      int32_t _M0L8exponentS3171;
      int32_t _M0L6_2atmpS3170;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3169;
      if (_M0L1rS1231 != 0ull) {
        _M0L1vS1226 = _M0L1xS1229;
        break;
      }
      _M0L8exponentS3171 = _M0L1xS1229->$1;
      moonbit_decref(_M0L1xS1229);
      _M0L6_2atmpS3170 = _M0L8exponentS3171 + 1;
      _M0L6_2atmpS3169
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3169)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3169->$0 = _M0L1qS1230;
      _M0L6_2atmpS3169->$1 = _M0L6_2atmpS3170;
      _M0L1xS1229 = _M0L6_2atmpS3169;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_3905 = _M0FPB9to__chars(_M0L1vS1226, _M0L8ieeeSignS1222);
  moonbit_decref(_M0L1vS1226);
  return _result_3905;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1215,
  int32_t _M0L12ieeeExponentS1217
) {
  uint64_t _M0L2m2S1214;
  int32_t _M0L6_2atmpS3165;
  int32_t _M0L2e2S1216;
  int32_t _M0L6_2atmpS3164;
  uint64_t _M0L6_2atmpS3163;
  uint64_t _M0L4maskS1218;
  uint64_t _M0L8fractionS1219;
  int32_t _M0L6_2atmpS3162;
  uint64_t _M0L6_2atmpS3161;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3160;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1214 = 4503599627370496ull | _M0L12ieeeMantissaS1215;
  _M0L6_2atmpS3165 = _M0L12ieeeExponentS1217 - 1023;
  _M0L2e2S1216 = _M0L6_2atmpS3165 - 52;
  if (_M0L2e2S1216 > 0) {
    return 0;
  }
  if (_M0L2e2S1216 < -52) {
    return 0;
  }
  _M0L6_2atmpS3164 = -_M0L2e2S1216;
  _M0L6_2atmpS3163 = 1ull << (_M0L6_2atmpS3164 & 63);
  _M0L4maskS1218 = _M0L6_2atmpS3163 - 1ull;
  _M0L8fractionS1219 = _M0L2m2S1214 & _M0L4maskS1218;
  if (_M0L8fractionS1219 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3162 = -_M0L2e2S1216;
  _M0L6_2atmpS3161 = _M0L2m2S1214 >> (_M0L6_2atmpS3162 & 63);
  _M0L6_2atmpS3160
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3160)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3160->$0 = _M0L6_2atmpS3161;
  _M0L6_2atmpS3160->$1 = 0;
  return _M0L6_2atmpS3160;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1182,
  int32_t _M0L4signS1180
) {
  int32_t _M0L6_2atmpS3159;
  moonbit_bytes_t _M0L6resultS1178;
  int32_t _M0Lm5indexS1179;
  uint64_t _M0L6outputS1181;
  int32_t _M0L7olengthS1183;
  int32_t _M0L8exponentS3158;
  int32_t _M0L6_2atmpS3157;
  int32_t _M0Lm3expS1184;
  int32_t _M0L6_2atmpS3156;
  int32_t _M0L6_2atmpS3154;
  int32_t _M0L18scientificNotationS1185;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3159 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1178
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3159);
  _M0Lm5indexS1179 = 0;
  if (_M0L4signS1180) {
    int32_t _M0L6_2atmpS3028 = _M0Lm5indexS1179;
    int32_t _M0L6_2atmpS3029;
    if (
      _M0L6_2atmpS3028 < 0
      || _M0L6_2atmpS3028 >= Moonbit_array_length(_M0L6resultS1178)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1178[_M0L6_2atmpS3028] = 45;
    _M0L6_2atmpS3029 = _M0Lm5indexS1179;
    _M0Lm5indexS1179 = _M0L6_2atmpS3029 + 1;
  }
  _M0L6outputS1181 = _M0L1vS1182->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1183 = _M0FPB17decimal__length17(_M0L6outputS1181);
  _M0L8exponentS3158 = _M0L1vS1182->$1;
  _M0L6_2atmpS3157 = _M0L8exponentS3158 + _M0L7olengthS1183;
  _M0Lm3expS1184 = _M0L6_2atmpS3157 - 1;
  _M0L6_2atmpS3156 = _M0Lm3expS1184;
  if (_M0L6_2atmpS3156 >= -6) {
    int32_t _M0L6_2atmpS3155 = _M0Lm3expS1184;
    _M0L6_2atmpS3154 = _M0L6_2atmpS3155 < 21;
  } else {
    _M0L6_2atmpS3154 = 0;
  }
  _M0L18scientificNotationS1185 = !_M0L6_2atmpS3154;
  if (_M0L18scientificNotationS1185) {
    int32_t _M0L7_2abindS1186 = _M0L7olengthS1183 - 1;
    uint64_t _M0L6outputS1187;
    int32_t _M0L1iS1188 = 0;
    uint64_t _M0L6outputS1189 = _M0L6outputS1181;
    int32_t _M0L6_2atmpS3030;
    int32_t _M0L6_2atmpS3034;
    int32_t _M0L6_2atmpS3033;
    int32_t _M0L6_2atmpS3032;
    int32_t _M0L6_2atmpS3031;
    int32_t _M0L6_2atmpS3038;
    int32_t _M0L6_2atmpS3039;
    int32_t _M0L6_2atmpS3040;
    int32_t _M0L6_2atmpS3041;
    int32_t _M0L6_2atmpS3042;
    int32_t _M0L6_2atmpS3048;
    int32_t _M0L6_2atmpS3081;
    moonbit_string_t _result_3907;
    while (1) {
      if (_M0L1iS1188 < _M0L7_2abindS1186) {
        uint64_t _M0L1cS1190 = _M0L6outputS1189 % 10ull;
        int32_t _M0L6_2atmpS3087 = _M0Lm5indexS1179;
        int32_t _M0L6_2atmpS3086 = _M0L6_2atmpS3087 + _M0L7olengthS1183;
        int32_t _M0L6_2atmpS3082 = _M0L6_2atmpS3086 - _M0L1iS1188;
        int32_t _M0L6_2atmpS3085 = (int32_t)_M0L1cS1190;
        int32_t _M0L6_2atmpS3084 = 48 + _M0L6_2atmpS3085;
        int32_t _M0L6_2atmpS3083 = _M0L6_2atmpS3084 & 0xff;
        int32_t _M0L6_2atmpS3088;
        uint64_t _M0L6_2atmpS3089;
        if (
          _M0L6_2atmpS3082 < 0
          || _M0L6_2atmpS3082 >= Moonbit_array_length(_M0L6resultS1178)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1178[_M0L6_2atmpS3082] = _M0L6_2atmpS3083;
        _M0L6_2atmpS3088 = _M0L1iS1188 + 1;
        _M0L6_2atmpS3089 = _M0L6outputS1189 / 10ull;
        _M0L1iS1188 = _M0L6_2atmpS3088;
        _M0L6outputS1189 = _M0L6_2atmpS3089;
        continue;
      } else {
        _M0L6outputS1187 = _M0L6outputS1189;
      }
      break;
    }
    _M0L6_2atmpS3030 = _M0Lm5indexS1179;
    _M0L6_2atmpS3034 = (int32_t)_M0L6outputS1187;
    _M0L6_2atmpS3033 = _M0L6_2atmpS3034 % 10;
    _M0L6_2atmpS3032 = 48 + _M0L6_2atmpS3033;
    _M0L6_2atmpS3031 = _M0L6_2atmpS3032 & 0xff;
    if (
      _M0L6_2atmpS3030 < 0
      || _M0L6_2atmpS3030 >= Moonbit_array_length(_M0L6resultS1178)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1178[_M0L6_2atmpS3030] = _M0L6_2atmpS3031;
    if (_M0L7olengthS1183 > 1) {
      int32_t _M0L6_2atmpS3036 = _M0Lm5indexS1179;
      int32_t _M0L6_2atmpS3035 = _M0L6_2atmpS3036 + 1;
      if (
        _M0L6_2atmpS3035 < 0
        || _M0L6_2atmpS3035 >= Moonbit_array_length(_M0L6resultS1178)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1178[_M0L6_2atmpS3035] = 46;
    } else {
      int32_t _M0L6_2atmpS3037 = _M0Lm5indexS1179;
      _M0Lm5indexS1179 = _M0L6_2atmpS3037 - 1;
    }
    _M0L6_2atmpS3038 = _M0Lm5indexS1179;
    _M0L6_2atmpS3039 = _M0L7olengthS1183 + 1;
    _M0Lm5indexS1179 = _M0L6_2atmpS3038 + _M0L6_2atmpS3039;
    _M0L6_2atmpS3040 = _M0Lm5indexS1179;
    if (
      _M0L6_2atmpS3040 < 0
      || _M0L6_2atmpS3040 >= Moonbit_array_length(_M0L6resultS1178)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1178[_M0L6_2atmpS3040] = 101;
    _M0L6_2atmpS3041 = _M0Lm5indexS1179;
    _M0Lm5indexS1179 = _M0L6_2atmpS3041 + 1;
    _M0L6_2atmpS3042 = _M0Lm3expS1184;
    if (_M0L6_2atmpS3042 < 0) {
      int32_t _M0L6_2atmpS3043 = _M0Lm5indexS1179;
      int32_t _M0L6_2atmpS3044;
      int32_t _M0L6_2atmpS3045;
      if (
        _M0L6_2atmpS3043 < 0
        || _M0L6_2atmpS3043 >= Moonbit_array_length(_M0L6resultS1178)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1178[_M0L6_2atmpS3043] = 45;
      _M0L6_2atmpS3044 = _M0Lm5indexS1179;
      _M0Lm5indexS1179 = _M0L6_2atmpS3044 + 1;
      _M0L6_2atmpS3045 = _M0Lm3expS1184;
      _M0Lm3expS1184 = -_M0L6_2atmpS3045;
    } else {
      int32_t _M0L6_2atmpS3046 = _M0Lm5indexS1179;
      int32_t _M0L6_2atmpS3047;
      if (
        _M0L6_2atmpS3046 < 0
        || _M0L6_2atmpS3046 >= Moonbit_array_length(_M0L6resultS1178)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1178[_M0L6_2atmpS3046] = 43;
      _M0L6_2atmpS3047 = _M0Lm5indexS1179;
      _M0Lm5indexS1179 = _M0L6_2atmpS3047 + 1;
    }
    _M0L6_2atmpS3048 = _M0Lm3expS1184;
    if (_M0L6_2atmpS3048 >= 100) {
      int32_t _M0L6_2atmpS3064 = _M0Lm3expS1184;
      int32_t _M0L1aS1192 = _M0L6_2atmpS3064 / 100;
      int32_t _M0L6_2atmpS3063 = _M0Lm3expS1184;
      int32_t _M0L6_2atmpS3062 = _M0L6_2atmpS3063 / 10;
      int32_t _M0L1bS1193 = _M0L6_2atmpS3062 % 10;
      int32_t _M0L6_2atmpS3061 = _M0Lm3expS1184;
      int32_t _M0L1cS1194 = _M0L6_2atmpS3061 % 10;
      int32_t _M0L6_2atmpS3049 = _M0Lm5indexS1179;
      int32_t _M0L6_2atmpS3051 = 48 + _M0L1aS1192;
      int32_t _M0L6_2atmpS3050 = _M0L6_2atmpS3051 & 0xff;
      int32_t _M0L6_2atmpS3055;
      int32_t _M0L6_2atmpS3052;
      int32_t _M0L6_2atmpS3054;
      int32_t _M0L6_2atmpS3053;
      int32_t _M0L6_2atmpS3059;
      int32_t _M0L6_2atmpS3056;
      int32_t _M0L6_2atmpS3058;
      int32_t _M0L6_2atmpS3057;
      int32_t _M0L6_2atmpS3060;
      if (
        _M0L6_2atmpS3049 < 0
        || _M0L6_2atmpS3049 >= Moonbit_array_length(_M0L6resultS1178)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1178[_M0L6_2atmpS3049] = _M0L6_2atmpS3050;
      _M0L6_2atmpS3055 = _M0Lm5indexS1179;
      _M0L6_2atmpS3052 = _M0L6_2atmpS3055 + 1;
      _M0L6_2atmpS3054 = 48 + _M0L1bS1193;
      _M0L6_2atmpS3053 = _M0L6_2atmpS3054 & 0xff;
      if (
        _M0L6_2atmpS3052 < 0
        || _M0L6_2atmpS3052 >= Moonbit_array_length(_M0L6resultS1178)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1178[_M0L6_2atmpS3052] = _M0L6_2atmpS3053;
      _M0L6_2atmpS3059 = _M0Lm5indexS1179;
      _M0L6_2atmpS3056 = _M0L6_2atmpS3059 + 2;
      _M0L6_2atmpS3058 = 48 + _M0L1cS1194;
      _M0L6_2atmpS3057 = _M0L6_2atmpS3058 & 0xff;
      if (
        _M0L6_2atmpS3056 < 0
        || _M0L6_2atmpS3056 >= Moonbit_array_length(_M0L6resultS1178)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1178[_M0L6_2atmpS3056] = _M0L6_2atmpS3057;
      _M0L6_2atmpS3060 = _M0Lm5indexS1179;
      _M0Lm5indexS1179 = _M0L6_2atmpS3060 + 3;
    } else {
      int32_t _M0L6_2atmpS3065 = _M0Lm3expS1184;
      if (_M0L6_2atmpS3065 >= 10) {
        int32_t _M0L6_2atmpS3075 = _M0Lm3expS1184;
        int32_t _M0L1aS1195 = _M0L6_2atmpS3075 / 10;
        int32_t _M0L6_2atmpS3074 = _M0Lm3expS1184;
        int32_t _M0L1bS1196 = _M0L6_2atmpS3074 % 10;
        int32_t _M0L6_2atmpS3066 = _M0Lm5indexS1179;
        int32_t _M0L6_2atmpS3068 = 48 + _M0L1aS1195;
        int32_t _M0L6_2atmpS3067 = _M0L6_2atmpS3068 & 0xff;
        int32_t _M0L6_2atmpS3072;
        int32_t _M0L6_2atmpS3069;
        int32_t _M0L6_2atmpS3071;
        int32_t _M0L6_2atmpS3070;
        int32_t _M0L6_2atmpS3073;
        if (
          _M0L6_2atmpS3066 < 0
          || _M0L6_2atmpS3066 >= Moonbit_array_length(_M0L6resultS1178)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1178[_M0L6_2atmpS3066] = _M0L6_2atmpS3067;
        _M0L6_2atmpS3072 = _M0Lm5indexS1179;
        _M0L6_2atmpS3069 = _M0L6_2atmpS3072 + 1;
        _M0L6_2atmpS3071 = 48 + _M0L1bS1196;
        _M0L6_2atmpS3070 = _M0L6_2atmpS3071 & 0xff;
        if (
          _M0L6_2atmpS3069 < 0
          || _M0L6_2atmpS3069 >= Moonbit_array_length(_M0L6resultS1178)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1178[_M0L6_2atmpS3069] = _M0L6_2atmpS3070;
        _M0L6_2atmpS3073 = _M0Lm5indexS1179;
        _M0Lm5indexS1179 = _M0L6_2atmpS3073 + 2;
      } else {
        int32_t _M0L6_2atmpS3076 = _M0Lm5indexS1179;
        int32_t _M0L6_2atmpS3079 = _M0Lm3expS1184;
        int32_t _M0L6_2atmpS3078 = 48 + _M0L6_2atmpS3079;
        int32_t _M0L6_2atmpS3077 = _M0L6_2atmpS3078 & 0xff;
        int32_t _M0L6_2atmpS3080;
        if (
          _M0L6_2atmpS3076 < 0
          || _M0L6_2atmpS3076 >= Moonbit_array_length(_M0L6resultS1178)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1178[_M0L6_2atmpS3076] = _M0L6_2atmpS3077;
        _M0L6_2atmpS3080 = _M0Lm5indexS1179;
        _M0Lm5indexS1179 = _M0L6_2atmpS3080 + 1;
      }
    }
    _M0L6_2atmpS3081 = _M0Lm5indexS1179;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_3907
    = _M0FPB19string__from__bytes(_M0L6resultS1178, 0, _M0L6_2atmpS3081);
    moonbit_decref(_M0L6resultS1178);
    return _result_3907;
  } else {
    int32_t _M0L6_2atmpS3090 = _M0Lm3expS1184;
    int32_t _M0L6_2atmpS3153;
    moonbit_string_t _result_3913;
    if (_M0L6_2atmpS3090 < 0) {
      int32_t _M0L6_2atmpS3091 = _M0Lm5indexS1179;
      int32_t _M0L6_2atmpS3093;
      int32_t _M0L6_2atmpS3092;
      int32_t _M0L6_2atmpS3094;
      int32_t _M0L1iS1197;
      int32_t _M0L6_2atmpS3109;
      int32_t _M0L6_2atmpS3111;
      int32_t _M0L6_2atmpS3110;
      int32_t _M0L7currentS1199;
      int32_t _M0L1iS1200;
      uint64_t _M0L6outputS1201;
      if (
        _M0L6_2atmpS3091 < 0
        || _M0L6_2atmpS3091 >= Moonbit_array_length(_M0L6resultS1178)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1178[_M0L6_2atmpS3091] = 48;
      _M0L6_2atmpS3093 = _M0Lm5indexS1179;
      _M0L6_2atmpS3092 = _M0L6_2atmpS3093 + 1;
      if (
        _M0L6_2atmpS3092 < 0
        || _M0L6_2atmpS3092 >= Moonbit_array_length(_M0L6resultS1178)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1178[_M0L6_2atmpS3092] = 46;
      _M0L6_2atmpS3094 = _M0Lm5indexS1179;
      _M0Lm5indexS1179 = _M0L6_2atmpS3094 + 2;
      _M0L1iS1197 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3095 = _M0Lm3expS1184;
        if (_M0L1iS1197 > _M0L6_2atmpS3095) {
          int32_t _M0L6_2atmpS3098 = _M0Lm5indexS1179;
          int32_t _M0L6_2atmpS3097 = _M0L6_2atmpS3098 - _M0L1iS1197;
          int32_t _M0L6_2atmpS3096 = _M0L6_2atmpS3097 - 1;
          int32_t _M0L6_2atmpS3099;
          if (
            _M0L6_2atmpS3096 < 0
            || _M0L6_2atmpS3096 >= Moonbit_array_length(_M0L6resultS1178)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1178[_M0L6_2atmpS3096] = 48;
          _M0L6_2atmpS3099 = _M0L1iS1197 - 1;
          _M0L1iS1197 = _M0L6_2atmpS3099;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3109 = _M0Lm5indexS1179;
      _M0L6_2atmpS3111 = _M0Lm3expS1184;
      _M0L6_2atmpS3110 = -1 - _M0L6_2atmpS3111;
      _M0L7currentS1199 = _M0L6_2atmpS3109 + _M0L6_2atmpS3110;
      _M0L1iS1200 = 0;
      _M0L6outputS1201 = _M0L6outputS1181;
      while (1) {
        if (_M0L1iS1200 < _M0L7olengthS1183) {
          int32_t _M0L6_2atmpS3106 = _M0L7currentS1199 + _M0L7olengthS1183;
          int32_t _M0L6_2atmpS3105 = _M0L6_2atmpS3106 - _M0L1iS1200;
          int32_t _M0L6_2atmpS3100 = _M0L6_2atmpS3105 - 1;
          uint64_t _M0L6_2atmpS3104 = _M0L6outputS1201 % 10ull;
          int32_t _M0L6_2atmpS3103 = (int32_t)_M0L6_2atmpS3104;
          int32_t _M0L6_2atmpS3102 = 48 + _M0L6_2atmpS3103;
          int32_t _M0L6_2atmpS3101 = _M0L6_2atmpS3102 & 0xff;
          int32_t _M0L6_2atmpS3107;
          uint64_t _M0L6_2atmpS3108;
          if (
            _M0L6_2atmpS3100 < 0
            || _M0L6_2atmpS3100 >= Moonbit_array_length(_M0L6resultS1178)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1178[_M0L6_2atmpS3100] = _M0L6_2atmpS3101;
          _M0L6_2atmpS3107 = _M0L1iS1200 + 1;
          _M0L6_2atmpS3108 = _M0L6outputS1201 / 10ull;
          _M0L1iS1200 = _M0L6_2atmpS3107;
          _M0L6outputS1201 = _M0L6_2atmpS3108;
          continue;
        }
        break;
      }
      _M0Lm5indexS1179 = _M0L7currentS1199 + _M0L7olengthS1183;
    } else {
      int32_t _M0L6_2atmpS3113 = _M0Lm3expS1184;
      int32_t _M0L6_2atmpS3112 = _M0L6_2atmpS3113 + 1;
      if (_M0L6_2atmpS3112 >= _M0L7olengthS1183) {
        int32_t _M0L1iS1203 = 0;
        uint64_t _M0L6outputS1204 = _M0L6outputS1181;
        int32_t _M0L6_2atmpS3124;
        int32_t _M0L6_2atmpS3129;
        int32_t _M0L7_2abindS1206;
        int32_t _M0L1iS1207;
        int32_t _M0L6_2atmpS3130;
        int32_t _M0L6_2atmpS3133;
        int32_t _M0L6_2atmpS3132;
        int32_t _M0L6_2atmpS3131;
        while (1) {
          if (_M0L1iS1203 < _M0L7olengthS1183) {
            int32_t _M0L6_2atmpS3121 = _M0Lm5indexS1179;
            int32_t _M0L6_2atmpS3120 = _M0L6_2atmpS3121 + _M0L7olengthS1183;
            int32_t _M0L6_2atmpS3119 = _M0L6_2atmpS3120 - _M0L1iS1203;
            int32_t _M0L6_2atmpS3114 = _M0L6_2atmpS3119 - 1;
            uint64_t _M0L6_2atmpS3118 = _M0L6outputS1204 % 10ull;
            int32_t _M0L6_2atmpS3117 = (int32_t)_M0L6_2atmpS3118;
            int32_t _M0L6_2atmpS3116 = 48 + _M0L6_2atmpS3117;
            int32_t _M0L6_2atmpS3115 = _M0L6_2atmpS3116 & 0xff;
            int32_t _M0L6_2atmpS3122;
            uint64_t _M0L6_2atmpS3123;
            if (
              _M0L6_2atmpS3114 < 0
              || _M0L6_2atmpS3114 >= Moonbit_array_length(_M0L6resultS1178)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1178[_M0L6_2atmpS3114] = _M0L6_2atmpS3115;
            _M0L6_2atmpS3122 = _M0L1iS1203 + 1;
            _M0L6_2atmpS3123 = _M0L6outputS1204 / 10ull;
            _M0L1iS1203 = _M0L6_2atmpS3122;
            _M0L6outputS1204 = _M0L6_2atmpS3123;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3124 = _M0Lm5indexS1179;
        _M0Lm5indexS1179 = _M0L6_2atmpS3124 + _M0L7olengthS1183;
        _M0L6_2atmpS3129 = _M0Lm3expS1184;
        _M0L7_2abindS1206 = _M0L6_2atmpS3129 + 1;
        _M0L1iS1207 = _M0L7olengthS1183;
        while (1) {
          if (_M0L1iS1207 < _M0L7_2abindS1206) {
            int32_t _M0L6_2atmpS3127 = _M0Lm5indexS1179;
            int32_t _M0L6_2atmpS3126 = _M0L6_2atmpS3127 + _M0L1iS1207;
            int32_t _M0L6_2atmpS3125 = _M0L6_2atmpS3126 - _M0L7olengthS1183;
            int32_t _M0L6_2atmpS3128;
            if (
              _M0L6_2atmpS3125 < 0
              || _M0L6_2atmpS3125 >= Moonbit_array_length(_M0L6resultS1178)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1178[_M0L6_2atmpS3125] = 48;
            _M0L6_2atmpS3128 = _M0L1iS1207 + 1;
            _M0L1iS1207 = _M0L6_2atmpS3128;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3130 = _M0Lm5indexS1179;
        _M0L6_2atmpS3133 = _M0Lm3expS1184;
        _M0L6_2atmpS3132 = _M0L6_2atmpS3133 + 1;
        _M0L6_2atmpS3131 = _M0L6_2atmpS3132 - _M0L7olengthS1183;
        _M0Lm5indexS1179 = _M0L6_2atmpS3130 + _M0L6_2atmpS3131;
      } else {
        int32_t _M0L6_2atmpS3150 = _M0Lm5indexS1179;
        int32_t _M0L6_2atmpS3149 = _M0L6_2atmpS3150 + 1;
        int32_t _M0L1iS1209 = 0;
        int32_t _M0L7currentS1210 = _M0L6_2atmpS3149;
        uint64_t _M0L6outputS1211 = _M0L6outputS1181;
        int32_t _M0L6_2atmpS3151;
        int32_t _M0L6_2atmpS3152;
        while (1) {
          if (_M0L1iS1209 < _M0L7olengthS1183) {
            int32_t _M0L6_2atmpS3145 = _M0L7olengthS1183 - _M0L1iS1209;
            int32_t _M0L6_2atmpS3143 = _M0L6_2atmpS3145 - 1;
            int32_t _M0L6_2atmpS3144 = _M0Lm3expS1184;
            int32_t _M0L7currentS1212;
            int32_t _M0L6_2atmpS3140;
            int32_t _M0L6_2atmpS3139;
            int32_t _M0L6_2atmpS3134;
            uint64_t _M0L6_2atmpS3138;
            int32_t _M0L6_2atmpS3137;
            int32_t _M0L6_2atmpS3136;
            int32_t _M0L6_2atmpS3135;
            int32_t _M0L6_2atmpS3141;
            uint64_t _M0L6_2atmpS3142;
            if (_M0L6_2atmpS3143 == _M0L6_2atmpS3144) {
              int32_t _M0L6_2atmpS3148 =
                _M0L7currentS1210 + _M0L7olengthS1183;
              int32_t _M0L6_2atmpS3147 = _M0L6_2atmpS3148 - _M0L1iS1209;
              int32_t _M0L6_2atmpS3146 = _M0L6_2atmpS3147 - 1;
              if (
                _M0L6_2atmpS3146 < 0
                || _M0L6_2atmpS3146 >= Moonbit_array_length(_M0L6resultS1178)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1178[_M0L6_2atmpS3146] = 46;
              _M0L7currentS1212 = _M0L7currentS1210 - 1;
            } else {
              _M0L7currentS1212 = _M0L7currentS1210;
            }
            _M0L6_2atmpS3140 = _M0L7currentS1212 + _M0L7olengthS1183;
            _M0L6_2atmpS3139 = _M0L6_2atmpS3140 - _M0L1iS1209;
            _M0L6_2atmpS3134 = _M0L6_2atmpS3139 - 1;
            _M0L6_2atmpS3138 = _M0L6outputS1211 % 10ull;
            _M0L6_2atmpS3137 = (int32_t)_M0L6_2atmpS3138;
            _M0L6_2atmpS3136 = 48 + _M0L6_2atmpS3137;
            _M0L6_2atmpS3135 = _M0L6_2atmpS3136 & 0xff;
            if (
              _M0L6_2atmpS3134 < 0
              || _M0L6_2atmpS3134 >= Moonbit_array_length(_M0L6resultS1178)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1178[_M0L6_2atmpS3134] = _M0L6_2atmpS3135;
            _M0L6_2atmpS3141 = _M0L1iS1209 + 1;
            _M0L6_2atmpS3142 = _M0L6outputS1211 / 10ull;
            _M0L1iS1209 = _M0L6_2atmpS3141;
            _M0L7currentS1210 = _M0L7currentS1212;
            _M0L6outputS1211 = _M0L6_2atmpS3142;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3151 = _M0Lm5indexS1179;
        _M0L6_2atmpS3152 = _M0L7olengthS1183 + 1;
        _M0Lm5indexS1179 = _M0L6_2atmpS3151 + _M0L6_2atmpS3152;
      }
    }
    _M0L6_2atmpS3153 = _M0Lm5indexS1179;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_3913
    = _M0FPB19string__from__bytes(_M0L6resultS1178, 0, _M0L6_2atmpS3153);
    moonbit_decref(_M0L6resultS1178);
    return _result_3913;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1124,
  uint32_t _M0L12ieeeExponentS1123
) {
  int32_t _M0Lm2e2S1121;
  uint64_t _M0Lm2m2S1122;
  uint64_t _M0L6_2atmpS3027;
  uint64_t _M0L6_2atmpS3026;
  int32_t _M0L4evenS1125;
  uint64_t _M0L6_2atmpS3025;
  uint64_t _M0L2mvS1126;
  int32_t _M0L7mmShiftS1127;
  uint64_t _M0Lm2vrS1128;
  uint64_t _M0Lm2vpS1129;
  uint64_t _M0Lm2vmS1130;
  int32_t _M0Lm3e10S1131;
  int32_t _M0Lm17vmIsTrailingZerosS1132;
  int32_t _M0Lm17vrIsTrailingZerosS1133;
  int32_t _M0L6_2atmpS2927;
  int32_t _M0Lm7removedS1152;
  int32_t _M0Lm16lastRemovedDigitS1153;
  uint64_t _M0Lm6outputS1154;
  int32_t _M0L6_2atmpS3023;
  int32_t _M0L6_2atmpS3024;
  int32_t _M0L3expS1177;
  uint64_t _M0L6_2atmpS3022;
  struct _M0TPB17FloatingDecimal64* _block_3919;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1121 = 0;
  _M0Lm2m2S1122 = 0ull;
  if (_M0L12ieeeExponentS1123 == 0u) {
    _M0Lm2e2S1121 = -1076;
    _M0Lm2m2S1122 = _M0L12ieeeMantissaS1124;
  } else {
    int32_t _M0L6_2atmpS2926 = *(int32_t*)&_M0L12ieeeExponentS1123;
    int32_t _M0L6_2atmpS2925 = _M0L6_2atmpS2926 - 1023;
    int32_t _M0L6_2atmpS2924 = _M0L6_2atmpS2925 - 52;
    _M0Lm2e2S1121 = _M0L6_2atmpS2924 - 2;
    _M0Lm2m2S1122 = 4503599627370496ull | _M0L12ieeeMantissaS1124;
  }
  _M0L6_2atmpS3027 = _M0Lm2m2S1122;
  _M0L6_2atmpS3026 = _M0L6_2atmpS3027 & 1ull;
  _M0L4evenS1125 = _M0L6_2atmpS3026 == 0ull;
  _M0L6_2atmpS3025 = _M0Lm2m2S1122;
  _M0L2mvS1126 = 4ull * _M0L6_2atmpS3025;
  if (_M0L12ieeeMantissaS1124 != 0ull) {
    _M0L7mmShiftS1127 = 1;
  } else {
    _M0L7mmShiftS1127 = _M0L12ieeeExponentS1123 <= 1u;
  }
  _M0Lm2vrS1128 = 0ull;
  _M0Lm2vpS1129 = 0ull;
  _M0Lm2vmS1130 = 0ull;
  _M0Lm3e10S1131 = 0;
  _M0Lm17vmIsTrailingZerosS1132 = 0;
  _M0Lm17vrIsTrailingZerosS1133 = 0;
  _M0L6_2atmpS2927 = _M0Lm2e2S1121;
  if (_M0L6_2atmpS2927 >= 0) {
    int32_t _M0L6_2atmpS2949 = _M0Lm2e2S1121;
    int32_t _M0L6_2atmpS2945;
    int32_t _M0L6_2atmpS2948;
    int32_t _M0L6_2atmpS2947;
    int32_t _M0L6_2atmpS2946;
    int32_t _M0L1qS1134;
    int32_t _M0L6_2atmpS2944;
    int32_t _M0L6_2atmpS2943;
    int32_t _M0L1kS1135;
    int32_t _M0L6_2atmpS2942;
    int32_t _M0L6_2atmpS2941;
    int32_t _M0L6_2atmpS2940;
    int32_t _M0L1iS1136;
    struct _M0TPB8Pow5Pair _M0L4pow5S1137;
    uint64_t _M0L6_2atmpS2939;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1138;
    uint64_t _M0L8_2avrOutS1139;
    uint64_t _M0L8_2avpOutS1140;
    uint64_t _M0L8_2avmOutS1141;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2945 = _M0FPB9log10Pow2(_M0L6_2atmpS2949);
    _M0L6_2atmpS2948 = _M0Lm2e2S1121;
    _M0L6_2atmpS2947 = _M0L6_2atmpS2948 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2946 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2947);
    _M0L1qS1134 = _M0L6_2atmpS2945 - _M0L6_2atmpS2946;
    _M0Lm3e10S1131 = _M0L1qS1134;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2944 = _M0FPB8pow5bits(_M0L1qS1134);
    _M0L6_2atmpS2943 = 125 + _M0L6_2atmpS2944;
    _M0L1kS1135 = _M0L6_2atmpS2943 - 1;
    _M0L6_2atmpS2942 = _M0Lm2e2S1121;
    _M0L6_2atmpS2941 = -_M0L6_2atmpS2942;
    _M0L6_2atmpS2940 = _M0L6_2atmpS2941 + _M0L1qS1134;
    _M0L1iS1136 = _M0L6_2atmpS2940 + _M0L1kS1135;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1137 = _M0FPB22double__computeInvPow5(_M0L1qS1134);
    _M0L6_2atmpS2939 = _M0Lm2m2S1122;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1138
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2939, _M0L4pow5S1137, _M0L1iS1136, _M0L7mmShiftS1127);
    _M0L8_2avrOutS1139 = _M0L7_2abindS1138.$0;
    _M0L8_2avpOutS1140 = _M0L7_2abindS1138.$1;
    _M0L8_2avmOutS1141 = _M0L7_2abindS1138.$2;
    _M0Lm2vrS1128 = _M0L8_2avrOutS1139;
    _M0Lm2vpS1129 = _M0L8_2avpOutS1140;
    _M0Lm2vmS1130 = _M0L8_2avmOutS1141;
    if (_M0L1qS1134 <= 21) {
      int32_t _M0L6_2atmpS2935 = (int32_t)_M0L2mvS1126;
      uint64_t _M0L6_2atmpS2938 = _M0L2mvS1126 / 5ull;
      int32_t _M0L6_2atmpS2937 = (int32_t)_M0L6_2atmpS2938;
      int32_t _M0L6_2atmpS2936 = 5 * _M0L6_2atmpS2937;
      int32_t _M0L6mvMod5S1142 = _M0L6_2atmpS2935 - _M0L6_2atmpS2936;
      if (_M0L6mvMod5S1142 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1133
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1126, _M0L1qS1134);
      } else if (_M0L4evenS1125) {
        uint64_t _M0L6_2atmpS2929 = _M0L2mvS1126 - 1ull;
        uint64_t _M0L6_2atmpS2930;
        uint64_t _M0L6_2atmpS2928;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2930 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1127);
        _M0L6_2atmpS2928 = _M0L6_2atmpS2929 - _M0L6_2atmpS2930;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1132
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2928, _M0L1qS1134);
      } else {
        uint64_t _M0L6_2atmpS2931 = _M0Lm2vpS1129;
        uint64_t _M0L6_2atmpS2934 = _M0L2mvS1126 + 2ull;
        int32_t _M0L6_2atmpS2933;
        uint64_t _M0L6_2atmpS2932;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2933
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2934, _M0L1qS1134);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2932 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2933);
        _M0Lm2vpS1129 = _M0L6_2atmpS2931 - _M0L6_2atmpS2932;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2963 = _M0Lm2e2S1121;
    int32_t _M0L6_2atmpS2962 = -_M0L6_2atmpS2963;
    int32_t _M0L6_2atmpS2957;
    int32_t _M0L6_2atmpS2961;
    int32_t _M0L6_2atmpS2960;
    int32_t _M0L6_2atmpS2959;
    int32_t _M0L6_2atmpS2958;
    int32_t _M0L1qS1143;
    int32_t _M0L6_2atmpS2950;
    int32_t _M0L6_2atmpS2956;
    int32_t _M0L6_2atmpS2955;
    int32_t _M0L1iS1144;
    int32_t _M0L6_2atmpS2954;
    int32_t _M0L1kS1145;
    int32_t _M0L1jS1146;
    struct _M0TPB8Pow5Pair _M0L4pow5S1147;
    uint64_t _M0L6_2atmpS2953;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1148;
    uint64_t _M0L8_2avrOutS1149;
    uint64_t _M0L8_2avpOutS1150;
    uint64_t _M0L8_2avmOutS1151;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2957 = _M0FPB9log10Pow5(_M0L6_2atmpS2962);
    _M0L6_2atmpS2961 = _M0Lm2e2S1121;
    _M0L6_2atmpS2960 = -_M0L6_2atmpS2961;
    _M0L6_2atmpS2959 = _M0L6_2atmpS2960 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2958 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2959);
    _M0L1qS1143 = _M0L6_2atmpS2957 - _M0L6_2atmpS2958;
    _M0L6_2atmpS2950 = _M0Lm2e2S1121;
    _M0Lm3e10S1131 = _M0L1qS1143 + _M0L6_2atmpS2950;
    _M0L6_2atmpS2956 = _M0Lm2e2S1121;
    _M0L6_2atmpS2955 = -_M0L6_2atmpS2956;
    _M0L1iS1144 = _M0L6_2atmpS2955 - _M0L1qS1143;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2954 = _M0FPB8pow5bits(_M0L1iS1144);
    _M0L1kS1145 = _M0L6_2atmpS2954 - 125;
    _M0L1jS1146 = _M0L1qS1143 - _M0L1kS1145;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1147 = _M0FPB19double__computePow5(_M0L1iS1144);
    _M0L6_2atmpS2953 = _M0Lm2m2S1122;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1148
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2953, _M0L4pow5S1147, _M0L1jS1146, _M0L7mmShiftS1127);
    _M0L8_2avrOutS1149 = _M0L7_2abindS1148.$0;
    _M0L8_2avpOutS1150 = _M0L7_2abindS1148.$1;
    _M0L8_2avmOutS1151 = _M0L7_2abindS1148.$2;
    _M0Lm2vrS1128 = _M0L8_2avrOutS1149;
    _M0Lm2vpS1129 = _M0L8_2avpOutS1150;
    _M0Lm2vmS1130 = _M0L8_2avmOutS1151;
    if (_M0L1qS1143 <= 1) {
      _M0Lm17vrIsTrailingZerosS1133 = 1;
      if (_M0L4evenS1125) {
        int32_t _M0L6_2atmpS2951;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2951 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1127);
        _M0Lm17vmIsTrailingZerosS1132 = _M0L6_2atmpS2951 == 1;
      } else {
        uint64_t _M0L6_2atmpS2952 = _M0Lm2vpS1129;
        _M0Lm2vpS1129 = _M0L6_2atmpS2952 - 1ull;
      }
    } else if (_M0L1qS1143 < 63) {
      #line 437 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1133
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1126, _M0L1qS1143);
    }
  }
  _M0Lm7removedS1152 = 0;
  _M0Lm16lastRemovedDigitS1153 = 0;
  _M0Lm6outputS1154 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1132 || _M0Lm17vrIsTrailingZerosS1133) {
    int32_t _if__result_3916;
    uint64_t _M0L6_2atmpS2993;
    uint64_t _M0L6_2atmpS2999;
    uint64_t _M0L6_2atmpS3000;
    int32_t _if__result_3917;
    int32_t _M0L6_2atmpS2996;
    int64_t _M0L6_2atmpS2995;
    uint64_t _M0L6_2atmpS2994;
    while (1) {
      uint64_t _M0L6_2atmpS2976 = _M0Lm2vpS1129;
      uint64_t _M0L7vpDiv10S1155 = _M0L6_2atmpS2976 / 10ull;
      uint64_t _M0L6_2atmpS2975 = _M0Lm2vmS1130;
      uint64_t _M0L7vmDiv10S1156 = _M0L6_2atmpS2975 / 10ull;
      uint64_t _M0L6_2atmpS2974;
      int32_t _M0L6_2atmpS2971;
      int32_t _M0L6_2atmpS2973;
      int32_t _M0L6_2atmpS2972;
      int32_t _M0L7vmMod10S1158;
      uint64_t _M0L6_2atmpS2970;
      uint64_t _M0L7vrDiv10S1159;
      uint64_t _M0L6_2atmpS2969;
      int32_t _M0L6_2atmpS2966;
      int32_t _M0L6_2atmpS2968;
      int32_t _M0L6_2atmpS2967;
      int32_t _M0L7vrMod10S1160;
      int32_t _M0L6_2atmpS2965;
      if (_M0L7vpDiv10S1155 <= _M0L7vmDiv10S1156) {
        break;
      }
      _M0L6_2atmpS2974 = _M0Lm2vmS1130;
      _M0L6_2atmpS2971 = (int32_t)_M0L6_2atmpS2974;
      _M0L6_2atmpS2973 = (int32_t)_M0L7vmDiv10S1156;
      _M0L6_2atmpS2972 = 10 * _M0L6_2atmpS2973;
      _M0L7vmMod10S1158 = _M0L6_2atmpS2971 - _M0L6_2atmpS2972;
      _M0L6_2atmpS2970 = _M0Lm2vrS1128;
      _M0L7vrDiv10S1159 = _M0L6_2atmpS2970 / 10ull;
      _M0L6_2atmpS2969 = _M0Lm2vrS1128;
      _M0L6_2atmpS2966 = (int32_t)_M0L6_2atmpS2969;
      _M0L6_2atmpS2968 = (int32_t)_M0L7vrDiv10S1159;
      _M0L6_2atmpS2967 = 10 * _M0L6_2atmpS2968;
      _M0L7vrMod10S1160 = _M0L6_2atmpS2966 - _M0L6_2atmpS2967;
      if (_M0Lm17vmIsTrailingZerosS1132) {
        _M0Lm17vmIsTrailingZerosS1132 = _M0L7vmMod10S1158 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1132 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1133) {
        int32_t _M0L6_2atmpS2964 = _M0Lm16lastRemovedDigitS1153;
        _M0Lm17vrIsTrailingZerosS1133 = _M0L6_2atmpS2964 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1133 = 0;
      }
      _M0Lm16lastRemovedDigitS1153 = _M0L7vrMod10S1160;
      _M0Lm2vrS1128 = _M0L7vrDiv10S1159;
      _M0Lm2vpS1129 = _M0L7vpDiv10S1155;
      _M0Lm2vmS1130 = _M0L7vmDiv10S1156;
      _M0L6_2atmpS2965 = _M0Lm7removedS1152;
      _M0Lm7removedS1152 = _M0L6_2atmpS2965 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1132) {
      while (1) {
        uint64_t _M0L6_2atmpS2989 = _M0Lm2vmS1130;
        uint64_t _M0L7vmDiv10S1161 = _M0L6_2atmpS2989 / 10ull;
        uint64_t _M0L6_2atmpS2988 = _M0Lm2vmS1130;
        int32_t _M0L6_2atmpS2985 = (int32_t)_M0L6_2atmpS2988;
        int32_t _M0L6_2atmpS2987 = (int32_t)_M0L7vmDiv10S1161;
        int32_t _M0L6_2atmpS2986 = 10 * _M0L6_2atmpS2987;
        int32_t _M0L7vmMod10S1162 = _M0L6_2atmpS2985 - _M0L6_2atmpS2986;
        uint64_t _M0L6_2atmpS2984;
        uint64_t _M0L7vpDiv10S1164;
        uint64_t _M0L6_2atmpS2983;
        uint64_t _M0L7vrDiv10S1165;
        uint64_t _M0L6_2atmpS2982;
        int32_t _M0L6_2atmpS2979;
        int32_t _M0L6_2atmpS2981;
        int32_t _M0L6_2atmpS2980;
        int32_t _M0L7vrMod10S1166;
        int32_t _M0L6_2atmpS2978;
        if (_M0L7vmMod10S1162 != 0) {
          break;
        }
        _M0L6_2atmpS2984 = _M0Lm2vpS1129;
        _M0L7vpDiv10S1164 = _M0L6_2atmpS2984 / 10ull;
        _M0L6_2atmpS2983 = _M0Lm2vrS1128;
        _M0L7vrDiv10S1165 = _M0L6_2atmpS2983 / 10ull;
        _M0L6_2atmpS2982 = _M0Lm2vrS1128;
        _M0L6_2atmpS2979 = (int32_t)_M0L6_2atmpS2982;
        _M0L6_2atmpS2981 = (int32_t)_M0L7vrDiv10S1165;
        _M0L6_2atmpS2980 = 10 * _M0L6_2atmpS2981;
        _M0L7vrMod10S1166 = _M0L6_2atmpS2979 - _M0L6_2atmpS2980;
        if (_M0Lm17vrIsTrailingZerosS1133) {
          int32_t _M0L6_2atmpS2977 = _M0Lm16lastRemovedDigitS1153;
          _M0Lm17vrIsTrailingZerosS1133 = _M0L6_2atmpS2977 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1133 = 0;
        }
        _M0Lm16lastRemovedDigitS1153 = _M0L7vrMod10S1166;
        _M0Lm2vrS1128 = _M0L7vrDiv10S1165;
        _M0Lm2vpS1129 = _M0L7vpDiv10S1164;
        _M0Lm2vmS1130 = _M0L7vmDiv10S1161;
        _M0L6_2atmpS2978 = _M0Lm7removedS1152;
        _M0Lm7removedS1152 = _M0L6_2atmpS2978 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1133) {
      int32_t _M0L6_2atmpS2992 = _M0Lm16lastRemovedDigitS1153;
      if (_M0L6_2atmpS2992 == 5) {
        uint64_t _M0L6_2atmpS2991 = _M0Lm2vrS1128;
        uint64_t _M0L6_2atmpS2990 = _M0L6_2atmpS2991 % 2ull;
        _if__result_3916 = _M0L6_2atmpS2990 == 0ull;
      } else {
        _if__result_3916 = 0;
      }
    } else {
      _if__result_3916 = 0;
    }
    if (_if__result_3916) {
      _M0Lm16lastRemovedDigitS1153 = 4;
    }
    _M0L6_2atmpS2993 = _M0Lm2vrS1128;
    _M0L6_2atmpS2999 = _M0Lm2vrS1128;
    _M0L6_2atmpS3000 = _M0Lm2vmS1130;
    if (_M0L6_2atmpS2999 == _M0L6_2atmpS3000) {
      if (!_M0L4evenS1125) {
        _if__result_3917 = 1;
      } else {
        int32_t _M0L6_2atmpS2998 = _M0Lm17vmIsTrailingZerosS1132;
        _if__result_3917 = !_M0L6_2atmpS2998;
      }
    } else {
      _if__result_3917 = 0;
    }
    if (_if__result_3917) {
      _M0L6_2atmpS2996 = 1;
    } else {
      int32_t _M0L6_2atmpS2997 = _M0Lm16lastRemovedDigitS1153;
      _M0L6_2atmpS2996 = _M0L6_2atmpS2997 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2995 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2996);
    _M0L6_2atmpS2994 = *(uint64_t*)&_M0L6_2atmpS2995;
    _M0Lm6outputS1154 = _M0L6_2atmpS2993 + _M0L6_2atmpS2994;
  } else {
    int32_t _M0Lm7roundUpS1167 = 0;
    uint64_t _M0L6_2atmpS3021 = _M0Lm2vpS1129;
    uint64_t _M0L8vpDiv100S1168 = _M0L6_2atmpS3021 / 100ull;
    uint64_t _M0L6_2atmpS3020 = _M0Lm2vmS1130;
    uint64_t _M0L8vmDiv100S1169 = _M0L6_2atmpS3020 / 100ull;
    uint64_t _M0L6_2atmpS3015;
    uint64_t _M0L6_2atmpS3018;
    uint64_t _M0L6_2atmpS3019;
    int32_t _M0L6_2atmpS3017;
    uint64_t _M0L6_2atmpS3016;
    if (_M0L8vpDiv100S1168 > _M0L8vmDiv100S1169) {
      uint64_t _M0L6_2atmpS3006 = _M0Lm2vrS1128;
      uint64_t _M0L8vrDiv100S1170 = _M0L6_2atmpS3006 / 100ull;
      uint64_t _M0L6_2atmpS3005 = _M0Lm2vrS1128;
      int32_t _M0L6_2atmpS3002 = (int32_t)_M0L6_2atmpS3005;
      int32_t _M0L6_2atmpS3004 = (int32_t)_M0L8vrDiv100S1170;
      int32_t _M0L6_2atmpS3003 = 100 * _M0L6_2atmpS3004;
      int32_t _M0L8vrMod100S1171 = _M0L6_2atmpS3002 - _M0L6_2atmpS3003;
      int32_t _M0L6_2atmpS3001;
      _M0Lm7roundUpS1167 = _M0L8vrMod100S1171 >= 50;
      _M0Lm2vrS1128 = _M0L8vrDiv100S1170;
      _M0Lm2vpS1129 = _M0L8vpDiv100S1168;
      _M0Lm2vmS1130 = _M0L8vmDiv100S1169;
      _M0L6_2atmpS3001 = _M0Lm7removedS1152;
      _M0Lm7removedS1152 = _M0L6_2atmpS3001 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3014 = _M0Lm2vpS1129;
      uint64_t _M0L7vpDiv10S1172 = _M0L6_2atmpS3014 / 10ull;
      uint64_t _M0L6_2atmpS3013 = _M0Lm2vmS1130;
      uint64_t _M0L7vmDiv10S1173 = _M0L6_2atmpS3013 / 10ull;
      uint64_t _M0L6_2atmpS3012;
      uint64_t _M0L7vrDiv10S1175;
      uint64_t _M0L6_2atmpS3011;
      int32_t _M0L6_2atmpS3008;
      int32_t _M0L6_2atmpS3010;
      int32_t _M0L6_2atmpS3009;
      int32_t _M0L7vrMod10S1176;
      int32_t _M0L6_2atmpS3007;
      if (_M0L7vpDiv10S1172 <= _M0L7vmDiv10S1173) {
        break;
      }
      _M0L6_2atmpS3012 = _M0Lm2vrS1128;
      _M0L7vrDiv10S1175 = _M0L6_2atmpS3012 / 10ull;
      _M0L6_2atmpS3011 = _M0Lm2vrS1128;
      _M0L6_2atmpS3008 = (int32_t)_M0L6_2atmpS3011;
      _M0L6_2atmpS3010 = (int32_t)_M0L7vrDiv10S1175;
      _M0L6_2atmpS3009 = 10 * _M0L6_2atmpS3010;
      _M0L7vrMod10S1176 = _M0L6_2atmpS3008 - _M0L6_2atmpS3009;
      _M0Lm7roundUpS1167 = _M0L7vrMod10S1176 >= 5;
      _M0Lm2vrS1128 = _M0L7vrDiv10S1175;
      _M0Lm2vpS1129 = _M0L7vpDiv10S1172;
      _M0Lm2vmS1130 = _M0L7vmDiv10S1173;
      _M0L6_2atmpS3007 = _M0Lm7removedS1152;
      _M0Lm7removedS1152 = _M0L6_2atmpS3007 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3015 = _M0Lm2vrS1128;
    _M0L6_2atmpS3018 = _M0Lm2vrS1128;
    _M0L6_2atmpS3019 = _M0Lm2vmS1130;
    _M0L6_2atmpS3017
    = _M0L6_2atmpS3018 == _M0L6_2atmpS3019 || _M0Lm7roundUpS1167;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3016 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3017);
    _M0Lm6outputS1154 = _M0L6_2atmpS3015 + _M0L6_2atmpS3016;
  }
  _M0L6_2atmpS3023 = _M0Lm3e10S1131;
  _M0L6_2atmpS3024 = _M0Lm7removedS1152;
  _M0L3expS1177 = _M0L6_2atmpS3023 + _M0L6_2atmpS3024;
  _M0L6_2atmpS3022 = _M0Lm6outputS1154;
  _block_3919
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3919)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_3919->$0 = _M0L6_2atmpS3022;
  _block_3919->$1 = _M0L3expS1177;
  return _block_3919;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1120) {
  #line 110 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1120) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1119) {
  #line 58 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1119) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1118) {
  #line 32 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1118) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1117) {
  #line 280 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L1vS1117 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1117 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1117 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1117 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1117 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1117 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1117 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1117 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1117 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1117 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1117 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1117 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1117 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1117 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1117 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1117 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1100) {
  int32_t _M0L6_2atmpS2923;
  int32_t _M0L6_2atmpS2922;
  int32_t _M0L4baseS1099;
  int32_t _M0L5base2S1101;
  int32_t _M0L6offsetS1102;
  int32_t _M0L6_2atmpS2921;
  uint64_t _M0L4mul0S1103;
  int32_t _M0L6_2atmpS2920;
  int32_t _M0L6_2atmpS2919;
  uint64_t _M0L4mul1S1104;
  uint64_t _M0L1mS1105;
  struct _M0TPB7Umul128 _M0L7_2abindS1106;
  uint64_t _M0L7_2alow1S1107;
  uint64_t _M0L8_2ahigh1S1108;
  struct _M0TPB7Umul128 _M0L7_2abindS1109;
  uint64_t _M0L7_2alow0S1110;
  uint64_t _M0L8_2ahigh0S1111;
  uint64_t _M0L3sumS1112;
  uint64_t _M0Lm5high1S1113;
  int32_t _M0L6_2atmpS2917;
  int32_t _M0L6_2atmpS2918;
  int32_t _M0L5deltaS1114;
  uint64_t _M0L6_2atmpS2916;
  uint64_t _M0L6_2atmpS2908;
  int32_t _M0L6_2atmpS2915;
  uint32_t _M0L6_2atmpS2912;
  int32_t _M0L6_2atmpS2914;
  int32_t _M0L6_2atmpS2913;
  uint32_t _M0L6_2atmpS2911;
  uint32_t _M0L6_2atmpS2910;
  uint64_t _M0L6_2atmpS2909;
  uint64_t _M0L1aS1115;
  uint64_t _M0L6_2atmpS2907;
  uint64_t _M0L1bS1116;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2923 = _M0L1iS1100 + 26;
  _M0L6_2atmpS2922 = _M0L6_2atmpS2923 - 1;
  _M0L4baseS1099 = _M0L6_2atmpS2922 / 26;
  _M0L5base2S1101 = _M0L4baseS1099 * 26;
  _M0L6offsetS1102 = _M0L5base2S1101 - _M0L1iS1100;
  _M0L6_2atmpS2921 = _M0L4baseS1099 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1103
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2921);
  _M0L6_2atmpS2920 = _M0L4baseS1099 * 2;
  _M0L6_2atmpS2919 = _M0L6_2atmpS2920 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1104
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2919);
  if (_M0L6offsetS1102 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1103,
                                      .$1 = _M0L4mul1S1104};
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1105
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1102);
  #line 249 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1106 = _M0FPB7umul128(_M0L1mS1105, _M0L4mul1S1104);
  _M0L7_2alow1S1107 = _M0L7_2abindS1106.$0;
  _M0L8_2ahigh1S1108 = _M0L7_2abindS1106.$1;
  #line 250 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1109 = _M0FPB7umul128(_M0L1mS1105, _M0L4mul0S1103);
  _M0L7_2alow0S1110 = _M0L7_2abindS1109.$0;
  _M0L8_2ahigh0S1111 = _M0L7_2abindS1109.$1;
  _M0L3sumS1112 = _M0L8_2ahigh0S1111 + _M0L7_2alow1S1107;
  _M0Lm5high1S1113 = _M0L8_2ahigh1S1108;
  if (_M0L3sumS1112 < _M0L8_2ahigh0S1111) {
    uint64_t _M0L6_2atmpS2906 = _M0Lm5high1S1113;
    _M0Lm5high1S1113 = _M0L6_2atmpS2906 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2917 = _M0FPB8pow5bits(_M0L5base2S1101);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2918 = _M0FPB8pow5bits(_M0L1iS1100);
  _M0L5deltaS1114 = _M0L6_2atmpS2917 - _M0L6_2atmpS2918;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2916
  = _M0FPB13shiftright128(_M0L7_2alow0S1110, _M0L3sumS1112, _M0L5deltaS1114);
  _M0L6_2atmpS2908 = _M0L6_2atmpS2916 + 1ull;
  _M0L6_2atmpS2915 = _M0L1iS1100 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2912
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2915);
  _M0L6_2atmpS2914 = _M0L1iS1100 % 16;
  _M0L6_2atmpS2913 = _M0L6_2atmpS2914 << 1;
  _M0L6_2atmpS2911 = _M0L6_2atmpS2912 >> (_M0L6_2atmpS2913 & 31);
  _M0L6_2atmpS2910 = _M0L6_2atmpS2911 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2909 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2910);
  _M0L1aS1115 = _M0L6_2atmpS2908 + _M0L6_2atmpS2909;
  _M0L6_2atmpS2907 = _M0Lm5high1S1113;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1116
  = _M0FPB13shiftright128(_M0L3sumS1112, _M0L6_2atmpS2907, _M0L5deltaS1114);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1115, .$1 = _M0L1bS1116};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1082) {
  int32_t _M0L4baseS1081;
  int32_t _M0L5base2S1083;
  int32_t _M0L6offsetS1084;
  int32_t _M0L6_2atmpS2905;
  uint64_t _M0L4mul0S1085;
  int32_t _M0L6_2atmpS2904;
  int32_t _M0L6_2atmpS2903;
  uint64_t _M0L4mul1S1086;
  uint64_t _M0L1mS1087;
  struct _M0TPB7Umul128 _M0L7_2abindS1088;
  uint64_t _M0L7_2alow1S1089;
  uint64_t _M0L8_2ahigh1S1090;
  struct _M0TPB7Umul128 _M0L7_2abindS1091;
  uint64_t _M0L7_2alow0S1092;
  uint64_t _M0L8_2ahigh0S1093;
  uint64_t _M0L3sumS1094;
  uint64_t _M0Lm5high1S1095;
  int32_t _M0L6_2atmpS2901;
  int32_t _M0L6_2atmpS2902;
  int32_t _M0L5deltaS1096;
  uint64_t _M0L6_2atmpS2893;
  int32_t _M0L6_2atmpS2900;
  uint32_t _M0L6_2atmpS2897;
  int32_t _M0L6_2atmpS2899;
  int32_t _M0L6_2atmpS2898;
  uint32_t _M0L6_2atmpS2896;
  uint32_t _M0L6_2atmpS2895;
  uint64_t _M0L6_2atmpS2894;
  uint64_t _M0L1aS1097;
  uint64_t _M0L6_2atmpS2892;
  uint64_t _M0L1bS1098;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1081 = _M0L1iS1082 / 26;
  _M0L5base2S1083 = _M0L4baseS1081 * 26;
  _M0L6offsetS1084 = _M0L1iS1082 - _M0L5base2S1083;
  _M0L6_2atmpS2905 = _M0L4baseS1081 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1085
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2905);
  _M0L6_2atmpS2904 = _M0L4baseS1081 * 2;
  _M0L6_2atmpS2903 = _M0L6_2atmpS2904 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1086
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2903);
  if (_M0L6offsetS1084 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1085,
                                      .$1 = _M0L4mul1S1086};
  }
  #line 222 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1087
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1084);
  #line 223 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1088 = _M0FPB7umul128(_M0L1mS1087, _M0L4mul1S1086);
  _M0L7_2alow1S1089 = _M0L7_2abindS1088.$0;
  _M0L8_2ahigh1S1090 = _M0L7_2abindS1088.$1;
  #line 224 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1091 = _M0FPB7umul128(_M0L1mS1087, _M0L4mul0S1085);
  _M0L7_2alow0S1092 = _M0L7_2abindS1091.$0;
  _M0L8_2ahigh0S1093 = _M0L7_2abindS1091.$1;
  _M0L3sumS1094 = _M0L8_2ahigh0S1093 + _M0L7_2alow1S1089;
  _M0Lm5high1S1095 = _M0L8_2ahigh1S1090;
  if (_M0L3sumS1094 < _M0L8_2ahigh0S1093) {
    uint64_t _M0L6_2atmpS2891 = _M0Lm5high1S1095;
    _M0Lm5high1S1095 = _M0L6_2atmpS2891 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2901 = _M0FPB8pow5bits(_M0L1iS1082);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2902 = _M0FPB8pow5bits(_M0L5base2S1083);
  _M0L5deltaS1096 = _M0L6_2atmpS2901 - _M0L6_2atmpS2902;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2893
  = _M0FPB13shiftright128(_M0L7_2alow0S1092, _M0L3sumS1094, _M0L5deltaS1096);
  _M0L6_2atmpS2900 = _M0L1iS1082 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2897
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2900);
  _M0L6_2atmpS2899 = _M0L1iS1082 % 16;
  _M0L6_2atmpS2898 = _M0L6_2atmpS2899 << 1;
  _M0L6_2atmpS2896 = _M0L6_2atmpS2897 >> (_M0L6_2atmpS2898 & 31);
  _M0L6_2atmpS2895 = _M0L6_2atmpS2896 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2894 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2895);
  _M0L1aS1097 = _M0L6_2atmpS2893 + _M0L6_2atmpS2894;
  _M0L6_2atmpS2892 = _M0Lm5high1S1095;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1098
  = _M0FPB13shiftright128(_M0L3sumS1094, _M0L6_2atmpS2892, _M0L5deltaS1096);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1097, .$1 = _M0L1bS1098};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1055,
  struct _M0TPB8Pow5Pair _M0L3mulS1052,
  int32_t _M0L1jS1068,
  int32_t _M0L7mmShiftS1070
) {
  uint64_t _M0L7_2amul0S1051;
  uint64_t _M0L7_2amul1S1053;
  uint64_t _M0L1mS1054;
  struct _M0TPB7Umul128 _M0L7_2abindS1056;
  uint64_t _M0L5_2aloS1057;
  uint64_t _M0L6_2atmpS1058;
  struct _M0TPB7Umul128 _M0L7_2abindS1059;
  uint64_t _M0L6_2alo2S1060;
  uint64_t _M0L6_2ahi2S1061;
  uint64_t _M0L3midS1062;
  uint64_t _M0L6_2atmpS2890;
  uint64_t _M0L2hiS1063;
  uint64_t _M0L3lo2S1064;
  uint64_t _M0L6_2atmpS2888;
  uint64_t _M0L6_2atmpS2889;
  uint64_t _M0L4mid2S1065;
  uint64_t _M0L6_2atmpS2887;
  uint64_t _M0L3hi2S1066;
  int32_t _M0L6_2atmpS2886;
  int32_t _M0L6_2atmpS2885;
  uint64_t _M0L2vpS1067;
  uint64_t _M0Lm2vmS1069;
  int32_t _M0L6_2atmpS2884;
  int32_t _M0L6_2atmpS2883;
  uint64_t _M0L2vrS1080;
  uint64_t _M0L6_2atmpS2882;
  #line 129 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2amul0S1051 = _M0L3mulS1052.$0;
  _M0L7_2amul1S1053 = _M0L3mulS1052.$1;
  _M0L1mS1054 = _M0L1mS1055 << 1;
  #line 137 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1056 = _M0FPB7umul128(_M0L1mS1054, _M0L7_2amul0S1051);
  _M0L5_2aloS1057 = _M0L7_2abindS1056.$0;
  _M0L6_2atmpS1058 = _M0L7_2abindS1056.$1;
  #line 138 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1059 = _M0FPB7umul128(_M0L1mS1054, _M0L7_2amul1S1053);
  _M0L6_2alo2S1060 = _M0L7_2abindS1059.$0;
  _M0L6_2ahi2S1061 = _M0L7_2abindS1059.$1;
  _M0L3midS1062 = _M0L6_2atmpS1058 + _M0L6_2alo2S1060;
  if (_M0L3midS1062 < _M0L6_2atmpS1058) {
    _M0L6_2atmpS2890 = 1ull;
  } else {
    _M0L6_2atmpS2890 = 0ull;
  }
  _M0L2hiS1063 = _M0L6_2ahi2S1061 + _M0L6_2atmpS2890;
  _M0L3lo2S1064 = _M0L5_2aloS1057 + _M0L7_2amul0S1051;
  _M0L6_2atmpS2888 = _M0L3midS1062 + _M0L7_2amul1S1053;
  if (_M0L3lo2S1064 < _M0L5_2aloS1057) {
    _M0L6_2atmpS2889 = 1ull;
  } else {
    _M0L6_2atmpS2889 = 0ull;
  }
  _M0L4mid2S1065 = _M0L6_2atmpS2888 + _M0L6_2atmpS2889;
  if (_M0L4mid2S1065 < _M0L3midS1062) {
    _M0L6_2atmpS2887 = 1ull;
  } else {
    _M0L6_2atmpS2887 = 0ull;
  }
  _M0L3hi2S1066 = _M0L2hiS1063 + _M0L6_2atmpS2887;
  _M0L6_2atmpS2886 = _M0L1jS1068 - 64;
  _M0L6_2atmpS2885 = _M0L6_2atmpS2886 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1067
  = _M0FPB13shiftright128(_M0L4mid2S1065, _M0L3hi2S1066, _M0L6_2atmpS2885);
  _M0Lm2vmS1069 = 0ull;
  if (_M0L7mmShiftS1070) {
    uint64_t _M0L3lo3S1071 = _M0L5_2aloS1057 - _M0L7_2amul0S1051;
    uint64_t _M0L6_2atmpS2872 = _M0L3midS1062 - _M0L7_2amul1S1053;
    uint64_t _M0L6_2atmpS2873;
    uint64_t _M0L4mid3S1072;
    uint64_t _M0L6_2atmpS2871;
    uint64_t _M0L3hi3S1073;
    int32_t _M0L6_2atmpS2870;
    int32_t _M0L6_2atmpS2869;
    if (_M0L5_2aloS1057 < _M0L3lo3S1071) {
      _M0L6_2atmpS2873 = 1ull;
    } else {
      _M0L6_2atmpS2873 = 0ull;
    }
    _M0L4mid3S1072 = _M0L6_2atmpS2872 - _M0L6_2atmpS2873;
    if (_M0L3midS1062 < _M0L4mid3S1072) {
      _M0L6_2atmpS2871 = 1ull;
    } else {
      _M0L6_2atmpS2871 = 0ull;
    }
    _M0L3hi3S1073 = _M0L2hiS1063 - _M0L6_2atmpS2871;
    _M0L6_2atmpS2870 = _M0L1jS1068 - 64;
    _M0L6_2atmpS2869 = _M0L6_2atmpS2870 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1069
    = _M0FPB13shiftright128(_M0L4mid3S1072, _M0L3hi3S1073, _M0L6_2atmpS2869);
  } else {
    uint64_t _M0L3lo3S1074 = _M0L5_2aloS1057 + _M0L5_2aloS1057;
    uint64_t _M0L6_2atmpS2880 = _M0L3midS1062 + _M0L3midS1062;
    uint64_t _M0L6_2atmpS2881;
    uint64_t _M0L4mid3S1075;
    uint64_t _M0L6_2atmpS2878;
    uint64_t _M0L6_2atmpS2879;
    uint64_t _M0L3hi3S1076;
    uint64_t _M0L3lo4S1077;
    uint64_t _M0L6_2atmpS2876;
    uint64_t _M0L6_2atmpS2877;
    uint64_t _M0L4mid4S1078;
    uint64_t _M0L6_2atmpS2875;
    uint64_t _M0L3hi4S1079;
    int32_t _M0L6_2atmpS2874;
    if (_M0L3lo3S1074 < _M0L5_2aloS1057) {
      _M0L6_2atmpS2881 = 1ull;
    } else {
      _M0L6_2atmpS2881 = 0ull;
    }
    _M0L4mid3S1075 = _M0L6_2atmpS2880 + _M0L6_2atmpS2881;
    _M0L6_2atmpS2878 = _M0L2hiS1063 + _M0L2hiS1063;
    if (_M0L4mid3S1075 < _M0L3midS1062) {
      _M0L6_2atmpS2879 = 1ull;
    } else {
      _M0L6_2atmpS2879 = 0ull;
    }
    _M0L3hi3S1076 = _M0L6_2atmpS2878 + _M0L6_2atmpS2879;
    _M0L3lo4S1077 = _M0L3lo3S1074 - _M0L7_2amul0S1051;
    _M0L6_2atmpS2876 = _M0L4mid3S1075 - _M0L7_2amul1S1053;
    if (_M0L3lo3S1074 < _M0L3lo4S1077) {
      _M0L6_2atmpS2877 = 1ull;
    } else {
      _M0L6_2atmpS2877 = 0ull;
    }
    _M0L4mid4S1078 = _M0L6_2atmpS2876 - _M0L6_2atmpS2877;
    if (_M0L4mid3S1075 < _M0L4mid4S1078) {
      _M0L6_2atmpS2875 = 1ull;
    } else {
      _M0L6_2atmpS2875 = 0ull;
    }
    _M0L3hi4S1079 = _M0L3hi3S1076 - _M0L6_2atmpS2875;
    _M0L6_2atmpS2874 = _M0L1jS1068 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1069
    = _M0FPB13shiftright128(_M0L4mid4S1078, _M0L3hi4S1079, _M0L6_2atmpS2874);
  }
  _M0L6_2atmpS2884 = _M0L1jS1068 - 64;
  _M0L6_2atmpS2883 = _M0L6_2atmpS2884 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1080
  = _M0FPB13shiftright128(_M0L3midS1062, _M0L2hiS1063, _M0L6_2atmpS2883);
  _M0L6_2atmpS2882 = _M0Lm2vmS1069;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1080,
                                                .$1 = _M0L2vpS1067,
                                                .$2 = _M0L6_2atmpS2882};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1049,
  int32_t _M0L1pS1050
) {
  uint64_t _M0L6_2atmpS2868;
  uint64_t _M0L6_2atmpS2867;
  uint64_t _M0L6_2atmpS2866;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2868 = 1ull << (_M0L1pS1050 & 63);
  _M0L6_2atmpS2867 = _M0L6_2atmpS2868 - 1ull;
  _M0L6_2atmpS2866 = _M0L5valueS1049 & _M0L6_2atmpS2867;
  return _M0L6_2atmpS2866 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1047,
  int32_t _M0L1pS1048
) {
  int32_t _M0L6_2atmpS2865;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2865 = _M0FPB10pow5Factor(_M0L5valueS1047);
  return _M0L6_2atmpS2865 >= _M0L1pS1048;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1042) {
  uint64_t _M0L6_2atmpS2856;
  uint64_t _M0L6_2atmpS2857;
  uint64_t _M0L6_2atmpS2858;
  uint64_t _M0L6_2atmpS2859;
  uint64_t _M0L6_2atmpS2864;
  int32_t _M0L5countS1043;
  uint64_t _M0L1vS1044;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2856 = _M0L5valueS1042 % 5ull;
  if (_M0L6_2atmpS2856 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2857 = _M0L5valueS1042 % 25ull;
  if (_M0L6_2atmpS2857 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2858 = _M0L5valueS1042 % 125ull;
  if (_M0L6_2atmpS2858 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2859 = _M0L5valueS1042 % 625ull;
  if (_M0L6_2atmpS2859 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS2864 = _M0L5valueS1042 / 625ull;
  _M0L5countS1043 = 4;
  _M0L1vS1044 = _M0L6_2atmpS2864;
  while (1) {
    if (_M0L1vS1044 > 0ull) {
      uint64_t _M0L6_2atmpS2860 = _M0L1vS1044 % 5ull;
      int32_t _M0L6_2atmpS2861;
      uint64_t _M0L6_2atmpS2862;
      if (_M0L6_2atmpS2860 != 0ull) {
        return _M0L5countS1043;
      }
      _M0L6_2atmpS2861 = _M0L5countS1043 + 1;
      _M0L6_2atmpS2862 = _M0L1vS1044 / 5ull;
      _M0L5countS1043 = _M0L6_2atmpS2861;
      _M0L1vS1044 = _M0L6_2atmpS2862;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1046;
      moonbit_string_t _M0L6_2atmpS2863;
      int32_t _result_3921;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1046
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1046, (moonbit_string_t)moonbit_string_literal_84.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1046, _M0L5valueS1042);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS2863
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1046);
      moonbit_decref(_M0L18_2astring__builderS1046);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_3921 = _M0FPC15abort5abortGiE(_M0L6_2atmpS2863);
      moonbit_decref(_M0L6_2atmpS2863);
      return _result_3921;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1041,
  uint64_t _M0L2hiS1039,
  int32_t _M0L4distS1040
) {
  int32_t _M0L6_2atmpS2855;
  uint64_t _M0L6_2atmpS2853;
  uint64_t _M0L6_2atmpS2854;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2855 = 64 - _M0L4distS1040;
  _M0L6_2atmpS2853 = _M0L2hiS1039 << (_M0L6_2atmpS2855 & 63);
  _M0L6_2atmpS2854 = _M0L2loS1041 >> (_M0L4distS1040 & 63);
  return _M0L6_2atmpS2853 | _M0L6_2atmpS2854;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS1029,
  uint64_t _M0L1bS1032
) {
  uint64_t _M0L3aLoS1028;
  uint64_t _M0L3aHiS1030;
  uint64_t _M0L3bLoS1031;
  uint64_t _M0L3bHiS1033;
  uint64_t _M0L1xS1034;
  uint64_t _M0L6_2atmpS2851;
  uint64_t _M0L6_2atmpS2852;
  uint64_t _M0L1yS1035;
  uint64_t _M0L6_2atmpS2849;
  uint64_t _M0L6_2atmpS2850;
  uint64_t _M0L1zS1036;
  uint64_t _M0L6_2atmpS2847;
  uint64_t _M0L6_2atmpS2848;
  uint64_t _M0L6_2atmpS2845;
  uint64_t _M0L6_2atmpS2846;
  uint64_t _M0L1wS1037;
  uint64_t _M0L2loS1038;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1028 = _M0L1aS1029 & 4294967295ull;
  _M0L3aHiS1030 = _M0L1aS1029 >> 32;
  _M0L3bLoS1031 = _M0L1bS1032 & 4294967295ull;
  _M0L3bHiS1033 = _M0L1bS1032 >> 32;
  _M0L1xS1034 = _M0L3aLoS1028 * _M0L3bLoS1031;
  _M0L6_2atmpS2851 = _M0L3aHiS1030 * _M0L3bLoS1031;
  _M0L6_2atmpS2852 = _M0L1xS1034 >> 32;
  _M0L1yS1035 = _M0L6_2atmpS2851 + _M0L6_2atmpS2852;
  _M0L6_2atmpS2849 = _M0L3aLoS1028 * _M0L3bHiS1033;
  _M0L6_2atmpS2850 = _M0L1yS1035 & 4294967295ull;
  _M0L1zS1036 = _M0L6_2atmpS2849 + _M0L6_2atmpS2850;
  _M0L6_2atmpS2847 = _M0L3aHiS1030 * _M0L3bHiS1033;
  _M0L6_2atmpS2848 = _M0L1yS1035 >> 32;
  _M0L6_2atmpS2845 = _M0L6_2atmpS2847 + _M0L6_2atmpS2848;
  _M0L6_2atmpS2846 = _M0L1zS1036 >> 32;
  _M0L1wS1037 = _M0L6_2atmpS2845 + _M0L6_2atmpS2846;
  _M0L2loS1038 = _M0L1aS1029 * _M0L1bS1032;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1038, .$1 = _M0L1wS1037};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1026,
  int32_t _M0L4fromS1023,
  int32_t _M0L2toS1022
) {
  int32_t _M0L3lenS1021;
  int32_t _M0L6_2atmpS2844;
  uint16_t* _M0L6bufferS1024;
  int32_t _M0L1iS1025;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1021 = _M0L2toS1022 - _M0L4fromS1023;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2844 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1024
  = (uint16_t*)moonbit_make_string(_M0L3lenS1021, _M0L6_2atmpS2844);
  _M0L1iS1025 = 0;
  while (1) {
    if (_M0L1iS1025 < _M0L3lenS1021) {
      int32_t _M0L6_2atmpS2842 = _M0L4fromS1023 + _M0L1iS1025;
      int32_t _M0L6_2atmpS2841;
      int32_t _M0L6_2atmpS2840;
      int32_t _M0L6_2atmpS2843;
      if (
        _M0L6_2atmpS2842 < 0
        || _M0L6_2atmpS2842 >= Moonbit_array_length(_M0L5bytesS1026)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2841 = (int32_t)_M0L5bytesS1026[_M0L6_2atmpS2842];
      _M0L6_2atmpS2840 = (uint16_t)_M0L6_2atmpS2841;
      if (
        _M0L1iS1025 < 0
        || _M0L1iS1025 >= Moonbit_array_length(_M0L6bufferS1024)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1024[_M0L1iS1025] = _M0L6_2atmpS2840;
      _M0L6_2atmpS2843 = _M0L1iS1025 + 1;
      _M0L1iS1025 = _M0L6_2atmpS2843;
      continue;
    }
    break;
  }
  return _M0L6bufferS1024;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1020) {
  int32_t _M0L6_2atmpS2839;
  uint32_t _M0L6_2atmpS2838;
  uint32_t _M0L6_2atmpS2837;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2839 = _M0L1eS1020 * 78913;
  _M0L6_2atmpS2838 = *(uint32_t*)&_M0L6_2atmpS2839;
  _M0L6_2atmpS2837 = _M0L6_2atmpS2838 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2837;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1019) {
  int32_t _M0L6_2atmpS2836;
  uint32_t _M0L6_2atmpS2835;
  uint32_t _M0L6_2atmpS2834;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2836 = _M0L1eS1019 * 732923;
  _M0L6_2atmpS2835 = *(uint32_t*)&_M0L6_2atmpS2836;
  _M0L6_2atmpS2834 = _M0L6_2atmpS2835 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2834;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1017,
  int32_t _M0L8exponentS1018,
  int32_t _M0L8mantissaS1015
) {
  moonbit_string_t _M0L1sS1016;
  moonbit_string_t _result_3924;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1015) {
    return (moonbit_string_t)moonbit_string_literal_85.data;
  }
  if (_M0L4signS1017) {
    _M0L1sS1016 = (moonbit_string_t)moonbit_string_literal_86.data;
  } else {
    _M0L1sS1016 = (moonbit_string_t)moonbit_string_literal_82.data;
  }
  if (_M0L8exponentS1018) {
    moonbit_string_t _result_3923;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_3923
    = moonbit_add_string(_M0L1sS1016, (moonbit_string_t)moonbit_string_literal_87.data);
    moonbit_decref(_M0L1sS1016);
    return _result_3923;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_3924
  = moonbit_add_string(_M0L1sS1016, (moonbit_string_t)moonbit_string_literal_88.data);
  moonbit_decref(_M0L1sS1016);
  return _result_3924;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1014) {
  int32_t _M0L6_2atmpS2833;
  uint32_t _M0L6_2atmpS2832;
  uint32_t _M0L6_2atmpS2831;
  int32_t _M0L6_2atmpS2830;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2833 = _M0L1eS1014 * 1217359;
  _M0L6_2atmpS2832 = *(uint32_t*)&_M0L6_2atmpS2833;
  _M0L6_2atmpS2831 = _M0L6_2atmpS2832 >> 19;
  _M0L6_2atmpS2830 = *(int32_t*)&_M0L6_2atmpS2831;
  return _M0L6_2atmpS2830 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1010) {
  int32_t _tmp_3925;
  uint32_t _M0L6_2atmpS2829;
  uint32_t _M0Lm3accS1008;
  int32_t _M0L7_2abindS1009;
  int32_t _M0L1iS1011;
  uint32_t _M0L6_2atmpS2828;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_3925 = 0;
  _M0L6_2atmpS2829 = *(uint32_t*)&_tmp_3925;
  _M0Lm3accS1008 = _M0L6_2atmpS2829 + 374761393u;
  _M0L7_2abindS1009 = Moonbit_array_length(_M0L4selfS1010);
  _M0L1iS1011 = 0;
  while (1) {
    if (_M0L1iS1011 < _M0L7_2abindS1009) {
      uint32_t _M0L6_2atmpS2823 = _M0Lm3accS1008;
      int32_t _M0L6_2atmpS2826;
      int32_t _M0L6_2atmpS2825;
      uint32_t _M0L1vS1012;
      uint32_t _M0L6_2atmpS2824;
      int32_t _M0L6_2atmpS2827;
      _M0Lm3accS1008 = _M0L6_2atmpS2823 + 4u;
      _M0L6_2atmpS2826 = _M0L4selfS1010[_M0L1iS1011];
      _M0L6_2atmpS2825 = (int32_t)_M0L6_2atmpS2826;
      _M0L1vS1012 = *(uint32_t*)&_M0L6_2atmpS2825;
      _M0L6_2atmpS2824 = _M0Lm3accS1008;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1008 = _M0FPB13consume4__acc(_M0L6_2atmpS2824, _M0L1vS1012);
      _M0L6_2atmpS2827 = _M0L1iS1011 + 1;
      _M0L1iS1011 = _M0L6_2atmpS2827;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2828 = _M0Lm3accS1008;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS2828);
}

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS1005
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsbEE(_M0L4selfS1005);
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L4selfS1006
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L4selfS1006);
}

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS1007
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsfEE(_M0L4selfS1007);
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1002
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsbE(_M0L4selfS1002);
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1003
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS1003);
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1004
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsfE(_M0L4selfS1004);
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS970
) {
  struct _M0TPB5EntryGsbE* _M0L4headS2802;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS969;
  int32_t _M0L3lenS971;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS972;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__* _closure_3927;
  struct _M0TWEOUsbE* _M0L6_2atmpS2793;
  int64_t _M0L6_2atmpS2794;
  struct _M0TPB4IterGUsbEE* _result_3928;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2802 = _M0L4selfS970->$5;
  if (_M0L4headS2802) {
    moonbit_incref(_M0L4headS2802);
  }
  _M0L11curr__entryS969
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS969)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 33, 0);
  _M0L11curr__entryS969->$0 = _M0L4headS2802;
  _M0L3lenS971 = _M0L4selfS970->$1;
  _M0L9remainingS972
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS972)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS972->$0 = _M0L3lenS971;
  _closure_3927
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__));
  Moonbit_object_header(_closure_3927)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 36, 0);
  _closure_3927->code = &_M0MPB3Map4iterGsbEC2795l711;
  _closure_3927->$0 = _M0L9remainingS972;
  _closure_3927->$1 = _M0L11curr__entryS969;
  _M0L6_2atmpS2793 = (struct _M0TWEOUsbE*)_closure_3927;
  _M0L6_2atmpS2794 = (int64_t)_M0L3lenS971;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_3928 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS2793, _M0L6_2atmpS2794);
  moonbit_decref(_M0L6_2atmpS2793);
  return _result_3928;
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS981
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4headS2812;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L11curr__entryS980;
  int32_t _M0L3lenS982;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS983;
  struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__* _closure_3929;
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2803;
  int64_t _M0L6_2atmpS2804;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _result_3930;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2812 = _M0L4selfS981->$5;
  if (_M0L4headS2812) {
    moonbit_incref(_M0L4headS2812);
  }
  _M0L11curr__entryS980
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS980)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 40, 0);
  _M0L11curr__entryS980->$0 = _M0L4headS2812;
  _M0L3lenS982 = _M0L4selfS981->$1;
  _M0L9remainingS983
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS983)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS983->$0 = _M0L3lenS982;
  _closure_3929
  = (struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__*)moonbit_malloc(sizeof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__));
  Moonbit_object_header(_closure_3929)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 43, 0);
  _closure_3929->code
  = &_M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2805l711;
  _closure_3929->$0 = _M0L9remainingS983;
  _closure_3929->$1 = _M0L11curr__entryS980;
  _M0L6_2atmpS2803
  = (struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_closure_3929;
  _M0L6_2atmpS2804 = (int64_t)_M0L3lenS982;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_3930
  = _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2803, _M0L6_2atmpS2804);
  moonbit_decref(_M0L6_2atmpS2803);
  return _result_3930;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS992
) {
  struct _M0TPB5EntryGsfE* _M0L4headS2822;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS991;
  int32_t _M0L3lenS993;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS994;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__* _closure_3931;
  struct _M0TWEOUsfE* _M0L6_2atmpS2813;
  int64_t _M0L6_2atmpS2814;
  struct _M0TPB4IterGUsfEE* _result_3932;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2822 = _M0L4selfS992->$5;
  if (_M0L4headS2822) {
    moonbit_incref(_M0L4headS2822);
  }
  _M0L11curr__entryS991
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS991)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 47, 0);
  _M0L11curr__entryS991->$0 = _M0L4headS2822;
  _M0L3lenS993 = _M0L4selfS992->$1;
  _M0L9remainingS994
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS994)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS994->$0 = _M0L3lenS993;
  _closure_3931
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__));
  Moonbit_object_header(_closure_3931)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 50, 0);
  _closure_3931->code = &_M0MPB3Map4iterGsfEC2815l711;
  _closure_3931->$0 = _M0L9remainingS994;
  _closure_3931->$1 = _M0L11curr__entryS991;
  _M0L6_2atmpS2813 = (struct _M0TWEOUsfE*)_closure_3931;
  _M0L6_2atmpS2814 = (int64_t)_M0L3lenS993;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_3932 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS2813, _M0L6_2atmpS2814);
  moonbit_decref(_M0L6_2atmpS2813);
  return _result_3932;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2815l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS2816
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__* _M0L14_2acasted__envS2817;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS991;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS994;
  int32_t _M0L3valS2818;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2817
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2815__l711__*)_M0L6_2aenvS2816;
  _M0L11curr__entryS991 = _M0L14_2acasted__envS2817->$1;
  _M0L9remainingS994 = _M0L14_2acasted__envS2817->$0;
  _M0L3valS2818 = _M0L9remainingS994->$0;
  if (_M0L3valS2818 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS996 = _M0L11curr__entryS991->$0;
    if (_M0L7_2abindS996 == 0) {
      goto join_995;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS997 = _M0L7_2abindS996;
      struct _M0TPB5EntryGsfE* _M0L4_2axS998 = _M0L7_2aSomeS997;
      moonbit_string_t _M0L6_2akeyS999 = _M0L4_2axS998->$4;
      float _M0L8_2avalueS1000 = _M0L4_2axS998->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1001 = _M0L4_2axS998->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3475 = _M0L11curr__entryS991->$0;
      int32_t _M0L3valS2820;
      int32_t _M0L6_2atmpS2819;
      struct _M0TUsfE* _M0L8_2atupleS2821;
      if (_M0L7_2anextS1001) {
        moonbit_incref(_M0L7_2anextS1001);
      }
      moonbit_incref(_M0L6_2akeyS999);
      if (_M0L6_2aoldS3475) {
        moonbit_decref(_M0L6_2aoldS3475);
      }
      _M0L11curr__entryS991->$0 = _M0L7_2anextS1001;
      _M0L3valS2820 = _M0L9remainingS994->$0;
      _M0L6_2atmpS2819 = _M0L3valS2820 - 1;
      _M0L9remainingS994->$0 = _M0L6_2atmpS2819;
      _M0L8_2atupleS2821
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS2821)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
      _M0L8_2atupleS2821->$0 = _M0L6_2akeyS999;
      _M0L8_2atupleS2821->$1 = _M0L8_2avalueS1000;
      return _M0L8_2atupleS2821;
    }
  } else {
    goto join_995;
  }
  join_995:;
  return 0;
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2805l711(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aenvS2806
) {
  struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__* _M0L14_2acasted__envS2807;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L11curr__entryS980;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS983;
  int32_t _M0L3valS2808;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2807
  = (struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2805__l711__*)_M0L6_2aenvS2806;
  _M0L11curr__entryS980 = _M0L14_2acasted__envS2807->$1;
  _M0L9remainingS983 = _M0L14_2acasted__envS2807->$0;
  _M0L3valS2808 = _M0L9remainingS983->$0;
  if (_M0L3valS2808 > 0) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS985 =
      _M0L11curr__entryS980->$0;
    if (_M0L7_2abindS985 == 0) {
      goto join_984;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS986 =
        _M0L7_2abindS985;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS987 =
        _M0L7_2aSomeS986;
      moonbit_string_t _M0L6_2akeyS988 = _M0L4_2axS987->$4;
      void* _M0L8_2avalueS989 = _M0L4_2axS987->$5;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS990 =
        _M0L4_2axS987->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3479 =
        _M0L11curr__entryS980->$0;
      int32_t _M0L3valS2810;
      int32_t _M0L6_2atmpS2809;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2atupleS2811;
      if (_M0L7_2anextS990) {
        moonbit_incref(_M0L7_2anextS990);
      }
      moonbit_incref(_M0L8_2avalueS989);
      moonbit_incref(_M0L6_2akeyS988);
      if (_M0L6_2aoldS3479) {
        moonbit_decref(_M0L6_2aoldS3479);
      }
      _M0L11curr__entryS980->$0 = _M0L7_2anextS990;
      _M0L3valS2810 = _M0L9remainingS983->$0;
      _M0L6_2atmpS2809 = _M0L3valS2810 - 1;
      _M0L9remainingS983->$0 = _M0L6_2atmpS2809;
      _M0L8_2atupleS2811
      = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS2811)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 54, 0);
      _M0L8_2atupleS2811->$0 = _M0L6_2akeyS988;
      _M0L8_2atupleS2811->$1 = _M0L8_2avalueS989;
      return _M0L8_2atupleS2811;
    }
  } else {
    goto join_984;
  }
  join_984:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2795l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS2796
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__* _M0L14_2acasted__envS2797;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS969;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS972;
  int32_t _M0L3valS2798;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2797
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2795__l711__*)_M0L6_2aenvS2796;
  _M0L11curr__entryS969 = _M0L14_2acasted__envS2797->$1;
  _M0L9remainingS972 = _M0L14_2acasted__envS2797->$0;
  _M0L3valS2798 = _M0L9remainingS972->$0;
  if (_M0L3valS2798 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS974 = _M0L11curr__entryS969->$0;
    if (_M0L7_2abindS974 == 0) {
      goto join_973;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS975 = _M0L7_2abindS974;
      struct _M0TPB5EntryGsbE* _M0L4_2axS976 = _M0L7_2aSomeS975;
      moonbit_string_t _M0L6_2akeyS977 = _M0L4_2axS976->$4;
      int32_t _M0L8_2avalueS978 = _M0L4_2axS976->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS979 = _M0L4_2axS976->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3484 = _M0L11curr__entryS969->$0;
      int32_t _M0L3valS2800;
      int32_t _M0L6_2atmpS2799;
      struct _M0TUsbE* _M0L8_2atupleS2801;
      if (_M0L7_2anextS979) {
        moonbit_incref(_M0L7_2anextS979);
      }
      moonbit_incref(_M0L6_2akeyS977);
      if (_M0L6_2aoldS3484) {
        moonbit_decref(_M0L6_2aoldS3484);
      }
      _M0L11curr__entryS969->$0 = _M0L7_2anextS979;
      _M0L3valS2800 = _M0L9remainingS972->$0;
      _M0L6_2atmpS2799 = _M0L3valS2800 - 1;
      _M0L9remainingS972->$0 = _M0L6_2atmpS2799;
      _M0L8_2atupleS2801
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS2801)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 58, 0);
      _M0L8_2atupleS2801->$0 = _M0L6_2akeyS977;
      _M0L8_2atupleS2801->$1 = _M0L8_2avalueS978;
      return _M0L8_2atupleS2801;
    }
  } else {
    goto join_973;
  }
  join_973:;
  return 0;
}

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE* _M0L4selfS968) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS968->$1;
}

int32_t _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS960,
  moonbit_string_t _M0L3keyS961
) {
  int32_t _M0L6_2atmpS2789;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2789 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS961);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS960, _M0L3keyS961, _M0L6_2atmpS2789);
  return 0;
}

int32_t _M0MPB3Map6removeGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS962,
  moonbit_string_t _M0L3keyS963
) {
  int32_t _M0L6_2atmpS2790;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2790 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS963);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS962, _M0L3keyS963, _M0L6_2atmpS2790);
  return 0;
}

int32_t _M0MPB3Map6removeGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS964,
  moonbit_string_t _M0L3keyS965
) {
  int32_t _M0L6_2atmpS2791;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2791 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS965);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsfE(_M0L4selfS964, _M0L3keyS965, _M0L6_2atmpS2791);
  return 0;
}

int32_t _M0MPB3Map6removeGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS966,
  moonbit_string_t _M0L3keyS967
) {
  int32_t _M0L6_2atmpS2792;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2792 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS967);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsbE(_M0L4selfS966, _M0L3keyS967, _M0L6_2atmpS2792);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS927,
  moonbit_string_t _M0L3keyS931,
  int32_t _M0L4hashS930
) {
  int32_t _M0L14capacity__maskS2752;
  int32_t _M0L6_2atmpS2751;
  int32_t _M0L1iS924;
  int32_t _M0L3idxS925;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2752 = _M0L4selfS927->$3;
  _M0L6_2atmpS2751 = _M0L4hashS930 & _M0L14capacity__maskS2752;
  _M0L1iS924 = 0;
  _M0L3idxS925 = _M0L6_2atmpS2751;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2750 =
      _M0L4selfS927->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS926;
    if (
      _M0L3idxS925 < 0
      || _M0L3idxS925 >= Moonbit_array_length(_M0L7entriesS2750)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS926
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2750[
        _M0L3idxS925
      ];
    if (_M0L7_2abindS926 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS928 =
        _M0L7_2abindS926;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS929 =
        _M0L7_2aSomeS928;
      int32_t _M0L4hashS2742 = _M0L8_2aentryS929->$3;
      int32_t _if__result_3937;
      int32_t _M0L3pslS2745;
      int32_t _M0L6_2atmpS2746;
      int32_t _M0L6_2atmpS2748;
      int32_t _M0L14capacity__maskS2749;
      int32_t _M0L6_2atmpS2747;
      if (_M0L4hashS2742 == _M0L4hashS930) {
        moonbit_string_t _M0L3keyS2741 = _M0L8_2aentryS929->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3937
        = _M0L3keyS2741 == _M0L3keyS931
          || Moonbit_array_length(_M0L3keyS2741)
             == Moonbit_array_length(_M0L3keyS931)
             && 0
                == memcmp(_M0L3keyS2741, _M0L3keyS931, Moonbit_array_length(_M0L3keyS2741) * 2);
      } else {
        _if__result_3937 = 0;
      }
      if (_if__result_3937) {
        int32_t _M0L4sizeS2744;
        int32_t _M0L6_2atmpS2743;
        moonbit_incref(_M0L8_2aentryS929);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS927, _M0L8_2aentryS929);
        moonbit_decref(_M0L8_2aentryS929);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS927, _M0L3idxS925);
        _M0L4sizeS2744 = _M0L4selfS927->$1;
        _M0L6_2atmpS2743 = _M0L4sizeS2744 - 1;
        _M0L4selfS927->$1 = _M0L6_2atmpS2743;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS929);
      }
      _M0L3pslS2745 = _M0L8_2aentryS929->$2;
      moonbit_decref(_M0L8_2aentryS929);
      if (_M0L1iS924 > _M0L3pslS2745) {
        break;
      }
      _M0L6_2atmpS2746 = _M0L1iS924 + 1;
      _M0L6_2atmpS2748 = _M0L3idxS925 + 1;
      _M0L14capacity__maskS2749 = _M0L4selfS927->$3;
      _M0L6_2atmpS2747 = _M0L6_2atmpS2748 & _M0L14capacity__maskS2749;
      _M0L1iS924 = _M0L6_2atmpS2746;
      _M0L3idxS925 = _M0L6_2atmpS2747;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS936,
  moonbit_string_t _M0L3keyS940,
  int32_t _M0L4hashS939
) {
  int32_t _M0L14capacity__maskS2764;
  int32_t _M0L6_2atmpS2763;
  int32_t _M0L1iS933;
  int32_t _M0L3idxS934;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2764 = _M0L4selfS936->$3;
  _M0L6_2atmpS2763 = _M0L4hashS939 & _M0L14capacity__maskS2764;
  _M0L1iS933 = 0;
  _M0L3idxS934 = _M0L6_2atmpS2763;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2762 = _M0L4selfS936->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS935;
    if (
      _M0L3idxS934 < 0
      || _M0L3idxS934 >= Moonbit_array_length(_M0L7entriesS2762)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS935
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2762[_M0L3idxS934];
    if (_M0L7_2abindS935 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS937 = _M0L7_2abindS935;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS938 = _M0L7_2aSomeS937;
      int32_t _M0L4hashS2754 = _M0L8_2aentryS938->$3;
      int32_t _if__result_3939;
      int32_t _M0L3pslS2757;
      int32_t _M0L6_2atmpS2758;
      int32_t _M0L6_2atmpS2760;
      int32_t _M0L14capacity__maskS2761;
      int32_t _M0L6_2atmpS2759;
      if (_M0L4hashS2754 == _M0L4hashS939) {
        moonbit_string_t _M0L3keyS2753 = _M0L8_2aentryS938->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3939
        = _M0L3keyS2753 == _M0L3keyS940
          || Moonbit_array_length(_M0L3keyS2753)
             == Moonbit_array_length(_M0L3keyS940)
             && 0
                == memcmp(_M0L3keyS2753, _M0L3keyS940, Moonbit_array_length(_M0L3keyS2753) * 2);
      } else {
        _if__result_3939 = 0;
      }
      if (_if__result_3939) {
        int32_t _M0L4sizeS2756;
        int32_t _M0L6_2atmpS2755;
        moonbit_incref(_M0L8_2aentryS938);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS936, _M0L8_2aentryS938);
        moonbit_decref(_M0L8_2aentryS938);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS936, _M0L3idxS934);
        _M0L4sizeS2756 = _M0L4selfS936->$1;
        _M0L6_2atmpS2755 = _M0L4sizeS2756 - 1;
        _M0L4selfS936->$1 = _M0L6_2atmpS2755;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS938);
      }
      _M0L3pslS2757 = _M0L8_2aentryS938->$2;
      moonbit_decref(_M0L8_2aentryS938);
      if (_M0L1iS933 > _M0L3pslS2757) {
        break;
      }
      _M0L6_2atmpS2758 = _M0L1iS933 + 1;
      _M0L6_2atmpS2760 = _M0L3idxS934 + 1;
      _M0L14capacity__maskS2761 = _M0L4selfS936->$3;
      _M0L6_2atmpS2759 = _M0L6_2atmpS2760 & _M0L14capacity__maskS2761;
      _M0L1iS933 = _M0L6_2atmpS2758;
      _M0L3idxS934 = _M0L6_2atmpS2759;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS945,
  moonbit_string_t _M0L3keyS949,
  int32_t _M0L4hashS948
) {
  int32_t _M0L14capacity__maskS2776;
  int32_t _M0L6_2atmpS2775;
  int32_t _M0L1iS942;
  int32_t _M0L3idxS943;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2776 = _M0L4selfS945->$3;
  _M0L6_2atmpS2775 = _M0L4hashS948 & _M0L14capacity__maskS2776;
  _M0L1iS942 = 0;
  _M0L3idxS943 = _M0L6_2atmpS2775;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2774 = _M0L4selfS945->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS944;
    if (
      _M0L3idxS943 < 0
      || _M0L3idxS943 >= Moonbit_array_length(_M0L7entriesS2774)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS944
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2774[_M0L3idxS943];
    if (_M0L7_2abindS944 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS946 = _M0L7_2abindS944;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS947 = _M0L7_2aSomeS946;
      int32_t _M0L4hashS2766 = _M0L8_2aentryS947->$3;
      int32_t _if__result_3941;
      int32_t _M0L3pslS2769;
      int32_t _M0L6_2atmpS2770;
      int32_t _M0L6_2atmpS2772;
      int32_t _M0L14capacity__maskS2773;
      int32_t _M0L6_2atmpS2771;
      if (_M0L4hashS2766 == _M0L4hashS948) {
        moonbit_string_t _M0L3keyS2765 = _M0L8_2aentryS947->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3941
        = _M0L3keyS2765 == _M0L3keyS949
          || Moonbit_array_length(_M0L3keyS2765)
             == Moonbit_array_length(_M0L3keyS949)
             && 0
                == memcmp(_M0L3keyS2765, _M0L3keyS949, Moonbit_array_length(_M0L3keyS2765) * 2);
      } else {
        _if__result_3941 = 0;
      }
      if (_if__result_3941) {
        int32_t _M0L4sizeS2768;
        int32_t _M0L6_2atmpS2767;
        moonbit_incref(_M0L8_2aentryS947);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsfE(_M0L4selfS945, _M0L8_2aentryS947);
        moonbit_decref(_M0L8_2aentryS947);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsfE(_M0L4selfS945, _M0L3idxS943);
        _M0L4sizeS2768 = _M0L4selfS945->$1;
        _M0L6_2atmpS2767 = _M0L4sizeS2768 - 1;
        _M0L4selfS945->$1 = _M0L6_2atmpS2767;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS947);
      }
      _M0L3pslS2769 = _M0L8_2aentryS947->$2;
      moonbit_decref(_M0L8_2aentryS947);
      if (_M0L1iS942 > _M0L3pslS2769) {
        break;
      }
      _M0L6_2atmpS2770 = _M0L1iS942 + 1;
      _M0L6_2atmpS2772 = _M0L3idxS943 + 1;
      _M0L14capacity__maskS2773 = _M0L4selfS945->$3;
      _M0L6_2atmpS2771 = _M0L6_2atmpS2772 & _M0L14capacity__maskS2773;
      _M0L1iS942 = _M0L6_2atmpS2770;
      _M0L3idxS943 = _M0L6_2atmpS2771;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS954,
  moonbit_string_t _M0L3keyS958,
  int32_t _M0L4hashS957
) {
  int32_t _M0L14capacity__maskS2788;
  int32_t _M0L6_2atmpS2787;
  int32_t _M0L1iS951;
  int32_t _M0L3idxS952;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2788 = _M0L4selfS954->$3;
  _M0L6_2atmpS2787 = _M0L4hashS957 & _M0L14capacity__maskS2788;
  _M0L1iS951 = 0;
  _M0L3idxS952 = _M0L6_2atmpS2787;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2786 = _M0L4selfS954->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS953;
    if (
      _M0L3idxS952 < 0
      || _M0L3idxS952 >= Moonbit_array_length(_M0L7entriesS2786)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS953
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2786[_M0L3idxS952];
    if (_M0L7_2abindS953 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS955 = _M0L7_2abindS953;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS956 = _M0L7_2aSomeS955;
      int32_t _M0L4hashS2778 = _M0L8_2aentryS956->$3;
      int32_t _if__result_3943;
      int32_t _M0L3pslS2781;
      int32_t _M0L6_2atmpS2782;
      int32_t _M0L6_2atmpS2784;
      int32_t _M0L14capacity__maskS2785;
      int32_t _M0L6_2atmpS2783;
      if (_M0L4hashS2778 == _M0L4hashS957) {
        moonbit_string_t _M0L3keyS2777 = _M0L8_2aentryS956->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3943
        = _M0L3keyS2777 == _M0L3keyS958
          || Moonbit_array_length(_M0L3keyS2777)
             == Moonbit_array_length(_M0L3keyS958)
             && 0
                == memcmp(_M0L3keyS2777, _M0L3keyS958, Moonbit_array_length(_M0L3keyS2777) * 2);
      } else {
        _if__result_3943 = 0;
      }
      if (_if__result_3943) {
        int32_t _M0L4sizeS2780;
        int32_t _M0L6_2atmpS2779;
        moonbit_incref(_M0L8_2aentryS956);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsbE(_M0L4selfS954, _M0L8_2aentryS956);
        moonbit_decref(_M0L8_2aentryS956);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsbE(_M0L4selfS954, _M0L3idxS952);
        _M0L4sizeS2780 = _M0L4selfS954->$1;
        _M0L6_2atmpS2779 = _M0L4sizeS2780 - 1;
        _M0L4selfS954->$1 = _M0L6_2atmpS2779;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS956);
      }
      _M0L3pslS2781 = _M0L8_2aentryS956->$2;
      moonbit_decref(_M0L8_2aentryS956);
      if (_M0L1iS951 > _M0L3pslS2781) {
        break;
      }
      _M0L6_2atmpS2782 = _M0L1iS951 + 1;
      _M0L6_2atmpS2784 = _M0L3idxS952 + 1;
      _M0L14capacity__maskS2785 = _M0L4selfS954->$3;
      _M0L6_2atmpS2783 = _M0L6_2atmpS2784 & _M0L14capacity__maskS2785;
      _M0L1iS951 = _M0L6_2atmpS2782;
      _M0L3idxS952 = _M0L6_2atmpS2783;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS886,
  int32_t _M0L3idxS893
) {
  int32_t _M0L3curS884;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS884 = _M0L3idxS893;
  _2afor_888:;
  while (1) {
    int32_t _M0L6_2atmpS2718 = _M0L3curS884 + 1;
    int32_t _M0L14capacity__maskS2719 = _M0L4selfS886->$3;
    int32_t _M0L4nextS885 = _M0L6_2atmpS2718 & _M0L14capacity__maskS2719;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2717 =
      _M0L4selfS886->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS889;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2713;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2714;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3500;
    int32_t _tmp_3946;
    if (
      _M0L4nextS885 < 0
      || _M0L4nextS885 >= Moonbit_array_length(_M0L7entriesS2717)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS889
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2717[
        _M0L4nextS885
      ];
    if (_M0L7_2abindS889 == 0) {
      goto join_887;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS890 =
        _M0L7_2abindS889;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS891 =
        _M0L7_2aSomeS890;
      int32_t _M0L4_2axS892 = _M0L4_2axS891->$2;
      switch (_M0L4_2axS892) {
        case 0: {
          goto join_887;
          break;
        }
        default: {
          int32_t _M0L3pslS2716 = _M0L4_2axS891->$2;
          int32_t _M0L6_2atmpS2715 = _M0L3pslS2716 - 1;
          _M0L4_2axS891->$2 = _M0L6_2atmpS2715;
          moonbit_incref(_M0L4_2axS891);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS886, _M0L4_2axS891, _M0L3curS884);
          moonbit_decref(_M0L4_2axS891);
          _M0L3curS884 = _M0L4nextS885;
          goto _2afor_888;
          break;
        }
      }
    }
    goto joinlet_3945;
    join_887:;
    _M0L7entriesS2713 = _M0L4selfS886->$0;
    _M0L6_2atmpS2714 = 0;
    if (
      _M0L3curS884 < 0
      || _M0L3curS884 >= Moonbit_array_length(_M0L7entriesS2713)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3500
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2713[
        _M0L3curS884
      ];
    if (_M0L6_2aoldS3500) {
      moonbit_decref(_M0L6_2aoldS3500);
    }
    _M0L7entriesS2713[_M0L3curS884] = _M0L6_2atmpS2714;
    break;
    joinlet_3945:;
    _tmp_3946 = _M0L3curS884;
    _M0L3curS884 = _tmp_3946;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS896,
  int32_t _M0L3idxS903
) {
  int32_t _M0L3curS894;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS894 = _M0L3idxS903;
  _2afor_898:;
  while (1) {
    int32_t _M0L6_2atmpS2725 = _M0L3curS894 + 1;
    int32_t _M0L14capacity__maskS2726 = _M0L4selfS896->$3;
    int32_t _M0L4nextS895 = _M0L6_2atmpS2725 & _M0L14capacity__maskS2726;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2724 = _M0L4selfS896->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS899;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2720;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS2721;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS3504;
    int32_t _tmp_3949;
    if (
      _M0L4nextS895 < 0
      || _M0L4nextS895 >= Moonbit_array_length(_M0L7entriesS2724)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS899
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2724[_M0L4nextS895];
    if (_M0L7_2abindS899 == 0) {
      goto join_897;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS900 = _M0L7_2abindS899;
      struct _M0TPB5EntryGsiE* _M0L4_2axS901 = _M0L7_2aSomeS900;
      int32_t _M0L4_2axS902 = _M0L4_2axS901->$2;
      switch (_M0L4_2axS902) {
        case 0: {
          goto join_897;
          break;
        }
        default: {
          int32_t _M0L3pslS2723 = _M0L4_2axS901->$2;
          int32_t _M0L6_2atmpS2722 = _M0L3pslS2723 - 1;
          _M0L4_2axS901->$2 = _M0L6_2atmpS2722;
          moonbit_incref(_M0L4_2axS901);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsiE(_M0L4selfS896, _M0L4_2axS901, _M0L3curS894);
          moonbit_decref(_M0L4_2axS901);
          _M0L3curS894 = _M0L4nextS895;
          goto _2afor_898;
          break;
        }
      }
    }
    goto joinlet_3948;
    join_897:;
    _M0L7entriesS2720 = _M0L4selfS896->$0;
    _M0L6_2atmpS2721 = 0;
    if (
      _M0L3curS894 < 0
      || _M0L3curS894 >= Moonbit_array_length(_M0L7entriesS2720)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3504
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2720[_M0L3curS894];
    if (_M0L6_2aoldS3504) {
      moonbit_decref(_M0L6_2aoldS3504);
    }
    _M0L7entriesS2720[_M0L3curS894] = _M0L6_2atmpS2721;
    break;
    joinlet_3948:;
    _tmp_3949 = _M0L3curS894;
    _M0L3curS894 = _tmp_3949;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS906,
  int32_t _M0L3idxS913
) {
  int32_t _M0L3curS904;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS904 = _M0L3idxS913;
  _2afor_908:;
  while (1) {
    int32_t _M0L6_2atmpS2732 = _M0L3curS904 + 1;
    int32_t _M0L14capacity__maskS2733 = _M0L4selfS906->$3;
    int32_t _M0L4nextS905 = _M0L6_2atmpS2732 & _M0L14capacity__maskS2733;
    struct _M0TPB5EntryGsfE** _M0L7entriesS2731 = _M0L4selfS906->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS909;
    struct _M0TPB5EntryGsfE** _M0L7entriesS2727;
    struct _M0TPB5EntryGsfE* _M0L6_2atmpS2728;
    struct _M0TPB5EntryGsfE* _M0L6_2aoldS3508;
    int32_t _tmp_3952;
    if (
      _M0L4nextS905 < 0
      || _M0L4nextS905 >= Moonbit_array_length(_M0L7entriesS2731)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS909
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2731[_M0L4nextS905];
    if (_M0L7_2abindS909 == 0) {
      goto join_907;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS910 = _M0L7_2abindS909;
      struct _M0TPB5EntryGsfE* _M0L4_2axS911 = _M0L7_2aSomeS910;
      int32_t _M0L4_2axS912 = _M0L4_2axS911->$2;
      switch (_M0L4_2axS912) {
        case 0: {
          goto join_907;
          break;
        }
        default: {
          int32_t _M0L3pslS2730 = _M0L4_2axS911->$2;
          int32_t _M0L6_2atmpS2729 = _M0L3pslS2730 - 1;
          _M0L4_2axS911->$2 = _M0L6_2atmpS2729;
          moonbit_incref(_M0L4_2axS911);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsfE(_M0L4selfS906, _M0L4_2axS911, _M0L3curS904);
          moonbit_decref(_M0L4_2axS911);
          _M0L3curS904 = _M0L4nextS905;
          goto _2afor_908;
          break;
        }
      }
    }
    goto joinlet_3951;
    join_907:;
    _M0L7entriesS2727 = _M0L4selfS906->$0;
    _M0L6_2atmpS2728 = 0;
    if (
      _M0L3curS904 < 0
      || _M0L3curS904 >= Moonbit_array_length(_M0L7entriesS2727)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3508
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2727[_M0L3curS904];
    if (_M0L6_2aoldS3508) {
      moonbit_decref(_M0L6_2aoldS3508);
    }
    _M0L7entriesS2727[_M0L3curS904] = _M0L6_2atmpS2728;
    break;
    joinlet_3951:;
    _tmp_3952 = _M0L3curS904;
    _M0L3curS904 = _tmp_3952;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS916,
  int32_t _M0L3idxS923
) {
  int32_t _M0L3curS914;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS914 = _M0L3idxS923;
  _2afor_918:;
  while (1) {
    int32_t _M0L6_2atmpS2739 = _M0L3curS914 + 1;
    int32_t _M0L14capacity__maskS2740 = _M0L4selfS916->$3;
    int32_t _M0L4nextS915 = _M0L6_2atmpS2739 & _M0L14capacity__maskS2740;
    struct _M0TPB5EntryGsbE** _M0L7entriesS2738 = _M0L4selfS916->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS919;
    struct _M0TPB5EntryGsbE** _M0L7entriesS2734;
    struct _M0TPB5EntryGsbE* _M0L6_2atmpS2735;
    struct _M0TPB5EntryGsbE* _M0L6_2aoldS3512;
    int32_t _tmp_3955;
    if (
      _M0L4nextS915 < 0
      || _M0L4nextS915 >= Moonbit_array_length(_M0L7entriesS2738)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS919
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2738[_M0L4nextS915];
    if (_M0L7_2abindS919 == 0) {
      goto join_917;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS920 = _M0L7_2abindS919;
      struct _M0TPB5EntryGsbE* _M0L4_2axS921 = _M0L7_2aSomeS920;
      int32_t _M0L4_2axS922 = _M0L4_2axS921->$2;
      switch (_M0L4_2axS922) {
        case 0: {
          goto join_917;
          break;
        }
        default: {
          int32_t _M0L3pslS2737 = _M0L4_2axS921->$2;
          int32_t _M0L6_2atmpS2736 = _M0L3pslS2737 - 1;
          _M0L4_2axS921->$2 = _M0L6_2atmpS2736;
          moonbit_incref(_M0L4_2axS921);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsbE(_M0L4selfS916, _M0L4_2axS921, _M0L3curS914);
          moonbit_decref(_M0L4_2axS921);
          _M0L3curS914 = _M0L4nextS915;
          goto _2afor_918;
          break;
        }
      }
    }
    goto joinlet_3954;
    join_917:;
    _M0L7entriesS2734 = _M0L4selfS916->$0;
    _M0L6_2atmpS2735 = 0;
    if (
      _M0L3curS914 < 0
      || _M0L3curS914 >= Moonbit_array_length(_M0L7entriesS2734)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3512
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2734[_M0L3curS914];
    if (_M0L6_2aoldS3512) {
      moonbit_decref(_M0L6_2aoldS3512);
    }
    _M0L7entriesS2734[_M0L3curS914] = _M0L6_2atmpS2735;
    break;
    joinlet_3954:;
    _tmp_3955 = _M0L3curS914;
    _M0L3curS914 = _tmp_3955;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS862,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS861
) {
  int32_t _M0L7_2abindS860;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS863;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS860 = _M0L5entryS861->$0;
  switch (_M0L7_2abindS860) {
    case -1: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4nextS2685 =
        _M0L5entryS861->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3517 =
        _M0L4selfS862->$5;
      if (_M0L4nextS2685) {
        moonbit_incref(_M0L4nextS2685);
      }
      if (_M0L6_2aoldS3517) {
        moonbit_decref(_M0L6_2aoldS3517);
      }
      _M0L4selfS862->$5 = _M0L4nextS2685;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2689 =
        _M0L4selfS862->$0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2688;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2686;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4nextS2687;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3519;
      if (
        _M0L7_2abindS860 < 0
        || _M0L7_2abindS860 >= Moonbit_array_length(_M0L7entriesS2689)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2688
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2689[
          _M0L7_2abindS860
        ];
      if (_M0L6_2atmpS2688) {
        moonbit_incref(_M0L6_2atmpS2688);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2686
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2688);
      if (_M0L6_2atmpS2688) {
        moonbit_decref(_M0L6_2atmpS2688);
      }
      _M0L4nextS2687 = _M0L5entryS861->$1;
      _M0L6_2aoldS3519 = _M0L6_2atmpS2686->$1;
      if (_M0L4nextS2687) {
        moonbit_incref(_M0L4nextS2687);
      }
      if (_M0L6_2aoldS3519) {
        moonbit_decref(_M0L6_2aoldS3519);
      }
      _M0L6_2atmpS2686->$1 = _M0L4nextS2687;
      moonbit_decref(_M0L6_2atmpS2686);
      break;
    }
  }
  _M0L7_2abindS863 = _M0L5entryS861->$1;
  if (_M0L7_2abindS863 == 0) {
    int32_t _M0L4prevS2690 = _M0L5entryS861->$0;
    _M0L4selfS862->$6 = _M0L4prevS2690;
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS864 =
      _M0L7_2abindS863;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS865 =
      _M0L7_2aSomeS864;
    int32_t _M0L4prevS2691 = _M0L5entryS861->$0;
    _M0L7_2anextS865->$0 = _M0L4prevS2691;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS868,
  struct _M0TPB5EntryGsiE* _M0L5entryS867
) {
  int32_t _M0L7_2abindS866;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS869;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS866 = _M0L5entryS867->$0;
  switch (_M0L7_2abindS866) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L4nextS2692 = _M0L5entryS867->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3524 = _M0L4selfS868->$5;
      if (_M0L4nextS2692) {
        moonbit_incref(_M0L4nextS2692);
      }
      if (_M0L6_2aoldS3524) {
        moonbit_decref(_M0L6_2aoldS3524);
      }
      _M0L4selfS868->$5 = _M0L4nextS2692;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2696 = _M0L4selfS868->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2695;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2693;
      struct _M0TPB5EntryGsiE* _M0L4nextS2694;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3526;
      if (
        _M0L7_2abindS866 < 0
        || _M0L7_2abindS866 >= Moonbit_array_length(_M0L7entriesS2696)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2695
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2696[_M0L7_2abindS866];
      if (_M0L6_2atmpS2695) {
        moonbit_incref(_M0L6_2atmpS2695);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2693
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2695);
      if (_M0L6_2atmpS2695) {
        moonbit_decref(_M0L6_2atmpS2695);
      }
      _M0L4nextS2694 = _M0L5entryS867->$1;
      _M0L6_2aoldS3526 = _M0L6_2atmpS2693->$1;
      if (_M0L4nextS2694) {
        moonbit_incref(_M0L4nextS2694);
      }
      if (_M0L6_2aoldS3526) {
        moonbit_decref(_M0L6_2aoldS3526);
      }
      _M0L6_2atmpS2693->$1 = _M0L4nextS2694;
      moonbit_decref(_M0L6_2atmpS2693);
      break;
    }
  }
  _M0L7_2abindS869 = _M0L5entryS867->$1;
  if (_M0L7_2abindS869 == 0) {
    int32_t _M0L4prevS2697 = _M0L5entryS867->$0;
    _M0L4selfS868->$6 = _M0L4prevS2697;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS870 = _M0L7_2abindS869;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS871 = _M0L7_2aSomeS870;
    int32_t _M0L4prevS2698 = _M0L5entryS867->$0;
    _M0L7_2anextS871->$0 = _M0L4prevS2698;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS874,
  struct _M0TPB5EntryGsfE* _M0L5entryS873
) {
  int32_t _M0L7_2abindS872;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS875;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS872 = _M0L5entryS873->$0;
  switch (_M0L7_2abindS872) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L4nextS2699 = _M0L5entryS873->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3531 = _M0L4selfS874->$5;
      if (_M0L4nextS2699) {
        moonbit_incref(_M0L4nextS2699);
      }
      if (_M0L6_2aoldS3531) {
        moonbit_decref(_M0L6_2aoldS3531);
      }
      _M0L4selfS874->$5 = _M0L4nextS2699;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2703 = _M0L4selfS874->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2702;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2700;
      struct _M0TPB5EntryGsfE* _M0L4nextS2701;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3533;
      if (
        _M0L7_2abindS872 < 0
        || _M0L7_2abindS872 >= Moonbit_array_length(_M0L7entriesS2703)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2702
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2703[_M0L7_2abindS872];
      if (_M0L6_2atmpS2702) {
        moonbit_incref(_M0L6_2atmpS2702);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2700
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2702);
      if (_M0L6_2atmpS2702) {
        moonbit_decref(_M0L6_2atmpS2702);
      }
      _M0L4nextS2701 = _M0L5entryS873->$1;
      _M0L6_2aoldS3533 = _M0L6_2atmpS2700->$1;
      if (_M0L4nextS2701) {
        moonbit_incref(_M0L4nextS2701);
      }
      if (_M0L6_2aoldS3533) {
        moonbit_decref(_M0L6_2aoldS3533);
      }
      _M0L6_2atmpS2700->$1 = _M0L4nextS2701;
      moonbit_decref(_M0L6_2atmpS2700);
      break;
    }
  }
  _M0L7_2abindS875 = _M0L5entryS873->$1;
  if (_M0L7_2abindS875 == 0) {
    int32_t _M0L4prevS2704 = _M0L5entryS873->$0;
    _M0L4selfS874->$6 = _M0L4prevS2704;
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS876 = _M0L7_2abindS875;
    struct _M0TPB5EntryGsfE* _M0L7_2anextS877 = _M0L7_2aSomeS876;
    int32_t _M0L4prevS2705 = _M0L5entryS873->$0;
    _M0L7_2anextS877->$0 = _M0L4prevS2705;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS880,
  struct _M0TPB5EntryGsbE* _M0L5entryS879
) {
  int32_t _M0L7_2abindS878;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS881;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS878 = _M0L5entryS879->$0;
  switch (_M0L7_2abindS878) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L4nextS2706 = _M0L5entryS879->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3538 = _M0L4selfS880->$5;
      if (_M0L4nextS2706) {
        moonbit_incref(_M0L4nextS2706);
      }
      if (_M0L6_2aoldS3538) {
        moonbit_decref(_M0L6_2aoldS3538);
      }
      _M0L4selfS880->$5 = _M0L4nextS2706;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2710 = _M0L4selfS880->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2709;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2707;
      struct _M0TPB5EntryGsbE* _M0L4nextS2708;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3540;
      if (
        _M0L7_2abindS878 < 0
        || _M0L7_2abindS878 >= Moonbit_array_length(_M0L7entriesS2710)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2709
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2710[_M0L7_2abindS878];
      if (_M0L6_2atmpS2709) {
        moonbit_incref(_M0L6_2atmpS2709);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2707
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2709);
      if (_M0L6_2atmpS2709) {
        moonbit_decref(_M0L6_2atmpS2709);
      }
      _M0L4nextS2708 = _M0L5entryS879->$1;
      _M0L6_2aoldS3540 = _M0L6_2atmpS2707->$1;
      if (_M0L4nextS2708) {
        moonbit_incref(_M0L4nextS2708);
      }
      if (_M0L6_2aoldS3540) {
        moonbit_decref(_M0L6_2aoldS3540);
      }
      _M0L6_2atmpS2707->$1 = _M0L4nextS2708;
      moonbit_decref(_M0L6_2atmpS2707);
      break;
    }
  }
  _M0L7_2abindS881 = _M0L5entryS879->$1;
  if (_M0L7_2abindS881 == 0) {
    int32_t _M0L4prevS2711 = _M0L5entryS879->$0;
    _M0L4selfS880->$6 = _M0L4prevS2711;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS882 = _M0L7_2abindS881;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS883 = _M0L7_2aSomeS882;
    int32_t _M0L4prevS2712 = _M0L5entryS879->$0;
    _M0L7_2anextS883->$0 = _M0L4prevS2712;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS829,
  moonbit_string_t _M0L3keyS825
) {
  int32_t _M0L4hashS824;
  int32_t _M0L14capacity__maskS2654;
  int32_t _M0L6_2atmpS2653;
  int32_t _M0L1iS826;
  int32_t _M0L3idxS827;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS824 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS825);
  _M0L14capacity__maskS2654 = _M0L4selfS829->$3;
  _M0L6_2atmpS2653 = _M0L4hashS824 & _M0L14capacity__maskS2654;
  _M0L1iS826 = 0;
  _M0L3idxS827 = _M0L6_2atmpS2653;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2652 = _M0L4selfS829->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS828;
    if (
      _M0L3idxS827 < 0
      || _M0L3idxS827 >= Moonbit_array_length(_M0L7entriesS2652)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS828
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2652[_M0L3idxS827];
    if (_M0L7_2abindS828 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS830 = _M0L7_2abindS828;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS831 = _M0L7_2aSomeS830;
      int32_t _M0L4hashS2646 = _M0L8_2aentryS831->$3;
      int32_t _if__result_3957;
      int32_t _M0L3pslS2647;
      int32_t _M0L6_2atmpS2648;
      int32_t _M0L6_2atmpS2650;
      int32_t _M0L14capacity__maskS2651;
      int32_t _M0L6_2atmpS2649;
      if (_M0L4hashS2646 == _M0L4hashS824) {
        moonbit_string_t _M0L3keyS2645 = _M0L8_2aentryS831->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3957
        = _M0L3keyS2645 == _M0L3keyS825
          || Moonbit_array_length(_M0L3keyS2645)
             == Moonbit_array_length(_M0L3keyS825)
             && 0
                == memcmp(_M0L3keyS2645, _M0L3keyS825, Moonbit_array_length(_M0L3keyS2645) * 2);
      } else {
        _if__result_3957 = 0;
      }
      if (_if__result_3957) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS831);
      }
      _M0L3pslS2647 = _M0L8_2aentryS831->$2;
      moonbit_decref(_M0L8_2aentryS831);
      if (_M0L1iS826 > _M0L3pslS2647) {
        return 0;
      }
      _M0L6_2atmpS2648 = _M0L1iS826 + 1;
      _M0L6_2atmpS2650 = _M0L3idxS827 + 1;
      _M0L14capacity__maskS2651 = _M0L4selfS829->$3;
      _M0L6_2atmpS2649 = _M0L6_2atmpS2650 & _M0L14capacity__maskS2651;
      _M0L1iS826 = _M0L6_2atmpS2648;
      _M0L3idxS827 = _M0L6_2atmpS2649;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS838,
  moonbit_string_t _M0L3keyS834
) {
  int32_t _M0L4hashS833;
  int32_t _M0L14capacity__maskS2664;
  int32_t _M0L6_2atmpS2663;
  int32_t _M0L1iS835;
  int32_t _M0L3idxS836;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS833 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS834);
  _M0L14capacity__maskS2664 = _M0L4selfS838->$3;
  _M0L6_2atmpS2663 = _M0L4hashS833 & _M0L14capacity__maskS2664;
  _M0L1iS835 = 0;
  _M0L3idxS836 = _M0L6_2atmpS2663;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2662 =
      _M0L4selfS838->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS837;
    if (
      _M0L3idxS836 < 0
      || _M0L3idxS836 >= Moonbit_array_length(_M0L7entriesS2662)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS837
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2662[
        _M0L3idxS836
      ];
    if (_M0L7_2abindS837 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS839 =
        _M0L7_2abindS837;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS840 =
        _M0L7_2aSomeS839;
      int32_t _M0L4hashS2656 = _M0L8_2aentryS840->$3;
      int32_t _if__result_3959;
      int32_t _M0L3pslS2657;
      int32_t _M0L6_2atmpS2658;
      int32_t _M0L6_2atmpS2660;
      int32_t _M0L14capacity__maskS2661;
      int32_t _M0L6_2atmpS2659;
      if (_M0L4hashS2656 == _M0L4hashS833) {
        moonbit_string_t _M0L3keyS2655 = _M0L8_2aentryS840->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3959
        = _M0L3keyS2655 == _M0L3keyS834
          || Moonbit_array_length(_M0L3keyS2655)
             == Moonbit_array_length(_M0L3keyS834)
             && 0
                == memcmp(_M0L3keyS2655, _M0L3keyS834, Moonbit_array_length(_M0L3keyS2655) * 2);
      } else {
        _if__result_3959 = 0;
      }
      if (_if__result_3959) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS840);
      }
      _M0L3pslS2657 = _M0L8_2aentryS840->$2;
      moonbit_decref(_M0L8_2aentryS840);
      if (_M0L1iS835 > _M0L3pslS2657) {
        return 0;
      }
      _M0L6_2atmpS2658 = _M0L1iS835 + 1;
      _M0L6_2atmpS2660 = _M0L3idxS836 + 1;
      _M0L14capacity__maskS2661 = _M0L4selfS838->$3;
      _M0L6_2atmpS2659 = _M0L6_2atmpS2660 & _M0L14capacity__maskS2661;
      _M0L1iS835 = _M0L6_2atmpS2658;
      _M0L3idxS836 = _M0L6_2atmpS2659;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS847,
  moonbit_string_t _M0L3keyS843
) {
  int32_t _M0L4hashS842;
  int32_t _M0L14capacity__maskS2674;
  int32_t _M0L6_2atmpS2673;
  int32_t _M0L1iS844;
  int32_t _M0L3idxS845;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS842 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS843);
  _M0L14capacity__maskS2674 = _M0L4selfS847->$3;
  _M0L6_2atmpS2673 = _M0L4hashS842 & _M0L14capacity__maskS2674;
  _M0L1iS844 = 0;
  _M0L3idxS845 = _M0L6_2atmpS2673;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2672 = _M0L4selfS847->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS846;
    if (
      _M0L3idxS845 < 0
      || _M0L3idxS845 >= Moonbit_array_length(_M0L7entriesS2672)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS846
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2672[_M0L3idxS845];
    if (_M0L7_2abindS846 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS848 = _M0L7_2abindS846;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS849 = _M0L7_2aSomeS848;
      int32_t _M0L4hashS2666 = _M0L8_2aentryS849->$3;
      int32_t _if__result_3961;
      int32_t _M0L3pslS2667;
      int32_t _M0L6_2atmpS2668;
      int32_t _M0L6_2atmpS2670;
      int32_t _M0L14capacity__maskS2671;
      int32_t _M0L6_2atmpS2669;
      if (_M0L4hashS2666 == _M0L4hashS842) {
        moonbit_string_t _M0L3keyS2665 = _M0L8_2aentryS849->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3961
        = _M0L3keyS2665 == _M0L3keyS843
          || Moonbit_array_length(_M0L3keyS2665)
             == Moonbit_array_length(_M0L3keyS843)
             && 0
                == memcmp(_M0L3keyS2665, _M0L3keyS843, Moonbit_array_length(_M0L3keyS2665) * 2);
      } else {
        _if__result_3961 = 0;
      }
      if (_if__result_3961) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS849);
      }
      _M0L3pslS2667 = _M0L8_2aentryS849->$2;
      moonbit_decref(_M0L8_2aentryS849);
      if (_M0L1iS844 > _M0L3pslS2667) {
        return 0;
      }
      _M0L6_2atmpS2668 = _M0L1iS844 + 1;
      _M0L6_2atmpS2670 = _M0L3idxS845 + 1;
      _M0L14capacity__maskS2671 = _M0L4selfS847->$3;
      _M0L6_2atmpS2669 = _M0L6_2atmpS2670 & _M0L14capacity__maskS2671;
      _M0L1iS844 = _M0L6_2atmpS2668;
      _M0L3idxS845 = _M0L6_2atmpS2669;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS856,
  moonbit_string_t _M0L3keyS852
) {
  int32_t _M0L4hashS851;
  int32_t _M0L14capacity__maskS2684;
  int32_t _M0L6_2atmpS2683;
  int32_t _M0L1iS853;
  int32_t _M0L3idxS854;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS851 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS852);
  _M0L14capacity__maskS2684 = _M0L4selfS856->$3;
  _M0L6_2atmpS2683 = _M0L4hashS851 & _M0L14capacity__maskS2684;
  _M0L1iS853 = 0;
  _M0L3idxS854 = _M0L6_2atmpS2683;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2682 = _M0L4selfS856->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS855;
    if (
      _M0L3idxS854 < 0
      || _M0L3idxS854 >= Moonbit_array_length(_M0L7entriesS2682)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS855
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2682[_M0L3idxS854];
    if (_M0L7_2abindS855 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS857 = _M0L7_2abindS855;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS858 = _M0L7_2aSomeS857;
      int32_t _M0L4hashS2676 = _M0L8_2aentryS858->$3;
      int32_t _if__result_3963;
      int32_t _M0L3pslS2677;
      int32_t _M0L6_2atmpS2678;
      int32_t _M0L6_2atmpS2680;
      int32_t _M0L14capacity__maskS2681;
      int32_t _M0L6_2atmpS2679;
      if (_M0L4hashS2676 == _M0L4hashS851) {
        moonbit_string_t _M0L3keyS2675 = _M0L8_2aentryS858->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3963
        = _M0L3keyS2675 == _M0L3keyS852
          || Moonbit_array_length(_M0L3keyS2675)
             == Moonbit_array_length(_M0L3keyS852)
             && 0
                == memcmp(_M0L3keyS2675, _M0L3keyS852, Moonbit_array_length(_M0L3keyS2675) * 2);
      } else {
        _if__result_3963 = 0;
      }
      if (_if__result_3963) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS858);
      }
      _M0L3pslS2677 = _M0L8_2aentryS858->$2;
      moonbit_decref(_M0L8_2aentryS858);
      if (_M0L1iS853 > _M0L3pslS2677) {
        return 0;
      }
      _M0L6_2atmpS2678 = _M0L1iS853 + 1;
      _M0L6_2atmpS2680 = _M0L3idxS854 + 1;
      _M0L14capacity__maskS2681 = _M0L4selfS856->$3;
      _M0L6_2atmpS2679 = _M0L6_2atmpS2680 & _M0L14capacity__maskS2681;
      _M0L1iS853 = _M0L6_2atmpS2678;
      _M0L3idxS854 = _M0L6_2atmpS2679;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS793,
  moonbit_string_t _M0L3keyS789
) {
  int32_t _M0L4hashS788;
  int32_t _M0L14capacity__maskS2604;
  int32_t _M0L6_2atmpS2603;
  int32_t _M0L1iS790;
  int32_t _M0L3idxS791;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS788 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS789);
  _M0L14capacity__maskS2604 = _M0L4selfS793->$3;
  _M0L6_2atmpS2603 = _M0L4hashS788 & _M0L14capacity__maskS2604;
  _M0L1iS790 = 0;
  _M0L3idxS791 = _M0L6_2atmpS2603;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2602 =
      _M0L4selfS793->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS792;
    if (
      _M0L3idxS791 < 0
      || _M0L3idxS791 >= Moonbit_array_length(_M0L7entriesS2602)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS792
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2602[
        _M0L3idxS791
      ];
    if (_M0L7_2abindS792 == 0) {
      void* _M0L6_2atmpS2591 = 0;
      return _M0L6_2atmpS2591;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS794 =
        _M0L7_2abindS792;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS795 =
        _M0L7_2aSomeS794;
      int32_t _M0L4hashS2593 = _M0L8_2aentryS795->$3;
      int32_t _if__result_3965;
      int32_t _M0L3pslS2596;
      int32_t _M0L6_2atmpS2598;
      int32_t _M0L6_2atmpS2600;
      int32_t _M0L14capacity__maskS2601;
      int32_t _M0L6_2atmpS2599;
      if (_M0L4hashS2593 == _M0L4hashS788) {
        moonbit_string_t _M0L3keyS2592 = _M0L8_2aentryS795->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3965
        = _M0L3keyS2592 == _M0L3keyS789
          || Moonbit_array_length(_M0L3keyS2592)
             == Moonbit_array_length(_M0L3keyS789)
             && 0
                == memcmp(_M0L3keyS2592, _M0L3keyS789, Moonbit_array_length(_M0L3keyS2592) * 2);
      } else {
        _if__result_3965 = 0;
      }
      if (_if__result_3965) {
        void* _M0L5valueS2595 = _M0L8_2aentryS795->$5;
        void* _M0L6_2atmpS2594;
        moonbit_incref(_M0L5valueS2595);
        _M0L6_2atmpS2594 = _M0L5valueS2595;
        return _M0L6_2atmpS2594;
      } else {
        moonbit_incref(_M0L8_2aentryS795);
      }
      _M0L3pslS2596 = _M0L8_2aentryS795->$2;
      moonbit_decref(_M0L8_2aentryS795);
      if (_M0L1iS790 > _M0L3pslS2596) {
        void* _M0L6_2atmpS2597 = 0;
        return _M0L6_2atmpS2597;
      }
      _M0L6_2atmpS2598 = _M0L1iS790 + 1;
      _M0L6_2atmpS2600 = _M0L3idxS791 + 1;
      _M0L14capacity__maskS2601 = _M0L4selfS793->$3;
      _M0L6_2atmpS2599 = _M0L6_2atmpS2600 & _M0L14capacity__maskS2601;
      _M0L1iS790 = _M0L6_2atmpS2598;
      _M0L3idxS791 = _M0L6_2atmpS2599;
      continue;
    }
    break;
  }
}

moonbit_string_t _M0MPB3Map3getGssE(
  struct _M0TPB3MapGssE* _M0L4selfS802,
  moonbit_string_t _M0L3keyS798
) {
  int32_t _M0L4hashS797;
  int32_t _M0L14capacity__maskS2618;
  int32_t _M0L6_2atmpS2617;
  int32_t _M0L1iS799;
  int32_t _M0L3idxS800;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS797 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS798);
  _M0L14capacity__maskS2618 = _M0L4selfS802->$3;
  _M0L6_2atmpS2617 = _M0L4hashS797 & _M0L14capacity__maskS2618;
  _M0L1iS799 = 0;
  _M0L3idxS800 = _M0L6_2atmpS2617;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2616 = _M0L4selfS802->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS801;
    if (
      _M0L3idxS800 < 0
      || _M0L3idxS800 >= Moonbit_array_length(_M0L7entriesS2616)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS801
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2616[_M0L3idxS800];
    if (_M0L7_2abindS801 == 0) {
      moonbit_string_t _M0L6_2atmpS2605 = 0;
      return _M0L6_2atmpS2605;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS803 = _M0L7_2abindS801;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS804 = _M0L7_2aSomeS803;
      int32_t _M0L4hashS2607 = _M0L8_2aentryS804->$3;
      int32_t _if__result_3967;
      int32_t _M0L3pslS2610;
      int32_t _M0L6_2atmpS2612;
      int32_t _M0L6_2atmpS2614;
      int32_t _M0L14capacity__maskS2615;
      int32_t _M0L6_2atmpS2613;
      if (_M0L4hashS2607 == _M0L4hashS797) {
        moonbit_string_t _M0L3keyS2606 = _M0L8_2aentryS804->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3967
        = _M0L3keyS2606 == _M0L3keyS798
          || Moonbit_array_length(_M0L3keyS2606)
             == Moonbit_array_length(_M0L3keyS798)
             && 0
                == memcmp(_M0L3keyS2606, _M0L3keyS798, Moonbit_array_length(_M0L3keyS2606) * 2);
      } else {
        _if__result_3967 = 0;
      }
      if (_if__result_3967) {
        moonbit_string_t _M0L5valueS2609 = _M0L8_2aentryS804->$5;
        moonbit_string_t _M0L6_2atmpS2608;
        moonbit_incref(_M0L5valueS2609);
        _M0L6_2atmpS2608 = _M0L5valueS2609;
        return _M0L6_2atmpS2608;
      } else {
        moonbit_incref(_M0L8_2aentryS804);
      }
      _M0L3pslS2610 = _M0L8_2aentryS804->$2;
      moonbit_decref(_M0L8_2aentryS804);
      if (_M0L1iS799 > _M0L3pslS2610) {
        moonbit_string_t _M0L6_2atmpS2611 = 0;
        return _M0L6_2atmpS2611;
      }
      _M0L6_2atmpS2612 = _M0L1iS799 + 1;
      _M0L6_2atmpS2614 = _M0L3idxS800 + 1;
      _M0L14capacity__maskS2615 = _M0L4selfS802->$3;
      _M0L6_2atmpS2613 = _M0L6_2atmpS2614 & _M0L14capacity__maskS2615;
      _M0L1iS799 = _M0L6_2atmpS2612;
      _M0L3idxS800 = _M0L6_2atmpS2613;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS811,
  moonbit_string_t _M0L3keyS807
) {
  int32_t _M0L4hashS806;
  int32_t _M0L14capacity__maskS2632;
  int32_t _M0L6_2atmpS2631;
  int32_t _M0L1iS808;
  int32_t _M0L3idxS809;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS806 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS807);
  _M0L14capacity__maskS2632 = _M0L4selfS811->$3;
  _M0L6_2atmpS2631 = _M0L4hashS806 & _M0L14capacity__maskS2632;
  _M0L1iS808 = 0;
  _M0L3idxS809 = _M0L6_2atmpS2631;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2630 = _M0L4selfS811->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS810;
    if (
      _M0L3idxS809 < 0
      || _M0L3idxS809 >= Moonbit_array_length(_M0L7entriesS2630)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS810
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2630[_M0L3idxS809];
    if (_M0L7_2abindS810 == 0) {
      void* _M0L4NoneS2619 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS2619;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS812 = _M0L7_2abindS810;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS813 = _M0L7_2aSomeS812;
      int32_t _M0L4hashS2621 = _M0L8_2aentryS813->$3;
      int32_t _if__result_3969;
      int32_t _M0L3pslS2624;
      int32_t _M0L6_2atmpS2626;
      int32_t _M0L6_2atmpS2628;
      int32_t _M0L14capacity__maskS2629;
      int32_t _M0L6_2atmpS2627;
      if (_M0L4hashS2621 == _M0L4hashS806) {
        moonbit_string_t _M0L3keyS2620 = _M0L8_2aentryS813->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3969
        = _M0L3keyS2620 == _M0L3keyS807
          || Moonbit_array_length(_M0L3keyS2620)
             == Moonbit_array_length(_M0L3keyS807)
             && 0
                == memcmp(_M0L3keyS2620, _M0L3keyS807, Moonbit_array_length(_M0L3keyS2620) * 2);
      } else {
        _if__result_3969 = 0;
      }
      if (_if__result_3969) {
        float _M0L5valueS2623 = _M0L8_2aentryS813->$5;
        void* _M0L4SomeS2622 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS2622)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS2622)->$0
        = _M0L5valueS2623;
        return _M0L4SomeS2622;
      } else {
        moonbit_incref(_M0L8_2aentryS813);
      }
      _M0L3pslS2624 = _M0L8_2aentryS813->$2;
      moonbit_decref(_M0L8_2aentryS813);
      if (_M0L1iS808 > _M0L3pslS2624) {
        void* _M0L4NoneS2625 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS2625;
      }
      _M0L6_2atmpS2626 = _M0L1iS808 + 1;
      _M0L6_2atmpS2628 = _M0L3idxS809 + 1;
      _M0L14capacity__maskS2629 = _M0L4selfS811->$3;
      _M0L6_2atmpS2627 = _M0L6_2atmpS2628 & _M0L14capacity__maskS2629;
      _M0L1iS808 = _M0L6_2atmpS2626;
      _M0L3idxS809 = _M0L6_2atmpS2627;
      continue;
    }
    break;
  }
}

int64_t _M0MPB3Map3getGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS820,
  moonbit_string_t _M0L3keyS816
) {
  int32_t _M0L4hashS815;
  int32_t _M0L14capacity__maskS2644;
  int32_t _M0L6_2atmpS2643;
  int32_t _M0L1iS817;
  int32_t _M0L3idxS818;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS815 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS816);
  _M0L14capacity__maskS2644 = _M0L4selfS820->$3;
  _M0L6_2atmpS2643 = _M0L4hashS815 & _M0L14capacity__maskS2644;
  _M0L1iS817 = 0;
  _M0L3idxS818 = _M0L6_2atmpS2643;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2642 = _M0L4selfS820->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS819;
    if (
      _M0L3idxS818 < 0
      || _M0L3idxS818 >= Moonbit_array_length(_M0L7entriesS2642)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS819
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2642[_M0L3idxS818];
    if (_M0L7_2abindS819 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS821 = _M0L7_2abindS819;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS822 = _M0L7_2aSomeS821;
      int32_t _M0L4hashS2634 = _M0L8_2aentryS822->$3;
      int32_t _if__result_3971;
      int32_t _M0L3pslS2637;
      int32_t _M0L6_2atmpS2638;
      int32_t _M0L6_2atmpS2640;
      int32_t _M0L14capacity__maskS2641;
      int32_t _M0L6_2atmpS2639;
      if (_M0L4hashS2634 == _M0L4hashS815) {
        moonbit_string_t _M0L3keyS2633 = _M0L8_2aentryS822->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3971
        = _M0L3keyS2633 == _M0L3keyS816
          || Moonbit_array_length(_M0L3keyS2633)
             == Moonbit_array_length(_M0L3keyS816)
             && 0
                == memcmp(_M0L3keyS2633, _M0L3keyS816, Moonbit_array_length(_M0L3keyS2633) * 2);
      } else {
        _if__result_3971 = 0;
      }
      if (_if__result_3971) {
        int32_t _M0L5valueS2636 = _M0L8_2aentryS822->$5;
        int64_t _M0L6_2atmpS2635 = (int64_t)_M0L5valueS2636;
        return _M0L6_2atmpS2635;
      } else {
        moonbit_incref(_M0L8_2aentryS822);
      }
      _M0L3pslS2637 = _M0L8_2aentryS822->$2;
      moonbit_decref(_M0L8_2aentryS822);
      if (_M0L1iS817 > _M0L3pslS2637) {
        return 4294967296ll;
      }
      _M0L6_2atmpS2638 = _M0L1iS817 + 1;
      _M0L6_2atmpS2640 = _M0L3idxS818 + 1;
      _M0L14capacity__maskS2641 = _M0L4selfS820->$3;
      _M0L6_2atmpS2639 = _M0L6_2atmpS2640 & _M0L14capacity__maskS2641;
      _M0L1iS817 = _M0L6_2atmpS2638;
      _M0L3idxS818 = _M0L6_2atmpS2639;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L3arrS734,
  int64_t _M0L8capacityS736
) {
  int32_t _M0L3endS2545;
  int32_t _M0L5startS2546;
  int32_t _M0L6lengthS733;
  int32_t _M0L8capacityS735;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1mS739;
  int32_t _M0L3endS2542;
  int32_t _M0L5startS2543;
  int32_t _M0L7_2abindS740;
  int32_t _M0L2__S741;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2545 = _M0L3arrS734.$2;
  _M0L5startS2546 = _M0L3arrS734.$1;
  _M0L6lengthS733 = _M0L3endS2545 - _M0L5startS2546;
  if (_M0L8capacityS736 == 4294967296ll) {
    if (_M0L6lengthS733 == 0) {
      _M0L8capacityS735 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS735 = _M0FPB21capacity__for__length(_M0L6lengthS733);
    }
  } else {
    int64_t _M0L7_2aSomeS737 = _M0L8capacityS736;
    int32_t _M0L11_2acapacityS738 = (int32_t)_M0L7_2aSomeS737;
    int32_t _M0L6_2atmpS2544;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2544 = _M0FPB21capacity__for__length(_M0L6lengthS733);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS735
    = _M0MPC13int3Int3max(_M0L11_2acapacityS738, _M0L6_2atmpS2544);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS739
  = _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L8capacityS735);
  _M0L3endS2542 = _M0L3arrS734.$2;
  _M0L5startS2543 = _M0L3arrS734.$1;
  _M0L7_2abindS740 = _M0L3endS2542 - _M0L5startS2543;
  _M0L2__S741 = 0;
  while (1) {
    if (_M0L2__S741 < _M0L7_2abindS740) {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L3bufS2539 =
        _M0L3arrS734.$0;
      int32_t _M0L5startS2541 = _M0L3arrS734.$1;
      int32_t _M0L6_2atmpS2540 = _M0L5startS2541 + _M0L2__S741;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1eS742 =
        (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L3bufS2539[
          _M0L6_2atmpS2540
        ];
      moonbit_string_t _M0L6_2atmpS2536 = _M0L1eS742->$0;
      void* _M0L6_2atmpS2537 = _M0L1eS742->$1;
      int32_t _M0L6_2atmpS2538;
      moonbit_incref(_M0L6_2atmpS2537);
      moonbit_incref(_M0L6_2atmpS2536);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L1mS739, _M0L6_2atmpS2536, _M0L6_2atmpS2537);
      moonbit_decref(_M0L6_2atmpS2536);
      moonbit_decref(_M0L6_2atmpS2537);
      _M0L6_2atmpS2538 = _M0L2__S741 + 1;
      _M0L2__S741 = _M0L6_2atmpS2538;
      continue;
    }
    break;
  }
  return _M0L1mS739;
}

struct _M0TPB3MapGsiE* _M0MPB3Map3MapGsiE(
  struct _M0TPB9ArrayViewGUsiEE _M0L3arrS745,
  int64_t _M0L8capacityS747
) {
  int32_t _M0L3endS2556;
  int32_t _M0L5startS2557;
  int32_t _M0L6lengthS744;
  int32_t _M0L8capacityS746;
  struct _M0TPB3MapGsiE* _M0L1mS750;
  int32_t _M0L3endS2553;
  int32_t _M0L5startS2554;
  int32_t _M0L7_2abindS751;
  int32_t _M0L2__S752;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2556 = _M0L3arrS745.$2;
  _M0L5startS2557 = _M0L3arrS745.$1;
  _M0L6lengthS744 = _M0L3endS2556 - _M0L5startS2557;
  if (_M0L8capacityS747 == 4294967296ll) {
    if (_M0L6lengthS744 == 0) {
      _M0L8capacityS746 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS746 = _M0FPB21capacity__for__length(_M0L6lengthS744);
    }
  } else {
    int64_t _M0L7_2aSomeS748 = _M0L8capacityS747;
    int32_t _M0L11_2acapacityS749 = (int32_t)_M0L7_2aSomeS748;
    int32_t _M0L6_2atmpS2555;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2555 = _M0FPB21capacity__for__length(_M0L6lengthS744);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS746
    = _M0MPC13int3Int3max(_M0L11_2acapacityS749, _M0L6_2atmpS2555);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS750 = _M0FPB8new__mapGsiE(_M0L8capacityS746);
  _M0L3endS2553 = _M0L3arrS745.$2;
  _M0L5startS2554 = _M0L3arrS745.$1;
  _M0L7_2abindS751 = _M0L3endS2553 - _M0L5startS2554;
  _M0L2__S752 = 0;
  while (1) {
    if (_M0L2__S752 < _M0L7_2abindS751) {
      struct _M0TUsiE** _M0L3bufS2550 = _M0L3arrS745.$0;
      int32_t _M0L5startS2552 = _M0L3arrS745.$1;
      int32_t _M0L6_2atmpS2551 = _M0L5startS2552 + _M0L2__S752;
      struct _M0TUsiE* _M0L1eS753 =
        (struct _M0TUsiE*)_M0L3bufS2550[_M0L6_2atmpS2551];
      moonbit_string_t _M0L6_2atmpS2547 = _M0L1eS753->$0;
      int32_t _M0L6_2atmpS2548 = _M0L1eS753->$1;
      int32_t _M0L6_2atmpS2549;
      moonbit_incref(_M0L6_2atmpS2547);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS750, _M0L6_2atmpS2547, _M0L6_2atmpS2548);
      moonbit_decref(_M0L6_2atmpS2547);
      _M0L6_2atmpS2549 = _M0L2__S752 + 1;
      _M0L2__S752 = _M0L6_2atmpS2549;
      continue;
    }
    break;
  }
  return _M0L1mS750;
}

struct _M0TPB3MapGsfE* _M0MPB3Map3MapGsfE(
  struct _M0TPB9ArrayViewGUsfEE _M0L3arrS756,
  int64_t _M0L8capacityS758
) {
  int32_t _M0L3endS2567;
  int32_t _M0L5startS2568;
  int32_t _M0L6lengthS755;
  int32_t _M0L8capacityS757;
  struct _M0TPB3MapGsfE* _M0L1mS761;
  int32_t _M0L3endS2564;
  int32_t _M0L5startS2565;
  int32_t _M0L7_2abindS762;
  int32_t _M0L2__S763;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2567 = _M0L3arrS756.$2;
  _M0L5startS2568 = _M0L3arrS756.$1;
  _M0L6lengthS755 = _M0L3endS2567 - _M0L5startS2568;
  if (_M0L8capacityS758 == 4294967296ll) {
    if (_M0L6lengthS755 == 0) {
      _M0L8capacityS757 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS757 = _M0FPB21capacity__for__length(_M0L6lengthS755);
    }
  } else {
    int64_t _M0L7_2aSomeS759 = _M0L8capacityS758;
    int32_t _M0L11_2acapacityS760 = (int32_t)_M0L7_2aSomeS759;
    int32_t _M0L6_2atmpS2566;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2566 = _M0FPB21capacity__for__length(_M0L6lengthS755);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS757
    = _M0MPC13int3Int3max(_M0L11_2acapacityS760, _M0L6_2atmpS2566);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS761 = _M0FPB8new__mapGsfE(_M0L8capacityS757);
  _M0L3endS2564 = _M0L3arrS756.$2;
  _M0L5startS2565 = _M0L3arrS756.$1;
  _M0L7_2abindS762 = _M0L3endS2564 - _M0L5startS2565;
  _M0L2__S763 = 0;
  while (1) {
    if (_M0L2__S763 < _M0L7_2abindS762) {
      struct _M0TUsfE** _M0L3bufS2561 = _M0L3arrS756.$0;
      int32_t _M0L5startS2563 = _M0L3arrS756.$1;
      int32_t _M0L6_2atmpS2562 = _M0L5startS2563 + _M0L2__S763;
      struct _M0TUsfE* _M0L1eS764 =
        (struct _M0TUsfE*)_M0L3bufS2561[_M0L6_2atmpS2562];
      moonbit_string_t _M0L6_2atmpS2558 = _M0L1eS764->$0;
      float _M0L6_2atmpS2559 = _M0L1eS764->$1;
      int32_t _M0L6_2atmpS2560;
      moonbit_incref(_M0L6_2atmpS2558);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS761, _M0L6_2atmpS2558, _M0L6_2atmpS2559);
      moonbit_decref(_M0L6_2atmpS2558);
      _M0L6_2atmpS2560 = _M0L2__S763 + 1;
      _M0L2__S763 = _M0L6_2atmpS2560;
      continue;
    }
    break;
  }
  return _M0L1mS761;
}

struct _M0TPB3MapGssE* _M0MPB3Map3MapGssE(
  struct _M0TPB9ArrayViewGUssEE _M0L3arrS767,
  int64_t _M0L8capacityS769
) {
  int32_t _M0L3endS2578;
  int32_t _M0L5startS2579;
  int32_t _M0L6lengthS766;
  int32_t _M0L8capacityS768;
  struct _M0TPB3MapGssE* _M0L1mS772;
  int32_t _M0L3endS2575;
  int32_t _M0L5startS2576;
  int32_t _M0L7_2abindS773;
  int32_t _M0L2__S774;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2578 = _M0L3arrS767.$2;
  _M0L5startS2579 = _M0L3arrS767.$1;
  _M0L6lengthS766 = _M0L3endS2578 - _M0L5startS2579;
  if (_M0L8capacityS769 == 4294967296ll) {
    if (_M0L6lengthS766 == 0) {
      _M0L8capacityS768 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS768 = _M0FPB21capacity__for__length(_M0L6lengthS766);
    }
  } else {
    int64_t _M0L7_2aSomeS770 = _M0L8capacityS769;
    int32_t _M0L11_2acapacityS771 = (int32_t)_M0L7_2aSomeS770;
    int32_t _M0L6_2atmpS2577;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2577 = _M0FPB21capacity__for__length(_M0L6lengthS766);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS768
    = _M0MPC13int3Int3max(_M0L11_2acapacityS771, _M0L6_2atmpS2577);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS772 = _M0FPB8new__mapGssE(_M0L8capacityS768);
  _M0L3endS2575 = _M0L3arrS767.$2;
  _M0L5startS2576 = _M0L3arrS767.$1;
  _M0L7_2abindS773 = _M0L3endS2575 - _M0L5startS2576;
  _M0L2__S774 = 0;
  while (1) {
    if (_M0L2__S774 < _M0L7_2abindS773) {
      struct _M0TUssE** _M0L3bufS2572 = _M0L3arrS767.$0;
      int32_t _M0L5startS2574 = _M0L3arrS767.$1;
      int32_t _M0L6_2atmpS2573 = _M0L5startS2574 + _M0L2__S774;
      struct _M0TUssE* _M0L1eS775 =
        (struct _M0TUssE*)_M0L3bufS2572[_M0L6_2atmpS2573];
      moonbit_string_t _M0L6_2atmpS2569 = _M0L1eS775->$0;
      moonbit_string_t _M0L6_2atmpS2570 = _M0L1eS775->$1;
      int32_t _M0L6_2atmpS2571;
      moonbit_incref(_M0L6_2atmpS2570);
      moonbit_incref(_M0L6_2atmpS2569);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS772, _M0L6_2atmpS2569, _M0L6_2atmpS2570);
      moonbit_decref(_M0L6_2atmpS2569);
      moonbit_decref(_M0L6_2atmpS2570);
      _M0L6_2atmpS2571 = _M0L2__S774 + 1;
      _M0L2__S774 = _M0L6_2atmpS2571;
      continue;
    }
    break;
  }
  return _M0L1mS772;
}

struct _M0TPB3MapGsbE* _M0MPB3Map3MapGsbE(
  struct _M0TPB9ArrayViewGUsbEE _M0L3arrS778,
  int64_t _M0L8capacityS780
) {
  int32_t _M0L3endS2589;
  int32_t _M0L5startS2590;
  int32_t _M0L6lengthS777;
  int32_t _M0L8capacityS779;
  struct _M0TPB3MapGsbE* _M0L1mS783;
  int32_t _M0L3endS2586;
  int32_t _M0L5startS2587;
  int32_t _M0L7_2abindS784;
  int32_t _M0L2__S785;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2589 = _M0L3arrS778.$2;
  _M0L5startS2590 = _M0L3arrS778.$1;
  _M0L6lengthS777 = _M0L3endS2589 - _M0L5startS2590;
  if (_M0L8capacityS780 == 4294967296ll) {
    if (_M0L6lengthS777 == 0) {
      _M0L8capacityS779 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS779 = _M0FPB21capacity__for__length(_M0L6lengthS777);
    }
  } else {
    int64_t _M0L7_2aSomeS781 = _M0L8capacityS780;
    int32_t _M0L11_2acapacityS782 = (int32_t)_M0L7_2aSomeS781;
    int32_t _M0L6_2atmpS2588;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2588 = _M0FPB21capacity__for__length(_M0L6lengthS777);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS779
    = _M0MPC13int3Int3max(_M0L11_2acapacityS782, _M0L6_2atmpS2588);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS783 = _M0FPB8new__mapGsbE(_M0L8capacityS779);
  _M0L3endS2586 = _M0L3arrS778.$2;
  _M0L5startS2587 = _M0L3arrS778.$1;
  _M0L7_2abindS784 = _M0L3endS2586 - _M0L5startS2587;
  _M0L2__S785 = 0;
  while (1) {
    if (_M0L2__S785 < _M0L7_2abindS784) {
      struct _M0TUsbE** _M0L3bufS2583 = _M0L3arrS778.$0;
      int32_t _M0L5startS2585 = _M0L3arrS778.$1;
      int32_t _M0L6_2atmpS2584 = _M0L5startS2585 + _M0L2__S785;
      struct _M0TUsbE* _M0L1eS786 =
        (struct _M0TUsbE*)_M0L3bufS2583[_M0L6_2atmpS2584];
      moonbit_string_t _M0L6_2atmpS2580 = _M0L1eS786->$0;
      int32_t _M0L6_2atmpS2581 = _M0L1eS786->$1;
      int32_t _M0L6_2atmpS2582;
      moonbit_incref(_M0L6_2atmpS2580);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS783, _M0L6_2atmpS2580, _M0L6_2atmpS2581);
      moonbit_decref(_M0L6_2atmpS2580);
      _M0L6_2atmpS2582 = _M0L2__S785 + 1;
      _M0L2__S785 = _M0L6_2atmpS2582;
      continue;
    }
    break;
  }
  return _M0L1mS783;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS718,
  moonbit_string_t _M0L3keyS719,
  float _M0L5valueS720
) {
  int32_t _M0L6_2atmpS2531;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2531 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS719);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS718, _M0L3keyS719, _M0L5valueS720, _M0L6_2atmpS2531);
  return 0;
}

int32_t _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS721,
  moonbit_string_t _M0L3keyS722,
  void* _M0L5valueS723
) {
  int32_t _M0L6_2atmpS2532;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2532 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS722);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS721, _M0L3keyS722, _M0L5valueS723, _M0L6_2atmpS2532);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS724,
  moonbit_string_t _M0L3keyS725,
  moonbit_string_t _M0L5valueS726
) {
  int32_t _M0L6_2atmpS2533;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2533 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS725);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS724, _M0L3keyS725, _M0L5valueS726, _M0L6_2atmpS2533);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS727,
  moonbit_string_t _M0L3keyS728,
  int32_t _M0L5valueS729
) {
  int32_t _M0L6_2atmpS2534;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2534 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS728);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS727, _M0L3keyS728, _M0L5valueS729, _M0L6_2atmpS2534);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS730,
  moonbit_string_t _M0L3keyS731,
  int32_t _M0L5valueS732
) {
  int32_t _M0L6_2atmpS2535;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2535 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS731);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS730, _M0L3keyS731, _M0L5valueS732, _M0L6_2atmpS2535);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS641,
  moonbit_string_t _M0L3keyS647,
  float _M0L5valueS648,
  int32_t _M0L4hashS643
) {
  int32_t _M0L14capacity__maskS2458;
  int32_t _M0L6_2atmpS2457;
  int32_t _M0L3pslS638;
  int32_t _M0L3idxS639;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2458 = _M0L4selfS641->$3;
  _M0L6_2atmpS2457 = _M0L4hashS643 & _M0L14capacity__maskS2458;
  _M0L3pslS638 = 0;
  _M0L3idxS639 = _M0L6_2atmpS2457;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2456 = _M0L4selfS641->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS640;
    if (
      _M0L3idxS639 < 0
      || _M0L3idxS639 >= Moonbit_array_length(_M0L7entriesS2456)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS640
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2456[_M0L3idxS639];
    if (_M0L7_2abindS640 == 0) {
      int32_t _M0L4sizeS2441 = _M0L4selfS641->$1;
      int32_t _M0L8grow__atS2442 = _M0L4selfS641->$4;
      int32_t _M0L7_2abindS644;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS645;
      struct _M0TPB5EntryGsfE* _M0L5entryS646;
      if (_M0L4sizeS2441 >= _M0L8grow__atS2442) {
        int32_t _M0L14capacity__maskS2444;
        int32_t _M0L6_2atmpS2443;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS641);
        _M0L14capacity__maskS2444 = _M0L4selfS641->$3;
        _M0L6_2atmpS2443 = _M0L4hashS643 & _M0L14capacity__maskS2444;
        _M0L3pslS638 = 0;
        _M0L3idxS639 = _M0L6_2atmpS2443;
        continue;
      }
      _M0L7_2abindS644 = _M0L4selfS641->$6;
      _M0L7_2abindS645 = 0;
      moonbit_incref(_M0L3keyS647);
      _M0L5entryS646
      = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
      Moonbit_object_header(_M0L5entryS646)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 61, 0);
      _M0L5entryS646->$0 = _M0L7_2abindS644;
      _M0L5entryS646->$1 = _M0L7_2abindS645;
      _M0L5entryS646->$2 = _M0L3pslS638;
      _M0L5entryS646->$3 = _M0L4hashS643;
      _M0L5entryS646->$4 = _M0L3keyS647;
      _M0L5entryS646->$5 = _M0L5valueS648;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5entryS646);
      moonbit_decref(_M0L5entryS646);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS649 = _M0L7_2abindS640;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS650 = _M0L7_2aSomeS649;
      int32_t _M0L4hashS2446 = _M0L14_2acurr__entryS650->$3;
      int32_t _if__result_3978;
      int32_t _M0L3pslS2447;
      int32_t _M0L6_2atmpS2452;
      int32_t _M0L6_2atmpS2454;
      int32_t _M0L14capacity__maskS2455;
      int32_t _M0L6_2atmpS2453;
      if (_M0L4hashS2446 == _M0L4hashS643) {
        moonbit_string_t _M0L3keyS2445 = _M0L14_2acurr__entryS650->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3978
        = _M0L3keyS2445 == _M0L3keyS647
          || Moonbit_array_length(_M0L3keyS2445)
             == Moonbit_array_length(_M0L3keyS647)
             && 0
                == memcmp(_M0L3keyS2445, _M0L3keyS647, Moonbit_array_length(_M0L3keyS2445) * 2);
      } else {
        _if__result_3978 = 0;
      }
      if (_if__result_3978) {
        _M0L14_2acurr__entryS650->$5 = _M0L5valueS648;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS650);
      }
      _M0L3pslS2447 = _M0L14_2acurr__entryS650->$2;
      if (_M0L3pslS638 > _M0L3pslS2447) {
        int32_t _M0L4sizeS2448 = _M0L4selfS641->$1;
        int32_t _M0L8grow__atS2449 = _M0L4selfS641->$4;
        int32_t _M0L7_2abindS651;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS652;
        struct _M0TPB5EntryGsfE* _M0L5entryS653;
        if (_M0L4sizeS2448 >= _M0L8grow__atS2449) {
          int32_t _M0L14capacity__maskS2451;
          int32_t _M0L6_2atmpS2450;
          moonbit_decref(_M0L14_2acurr__entryS650);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS641);
          _M0L14capacity__maskS2451 = _M0L4selfS641->$3;
          _M0L6_2atmpS2450 = _M0L4hashS643 & _M0L14capacity__maskS2451;
          _M0L3pslS638 = 0;
          _M0L3idxS639 = _M0L6_2atmpS2450;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS641, _M0L3idxS639, _M0L14_2acurr__entryS650);
        moonbit_decref(_M0L14_2acurr__entryS650);
        _M0L7_2abindS651 = _M0L4selfS641->$6;
        _M0L7_2abindS652 = 0;
        moonbit_incref(_M0L3keyS647);
        _M0L5entryS653
        = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
        Moonbit_object_header(_M0L5entryS653)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 61, 0);
        _M0L5entryS653->$0 = _M0L7_2abindS651;
        _M0L5entryS653->$1 = _M0L7_2abindS652;
        _M0L5entryS653->$2 = _M0L3pslS638;
        _M0L5entryS653->$3 = _M0L4hashS643;
        _M0L5entryS653->$4 = _M0L3keyS647;
        _M0L5entryS653->$5 = _M0L5valueS648;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5entryS653);
        moonbit_decref(_M0L5entryS653);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS650);
      }
      _M0L6_2atmpS2452 = _M0L3pslS638 + 1;
      _M0L6_2atmpS2454 = _M0L3idxS639 + 1;
      _M0L14capacity__maskS2455 = _M0L4selfS641->$3;
      _M0L6_2atmpS2453 = _M0L6_2atmpS2454 & _M0L14capacity__maskS2455;
      _M0L3pslS638 = _M0L6_2atmpS2452;
      _M0L3idxS639 = _M0L6_2atmpS2453;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS657,
  moonbit_string_t _M0L3keyS663,
  void* _M0L5valueS664,
  int32_t _M0L4hashS659
) {
  int32_t _M0L14capacity__maskS2476;
  int32_t _M0L6_2atmpS2475;
  int32_t _M0L3pslS654;
  int32_t _M0L3idxS655;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2476 = _M0L4selfS657->$3;
  _M0L6_2atmpS2475 = _M0L4hashS659 & _M0L14capacity__maskS2476;
  _M0L3pslS654 = 0;
  _M0L3idxS655 = _M0L6_2atmpS2475;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2474 =
      _M0L4selfS657->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS656;
    if (
      _M0L3idxS655 < 0
      || _M0L3idxS655 >= Moonbit_array_length(_M0L7entriesS2474)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS656
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2474[
        _M0L3idxS655
      ];
    if (_M0L7_2abindS656 == 0) {
      int32_t _M0L4sizeS2459 = _M0L4selfS657->$1;
      int32_t _M0L8grow__atS2460 = _M0L4selfS657->$4;
      int32_t _M0L7_2abindS660;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS661;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS662;
      if (_M0L4sizeS2459 >= _M0L8grow__atS2460) {
        int32_t _M0L14capacity__maskS2462;
        int32_t _M0L6_2atmpS2461;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS657);
        _M0L14capacity__maskS2462 = _M0L4selfS657->$3;
        _M0L6_2atmpS2461 = _M0L4hashS659 & _M0L14capacity__maskS2462;
        _M0L3pslS654 = 0;
        _M0L3idxS655 = _M0L6_2atmpS2461;
        continue;
      }
      _M0L7_2abindS660 = _M0L4selfS657->$6;
      _M0L7_2abindS661 = 0;
      moonbit_incref(_M0L3keyS663);
      moonbit_incref(_M0L5valueS664);
      _M0L5entryS662
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L5entryS662)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 65, 0);
      _M0L5entryS662->$0 = _M0L7_2abindS660;
      _M0L5entryS662->$1 = _M0L7_2abindS661;
      _M0L5entryS662->$2 = _M0L3pslS654;
      _M0L5entryS662->$3 = _M0L4hashS659;
      _M0L5entryS662->$4 = _M0L3keyS663;
      _M0L5entryS662->$5 = _M0L5valueS664;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS657, _M0L3idxS655, _M0L5entryS662);
      moonbit_decref(_M0L5entryS662);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS665 =
        _M0L7_2abindS656;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS666 =
        _M0L7_2aSomeS665;
      int32_t _M0L4hashS2464 = _M0L14_2acurr__entryS666->$3;
      int32_t _if__result_3980;
      int32_t _M0L3pslS2465;
      int32_t _M0L6_2atmpS2470;
      int32_t _M0L6_2atmpS2472;
      int32_t _M0L14capacity__maskS2473;
      int32_t _M0L6_2atmpS2471;
      if (_M0L4hashS2464 == _M0L4hashS659) {
        moonbit_string_t _M0L3keyS2463 = _M0L14_2acurr__entryS666->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3980
        = _M0L3keyS2463 == _M0L3keyS663
          || Moonbit_array_length(_M0L3keyS2463)
             == Moonbit_array_length(_M0L3keyS663)
             && 0
                == memcmp(_M0L3keyS2463, _M0L3keyS663, Moonbit_array_length(_M0L3keyS2463) * 2);
      } else {
        _if__result_3980 = 0;
      }
      if (_if__result_3980) {
        void* _M0L6_2aoldS3590 = _M0L14_2acurr__entryS666->$5;
        moonbit_incref(_M0L5valueS664);
        moonbit_decref(_M0L6_2aoldS3590);
        _M0L14_2acurr__entryS666->$5 = _M0L5valueS664;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS666);
      }
      _M0L3pslS2465 = _M0L14_2acurr__entryS666->$2;
      if (_M0L3pslS654 > _M0L3pslS2465) {
        int32_t _M0L4sizeS2466 = _M0L4selfS657->$1;
        int32_t _M0L8grow__atS2467 = _M0L4selfS657->$4;
        int32_t _M0L7_2abindS667;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS668;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS669;
        if (_M0L4sizeS2466 >= _M0L8grow__atS2467) {
          int32_t _M0L14capacity__maskS2469;
          int32_t _M0L6_2atmpS2468;
          moonbit_decref(_M0L14_2acurr__entryS666);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS657);
          _M0L14capacity__maskS2469 = _M0L4selfS657->$3;
          _M0L6_2atmpS2468 = _M0L4hashS659 & _M0L14capacity__maskS2469;
          _M0L3pslS654 = 0;
          _M0L3idxS655 = _M0L6_2atmpS2468;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS657, _M0L3idxS655, _M0L14_2acurr__entryS666);
        moonbit_decref(_M0L14_2acurr__entryS666);
        _M0L7_2abindS667 = _M0L4selfS657->$6;
        _M0L7_2abindS668 = 0;
        moonbit_incref(_M0L3keyS663);
        moonbit_incref(_M0L5valueS664);
        _M0L5entryS669
        = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
        Moonbit_object_header(_M0L5entryS669)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 65, 0);
        _M0L5entryS669->$0 = _M0L7_2abindS667;
        _M0L5entryS669->$1 = _M0L7_2abindS668;
        _M0L5entryS669->$2 = _M0L3pslS654;
        _M0L5entryS669->$3 = _M0L4hashS659;
        _M0L5entryS669->$4 = _M0L3keyS663;
        _M0L5entryS669->$5 = _M0L5valueS664;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS657, _M0L3idxS655, _M0L5entryS669);
        moonbit_decref(_M0L5entryS669);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS666);
      }
      _M0L6_2atmpS2470 = _M0L3pslS654 + 1;
      _M0L6_2atmpS2472 = _M0L3idxS655 + 1;
      _M0L14capacity__maskS2473 = _M0L4selfS657->$3;
      _M0L6_2atmpS2471 = _M0L6_2atmpS2472 & _M0L14capacity__maskS2473;
      _M0L3pslS654 = _M0L6_2atmpS2470;
      _M0L3idxS655 = _M0L6_2atmpS2471;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS673,
  moonbit_string_t _M0L3keyS679,
  moonbit_string_t _M0L5valueS680,
  int32_t _M0L4hashS675
) {
  int32_t _M0L14capacity__maskS2494;
  int32_t _M0L6_2atmpS2493;
  int32_t _M0L3pslS670;
  int32_t _M0L3idxS671;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2494 = _M0L4selfS673->$3;
  _M0L6_2atmpS2493 = _M0L4hashS675 & _M0L14capacity__maskS2494;
  _M0L3pslS670 = 0;
  _M0L3idxS671 = _M0L6_2atmpS2493;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2492 = _M0L4selfS673->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS672;
    if (
      _M0L3idxS671 < 0
      || _M0L3idxS671 >= Moonbit_array_length(_M0L7entriesS2492)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS672
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2492[_M0L3idxS671];
    if (_M0L7_2abindS672 == 0) {
      int32_t _M0L4sizeS2477 = _M0L4selfS673->$1;
      int32_t _M0L8grow__atS2478 = _M0L4selfS673->$4;
      int32_t _M0L7_2abindS676;
      struct _M0TPB5EntryGssE* _M0L7_2abindS677;
      struct _M0TPB5EntryGssE* _M0L5entryS678;
      if (_M0L4sizeS2477 >= _M0L8grow__atS2478) {
        int32_t _M0L14capacity__maskS2480;
        int32_t _M0L6_2atmpS2479;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS673);
        _M0L14capacity__maskS2480 = _M0L4selfS673->$3;
        _M0L6_2atmpS2479 = _M0L4hashS675 & _M0L14capacity__maskS2480;
        _M0L3pslS670 = 0;
        _M0L3idxS671 = _M0L6_2atmpS2479;
        continue;
      }
      _M0L7_2abindS676 = _M0L4selfS673->$6;
      _M0L7_2abindS677 = 0;
      moonbit_incref(_M0L3keyS679);
      moonbit_incref(_M0L5valueS680);
      _M0L5entryS678
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS678)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 70, 0);
      _M0L5entryS678->$0 = _M0L7_2abindS676;
      _M0L5entryS678->$1 = _M0L7_2abindS677;
      _M0L5entryS678->$2 = _M0L3pslS670;
      _M0L5entryS678->$3 = _M0L4hashS675;
      _M0L5entryS678->$4 = _M0L3keyS679;
      _M0L5entryS678->$5 = _M0L5valueS680;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS673, _M0L3idxS671, _M0L5entryS678);
      moonbit_decref(_M0L5entryS678);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS681 = _M0L7_2abindS672;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS682 = _M0L7_2aSomeS681;
      int32_t _M0L4hashS2482 = _M0L14_2acurr__entryS682->$3;
      int32_t _if__result_3982;
      int32_t _M0L3pslS2483;
      int32_t _M0L6_2atmpS2488;
      int32_t _M0L6_2atmpS2490;
      int32_t _M0L14capacity__maskS2491;
      int32_t _M0L6_2atmpS2489;
      if (_M0L4hashS2482 == _M0L4hashS675) {
        moonbit_string_t _M0L3keyS2481 = _M0L14_2acurr__entryS682->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3982
        = _M0L3keyS2481 == _M0L3keyS679
          || Moonbit_array_length(_M0L3keyS2481)
             == Moonbit_array_length(_M0L3keyS679)
             && 0
                == memcmp(_M0L3keyS2481, _M0L3keyS679, Moonbit_array_length(_M0L3keyS2481) * 2);
      } else {
        _if__result_3982 = 0;
      }
      if (_if__result_3982) {
        moonbit_string_t _M0L6_2aoldS3594 = _M0L14_2acurr__entryS682->$5;
        moonbit_incref(_M0L5valueS680);
        moonbit_decref(_M0L6_2aoldS3594);
        _M0L14_2acurr__entryS682->$5 = _M0L5valueS680;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS682);
      }
      _M0L3pslS2483 = _M0L14_2acurr__entryS682->$2;
      if (_M0L3pslS670 > _M0L3pslS2483) {
        int32_t _M0L4sizeS2484 = _M0L4selfS673->$1;
        int32_t _M0L8grow__atS2485 = _M0L4selfS673->$4;
        int32_t _M0L7_2abindS683;
        struct _M0TPB5EntryGssE* _M0L7_2abindS684;
        struct _M0TPB5EntryGssE* _M0L5entryS685;
        if (_M0L4sizeS2484 >= _M0L8grow__atS2485) {
          int32_t _M0L14capacity__maskS2487;
          int32_t _M0L6_2atmpS2486;
          moonbit_decref(_M0L14_2acurr__entryS682);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS673);
          _M0L14capacity__maskS2487 = _M0L4selfS673->$3;
          _M0L6_2atmpS2486 = _M0L4hashS675 & _M0L14capacity__maskS2487;
          _M0L3pslS670 = 0;
          _M0L3idxS671 = _M0L6_2atmpS2486;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS673, _M0L3idxS671, _M0L14_2acurr__entryS682);
        moonbit_decref(_M0L14_2acurr__entryS682);
        _M0L7_2abindS683 = _M0L4selfS673->$6;
        _M0L7_2abindS684 = 0;
        moonbit_incref(_M0L3keyS679);
        moonbit_incref(_M0L5valueS680);
        _M0L5entryS685
        = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
        Moonbit_object_header(_M0L5entryS685)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 70, 0);
        _M0L5entryS685->$0 = _M0L7_2abindS683;
        _M0L5entryS685->$1 = _M0L7_2abindS684;
        _M0L5entryS685->$2 = _M0L3pslS670;
        _M0L5entryS685->$3 = _M0L4hashS675;
        _M0L5entryS685->$4 = _M0L3keyS679;
        _M0L5entryS685->$5 = _M0L5valueS680;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS673, _M0L3idxS671, _M0L5entryS685);
        moonbit_decref(_M0L5entryS685);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS682);
      }
      _M0L6_2atmpS2488 = _M0L3pslS670 + 1;
      _M0L6_2atmpS2490 = _M0L3idxS671 + 1;
      _M0L14capacity__maskS2491 = _M0L4selfS673->$3;
      _M0L6_2atmpS2489 = _M0L6_2atmpS2490 & _M0L14capacity__maskS2491;
      _M0L3pslS670 = _M0L6_2atmpS2488;
      _M0L3idxS671 = _M0L6_2atmpS2489;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS689,
  moonbit_string_t _M0L3keyS695,
  int32_t _M0L5valueS696,
  int32_t _M0L4hashS691
) {
  int32_t _M0L14capacity__maskS2512;
  int32_t _M0L6_2atmpS2511;
  int32_t _M0L3pslS686;
  int32_t _M0L3idxS687;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2512 = _M0L4selfS689->$3;
  _M0L6_2atmpS2511 = _M0L4hashS691 & _M0L14capacity__maskS2512;
  _M0L3pslS686 = 0;
  _M0L3idxS687 = _M0L6_2atmpS2511;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2510 = _M0L4selfS689->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS688;
    if (
      _M0L3idxS687 < 0
      || _M0L3idxS687 >= Moonbit_array_length(_M0L7entriesS2510)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS688
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2510[_M0L3idxS687];
    if (_M0L7_2abindS688 == 0) {
      int32_t _M0L4sizeS2495 = _M0L4selfS689->$1;
      int32_t _M0L8grow__atS2496 = _M0L4selfS689->$4;
      int32_t _M0L7_2abindS692;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS693;
      struct _M0TPB5EntryGsiE* _M0L5entryS694;
      if (_M0L4sizeS2495 >= _M0L8grow__atS2496) {
        int32_t _M0L14capacity__maskS2498;
        int32_t _M0L6_2atmpS2497;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS689);
        _M0L14capacity__maskS2498 = _M0L4selfS689->$3;
        _M0L6_2atmpS2497 = _M0L4hashS691 & _M0L14capacity__maskS2498;
        _M0L3pslS686 = 0;
        _M0L3idxS687 = _M0L6_2atmpS2497;
        continue;
      }
      _M0L7_2abindS692 = _M0L4selfS689->$6;
      _M0L7_2abindS693 = 0;
      moonbit_incref(_M0L3keyS695);
      _M0L5entryS694
      = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
      Moonbit_object_header(_M0L5entryS694)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 75, 0);
      _M0L5entryS694->$0 = _M0L7_2abindS692;
      _M0L5entryS694->$1 = _M0L7_2abindS693;
      _M0L5entryS694->$2 = _M0L3pslS686;
      _M0L5entryS694->$3 = _M0L4hashS691;
      _M0L5entryS694->$4 = _M0L3keyS695;
      _M0L5entryS694->$5 = _M0L5valueS696;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS689, _M0L3idxS687, _M0L5entryS694);
      moonbit_decref(_M0L5entryS694);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS697 = _M0L7_2abindS688;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS698 = _M0L7_2aSomeS697;
      int32_t _M0L4hashS2500 = _M0L14_2acurr__entryS698->$3;
      int32_t _if__result_3984;
      int32_t _M0L3pslS2501;
      int32_t _M0L6_2atmpS2506;
      int32_t _M0L6_2atmpS2508;
      int32_t _M0L14capacity__maskS2509;
      int32_t _M0L6_2atmpS2507;
      if (_M0L4hashS2500 == _M0L4hashS691) {
        moonbit_string_t _M0L3keyS2499 = _M0L14_2acurr__entryS698->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3984
        = _M0L3keyS2499 == _M0L3keyS695
          || Moonbit_array_length(_M0L3keyS2499)
             == Moonbit_array_length(_M0L3keyS695)
             && 0
                == memcmp(_M0L3keyS2499, _M0L3keyS695, Moonbit_array_length(_M0L3keyS2499) * 2);
      } else {
        _if__result_3984 = 0;
      }
      if (_if__result_3984) {
        _M0L14_2acurr__entryS698->$5 = _M0L5valueS696;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS698);
      }
      _M0L3pslS2501 = _M0L14_2acurr__entryS698->$2;
      if (_M0L3pslS686 > _M0L3pslS2501) {
        int32_t _M0L4sizeS2502 = _M0L4selfS689->$1;
        int32_t _M0L8grow__atS2503 = _M0L4selfS689->$4;
        int32_t _M0L7_2abindS699;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS700;
        struct _M0TPB5EntryGsiE* _M0L5entryS701;
        if (_M0L4sizeS2502 >= _M0L8grow__atS2503) {
          int32_t _M0L14capacity__maskS2505;
          int32_t _M0L6_2atmpS2504;
          moonbit_decref(_M0L14_2acurr__entryS698);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS689);
          _M0L14capacity__maskS2505 = _M0L4selfS689->$3;
          _M0L6_2atmpS2504 = _M0L4hashS691 & _M0L14capacity__maskS2505;
          _M0L3pslS686 = 0;
          _M0L3idxS687 = _M0L6_2atmpS2504;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS689, _M0L3idxS687, _M0L14_2acurr__entryS698);
        moonbit_decref(_M0L14_2acurr__entryS698);
        _M0L7_2abindS699 = _M0L4selfS689->$6;
        _M0L7_2abindS700 = 0;
        moonbit_incref(_M0L3keyS695);
        _M0L5entryS701
        = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
        Moonbit_object_header(_M0L5entryS701)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 75, 0);
        _M0L5entryS701->$0 = _M0L7_2abindS699;
        _M0L5entryS701->$1 = _M0L7_2abindS700;
        _M0L5entryS701->$2 = _M0L3pslS686;
        _M0L5entryS701->$3 = _M0L4hashS691;
        _M0L5entryS701->$4 = _M0L3keyS695;
        _M0L5entryS701->$5 = _M0L5valueS696;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS689, _M0L3idxS687, _M0L5entryS701);
        moonbit_decref(_M0L5entryS701);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS698);
      }
      _M0L6_2atmpS2506 = _M0L3pslS686 + 1;
      _M0L6_2atmpS2508 = _M0L3idxS687 + 1;
      _M0L14capacity__maskS2509 = _M0L4selfS689->$3;
      _M0L6_2atmpS2507 = _M0L6_2atmpS2508 & _M0L14capacity__maskS2509;
      _M0L3pslS686 = _M0L6_2atmpS2506;
      _M0L3idxS687 = _M0L6_2atmpS2507;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS705,
  moonbit_string_t _M0L3keyS711,
  int32_t _M0L5valueS712,
  int32_t _M0L4hashS707
) {
  int32_t _M0L14capacity__maskS2530;
  int32_t _M0L6_2atmpS2529;
  int32_t _M0L3pslS702;
  int32_t _M0L3idxS703;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2530 = _M0L4selfS705->$3;
  _M0L6_2atmpS2529 = _M0L4hashS707 & _M0L14capacity__maskS2530;
  _M0L3pslS702 = 0;
  _M0L3idxS703 = _M0L6_2atmpS2529;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2528 = _M0L4selfS705->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS704;
    if (
      _M0L3idxS703 < 0
      || _M0L3idxS703 >= Moonbit_array_length(_M0L7entriesS2528)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS704
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2528[_M0L3idxS703];
    if (_M0L7_2abindS704 == 0) {
      int32_t _M0L4sizeS2513 = _M0L4selfS705->$1;
      int32_t _M0L8grow__atS2514 = _M0L4selfS705->$4;
      int32_t _M0L7_2abindS708;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS709;
      struct _M0TPB5EntryGsbE* _M0L5entryS710;
      if (_M0L4sizeS2513 >= _M0L8grow__atS2514) {
        int32_t _M0L14capacity__maskS2516;
        int32_t _M0L6_2atmpS2515;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS705);
        _M0L14capacity__maskS2516 = _M0L4selfS705->$3;
        _M0L6_2atmpS2515 = _M0L4hashS707 & _M0L14capacity__maskS2516;
        _M0L3pslS702 = 0;
        _M0L3idxS703 = _M0L6_2atmpS2515;
        continue;
      }
      _M0L7_2abindS708 = _M0L4selfS705->$6;
      _M0L7_2abindS709 = 0;
      moonbit_incref(_M0L3keyS711);
      _M0L5entryS710
      = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
      Moonbit_object_header(_M0L5entryS710)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 79, 0);
      _M0L5entryS710->$0 = _M0L7_2abindS708;
      _M0L5entryS710->$1 = _M0L7_2abindS709;
      _M0L5entryS710->$2 = _M0L3pslS702;
      _M0L5entryS710->$3 = _M0L4hashS707;
      _M0L5entryS710->$4 = _M0L3keyS711;
      _M0L5entryS710->$5 = _M0L5valueS712;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS705, _M0L3idxS703, _M0L5entryS710);
      moonbit_decref(_M0L5entryS710);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS713 = _M0L7_2abindS704;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS714 = _M0L7_2aSomeS713;
      int32_t _M0L4hashS2518 = _M0L14_2acurr__entryS714->$3;
      int32_t _if__result_3986;
      int32_t _M0L3pslS2519;
      int32_t _M0L6_2atmpS2524;
      int32_t _M0L6_2atmpS2526;
      int32_t _M0L14capacity__maskS2527;
      int32_t _M0L6_2atmpS2525;
      if (_M0L4hashS2518 == _M0L4hashS707) {
        moonbit_string_t _M0L3keyS2517 = _M0L14_2acurr__entryS714->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3986
        = _M0L3keyS2517 == _M0L3keyS711
          || Moonbit_array_length(_M0L3keyS2517)
             == Moonbit_array_length(_M0L3keyS711)
             && 0
                == memcmp(_M0L3keyS2517, _M0L3keyS711, Moonbit_array_length(_M0L3keyS2517) * 2);
      } else {
        _if__result_3986 = 0;
      }
      if (_if__result_3986) {
        _M0L14_2acurr__entryS714->$5 = _M0L5valueS712;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS714);
      }
      _M0L3pslS2519 = _M0L14_2acurr__entryS714->$2;
      if (_M0L3pslS702 > _M0L3pslS2519) {
        int32_t _M0L4sizeS2520 = _M0L4selfS705->$1;
        int32_t _M0L8grow__atS2521 = _M0L4selfS705->$4;
        int32_t _M0L7_2abindS715;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS716;
        struct _M0TPB5EntryGsbE* _M0L5entryS717;
        if (_M0L4sizeS2520 >= _M0L8grow__atS2521) {
          int32_t _M0L14capacity__maskS2523;
          int32_t _M0L6_2atmpS2522;
          moonbit_decref(_M0L14_2acurr__entryS714);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS705);
          _M0L14capacity__maskS2523 = _M0L4selfS705->$3;
          _M0L6_2atmpS2522 = _M0L4hashS707 & _M0L14capacity__maskS2523;
          _M0L3pslS702 = 0;
          _M0L3idxS703 = _M0L6_2atmpS2522;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS705, _M0L3idxS703, _M0L14_2acurr__entryS714);
        moonbit_decref(_M0L14_2acurr__entryS714);
        _M0L7_2abindS715 = _M0L4selfS705->$6;
        _M0L7_2abindS716 = 0;
        moonbit_incref(_M0L3keyS711);
        _M0L5entryS717
        = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
        Moonbit_object_header(_M0L5entryS717)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 79, 0);
        _M0L5entryS717->$0 = _M0L7_2abindS715;
        _M0L5entryS717->$1 = _M0L7_2abindS716;
        _M0L5entryS717->$2 = _M0L3pslS702;
        _M0L5entryS717->$3 = _M0L4hashS707;
        _M0L5entryS717->$4 = _M0L3keyS711;
        _M0L5entryS717->$5 = _M0L5valueS712;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS705, _M0L3idxS703, _M0L5entryS717);
        moonbit_decref(_M0L5entryS717);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS714);
      }
      _M0L6_2atmpS2524 = _M0L3pslS702 + 1;
      _M0L6_2atmpS2526 = _M0L3idxS703 + 1;
      _M0L14capacity__maskS2527 = _M0L4selfS705->$3;
      _M0L6_2atmpS2525 = _M0L6_2atmpS2526 & _M0L14capacity__maskS2527;
      _M0L3pslS702 = _M0L6_2atmpS2524;
      _M0L3idxS703 = _M0L6_2atmpS2525;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE* _M0L4selfS599) {
  struct _M0TPB5EntryGsfE* _M0L9old__headS598;
  int32_t _M0L8capacityS2408;
  int32_t _M0L13new__capacityS600;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2402;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS2401;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS3607;
  int32_t _M0L6_2atmpS2403;
  int32_t _M0L8capacityS2405;
  int32_t _M0L6_2atmpS2404;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2406;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3606;
  struct _M0TPB5EntryGsfE* _M0L1xS601;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS598 = _M0L4selfS599->$5;
  _M0L8capacityS2408 = _M0L4selfS599->$2;
  _M0L13new__capacityS600 = _M0L8capacityS2408 << 1;
  _M0L6_2atmpS2402 = 0;
  _M0L6_2atmpS2401
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS600, _M0L6_2atmpS2402);
  _M0L6_2aoldS3607 = _M0L4selfS599->$0;
  if (_M0L9old__headS598) {
    moonbit_incref(_M0L9old__headS598);
  }
  moonbit_decref(_M0L6_2aoldS3607);
  _M0L4selfS599->$0 = _M0L6_2atmpS2401;
  _M0L4selfS599->$2 = _M0L13new__capacityS600;
  _M0L6_2atmpS2403 = _M0L13new__capacityS600 - 1;
  _M0L4selfS599->$3 = _M0L6_2atmpS2403;
  _M0L8capacityS2405 = _M0L4selfS599->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2404 = _M0FPB21calc__grow__threshold(_M0L8capacityS2405);
  _M0L4selfS599->$4 = _M0L6_2atmpS2404;
  _M0L4selfS599->$1 = 0;
  _M0L6_2atmpS2406 = 0;
  _M0L6_2aoldS3606 = _M0L4selfS599->$5;
  if (_M0L6_2aoldS3606) {
    moonbit_decref(_M0L6_2aoldS3606);
  }
  _M0L4selfS599->$5 = _M0L6_2atmpS2406;
  _M0L4selfS599->$6 = -1;
  _M0L1xS601 = _M0L9old__headS598;
  while (1) {
    if (_M0L1xS601 == 0) {
      if (_M0L1xS601) {
        moonbit_decref(_M0L1xS601);
      }
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS603 = _M0L1xS601;
      struct _M0TPB5EntryGsfE* _M0L4_2aeS604 = _M0L7_2aSomeS603;
      struct _M0TPB5EntryGsfE* _M0L15next__in__chainS605 = _M0L4_2aeS604->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2407 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3604 = _M0L4_2aeS604->$1;
      if (_M0L15next__in__chainS605) {
        moonbit_incref(_M0L15next__in__chainS605);
      }
      if (_M0L6_2aoldS3604) {
        moonbit_decref(_M0L6_2aoldS3604);
      }
      _M0L4_2aeS604->$1 = _M0L6_2atmpS2407;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsfE(_M0L4selfS599, _M0L4_2aeS604);
      moonbit_decref(_M0L4_2aeS604);
      _M0L1xS601 = _M0L15next__in__chainS605;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS607
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L9old__headS606;
  int32_t _M0L8capacityS2416;
  int32_t _M0L13new__capacityS608;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2410;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS2409;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2aoldS3612;
  int32_t _M0L6_2atmpS2411;
  int32_t _M0L8capacityS2413;
  int32_t _M0L6_2atmpS2412;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2414;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3611;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1xS609;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS606 = _M0L4selfS607->$5;
  _M0L8capacityS2416 = _M0L4selfS607->$2;
  _M0L13new__capacityS608 = _M0L8capacityS2416 << 1;
  _M0L6_2atmpS2410 = 0;
  _M0L6_2atmpS2409
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS608, _M0L6_2atmpS2410);
  _M0L6_2aoldS3612 = _M0L4selfS607->$0;
  if (_M0L9old__headS606) {
    moonbit_incref(_M0L9old__headS606);
  }
  moonbit_decref(_M0L6_2aoldS3612);
  _M0L4selfS607->$0 = _M0L6_2atmpS2409;
  _M0L4selfS607->$2 = _M0L13new__capacityS608;
  _M0L6_2atmpS2411 = _M0L13new__capacityS608 - 1;
  _M0L4selfS607->$3 = _M0L6_2atmpS2411;
  _M0L8capacityS2413 = _M0L4selfS607->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2412 = _M0FPB21calc__grow__threshold(_M0L8capacityS2413);
  _M0L4selfS607->$4 = _M0L6_2atmpS2412;
  _M0L4selfS607->$1 = 0;
  _M0L6_2atmpS2414 = 0;
  _M0L6_2aoldS3611 = _M0L4selfS607->$5;
  if (_M0L6_2aoldS3611) {
    moonbit_decref(_M0L6_2aoldS3611);
  }
  _M0L4selfS607->$5 = _M0L6_2atmpS2414;
  _M0L4selfS607->$6 = -1;
  _M0L1xS609 = _M0L9old__headS606;
  while (1) {
    if (_M0L1xS609 == 0) {
      if (_M0L1xS609) {
        moonbit_decref(_M0L1xS609);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS611 =
        _M0L1xS609;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2aeS612 =
        _M0L7_2aSomeS611;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L15next__in__chainS613 =
        _M0L4_2aeS612->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2415 =
        0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3609 =
        _M0L4_2aeS612->$1;
      if (_M0L15next__in__chainS613) {
        moonbit_incref(_M0L15next__in__chainS613);
      }
      if (_M0L6_2aoldS3609) {
        moonbit_decref(_M0L6_2aoldS3609);
      }
      _M0L4_2aeS612->$1 = _M0L6_2atmpS2415;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS607, _M0L4_2aeS612);
      moonbit_decref(_M0L4_2aeS612);
      _M0L1xS609 = _M0L15next__in__chainS613;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE* _M0L4selfS615) {
  struct _M0TPB5EntryGssE* _M0L9old__headS614;
  int32_t _M0L8capacityS2424;
  int32_t _M0L13new__capacityS616;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2418;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS2417;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS3617;
  int32_t _M0L6_2atmpS2419;
  int32_t _M0L8capacityS2421;
  int32_t _M0L6_2atmpS2420;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2422;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3616;
  struct _M0TPB5EntryGssE* _M0L1xS617;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS614 = _M0L4selfS615->$5;
  _M0L8capacityS2424 = _M0L4selfS615->$2;
  _M0L13new__capacityS616 = _M0L8capacityS2424 << 1;
  _M0L6_2atmpS2418 = 0;
  _M0L6_2atmpS2417
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS616, _M0L6_2atmpS2418);
  _M0L6_2aoldS3617 = _M0L4selfS615->$0;
  if (_M0L9old__headS614) {
    moonbit_incref(_M0L9old__headS614);
  }
  moonbit_decref(_M0L6_2aoldS3617);
  _M0L4selfS615->$0 = _M0L6_2atmpS2417;
  _M0L4selfS615->$2 = _M0L13new__capacityS616;
  _M0L6_2atmpS2419 = _M0L13new__capacityS616 - 1;
  _M0L4selfS615->$3 = _M0L6_2atmpS2419;
  _M0L8capacityS2421 = _M0L4selfS615->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2420 = _M0FPB21calc__grow__threshold(_M0L8capacityS2421);
  _M0L4selfS615->$4 = _M0L6_2atmpS2420;
  _M0L4selfS615->$1 = 0;
  _M0L6_2atmpS2422 = 0;
  _M0L6_2aoldS3616 = _M0L4selfS615->$5;
  if (_M0L6_2aoldS3616) {
    moonbit_decref(_M0L6_2aoldS3616);
  }
  _M0L4selfS615->$5 = _M0L6_2atmpS2422;
  _M0L4selfS615->$6 = -1;
  _M0L1xS617 = _M0L9old__headS614;
  while (1) {
    if (_M0L1xS617 == 0) {
      if (_M0L1xS617) {
        moonbit_decref(_M0L1xS617);
      }
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS619 = _M0L1xS617;
      struct _M0TPB5EntryGssE* _M0L4_2aeS620 = _M0L7_2aSomeS619;
      struct _M0TPB5EntryGssE* _M0L15next__in__chainS621 = _M0L4_2aeS620->$1;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2423 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3614 = _M0L4_2aeS620->$1;
      if (_M0L15next__in__chainS621) {
        moonbit_incref(_M0L15next__in__chainS621);
      }
      if (_M0L6_2aoldS3614) {
        moonbit_decref(_M0L6_2aoldS3614);
      }
      _M0L4_2aeS620->$1 = _M0L6_2atmpS2423;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGssE(_M0L4selfS615, _M0L4_2aeS620);
      moonbit_decref(_M0L4_2aeS620);
      _M0L1xS617 = _M0L15next__in__chainS621;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE* _M0L4selfS623) {
  struct _M0TPB5EntryGsiE* _M0L9old__headS622;
  int32_t _M0L8capacityS2432;
  int32_t _M0L13new__capacityS624;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2426;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS2425;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS3622;
  int32_t _M0L6_2atmpS2427;
  int32_t _M0L8capacityS2429;
  int32_t _M0L6_2atmpS2428;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2430;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3621;
  struct _M0TPB5EntryGsiE* _M0L1xS625;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS622 = _M0L4selfS623->$5;
  _M0L8capacityS2432 = _M0L4selfS623->$2;
  _M0L13new__capacityS624 = _M0L8capacityS2432 << 1;
  _M0L6_2atmpS2426 = 0;
  _M0L6_2atmpS2425
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS624, _M0L6_2atmpS2426);
  _M0L6_2aoldS3622 = _M0L4selfS623->$0;
  if (_M0L9old__headS622) {
    moonbit_incref(_M0L9old__headS622);
  }
  moonbit_decref(_M0L6_2aoldS3622);
  _M0L4selfS623->$0 = _M0L6_2atmpS2425;
  _M0L4selfS623->$2 = _M0L13new__capacityS624;
  _M0L6_2atmpS2427 = _M0L13new__capacityS624 - 1;
  _M0L4selfS623->$3 = _M0L6_2atmpS2427;
  _M0L8capacityS2429 = _M0L4selfS623->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2428 = _M0FPB21calc__grow__threshold(_M0L8capacityS2429);
  _M0L4selfS623->$4 = _M0L6_2atmpS2428;
  _M0L4selfS623->$1 = 0;
  _M0L6_2atmpS2430 = 0;
  _M0L6_2aoldS3621 = _M0L4selfS623->$5;
  if (_M0L6_2aoldS3621) {
    moonbit_decref(_M0L6_2aoldS3621);
  }
  _M0L4selfS623->$5 = _M0L6_2atmpS2430;
  _M0L4selfS623->$6 = -1;
  _M0L1xS625 = _M0L9old__headS622;
  while (1) {
    if (_M0L1xS625 == 0) {
      if (_M0L1xS625) {
        moonbit_decref(_M0L1xS625);
      }
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS627 = _M0L1xS625;
      struct _M0TPB5EntryGsiE* _M0L4_2aeS628 = _M0L7_2aSomeS627;
      struct _M0TPB5EntryGsiE* _M0L15next__in__chainS629 = _M0L4_2aeS628->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2431 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3619 = _M0L4_2aeS628->$1;
      if (_M0L15next__in__chainS629) {
        moonbit_incref(_M0L15next__in__chainS629);
      }
      if (_M0L6_2aoldS3619) {
        moonbit_decref(_M0L6_2aoldS3619);
      }
      _M0L4_2aeS628->$1 = _M0L6_2atmpS2431;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsiE(_M0L4selfS623, _M0L4_2aeS628);
      moonbit_decref(_M0L4_2aeS628);
      _M0L1xS625 = _M0L15next__in__chainS629;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE* _M0L4selfS631) {
  struct _M0TPB5EntryGsbE* _M0L9old__headS630;
  int32_t _M0L8capacityS2440;
  int32_t _M0L13new__capacityS632;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2434;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS2433;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS3627;
  int32_t _M0L6_2atmpS2435;
  int32_t _M0L8capacityS2437;
  int32_t _M0L6_2atmpS2436;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2438;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3626;
  struct _M0TPB5EntryGsbE* _M0L1xS633;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS630 = _M0L4selfS631->$5;
  _M0L8capacityS2440 = _M0L4selfS631->$2;
  _M0L13new__capacityS632 = _M0L8capacityS2440 << 1;
  _M0L6_2atmpS2434 = 0;
  _M0L6_2atmpS2433
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS632, _M0L6_2atmpS2434);
  _M0L6_2aoldS3627 = _M0L4selfS631->$0;
  if (_M0L9old__headS630) {
    moonbit_incref(_M0L9old__headS630);
  }
  moonbit_decref(_M0L6_2aoldS3627);
  _M0L4selfS631->$0 = _M0L6_2atmpS2433;
  _M0L4selfS631->$2 = _M0L13new__capacityS632;
  _M0L6_2atmpS2435 = _M0L13new__capacityS632 - 1;
  _M0L4selfS631->$3 = _M0L6_2atmpS2435;
  _M0L8capacityS2437 = _M0L4selfS631->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2436 = _M0FPB21calc__grow__threshold(_M0L8capacityS2437);
  _M0L4selfS631->$4 = _M0L6_2atmpS2436;
  _M0L4selfS631->$1 = 0;
  _M0L6_2atmpS2438 = 0;
  _M0L6_2aoldS3626 = _M0L4selfS631->$5;
  if (_M0L6_2aoldS3626) {
    moonbit_decref(_M0L6_2aoldS3626);
  }
  _M0L4selfS631->$5 = _M0L6_2atmpS2438;
  _M0L4selfS631->$6 = -1;
  _M0L1xS633 = _M0L9old__headS630;
  while (1) {
    if (_M0L1xS633 == 0) {
      if (_M0L1xS633) {
        moonbit_decref(_M0L1xS633);
      }
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS635 = _M0L1xS633;
      struct _M0TPB5EntryGsbE* _M0L4_2aeS636 = _M0L7_2aSomeS635;
      struct _M0TPB5EntryGsbE* _M0L15next__in__chainS637 = _M0L4_2aeS636->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2439 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3624 = _M0L4_2aeS636->$1;
      if (_M0L15next__in__chainS637) {
        moonbit_incref(_M0L15next__in__chainS637);
      }
      if (_M0L6_2aoldS3624) {
        moonbit_decref(_M0L6_2aoldS3624);
      }
      _M0L4_2aeS636->$1 = _M0L6_2atmpS2439;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsbE(_M0L4selfS631, _M0L4_2aeS636);
      moonbit_decref(_M0L4_2aeS636);
      _M0L1xS633 = _M0L15next__in__chainS637;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS558,
  struct _M0TPB5EntryGsfE* _M0L5outerS554
) {
  int32_t _M0L4hashS553;
  int32_t _M0L14capacity__maskS2360;
  int32_t _M0L6_2atmpS2359;
  int32_t _M0L3pslS555;
  int32_t _M0L3idxS556;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS553 = _M0L5outerS554->$3;
  _M0L14capacity__maskS2360 = _M0L4selfS558->$3;
  _M0L6_2atmpS2359 = _M0L4hashS553 & _M0L14capacity__maskS2360;
  _M0L3pslS555 = 0;
  _M0L3idxS556 = _M0L6_2atmpS2359;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2358 = _M0L4selfS558->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS557;
    if (
      _M0L3idxS556 < 0
      || _M0L3idxS556 >= Moonbit_array_length(_M0L7entriesS2358)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS557
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2358[_M0L3idxS556];
    if (_M0L7_2abindS557 == 0) {
      int32_t _M0L4tailS2351;
      _M0L5outerS554->$2 = _M0L3pslS555;
      _M0L4tailS2351 = _M0L4selfS558->$6;
      _M0L5outerS554->$0 = _M0L4tailS2351;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS558, _M0L3idxS556, _M0L5outerS554);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS559 = _M0L7_2abindS557;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS560 = _M0L7_2aSomeS559;
      int32_t _M0L3pslS2352 = _M0L7_2acurrS560->$2;
      if (_M0L3pslS555 > _M0L3pslS2352) {
        int32_t _M0L4tailS2353;
        moonbit_incref(_M0L7_2acurrS560);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS558, _M0L3idxS556, _M0L7_2acurrS560);
        moonbit_decref(_M0L7_2acurrS560);
        _M0L5outerS554->$2 = _M0L3pslS555;
        _M0L4tailS2353 = _M0L4selfS558->$6;
        _M0L5outerS554->$0 = _M0L4tailS2353;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS558, _M0L3idxS556, _M0L5outerS554);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2354 = _M0L3pslS555 + 1;
        int32_t _M0L6_2atmpS2356 = _M0L3idxS556 + 1;
        int32_t _M0L14capacity__maskS2357 = _M0L4selfS558->$3;
        int32_t _M0L6_2atmpS2355 =
          _M0L6_2atmpS2356 & _M0L14capacity__maskS2357;
        _M0L3pslS555 = _M0L6_2atmpS2354;
        _M0L3idxS556 = _M0L6_2atmpS2355;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS567,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5outerS563
) {
  int32_t _M0L4hashS562;
  int32_t _M0L14capacity__maskS2370;
  int32_t _M0L6_2atmpS2369;
  int32_t _M0L3pslS564;
  int32_t _M0L3idxS565;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS562 = _M0L5outerS563->$3;
  _M0L14capacity__maskS2370 = _M0L4selfS567->$3;
  _M0L6_2atmpS2369 = _M0L4hashS562 & _M0L14capacity__maskS2370;
  _M0L3pslS564 = 0;
  _M0L3idxS565 = _M0L6_2atmpS2369;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2368 =
      _M0L4selfS567->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS566;
    if (
      _M0L3idxS565 < 0
      || _M0L3idxS565 >= Moonbit_array_length(_M0L7entriesS2368)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS566
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2368[
        _M0L3idxS565
      ];
    if (_M0L7_2abindS566 == 0) {
      int32_t _M0L4tailS2361;
      _M0L5outerS563->$2 = _M0L3pslS564;
      _M0L4tailS2361 = _M0L4selfS567->$6;
      _M0L5outerS563->$0 = _M0L4tailS2361;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS567, _M0L3idxS565, _M0L5outerS563);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS568 =
        _M0L7_2abindS566;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2acurrS569 =
        _M0L7_2aSomeS568;
      int32_t _M0L3pslS2362 = _M0L7_2acurrS569->$2;
      if (_M0L3pslS564 > _M0L3pslS2362) {
        int32_t _M0L4tailS2363;
        moonbit_incref(_M0L7_2acurrS569);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS567, _M0L3idxS565, _M0L7_2acurrS569);
        moonbit_decref(_M0L7_2acurrS569);
        _M0L5outerS563->$2 = _M0L3pslS564;
        _M0L4tailS2363 = _M0L4selfS567->$6;
        _M0L5outerS563->$0 = _M0L4tailS2363;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS567, _M0L3idxS565, _M0L5outerS563);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2364 = _M0L3pslS564 + 1;
        int32_t _M0L6_2atmpS2366 = _M0L3idxS565 + 1;
        int32_t _M0L14capacity__maskS2367 = _M0L4selfS567->$3;
        int32_t _M0L6_2atmpS2365 =
          _M0L6_2atmpS2366 & _M0L14capacity__maskS2367;
        _M0L3pslS564 = _M0L6_2atmpS2364;
        _M0L3idxS565 = _M0L6_2atmpS2365;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS576,
  struct _M0TPB5EntryGssE* _M0L5outerS572
) {
  int32_t _M0L4hashS571;
  int32_t _M0L14capacity__maskS2380;
  int32_t _M0L6_2atmpS2379;
  int32_t _M0L3pslS573;
  int32_t _M0L3idxS574;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS571 = _M0L5outerS572->$3;
  _M0L14capacity__maskS2380 = _M0L4selfS576->$3;
  _M0L6_2atmpS2379 = _M0L4hashS571 & _M0L14capacity__maskS2380;
  _M0L3pslS573 = 0;
  _M0L3idxS574 = _M0L6_2atmpS2379;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2378 = _M0L4selfS576->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS575;
    if (
      _M0L3idxS574 < 0
      || _M0L3idxS574 >= Moonbit_array_length(_M0L7entriesS2378)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS575
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2378[_M0L3idxS574];
    if (_M0L7_2abindS575 == 0) {
      int32_t _M0L4tailS2371;
      _M0L5outerS572->$2 = _M0L3pslS573;
      _M0L4tailS2371 = _M0L4selfS576->$6;
      _M0L5outerS572->$0 = _M0L4tailS2371;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS576, _M0L3idxS574, _M0L5outerS572);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS577 = _M0L7_2abindS575;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS578 = _M0L7_2aSomeS577;
      int32_t _M0L3pslS2372 = _M0L7_2acurrS578->$2;
      if (_M0L3pslS573 > _M0L3pslS2372) {
        int32_t _M0L4tailS2373;
        moonbit_incref(_M0L7_2acurrS578);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS576, _M0L3idxS574, _M0L7_2acurrS578);
        moonbit_decref(_M0L7_2acurrS578);
        _M0L5outerS572->$2 = _M0L3pslS573;
        _M0L4tailS2373 = _M0L4selfS576->$6;
        _M0L5outerS572->$0 = _M0L4tailS2373;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS576, _M0L3idxS574, _M0L5outerS572);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2374 = _M0L3pslS573 + 1;
        int32_t _M0L6_2atmpS2376 = _M0L3idxS574 + 1;
        int32_t _M0L14capacity__maskS2377 = _M0L4selfS576->$3;
        int32_t _M0L6_2atmpS2375 =
          _M0L6_2atmpS2376 & _M0L14capacity__maskS2377;
        _M0L3pslS573 = _M0L6_2atmpS2374;
        _M0L3idxS574 = _M0L6_2atmpS2375;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS585,
  struct _M0TPB5EntryGsiE* _M0L5outerS581
) {
  int32_t _M0L4hashS580;
  int32_t _M0L14capacity__maskS2390;
  int32_t _M0L6_2atmpS2389;
  int32_t _M0L3pslS582;
  int32_t _M0L3idxS583;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS580 = _M0L5outerS581->$3;
  _M0L14capacity__maskS2390 = _M0L4selfS585->$3;
  _M0L6_2atmpS2389 = _M0L4hashS580 & _M0L14capacity__maskS2390;
  _M0L3pslS582 = 0;
  _M0L3idxS583 = _M0L6_2atmpS2389;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2388 = _M0L4selfS585->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS584;
    if (
      _M0L3idxS583 < 0
      || _M0L3idxS583 >= Moonbit_array_length(_M0L7entriesS2388)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS584
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2388[_M0L3idxS583];
    if (_M0L7_2abindS584 == 0) {
      int32_t _M0L4tailS2381;
      _M0L5outerS581->$2 = _M0L3pslS582;
      _M0L4tailS2381 = _M0L4selfS585->$6;
      _M0L5outerS581->$0 = _M0L4tailS2381;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS585, _M0L3idxS583, _M0L5outerS581);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS586 = _M0L7_2abindS584;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS587 = _M0L7_2aSomeS586;
      int32_t _M0L3pslS2382 = _M0L7_2acurrS587->$2;
      if (_M0L3pslS582 > _M0L3pslS2382) {
        int32_t _M0L4tailS2383;
        moonbit_incref(_M0L7_2acurrS587);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS585, _M0L3idxS583, _M0L7_2acurrS587);
        moonbit_decref(_M0L7_2acurrS587);
        _M0L5outerS581->$2 = _M0L3pslS582;
        _M0L4tailS2383 = _M0L4selfS585->$6;
        _M0L5outerS581->$0 = _M0L4tailS2383;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS585, _M0L3idxS583, _M0L5outerS581);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2384 = _M0L3pslS582 + 1;
        int32_t _M0L6_2atmpS2386 = _M0L3idxS583 + 1;
        int32_t _M0L14capacity__maskS2387 = _M0L4selfS585->$3;
        int32_t _M0L6_2atmpS2385 =
          _M0L6_2atmpS2386 & _M0L14capacity__maskS2387;
        _M0L3pslS582 = _M0L6_2atmpS2384;
        _M0L3idxS583 = _M0L6_2atmpS2385;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS594,
  struct _M0TPB5EntryGsbE* _M0L5outerS590
) {
  int32_t _M0L4hashS589;
  int32_t _M0L14capacity__maskS2400;
  int32_t _M0L6_2atmpS2399;
  int32_t _M0L3pslS591;
  int32_t _M0L3idxS592;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS589 = _M0L5outerS590->$3;
  _M0L14capacity__maskS2400 = _M0L4selfS594->$3;
  _M0L6_2atmpS2399 = _M0L4hashS589 & _M0L14capacity__maskS2400;
  _M0L3pslS591 = 0;
  _M0L3idxS592 = _M0L6_2atmpS2399;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2398 = _M0L4selfS594->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS593;
    if (
      _M0L3idxS592 < 0
      || _M0L3idxS592 >= Moonbit_array_length(_M0L7entriesS2398)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS593
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2398[_M0L3idxS592];
    if (_M0L7_2abindS593 == 0) {
      int32_t _M0L4tailS2391;
      _M0L5outerS590->$2 = _M0L3pslS591;
      _M0L4tailS2391 = _M0L4selfS594->$6;
      _M0L5outerS590->$0 = _M0L4tailS2391;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS594, _M0L3idxS592, _M0L5outerS590);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS595 = _M0L7_2abindS593;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS596 = _M0L7_2aSomeS595;
      int32_t _M0L3pslS2392 = _M0L7_2acurrS596->$2;
      if (_M0L3pslS591 > _M0L3pslS2392) {
        int32_t _M0L4tailS2393;
        moonbit_incref(_M0L7_2acurrS596);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS594, _M0L3idxS592, _M0L7_2acurrS596);
        moonbit_decref(_M0L7_2acurrS596);
        _M0L5outerS590->$2 = _M0L3pslS591;
        _M0L4tailS2393 = _M0L4selfS594->$6;
        _M0L5outerS590->$0 = _M0L4tailS2393;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS594, _M0L3idxS592, _M0L5outerS590);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2394 = _M0L3pslS591 + 1;
        int32_t _M0L6_2atmpS2396 = _M0L3idxS592 + 1;
        int32_t _M0L14capacity__maskS2397 = _M0L4selfS594->$3;
        int32_t _M0L6_2atmpS2395 =
          _M0L6_2atmpS2396 & _M0L14capacity__maskS2397;
        _M0L3pslS591 = _M0L6_2atmpS2394;
        _M0L3idxS592 = _M0L6_2atmpS2395;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS507,
  int32_t _M0L3idxS512,
  struct _M0TPB5EntryGsfE* _M0L5entryS511
) {
  int32_t _M0L3pslS2286;
  int32_t _M0L6_2atmpS2282;
  int32_t _M0L6_2atmpS2284;
  int32_t _M0L14capacity__maskS2285;
  int32_t _M0L6_2atmpS2283;
  int32_t _M0L3pslS503;
  int32_t _M0L3idxS504;
  struct _M0TPB5EntryGsfE* _M0L5entryS505;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2286 = _M0L5entryS511->$2;
  _M0L6_2atmpS2282 = _M0L3pslS2286 + 1;
  _M0L6_2atmpS2284 = _M0L3idxS512 + 1;
  _M0L14capacity__maskS2285 = _M0L4selfS507->$3;
  _M0L6_2atmpS2283 = _M0L6_2atmpS2284 & _M0L14capacity__maskS2285;
  moonbit_incref(_M0L5entryS511);
  _M0L3pslS503 = _M0L6_2atmpS2282;
  _M0L3idxS504 = _M0L6_2atmpS2283;
  _M0L5entryS505 = _M0L5entryS511;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2281 = _M0L4selfS507->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS506;
    if (
      _M0L3idxS504 < 0
      || _M0L3idxS504 >= Moonbit_array_length(_M0L7entriesS2281)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS506
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2281[_M0L3idxS504];
    if (_M0L7_2abindS506 == 0) {
      _M0L5entryS505->$2 = _M0L3pslS503;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS507, _M0L5entryS505, _M0L3idxS504);
      moonbit_decref(_M0L5entryS505);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS509 = _M0L7_2abindS506;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS510 = _M0L7_2aSomeS509;
      int32_t _M0L3pslS2271 = _M0L14_2acurr__entryS510->$2;
      if (_M0L3pslS503 > _M0L3pslS2271) {
        int32_t _M0L3pslS2276;
        int32_t _M0L6_2atmpS2272;
        int32_t _M0L6_2atmpS2274;
        int32_t _M0L14capacity__maskS2275;
        int32_t _M0L6_2atmpS2273;
        _M0L5entryS505->$2 = _M0L3pslS503;
        moonbit_incref(_M0L14_2acurr__entryS510);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS507, _M0L5entryS505, _M0L3idxS504);
        moonbit_decref(_M0L5entryS505);
        _M0L3pslS2276 = _M0L14_2acurr__entryS510->$2;
        _M0L6_2atmpS2272 = _M0L3pslS2276 + 1;
        _M0L6_2atmpS2274 = _M0L3idxS504 + 1;
        _M0L14capacity__maskS2275 = _M0L4selfS507->$3;
        _M0L6_2atmpS2273 = _M0L6_2atmpS2274 & _M0L14capacity__maskS2275;
        _M0L3pslS503 = _M0L6_2atmpS2272;
        _M0L3idxS504 = _M0L6_2atmpS2273;
        _M0L5entryS505 = _M0L14_2acurr__entryS510;
        continue;
      } else {
        int32_t _M0L6_2atmpS2277 = _M0L3pslS503 + 1;
        int32_t _M0L6_2atmpS2279 = _M0L3idxS504 + 1;
        int32_t _M0L14capacity__maskS2280 = _M0L4selfS507->$3;
        int32_t _M0L6_2atmpS2278 =
          _M0L6_2atmpS2279 & _M0L14capacity__maskS2280;
        struct _M0TPB5EntryGsfE* _tmp_3998 = _M0L5entryS505;
        _M0L3pslS503 = _M0L6_2atmpS2277;
        _M0L3idxS504 = _M0L6_2atmpS2278;
        _M0L5entryS505 = _tmp_3998;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS517,
  int32_t _M0L3idxS522,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS521
) {
  int32_t _M0L3pslS2302;
  int32_t _M0L6_2atmpS2298;
  int32_t _M0L6_2atmpS2300;
  int32_t _M0L14capacity__maskS2301;
  int32_t _M0L6_2atmpS2299;
  int32_t _M0L3pslS513;
  int32_t _M0L3idxS514;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS515;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2302 = _M0L5entryS521->$2;
  _M0L6_2atmpS2298 = _M0L3pslS2302 + 1;
  _M0L6_2atmpS2300 = _M0L3idxS522 + 1;
  _M0L14capacity__maskS2301 = _M0L4selfS517->$3;
  _M0L6_2atmpS2299 = _M0L6_2atmpS2300 & _M0L14capacity__maskS2301;
  moonbit_incref(_M0L5entryS521);
  _M0L3pslS513 = _M0L6_2atmpS2298;
  _M0L3idxS514 = _M0L6_2atmpS2299;
  _M0L5entryS515 = _M0L5entryS521;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2297 =
      _M0L4selfS517->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS516;
    if (
      _M0L3idxS514 < 0
      || _M0L3idxS514 >= Moonbit_array_length(_M0L7entriesS2297)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS516
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2297[
        _M0L3idxS514
      ];
    if (_M0L7_2abindS516 == 0) {
      _M0L5entryS515->$2 = _M0L3pslS513;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS517, _M0L5entryS515, _M0L3idxS514);
      moonbit_decref(_M0L5entryS515);
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS519 =
        _M0L7_2abindS516;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS520 =
        _M0L7_2aSomeS519;
      int32_t _M0L3pslS2287 = _M0L14_2acurr__entryS520->$2;
      if (_M0L3pslS513 > _M0L3pslS2287) {
        int32_t _M0L3pslS2292;
        int32_t _M0L6_2atmpS2288;
        int32_t _M0L6_2atmpS2290;
        int32_t _M0L14capacity__maskS2291;
        int32_t _M0L6_2atmpS2289;
        _M0L5entryS515->$2 = _M0L3pslS513;
        moonbit_incref(_M0L14_2acurr__entryS520);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS517, _M0L5entryS515, _M0L3idxS514);
        moonbit_decref(_M0L5entryS515);
        _M0L3pslS2292 = _M0L14_2acurr__entryS520->$2;
        _M0L6_2atmpS2288 = _M0L3pslS2292 + 1;
        _M0L6_2atmpS2290 = _M0L3idxS514 + 1;
        _M0L14capacity__maskS2291 = _M0L4selfS517->$3;
        _M0L6_2atmpS2289 = _M0L6_2atmpS2290 & _M0L14capacity__maskS2291;
        _M0L3pslS513 = _M0L6_2atmpS2288;
        _M0L3idxS514 = _M0L6_2atmpS2289;
        _M0L5entryS515 = _M0L14_2acurr__entryS520;
        continue;
      } else {
        int32_t _M0L6_2atmpS2293 = _M0L3pslS513 + 1;
        int32_t _M0L6_2atmpS2295 = _M0L3idxS514 + 1;
        int32_t _M0L14capacity__maskS2296 = _M0L4selfS517->$3;
        int32_t _M0L6_2atmpS2294 =
          _M0L6_2atmpS2295 & _M0L14capacity__maskS2296;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _tmp_4000 =
          _M0L5entryS515;
        _M0L3pslS513 = _M0L6_2atmpS2293;
        _M0L3idxS514 = _M0L6_2atmpS2294;
        _M0L5entryS515 = _tmp_4000;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE* _M0L4selfS527,
  int32_t _M0L3idxS532,
  struct _M0TPB5EntryGssE* _M0L5entryS531
) {
  int32_t _M0L3pslS2318;
  int32_t _M0L6_2atmpS2314;
  int32_t _M0L6_2atmpS2316;
  int32_t _M0L14capacity__maskS2317;
  int32_t _M0L6_2atmpS2315;
  int32_t _M0L3pslS523;
  int32_t _M0L3idxS524;
  struct _M0TPB5EntryGssE* _M0L5entryS525;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2318 = _M0L5entryS531->$2;
  _M0L6_2atmpS2314 = _M0L3pslS2318 + 1;
  _M0L6_2atmpS2316 = _M0L3idxS532 + 1;
  _M0L14capacity__maskS2317 = _M0L4selfS527->$3;
  _M0L6_2atmpS2315 = _M0L6_2atmpS2316 & _M0L14capacity__maskS2317;
  moonbit_incref(_M0L5entryS531);
  _M0L3pslS523 = _M0L6_2atmpS2314;
  _M0L3idxS524 = _M0L6_2atmpS2315;
  _M0L5entryS525 = _M0L5entryS531;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2313 = _M0L4selfS527->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS526;
    if (
      _M0L3idxS524 < 0
      || _M0L3idxS524 >= Moonbit_array_length(_M0L7entriesS2313)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS526
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2313[_M0L3idxS524];
    if (_M0L7_2abindS526 == 0) {
      _M0L5entryS525->$2 = _M0L3pslS523;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS527, _M0L5entryS525, _M0L3idxS524);
      moonbit_decref(_M0L5entryS525);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS529 = _M0L7_2abindS526;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS530 = _M0L7_2aSomeS529;
      int32_t _M0L3pslS2303 = _M0L14_2acurr__entryS530->$2;
      if (_M0L3pslS523 > _M0L3pslS2303) {
        int32_t _M0L3pslS2308;
        int32_t _M0L6_2atmpS2304;
        int32_t _M0L6_2atmpS2306;
        int32_t _M0L14capacity__maskS2307;
        int32_t _M0L6_2atmpS2305;
        _M0L5entryS525->$2 = _M0L3pslS523;
        moonbit_incref(_M0L14_2acurr__entryS530);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS527, _M0L5entryS525, _M0L3idxS524);
        moonbit_decref(_M0L5entryS525);
        _M0L3pslS2308 = _M0L14_2acurr__entryS530->$2;
        _M0L6_2atmpS2304 = _M0L3pslS2308 + 1;
        _M0L6_2atmpS2306 = _M0L3idxS524 + 1;
        _M0L14capacity__maskS2307 = _M0L4selfS527->$3;
        _M0L6_2atmpS2305 = _M0L6_2atmpS2306 & _M0L14capacity__maskS2307;
        _M0L3pslS523 = _M0L6_2atmpS2304;
        _M0L3idxS524 = _M0L6_2atmpS2305;
        _M0L5entryS525 = _M0L14_2acurr__entryS530;
        continue;
      } else {
        int32_t _M0L6_2atmpS2309 = _M0L3pslS523 + 1;
        int32_t _M0L6_2atmpS2311 = _M0L3idxS524 + 1;
        int32_t _M0L14capacity__maskS2312 = _M0L4selfS527->$3;
        int32_t _M0L6_2atmpS2310 =
          _M0L6_2atmpS2311 & _M0L14capacity__maskS2312;
        struct _M0TPB5EntryGssE* _tmp_4002 = _M0L5entryS525;
        _M0L3pslS523 = _M0L6_2atmpS2309;
        _M0L3idxS524 = _M0L6_2atmpS2310;
        _M0L5entryS525 = _tmp_4002;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS537,
  int32_t _M0L3idxS542,
  struct _M0TPB5EntryGsiE* _M0L5entryS541
) {
  int32_t _M0L3pslS2334;
  int32_t _M0L6_2atmpS2330;
  int32_t _M0L6_2atmpS2332;
  int32_t _M0L14capacity__maskS2333;
  int32_t _M0L6_2atmpS2331;
  int32_t _M0L3pslS533;
  int32_t _M0L3idxS534;
  struct _M0TPB5EntryGsiE* _M0L5entryS535;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2334 = _M0L5entryS541->$2;
  _M0L6_2atmpS2330 = _M0L3pslS2334 + 1;
  _M0L6_2atmpS2332 = _M0L3idxS542 + 1;
  _M0L14capacity__maskS2333 = _M0L4selfS537->$3;
  _M0L6_2atmpS2331 = _M0L6_2atmpS2332 & _M0L14capacity__maskS2333;
  moonbit_incref(_M0L5entryS541);
  _M0L3pslS533 = _M0L6_2atmpS2330;
  _M0L3idxS534 = _M0L6_2atmpS2331;
  _M0L5entryS535 = _M0L5entryS541;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2329 = _M0L4selfS537->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS536;
    if (
      _M0L3idxS534 < 0
      || _M0L3idxS534 >= Moonbit_array_length(_M0L7entriesS2329)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS536
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2329[_M0L3idxS534];
    if (_M0L7_2abindS536 == 0) {
      _M0L5entryS535->$2 = _M0L3pslS533;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS537, _M0L5entryS535, _M0L3idxS534);
      moonbit_decref(_M0L5entryS535);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS539 = _M0L7_2abindS536;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS540 = _M0L7_2aSomeS539;
      int32_t _M0L3pslS2319 = _M0L14_2acurr__entryS540->$2;
      if (_M0L3pslS533 > _M0L3pslS2319) {
        int32_t _M0L3pslS2324;
        int32_t _M0L6_2atmpS2320;
        int32_t _M0L6_2atmpS2322;
        int32_t _M0L14capacity__maskS2323;
        int32_t _M0L6_2atmpS2321;
        _M0L5entryS535->$2 = _M0L3pslS533;
        moonbit_incref(_M0L14_2acurr__entryS540);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS537, _M0L5entryS535, _M0L3idxS534);
        moonbit_decref(_M0L5entryS535);
        _M0L3pslS2324 = _M0L14_2acurr__entryS540->$2;
        _M0L6_2atmpS2320 = _M0L3pslS2324 + 1;
        _M0L6_2atmpS2322 = _M0L3idxS534 + 1;
        _M0L14capacity__maskS2323 = _M0L4selfS537->$3;
        _M0L6_2atmpS2321 = _M0L6_2atmpS2322 & _M0L14capacity__maskS2323;
        _M0L3pslS533 = _M0L6_2atmpS2320;
        _M0L3idxS534 = _M0L6_2atmpS2321;
        _M0L5entryS535 = _M0L14_2acurr__entryS540;
        continue;
      } else {
        int32_t _M0L6_2atmpS2325 = _M0L3pslS533 + 1;
        int32_t _M0L6_2atmpS2327 = _M0L3idxS534 + 1;
        int32_t _M0L14capacity__maskS2328 = _M0L4selfS537->$3;
        int32_t _M0L6_2atmpS2326 =
          _M0L6_2atmpS2327 & _M0L14capacity__maskS2328;
        struct _M0TPB5EntryGsiE* _tmp_4004 = _M0L5entryS535;
        _M0L3pslS533 = _M0L6_2atmpS2325;
        _M0L3idxS534 = _M0L6_2atmpS2326;
        _M0L5entryS535 = _tmp_4004;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS547,
  int32_t _M0L3idxS552,
  struct _M0TPB5EntryGsbE* _M0L5entryS551
) {
  int32_t _M0L3pslS2350;
  int32_t _M0L6_2atmpS2346;
  int32_t _M0L6_2atmpS2348;
  int32_t _M0L14capacity__maskS2349;
  int32_t _M0L6_2atmpS2347;
  int32_t _M0L3pslS543;
  int32_t _M0L3idxS544;
  struct _M0TPB5EntryGsbE* _M0L5entryS545;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2350 = _M0L5entryS551->$2;
  _M0L6_2atmpS2346 = _M0L3pslS2350 + 1;
  _M0L6_2atmpS2348 = _M0L3idxS552 + 1;
  _M0L14capacity__maskS2349 = _M0L4selfS547->$3;
  _M0L6_2atmpS2347 = _M0L6_2atmpS2348 & _M0L14capacity__maskS2349;
  moonbit_incref(_M0L5entryS551);
  _M0L3pslS543 = _M0L6_2atmpS2346;
  _M0L3idxS544 = _M0L6_2atmpS2347;
  _M0L5entryS545 = _M0L5entryS551;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2345 = _M0L4selfS547->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS546;
    if (
      _M0L3idxS544 < 0
      || _M0L3idxS544 >= Moonbit_array_length(_M0L7entriesS2345)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS546
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2345[_M0L3idxS544];
    if (_M0L7_2abindS546 == 0) {
      _M0L5entryS545->$2 = _M0L3pslS543;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS547, _M0L5entryS545, _M0L3idxS544);
      moonbit_decref(_M0L5entryS545);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS549 = _M0L7_2abindS546;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS550 = _M0L7_2aSomeS549;
      int32_t _M0L3pslS2335 = _M0L14_2acurr__entryS550->$2;
      if (_M0L3pslS543 > _M0L3pslS2335) {
        int32_t _M0L3pslS2340;
        int32_t _M0L6_2atmpS2336;
        int32_t _M0L6_2atmpS2338;
        int32_t _M0L14capacity__maskS2339;
        int32_t _M0L6_2atmpS2337;
        _M0L5entryS545->$2 = _M0L3pslS543;
        moonbit_incref(_M0L14_2acurr__entryS550);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS547, _M0L5entryS545, _M0L3idxS544);
        moonbit_decref(_M0L5entryS545);
        _M0L3pslS2340 = _M0L14_2acurr__entryS550->$2;
        _M0L6_2atmpS2336 = _M0L3pslS2340 + 1;
        _M0L6_2atmpS2338 = _M0L3idxS544 + 1;
        _M0L14capacity__maskS2339 = _M0L4selfS547->$3;
        _M0L6_2atmpS2337 = _M0L6_2atmpS2338 & _M0L14capacity__maskS2339;
        _M0L3pslS543 = _M0L6_2atmpS2336;
        _M0L3idxS544 = _M0L6_2atmpS2337;
        _M0L5entryS545 = _M0L14_2acurr__entryS550;
        continue;
      } else {
        int32_t _M0L6_2atmpS2341 = _M0L3pslS543 + 1;
        int32_t _M0L6_2atmpS2343 = _M0L3idxS544 + 1;
        int32_t _M0L14capacity__maskS2344 = _M0L4selfS547->$3;
        int32_t _M0L6_2atmpS2342 =
          _M0L6_2atmpS2343 & _M0L14capacity__maskS2344;
        struct _M0TPB5EntryGsbE* _tmp_4006 = _M0L5entryS545;
        _M0L3pslS543 = _M0L6_2atmpS2341;
        _M0L3idxS544 = _M0L6_2atmpS2342;
        _M0L5entryS545 = _tmp_4006;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS473,
  struct _M0TPB5EntryGsfE* _M0L5entryS475,
  int32_t _M0L8new__idxS474
) {
  struct _M0TPB5EntryGsfE** _M0L7entriesS2261;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2262;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3650;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS476;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2261 = _M0L4selfS473->$0;
  _M0L6_2atmpS2262 = _M0L5entryS475;
  if (
    _M0L8new__idxS474 < 0
    || _M0L8new__idxS474 >= Moonbit_array_length(_M0L7entriesS2261)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3650
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2261[_M0L8new__idxS474];
  if (_M0L6_2atmpS2262) {
    moonbit_incref(_M0L6_2atmpS2262);
  }
  if (_M0L6_2aoldS3650) {
    moonbit_decref(_M0L6_2aoldS3650);
  }
  _M0L7entriesS2261[_M0L8new__idxS474] = _M0L6_2atmpS2262;
  _M0L7_2abindS476 = _M0L5entryS475->$1;
  if (_M0L7_2abindS476 == 0) {
    _M0L4selfS473->$6 = _M0L8new__idxS474;
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS477 = _M0L7_2abindS476;
    struct _M0TPB5EntryGsfE* _M0L7_2anextS478 = _M0L7_2aSomeS477;
    _M0L7_2anextS478->$0 = _M0L8new__idxS474;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS479,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS481,
  int32_t _M0L8new__idxS480
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2263;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2264;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3653;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS482;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2263 = _M0L4selfS479->$0;
  _M0L6_2atmpS2264 = _M0L5entryS481;
  if (
    _M0L8new__idxS480 < 0
    || _M0L8new__idxS480 >= Moonbit_array_length(_M0L7entriesS2263)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3653
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2263[
      _M0L8new__idxS480
    ];
  if (_M0L6_2atmpS2264) {
    moonbit_incref(_M0L6_2atmpS2264);
  }
  if (_M0L6_2aoldS3653) {
    moonbit_decref(_M0L6_2aoldS3653);
  }
  _M0L7entriesS2263[_M0L8new__idxS480] = _M0L6_2atmpS2264;
  _M0L7_2abindS482 = _M0L5entryS481->$1;
  if (_M0L7_2abindS482 == 0) {
    _M0L4selfS479->$6 = _M0L8new__idxS480;
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS483 =
      _M0L7_2abindS482;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS484 =
      _M0L7_2aSomeS483;
    _M0L7_2anextS484->$0 = _M0L8new__idxS480;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS485,
  struct _M0TPB5EntryGssE* _M0L5entryS487,
  int32_t _M0L8new__idxS486
) {
  struct _M0TPB5EntryGssE** _M0L7entriesS2265;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2266;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3656;
  struct _M0TPB5EntryGssE* _M0L7_2abindS488;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2265 = _M0L4selfS485->$0;
  _M0L6_2atmpS2266 = _M0L5entryS487;
  if (
    _M0L8new__idxS486 < 0
    || _M0L8new__idxS486 >= Moonbit_array_length(_M0L7entriesS2265)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3656
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2265[_M0L8new__idxS486];
  if (_M0L6_2atmpS2266) {
    moonbit_incref(_M0L6_2atmpS2266);
  }
  if (_M0L6_2aoldS3656) {
    moonbit_decref(_M0L6_2aoldS3656);
  }
  _M0L7entriesS2265[_M0L8new__idxS486] = _M0L6_2atmpS2266;
  _M0L7_2abindS488 = _M0L5entryS487->$1;
  if (_M0L7_2abindS488 == 0) {
    _M0L4selfS485->$6 = _M0L8new__idxS486;
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS489 = _M0L7_2abindS488;
    struct _M0TPB5EntryGssE* _M0L7_2anextS490 = _M0L7_2aSomeS489;
    _M0L7_2anextS490->$0 = _M0L8new__idxS486;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS491,
  struct _M0TPB5EntryGsiE* _M0L5entryS493,
  int32_t _M0L8new__idxS492
) {
  struct _M0TPB5EntryGsiE** _M0L7entriesS2267;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2268;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3659;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS494;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2267 = _M0L4selfS491->$0;
  _M0L6_2atmpS2268 = _M0L5entryS493;
  if (
    _M0L8new__idxS492 < 0
    || _M0L8new__idxS492 >= Moonbit_array_length(_M0L7entriesS2267)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3659
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2267[_M0L8new__idxS492];
  if (_M0L6_2atmpS2268) {
    moonbit_incref(_M0L6_2atmpS2268);
  }
  if (_M0L6_2aoldS3659) {
    moonbit_decref(_M0L6_2aoldS3659);
  }
  _M0L7entriesS2267[_M0L8new__idxS492] = _M0L6_2atmpS2268;
  _M0L7_2abindS494 = _M0L5entryS493->$1;
  if (_M0L7_2abindS494 == 0) {
    _M0L4selfS491->$6 = _M0L8new__idxS492;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS495 = _M0L7_2abindS494;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS496 = _M0L7_2aSomeS495;
    _M0L7_2anextS496->$0 = _M0L8new__idxS492;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS497,
  struct _M0TPB5EntryGsbE* _M0L5entryS499,
  int32_t _M0L8new__idxS498
) {
  struct _M0TPB5EntryGsbE** _M0L7entriesS2269;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2270;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3662;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS500;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2269 = _M0L4selfS497->$0;
  _M0L6_2atmpS2270 = _M0L5entryS499;
  if (
    _M0L8new__idxS498 < 0
    || _M0L8new__idxS498 >= Moonbit_array_length(_M0L7entriesS2269)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3662
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2269[_M0L8new__idxS498];
  if (_M0L6_2atmpS2270) {
    moonbit_incref(_M0L6_2atmpS2270);
  }
  if (_M0L6_2aoldS3662) {
    moonbit_decref(_M0L6_2aoldS3662);
  }
  _M0L7entriesS2269[_M0L8new__idxS498] = _M0L6_2atmpS2270;
  _M0L7_2abindS500 = _M0L5entryS499->$1;
  if (_M0L7_2abindS500 == 0) {
    _M0L4selfS497->$6 = _M0L8new__idxS498;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS501 = _M0L7_2abindS500;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS502 = _M0L7_2aSomeS501;
    _M0L7_2anextS502->$0 = _M0L8new__idxS498;
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS454,
  int32_t _M0L3idxS456,
  struct _M0TPB5EntryGsfE* _M0L5entryS455
) {
  int32_t _M0L7_2abindS453;
  struct _M0TPB5EntryGsfE** _M0L7entriesS2221;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2222;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3664;
  int32_t _M0L4sizeS2224;
  int32_t _M0L6_2atmpS2223;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS453 = _M0L4selfS454->$6;
  switch (_M0L7_2abindS453) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2216 = _M0L5entryS455;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3666 = _M0L4selfS454->$5;
      if (_M0L6_2atmpS2216) {
        moonbit_incref(_M0L6_2atmpS2216);
      }
      if (_M0L6_2aoldS3666) {
        moonbit_decref(_M0L6_2aoldS3666);
      }
      _M0L4selfS454->$5 = _M0L6_2atmpS2216;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2220 = _M0L4selfS454->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2219;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2217;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2218;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3667;
      if (
        _M0L7_2abindS453 < 0
        || _M0L7_2abindS453 >= Moonbit_array_length(_M0L7entriesS2220)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2219
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2220[_M0L7_2abindS453];
      if (_M0L6_2atmpS2219) {
        moonbit_incref(_M0L6_2atmpS2219);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2217
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2219);
      if (_M0L6_2atmpS2219) {
        moonbit_decref(_M0L6_2atmpS2219);
      }
      _M0L6_2atmpS2218 = _M0L5entryS455;
      _M0L6_2aoldS3667 = _M0L6_2atmpS2217->$1;
      if (_M0L6_2atmpS2218) {
        moonbit_incref(_M0L6_2atmpS2218);
      }
      if (_M0L6_2aoldS3667) {
        moonbit_decref(_M0L6_2aoldS3667);
      }
      _M0L6_2atmpS2217->$1 = _M0L6_2atmpS2218;
      moonbit_decref(_M0L6_2atmpS2217);
      break;
    }
  }
  _M0L4selfS454->$6 = _M0L3idxS456;
  _M0L7entriesS2221 = _M0L4selfS454->$0;
  _M0L6_2atmpS2222 = _M0L5entryS455;
  if (
    _M0L3idxS456 < 0
    || _M0L3idxS456 >= Moonbit_array_length(_M0L7entriesS2221)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3664
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2221[_M0L3idxS456];
  if (_M0L6_2atmpS2222) {
    moonbit_incref(_M0L6_2atmpS2222);
  }
  if (_M0L6_2aoldS3664) {
    moonbit_decref(_M0L6_2aoldS3664);
  }
  _M0L7entriesS2221[_M0L3idxS456] = _M0L6_2atmpS2222;
  _M0L4sizeS2224 = _M0L4selfS454->$1;
  _M0L6_2atmpS2223 = _M0L4sizeS2224 + 1;
  _M0L4selfS454->$1 = _M0L6_2atmpS2223;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS458,
  int32_t _M0L3idxS460,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS459
) {
  int32_t _M0L7_2abindS457;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2230;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2231;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3670;
  int32_t _M0L4sizeS2233;
  int32_t _M0L6_2atmpS2232;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS457 = _M0L4selfS458->$6;
  switch (_M0L7_2abindS457) {
    case -1: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2225 =
        _M0L5entryS459;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3672 =
        _M0L4selfS458->$5;
      if (_M0L6_2atmpS2225) {
        moonbit_incref(_M0L6_2atmpS2225);
      }
      if (_M0L6_2aoldS3672) {
        moonbit_decref(_M0L6_2aoldS3672);
      }
      _M0L4selfS458->$5 = _M0L6_2atmpS2225;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2229 =
        _M0L4selfS458->$0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2228;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2226;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2227;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3673;
      if (
        _M0L7_2abindS457 < 0
        || _M0L7_2abindS457 >= Moonbit_array_length(_M0L7entriesS2229)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2228
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2229[
          _M0L7_2abindS457
        ];
      if (_M0L6_2atmpS2228) {
        moonbit_incref(_M0L6_2atmpS2228);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2226
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2228);
      if (_M0L6_2atmpS2228) {
        moonbit_decref(_M0L6_2atmpS2228);
      }
      _M0L6_2atmpS2227 = _M0L5entryS459;
      _M0L6_2aoldS3673 = _M0L6_2atmpS2226->$1;
      if (_M0L6_2atmpS2227) {
        moonbit_incref(_M0L6_2atmpS2227);
      }
      if (_M0L6_2aoldS3673) {
        moonbit_decref(_M0L6_2aoldS3673);
      }
      _M0L6_2atmpS2226->$1 = _M0L6_2atmpS2227;
      moonbit_decref(_M0L6_2atmpS2226);
      break;
    }
  }
  _M0L4selfS458->$6 = _M0L3idxS460;
  _M0L7entriesS2230 = _M0L4selfS458->$0;
  _M0L6_2atmpS2231 = _M0L5entryS459;
  if (
    _M0L3idxS460 < 0
    || _M0L3idxS460 >= Moonbit_array_length(_M0L7entriesS2230)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3670
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2230[
      _M0L3idxS460
    ];
  if (_M0L6_2atmpS2231) {
    moonbit_incref(_M0L6_2atmpS2231);
  }
  if (_M0L6_2aoldS3670) {
    moonbit_decref(_M0L6_2aoldS3670);
  }
  _M0L7entriesS2230[_M0L3idxS460] = _M0L6_2atmpS2231;
  _M0L4sizeS2233 = _M0L4selfS458->$1;
  _M0L6_2atmpS2232 = _M0L4sizeS2233 + 1;
  _M0L4selfS458->$1 = _M0L6_2atmpS2232;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS462,
  int32_t _M0L3idxS464,
  struct _M0TPB5EntryGssE* _M0L5entryS463
) {
  int32_t _M0L7_2abindS461;
  struct _M0TPB5EntryGssE** _M0L7entriesS2239;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2240;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3676;
  int32_t _M0L4sizeS2242;
  int32_t _M0L6_2atmpS2241;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS461 = _M0L4selfS462->$6;
  switch (_M0L7_2abindS461) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2234 = _M0L5entryS463;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3678 = _M0L4selfS462->$5;
      if (_M0L6_2atmpS2234) {
        moonbit_incref(_M0L6_2atmpS2234);
      }
      if (_M0L6_2aoldS3678) {
        moonbit_decref(_M0L6_2aoldS3678);
      }
      _M0L4selfS462->$5 = _M0L6_2atmpS2234;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS2238 = _M0L4selfS462->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2237;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2235;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2236;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3679;
      if (
        _M0L7_2abindS461 < 0
        || _M0L7_2abindS461 >= Moonbit_array_length(_M0L7entriesS2238)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2237
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS2238[_M0L7_2abindS461];
      if (_M0L6_2atmpS2237) {
        moonbit_incref(_M0L6_2atmpS2237);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2235
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS2237);
      if (_M0L6_2atmpS2237) {
        moonbit_decref(_M0L6_2atmpS2237);
      }
      _M0L6_2atmpS2236 = _M0L5entryS463;
      _M0L6_2aoldS3679 = _M0L6_2atmpS2235->$1;
      if (_M0L6_2atmpS2236) {
        moonbit_incref(_M0L6_2atmpS2236);
      }
      if (_M0L6_2aoldS3679) {
        moonbit_decref(_M0L6_2aoldS3679);
      }
      _M0L6_2atmpS2235->$1 = _M0L6_2atmpS2236;
      moonbit_decref(_M0L6_2atmpS2235);
      break;
    }
  }
  _M0L4selfS462->$6 = _M0L3idxS464;
  _M0L7entriesS2239 = _M0L4selfS462->$0;
  _M0L6_2atmpS2240 = _M0L5entryS463;
  if (
    _M0L3idxS464 < 0
    || _M0L3idxS464 >= Moonbit_array_length(_M0L7entriesS2239)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3676
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2239[_M0L3idxS464];
  if (_M0L6_2atmpS2240) {
    moonbit_incref(_M0L6_2atmpS2240);
  }
  if (_M0L6_2aoldS3676) {
    moonbit_decref(_M0L6_2aoldS3676);
  }
  _M0L7entriesS2239[_M0L3idxS464] = _M0L6_2atmpS2240;
  _M0L4sizeS2242 = _M0L4selfS462->$1;
  _M0L6_2atmpS2241 = _M0L4sizeS2242 + 1;
  _M0L4selfS462->$1 = _M0L6_2atmpS2241;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS466,
  int32_t _M0L3idxS468,
  struct _M0TPB5EntryGsiE* _M0L5entryS467
) {
  int32_t _M0L7_2abindS465;
  struct _M0TPB5EntryGsiE** _M0L7entriesS2248;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2249;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3682;
  int32_t _M0L4sizeS2251;
  int32_t _M0L6_2atmpS2250;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS465 = _M0L4selfS466->$6;
  switch (_M0L7_2abindS465) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2243 = _M0L5entryS467;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3684 = _M0L4selfS466->$5;
      if (_M0L6_2atmpS2243) {
        moonbit_incref(_M0L6_2atmpS2243);
      }
      if (_M0L6_2aoldS3684) {
        moonbit_decref(_M0L6_2aoldS3684);
      }
      _M0L4selfS466->$5 = _M0L6_2atmpS2243;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2247 = _M0L4selfS466->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2246;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2244;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2245;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3685;
      if (
        _M0L7_2abindS465 < 0
        || _M0L7_2abindS465 >= Moonbit_array_length(_M0L7entriesS2247)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2246
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2247[_M0L7_2abindS465];
      if (_M0L6_2atmpS2246) {
        moonbit_incref(_M0L6_2atmpS2246);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2244
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2246);
      if (_M0L6_2atmpS2246) {
        moonbit_decref(_M0L6_2atmpS2246);
      }
      _M0L6_2atmpS2245 = _M0L5entryS467;
      _M0L6_2aoldS3685 = _M0L6_2atmpS2244->$1;
      if (_M0L6_2atmpS2245) {
        moonbit_incref(_M0L6_2atmpS2245);
      }
      if (_M0L6_2aoldS3685) {
        moonbit_decref(_M0L6_2aoldS3685);
      }
      _M0L6_2atmpS2244->$1 = _M0L6_2atmpS2245;
      moonbit_decref(_M0L6_2atmpS2244);
      break;
    }
  }
  _M0L4selfS466->$6 = _M0L3idxS468;
  _M0L7entriesS2248 = _M0L4selfS466->$0;
  _M0L6_2atmpS2249 = _M0L5entryS467;
  if (
    _M0L3idxS468 < 0
    || _M0L3idxS468 >= Moonbit_array_length(_M0L7entriesS2248)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3682
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2248[_M0L3idxS468];
  if (_M0L6_2atmpS2249) {
    moonbit_incref(_M0L6_2atmpS2249);
  }
  if (_M0L6_2aoldS3682) {
    moonbit_decref(_M0L6_2aoldS3682);
  }
  _M0L7entriesS2248[_M0L3idxS468] = _M0L6_2atmpS2249;
  _M0L4sizeS2251 = _M0L4selfS466->$1;
  _M0L6_2atmpS2250 = _M0L4sizeS2251 + 1;
  _M0L4selfS466->$1 = _M0L6_2atmpS2250;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS470,
  int32_t _M0L3idxS472,
  struct _M0TPB5EntryGsbE* _M0L5entryS471
) {
  int32_t _M0L7_2abindS469;
  struct _M0TPB5EntryGsbE** _M0L7entriesS2257;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2258;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3688;
  int32_t _M0L4sizeS2260;
  int32_t _M0L6_2atmpS2259;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS469 = _M0L4selfS470->$6;
  switch (_M0L7_2abindS469) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2252 = _M0L5entryS471;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3690 = _M0L4selfS470->$5;
      if (_M0L6_2atmpS2252) {
        moonbit_incref(_M0L6_2atmpS2252);
      }
      if (_M0L6_2aoldS3690) {
        moonbit_decref(_M0L6_2aoldS3690);
      }
      _M0L4selfS470->$5 = _M0L6_2atmpS2252;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2256 = _M0L4selfS470->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2255;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2253;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2254;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3691;
      if (
        _M0L7_2abindS469 < 0
        || _M0L7_2abindS469 >= Moonbit_array_length(_M0L7entriesS2256)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2255
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2256[_M0L7_2abindS469];
      if (_M0L6_2atmpS2255) {
        moonbit_incref(_M0L6_2atmpS2255);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2253
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2255);
      if (_M0L6_2atmpS2255) {
        moonbit_decref(_M0L6_2atmpS2255);
      }
      _M0L6_2atmpS2254 = _M0L5entryS471;
      _M0L6_2aoldS3691 = _M0L6_2atmpS2253->$1;
      if (_M0L6_2atmpS2254) {
        moonbit_incref(_M0L6_2atmpS2254);
      }
      if (_M0L6_2aoldS3691) {
        moonbit_decref(_M0L6_2aoldS3691);
      }
      _M0L6_2atmpS2253->$1 = _M0L6_2atmpS2254;
      moonbit_decref(_M0L6_2atmpS2253);
      break;
    }
  }
  _M0L4selfS470->$6 = _M0L3idxS472;
  _M0L7entriesS2257 = _M0L4selfS470->$0;
  _M0L6_2atmpS2258 = _M0L5entryS471;
  if (
    _M0L3idxS472 < 0
    || _M0L3idxS472 >= Moonbit_array_length(_M0L7entriesS2257)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3688
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2257[_M0L3idxS472];
  if (_M0L6_2atmpS2258) {
    moonbit_incref(_M0L6_2atmpS2258);
  }
  if (_M0L6_2aoldS3688) {
    moonbit_decref(_M0L6_2aoldS3688);
  }
  _M0L7entriesS2257[_M0L3idxS472] = _M0L6_2atmpS2258;
  _M0L4sizeS2260 = _M0L4selfS470->$1;
  _M0L6_2atmpS2259 = _M0L4sizeS2260 + 1;
  _M0L4selfS470->$1 = _M0L6_2atmpS2259;
  return 0;
}

int32_t _M0MPC13int3Int3max(int32_t _M0L4selfS451, int32_t _M0L5otherS452) {
  #line 75 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS451 > _M0L5otherS452) {
    return _M0L4selfS451;
  } else {
    return _M0L5otherS452;
  }
}

int32_t _M0FPB21capacity__for__length(int32_t _M0L6lengthS450) {
  int32_t _M0Lm8capacityS449;
  int32_t _M0L6_2atmpS2214;
  int32_t _M0L6_2atmpS2213;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS449 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS450);
  _M0L6_2atmpS2214 = _M0Lm8capacityS449;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2213 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2214);
  if (_M0L6lengthS450 > _M0L6_2atmpS2213) {
    int32_t _M0L6_2atmpS2215 = _M0Lm8capacityS449;
    _M0Lm8capacityS449 = _M0L6_2atmpS2215 * 2;
  }
  return _M0Lm8capacityS449;
}

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  int32_t _M0L8capacityS420
) {
  int32_t _M0L8capacityS419;
  int32_t _M0L7_2abindS421;
  int32_t _M0L7_2abindS422;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2208;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS423;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS424;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _block_4007;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS419
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS420);
  _M0L7_2abindS421 = _M0L8capacityS419 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS422 = _M0FPB21calc__grow__threshold(_M0L8capacityS419);
  _M0L6_2atmpS2208 = 0;
  _M0L7_2abindS423
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS419, _M0L6_2atmpS2208);
  _M0L7_2abindS424 = 0;
  _block_4007
  = (struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
  Moonbit_object_header(_block_4007)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 83, 0);
  _block_4007->$0 = _M0L7_2abindS423;
  _block_4007->$1 = 0;
  _block_4007->$2 = _M0L8capacityS419;
  _block_4007->$3 = _M0L7_2abindS421;
  _block_4007->$4 = _M0L7_2abindS422;
  _block_4007->$5 = _M0L7_2abindS424;
  _block_4007->$6 = -1;
  return _block_4007;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS426) {
  int32_t _M0L8capacityS425;
  int32_t _M0L7_2abindS427;
  int32_t _M0L7_2abindS428;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2209;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS429;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS430;
  struct _M0TPB3MapGsiE* _block_4008;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS425
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS426);
  _M0L7_2abindS427 = _M0L8capacityS425 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS428 = _M0FPB21calc__grow__threshold(_M0L8capacityS425);
  _M0L6_2atmpS2209 = 0;
  _M0L7_2abindS429
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS425, _M0L6_2atmpS2209);
  _M0L7_2abindS430 = 0;
  _block_4008
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_4008)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 87, 0);
  _block_4008->$0 = _M0L7_2abindS429;
  _block_4008->$1 = 0;
  _block_4008->$2 = _M0L8capacityS425;
  _block_4008->$3 = _M0L7_2abindS427;
  _block_4008->$4 = _M0L7_2abindS428;
  _block_4008->$5 = _M0L7_2abindS430;
  _block_4008->$6 = -1;
  return _block_4008;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS432) {
  int32_t _M0L8capacityS431;
  int32_t _M0L7_2abindS433;
  int32_t _M0L7_2abindS434;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2210;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS435;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS436;
  struct _M0TPB3MapGsfE* _block_4009;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS431
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS432);
  _M0L7_2abindS433 = _M0L8capacityS431 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS434 = _M0FPB21calc__grow__threshold(_M0L8capacityS431);
  _M0L6_2atmpS2210 = 0;
  _M0L7_2abindS435
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS431, _M0L6_2atmpS2210);
  _M0L7_2abindS436 = 0;
  _block_4009
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_4009)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 91, 0);
  _block_4009->$0 = _M0L7_2abindS435;
  _block_4009->$1 = 0;
  _block_4009->$2 = _M0L8capacityS431;
  _block_4009->$3 = _M0L7_2abindS433;
  _block_4009->$4 = _M0L7_2abindS434;
  _block_4009->$5 = _M0L7_2abindS436;
  _block_4009->$6 = -1;
  return _block_4009;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS438) {
  int32_t _M0L8capacityS437;
  int32_t _M0L7_2abindS439;
  int32_t _M0L7_2abindS440;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2211;
  struct _M0TPB5EntryGssE** _M0L7_2abindS441;
  struct _M0TPB5EntryGssE* _M0L7_2abindS442;
  struct _M0TPB3MapGssE* _block_4010;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS437
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS438);
  _M0L7_2abindS439 = _M0L8capacityS437 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS440 = _M0FPB21calc__grow__threshold(_M0L8capacityS437);
  _M0L6_2atmpS2211 = 0;
  _M0L7_2abindS441
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS437, _M0L6_2atmpS2211);
  _M0L7_2abindS442 = 0;
  _block_4010
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_4010)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 95, 0);
  _block_4010->$0 = _M0L7_2abindS441;
  _block_4010->$1 = 0;
  _block_4010->$2 = _M0L8capacityS437;
  _block_4010->$3 = _M0L7_2abindS439;
  _block_4010->$4 = _M0L7_2abindS440;
  _block_4010->$5 = _M0L7_2abindS442;
  _block_4010->$6 = -1;
  return _block_4010;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS444) {
  int32_t _M0L8capacityS443;
  int32_t _M0L7_2abindS445;
  int32_t _M0L7_2abindS446;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2212;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS447;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS448;
  struct _M0TPB3MapGsbE* _block_4011;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS443
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS444);
  _M0L7_2abindS445 = _M0L8capacityS443 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS446 = _M0FPB21calc__grow__threshold(_M0L8capacityS443);
  _M0L6_2atmpS2212 = 0;
  _M0L7_2abindS447
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS443, _M0L6_2atmpS2212);
  _M0L7_2abindS448 = 0;
  _block_4011
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_4011)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 99, 0);
  _block_4011->$0 = _M0L7_2abindS447;
  _block_4011->$1 = 0;
  _block_4011->$2 = _M0L8capacityS443;
  _block_4011->$3 = _M0L7_2abindS445;
  _block_4011->$4 = _M0L7_2abindS446;
  _block_4011->$5 = _M0L7_2abindS448;
  _block_4011->$6 = -1;
  return _block_4011;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS418) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS418 >= 0) {
    int32_t _M0L6_2atmpS2207;
    int32_t _M0L6_2atmpS2206;
    int32_t _M0L6_2atmpS2205;
    int32_t _M0L6_2atmpS2204;
    if (_M0L4selfS418 <= 1) {
      return 1;
    }
    if (_M0L4selfS418 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2207 = _M0L4selfS418 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2206 = moonbit_clz32(_M0L6_2atmpS2207);
    _M0L6_2atmpS2205 = _M0L6_2atmpS2206 - 1;
    _M0L6_2atmpS2204 = 2147483647 >> (_M0L6_2atmpS2205 & 31);
    return _M0L6_2atmpS2204 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS417) {
  int32_t _M0L6_2atmpS2203;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2203 = _M0L8capacityS417 * 13;
  return _M0L6_2atmpS2203 / 16;
}

int32_t _M0MPC16option6Option6unwrapGiE(int64_t _M0L4selfS405) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS405 == 4294967296ll) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    int64_t _M0L7_2aSomeS406 = _M0L4selfS405;
    return (int32_t)_M0L7_2aSomeS406;
  }
}

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE* _M0L4selfS407
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS407 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS408 = _M0L4selfS407;
    if (_M0L7_2aSomeS408) {
      moonbit_incref(_M0L7_2aSomeS408);
    }
    return _M0L7_2aSomeS408;
  }
}

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS409
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS409 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS410 =
      _M0L4selfS409;
    if (_M0L7_2aSomeS410) {
      moonbit_incref(_M0L7_2aSomeS410);
    }
    return _M0L7_2aSomeS410;
  }
}

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE* _M0L4selfS411
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS411 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS412 = _M0L4selfS411;
    if (_M0L7_2aSomeS412) {
      moonbit_incref(_M0L7_2aSomeS412);
    }
    return _M0L7_2aSomeS412;
  }
}

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE* _M0L4selfS413
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS413 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS414 = _M0L4selfS413;
    if (_M0L7_2aSomeS414) {
      moonbit_incref(_M0L7_2aSomeS414);
    }
    return _M0L7_2aSomeS414;
  }
}

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE* _M0L4selfS415
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS415 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS416 = _M0L4selfS415;
    if (_M0L7_2aSomeS416) {
      moonbit_incref(_M0L7_2aSomeS416);
    }
    return _M0L7_2aSomeS416;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS379,
  struct _M0TPC16string10StringView _M0L9separatorS392
) {
  int32_t _M0L3endS2178;
  int32_t _M0L5startS2179;
  int32_t _M0L6_2atmpS2177;
  #line 1497 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2178 = _M0L4selfS379.$2;
  _M0L5startS2179 = _M0L4selfS379.$1;
  _M0L6_2atmpS2177 = _M0L3endS2178 - _M0L5startS2179;
  if (_M0L6_2atmpS2177 == 0) {
    return (moonbit_string_t)moonbit_string_literal_82.data;
  } else {
    moonbit_string_t* _M0L3bufS2201 = _M0L4selfS379.$0;
    int32_t _M0L5startS2202 = _M0L4selfS379.$1;
    moonbit_string_t _M0L5_2ahdS380 =
      (moonbit_string_t)_M0L3bufS2201[_M0L5startS2202];
    moonbit_string_t* _M0L9_2ax__bufS381 = _M0L4selfS379.$0;
    int32_t _M0L5startS2200 = _M0L4selfS379.$1;
    int32_t _M0L11_2ax__startS382 = 1 + _M0L5startS2200;
    int32_t _M0L9_2ax__endS383 = _M0L4selfS379.$2;
    struct _M0TPC16string10StringView _M0L2hdS384;
    int32_t _M0L7_2abindS385;
    int32_t _M0L3endS2198;
    int32_t _M0L5startS2199;
    int32_t _M0L6_2atmpS2197;
    int32_t _M0L10size__hintS386;
    int32_t _M0L2__S387;
    int32_t _M0L10size__hintS388;
    int32_t _M0L10size__hintS393;
    struct _M0TPB13StringBuilder* _M0L3bufS394;
    int32_t _M0L3endS2181;
    int32_t _M0L5startS2182;
    int32_t _M0L6_2atmpS2180;
    moonbit_string_t _result_4015;
    moonbit_incref(_M0L9_2ax__bufS381);
    moonbit_incref(_M0L5_2ahdS380);
    #line 1504 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS384
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS380);
    moonbit_decref(_M0L5_2ahdS380);
    _M0L7_2abindS385 = _M0L9_2ax__endS383 - _M0L11_2ax__startS382;
    _M0L3endS2198 = _M0L2hdS384.$2;
    _M0L5startS2199 = _M0L2hdS384.$1;
    _M0L6_2atmpS2197 = _M0L3endS2198 - _M0L5startS2199;
    _M0L2__S387 = 0;
    _M0L10size__hintS388 = _M0L6_2atmpS2197;
    while (1) {
      if (_M0L2__S387 < _M0L7_2abindS385) {
        int32_t _M0L6_2atmpS2196 = _M0L11_2ax__startS382 + _M0L2__S387;
        moonbit_string_t _M0L1sS389 =
          (moonbit_string_t)_M0L9_2ax__bufS381[_M0L6_2atmpS2196];
        int32_t _M0L6_2atmpS2187 = _M0L2__S387 + 1;
        struct _M0TPC16string10StringView _M0L7_2abindS391;
        int32_t _M0L3endS2194;
        int32_t _M0L5startS2195;
        int32_t _M0L6_2atmpS2193;
        int32_t _M0L6_2atmpS2189;
        int32_t _M0L3endS2191;
        int32_t _M0L5startS2192;
        int32_t _M0L6_2atmpS2190;
        int32_t _M0L6_2atmpS2188;
        moonbit_incref(_M0L1sS389);
        #line 1506 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
        _M0L7_2abindS391
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS389);
        moonbit_decref(_M0L1sS389);
        _M0L3endS2194 = _M0L7_2abindS391.$2;
        _M0L5startS2195 = _M0L7_2abindS391.$1;
        moonbit_decref(_M0L7_2abindS391.$0);
        _M0L6_2atmpS2193 = _M0L3endS2194 - _M0L5startS2195;
        _M0L6_2atmpS2189 = _M0L10size__hintS388 + _M0L6_2atmpS2193;
        _M0L3endS2191 = _M0L9separatorS392.$2;
        _M0L5startS2192 = _M0L9separatorS392.$1;
        _M0L6_2atmpS2190 = _M0L3endS2191 - _M0L5startS2192;
        _M0L6_2atmpS2188 = _M0L6_2atmpS2189 + _M0L6_2atmpS2190;
        _M0L2__S387 = _M0L6_2atmpS2187;
        _M0L10size__hintS388 = _M0L6_2atmpS2188;
        continue;
      } else {
        _M0L10size__hintS386 = _M0L10size__hintS388;
      }
      break;
    }
    _M0L10size__hintS393 = _M0L10size__hintS386 << 1;
    #line 1511 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS394
    = _M0MPB13StringBuilder21StringBuilder_2einner(_M0L10size__hintS393);
    #line 1513 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS394, _M0L2hdS384);
    moonbit_decref(_M0L2hdS384.$0);
    _M0L3endS2181 = _M0L9separatorS392.$2;
    _M0L5startS2182 = _M0L9separatorS392.$1;
    _M0L6_2atmpS2180 = _M0L3endS2181 - _M0L5startS2182;
    if (_M0L6_2atmpS2180 == 0) {
      int32_t _M0L7_2abindS395 = _M0L9_2ax__endS383 - _M0L11_2ax__startS382;
      int32_t _M0L2__S396 = 0;
      while (1) {
        if (_M0L2__S396 < _M0L7_2abindS395) {
          int32_t _M0L6_2atmpS2184 = _M0L11_2ax__startS382 + _M0L2__S396;
          moonbit_string_t _M0L1sS397 =
            (moonbit_string_t)_M0L9_2ax__bufS381[_M0L6_2atmpS2184];
          struct _M0TPC16string10StringView _M0L1sS398;
          int32_t _M0L6_2atmpS2183;
          moonbit_incref(_M0L1sS397);
          #line 1517 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS398
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS397);
          moonbit_decref(_M0L1sS397);
          #line 1518 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS394, _M0L1sS398);
          moonbit_decref(_M0L1sS398.$0);
          _M0L6_2atmpS2183 = _M0L2__S396 + 1;
          _M0L2__S396 = _M0L6_2atmpS2183;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS381);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS400 = _M0L9_2ax__endS383 - _M0L11_2ax__startS382;
      int32_t _M0L2__S401 = 0;
      while (1) {
        if (_M0L2__S401 < _M0L7_2abindS400) {
          int32_t _M0L6_2atmpS2186 = _M0L11_2ax__startS382 + _M0L2__S401;
          moonbit_string_t _M0L1sS402 =
            (moonbit_string_t)_M0L9_2ax__bufS381[_M0L6_2atmpS2186];
          struct _M0TPC16string10StringView _M0L1sS403;
          int32_t _M0L6_2atmpS2185;
          moonbit_incref(_M0L1sS402);
          #line 1522 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS403
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS402);
          moonbit_decref(_M0L1sS402);
          #line 1523 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS394, _M0L9separatorS392);
          #line 1525 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS394, _M0L1sS403);
          moonbit_decref(_M0L1sS403.$0);
          _M0L6_2atmpS2185 = _M0L2__S401 + 1;
          _M0L2__S401 = _M0L6_2atmpS2185;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS381);
        }
        break;
      }
    }
    #line 1528 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _result_4015 = _M0MPB13StringBuilder10to__string(_M0L3bufS394);
    moonbit_decref(_M0L3bufS394);
    return _result_4015;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS375,
  int32_t _M0L5indexS376
) {
  uint64_t* _M0L6_2atmpS2175;
  uint64_t _result_4016;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS375);
  _M0L6_2atmpS2175 = _M0L4selfS375;
  if (
    _M0L5indexS376 < 0
    || _M0L5indexS376 >= Moonbit_array_length(_M0L6_2atmpS2175)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4016 = (uint64_t)_M0L6_2atmpS2175[_M0L5indexS376];
  moonbit_decref(_M0L6_2atmpS2175);
  return _result_4016;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS377,
  int32_t _M0L5indexS378
) {
  uint32_t* _M0L6_2atmpS2176;
  uint32_t _result_4017;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS377);
  _M0L6_2atmpS2176 = _M0L4selfS377;
  if (
    _M0L5indexS378 < 0
    || _M0L5indexS378 >= Moonbit_array_length(_M0L6_2atmpS2176)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4017 = (uint32_t)_M0L6_2atmpS2176[_M0L5indexS378];
  moonbit_decref(_M0L6_2atmpS2176);
  return _result_4017;
}

moonbit_string_t _M0IPC16uint646UInt64PB4Show10to__string(
  uint64_t _M0L4selfS374
) {
  #line 50 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 51 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS374, 10);
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS373) {
  #line 35 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 36 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS373, 10);
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS372) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS372;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS371
) {
  int32_t _M0L6_2atmpS2174;
  #line 24 "/home/developer/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2174 = Moonbit_array_length(_M0L4selfS371);
  moonbit_incref(_M0L4selfS371);
  return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS371,
                                               .$1 = 0,
                                               .$2 = _M0L6_2atmpS2174};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS365,
  moonbit_string_t _M0L5valueS367
) {
  int32_t _M0L3lenS2164;
  moonbit_string_t* _M0L6_2atmpS2166;
  int32_t _M0L6_2atmpS2165;
  int32_t _M0L6lengthS366;
  moonbit_string_t* _M0L3bufS2167;
  moonbit_string_t _M0L6_2aoldS3700;
  int32_t _M0L6_2atmpS2168;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2164 = _M0L4selfS365->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2166 = _M0MPC15array5Array6bufferGsE(_M0L4selfS365);
  _M0L6_2atmpS2165 = Moonbit_array_length(_M0L6_2atmpS2166);
  moonbit_decref(_M0L6_2atmpS2166);
  if (_M0L3lenS2164 == _M0L6_2atmpS2165) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS365);
  }
  _M0L6lengthS366 = _M0L4selfS365->$1;
  _M0L3bufS2167 = _M0L4selfS365->$0;
  _M0L6_2aoldS3700 = (moonbit_string_t)_M0L3bufS2167[_M0L6lengthS366];
  moonbit_incref(_M0L5valueS367);
  moonbit_decref(_M0L6_2aoldS3700);
  _M0L3bufS2167[_M0L6lengthS366] = _M0L5valueS367;
  _M0L6_2atmpS2168 = _M0L6lengthS366 + 1;
  _M0L4selfS365->$1 = _M0L6_2atmpS2168;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS368,
  struct _M0TUsfE* _M0L5valueS370
) {
  int32_t _M0L3lenS2169;
  struct _M0TUsfE** _M0L6_2atmpS2171;
  int32_t _M0L6_2atmpS2170;
  int32_t _M0L6lengthS369;
  struct _M0TUsfE** _M0L3bufS2172;
  struct _M0TUsfE* _M0L6_2aoldS3702;
  int32_t _M0L6_2atmpS2173;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2169 = _M0L4selfS368->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2171 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS368);
  _M0L6_2atmpS2170 = Moonbit_array_length(_M0L6_2atmpS2171);
  moonbit_decref(_M0L6_2atmpS2171);
  if (_M0L3lenS2169 == _M0L6_2atmpS2170) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS368);
  }
  _M0L6lengthS369 = _M0L4selfS368->$1;
  _M0L3bufS2172 = _M0L4selfS368->$0;
  _M0L6_2aoldS3702 = (struct _M0TUsfE*)_M0L3bufS2172[_M0L6lengthS369];
  moonbit_incref(_M0L5valueS370);
  if (_M0L6_2aoldS3702) {
    moonbit_decref(_M0L6_2aoldS3702);
  }
  _M0L3bufS2172[_M0L6lengthS369] = _M0L5valueS370;
  _M0L6_2atmpS2173 = _M0L6lengthS369 + 1;
  _M0L4selfS368->$1 = _M0L6_2atmpS2173;
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS360) {
  int32_t _M0L8old__capS359;
  int32_t _M0L8new__capS361;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS359 = _M0L4selfS360->$1;
  if (_M0L8old__capS359 == 0) {
    _M0L8new__capS361 = 8;
  } else {
    _M0L8new__capS361 = _M0L8old__capS359 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS360, _M0L8new__capS361);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS363
) {
  int32_t _M0L8old__capS362;
  int32_t _M0L8new__capS364;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS362 = _M0L4selfS363->$1;
  if (_M0L8old__capS362 == 0) {
    _M0L8new__capS364 = 8;
  } else {
    _M0L8new__capS364 = _M0L8old__capS362 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsfEE(_M0L4selfS363, _M0L8new__capS364);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS348,
  int32_t _M0L13new__capacityS351
) {
  moonbit_string_t* _M0L8old__bufS347;
  int32_t _M0L8old__capS349;
  int32_t _M0L9copy__lenS350;
  moonbit_string_t* _M0L8new__bufS352;
  moonbit_string_t* _M0L6_2aoldS3704;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS347 = _M0L4selfS348->$0;
  _M0L8old__capS349 = Moonbit_array_length(_M0L8old__bufS347);
  if (_M0L8old__capS349 < _M0L13new__capacityS351) {
    _M0L9copy__lenS350 = _M0L8old__capS349;
  } else {
    _M0L9copy__lenS350 = _M0L13new__capacityS351;
  }
  moonbit_incref(_M0L8old__bufS347);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS352
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(_M0L8old__bufS347, _M0L13new__capacityS351, _M0L9copy__lenS350, 0, 0);
  moonbit_decref(_M0L8old__bufS347);
  _M0L6_2aoldS3704 = _M0L4selfS348->$0;
  moonbit_decref(_M0L6_2aoldS3704);
  _M0L4selfS348->$0 = _M0L8new__bufS352;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS354,
  int32_t _M0L13new__capacityS357
) {
  struct _M0TUsfE** _M0L8old__bufS353;
  int32_t _M0L8old__capS355;
  int32_t _M0L9copy__lenS356;
  struct _M0TUsfE** _M0L8new__bufS358;
  struct _M0TUsfE** _M0L6_2aoldS3706;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS353 = _M0L4selfS354->$0;
  _M0L8old__capS355 = Moonbit_array_length(_M0L8old__bufS353);
  if (_M0L8old__capS355 < _M0L13new__capacityS357) {
    _M0L9copy__lenS356 = _M0L8old__capS355;
  } else {
    _M0L9copy__lenS356 = _M0L13new__capacityS357;
  }
  moonbit_incref(_M0L8old__bufS353);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS358
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(_M0L8old__bufS353, _M0L13new__capacityS357, _M0L9copy__lenS356, 0, 0);
  moonbit_decref(_M0L8old__bufS353);
  _M0L6_2aoldS3706 = _M0L4selfS354->$0;
  moonbit_decref(_M0L6_2aoldS3706);
  _M0L4selfS354->$0 = _M0L8new__bufS358;
  return 0;
}

int32_t _M0MPC15array5Array6lengthGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE* _M0L4selfS344
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS344->$1;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS345) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS345->$1;
}

int32_t _M0MPC15array5Array6lengthGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS346
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS346->$1;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS343
) {
  #line 222 "/home/developer/.moon/lib/core/builtin/show.mbt"
  moonbit_incref(_M0L4selfS343);
  return _M0L4selfS343;
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS342,
  struct _M0TPC16string10StringView _M0L3strS341
) {
  int32_t _M0L3endS2162;
  int32_t _M0L5startS2163;
  int32_t _M0L8str__lenS340;
  int32_t _M0L3lenS2155;
  int32_t _M0L6_2atmpS2154;
  uint16_t* _M0L4dataS2156;
  int32_t _M0L3lenS2157;
  moonbit_string_t _M0L6_2atmpS2158;
  int32_t _M0L6_2atmpS2159;
  int32_t _M0L3lenS2161;
  int32_t _M0L6_2atmpS2160;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2162 = _M0L3strS341.$2;
  _M0L5startS2163 = _M0L3strS341.$1;
  _M0L8str__lenS340 = _M0L3endS2162 - _M0L5startS2163;
  _M0L3lenS2155 = _M0L4selfS342->$1;
  _M0L6_2atmpS2154 = _M0L3lenS2155 + _M0L8str__lenS340;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS342, _M0L6_2atmpS2154);
  _M0L4dataS2156 = _M0L4selfS342->$0;
  _M0L3lenS2157 = _M0L4selfS342->$1;
  moonbit_incref(_M0L4dataS2156);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2158 = _M0MPC16string10StringView4data(_M0L3strS341);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2159 = _M0MPC16string10StringView13start__offset(_M0L3strS341);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2156, _M0L3lenS2157, _M0L6_2atmpS2158, _M0L6_2atmpS2159, _M0L8str__lenS340);
  moonbit_decref(_M0L4dataS2156);
  moonbit_decref(_M0L6_2atmpS2158);
  _M0L3lenS2161 = _M0L4selfS342->$1;
  _M0L6_2atmpS2160 = _M0L3lenS2161 + _M0L8str__lenS340;
  _M0L4selfS342->$1 = _M0L6_2atmpS2160;
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player** _M0MPC15array5Array6bufferGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE* _M0L4selfS337
) {
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player** _M0L8_2afieldS3709;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3709 = _M0L4selfS337->$0;
  moonbit_incref(_M0L8_2afieldS3709);
  return _M0L8_2afieldS3709;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS338
) {
  moonbit_string_t* _M0L8_2afieldS3710;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3710 = _M0L4selfS338->$0;
  moonbit_incref(_M0L8_2afieldS3710);
  return _M0L8_2afieldS3710;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS339
) {
  struct _M0TUsfE** _M0L8_2afieldS3711;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3711 = _M0L4selfS339->$0;
  moonbit_incref(_M0L8_2afieldS3711);
  return _M0L8_2afieldS3711;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS326,
  int64_t _M0L10size__hintS323
) {
  int64_t _M0L10size__hintS322;
  struct _M0TPB4IterGUsbEE* _block_4018;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS323 == 4294967296ll) {
    _M0L10size__hintS322 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS324 = _M0L10size__hintS323;
    int32_t _M0L4_2anS325 = (int32_t)_M0L7_2aSomeS324;
    if (_M0L4_2anS325 > 0) {
      _M0L10size__hintS322 = (int64_t)_M0L4_2anS325;
    } else {
      _M0L10size__hintS322 = _M0MPB4Iter3newN6constrS9988GUsbEE;
    }
  }
  moonbit_incref(_M0L1fS326);
  _block_4018
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_4018)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 103, 0);
  _block_4018->$0 = _M0L1fS326;
  _block_4018->$1 = _M0L10size__hintS322;
  return _block_4018;
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1fS331,
  int64_t _M0L10size__hintS328
) {
  int64_t _M0L10size__hintS327;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _block_4019;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS328 == 4294967296ll) {
    _M0L10size__hintS327 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS329 = _M0L10size__hintS328;
    int32_t _M0L4_2anS330 = (int32_t)_M0L7_2aSomeS329;
    if (_M0L4_2anS330 > 0) {
      _M0L10size__hintS327 = (int64_t)_M0L4_2anS330;
    } else {
      _M0L10size__hintS327
      = _M0MPB4Iter3newN6constrS9988GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
    }
  }
  moonbit_incref(_M0L1fS331);
  _block_4019
  = (struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_block_4019)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 106, 0);
  _block_4019->$0 = _M0L1fS331;
  _block_4019->$1 = _M0L10size__hintS327;
  return _block_4019;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS336,
  int64_t _M0L10size__hintS333
) {
  int64_t _M0L10size__hintS332;
  struct _M0TPB4IterGUsfEE* _block_4020;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS333 == 4294967296ll) {
    _M0L10size__hintS332 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS334 = _M0L10size__hintS333;
    int32_t _M0L4_2anS335 = (int32_t)_M0L7_2aSomeS334;
    if (_M0L4_2anS335 > 0) {
      _M0L10size__hintS332 = (int64_t)_M0L4_2anS335;
    } else {
      _M0L10size__hintS332 = _M0MPB4Iter3newN6constrS9988GUsfEE;
    }
  }
  moonbit_incref(_M0L1fS336);
  _block_4020
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_4020)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 109, 0);
  _block_4020->$0 = _M0L1fS336;
  _block_4020->$1 = _M0L10size__hintS332;
  return _block_4020;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS314,
  int32_t _M0L5radixS313
) {
  int32_t _if__result_4021;
  uint16_t* _M0L6bufferS315;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS313 < 2) {
    _if__result_4021 = 1;
  } else {
    _if__result_4021 = _M0L5radixS313 > 36;
  }
  if (_if__result_4021) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_89.data);
  }
  if (_M0L4selfS314 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_83.data;
  }
  switch (_M0L5radixS313) {
    case 10: {
      int32_t _M0L3lenS316;
      uint16_t* _M0L6bufferS317;
      #line 622 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS316 = _M0FPB12dec__count64(_M0L4selfS314);
      _M0L6bufferS317 = (uint16_t*)moonbit_make_string(_M0L3lenS316, 0);
      #line 624 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS317, _M0L4selfS314, 0, _M0L3lenS316);
      _M0L6bufferS315 = _M0L6bufferS317;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS318;
      uint16_t* _M0L6bufferS319;
      #line 628 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS318 = _M0FPB12hex__count64(_M0L4selfS314);
      _M0L6bufferS319 = (uint16_t*)moonbit_make_string(_M0L3lenS318, 0);
      #line 630 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS319, _M0L4selfS314, 0, _M0L3lenS318);
      _M0L6bufferS315 = _M0L6bufferS319;
      break;
    }
    default: {
      int32_t _M0L3lenS320;
      uint16_t* _M0L6bufferS321;
      #line 634 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS320 = _M0FPB14radix__count64(_M0L4selfS314, _M0L5radixS313);
      _M0L6bufferS321 = (uint16_t*)moonbit_make_string(_M0L3lenS320, 0);
      #line 636 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS321, _M0L4selfS314, 0, _M0L3lenS320, _M0L5radixS313);
      _M0L6bufferS315 = _M0L6bufferS321;
      break;
    }
  }
  return _M0L6bufferS315;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS299,
  uint64_t _M0L3numS311,
  int32_t _M0L12digit__startS300,
  int32_t _M0L10total__lenS312
) {
  int32_t _M0L6_2atmpS2153;
  uint64_t _M0L3numS289;
  int32_t _M0L6offsetS290;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2153 = _M0L10total__lenS312 - _M0L12digit__startS300;
  _M0L3numS289 = _M0L3numS311;
  _M0L6offsetS290 = _M0L6_2atmpS2153;
  while (1) {
    if (_M0L3numS289 >= 10000ull) {
      uint64_t _M0L1tS291 = _M0L3numS289 / 10000ull;
      uint64_t _M0L6_2atmpS2130 = _M0L3numS289 % 10000ull;
      int32_t _M0L1rS292 = (int32_t)_M0L6_2atmpS2130;
      int32_t _M0L2d1S293 = _M0L1rS292 / 100;
      int32_t _M0L2d2S294 = _M0L1rS292 % 100;
      int32_t _M0L6_2atmpS2129 = _M0L2d1S293 / 10;
      int32_t _M0L6_2atmpS2128 = 48 + _M0L6_2atmpS2129;
      int32_t _M0L6d1__hiS295 = (uint16_t)_M0L6_2atmpS2128;
      int32_t _M0L6_2atmpS2127 = _M0L2d1S293 % 10;
      int32_t _M0L6_2atmpS2126 = 48 + _M0L6_2atmpS2127;
      int32_t _M0L6d1__loS296 = (uint16_t)_M0L6_2atmpS2126;
      int32_t _M0L6_2atmpS2125 = _M0L2d2S294 / 10;
      int32_t _M0L6_2atmpS2124 = 48 + _M0L6_2atmpS2125;
      int32_t _M0L6d2__hiS297 = (uint16_t)_M0L6_2atmpS2124;
      int32_t _M0L6_2atmpS2123 = _M0L2d2S294 % 10;
      int32_t _M0L6_2atmpS2122 = 48 + _M0L6_2atmpS2123;
      int32_t _M0L6d2__loS298 = (uint16_t)_M0L6_2atmpS2122;
      int32_t _M0L6_2atmpS2114 = _M0L12digit__startS300 + _M0L6offsetS290;
      int32_t _M0L6_2atmpS2113 = _M0L6_2atmpS2114 - 4;
      int32_t _M0L6_2atmpS2116;
      int32_t _M0L6_2atmpS2115;
      int32_t _M0L6_2atmpS2118;
      int32_t _M0L6_2atmpS2117;
      int32_t _M0L6_2atmpS2120;
      int32_t _M0L6_2atmpS2119;
      int32_t _M0L6_2atmpS2121;
      _M0L6bufferS299[_M0L6_2atmpS2113] = _M0L6d1__hiS295;
      _M0L6_2atmpS2116 = _M0L12digit__startS300 + _M0L6offsetS290;
      _M0L6_2atmpS2115 = _M0L6_2atmpS2116 - 3;
      _M0L6bufferS299[_M0L6_2atmpS2115] = _M0L6d1__loS296;
      _M0L6_2atmpS2118 = _M0L12digit__startS300 + _M0L6offsetS290;
      _M0L6_2atmpS2117 = _M0L6_2atmpS2118 - 2;
      _M0L6bufferS299[_M0L6_2atmpS2117] = _M0L6d2__hiS297;
      _M0L6_2atmpS2120 = _M0L12digit__startS300 + _M0L6offsetS290;
      _M0L6_2atmpS2119 = _M0L6_2atmpS2120 - 1;
      _M0L6bufferS299[_M0L6_2atmpS2119] = _M0L6d2__loS298;
      _M0L6_2atmpS2121 = _M0L6offsetS290 - 4;
      _M0L3numS289 = _M0L1tS291;
      _M0L6offsetS290 = _M0L6_2atmpS2121;
      continue;
    } else {
      int32_t _M0L6_2atmpS2152 = (int32_t)_M0L3numS289;
      int32_t _M0L9remainingS302 = _M0L6_2atmpS2152;
      int32_t _M0L6offsetS303 = _M0L6offsetS290;
      while (1) {
        if (_M0L9remainingS302 >= 100) {
          int32_t _M0L1tS304 = _M0L9remainingS302 / 100;
          int32_t _M0L1dS305 = _M0L9remainingS302 % 100;
          int32_t _M0L6_2atmpS2139 = _M0L1dS305 / 10;
          int32_t _M0L6_2atmpS2138 = 48 + _M0L6_2atmpS2139;
          int32_t _M0L5d__hiS306 = (uint16_t)_M0L6_2atmpS2138;
          int32_t _M0L6_2atmpS2137 = _M0L1dS305 % 10;
          int32_t _M0L6_2atmpS2136 = 48 + _M0L6_2atmpS2137;
          int32_t _M0L5d__loS307 = (uint16_t)_M0L6_2atmpS2136;
          int32_t _M0L6_2atmpS2132 = _M0L12digit__startS300 + _M0L6offsetS303;
          int32_t _M0L6_2atmpS2131 = _M0L6_2atmpS2132 - 2;
          int32_t _M0L6_2atmpS2134;
          int32_t _M0L6_2atmpS2133;
          int32_t _M0L6_2atmpS2135;
          _M0L6bufferS299[_M0L6_2atmpS2131] = _M0L5d__hiS306;
          _M0L6_2atmpS2134 = _M0L12digit__startS300 + _M0L6offsetS303;
          _M0L6_2atmpS2133 = _M0L6_2atmpS2134 - 1;
          _M0L6bufferS299[_M0L6_2atmpS2133] = _M0L5d__loS307;
          _M0L6_2atmpS2135 = _M0L6offsetS303 - 2;
          _M0L9remainingS302 = _M0L1tS304;
          _M0L6offsetS303 = _M0L6_2atmpS2135;
          continue;
        } else if (_M0L9remainingS302 >= 10) {
          int32_t _M0L6_2atmpS2147 = _M0L9remainingS302 / 10;
          int32_t _M0L6_2atmpS2146 = 48 + _M0L6_2atmpS2147;
          int32_t _M0L5d__hiS309 = (uint16_t)_M0L6_2atmpS2146;
          int32_t _M0L6_2atmpS2145 = _M0L9remainingS302 % 10;
          int32_t _M0L6_2atmpS2144 = 48 + _M0L6_2atmpS2145;
          int32_t _M0L5d__loS310 = (uint16_t)_M0L6_2atmpS2144;
          int32_t _M0L6_2atmpS2141 = _M0L12digit__startS300 + _M0L6offsetS303;
          int32_t _M0L6_2atmpS2140 = _M0L6_2atmpS2141 - 2;
          int32_t _M0L6_2atmpS2143;
          int32_t _M0L6_2atmpS2142;
          _M0L6bufferS299[_M0L6_2atmpS2140] = _M0L5d__hiS309;
          _M0L6_2atmpS2143 = _M0L12digit__startS300 + _M0L6offsetS303;
          _M0L6_2atmpS2142 = _M0L6_2atmpS2143 - 1;
          _M0L6bufferS299[_M0L6_2atmpS2142] = _M0L5d__loS310;
        } else {
          int32_t _M0L6_2atmpS2151 = _M0L12digit__startS300 + _M0L6offsetS303;
          int32_t _M0L6_2atmpS2148 = _M0L6_2atmpS2151 - 1;
          int32_t _M0L6_2atmpS2150 = 48 + _M0L9remainingS302;
          int32_t _M0L6_2atmpS2149 = (uint16_t)_M0L6_2atmpS2150;
          _M0L6bufferS299[_M0L6_2atmpS2148] = _M0L6_2atmpS2149;
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS279,
  uint64_t _M0L3numS283,
  int32_t _M0L12digit__startS280,
  int32_t _M0L10total__lenS282,
  int32_t _M0L5radixS273
) {
  uint64_t _M0L4baseS272;
  int32_t _M0L6_2atmpS2098;
  int32_t _M0L6_2atmpS2097;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS272 = _M0MPC13int3Int10to__uint64(_M0L5radixS273);
  _M0L6_2atmpS2098 = _M0L5radixS273 - 1;
  _M0L6_2atmpS2097 = _M0L5radixS273 & _M0L6_2atmpS2098;
  if (_M0L6_2atmpS2097 == 0) {
    int32_t _M0L5shiftS274;
    uint64_t _M0L4maskS275;
    int32_t _M0L6_2atmpS2105;
    int32_t _M0L6offsetS276;
    uint64_t _M0L1nS277;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS274 = moonbit_ctz32(_M0L5radixS273);
    _M0L4maskS275 = _M0L4baseS272 - 1ull;
    _M0L6_2atmpS2105 = _M0L10total__lenS282 - _M0L12digit__startS280;
    _M0L6offsetS276 = _M0L6_2atmpS2105;
    _M0L1nS277 = _M0L3numS283;
    while (1) {
      if (_M0L1nS277 > 0ull) {
        uint64_t _M0L6_2atmpS2104 = _M0L1nS277 & _M0L4maskS275;
        int32_t _M0L5digitS278 = (int32_t)_M0L6_2atmpS2104;
        int32_t _M0L6_2atmpS2101 = _M0L12digit__startS280 + _M0L6offsetS276;
        int32_t _M0L6_2atmpS2099 = _M0L6_2atmpS2101 - 1;
        int32_t _M0L6_2atmpS2100 =
          ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L5digitS278];
        int32_t _M0L6_2atmpS2102;
        uint64_t _M0L6_2atmpS2103;
        _M0L6bufferS279[_M0L6_2atmpS2099] = _M0L6_2atmpS2100;
        _M0L6_2atmpS2102 = _M0L6offsetS276 - 1;
        _M0L6_2atmpS2103 = _M0L1nS277 >> (_M0L5shiftS274 & 63);
        _M0L6offsetS276 = _M0L6_2atmpS2102;
        _M0L1nS277 = _M0L6_2atmpS2103;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2112 = _M0L10total__lenS282 - _M0L12digit__startS280;
    int32_t _M0L6offsetS284 = _M0L6_2atmpS2112;
    uint64_t _M0L1nS285 = _M0L3numS283;
    while (1) {
      if (_M0L1nS285 > 0ull) {
        uint64_t _M0L1qS286 = _M0L1nS285 / _M0L4baseS272;
        uint64_t _M0L6_2atmpS2111 = _M0L1qS286 * _M0L4baseS272;
        uint64_t _M0L6_2atmpS2110 = _M0L1nS285 - _M0L6_2atmpS2111;
        int32_t _M0L5digitS287 = (int32_t)_M0L6_2atmpS2110;
        int32_t _M0L6_2atmpS2108 = _M0L12digit__startS280 + _M0L6offsetS284;
        int32_t _M0L6_2atmpS2106 = _M0L6_2atmpS2108 - 1;
        int32_t _M0L6_2atmpS2107 =
          ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L5digitS287];
        int32_t _M0L6_2atmpS2109;
        _M0L6bufferS279[_M0L6_2atmpS2106] = _M0L6_2atmpS2107;
        _M0L6_2atmpS2109 = _M0L6offsetS284 - 1;
        _M0L6offsetS284 = _M0L6_2atmpS2109;
        _M0L1nS285 = _M0L1qS286;
        continue;
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS266,
  uint64_t _M0L3numS271,
  int32_t _M0L12digit__startS267,
  int32_t _M0L10total__lenS270
) {
  int32_t _M0L6_2atmpS2096;
  int32_t _M0L6offsetS261;
  uint64_t _M0L1nS262;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2096 = _M0L10total__lenS270 - _M0L12digit__startS267;
  _M0L6offsetS261 = _M0L6_2atmpS2096;
  _M0L1nS262 = _M0L3numS271;
  while (1) {
    if (_M0L6offsetS261 >= 2) {
      uint64_t _M0L6_2atmpS2093 = _M0L1nS262 & 255ull;
      int32_t _M0L9byte__valS263 = (int32_t)_M0L6_2atmpS2093;
      int32_t _M0L2hiS264 = _M0L9byte__valS263 / 16;
      int32_t _M0L2loS265 = _M0L9byte__valS263 % 16;
      int32_t _M0L6_2atmpS2087 = _M0L12digit__startS267 + _M0L6offsetS261;
      int32_t _M0L6_2atmpS2085 = _M0L6_2atmpS2087 - 2;
      int32_t _M0L6_2atmpS2086 =
        ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L2hiS264];
      int32_t _M0L6_2atmpS2090;
      int32_t _M0L6_2atmpS2088;
      int32_t _M0L6_2atmpS2089;
      int32_t _M0L6_2atmpS2091;
      uint64_t _M0L6_2atmpS2092;
      _M0L6bufferS266[_M0L6_2atmpS2085] = _M0L6_2atmpS2086;
      _M0L6_2atmpS2090 = _M0L12digit__startS267 + _M0L6offsetS261;
      _M0L6_2atmpS2088 = _M0L6_2atmpS2090 - 1;
      _M0L6_2atmpS2089
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2loS265
      ];
      _M0L6bufferS266[_M0L6_2atmpS2088] = _M0L6_2atmpS2089;
      _M0L6_2atmpS2091 = _M0L6offsetS261 - 2;
      _M0L6_2atmpS2092 = _M0L1nS262 >> 8;
      _M0L6offsetS261 = _M0L6_2atmpS2091;
      _M0L1nS262 = _M0L6_2atmpS2092;
      continue;
    } else if (_M0L6offsetS261 == 1) {
      uint64_t _M0L6_2atmpS2095 = _M0L1nS262 & 15ull;
      int32_t _M0L6nibbleS269 = (int32_t)_M0L6_2atmpS2095;
      int32_t _M0L6_2atmpS2094 =
        ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L6nibbleS269];
      _M0L6bufferS266[_M0L12digit__startS267] = _M0L6_2atmpS2094;
    }
    break;
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS255,
  int32_t _M0L5radixS257
) {
  uint64_t _M0L4baseS256;
  uint64_t _M0L3numS258;
  int32_t _M0L5countS259;
  #line 419 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS255 == 0ull) {
    return 1;
  }
  #line 424 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS256 = _M0MPC13int3Int10to__uint64(_M0L5radixS257);
  _M0L3numS258 = _M0L5valueS255;
  _M0L5countS259 = 0;
  while (1) {
    if (_M0L3numS258 > 0ull) {
      uint64_t _M0L6_2atmpS2083 = _M0L3numS258 / _M0L4baseS256;
      int32_t _M0L6_2atmpS2084 = _M0L5countS259 + 1;
      _M0L3numS258 = _M0L6_2atmpS2083;
      _M0L5countS259 = _M0L6_2atmpS2084;
      continue;
    } else {
      return _M0L5countS259;
    }
    break;
  }
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS253) {
  #line 407 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS253 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS254;
    int32_t _M0L6_2atmpS2082;
    int32_t _M0L6_2atmpS2081;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS254 = moonbit_clz64(_M0L5valueS253);
    _M0L6_2atmpS2082 = 63 - _M0L14leading__zerosS254;
    _M0L6_2atmpS2081 = _M0L6_2atmpS2082 / 4;
    return _M0L6_2atmpS2081 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS252) {
  #line 343 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS252 >= 10000000000ull) {
    if (_M0L5valueS252 >= 100000000000000ull) {
      if (_M0L5valueS252 >= 10000000000000000ull) {
        if (_M0L5valueS252 >= 1000000000000000000ull) {
          if (_M0L5valueS252 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS252 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS252 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS252 >= 1000000000000ull) {
      if (_M0L5valueS252 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS252 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS252 >= 100000ull) {
    if (_M0L5valueS252 >= 10000000ull) {
      if (_M0L5valueS252 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS252 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS252 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS252 >= 1000ull) {
    if (_M0L5valueS252 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS252 >= 100ull) {
    return 3;
  } else if (_M0L5valueS252 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS236,
  int32_t _M0L5radixS235
) {
  int32_t _if__result_4028;
  int32_t _M0L12is__negativeS237;
  uint32_t _M0L3numS238;
  uint16_t* _M0L6bufferS239;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS235 < 2) {
    _if__result_4028 = 1;
  } else {
    _if__result_4028 = _M0L5radixS235 > 36;
  }
  if (_if__result_4028) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_89.data);
  }
  if (_M0L4selfS236 == 0) {
    return (moonbit_string_t)moonbit_string_literal_83.data;
  }
  _M0L12is__negativeS237 = _M0L4selfS236 < 0;
  if (_M0L12is__negativeS237) {
    int32_t _M0L6_2atmpS2080 = -_M0L4selfS236;
    _M0L3numS238 = *(uint32_t*)&_M0L6_2atmpS2080;
  } else {
    _M0L3numS238 = *(uint32_t*)&_M0L4selfS236;
  }
  switch (_M0L5radixS235) {
    case 10: {
      int32_t _M0L10digit__lenS240;
      int32_t _M0L6_2atmpS2077;
      int32_t _M0L10total__lenS241;
      uint16_t* _M0L6bufferS242;
      int32_t _M0L12digit__startS243;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS240 = _M0FPB12dec__count32(_M0L3numS238);
      if (_M0L12is__negativeS237) {
        _M0L6_2atmpS2077 = 1;
      } else {
        _M0L6_2atmpS2077 = 0;
      }
      _M0L10total__lenS241 = _M0L10digit__lenS240 + _M0L6_2atmpS2077;
      _M0L6bufferS242
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS241, 0);
      if (_M0L12is__negativeS237) {
        _M0L12digit__startS243 = 1;
      } else {
        _M0L12digit__startS243 = 0;
      }
      #line 239 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS242, _M0L3numS238, _M0L12digit__startS243, _M0L10total__lenS241);
      _M0L6bufferS239 = _M0L6bufferS242;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS244;
      int32_t _M0L6_2atmpS2078;
      int32_t _M0L10total__lenS245;
      uint16_t* _M0L6bufferS246;
      int32_t _M0L12digit__startS247;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS244 = _M0FPB12hex__count32(_M0L3numS238);
      if (_M0L12is__negativeS237) {
        _M0L6_2atmpS2078 = 1;
      } else {
        _M0L6_2atmpS2078 = 0;
      }
      _M0L10total__lenS245 = _M0L10digit__lenS244 + _M0L6_2atmpS2078;
      _M0L6bufferS246
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS245, 0);
      if (_M0L12is__negativeS237) {
        _M0L12digit__startS247 = 1;
      } else {
        _M0L12digit__startS247 = 0;
      }
      #line 247 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS246, _M0L3numS238, _M0L12digit__startS247, _M0L10total__lenS245);
      _M0L6bufferS239 = _M0L6bufferS246;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS248;
      int32_t _M0L6_2atmpS2079;
      int32_t _M0L10total__lenS249;
      uint16_t* _M0L6bufferS250;
      int32_t _M0L12digit__startS251;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS248
      = _M0FPB14radix__count32(_M0L3numS238, _M0L5radixS235);
      if (_M0L12is__negativeS237) {
        _M0L6_2atmpS2079 = 1;
      } else {
        _M0L6_2atmpS2079 = 0;
      }
      _M0L10total__lenS249 = _M0L10digit__lenS248 + _M0L6_2atmpS2079;
      _M0L6bufferS250
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS249, 0);
      if (_M0L12is__negativeS237) {
        _M0L12digit__startS251 = 1;
      } else {
        _M0L12digit__startS251 = 0;
      }
      #line 255 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS250, _M0L3numS238, _M0L12digit__startS251, _M0L10total__lenS249, _M0L5radixS235);
      _M0L6bufferS239 = _M0L6bufferS250;
      break;
    }
  }
  if (_M0L12is__negativeS237) {
    _M0L6bufferS239[0] = 45;
  }
  return _M0L6bufferS239;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS229,
  int32_t _M0L5radixS231
) {
  uint32_t _M0L4baseS230;
  uint32_t _M0L3numS232;
  int32_t _M0L5countS233;
  #line 189 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS229 == 0u) {
    return 1;
  }
  _M0L4baseS230 = *(uint32_t*)&_M0L5radixS231;
  _M0L3numS232 = _M0L5valueS229;
  _M0L5countS233 = 0;
  while (1) {
    if (_M0L3numS232 > 0u) {
      uint32_t _M0L6_2atmpS2075 = _M0L3numS232 / _M0L4baseS230;
      int32_t _M0L6_2atmpS2076 = _M0L5countS233 + 1;
      _M0L3numS232 = _M0L6_2atmpS2075;
      _M0L5countS233 = _M0L6_2atmpS2076;
      continue;
    } else {
      return _M0L5countS233;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS227) {
  #line 177 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS227 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS228;
    int32_t _M0L6_2atmpS2074;
    int32_t _M0L6_2atmpS2073;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS228 = moonbit_clz32(_M0L5valueS227);
    _M0L6_2atmpS2074 = 31 - _M0L14leading__zerosS228;
    _M0L6_2atmpS2073 = _M0L6_2atmpS2074 / 4;
    return _M0L6_2atmpS2073 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS226) {
  #line 143 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS226 >= 100000u) {
    if (_M0L5valueS226 >= 10000000u) {
      if (_M0L5valueS226 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS226 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS226 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS226 >= 1000u) {
    if (_M0L5valueS226 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS226 >= 100u) {
    return 3;
  } else if (_M0L5valueS226 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS212,
  uint32_t _M0L3numS224,
  int32_t _M0L12digit__startS213,
  int32_t _M0L10total__lenS225
) {
  int32_t _M0L6_2atmpS2072;
  uint32_t _M0L3numS202;
  int32_t _M0L6offsetS203;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2072 = _M0L10total__lenS225 - _M0L12digit__startS213;
  _M0L3numS202 = _M0L3numS224;
  _M0L6offsetS203 = _M0L6_2atmpS2072;
  while (1) {
    if (_M0L3numS202 >= 10000u) {
      uint32_t _M0L1tS204 = _M0L3numS202 / 10000u;
      uint32_t _M0L6_2atmpS2049 = _M0L3numS202 % 10000u;
      int32_t _M0L1rS205 = *(int32_t*)&_M0L6_2atmpS2049;
      int32_t _M0L2d1S206 = _M0L1rS205 / 100;
      int32_t _M0L2d2S207 = _M0L1rS205 % 100;
      int32_t _M0L6_2atmpS2048 = _M0L2d1S206 / 10;
      int32_t _M0L6_2atmpS2047 = 48 + _M0L6_2atmpS2048;
      int32_t _M0L6d1__hiS208 = (uint16_t)_M0L6_2atmpS2047;
      int32_t _M0L6_2atmpS2046 = _M0L2d1S206 % 10;
      int32_t _M0L6_2atmpS2045 = 48 + _M0L6_2atmpS2046;
      int32_t _M0L6d1__loS209 = (uint16_t)_M0L6_2atmpS2045;
      int32_t _M0L6_2atmpS2044 = _M0L2d2S207 / 10;
      int32_t _M0L6_2atmpS2043 = 48 + _M0L6_2atmpS2044;
      int32_t _M0L6d2__hiS210 = (uint16_t)_M0L6_2atmpS2043;
      int32_t _M0L6_2atmpS2042 = _M0L2d2S207 % 10;
      int32_t _M0L6_2atmpS2041 = 48 + _M0L6_2atmpS2042;
      int32_t _M0L6d2__loS211 = (uint16_t)_M0L6_2atmpS2041;
      int32_t _M0L6_2atmpS2033 = _M0L12digit__startS213 + _M0L6offsetS203;
      int32_t _M0L6_2atmpS2032 = _M0L6_2atmpS2033 - 4;
      int32_t _M0L6_2atmpS2035;
      int32_t _M0L6_2atmpS2034;
      int32_t _M0L6_2atmpS2037;
      int32_t _M0L6_2atmpS2036;
      int32_t _M0L6_2atmpS2039;
      int32_t _M0L6_2atmpS2038;
      int32_t _M0L6_2atmpS2040;
      _M0L6bufferS212[_M0L6_2atmpS2032] = _M0L6d1__hiS208;
      _M0L6_2atmpS2035 = _M0L12digit__startS213 + _M0L6offsetS203;
      _M0L6_2atmpS2034 = _M0L6_2atmpS2035 - 3;
      _M0L6bufferS212[_M0L6_2atmpS2034] = _M0L6d1__loS209;
      _M0L6_2atmpS2037 = _M0L12digit__startS213 + _M0L6offsetS203;
      _M0L6_2atmpS2036 = _M0L6_2atmpS2037 - 2;
      _M0L6bufferS212[_M0L6_2atmpS2036] = _M0L6d2__hiS210;
      _M0L6_2atmpS2039 = _M0L12digit__startS213 + _M0L6offsetS203;
      _M0L6_2atmpS2038 = _M0L6_2atmpS2039 - 1;
      _M0L6bufferS212[_M0L6_2atmpS2038] = _M0L6d2__loS211;
      _M0L6_2atmpS2040 = _M0L6offsetS203 - 4;
      _M0L3numS202 = _M0L1tS204;
      _M0L6offsetS203 = _M0L6_2atmpS2040;
      continue;
    } else {
      int32_t _M0L6_2atmpS2071 = *(int32_t*)&_M0L3numS202;
      int32_t _M0L9remainingS215 = _M0L6_2atmpS2071;
      int32_t _M0L6offsetS216 = _M0L6offsetS203;
      while (1) {
        if (_M0L9remainingS215 >= 100) {
          int32_t _M0L1tS217 = _M0L9remainingS215 / 100;
          int32_t _M0L1dS218 = _M0L9remainingS215 % 100;
          int32_t _M0L6_2atmpS2058 = _M0L1dS218 / 10;
          int32_t _M0L6_2atmpS2057 = 48 + _M0L6_2atmpS2058;
          int32_t _M0L5d__hiS219 = (uint16_t)_M0L6_2atmpS2057;
          int32_t _M0L6_2atmpS2056 = _M0L1dS218 % 10;
          int32_t _M0L6_2atmpS2055 = 48 + _M0L6_2atmpS2056;
          int32_t _M0L5d__loS220 = (uint16_t)_M0L6_2atmpS2055;
          int32_t _M0L6_2atmpS2051 = _M0L12digit__startS213 + _M0L6offsetS216;
          int32_t _M0L6_2atmpS2050 = _M0L6_2atmpS2051 - 2;
          int32_t _M0L6_2atmpS2053;
          int32_t _M0L6_2atmpS2052;
          int32_t _M0L6_2atmpS2054;
          _M0L6bufferS212[_M0L6_2atmpS2050] = _M0L5d__hiS219;
          _M0L6_2atmpS2053 = _M0L12digit__startS213 + _M0L6offsetS216;
          _M0L6_2atmpS2052 = _M0L6_2atmpS2053 - 1;
          _M0L6bufferS212[_M0L6_2atmpS2052] = _M0L5d__loS220;
          _M0L6_2atmpS2054 = _M0L6offsetS216 - 2;
          _M0L9remainingS215 = _M0L1tS217;
          _M0L6offsetS216 = _M0L6_2atmpS2054;
          continue;
        } else if (_M0L9remainingS215 >= 10) {
          int32_t _M0L6_2atmpS2066 = _M0L9remainingS215 / 10;
          int32_t _M0L6_2atmpS2065 = 48 + _M0L6_2atmpS2066;
          int32_t _M0L5d__hiS222 = (uint16_t)_M0L6_2atmpS2065;
          int32_t _M0L6_2atmpS2064 = _M0L9remainingS215 % 10;
          int32_t _M0L6_2atmpS2063 = 48 + _M0L6_2atmpS2064;
          int32_t _M0L5d__loS223 = (uint16_t)_M0L6_2atmpS2063;
          int32_t _M0L6_2atmpS2060 = _M0L12digit__startS213 + _M0L6offsetS216;
          int32_t _M0L6_2atmpS2059 = _M0L6_2atmpS2060 - 2;
          int32_t _M0L6_2atmpS2062;
          int32_t _M0L6_2atmpS2061;
          _M0L6bufferS212[_M0L6_2atmpS2059] = _M0L5d__hiS222;
          _M0L6_2atmpS2062 = _M0L12digit__startS213 + _M0L6offsetS216;
          _M0L6_2atmpS2061 = _M0L6_2atmpS2062 - 1;
          _M0L6bufferS212[_M0L6_2atmpS2061] = _M0L5d__loS223;
        } else {
          int32_t _M0L6_2atmpS2070 = _M0L12digit__startS213 + _M0L6offsetS216;
          int32_t _M0L6_2atmpS2067 = _M0L6_2atmpS2070 - 1;
          int32_t _M0L6_2atmpS2069 = 48 + _M0L9remainingS215;
          int32_t _M0L6_2atmpS2068 = (uint16_t)_M0L6_2atmpS2069;
          _M0L6bufferS212[_M0L6_2atmpS2067] = _M0L6_2atmpS2068;
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS192,
  uint32_t _M0L3numS196,
  int32_t _M0L12digit__startS193,
  int32_t _M0L10total__lenS195,
  int32_t _M0L5radixS186
) {
  uint32_t _M0L4baseS185;
  int32_t _M0L6_2atmpS2017;
  int32_t _M0L6_2atmpS2016;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS185 = *(uint32_t*)&_M0L5radixS186;
  _M0L6_2atmpS2017 = _M0L5radixS186 - 1;
  _M0L6_2atmpS2016 = _M0L5radixS186 & _M0L6_2atmpS2017;
  if (_M0L6_2atmpS2016 == 0) {
    int32_t _M0L5shiftS187;
    uint32_t _M0L4maskS188;
    int32_t _M0L6_2atmpS2024;
    int32_t _M0L6offsetS189;
    uint32_t _M0L1nS190;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS187 = moonbit_ctz32(_M0L5radixS186);
    _M0L4maskS188 = _M0L4baseS185 - 1u;
    _M0L6_2atmpS2024 = _M0L10total__lenS195 - _M0L12digit__startS193;
    _M0L6offsetS189 = _M0L6_2atmpS2024;
    _M0L1nS190 = _M0L3numS196;
    while (1) {
      if (_M0L1nS190 > 0u) {
        uint32_t _M0L6_2atmpS2023 = _M0L1nS190 & _M0L4maskS188;
        int32_t _M0L5digitS191 = *(int32_t*)&_M0L6_2atmpS2023;
        int32_t _M0L6_2atmpS2020 = _M0L12digit__startS193 + _M0L6offsetS189;
        int32_t _M0L6_2atmpS2018 = _M0L6_2atmpS2020 - 1;
        int32_t _M0L6_2atmpS2019 =
          ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L5digitS191];
        int32_t _M0L6_2atmpS2021;
        uint32_t _M0L6_2atmpS2022;
        _M0L6bufferS192[_M0L6_2atmpS2018] = _M0L6_2atmpS2019;
        _M0L6_2atmpS2021 = _M0L6offsetS189 - 1;
        _M0L6_2atmpS2022 = _M0L1nS190 >> (_M0L5shiftS187 & 31);
        _M0L6offsetS189 = _M0L6_2atmpS2021;
        _M0L1nS190 = _M0L6_2atmpS2022;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2031 = _M0L10total__lenS195 - _M0L12digit__startS193;
    int32_t _M0L6offsetS197 = _M0L6_2atmpS2031;
    uint32_t _M0L1nS198 = _M0L3numS196;
    while (1) {
      if (_M0L1nS198 > 0u) {
        uint32_t _M0L1qS199 = _M0L1nS198 / _M0L4baseS185;
        uint32_t _M0L6_2atmpS2030 = _M0L1qS199 * _M0L4baseS185;
        uint32_t _M0L6_2atmpS2029 = _M0L1nS198 - _M0L6_2atmpS2030;
        int32_t _M0L5digitS200 = *(int32_t*)&_M0L6_2atmpS2029;
        int32_t _M0L6_2atmpS2027 = _M0L12digit__startS193 + _M0L6offsetS197;
        int32_t _M0L6_2atmpS2025 = _M0L6_2atmpS2027 - 1;
        int32_t _M0L6_2atmpS2026 =
          ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L5digitS200];
        int32_t _M0L6_2atmpS2028;
        _M0L6bufferS192[_M0L6_2atmpS2025] = _M0L6_2atmpS2026;
        _M0L6_2atmpS2028 = _M0L6offsetS197 - 1;
        _M0L6offsetS197 = _M0L6_2atmpS2028;
        _M0L1nS198 = _M0L1qS199;
        continue;
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS179,
  uint32_t _M0L3numS184,
  int32_t _M0L12digit__startS180,
  int32_t _M0L10total__lenS183
) {
  int32_t _M0L6_2atmpS2015;
  int32_t _M0L6offsetS174;
  uint32_t _M0L1nS175;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2015 = _M0L10total__lenS183 - _M0L12digit__startS180;
  _M0L6offsetS174 = _M0L6_2atmpS2015;
  _M0L1nS175 = _M0L3numS184;
  while (1) {
    if (_M0L6offsetS174 >= 2) {
      uint32_t _M0L6_2atmpS2012 = _M0L1nS175 & 255u;
      int32_t _M0L9byte__valS176 = *(int32_t*)&_M0L6_2atmpS2012;
      int32_t _M0L2hiS177 = _M0L9byte__valS176 / 16;
      int32_t _M0L2loS178 = _M0L9byte__valS176 % 16;
      int32_t _M0L6_2atmpS2006 = _M0L12digit__startS180 + _M0L6offsetS174;
      int32_t _M0L6_2atmpS2004 = _M0L6_2atmpS2006 - 2;
      int32_t _M0L6_2atmpS2005 =
        ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L2hiS177];
      int32_t _M0L6_2atmpS2009;
      int32_t _M0L6_2atmpS2007;
      int32_t _M0L6_2atmpS2008;
      int32_t _M0L6_2atmpS2010;
      uint32_t _M0L6_2atmpS2011;
      _M0L6bufferS179[_M0L6_2atmpS2004] = _M0L6_2atmpS2005;
      _M0L6_2atmpS2009 = _M0L12digit__startS180 + _M0L6offsetS174;
      _M0L6_2atmpS2007 = _M0L6_2atmpS2009 - 1;
      _M0L6_2atmpS2008
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2loS178
      ];
      _M0L6bufferS179[_M0L6_2atmpS2007] = _M0L6_2atmpS2008;
      _M0L6_2atmpS2010 = _M0L6offsetS174 - 2;
      _M0L6_2atmpS2011 = _M0L1nS175 >> 8;
      _M0L6offsetS174 = _M0L6_2atmpS2010;
      _M0L1nS175 = _M0L6_2atmpS2011;
      continue;
    } else if (_M0L6offsetS174 == 1) {
      uint32_t _M0L6_2atmpS2014 = _M0L1nS175 & 15u;
      int32_t _M0L6nibbleS182 = *(int32_t*)&_M0L6_2atmpS2014;
      int32_t _M0L6_2atmpS2013 =
        ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L6nibbleS182];
      _M0L6bufferS179[_M0L12digit__startS180] = _M0L6_2atmpS2013;
    }
    break;
  }
  return 0;
}

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS157
) {
  struct _M0TWEOUsbE* _M0L7_2afuncS156;
  struct _M0TUsbE* _M0L6resultS158;
  int64_t _M0L7_2abindS159;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS156 = _M0L4selfS157->$0;
  moonbit_incref(_M0L7_2afuncS156);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS158 = _M0L7_2afuncS156->code(_M0L7_2afuncS156);
  moonbit_decref(_M0L7_2afuncS156);
  _M0L7_2abindS159 = _M0L4selfS157->$1;
  if (_M0L6resultS158 == 0) {
    _M0L4selfS157->$1 = _M0MPB4Iter4nextN6constrS9981GUsbEE;
  } else if (_M0L7_2abindS159 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS160 = _M0L7_2abindS159;
    int32_t _M0L4_2anS161 = (int32_t)_M0L7_2aSomeS160;
    int64_t _M0L6_2atmpS1998;
    if (_M0L4_2anS161 > 0) {
      int32_t _M0L6_2atmpS1999 = _M0L4_2anS161 - 1;
      _M0L6_2atmpS1998 = (int64_t)_M0L6_2atmpS1999;
    } else {
      _M0L6_2atmpS1998 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS157->$1 = _M0L6_2atmpS1998;
  }
  return _M0L6resultS158;
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L4selfS163
) {
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2afuncS162;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6resultS164;
  int64_t _M0L7_2abindS165;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS162 = _M0L4selfS163->$0;
  moonbit_incref(_M0L7_2afuncS162);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS164 = _M0L7_2afuncS162->code(_M0L7_2afuncS162);
  moonbit_decref(_M0L7_2afuncS162);
  _M0L7_2abindS165 = _M0L4selfS163->$1;
  if (_M0L6resultS164 == 0) {
    _M0L4selfS163->$1
    = _M0MPB4Iter4nextN6constrS9981GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
  } else if (_M0L7_2abindS165 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS166 = _M0L7_2abindS165;
    int32_t _M0L4_2anS167 = (int32_t)_M0L7_2aSomeS166;
    int64_t _M0L6_2atmpS2000;
    if (_M0L4_2anS167 > 0) {
      int32_t _M0L6_2atmpS2001 = _M0L4_2anS167 - 1;
      _M0L6_2atmpS2000 = (int64_t)_M0L6_2atmpS2001;
    } else {
      _M0L6_2atmpS2000
      = _M0MPB4Iter4nextN6constrS9980GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
    }
    _M0L4selfS163->$1 = _M0L6_2atmpS2000;
  }
  return _M0L6resultS164;
}

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS169
) {
  struct _M0TWEOUsfE* _M0L7_2afuncS168;
  struct _M0TUsfE* _M0L6resultS170;
  int64_t _M0L7_2abindS171;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS168 = _M0L4selfS169->$0;
  moonbit_incref(_M0L7_2afuncS168);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS170 = _M0L7_2afuncS168->code(_M0L7_2afuncS168);
  moonbit_decref(_M0L7_2afuncS168);
  _M0L7_2abindS171 = _M0L4selfS169->$1;
  if (_M0L6resultS170 == 0) {
    _M0L4selfS169->$1 = _M0MPB4Iter4nextN6constrS9981GUsfEE;
  } else if (_M0L7_2abindS171 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS172 = _M0L7_2abindS171;
    int32_t _M0L4_2anS173 = (int32_t)_M0L7_2aSomeS172;
    int64_t _M0L6_2atmpS2002;
    if (_M0L4_2anS173 > 0) {
      int32_t _M0L6_2atmpS2003 = _M0L4_2anS173 - 1;
      _M0L6_2atmpS2002 = (int64_t)_M0L6_2atmpS2003;
    } else {
      _M0L6_2atmpS2002 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS169->$1 = _M0L6_2atmpS2002;
  }
  return _M0L6resultS170;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS149,
  struct _M0TPB6Logger _M0L6loggerS148
) {
  moonbit_string_t _M0L6_2atmpS1994;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1994 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS149);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS148.$0->$method_0(_M0L6loggerS148.$1, _M0L6_2atmpS1994);
  moonbit_decref(_M0L6_2atmpS1994);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS151,
  struct _M0TPB6Logger _M0L6loggerS150
) {
  moonbit_string_t _M0L6_2atmpS1995;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1995 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS151);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS150.$0->$method_0(_M0L6loggerS150.$1, _M0L6_2atmpS1995);
  moonbit_decref(_M0L6_2atmpS1995);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS153,
  struct _M0TPB6Logger _M0L6loggerS152
) {
  moonbit_string_t _M0L6_2atmpS1996;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1996 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS153);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS152.$0->$method_0(_M0L6loggerS152.$1, _M0L6_2atmpS1996);
  moonbit_decref(_M0L6_2atmpS1996);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS155,
  struct _M0TPB6Logger _M0L6loggerS154
) {
  moonbit_string_t _M0L6_2atmpS1997;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1997 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS155);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS154.$0->$method_0(_M0L6loggerS154.$1, _M0L6_2atmpS1997);
  moonbit_decref(_M0L6_2atmpS1997);
  return 0;
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS147
) {
  #line 99 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  return _M0L4selfS147.$1;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS146
) {
  moonbit_string_t _M0L8_2afieldS3715;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS3715 = _M0L4selfS146.$0;
  moonbit_incref(_M0L8_2afieldS3715);
  return _M0L8_2afieldS3715;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS142,
  moonbit_string_t _M0L5valueS143,
  int32_t _M0L5startS144,
  int32_t _M0L3lenS145
) {
  int32_t _M0L6_2atmpS1993;
  int64_t _M0L6_2atmpS1992;
  struct _M0TPC16string10StringView _M0L6_2atmpS1991;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1993 = _M0L5startS144 + _M0L3lenS145;
  _M0L6_2atmpS1992 = (int64_t)_M0L6_2atmpS1993;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1991
  = _M0MPC16string6String11sub_2einner(_M0L5valueS143, _M0L5startS144, _M0L6_2atmpS1992);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS142, _M0L6_2atmpS1991);
  moonbit_decref(_M0L6_2atmpS1991.$0);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS135,
  int32_t _M0L5startS141,
  int64_t _M0L3endS137
) {
  int32_t _M0L3lenS134;
  int32_t _M0L3endS136;
  int32_t _M0L5startS140;
  int32_t _if__result_4035;
  #line 755 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS134 = Moonbit_array_length(_M0L4selfS135);
  if (_M0L3endS137 == 4294967296ll) {
    _M0L3endS136 = _M0L3lenS134;
  } else {
    int64_t _M0L7_2aSomeS138 = _M0L3endS137;
    int32_t _M0L6_2aendS139 = (int32_t)_M0L7_2aSomeS138;
    if (_M0L6_2aendS139 < 0) {
      _M0L3endS136 = _M0L3lenS134 + _M0L6_2aendS139;
    } else {
      _M0L3endS136 = _M0L6_2aendS139;
    }
  }
  if (_M0L5startS141 < 0) {
    _M0L5startS140 = _M0L3lenS134 + _M0L5startS141;
  } else {
    _M0L5startS140 = _M0L5startS141;
  }
  if (_M0L5startS140 >= 0) {
    if (_M0L5startS140 <= _M0L3endS136) {
      _if__result_4035 = _M0L3endS136 <= _M0L3lenS134;
    } else {
      _if__result_4035 = 0;
    }
  } else {
    _if__result_4035 = 0;
  }
  if (_if__result_4035) {
    if (_M0L5startS140 < _M0L3lenS134) {
      int32_t _M0L6_2atmpS1988 = _M0L4selfS135[_M0L5startS140];
      int32_t _M0L6_2atmpS1987;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1987
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1988);
      if (!_M0L6_2atmpS1987) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS136 < _M0L3lenS134) {
      int32_t _M0L6_2atmpS1990 = _M0L4selfS135[_M0L3endS136];
      int32_t _M0L6_2atmpS1989;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1989
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1990);
      if (!_M0L6_2atmpS1989) {
        
      } else {
        #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    moonbit_incref(_M0L4selfS135);
    return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS135,
                                                 .$1 = _M0L5startS140,
                                                 .$2 = _M0L3endS136};
  } else {
    #line 763 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS133,
  struct _M0TPB4Show _M0L4showS132
) {
  struct _M0TPB6Logger _M0L6_2atmpS1986;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS133);
  _M0L6_2atmpS1986
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS133
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS132.$0->$method_0(_M0L4showS132.$1, _M0L6_2atmpS1986);
  if (_M0L6_2atmpS1986.$1) {
    moonbit_decref(_M0L6_2atmpS1986.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS131,
  struct _M0TPB4Show _M0L4showS130
) {
  struct _M0TPB6Logger _M0L6_2atmpS1985;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS131);
  _M0L6_2atmpS1985
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS131
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS130.$0->$method_0(_M0L4showS130.$1, _M0L6_2atmpS1985);
  if (_M0L6_2atmpS1985.$1) {
    moonbit_decref(_M0L6_2atmpS1985.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS129) {
  uint32_t _M0L6_2atmpS1984;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1984 = _M0FPB14avalanche__acc(_M0L3accS129);
  return *(int32_t*)&_M0L6_2atmpS1984;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS128) {
  uint32_t _M0Lm3accS127;
  uint32_t _M0L6_2atmpS1973;
  uint32_t _M0L6_2atmpS1975;
  uint32_t _M0L6_2atmpS1974;
  uint32_t _M0L6_2atmpS1976;
  uint32_t _M0L6_2atmpS1977;
  uint32_t _M0L6_2atmpS1979;
  uint32_t _M0L6_2atmpS1978;
  uint32_t _M0L6_2atmpS1980;
  uint32_t _M0L6_2atmpS1981;
  uint32_t _M0L6_2atmpS1983;
  uint32_t _M0L6_2atmpS1982;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS127 = _M0L3accS128;
  _M0L6_2atmpS1973 = _M0Lm3accS127;
  _M0L6_2atmpS1975 = _M0Lm3accS127;
  _M0L6_2atmpS1974 = _M0L6_2atmpS1975 >> 15;
  _M0Lm3accS127 = _M0L6_2atmpS1973 ^ _M0L6_2atmpS1974;
  _M0L6_2atmpS1976 = _M0Lm3accS127;
  _M0Lm3accS127 = _M0L6_2atmpS1976 * 2246822519u;
  _M0L6_2atmpS1977 = _M0Lm3accS127;
  _M0L6_2atmpS1979 = _M0Lm3accS127;
  _M0L6_2atmpS1978 = _M0L6_2atmpS1979 >> 13;
  _M0Lm3accS127 = _M0L6_2atmpS1977 ^ _M0L6_2atmpS1978;
  _M0L6_2atmpS1980 = _M0Lm3accS127;
  _M0Lm3accS127 = _M0L6_2atmpS1980 * 3266489917u;
  _M0L6_2atmpS1981 = _M0Lm3accS127;
  _M0L6_2atmpS1983 = _M0Lm3accS127;
  _M0L6_2atmpS1982 = _M0L6_2atmpS1983 >> 16;
  _M0Lm3accS127 = _M0L6_2atmpS1981 ^ _M0L6_2atmpS1982;
  return _M0Lm3accS127;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS126) {
  int64_t _M0L6_2atmpS1972;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1972 = (int64_t)_M0L4selfS126;
  return *(uint64_t*)&_M0L6_2atmpS1972;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS125,
  moonbit_string_t _M0L3strS124
) {
  int32_t _M0L8str__lenS123;
  int32_t _M0L3lenS1967;
  int32_t _M0L6_2atmpS1966;
  uint16_t* _M0L4dataS1968;
  int32_t _M0L3lenS1969;
  int32_t _M0L3lenS1971;
  int32_t _M0L6_2atmpS1970;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS123 = Moonbit_array_length(_M0L3strS124);
  _M0L3lenS1967 = _M0L4selfS125->$1;
  _M0L6_2atmpS1966 = _M0L3lenS1967 + _M0L8str__lenS123;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS125, _M0L6_2atmpS1966);
  _M0L4dataS1968 = _M0L4selfS125->$0;
  _M0L3lenS1969 = _M0L4selfS125->$1;
  moonbit_incref(_M0L4dataS1968);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1968, _M0L3lenS1969, _M0L3strS124, 0, _M0L8str__lenS123);
  moonbit_decref(_M0L4dataS1968);
  _M0L3lenS1971 = _M0L4selfS125->$1;
  _M0L6_2atmpS1970 = _M0L3lenS1971 + _M0L8str__lenS123;
  _M0L4selfS125->$1 = _M0L6_2atmpS1970;
  return 0;
}

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t* _M0L4selfS119,
  int32_t _M0L11dst__offsetS122,
  moonbit_string_t _M0L3strS120,
  int32_t _M0L11str__offsetS115,
  int32_t _M0L3lenS116
) {
  int32_t _M0L16end__str__offsetS114;
  int32_t _M0L1iS117;
  int32_t _M0L1jS118;
  #line 71 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L16end__str__offsetS114 = _M0L11str__offsetS115 + _M0L3lenS116;
  _M0L1iS117 = _M0L11str__offsetS115;
  _M0L1jS118 = _M0L11dst__offsetS122;
  while (1) {
    if (_M0L1iS117 < _M0L16end__str__offsetS114) {
      int32_t _M0L6_2atmpS1963 = _M0L3strS120[_M0L1iS117];
      int32_t _M0L6_2atmpS1964;
      int32_t _M0L6_2atmpS1965;
      if (
        _M0L1jS118 < 0 || _M0L1jS118 >= Moonbit_array_length(_M0L4selfS119)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS119[_M0L1jS118] = _M0L6_2atmpS1963;
      _M0L6_2atmpS1964 = _M0L1iS117 + 1;
      _M0L6_2atmpS1965 = _M0L1jS118 + 1;
      _M0L1iS117 = _M0L6_2atmpS1964;
      _M0L1jS118 = _M0L6_2atmpS1965;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS113) {
  #line 45 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS113 >= 56320) {
    return _M0L4selfS113 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS111,
  int32_t _M0L2chS110
) {
  uint32_t _M0L4codeS109;
  #line 95 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  #line 96 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4codeS109 = _M0MPC14char4Char8to__uint(_M0L2chS110);
  if (_M0L4codeS109 <= 65535u) {
    int32_t _M0L3lenS1942 = _M0L4selfS111->$1;
    int32_t _M0L6_2atmpS1941 = _M0L3lenS1942 + 1;
    uint16_t* _M0L4dataS1943;
    int32_t _M0L3lenS1944;
    int32_t _M0L6_2atmpS1945;
    int32_t _M0L3lenS1947;
    int32_t _M0L6_2atmpS1946;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS111, _M0L6_2atmpS1941);
    _M0L4dataS1943 = _M0L4selfS111->$0;
    _M0L3lenS1944 = _M0L4selfS111->$1;
    moonbit_incref(_M0L4dataS1943);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1945 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS109);
    if (
      _M0L3lenS1944 < 0
      || _M0L3lenS1944 >= Moonbit_array_length(_M0L4dataS1943)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1943[_M0L3lenS1944] = _M0L6_2atmpS1945;
    moonbit_decref(_M0L4dataS1943);
    _M0L3lenS1947 = _M0L4selfS111->$1;
    _M0L6_2atmpS1946 = _M0L3lenS1947 + 1;
    _M0L4selfS111->$1 = _M0L6_2atmpS1946;
  } else if (_M0L4codeS109 <= 1114111u) {
    int32_t _M0L3lenS1949 = _M0L4selfS111->$1;
    int32_t _M0L6_2atmpS1948 = _M0L3lenS1949 + 2;
    uint32_t _M0L4codeS112;
    uint16_t* _M0L4dataS1950;
    int32_t _M0L3lenS1951;
    uint32_t _M0L6_2atmpS1954;
    uint32_t _M0L6_2atmpS1953;
    int32_t _M0L6_2atmpS1952;
    uint16_t* _M0L4dataS1955;
    int32_t _M0L3lenS1960;
    int32_t _M0L6_2atmpS1956;
    uint32_t _M0L6_2atmpS1959;
    uint32_t _M0L6_2atmpS1958;
    int32_t _M0L6_2atmpS1957;
    int32_t _M0L3lenS1962;
    int32_t _M0L6_2atmpS1961;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS111, _M0L6_2atmpS1948);
    _M0L4codeS112 = _M0L4codeS109 - 65536u;
    _M0L4dataS1950 = _M0L4selfS111->$0;
    _M0L3lenS1951 = _M0L4selfS111->$1;
    _M0L6_2atmpS1954 = _M0L4codeS112 >> 10;
    _M0L6_2atmpS1953 = 55296u + _M0L6_2atmpS1954;
    moonbit_incref(_M0L4dataS1950);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1952 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1953);
    if (
      _M0L3lenS1951 < 0
      || _M0L3lenS1951 >= Moonbit_array_length(_M0L4dataS1950)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1950[_M0L3lenS1951] = _M0L6_2atmpS1952;
    moonbit_decref(_M0L4dataS1950);
    _M0L4dataS1955 = _M0L4selfS111->$0;
    _M0L3lenS1960 = _M0L4selfS111->$1;
    _M0L6_2atmpS1956 = _M0L3lenS1960 + 1;
    _M0L6_2atmpS1959 = _M0L4codeS112 & 1023u;
    _M0L6_2atmpS1958 = 56320u + _M0L6_2atmpS1959;
    moonbit_incref(_M0L4dataS1955);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1957 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1958);
    if (
      _M0L6_2atmpS1956 < 0
      || _M0L6_2atmpS1956 >= Moonbit_array_length(_M0L4dataS1955)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1955[_M0L6_2atmpS1956] = _M0L6_2atmpS1957;
    moonbit_decref(_M0L4dataS1955);
    _M0L3lenS1962 = _M0L4selfS111->$1;
    _M0L6_2atmpS1961 = _M0L3lenS1962 + 2;
    _M0L4selfS111->$1 = _M0L6_2atmpS1961;
  } else {
    #line 108 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_91.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS103,
  int32_t _M0L8requiredS104
) {
  uint16_t* _M0L4dataS1940;
  int32_t _M0L12current__lenS102;
  int32_t _M0L13enough__spaceS105;
  int32_t _M0L13enough__spaceS106;
  uint16_t* _M0L4dataS1936;
  int32_t _M0L6_2atmpS1937;
  int32_t _M0L3lenS1938;
  uint16_t* _M0L9new__dataS108;
  uint16_t* _M0L6_2aoldS3720;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS1940 = _M0L4selfS103->$0;
  _M0L12current__lenS102 = Moonbit_array_length(_M0L4dataS1940);
  if (_M0L8requiredS104 <= _M0L12current__lenS102) {
    return 0;
  }
  _M0L13enough__spaceS106 = _M0L12current__lenS102;
  while (1) {
    if (_M0L13enough__spaceS106 < _M0L8requiredS104) {
      int32_t _M0L6_2atmpS1939 = _M0L13enough__spaceS106 * 2;
      _M0L13enough__spaceS106 = _M0L6_2atmpS1939;
      continue;
    } else {
      _M0L13enough__spaceS105 = _M0L13enough__spaceS106;
    }
    break;
  }
  _M0L4dataS1936 = _M0L4selfS103->$0;
  moonbit_incref(_M0L4dataS1936);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1937 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS1938 = _M0L4selfS103->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS108
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS1936, _M0L13enough__spaceS105, _M0L6_2atmpS1937, _M0L3lenS1938, 0, 0);
  moonbit_decref(_M0L4dataS1936);
  _M0L6_2aoldS3720 = _M0L4selfS103->$0;
  moonbit_decref(_M0L6_2aoldS3720);
  _M0L4selfS103->$0 = _M0L9new__dataS108;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS101) {
  int32_t _M0L6_2atmpS1935;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1935 = *(int32_t*)&_M0L4selfS101;
  return (uint16_t)_M0L6_2atmpS1935;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS100) {
  int32_t _M0L6_2atmpS1934;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1934 = _M0L4selfS100;
  return *(uint32_t*)&_M0L6_2atmpS1934;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS98
) {
  int32_t _M0L3lenS1926;
  uint16_t* _M0L4dataS1928;
  int32_t _M0L6_2atmpS1927;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1926 = _M0L4selfS98->$1;
  _M0L4dataS1928 = _M0L4selfS98->$0;
  _M0L6_2atmpS1927 = Moonbit_array_length(_M0L4dataS1928);
  if (_M0L3lenS1926 == _M0L6_2atmpS1927) {
    uint16_t* _M0L4dataS1929 = _M0L4selfS98->$0;
    moonbit_incref(_M0L4dataS1929);
    return _M0L4dataS1929;
  } else {
    uint16_t* _M0L4dataS1930 = _M0L4selfS98->$0;
    int32_t _M0L3lenS1931 = _M0L4selfS98->$1;
    int32_t _M0L6_2atmpS1932;
    int32_t _M0L3lenS1933;
    uint16_t* _M0L4dataS99;
    moonbit_incref(_M0L4dataS1930);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1932 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS1933 = _M0L4selfS98->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS99
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS1930, _M0L3lenS1931, _M0L6_2atmpS1932, _M0L3lenS1933, 0, 0);
    moonbit_decref(_M0L4dataS1930);
    return _M0L4dataS99;
  }
}

int32_t _M0IPC16uint166UInt16PB7Default7default() {
  #line 176 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  return 0;
}

uint16_t* _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(
  uint16_t* _M0L3srcS95,
  int32_t _M0L13allocate__lenS91,
  int32_t _M0L4initS96,
  int32_t _M0L3lenS92,
  int32_t _M0L11src__offsetS93,
  int32_t _M0L11dst__offsetS94
) {
  int32_t _if__result_4038;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS91 >= 0) {
    if (_M0L3lenS92 >= 0) {
      if (_M0L11src__offsetS93 >= 0) {
        if (_M0L11dst__offsetS94 >= 0) {
          int32_t _M0L6_2atmpS1922 = _M0L11src__offsetS93 + _M0L3lenS92;
          int32_t _M0L6_2atmpS1923 = Moonbit_array_length(_M0L3srcS95);
          if (_M0L6_2atmpS1922 <= _M0L6_2atmpS1923) {
            int32_t _M0L6_2atmpS1921 = _M0L11dst__offsetS94 + _M0L3lenS92;
            _if__result_4038 = _M0L6_2atmpS1921 <= _M0L13allocate__lenS91;
          } else {
            _if__result_4038 = 0;
          }
        } else {
          _if__result_4038 = 0;
        }
      } else {
        _if__result_4038 = 0;
      }
    } else {
      _if__result_4038 = 0;
    }
  } else {
    _if__result_4038 = 0;
  }
  if (_if__result_4038) {
    moonbit_incref(_M0L3srcS95);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS95, _M0L13allocate__lenS91, _M0L4initS96, _M0L11src__offsetS93, _M0L11dst__offsetS94, _M0L3lenS92);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS97;
    int32_t _M0L6_2atmpS1925;
    moonbit_string_t _M0L6_2atmpS1924;
    uint16_t* _result_4039;
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L18_2astring__builderS97
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS97, (moonbit_string_t)moonbit_string_literal_92.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS97, _M0L13allocate__lenS91);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS97, (moonbit_string_t)moonbit_string_literal_93.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS97, _M0L11src__offsetS93);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS97, (moonbit_string_t)moonbit_string_literal_94.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS97, _M0L11dst__offsetS94);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS97, (moonbit_string_t)moonbit_string_literal_95.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS97, _M0L3lenS92);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS97, (moonbit_string_t)moonbit_string_literal_96.data);
    _M0L6_2atmpS1925 = Moonbit_array_length(_M0L3srcS95);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS97, _M0L6_2atmpS1925);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS1924
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS97);
    moonbit_decref(_M0L18_2astring__builderS97);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_4039 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS1924);
    moonbit_decref(_M0L6_2atmpS1924);
    return _result_4039;
  }
}

uint16_t* _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(
  uint16_t* _M0L3srcS88,
  int32_t _M0L13allocate__lenS85,
  int32_t _M0L4initS86,
  int32_t _M0L11src__offsetS89,
  int32_t _M0L11dst__offsetS87,
  int32_t _M0L9blit__lenS90
) {
  uint16_t* _M0L3dstS84;
  #line 79 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  _M0L3dstS84
  = (uint16_t*)moonbit_make_string(_M0L13allocate__lenS85, _M0L4initS86);
  moonbit_incref(_M0L3dstS84);
  #line 90 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  moonbit_unsafe_val_array_blit(_M0L3dstS84, _M0L11dst__offsetS87, _M0L3srcS88, _M0L11src__offsetS89, _M0L9blit__lenS90, sizeof(uint16_t));
  return _M0L3dstS84;
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder21StringBuilder_2einner(
  int32_t _M0L10size__hintS82
) {
  int32_t _M0L7initialS81;
  uint16_t* _M0L4dataS83;
  struct _M0TPB13StringBuilder* _block_4040;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS82 < 1) {
    _M0L7initialS81 = 1;
  } else {
    int32_t _M0L6_2atmpS1920 = _M0L10size__hintS82 + 1;
    _M0L7initialS81 = _M0L6_2atmpS1920 / 2;
  }
  _M0L4dataS83 = (uint16_t*)moonbit_make_string(_M0L7initialS81, 0);
  _block_4040
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4040)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 112, 0);
  _block_4040->$0 = _M0L4dataS83;
  _block_4040->$1 = 0;
  return _block_4040;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS73,
  int32_t _M0L13allocate__lenS69,
  int32_t _M0L3lenS70,
  int32_t _M0L11src__offsetS71,
  int32_t _M0L11dst__offsetS72
) {
  int32_t _if__result_4041;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS69 >= 0) {
    if (_M0L3lenS70 >= 0) {
      if (_M0L11src__offsetS71 >= 0) {
        if (_M0L11dst__offsetS72 >= 0) {
          int32_t _M0L6_2atmpS1911 = _M0L11src__offsetS71 + _M0L3lenS70;
          int32_t _M0L6_2atmpS1912;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS1912
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS73);
          if (_M0L6_2atmpS1911 <= _M0L6_2atmpS1912) {
            int32_t _M0L6_2atmpS1910 = _M0L11dst__offsetS72 + _M0L3lenS70;
            _if__result_4041 = _M0L6_2atmpS1910 <= _M0L13allocate__lenS69;
          } else {
            _if__result_4041 = 0;
          }
        } else {
          _if__result_4041 = 0;
        }
      } else {
        _if__result_4041 = 0;
      }
    } else {
      _if__result_4041 = 0;
    }
  } else {
    _if__result_4041 = 0;
  }
  if (_if__result_4041) {
    moonbit_incref(_M0L3srcS73);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS69, (moonbit_string_t)moonbit_string_literal_82.data, _M0L3srcS73, _M0L11src__offsetS71, _M0L11dst__offsetS72, _M0L3lenS70);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS74;
    int32_t _M0L6_2atmpS1914;
    moonbit_string_t _M0L6_2atmpS1913;
    moonbit_string_t* _result_4042;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS74
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS74, (moonbit_string_t)moonbit_string_literal_92.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS74, _M0L13allocate__lenS69);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS74, (moonbit_string_t)moonbit_string_literal_93.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS74, _M0L11src__offsetS71);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS74, (moonbit_string_t)moonbit_string_literal_94.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS74, _M0L11dst__offsetS72);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS74, (moonbit_string_t)moonbit_string_literal_95.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS74, _M0L3lenS70);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS74, (moonbit_string_t)moonbit_string_literal_96.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1914 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS73);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS74, _M0L6_2atmpS1914);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1913
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS74);
    moonbit_decref(_M0L18_2astring__builderS74);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4042
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS1913);
    moonbit_decref(_M0L6_2atmpS1913);
    return _result_4042;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS79,
  int32_t _M0L13allocate__lenS75,
  int32_t _M0L3lenS76,
  int32_t _M0L11src__offsetS77,
  int32_t _M0L11dst__offsetS78
) {
  int32_t _if__result_4043;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS75 >= 0) {
    if (_M0L3lenS76 >= 0) {
      if (_M0L11src__offsetS77 >= 0) {
        if (_M0L11dst__offsetS78 >= 0) {
          int32_t _M0L6_2atmpS1916 = _M0L11src__offsetS77 + _M0L3lenS76;
          int32_t _M0L6_2atmpS1917;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS1917
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS79);
          if (_M0L6_2atmpS1916 <= _M0L6_2atmpS1917) {
            int32_t _M0L6_2atmpS1915 = _M0L11dst__offsetS78 + _M0L3lenS76;
            _if__result_4043 = _M0L6_2atmpS1915 <= _M0L13allocate__lenS75;
          } else {
            _if__result_4043 = 0;
          }
        } else {
          _if__result_4043 = 0;
        }
      } else {
        _if__result_4043 = 0;
      }
    } else {
      _if__result_4043 = 0;
    }
  } else {
    _if__result_4043 = 0;
  }
  if (_if__result_4043) {
    moonbit_incref(_M0L3srcS79);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS75, 0, _M0L3srcS79, _M0L11src__offsetS77, _M0L11dst__offsetS78, _M0L3lenS76);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS80;
    int32_t _M0L6_2atmpS1919;
    moonbit_string_t _M0L6_2atmpS1918;
    struct _M0TUsfE** _result_4044;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS80
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS80, (moonbit_string_t)moonbit_string_literal_92.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS80, _M0L13allocate__lenS75);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS80, (moonbit_string_t)moonbit_string_literal_93.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS80, _M0L11src__offsetS77);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS80, (moonbit_string_t)moonbit_string_literal_94.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS80, _M0L11dst__offsetS78);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS80, (moonbit_string_t)moonbit_string_literal_95.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS80, _M0L3lenS76);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS80, (moonbit_string_t)moonbit_string_literal_96.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1919 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS79);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS80, _M0L6_2atmpS1919);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1918
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS80);
    moonbit_decref(_M0L18_2astring__builderS80);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4044
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS1918);
    moonbit_decref(_M0L6_2atmpS1918);
    return _result_4044;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS62,
  moonbit_string_t _M0L3objS61
) {
  struct _M0TPB6Logger _M0L6_2atmpS1906;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS62);
  _M0L6_2atmpS1906
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS62
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS61, _M0L6_2atmpS1906);
  if (_M0L6_2atmpS1906.$1) {
    moonbit_decref(_M0L6_2atmpS1906.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS64,
  int32_t _M0L3objS63
) {
  struct _M0TPB6Logger _M0L6_2atmpS1907;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS64);
  _M0L6_2atmpS1907
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS64
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS63, _M0L6_2atmpS1907);
  if (_M0L6_2atmpS1907.$1) {
    moonbit_decref(_M0L6_2atmpS1907.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS66,
  float _M0L3objS65
) {
  struct _M0TPB6Logger _M0L6_2atmpS1908;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS66);
  _M0L6_2atmpS1908
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS66
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS65, _M0L6_2atmpS1908);
  if (_M0L6_2atmpS1908.$1) {
    moonbit_decref(_M0L6_2atmpS1908.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS68,
  uint64_t _M0L3objS67
) {
  struct _M0TPB6Logger _M0L6_2atmpS1909;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS68);
  _M0L6_2atmpS1909
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS68
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS67, _M0L6_2atmpS1909);
  if (_M0L6_2atmpS1909.$1) {
    moonbit_decref(_M0L6_2atmpS1909.$1);
  }
  return 0;
}

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGsE(
  moonbit_string_t* _M0L3srcS52,
  int32_t _M0L13allocate__lenS50,
  int32_t _M0L11src__offsetS53,
  int32_t _M0L11dst__offsetS51,
  int32_t _M0L9blit__lenS54
) {
  moonbit_string_t* _M0L3dstS49;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS49
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS50, (moonbit_string_t)moonbit_string_literal_82.data);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L3dstS49, _M0L11dst__offsetS51, _M0L3srcS52, _M0L11src__offsetS53, _M0L9blit__lenS54);
  moonbit_decref(_M0L3srcS52);
  return _M0L3dstS49;
}

struct _M0TUsfE** _M0MPB18UninitializedArray23unsafe__make__and__blitGUsfEE(
  struct _M0TUsfE** _M0L3srcS58,
  int32_t _M0L13allocate__lenS56,
  int32_t _M0L11src__offsetS59,
  int32_t _M0L11dst__offsetS57,
  int32_t _M0L9blit__lenS60
) {
  struct _M0TUsfE** _M0L3dstS55;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS55
  = (struct _M0TUsfE**)moonbit_make_ref_array(_M0L13allocate__lenS56, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsfEE(_M0L3dstS55, _M0L11dst__offsetS57, _M0L3srcS58, _M0L11src__offsetS59, _M0L9blit__lenS60);
  moonbit_decref(_M0L3srcS58);
  return _M0L3dstS55;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS39,
  int32_t _M0L11dst__offsetS40,
  moonbit_string_t* _M0L3srcS41,
  int32_t _M0L11src__offsetS42,
  int32_t _M0L3lenS43
) {
  #line 119 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_incref(_M0L3srcS41);
  moonbit_incref(_M0L3dstS39);
  #line 128 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_unsafe_ref_array_blit(_M0L3dstS39, _M0L11dst__offsetS40, _M0L3srcS41, _M0L11src__offsetS42, _M0L3lenS43);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsfEE(
  struct _M0TUsfE** _M0L3dstS44,
  int32_t _M0L11dst__offsetS45,
  struct _M0TUsfE** _M0L3srcS46,
  int32_t _M0L11src__offsetS47,
  int32_t _M0L3lenS48
) {
  #line 119 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_incref(_M0L3srcS46);
  moonbit_incref(_M0L3dstS44);
  #line 128 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  moonbit_unsafe_ref_array_blit(_M0L3dstS44, _M0L11dst__offsetS45, _M0L3srcS46, _M0L11src__offsetS47, _M0L3lenS48);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t* _M0L3dstS12,
  int32_t _M0L11dst__offsetS14,
  uint16_t* _M0L3srcS13,
  int32_t _M0L11src__offsetS15,
  int32_t _M0L3lenS17
) {
  int32_t _if__result_4045;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS12 == _M0L3srcS13) {
    _if__result_4045 = _M0L11dst__offsetS14 < _M0L11src__offsetS15;
  } else {
    _if__result_4045 = 0;
  }
  if (_if__result_4045) {
    int32_t _M0L1iS16 = 0;
    while (1) {
      if (_M0L1iS16 < _M0L3lenS17) {
        int32_t _M0L6_2atmpS1879 = _M0L11dst__offsetS14 + _M0L1iS16;
        int32_t _M0L6_2atmpS1881 = _M0L11src__offsetS15 + _M0L1iS16;
        int32_t _M0L6_2atmpS1880;
        int32_t _M0L6_2atmpS1882;
        if (
          _M0L6_2atmpS1881 < 0
          || _M0L6_2atmpS1881 >= Moonbit_array_length(_M0L3srcS13)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1880 = (int32_t)_M0L3srcS13[_M0L6_2atmpS1881];
        if (
          _M0L6_2atmpS1879 < 0
          || _M0L6_2atmpS1879 >= Moonbit_array_length(_M0L3dstS12)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS12[_M0L6_2atmpS1879] = _M0L6_2atmpS1880;
        _M0L6_2atmpS1882 = _M0L1iS16 + 1;
        _M0L1iS16 = _M0L6_2atmpS1882;
        continue;
      } else {
        moonbit_decref(_M0L3srcS13);
        moonbit_decref(_M0L3dstS12);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1887 = _M0L3lenS17 - 1;
    int32_t _M0L1iS19 = _M0L6_2atmpS1887;
    while (1) {
      if (_M0L1iS19 >= 0) {
        int32_t _M0L6_2atmpS1883 = _M0L11dst__offsetS14 + _M0L1iS19;
        int32_t _M0L6_2atmpS1885 = _M0L11src__offsetS15 + _M0L1iS19;
        int32_t _M0L6_2atmpS1884;
        int32_t _M0L6_2atmpS1886;
        if (
          _M0L6_2atmpS1885 < 0
          || _M0L6_2atmpS1885 >= Moonbit_array_length(_M0L3srcS13)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1884 = (int32_t)_M0L3srcS13[_M0L6_2atmpS1885];
        if (
          _M0L6_2atmpS1883 < 0
          || _M0L6_2atmpS1883 >= Moonbit_array_length(_M0L3dstS12)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS12[_M0L6_2atmpS1883] = _M0L6_2atmpS1884;
        _M0L6_2atmpS1886 = _M0L1iS19 - 1;
        _M0L1iS19 = _M0L6_2atmpS1886;
        continue;
      } else {
        moonbit_decref(_M0L3srcS13);
        moonbit_decref(_M0L3dstS12);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS21,
  int32_t _M0L11dst__offsetS23,
  moonbit_string_t* _M0L3srcS22,
  int32_t _M0L11src__offsetS24,
  int32_t _M0L3lenS26
) {
  int32_t _if__result_4048;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS21 == _M0L3srcS22) {
    _if__result_4048 = _M0L11dst__offsetS23 < _M0L11src__offsetS24;
  } else {
    _if__result_4048 = 0;
  }
  if (_if__result_4048) {
    int32_t _M0L1iS25 = 0;
    while (1) {
      if (_M0L1iS25 < _M0L3lenS26) {
        int32_t _M0L6_2atmpS1888 = _M0L11dst__offsetS23 + _M0L1iS25;
        int32_t _M0L6_2atmpS1890 = _M0L11src__offsetS24 + _M0L1iS25;
        moonbit_string_t _M0L6_2atmpS1889;
        moonbit_string_t _M0L6_2aoldS3726;
        int32_t _M0L6_2atmpS1891;
        if (
          _M0L6_2atmpS1890 < 0
          || _M0L6_2atmpS1890 >= Moonbit_array_length(_M0L3srcS22)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1889 = (moonbit_string_t)_M0L3srcS22[_M0L6_2atmpS1890];
        if (
          _M0L6_2atmpS1888 < 0
          || _M0L6_2atmpS1888 >= Moonbit_array_length(_M0L3dstS21)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3726 = (moonbit_string_t)_M0L3dstS21[_M0L6_2atmpS1888];
        moonbit_incref(_M0L6_2atmpS1889);
        moonbit_decref(_M0L6_2aoldS3726);
        _M0L3dstS21[_M0L6_2atmpS1888] = _M0L6_2atmpS1889;
        _M0L6_2atmpS1891 = _M0L1iS25 + 1;
        _M0L1iS25 = _M0L6_2atmpS1891;
        continue;
      } else {
        moonbit_decref(_M0L3srcS22);
        moonbit_decref(_M0L3dstS21);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1896 = _M0L3lenS26 - 1;
    int32_t _M0L1iS28 = _M0L6_2atmpS1896;
    while (1) {
      if (_M0L1iS28 >= 0) {
        int32_t _M0L6_2atmpS1892 = _M0L11dst__offsetS23 + _M0L1iS28;
        int32_t _M0L6_2atmpS1894 = _M0L11src__offsetS24 + _M0L1iS28;
        moonbit_string_t _M0L6_2atmpS1893;
        moonbit_string_t _M0L6_2aoldS3728;
        int32_t _M0L6_2atmpS1895;
        if (
          _M0L6_2atmpS1894 < 0
          || _M0L6_2atmpS1894 >= Moonbit_array_length(_M0L3srcS22)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1893 = (moonbit_string_t)_M0L3srcS22[_M0L6_2atmpS1894];
        if (
          _M0L6_2atmpS1892 < 0
          || _M0L6_2atmpS1892 >= Moonbit_array_length(_M0L3dstS21)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3728 = (moonbit_string_t)_M0L3dstS21[_M0L6_2atmpS1892];
        moonbit_incref(_M0L6_2atmpS1893);
        moonbit_decref(_M0L6_2aoldS3728);
        _M0L3dstS21[_M0L6_2atmpS1892] = _M0L6_2atmpS1893;
        _M0L6_2atmpS1895 = _M0L1iS28 - 1;
        _M0L1iS28 = _M0L6_2atmpS1895;
        continue;
      } else {
        moonbit_decref(_M0L3srcS22);
        moonbit_decref(_M0L3dstS21);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsfEEE(
  struct _M0TUsfE** _M0L3dstS30,
  int32_t _M0L11dst__offsetS32,
  struct _M0TUsfE** _M0L3srcS31,
  int32_t _M0L11src__offsetS33,
  int32_t _M0L3lenS35
) {
  int32_t _if__result_4051;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS30 == _M0L3srcS31) {
    _if__result_4051 = _M0L11dst__offsetS32 < _M0L11src__offsetS33;
  } else {
    _if__result_4051 = 0;
  }
  if (_if__result_4051) {
    int32_t _M0L1iS34 = 0;
    while (1) {
      if (_M0L1iS34 < _M0L3lenS35) {
        int32_t _M0L6_2atmpS1897 = _M0L11dst__offsetS32 + _M0L1iS34;
        int32_t _M0L6_2atmpS1899 = _M0L11src__offsetS33 + _M0L1iS34;
        struct _M0TUsfE* _M0L6_2atmpS1898;
        struct _M0TUsfE* _M0L6_2aoldS3730;
        int32_t _M0L6_2atmpS1900;
        if (
          _M0L6_2atmpS1899 < 0
          || _M0L6_2atmpS1899 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1898 = (struct _M0TUsfE*)_M0L3srcS31[_M0L6_2atmpS1899];
        if (
          _M0L6_2atmpS1897 < 0
          || _M0L6_2atmpS1897 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3730 = (struct _M0TUsfE*)_M0L3dstS30[_M0L6_2atmpS1897];
        if (_M0L6_2atmpS1898) {
          moonbit_incref(_M0L6_2atmpS1898);
        }
        if (_M0L6_2aoldS3730) {
          moonbit_decref(_M0L6_2aoldS3730);
        }
        _M0L3dstS30[_M0L6_2atmpS1897] = _M0L6_2atmpS1898;
        _M0L6_2atmpS1900 = _M0L1iS34 + 1;
        _M0L1iS34 = _M0L6_2atmpS1900;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1905 = _M0L3lenS35 - 1;
    int32_t _M0L1iS37 = _M0L6_2atmpS1905;
    while (1) {
      if (_M0L1iS37 >= 0) {
        int32_t _M0L6_2atmpS1901 = _M0L11dst__offsetS32 + _M0L1iS37;
        int32_t _M0L6_2atmpS1903 = _M0L11src__offsetS33 + _M0L1iS37;
        struct _M0TUsfE* _M0L6_2atmpS1902;
        struct _M0TUsfE* _M0L6_2aoldS3732;
        int32_t _M0L6_2atmpS1904;
        if (
          _M0L6_2atmpS1903 < 0
          || _M0L6_2atmpS1903 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1902 = (struct _M0TUsfE*)_M0L3srcS31[_M0L6_2atmpS1903];
        if (
          _M0L6_2atmpS1901 < 0
          || _M0L6_2atmpS1901 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3732 = (struct _M0TUsfE*)_M0L3dstS30[_M0L6_2atmpS1901];
        if (_M0L6_2atmpS1902) {
          moonbit_incref(_M0L6_2atmpS1902);
        }
        if (_M0L6_2aoldS3732) {
          moonbit_decref(_M0L6_2aoldS3732);
        }
        _M0L3dstS30[_M0L6_2atmpS1901] = _M0L6_2atmpS1902;
        _M0L6_2atmpS1904 = _M0L1iS37 - 1;
        _M0L1iS37 = _M0L6_2atmpS1904;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPB18UninitializedArray6lengthGsE(moonbit_string_t* _M0L4selfS10) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS10);
}

int32_t _M0MPB18UninitializedArray6lengthGUsfEE(
  struct _M0TUsfE** _M0L4selfS11
) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS11);
}

uint32_t _M0FPB13consume4__acc(uint32_t _M0L3accS8, uint32_t _M0L5inputS9) {
  uint32_t _M0L6_2atmpS1878;
  uint32_t _M0L6_2atmpS1877;
  uint32_t _M0L6_2atmpS1876;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1878 = _M0L5inputS9 * 3266489917u;
  _M0L6_2atmpS1877 = _M0L3accS8 + _M0L6_2atmpS1878;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1876 = _M0FPB4rotl(_M0L6_2atmpS1877, 17);
  return _M0L6_2atmpS1876 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS6, int32_t _M0L1rS7) {
  uint32_t _M0L6_2atmpS1873;
  int32_t _M0L6_2atmpS1875;
  uint32_t _M0L6_2atmpS1874;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1873 = _M0L1xS6 << (_M0L1rS7 & 31);
  _M0L6_2atmpS1875 = 32 - _M0L1rS7;
  _M0L6_2atmpS1874 = _M0L1xS6 >> (_M0L6_2atmpS1875 & 31);
  return _M0L6_2atmpS1873 | _M0L6_2atmpS1874;
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

struct _M0TUsfE** _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS4);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS5) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS5);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

int32_t _M0IP016_24default__implPB6Logger61write_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1764,
  struct _M0TPB4Show _M0L8_2aparamS1763
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1762 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1764;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS1762, _M0L8_2aparamS1763);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1761,
  struct _M0TPB4Show _M0L8_2aparamS1760
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1759 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1761;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS1759, _M0L8_2aparamS1760);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1758,
  int32_t _M0L8_2aparamS1757
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1756 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1758;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1756, _M0L8_2aparamS1757);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1755,
  struct _M0TPC16string10StringView _M0L8_2aparamS1754
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1753 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1755;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1753, _M0L8_2aparamS1754);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1752,
  moonbit_string_t _M0L8_2aparamS1749,
  int32_t _M0L8_2aparamS1750,
  int32_t _M0L8_2aparamS1751
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1748 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1752;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1748, _M0L8_2aparamS1749, _M0L8_2aparamS1750, _M0L8_2aparamS1751);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1747,
  moonbit_string_t _M0L8_2aparamS1746
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1745 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1747;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1745, _M0L8_2aparamS1746);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L2dbS1629;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS1865;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS1866;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS1867;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS1868;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS1869;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS1870;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS1871;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6_2atmpS1872;
  struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player** _M0L6_2atmpS1864;
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE* _M0L7playersS1630;
  int32_t _M0L1iS1631;
  struct _M0TPB5ArrayGsE* _M0L9top5__idsS1639;
  int32_t _M0L1iS1640;
  struct _M0TUsfE* _M0L8_2atupleS1860;
  struct _M0TUsfE* _M0L8_2atupleS1861;
  struct _M0TUsfE* _M0L8_2atupleS1862;
  struct _M0TUsfE* _M0L8_2atupleS1863;
  struct _M0TUsfE** _M0L6_2atmpS1859;
  struct _M0TPB5ArrayGUsfEE* _M0L7updatesS1652;
  int32_t _M0L1iS1653;
  struct _M0TPB5ArrayGsE* _M0L8all__idsS1670;
  int32_t _M0L1iS1671;
  int32_t _M0L15count__200__400S1695;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1696;
  moonbit_string_t _M0L6_2atmpS1803;
  struct _M0TPB5ArrayGsE* _M0L12mid__playersS1697;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1698;
  moonbit_string_t _M0L7_2abindS1699;
  int32_t _M0L6_2atmpS1807;
  struct _M0TPC16string10StringView _M0L6_2atmpS1806;
  moonbit_string_t _M0L6_2atmpS1805;
  moonbit_string_t _M0L6_2atmpS1804;
  int32_t _M0L6_2atmpS1808;
  int32_t _M0L6_2atmpS1809;
  int32_t _M0L6_2atmpS1810;
  int32_t _M0L6_2atmpS1811;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1700;
  int32_t _M0L6_2atmpS1813;
  moonbit_string_t _M0L6_2atmpS1812;
  int32_t _M0L1iS1701;
  int32_t _M0L6_2atmpS1820;
  int32_t _M0L6_2atmpS1821;
  int32_t _M0L6_2atmpS1822;
  int32_t _M0L6_2atmpS1823;
  int32_t _M0L6_2atmpS1824;
  int32_t _M0L6_2atmpS1825;
  moonbit_string_t* _M0L6_2atmpS1858;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1857;
  struct _M0TPB5ArrayGsE* _M0L6mutualS1720;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1721;
  int32_t _M0L6_2atmpS1827;
  moonbit_string_t _M0L6_2atmpS1826;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1722;
  moonbit_string_t _M0L7_2abindS1723;
  int32_t _M0L6_2atmpS1831;
  struct _M0TPC16string10StringView _M0L6_2atmpS1830;
  moonbit_string_t _M0L6_2atmpS1829;
  moonbit_string_t _M0L6_2atmpS1828;
  moonbit_string_t* _M0L6_2atmpS1856;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1855;
  struct _M0TPB5ArrayGsE* _M0L12all__friendsS1724;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1725;
  int32_t _M0L6_2atmpS1833;
  moonbit_string_t _M0L6_2atmpS1832;
  moonbit_string_t* _M0L6_2atmpS1854;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1853;
  struct _M0TPB5ArrayGsE* _M0L8only__p1S1726;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1727;
  moonbit_string_t _M0L7_2abindS1728;
  int32_t _M0L6_2atmpS1837;
  struct _M0TPC16string10StringView _M0L6_2atmpS1836;
  moonbit_string_t _M0L6_2atmpS1835;
  moonbit_string_t _M0L6_2atmpS1834;
  moonbit_string_t* _M0L6_2atmpS1852;
  struct _M0TPB5ArrayGsE* _M0L8messagesS1729;
  int32_t _M0L7_2abindS1730;
  int32_t _M0L2__S1731;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1734;
  int32_t _M0L6_2atmpS1842;
  moonbit_string_t _M0L6_2atmpS1841;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1742;
  int32_t _M0L6_2atmpS1846;
  moonbit_string_t _M0L6_2atmpS1845;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1743;
  int32_t _M0L6_2atmpS1848;
  moonbit_string_t _M0L6_2atmpS1847;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1744;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1851;
  int32_t _M0L6_2atmpS1850;
  moonbit_string_t _M0L6_2atmpS1849;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 18 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L2dbS1629 = _M0MP38JIA2JIA29moonbitdb3lib8Database3new();
  #line 20 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_97.data);
  #line 21 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_98.data);
  #line 22 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_97.data);
  #line 25 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1865
  = _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_99.data, (moonbit_string_t)moonbit_string_literal_100.data);
  #line 26 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1866
  = _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_101.data, (moonbit_string_t)moonbit_string_literal_102.data);
  #line 27 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1867
  = _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_103.data, (moonbit_string_t)moonbit_string_literal_104.data);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1868
  = _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_105.data, (moonbit_string_t)moonbit_string_literal_106.data);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1869
  = _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_107.data, (moonbit_string_t)moonbit_string_literal_108.data);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1870
  = _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_109.data, (moonbit_string_t)moonbit_string_literal_110.data);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1871
  = _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_111.data, (moonbit_string_t)moonbit_string_literal_112.data);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1872
  = _M0MP48JIA2JIA29moonbitdb8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_113.data, (moonbit_string_t)moonbit_string_literal_114.data);
  _M0L6_2atmpS1864
  = (struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player**)moonbit_make_ref_array_raw(8);
  _M0L6_2atmpS1864[0] = _M0L6_2atmpS1865;
  _M0L6_2atmpS1864[1] = _M0L6_2atmpS1866;
  _M0L6_2atmpS1864[2] = _M0L6_2atmpS1867;
  _M0L6_2atmpS1864[3] = _M0L6_2atmpS1868;
  _M0L6_2atmpS1864[4] = _M0L6_2atmpS1869;
  _M0L6_2atmpS1864[5] = _M0L6_2atmpS1870;
  _M0L6_2atmpS1864[6] = _M0L6_2atmpS1871;
  _M0L6_2atmpS1864[7] = _M0L6_2atmpS1872;
  _M0L7playersS1630
  = (struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE));
  Moonbit_object_header(_M0L7playersS1630)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 115, 0);
  _M0L7playersS1630->$0 = _M0L6_2atmpS1864;
  _M0L7playersS1630->$1 = 8;
  #line 35 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_115.data);
  #line 36 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  _M0L1iS1631 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1765;
    #line 37 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS1765
    = _M0MPC15array5Array6lengthGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(_M0L7playersS1630);
    if (_M0L1iS1631 < _M0L6_2atmpS1765) {
      struct _M0TP48JIA2JIA29moonbitdb8examples11leaderboard6Player* _M0L6playerS1632;
      int32_t _M0L6_2atmpS1784;
      int32_t _M0L6_2atmpS1783;
      float _M0L5scoreS1633;
      moonbit_string_t _M0L2idS1767;
      int32_t _M0L6_2atmpS1766;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1634;
      moonbit_string_t _M0L2idS1771;
      moonbit_string_t _M0L6_2atmpS1769;
      moonbit_string_t _M0L4nameS1770;
      int32_t _M0L6_2atmpS1768;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1635;
      moonbit_string_t _M0L2idS1777;
      moonbit_string_t _M0L6_2atmpS1773;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1636;
      int32_t _M0L6_2atmpS1776;
      int32_t _M0L6_2atmpS1775;
      moonbit_string_t _M0L6_2atmpS1774;
      int32_t _M0L6_2atmpS1772;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1637;
      moonbit_string_t _M0L4nameS1779;
      moonbit_string_t _M0L8_2afieldS3738;
      int32_t _M0L6_2acntS3798;
      moonbit_string_t _M0L2idS1780;
      int32_t _M0L6_2atmpS1782;
      int32_t _M0L6_2atmpS1781;
      moonbit_string_t _M0L6_2atmpS1778;
      int32_t _M0L6_2atmpS1785;
      #line 38 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6playerS1632
      = _M0MPC15array5Array2atGRP48JIA2JIA29moonbitdb8examples11leaderboard6PlayerE(_M0L7playersS1630, _M0L1iS1631);
      _M0L6_2atmpS1784 = _M0L1iS1631 * 100;
      _M0L6_2atmpS1783 = _M0L6_2atmpS1784 + 50;
      _M0L5scoreS1633 = (float)_M0L6_2atmpS1783;
      _M0L2idS1767 = _M0L6playerS1632->$0;
      moonbit_incref(_M0L2idS1767);
      #line 40 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1766
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, _M0L5scoreS1633, _M0L2idS1767);
      moonbit_decref(_M0L2idS1767);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1634
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1634, (moonbit_string_t)moonbit_string_literal_118.data);
      _M0L2idS1771 = _M0L6playerS1632->$0;
      moonbit_incref(_M0L2idS1771);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1634, _M0L2idS1771);
      moonbit_decref(_M0L2idS1771);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1769
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1634);
      moonbit_decref(_M0L18_2astring__builderS1634);
      _M0L4nameS1770 = _M0L6playerS1632->$1;
      moonbit_incref(_M0L4nameS1770);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1768
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1629, _M0L6_2atmpS1769, (moonbit_string_t)moonbit_string_literal_119.data, _M0L4nameS1770);
      moonbit_decref(_M0L6_2atmpS1769);
      moonbit_decref(_M0L4nameS1770);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1635
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1635, (moonbit_string_t)moonbit_string_literal_118.data);
      _M0L2idS1777 = _M0L6playerS1632->$0;
      moonbit_incref(_M0L2idS1777);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1635, _M0L2idS1777);
      moonbit_decref(_M0L2idS1777);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1773
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1635);
      moonbit_decref(_M0L18_2astring__builderS1635);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1636
      = _M0MPB13StringBuilder21StringBuilder_2einner(0);
      _M0L6_2atmpS1776 = _M0L1iS1631 * 100;
      _M0L6_2atmpS1775 = _M0L6_2atmpS1776 + 50;
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1636, _M0L6_2atmpS1775);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1774
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1636);
      moonbit_decref(_M0L18_2astring__builderS1636);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1772
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1629, _M0L6_2atmpS1773, (moonbit_string_t)moonbit_string_literal_120.data, _M0L6_2atmpS1774);
      moonbit_decref(_M0L6_2atmpS1773);
      moonbit_decref(_M0L6_2atmpS1774);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1637
      = _M0MPB13StringBuilder21StringBuilder_2einner(22);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1637, (moonbit_string_t)moonbit_string_literal_121.data);
      _M0L4nameS1779 = _M0L6playerS1632->$1;
      moonbit_incref(_M0L4nameS1779);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1637, _M0L4nameS1779);
      moonbit_decref(_M0L4nameS1779);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1637, (moonbit_string_t)moonbit_string_literal_122.data);
      _M0L8_2afieldS3738 = _M0L6playerS1632->$0;
      _M0L6_2acntS3798
      = Moonbit_rc_count(Moonbit_object_header(_M0L6playerS1632));
      if (_M0L6_2acntS3798 > 1) {
        int32_t _M0L11_2anew__cntS3800 = _M0L6_2acntS3798 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L6playerS1632), _M0L11_2anew__cntS3800);
        moonbit_incref(_M0L8_2afieldS3738);
      } else if (_M0L6_2acntS3798 == 1) {
        moonbit_string_t _M0L8_2afieldS3799 = _M0L6playerS1632->$1;
        moonbit_decref(_M0L8_2afieldS3799);
        #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        moonbit_free(_M0L6playerS1632);
      }
      _M0L2idS1780 = _M0L8_2afieldS3738;
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1637, _M0L2idS1780);
      moonbit_decref(_M0L2idS1780);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1637, (moonbit_string_t)moonbit_string_literal_123.data);
      _M0L6_2atmpS1782 = _M0L1iS1631 * 100;
      _M0L6_2atmpS1781 = _M0L6_2atmpS1782 + 50;
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1637, _M0L6_2atmpS1781);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1637, (moonbit_string_t)moonbit_string_literal_124.data);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1778
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1637);
      moonbit_decref(_M0L18_2astring__builderS1637);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1778);
      moonbit_decref(_M0L6_2atmpS1778);
      _M0L6_2atmpS1785 = _M0L1iS1631 + 1;
      _M0L1iS1631 = _M0L6_2atmpS1785;
      continue;
    }
    break;
  }
  #line 46 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_125.data);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  #line 48 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L9top5__idsS1639
  = _M0MP38JIA2JIA29moonbitdb3lib8Database9zrevrange(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, 0, 4);
  #line 49 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_126.data);
  #line 50 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_127.data);
  _M0L1iS1640 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1786;
    #line 51 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS1786 = _M0MPC15array5Array6lengthGsE(_M0L9top5__idsS1639);
    if (_M0L1iS1640 < _M0L6_2atmpS1786) {
      moonbit_string_t _M0L3pidS1641;
      moonbit_string_t _M0L1nS1644;
      moonbit_string_t _M0L5pnameS1642;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1646;
      moonbit_string_t _M0L6_2atmpS1790;
      moonbit_string_t _M0L7_2abindS1645;
      void* _M0L6_2atmpS1789;
      moonbit_string_t _M0L5scoreS1649;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1650;
      int32_t _M0L6_2atmpS1788;
      moonbit_string_t _M0L6_2atmpS1787;
      int32_t _M0L6_2atmpS1791;
      #line 52 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L3pidS1641
      = _M0MPC15array5Array2atGsE(_M0L9top5__idsS1639, _M0L1iS1640);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1646
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1646, (moonbit_string_t)moonbit_string_literal_118.data);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1646, _M0L3pidS1641);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1790
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1646);
      moonbit_decref(_M0L18_2astring__builderS1646);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS1645
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1629, _M0L6_2atmpS1790, (moonbit_string_t)moonbit_string_literal_119.data);
      moonbit_decref(_M0L6_2atmpS1790);
      if (_M0L7_2abindS1645 == 0) {
        if (_M0L7_2abindS1645) {
          moonbit_decref(_M0L7_2abindS1645);
        }
        _M0L5pnameS1642 = (moonbit_string_t)moonbit_string_literal_128.data;
      } else {
        moonbit_string_t _M0L7_2aSomeS1647 = _M0L7_2abindS1645;
        moonbit_string_t _M0L4_2anS1648 = _M0L7_2aSomeS1647;
        _M0L1nS1644 = _M0L4_2anS1648;
        goto join_1643;
      }
      goto joinlet_4056;
      join_1643:;
      _M0L5pnameS1642 = _M0L1nS1644;
      joinlet_4056:;
      #line 57 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1789
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, _M0L3pidS1641);
      #line 57 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L5scoreS1649
      = _M0FP48JIA2JIA29moonbitdb8examples11leaderboard16show__opt__float(_M0L6_2atmpS1789);
      moonbit_decref(_M0L6_2atmpS1789);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1650
      = _M0MPB13StringBuilder21StringBuilder_2einner(21);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1650, (moonbit_string_t)moonbit_string_literal_129.data);
      _M0L6_2atmpS1788 = _M0L1iS1640 + 1;
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1650, _M0L6_2atmpS1788);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1650, (moonbit_string_t)moonbit_string_literal_130.data);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1650, _M0L3pidS1641);
      moonbit_decref(_M0L3pidS1641);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1650, (moonbit_string_t)moonbit_string_literal_131.data);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1650, _M0L5pnameS1642);
      moonbit_decref(_M0L5pnameS1642);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1650, (moonbit_string_t)moonbit_string_literal_132.data);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1650, _M0L5scoreS1649);
      moonbit_decref(_M0L5scoreS1649);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1787
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1650);
      moonbit_decref(_M0L18_2astring__builderS1650);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1787);
      moonbit_decref(_M0L6_2atmpS1787);
      _M0L6_2atmpS1791 = _M0L1iS1640 + 1;
      _M0L1iS1640 = _M0L6_2atmpS1791;
      continue;
    } else {
      moonbit_decref(_M0L9top5__idsS1639);
    }
    break;
  }
  #line 61 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_133.data);
  #line 62 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  _M0L8_2atupleS1860
  = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
  Moonbit_object_header(_M0L8_2atupleS1860)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
  _M0L8_2atupleS1860->$0 = (moonbit_string_t)moonbit_string_literal_99.data;
  _M0L8_2atupleS1860->$1 = 0x1.f4p+7f;
  _M0L8_2atupleS1861
  = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
  Moonbit_object_header(_M0L8_2atupleS1861)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
  _M0L8_2atupleS1861->$0 = (moonbit_string_t)moonbit_string_literal_103.data;
  _M0L8_2atupleS1861->$1 = 0x1.68p+7f;
  _M0L8_2atupleS1862
  = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
  Moonbit_object_header(_M0L8_2atupleS1862)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
  _M0L8_2atupleS1862->$0 = (moonbit_string_t)moonbit_string_literal_107.data;
  _M0L8_2atupleS1862->$1 = 0x1.4p+8f;
  _M0L8_2atupleS1863
  = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
  Moonbit_object_header(_M0L8_2atupleS1863)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
  _M0L8_2atupleS1863->$0 = (moonbit_string_t)moonbit_string_literal_111.data;
  _M0L8_2atupleS1863->$1 = 0x1.9ap+8f;
  _M0L6_2atmpS1859 = (struct _M0TUsfE**)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1859[0] = _M0L8_2atupleS1860;
  _M0L6_2atmpS1859[1] = _M0L8_2atupleS1861;
  _M0L6_2atmpS1859[2] = _M0L8_2atupleS1862;
  _M0L6_2atmpS1859[3] = _M0L8_2atupleS1863;
  _M0L7updatesS1652
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L7updatesS1652)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
  _M0L7updatesS1652->$0 = _M0L6_2atmpS1859;
  _M0L7updatesS1652->$1 = 4;
  _M0L1iS1653 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1792;
    #line 64 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS1792 = _M0MPC15array5Array6lengthGUsfEE(_M0L7updatesS1652);
    if (_M0L1iS1653 < _M0L6_2atmpS1792) {
      moonbit_string_t _M0L3pidS1655;
      float _M0L10add__scoreS1656;
      struct _M0TUsfE* _M0L7_2abindS1666;
      moonbit_string_t _M0L6_2apidS1667;
      float _M0L13_2aadd__scoreS1668;
      int32_t _M0L6_2acntS3801;
      float _M0L10new__scoreS1657;
      moonbit_string_t _M0L1nS1660;
      moonbit_string_t _M0L5pnameS1658;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1662;
      moonbit_string_t _M0L6_2atmpS1794;
      moonbit_string_t _M0L7_2abindS1661;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1665;
      moonbit_string_t _M0L6_2atmpS1793;
      int32_t _M0L6_2atmpS1795;
      #line 65 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS1666
      = _M0MPC15array5Array2atGUsfEE(_M0L7updatesS1652, _M0L1iS1653);
      _M0L6_2apidS1667 = _M0L7_2abindS1666->$0;
      _M0L13_2aadd__scoreS1668 = _M0L7_2abindS1666->$1;
      _M0L6_2acntS3801
      = Moonbit_rc_count(Moonbit_object_header(_M0L7_2abindS1666));
      if (_M0L6_2acntS3801 > 1) {
        int32_t _M0L11_2anew__cntS3802 = _M0L6_2acntS3801 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2abindS1666), _M0L11_2anew__cntS3802);
        moonbit_incref(_M0L6_2apidS1667);
      } else if (_M0L6_2acntS3801 == 1) {
        #line 65 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        moonbit_free(_M0L7_2abindS1666);
      }
      _M0L3pidS1655 = _M0L6_2apidS1667;
      _M0L10add__scoreS1656 = _M0L13_2aadd__scoreS1668;
      goto join_1654;
      goto joinlet_4058;
      join_1654:;
      #line 66 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L10new__scoreS1657
      = _M0MP38JIA2JIA29moonbitdb3lib8Database7zincrby(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, _M0L10add__scoreS1656, _M0L3pidS1655);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1662
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1662, (moonbit_string_t)moonbit_string_literal_118.data);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1662, _M0L3pidS1655);
      moonbit_decref(_M0L3pidS1655);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1794
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1662);
      moonbit_decref(_M0L18_2astring__builderS1662);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS1661
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1629, _M0L6_2atmpS1794, (moonbit_string_t)moonbit_string_literal_119.data);
      moonbit_decref(_M0L6_2atmpS1794);
      if (_M0L7_2abindS1661 == 0) {
        if (_M0L7_2abindS1661) {
          moonbit_decref(_M0L7_2abindS1661);
        }
        _M0L5pnameS1658 = (moonbit_string_t)moonbit_string_literal_128.data;
      } else {
        moonbit_string_t _M0L7_2aSomeS1663 = _M0L7_2abindS1661;
        moonbit_string_t _M0L4_2anS1664 = _M0L7_2aSomeS1663;
        _M0L1nS1660 = _M0L4_2anS1664;
        goto join_1659;
      }
      goto joinlet_4059;
      join_1659:;
      _M0L5pnameS1658 = _M0L1nS1660;
      joinlet_4059:;
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1665
      = _M0MPB13StringBuilder21StringBuilder_2einner(28);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1665, (moonbit_string_t)moonbit_string_literal_134.data);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1665, _M0L5pnameS1658);
      moonbit_decref(_M0L5pnameS1658);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1665, (moonbit_string_t)moonbit_string_literal_135.data);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1665, _M0L10add__scoreS1656);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1665, (moonbit_string_t)moonbit_string_literal_136.data);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1665, _M0L10new__scoreS1657);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1793
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1665);
      moonbit_decref(_M0L18_2astring__builderS1665);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1793);
      moonbit_decref(_M0L6_2atmpS1793);
      joinlet_4058:;
      _M0L6_2atmpS1795 = _M0L1iS1653 + 1;
      _M0L1iS1653 = _M0L6_2atmpS1795;
      continue;
    } else {
      moonbit_decref(_M0L7updatesS1652);
    }
    break;
  }
  #line 74 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_137.data);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  #line 76 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L8all__idsS1670
  = _M0MP38JIA2JIA29moonbitdb3lib8Database9zrevrange(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, 0, -1);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_138.data);
  #line 78 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_139.data);
  _M0L1iS1671 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1796;
    #line 79 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS1796 = _M0MPC15array5Array6lengthGsE(_M0L8all__idsS1670);
    if (_M0L1iS1671 < _M0L6_2atmpS1796) {
      moonbit_string_t _M0L3pidS1672;
      moonbit_string_t _M0L1nS1675;
      moonbit_string_t _M0L5pnameS1673;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1677;
      moonbit_string_t _M0L6_2atmpS1801;
      moonbit_string_t _M0L7_2abindS1676;
      void* _M0L6_2atmpS1800;
      moonbit_string_t _M0L5scoreS1680;
      int64_t _M0L9new__rankS1681;
      int32_t _M0L1rS1684;
      moonbit_string_t _M0L9rank__strS1682;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1685;
      int32_t _M0L6_2atmpS1799;
      int32_t _M0L8old__idxS1688;
      int32_t _M0L6changeS1689;
      moonbit_string_t _M0L11change__strS1690;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1693;
      moonbit_string_t _M0L6_2atmpS1797;
      int32_t _M0L6_2atmpS1802;
      #line 80 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L3pidS1672
      = _M0MPC15array5Array2atGsE(_M0L8all__idsS1670, _M0L1iS1671);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1677
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1677, (moonbit_string_t)moonbit_string_literal_118.data);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1677, _M0L3pidS1672);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1801
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1677);
      moonbit_decref(_M0L18_2astring__builderS1677);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS1676
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1629, _M0L6_2atmpS1801, (moonbit_string_t)moonbit_string_literal_119.data);
      moonbit_decref(_M0L6_2atmpS1801);
      if (_M0L7_2abindS1676 == 0) {
        if (_M0L7_2abindS1676) {
          moonbit_decref(_M0L7_2abindS1676);
        }
        _M0L5pnameS1673 = (moonbit_string_t)moonbit_string_literal_128.data;
      } else {
        moonbit_string_t _M0L7_2aSomeS1678 = _M0L7_2abindS1676;
        moonbit_string_t _M0L4_2anS1679 = _M0L7_2aSomeS1678;
        _M0L1nS1675 = _M0L4_2anS1679;
        goto join_1674;
      }
      goto joinlet_4061;
      join_1674:;
      _M0L5pnameS1673 = _M0L1nS1675;
      joinlet_4061:;
      #line 85 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1800
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, _M0L3pidS1672);
      #line 85 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L5scoreS1680
      = _M0FP48JIA2JIA29moonbitdb8examples11leaderboard16show__opt__float(_M0L6_2atmpS1800);
      moonbit_decref(_M0L6_2atmpS1800);
      #line 86 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L9new__rankS1681
      = _M0MP38JIA2JIA29moonbitdb3lib8Database8zrevrank(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, _M0L3pidS1672);
      if (_M0L9new__rankS1681 == 4294967296ll) {
        _M0L9rank__strS1682 = (moonbit_string_t)moonbit_string_literal_0.data;
      } else {
        int64_t _M0L7_2aSomeS1686 = _M0L9new__rankS1681;
        int32_t _M0L4_2arS1687 = (int32_t)_M0L7_2aSomeS1686;
        _M0L1rS1684 = _M0L4_2arS1687;
        goto join_1683;
      }
      goto joinlet_4062;
      join_1683:;
      #line 88 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1685
      = _M0MPB13StringBuilder21StringBuilder_2einner(1);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1685, (moonbit_string_t)moonbit_string_literal_140.data);
      _M0L6_2atmpS1799 = _M0L1rS1684 + 1;
      #line 88 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1685, _M0L6_2atmpS1799);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L9rank__strS1682
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1685);
      moonbit_decref(_M0L18_2astring__builderS1685);
      joinlet_4062:;
      #line 91 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L8old__idxS1688
      = _M0FP48JIA2JIA29moonbitdb8examples11leaderboard19find__player__index(_M0L7playersS1630, _M0L3pidS1672);
      if (_M0L8old__idxS1688 >= 0) {
        _M0L6changeS1689 = _M0L8old__idxS1688 - _M0L1iS1671;
      } else {
        _M0L6changeS1689 = 0;
      }
      if (_M0L6changeS1689 > 0) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1691;
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L18_2astring__builderS1691
        = _M0MPB13StringBuilder21StringBuilder_2einner(3);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1691, (moonbit_string_t)moonbit_string_literal_141.data);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1691, _M0L6changeS1689);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L11change__strS1690
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1691);
        moonbit_decref(_M0L18_2astring__builderS1691);
      } else if (_M0L6changeS1689 < 0) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1692;
        int32_t _M0L6_2atmpS1798;
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L18_2astring__builderS1692
        = _M0MPB13StringBuilder21StringBuilder_2einner(3);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1692, (moonbit_string_t)moonbit_string_literal_142.data);
        _M0L6_2atmpS1798 = -_M0L6changeS1689;
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1692, _M0L6_2atmpS1798);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L11change__strS1690
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1692);
        moonbit_decref(_M0L18_2astring__builderS1692);
      } else {
        _M0L11change__strS1690
        = (moonbit_string_t)moonbit_string_literal_143.data;
      }
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1693
      = _M0MPB13StringBuilder21StringBuilder_2einner(23);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1693, (moonbit_string_t)moonbit_string_literal_144.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1693, _M0L9rank__strS1682);
      moonbit_decref(_M0L9rank__strS1682);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1693, (moonbit_string_t)moonbit_string_literal_130.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1693, _M0L3pidS1672);
      moonbit_decref(_M0L3pidS1672);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1693, (moonbit_string_t)moonbit_string_literal_131.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1693, _M0L5pnameS1673);
      moonbit_decref(_M0L5pnameS1673);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1693, (moonbit_string_t)moonbit_string_literal_132.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1693, _M0L5scoreS1680);
      moonbit_decref(_M0L5scoreS1680);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1693, (moonbit_string_t)moonbit_string_literal_145.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1693, _M0L11change__strS1690);
      moonbit_decref(_M0L11change__strS1690);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1797
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1693);
      moonbit_decref(_M0L18_2astring__builderS1693);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1797);
      moonbit_decref(_M0L6_2atmpS1797);
      _M0L6_2atmpS1802 = _M0L1iS1671 + 1;
      _M0L1iS1671 = _M0L6_2atmpS1802;
      continue;
    } else {
      moonbit_decref(_M0L8all__idsS1670);
      moonbit_decref(_M0L7playersS1630);
    }
    break;
  }
  #line 97 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_146.data);
  #line 98 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L15count__200__400S1695
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6zcount(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, 0x1.9p+7f, 0x1.9p+8f);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1696
  = _M0MPB13StringBuilder21StringBuilder_2einner(30);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1696, (moonbit_string_t)moonbit_string_literal_147.data);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1696, _M0L15count__200__400S1695);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1803
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1696);
  moonbit_decref(_M0L18_2astring__builderS1696);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1803);
  moonbit_decref(_M0L6_2atmpS1803);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L12mid__playersS1697
  = _M0MP38JIA2JIA29moonbitdb3lib8Database13zrangebyscore(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_117.data, 0x1.9p+6f, 0x1.2cp+8f);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1698
  = _M0MPB13StringBuilder21StringBuilder_2einner(27);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1698, (moonbit_string_t)moonbit_string_literal_148.data);
  _M0L7_2abindS1699 = (moonbit_string_t)moonbit_string_literal_149.data;
  _M0L6_2atmpS1807 = Moonbit_array_length(_M0L7_2abindS1699);
  _M0L6_2atmpS1806
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1699, .$1 = 0, .$2 = _M0L6_2atmpS1807
  };
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1805
  = _M0MPC15array5Array4joinGsE(_M0L12mid__playersS1697, _M0L6_2atmpS1806);
  moonbit_decref(_M0L12mid__playersS1697);
  moonbit_decref(_M0L6_2atmpS1806.$0);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1698, _M0L6_2atmpS1805);
  moonbit_decref(_M0L6_2atmpS1805);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1804
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1698);
  moonbit_decref(_M0L18_2astring__builderS1698);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1804);
  moonbit_decref(_M0L6_2atmpS1804);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_150.data);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1808
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_151.data, 0x1.f4p+8f, (moonbit_string_t)moonbit_string_literal_99.data);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1809
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_151.data, 0x1.9p+8f, (moonbit_string_t)moonbit_string_literal_101.data);
  #line 108 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1810
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_151.data, 0x1.2cp+8f, (moonbit_string_t)moonbit_string_literal_103.data);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1811
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_151.data, 86400);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_152.data);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1700
  = _M0MPB13StringBuilder21StringBuilder_2einner(11);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1700, (moonbit_string_t)moonbit_string_literal_153.data);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1813
  = _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_151.data);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1700, _M0L6_2atmpS1813);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1700, (moonbit_string_t)moonbit_string_literal_154.data);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1812
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1700);
  moonbit_decref(_M0L18_2astring__builderS1700);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1812);
  moonbit_decref(_M0L6_2atmpS1812);
  #line 113 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_155.data);
  #line 114 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  _M0L1iS1701 = 0;
  while (1) {
    if (_M0L1iS1701 < 3) {
      struct _M0TUsfE* _M0L6winnerS1702;
      moonbit_string_t _M0L3pidS1704;
      float _M0L5scoreS1705;
      moonbit_string_t _M0L1nS1708;
      moonbit_string_t _M0L5pnameS1706;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1710;
      moonbit_string_t _M0L6_2atmpS1816;
      moonbit_string_t _M0L7_2abindS1709;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1713;
      int32_t _M0L6_2atmpS1815;
      moonbit_string_t _M0L6_2atmpS1814;
      int32_t _M0L6_2atmpS1819;
      #line 116 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6winnerS1702
      = _M0MP38JIA2JIA29moonbitdb3lib8Database7zpopmax(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_151.data);
      if (_M0L6winnerS1702 == 0) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1718;
        int32_t _M0L6_2atmpS1818;
        moonbit_string_t _M0L6_2atmpS1817;
        if (_M0L6winnerS1702) {
          moonbit_decref(_M0L6winnerS1702);
        }
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L18_2astring__builderS1718
        = _M0MPB13StringBuilder21StringBuilder_2einner(15);
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1718, (moonbit_string_t)moonbit_string_literal_156.data);
        _M0L6_2atmpS1818 = _M0L1iS1701 + 1;
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1718, _M0L6_2atmpS1818);
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1718, (moonbit_string_t)moonbit_string_literal_157.data);
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L6_2atmpS1817
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1718);
        moonbit_decref(_M0L18_2astring__builderS1718);
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0FPB7printlnGsE(_M0L6_2atmpS1817);
        moonbit_decref(_M0L6_2atmpS1817);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1714 = _M0L6winnerS1702;
        struct _M0TUsfE* _M0L4_2axS1715 = _M0L7_2aSomeS1714;
        moonbit_string_t _M0L6_2apidS1716 = _M0L4_2axS1715->$0;
        float _M0L8_2ascoreS1717 = _M0L4_2axS1715->$1;
        int32_t _M0L6_2acntS3803 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1715));
        if (_M0L6_2acntS3803 > 1) {
          int32_t _M0L11_2anew__cntS3804 = _M0L6_2acntS3803 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1715), _M0L11_2anew__cntS3804);
          moonbit_incref(_M0L6_2apidS1716);
        } else if (_M0L6_2acntS3803 == 1) {
          #line 117 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
          moonbit_free(_M0L4_2axS1715);
        }
        _M0L3pidS1704 = _M0L6_2apidS1716;
        _M0L5scoreS1705 = _M0L8_2ascoreS1717;
        goto join_1703;
      }
      goto joinlet_4064;
      join_1703:;
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1710
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1710, (moonbit_string_t)moonbit_string_literal_118.data);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1710, _M0L3pidS1704);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1816
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1710);
      moonbit_decref(_M0L18_2astring__builderS1710);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS1709
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1629, _M0L6_2atmpS1816, (moonbit_string_t)moonbit_string_literal_119.data);
      moonbit_decref(_M0L6_2atmpS1816);
      if (_M0L7_2abindS1709 == 0) {
        if (_M0L7_2abindS1709) {
          moonbit_decref(_M0L7_2abindS1709);
        }
        _M0L5pnameS1706 = (moonbit_string_t)moonbit_string_literal_128.data;
      } else {
        moonbit_string_t _M0L7_2aSomeS1711 = _M0L7_2abindS1709;
        moonbit_string_t _M0L4_2anS1712 = _M0L7_2aSomeS1711;
        _M0L1nS1708 = _M0L4_2anS1712;
        goto join_1707;
      }
      goto joinlet_4065;
      join_1707:;
      _M0L5pnameS1706 = _M0L1nS1708;
      joinlet_4065:;
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1713
      = _M0MPB13StringBuilder21StringBuilder_2einner(22);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1713, (moonbit_string_t)moonbit_string_literal_156.data);
      _M0L6_2atmpS1815 = _M0L1iS1701 + 1;
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1713, _M0L6_2atmpS1815);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1713, (moonbit_string_t)moonbit_string_literal_158.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1713, _M0L5pnameS1706);
      moonbit_decref(_M0L5pnameS1706);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1713, (moonbit_string_t)moonbit_string_literal_159.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1713, _M0L3pidS1704);
      moonbit_decref(_M0L3pidS1704);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1713, (moonbit_string_t)moonbit_string_literal_160.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1713, _M0L5scoreS1705);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1713, (moonbit_string_t)moonbit_string_literal_124.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1814
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1713);
      moonbit_decref(_M0L18_2astring__builderS1713);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1814);
      moonbit_decref(_M0L6_2atmpS1814);
      joinlet_4064:;
      _M0L6_2atmpS1819 = _M0L1iS1701 + 1;
      _M0L1iS1701 = _M0L6_2atmpS1819;
      continue;
    }
    break;
  }
  #line 129 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_161.data);
  #line 130 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  #line 131 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1820
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_162.data, (moonbit_string_t)moonbit_string_literal_101.data);
  #line 132 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1821
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_162.data, (moonbit_string_t)moonbit_string_literal_103.data);
  #line 133 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1822
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_162.data, (moonbit_string_t)moonbit_string_literal_105.data);
  #line 134 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1823
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_163.data, (moonbit_string_t)moonbit_string_literal_99.data);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1824
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_163.data, (moonbit_string_t)moonbit_string_literal_103.data);
  #line 136 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1825
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_163.data, (moonbit_string_t)moonbit_string_literal_107.data);
  _M0L6_2atmpS1858 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1858[0] = (moonbit_string_t)moonbit_string_literal_162.data;
  _M0L6_2atmpS1858[1] = (moonbit_string_t)moonbit_string_literal_163.data;
  _M0L6_2atmpS1857
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1857)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS1857->$0 = _M0L6_2atmpS1858;
  _M0L6_2atmpS1857->$1 = 2;
  #line 137 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6mutualS1720
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6sinter(_M0L2dbS1629, _M0L6_2atmpS1857);
  moonbit_decref(_M0L6_2atmpS1857);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1721
  = _M0MPB13StringBuilder21StringBuilder_2einner(36);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1721, (moonbit_string_t)moonbit_string_literal_164.data);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1827 = _M0MPC15array5Array6lengthGsE(_M0L6mutualS1720);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1721, _M0L6_2atmpS1827);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1826
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1721);
  moonbit_decref(_M0L18_2astring__builderS1721);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1826);
  moonbit_decref(_M0L6_2atmpS1826);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1722
  = _M0MPB13StringBuilder21StringBuilder_2einner(18);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1722, (moonbit_string_t)moonbit_string_literal_165.data);
  _M0L7_2abindS1723 = (moonbit_string_t)moonbit_string_literal_149.data;
  _M0L6_2atmpS1831 = Moonbit_array_length(_M0L7_2abindS1723);
  _M0L6_2atmpS1830
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1723, .$1 = 0, .$2 = _M0L6_2atmpS1831
  };
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1829
  = _M0MPC15array5Array4joinGsE(_M0L6mutualS1720, _M0L6_2atmpS1830);
  moonbit_decref(_M0L6mutualS1720);
  moonbit_decref(_M0L6_2atmpS1830.$0);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1722, _M0L6_2atmpS1829);
  moonbit_decref(_M0L6_2atmpS1829);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1828
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1722);
  moonbit_decref(_M0L18_2astring__builderS1722);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1828);
  moonbit_decref(_M0L6_2atmpS1828);
  _M0L6_2atmpS1856 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1856[0] = (moonbit_string_t)moonbit_string_literal_162.data;
  _M0L6_2atmpS1856[1] = (moonbit_string_t)moonbit_string_literal_163.data;
  _M0L6_2atmpS1855
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1855)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS1855->$0 = _M0L6_2atmpS1856;
  _M0L6_2atmpS1855->$1 = 2;
  #line 140 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L12all__friendsS1724
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6sunion(_M0L2dbS1629, _M0L6_2atmpS1855);
  moonbit_decref(_M0L6_2atmpS1855);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1725
  = _M0MPB13StringBuilder21StringBuilder_2einner(34);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1725, (moonbit_string_t)moonbit_string_literal_166.data);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1833 = _M0MPC15array5Array6lengthGsE(_M0L12all__friendsS1724);
  moonbit_decref(_M0L12all__friendsS1724);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1725, _M0L6_2atmpS1833);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1832
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1725);
  moonbit_decref(_M0L18_2astring__builderS1725);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1832);
  moonbit_decref(_M0L6_2atmpS1832);
  _M0L6_2atmpS1854 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1854[0] = (moonbit_string_t)moonbit_string_literal_162.data;
  _M0L6_2atmpS1854[1] = (moonbit_string_t)moonbit_string_literal_163.data;
  _M0L6_2atmpS1853
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1853)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS1853->$0 = _M0L6_2atmpS1854;
  _M0L6_2atmpS1853->$1 = 2;
  #line 142 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L8only__p1S1726
  = _M0MP38JIA2JIA29moonbitdb3lib8Database5sdiff(_M0L2dbS1629, _M0L6_2atmpS1853);
  moonbit_decref(_M0L6_2atmpS1853);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1727
  = _M0MPB13StringBuilder21StringBuilder_2einner(26);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1727, (moonbit_string_t)moonbit_string_literal_167.data);
  _M0L7_2abindS1728 = (moonbit_string_t)moonbit_string_literal_149.data;
  _M0L6_2atmpS1837 = Moonbit_array_length(_M0L7_2abindS1728);
  _M0L6_2atmpS1836
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1728, .$1 = 0, .$2 = _M0L6_2atmpS1837
  };
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1835
  = _M0MPC15array5Array4joinGsE(_M0L8only__p1S1726, _M0L6_2atmpS1836);
  moonbit_decref(_M0L8only__p1S1726);
  moonbit_decref(_M0L6_2atmpS1836.$0);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1727, _M0L6_2atmpS1835);
  moonbit_decref(_M0L6_2atmpS1835);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1834
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1727);
  moonbit_decref(_M0L18_2astring__builderS1727);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1834);
  moonbit_decref(_M0L6_2atmpS1834);
  #line 145 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_168.data);
  #line 146 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  _M0L6_2atmpS1852 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1852[0] = (moonbit_string_t)moonbit_string_literal_169.data;
  _M0L6_2atmpS1852[1] = (moonbit_string_t)moonbit_string_literal_170.data;
  _M0L6_2atmpS1852[2] = (moonbit_string_t)moonbit_string_literal_171.data;
  _M0L6_2atmpS1852[3] = (moonbit_string_t)moonbit_string_literal_172.data;
  _M0L8messagesS1729
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L8messagesS1729)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L8messagesS1729->$0 = _M0L6_2atmpS1852;
  _M0L8messagesS1729->$1 = 4;
  _M0L7_2abindS1730 = _M0L8messagesS1729->$1;
  _M0L2__S1731 = 0;
  while (1) {
    if (_M0L2__S1731 < _M0L7_2abindS1730) {
      moonbit_string_t* _M0L3bufS1840 = _M0L8messagesS1729->$0;
      moonbit_string_t _M0L3msgS1732 =
        (moonbit_string_t)_M0L3bufS1840[_M0L2__S1731];
      int32_t _M0L6_2atmpS1838;
      int32_t _M0L6_2atmpS1839;
      moonbit_incref(_M0L3msgS1732);
      #line 149 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1838
      = _M0MP38JIA2JIA29moonbitdb3lib8Database5rpush(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_173.data, _M0L3msgS1732);
      moonbit_decref(_M0L3msgS1732);
      _M0L6_2atmpS1839 = _M0L2__S1731 + 1;
      _M0L2__S1731 = _M0L6_2atmpS1839;
      continue;
    } else {
      moonbit_decref(_M0L8messagesS1729);
    }
    break;
  }
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1734
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1734, (moonbit_string_t)moonbit_string_literal_174.data);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1842
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_173.data);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1734, _M0L6_2atmpS1842);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1841
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1734);
  moonbit_decref(_M0L18_2astring__builderS1734);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1841);
  moonbit_decref(_M0L6_2atmpS1841);
  #line 152 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_175.data);
  while (1) {
    int32_t _M0L6_2atmpS1843;
    #line 153 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS1843
    = _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_173.data);
    if (_M0L6_2atmpS1843 > 0) {
      moonbit_string_t _M0L3msgS1735;
      moonbit_string_t _M0L1mS1737;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1738;
      moonbit_string_t _M0L6_2atmpS1844;
      #line 154 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L3msgS1735
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4lpop(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_173.data);
      if (_M0L3msgS1735 == 0) {
        if (_M0L3msgS1735) {
          moonbit_decref(_M0L3msgS1735);
        }
        break;
      } else {
        moonbit_string_t _M0L7_2aSomeS1739 = _M0L3msgS1735;
        moonbit_string_t _M0L4_2amS1740 = _M0L7_2aSomeS1739;
        _M0L1mS1737 = _M0L4_2amS1740;
        goto join_1736;
      }
      goto joinlet_4068;
      join_1736:;
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS1738
      = _M0MPB13StringBuilder21StringBuilder_2einner(15);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1738, (moonbit_string_t)moonbit_string_literal_176.data);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1738, _M0L1mS1737);
      moonbit_decref(_M0L1mS1737);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS1844
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1738);
      moonbit_decref(_M0L18_2astring__builderS1738);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1844);
      moonbit_decref(_M0L6_2atmpS1844);
      joinlet_4068:;
      continue;
    }
    break;
  }
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1742
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1742, (moonbit_string_t)moonbit_string_literal_177.data);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1846
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(_M0L2dbS1629, (moonbit_string_t)moonbit_string_literal_173.data);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1742, _M0L6_2atmpS1846);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1845
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1742);
  moonbit_decref(_M0L18_2astring__builderS1742);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1845);
  moonbit_decref(_M0L6_2atmpS1845);
  #line 162 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_178.data);
  #line 163 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1743
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1743, (moonbit_string_t)moonbit_string_literal_179.data);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1848
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(_M0L2dbS1629);
  moonbit_decref(_M0L2dbS1629);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1743, _M0L6_2atmpS1848);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1847
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1743);
  moonbit_decref(_M0L18_2astring__builderS1743);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1847);
  moonbit_decref(_M0L6_2atmpS1847);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS1744
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1744, (moonbit_string_t)moonbit_string_literal_180.data);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1851 = _M0MP38JIA2JIA29moonbitdb3lib8Database7command();
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1850 = _M0MPC15array5Array6lengthGsE(_M0L6_2atmpS1851);
  moonbit_decref(_M0L6_2atmpS1851);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1744, _M0L6_2atmpS1850);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS1849
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1744);
  moonbit_decref(_M0L18_2astring__builderS1744);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1849);
  moonbit_decref(_M0L6_2atmpS1849);
  #line 167 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_181.data);
  #line 168 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_182.data);
  #line 169 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_97.data);
  return 0;
}