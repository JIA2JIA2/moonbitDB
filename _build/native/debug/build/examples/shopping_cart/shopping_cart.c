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

struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples14shopping__cart7ProductE;

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

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__;

struct _M0TPB6Logger;

struct _M0TP38JIA2JIA29moonbitdb3lib8Database;

struct _M0TUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsfEE;

struct _M0TPB5EntryGsbE;

struct _M0TPB8MutLocalGORPB5EntryGssEE;

struct _M0DTPC16option6OptionGfE4Some;

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque;

struct _M0TPB19MulShiftAll64Result;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set;

struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0TPB5ArrayGOsE;

struct _M0TWEOUsbE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet;

struct _M0TPB5EntryGsfE;

struct _M0TPB8MutLocalGiE;

struct _M0TPB5ArrayGUsfEE;

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0TPB3MapGssE;

struct _M0TPB4Show;

struct _M0TPB8MutLocalGfE;

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__;

struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE;

struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash;

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;

struct _M0BTPB4Show;

struct _M0TPC16string10StringView;

struct _M0TPB8MutLocalGbE;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB8MutLocalGORPB5EntryGsbEE;

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__;

struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__;

struct _M0TPB3MapGsbE;

struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product;

struct _M0TPB5ArrayGsE;

struct _M0TPB3MapGsiE;

struct _M0TPB9ArrayViewGUssEE;

struct _M0TPB9ArrayViewGUsbEE;

struct _M0TPB4IterGUsbEE;

struct _M0TPB4IterGUssEE;

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

struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples14shopping__cart7ProductE {
  struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product** $0;
  int32_t $1;
  
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

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
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

struct _M0TPB8MutLocalGfE {
  float $0;
  
};

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__ {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* $1;
  
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

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
};

struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__ {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*(* code)(
    struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* $1;
  
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

struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product {
  moonbit_string_t $0;
  moonbit_string_t $1;
  float $2;
  int32_t $3;
  
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

moonbit_string_t _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart16show__opt__float(
  void*
);

struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _M0MP48JIA2JIA29moonbitdb8examples14shopping__cart7Product3new(
  moonbit_string_t,
  moonbit_string_t,
  float,
  int32_t
);

float _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart12parse__float(
  moonbit_string_t
);

int32_t _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart10parse__int(
  moonbit_string_t
);

moonbit_string_t _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(
  moonbit_string_t
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database7flushdb(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
);

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database*
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

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque11push__front(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*,
  moonbit_string_t
);

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0MP38JIA2JIA29moonbitdb3lib5Deque3new(
  
);

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float);

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

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(struct _M0TPB3MapGsfE*);

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2863l711(struct _M0TWEOUsfE*);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2853l711(struct _M0TWEOUsbE*);

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2843l711(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TUssE* _M0MPB3Map4iterGssEC2833l711(struct _M0TWEOUssE*);

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6removeGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE*,
  moonbit_string_t,
  int32_t
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

int32_t _M0MPB3Map11shift__backGsbE(struct _M0TPB3MapGsbE*, int32_t);

int32_t _M0MPB3Map11shift__backGsiE(struct _M0TPB3MapGsiE*, int32_t);

int32_t _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map8containsGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

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

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  moonbit_string_t,
  void*
);

int32_t _M0MPB3Map3setGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t, int32_t);

int32_t _M0MPB3Map3setGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t, int32_t);

int32_t _M0MPB3Map3setGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t, float);

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

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

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE*);

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*
);

int32_t _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE*,
  struct _M0TPB5EntryGsfE*
);

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE*,
  int32_t,
  struct _M0TPB5EntryGssE*
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

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*,
  int32_t
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

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE*,
  int32_t,
  struct _M0TPB5EntryGssE*
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

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE*
);

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*
);

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE*
);

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE*
);

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE*
);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

moonbit_string_t _M0IPC16uint646UInt64PB4Show10to__string(uint64_t);

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t);

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

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

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*,
  int64_t
);

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(struct _M0TWEOUsbE*, int64_t);

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

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*
);

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(struct _M0TPB4IterGUsbEE*);

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(struct _M0TPB4IterGUsfEE*);

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float,
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

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder*,
  float
);

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder*,
  int32_t
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

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_39 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    32531, 23384, 21830, 21697, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_32 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 112, 114, 
    111, 100, 117, 99, 116, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_5 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 0, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_85 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    20851, 32852, 29992, 25143, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_52 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 104, 105, 
    115, 116, 111, 114, 121, 58, 117, 115, 101, 114, 58, 49, 48, 48, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_45 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 32, 32, 
    29992, 25143, 32, 117, 115, 101, 114, 58, 49, 48, 48, 49, 32, 30340,
    36141, 29289, 36710, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_61 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 32, 
    32, 32, 45, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_86 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    21097, 20313, 26102, 38388, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_42 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_10 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 114, 97, 
    100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 98, 101, 
    116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_6 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 110, 
    102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_3 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 78, 97, 78, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_88 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    49, 48, 48, 48, 31186, 21518, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_82 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 115, 101, 
    115, 115, 58, 97, 98, 99, 49, 50, 51, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_81 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 10, 9201,
    65039, 32, 38454, 27573, 56, 65306, 20250, 35805, 31649, 29702, 65288,
    83, 116, 114, 105, 110, 103, 32, 43, 32, 36807, 26399, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_43 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_0 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_62 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55357,
    56599, 32, 38454, 27573, 53, 65306, 20849, 21516, 25910, 34255, 65288,
    83, 101, 116, 20132, 38598, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_16 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 44, 32, 
    108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_9 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 102, 97, 
    108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_13 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 98, 111, 
    117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 97, 105, 
    108, 101, 100, 58, 32, 97, 108, 108, 111, 99, 97, 116, 101, 95, 108, 
    101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_100 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_77 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 32, 32, 
    32, 32, 31532, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_66 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55356,
    57335, 65039, 32, 38454, 27573, 54, 65306, 21830, 21697, 26631, 31614,
    65288, 83, 101, 116, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_108 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 41, 10, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_101 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_57 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    21382, 21490, 35760, 24405, 24635, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_72 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    26082, 26159, 39, 30828, 20214, 39, 21448, 26159, 39, 22806, 35774, 
    39, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_34 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 114, 
    105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_56 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_21 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 77, 111, 
    111, 110, 66, 105, 116, 32534, 31243, 25351, 21335, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_71 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 32, 32, 
    39, 30828, 20214, 39, 26631, 31614, 21830, 21697, 25968, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_11 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 
    118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_55 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 40, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_22 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_23 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 39640, 24615,
    33021, 26381, 21153, 22120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_17 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_29 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 26080, 32447,
    40736, 26631, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_105 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 61, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_94 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_19 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 32, 32, 
    30005, 21830, 36141, 29289, 36710, 32, 38, 32, 32531, 23384, 31995,
    32479, 32, 45, 32, 37096, 32626, 31034, 20363, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_14 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_35 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 115, 116, 
    111, 99, 107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_74 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 10, 55357,
    56522, 32, 38454, 27573, 55, 65306, 38144, 37327, 25490, 34892, 27036,
    65288, 83, 111, 114, 116, 101, 100, 32, 83, 101, 116, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_76 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    38144, 37327, 32, 84, 79, 80, 32, 51, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_53 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 32, 32, 
    26368, 36817, 27983, 35272, 30340, 53, 20010, 21830, 21697, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_36 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    32531, 23384, 21830, 21697, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_2 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 73, 108, 
    108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 116, 69, 
    120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_79 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 45, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_70 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 116, 97, 
    103, 58, 26174, 31034, 22120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_40 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55357,
    57042, 32, 38454, 27573, 50, 65306, 29992, 25143, 36141, 29289, 36710,
    65288, 72, 97, 115, 104, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_102 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 32, 32, 
    25209, 37327, 39044, 28909, 32, 53, 32, 20010, 28909, 28857, 107, 
    101, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_87 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 31186, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_75 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 115, 97, 
    108, 101, 115, 58, 114, 97, 110, 107, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_104 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 61, 
    32, 40, 110, 105, 108, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_90 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    50, 48, 48, 48, 31186, 21518, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_107 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    24635, 32, 75, 101, 121, 32, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_106 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 10, 55357,
    56520, 32, 38454, 27573, 49, 48, 65306, 31995, 32479, 29366, 24577,
    24635, 35272, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_89 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    20250, 35805, 26159, 21542, 26377, 25928, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_98 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_15 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    100, 115, 116, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_12 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 105, 110, 
    118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 111, 105, 
    110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_4 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_63 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 102, 97, 
    118, 111, 114, 105, 116, 101, 115, 58, 117, 115, 101, 114, 58, 49, 
    48, 48, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_97 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_95 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_80 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 32, 32, 
    38144, 37327, 22312, 49, 48, 48, 45, 51, 48, 48, 20043, 38388, 30340,
    21830, 21697, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 21517, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_73 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    25152, 26377, 26631, 31614, 21830, 21697, 40, 21435, 37325, 41, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_51 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55357,
    56541, 32, 38454, 27573, 51, 65306, 27983, 35272, 21382, 21490, 65288,
    76, 105, 115, 116, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_68 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 116, 97, 
    103, 58, 30828, 20214, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_26 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_24 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_1 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 85, 110, 
    107, 110, 111, 119, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_20 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_58 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 11088, 
    32, 38454, 27573, 52, 65306, 21830, 21697, 25910, 34255, 65288, 83, 
    101, 116, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_49 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    36141, 29289, 36710, 24635, 20215, 58, 32, 165, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_91 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55357,
    56580, 32, 38454, 27573, 57, 65306, 32531, 23384, 39044, 28909, 32, 
    38, 32, 25209, 37327, 25805, 20316, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 116, 97, 
    103, 58, 32534, 31243, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_31 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_103 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    25209, 37327, 35835, 21462, 32467, 26524, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_48 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 61, 
    32, 165, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_38 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 44, 32, 
    84, 84, 76, 58, 32, 51, 54, 48, 48, 115, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_109 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 32, 32, 
    9989, 32, 36141, 29289, 36710, 32, 38, 32, 32531, 23384, 31995, 32479,
    31034, 20363, 23436, 25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_33 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 97, 
    109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_83 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 117, 115, 
    101, 114, 58, 49, 48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_84 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    20250, 35805, 73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_8 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 114, 
    117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_37 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 40, 
    73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_96 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_69 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 116, 97, 
    103, 58, 22806, 35774, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_50 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    21830, 21697, 31181, 31867, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_44 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_47 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 120, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_54 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 46, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_60 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    25910, 34255, 21830, 21697, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_59 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 102, 97, 
    118, 111, 114, 105, 116, 101, 115, 58, 117, 115, 101, 114, 58, 49, 
    48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_27 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 52, 75, 
    26174, 31034, 22120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_99 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_93 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_41 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 99, 97, 
    114, 116, 58, 117, 115, 101, 114, 58, 49, 48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_30 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 10, 55357,
    56550, 32, 38454, 27573, 49, 65306, 21830, 21697, 20449, 24687, 32531,
    23384, 65288, 72, 97, 115, 104, 32, 43, 32, 36807, 26399, 65289, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_7 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 48, 46, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_92 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_46 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_65 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 20214, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_64 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 32, 32, 
    20004, 20301, 29992, 25143, 20849, 21516, 25910, 34255, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_28 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_25 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 26426, 26800,
    38190, 30424, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0)
  };

uint32_t const moonbit_layout_table_data[138] =
  {
    sizeof(struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product)
    / 4, 2,
    offsetof(struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product, $0)
    / 4,
    offsetof(struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product, $1)
    / 4, sizeof(struct _M0TPB5ArrayGsE) / 4, 1,
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
    sizeof(struct _M0TPB5ArrayGOsE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGOsE, $0) / 4,
    sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String) / 4, 
    1,
    offsetof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String, $0)
    / 4, sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database) / 4, 2,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database, $0) / 4,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database, $1) / 4,
    sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque) / 4, 2,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque, $0) / 4,
    offsetof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque, $1) / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGssEE, $0) / 4,
    sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__)
    / 4, 2,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__, $0)
    / 4,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__)
    / 4, 2,
    offsetof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__, $0)
    / 4,
    offsetof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__, $1)
    / 4, sizeof(struct _M0TUsbE) / 4, 1, offsetof(struct _M0TUsbE, $0) / 4,
    sizeof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE) / 4, 
    2,
    offsetof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $0) / 4,
    offsetof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE, $1) / 4,
    sizeof(struct _M0TUssE) / 4, 2, offsetof(struct _M0TUssE, $0) / 4,
    offsetof(struct _M0TUssE, $1) / 4, sizeof(struct _M0TPB5EntryGssE) / 4,
    3, offsetof(struct _M0TPB5EntryGssE, $1) / 4,
    offsetof(struct _M0TPB5EntryGssE, $4) / 4,
    offsetof(struct _M0TPB5EntryGssE, $5) / 4,
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
    sizeof(struct _M0TPB4IterGUssEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUssEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE) / 4,
    1,
    offsetof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE, $0)
    / 4, sizeof(struct _M0TPB4IterGUsbEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsbEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsfEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsfEE, $0) / 4,
    sizeof(struct _M0TPB13StringBuilder) / 4, 1,
    offsetof(struct _M0TPB13StringBuilder, $0) / 4,
    sizeof(struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples14shopping__cart7ProductE)
    / 4, 1,
    offsetof(struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples14shopping__cart7ProductE, $0)
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

int64_t _M0MPB4Iter3newN6constrS9988GUssEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsbEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsfEE = 0ll;

moonbit_string_t _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart16show__opt__float(
  void* _M0L3optS1671
) {
  float _M0L1vS1669;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1670;
  moonbit_string_t _result_3894;
  #line 238 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  switch (Moonbit_object_tag(_M0L3optS1671)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1672 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1671;
      float _M0L4_2avS1673 = _M0L7_2aSomeS1672->$0;
      _M0L1vS1669 = _M0L4_2avS1673;
      goto join_1668;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_0.data;
      break;
    }
  }
  join_1668:;
  #line 240 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1670
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 240 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1670, _M0L1vS1669);
  #line 240 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _result_3894
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1670);
  moonbit_decref(_M0L18_2astring__builderS1670);
  return _result_3894;
}

struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _M0MP48JIA2JIA29moonbitdb8examples14shopping__cart7Product3new(
  moonbit_string_t _M0L2idS1664,
  moonbit_string_t _M0L4nameS1665,
  float _M0L5priceS1666,
  int32_t _M0L5stockS1667
) {
  struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _block_3895;
  #line 61 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  moonbit_incref(_M0L2idS1664);
  moonbit_incref(_M0L4nameS1665);
  _block_3895
  = (struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product*)moonbit_malloc(sizeof(struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product));
  Moonbit_object_header(_block_3895)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _block_3895->$0 = _M0L2idS1664;
  _block_3895->$1 = _M0L4nameS1665;
  _block_3895->$2 = _M0L5priceS1666;
  _block_3895->$3 = _M0L5stockS1667;
  return _block_3895;
}

float _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart12parse__float(
  moonbit_string_t _M0L1sS1658
) {
  struct _M0TPB8MutLocalGfE* _M0L6resultS1655;
  struct _M0TPB8MutLocalGiE* _M0L1iS1656;
  struct _M0TPB8MutLocalGbE* _M0L3negS1657;
  int32_t _M0L6_2atmpS3454;
  int32_t _if__result_3896;
  struct _M0TPB8MutLocalGbE* _M0L8has__dotS1659;
  struct _M0TPB8MutLocalGfE* _M0L12decimal__posS1660;
  int32_t _result_3900;
  #line 26 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6resultS1655
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L6resultS1655)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6resultS1655->$0 = 0x0p+0f;
  _M0L1iS1656
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1656)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1656->$0 = 0;
  _M0L3negS1657
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L3negS1657)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L3negS1657->$0 = 0;
  _M0L6_2atmpS3454 = Moonbit_array_length(_M0L1sS1658);
  if (_M0L6_2atmpS3454 > 0) {
    int32_t _M0L6_2atmpS3453;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1658)) {
      #line 30 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3453 = _M0L1sS1658[0];
    _if__result_3896 = _M0L6_2atmpS3453 == 45;
  } else {
    _if__result_3896 = 0;
  }
  if (_if__result_3896) {
    _M0L3negS1657->$0 = 1;
    _M0L1iS1656->$0 = 1;
  }
  _M0L8has__dotS1659
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L8has__dotS1659)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L8has__dotS1659->$0 = 0;
  _M0L12decimal__posS1660
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L12decimal__posS1660)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L12decimal__posS1660->$0 = 0x1.4p+3f;
  while (1) {
    int32_t _M0L3valS3455 = _M0L1iS1656->$0;
    int32_t _M0L6_2atmpS3456 = Moonbit_array_length(_M0L1sS1658);
    if (_M0L3valS3455 < _M0L6_2atmpS3456) {
      int32_t _M0L3valS3472 = _M0L1iS1656->$0;
      int32_t _M0L1cS1661;
      int32_t _if__result_3898;
      int32_t _M0L3valS3471;
      int32_t _M0L6_2atmpS3470;
      if (
        _M0L3valS3472 < 0
        || _M0L3valS3472 >= Moonbit_array_length(_M0L1sS1658)
      ) {
        #line 37 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        moonbit_panic();
      }
      _M0L1cS1661 = _M0L1sS1658[_M0L3valS3472];
      if (_M0L1cS1661 >= 48) {
        _if__result_3898 = _M0L1cS1661 <= 57;
      } else {
        _if__result_3898 = 0;
      }
      if (_if__result_3898) {
        int32_t _M0L6_2atmpS3467 = (int32_t)_M0L1cS1661;
        int32_t _M0L6_2atmpS3468 = 48;
        int32_t _M0L6_2atmpS3466 = _M0L6_2atmpS3467 - _M0L6_2atmpS3468;
        float _M0L5digitS1662 = (float)_M0L6_2atmpS3466;
        if (_M0L8has__dotS1659->$0) {
          float _M0L3valS3458 = _M0L6resultS1655->$0;
          float _M0L3valS3460 = _M0L12decimal__posS1660->$0;
          float _M0L6_2atmpS3459 = _M0L5digitS1662 / _M0L3valS3460;
          float _M0L6_2atmpS3457 = _M0L3valS3458 + _M0L6_2atmpS3459;
          float _M0L3valS3462;
          float _M0L6_2atmpS3461;
          _M0L6resultS1655->$0 = _M0L6_2atmpS3457;
          _M0L3valS3462 = _M0L12decimal__posS1660->$0;
          _M0L6_2atmpS3461 = _M0L3valS3462 * 0x1.4p+3f;
          _M0L12decimal__posS1660->$0 = _M0L6_2atmpS3461;
        } else {
          float _M0L3valS3465 = _M0L6resultS1655->$0;
          float _M0L6_2atmpS3464 = _M0L3valS3465 * 0x1.4p+3f;
          float _M0L6_2atmpS3463 = _M0L6_2atmpS3464 + _M0L5digitS1662;
          _M0L6resultS1655->$0 = _M0L6_2atmpS3463;
        }
      } else {
        int32_t _if__result_3899;
        if (_M0L1cS1661 == 46) {
          int32_t _M0L3valS3469 = _M0L8has__dotS1659->$0;
          _if__result_3899 = !_M0L3valS3469;
        } else {
          _if__result_3899 = 0;
        }
        if (_if__result_3899) {
          _M0L8has__dotS1659->$0 = 1;
        }
      }
      _M0L3valS3471 = _M0L1iS1656->$0;
      _M0L6_2atmpS3470 = _M0L3valS3471 + 1;
      _M0L1iS1656->$0 = _M0L6_2atmpS3470;
      continue;
    } else {
      moonbit_decref(_M0L12decimal__posS1660);
      moonbit_decref(_M0L8has__dotS1659);
      moonbit_decref(_M0L1iS1656);
    }
    break;
  }
  _result_3900 = _M0L3negS1657->$0;
  moonbit_decref(_M0L3negS1657);
  if (_result_3900) {
    float _M0L3valS3473 = _M0L6resultS1655->$0;
    moonbit_decref(_M0L6resultS1655);
    return -_M0L3valS3473;
  } else {
    float _result_3901 = _M0L6resultS1655->$0;
    moonbit_decref(_M0L6resultS1655);
    return _result_3901;
  }
}

int32_t _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart10parse__int(
  moonbit_string_t _M0L1sS1652
) {
  struct _M0TPB8MutLocalGiE* _M0L6resultS1649;
  struct _M0TPB8MutLocalGiE* _M0L1iS1650;
  struct _M0TPB8MutLocalGbE* _M0L3negS1651;
  int32_t _M0L6_2atmpS3440;
  int32_t _if__result_3902;
  int32_t _result_3905;
  #line 8 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6resultS1649
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L6resultS1649)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6resultS1649->$0 = 0;
  _M0L1iS1650
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1650)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1650->$0 = 0;
  _M0L3negS1651
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L3negS1651)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L3negS1651->$0 = 0;
  _M0L6_2atmpS3440 = Moonbit_array_length(_M0L1sS1652);
  if (_M0L6_2atmpS3440 > 0) {
    int32_t _M0L6_2atmpS3439;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1652)) {
      #line 12 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3439 = _M0L1sS1652[0];
    _if__result_3902 = _M0L6_2atmpS3439 == 45;
  } else {
    _if__result_3902 = 0;
  }
  if (_if__result_3902) {
    _M0L3negS1651->$0 = 1;
    _M0L1iS1650->$0 = 1;
  }
  while (1) {
    int32_t _M0L3valS3441 = _M0L1iS1650->$0;
    int32_t _M0L6_2atmpS3442 = Moonbit_array_length(_M0L1sS1652);
    if (_M0L3valS3441 < _M0L6_2atmpS3442) {
      int32_t _M0L3valS3451 = _M0L1iS1650->$0;
      int32_t _M0L1cS1653;
      int32_t _if__result_3904;
      int32_t _M0L3valS3450;
      int32_t _M0L6_2atmpS3449;
      if (
        _M0L3valS3451 < 0
        || _M0L3valS3451 >= Moonbit_array_length(_M0L1sS1652)
      ) {
        #line 17 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        moonbit_panic();
      }
      _M0L1cS1653 = _M0L1sS1652[_M0L3valS3451];
      if (_M0L1cS1653 >= 48) {
        _if__result_3904 = _M0L1cS1653 <= 57;
      } else {
        _if__result_3904 = 0;
      }
      if (_if__result_3904) {
        int32_t _M0L3valS3448 = _M0L6resultS1649->$0;
        int32_t _M0L6_2atmpS3444 = _M0L3valS3448 * 10;
        int32_t _M0L6_2atmpS3446 = (int32_t)_M0L1cS1653;
        int32_t _M0L6_2atmpS3447 = 48;
        int32_t _M0L6_2atmpS3445 = _M0L6_2atmpS3446 - _M0L6_2atmpS3447;
        int32_t _M0L6_2atmpS3443 = _M0L6_2atmpS3444 + _M0L6_2atmpS3445;
        _M0L6resultS1649->$0 = _M0L6_2atmpS3443;
      }
      _M0L3valS3450 = _M0L1iS1650->$0;
      _M0L6_2atmpS3449 = _M0L3valS3450 + 1;
      _M0L1iS1650->$0 = _M0L6_2atmpS3449;
      continue;
    } else {
      moonbit_decref(_M0L1iS1650);
    }
    break;
  }
  _result_3905 = _M0L3negS1651->$0;
  moonbit_decref(_M0L3negS1651);
  if (_result_3905) {
    int32_t _M0L3valS3452 = _M0L6resultS1649->$0;
    moonbit_decref(_M0L6resultS1649);
    return -_M0L3valS3452;
  } else {
    int32_t _result_3906 = _M0L6resultS1649->$0;
    moonbit_decref(_M0L6resultS1649);
    return _result_3906;
  }
}

moonbit_string_t _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(
  moonbit_string_t _M0L3optS1646
) {
  moonbit_string_t _M0L1vS1645;
  #line 1 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  if (_M0L3optS1646 == 0) {
    return (moonbit_string_t)moonbit_string_literal_1.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS1647 = _M0L3optS1646;
    moonbit_string_t _M0L4_2avS1648 = _M0L7_2aSomeS1647;
    moonbit_incref(_M0L4_2avS1648);
    _M0L1vS1645 = _M0L4_2avS1648;
    goto join_1644;
  }
  join_1644:;
  return _M0L1vS1645;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database7flushdb(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1641
) {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS1642;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS3435;
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L6_2atmpS3434;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3433;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3475;
  struct _M0TUsiE** _M0L7_2abindS1643;
  struct _M0TUsiE** _M0L6_2atmpS3438;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3437;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3436;
  struct _M0TPB3MapGsiE* _M0L6_2aoldS3474;
  #line 1494 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1642
  = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3435 = _M0L7_2abindS1642;
  _M0L6_2atmpS3434
  = (struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE){
    .$0 = _M0L6_2atmpS3435, .$1 = 0, .$2 = 0
  };
  #line 1495 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3433
  = _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L6_2atmpS3434, 1000ll);
  moonbit_decref(_M0L6_2atmpS3434.$0);
  _M0L6_2aoldS3475 = _M0L4selfS1641->$0;
  moonbit_decref(_M0L6_2aoldS3475);
  _M0L4selfS1641->$0 = _M0L6_2atmpS3433;
  _M0L7_2abindS1643 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3438 = _M0L7_2abindS1643;
  _M0L6_2atmpS3437
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3438, .$1 = 0, .$2 = 0
  };
  #line 1496 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3436 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3437, 1000ll);
  moonbit_decref(_M0L6_2atmpS3437.$0);
  _M0L6_2aoldS3474 = _M0L4selfS1641->$1;
  moonbit_decref(_M0L6_2aoldS3474);
  _M0L4selfS1641->$1 = _M0L6_2atmpS3436;
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1633
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1631;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3432;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L5_2aitS1632;
  int32_t _result_3910;
  #line 1484 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5countS1631
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1631)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1631->$0 = 0;
  _M0L4dataS3432 = _M0L4selfS1633->$0;
  moonbit_incref(_M0L4dataS3432);
  #line 1485 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1632
  = _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3432);
  moonbit_decref(_M0L4dataS3432);
  while (1) {
    moonbit_string_t _M0L3keyS1635;
    struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS1637;
    int32_t _M0L6_2atmpS3429;
    #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1637
    = _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L5_2aitS1632);
    if (_M0L7_2abindS1637 == 0) {
      if (_M0L7_2abindS1637) {
        moonbit_decref(_M0L7_2abindS1637);
      }
      moonbit_decref(_M0L5_2aitS1632);
    } else {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1638 =
        _M0L7_2abindS1637;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS1639 =
        _M0L7_2aSomeS1638;
      moonbit_string_t _M0L8_2afieldS3476 = _M0L4_2axS1639->$0;
      int32_t _M0L6_2acntS3833 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1639));
      moonbit_string_t _M0L6_2akeyS1640;
      if (_M0L6_2acntS3833 > 1) {
        int32_t _M0L11_2anew__cntS3835 = _M0L6_2acntS3833 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1639), _M0L11_2anew__cntS3835);
        moonbit_incref(_M0L8_2afieldS3476);
      } else if (_M0L6_2acntS3833 == 1) {
        void* _M0L8_2afieldS3834 = _M0L4_2axS1639->$1;
        moonbit_decref(_M0L8_2afieldS3834);
        #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1639);
      }
      _M0L6_2akeyS1640 = _M0L8_2afieldS3476;
      _M0L3keyS1635 = _M0L6_2akeyS1640;
      goto join_1634;
    }
    goto joinlet_3909;
    join_1634:;
    #line 1487 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3429
    = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1633, _M0L3keyS1635);
    moonbit_decref(_M0L3keyS1635);
    if (!_M0L6_2atmpS3429) {
      int32_t _M0L3valS3431 = _M0L5countS1631->$0;
      int32_t _M0L6_2atmpS3430 = _M0L3valS3431 + 1;
      _M0L5countS1631->$0 = _M0L6_2atmpS3430;
    }
    continue;
    joinlet_3909:;
    break;
  }
  _result_3910 = _M0L5countS1631->$0;
  moonbit_decref(_M0L5countS1631);
  return _result_3910;
}

int64_t _M0MP38JIA2JIA29moonbitdb3lib8Database8zrevrank(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1615,
  moonbit_string_t _M0L3keyS1616,
  moonbit_string_t _M0L11member__valS1620
) {
  #line 1353 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1354 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1615, _M0L3keyS1616)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1619;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3428 =
      _M0L4selfS1615->$0;
    void* _M0L7_2abindS1626;
    int32_t _M0L6_2atmpS3421;
    moonbit_incref(_M0L4dataS3428);
    #line 1357 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1626
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3428, _M0L3keyS1616);
    moonbit_decref(_M0L4dataS3428);
    if (_M0L7_2abindS1626 == 0) {
      if (_M0L7_2abindS1626) {
        moonbit_decref(_M0L7_2abindS1626);
      }
      goto join_1617;
    } else {
      void* _M0L7_2aSomeS1627 = _M0L7_2abindS1626;
      void* _M0L4_2axS1628 = _M0L7_2aSomeS1627;
      switch (Moonbit_object_tag(_M0L4_2axS1628)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1629 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1628;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3479 = _M0L7_2aZSetS1629->$0;
          int32_t _M0L6_2acntS3838 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1629));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1630;
          if (_M0L6_2acntS3838 > 1) {
            int32_t _M0L11_2anew__cntS3839 = _M0L6_2acntS3838 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1629), _M0L11_2anew__cntS3839);
            moonbit_incref(_M0L8_2afieldS3479);
          } else if (_M0L6_2acntS3838 == 1) {
            #line 1357 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1629);
          }
          _M0L7_2azsetS1630 = _M0L8_2afieldS3479;
          _M0L4zsetS1619 = _M0L7_2azsetS1630;
          goto join_1618;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1628);
          goto join_1617;
          break;
        }
      }
    }
    join_1618:;
    #line 1359 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3421
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1619, _M0L11member__valS1620);
    moonbit_decref(_M0L4zsetS1619);
    if (!_M0L6_2atmpS3421) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1621;
      int32_t _M0L3lenS1622;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1623;
      int32_t _M0L1iS1624;
      int32_t _M0L3valS3427;
      #line 1362 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6sortedS1621
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1615, _M0L3keyS1616);
      #line 1363 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L3lenS1622 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1621);
      _M0L4rankS1623
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1623)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1623->$0 = 0;
      _M0L1iS1624 = 0;
      while (1) {
        if (_M0L1iS1624 < _M0L3lenS1622) {
          struct _M0TUsfE* _M0L6_2atmpS3423;
          moonbit_string_t _M0L8_2afieldS3478;
          int32_t _M0L6_2acntS3836;
          moonbit_string_t _M0L6_2atmpS3422;
          int32_t _result_3914;
          int32_t _M0L6_2atmpS3426;
          #line 1366 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0L6_2atmpS3423
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1621, _M0L1iS1624);
          _M0L8_2afieldS3478 = _M0L6_2atmpS3423->$0;
          _M0L6_2acntS3836
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3423));
          if (_M0L6_2acntS3836 > 1) {
            int32_t _M0L11_2anew__cntS3837 = _M0L6_2acntS3836 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3423), _M0L11_2anew__cntS3837);
            moonbit_incref(_M0L8_2afieldS3478);
          } else if (_M0L6_2acntS3836 == 1) {
            #line 1366 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2atmpS3423);
          }
          _M0L6_2atmpS3422 = _M0L8_2afieldS3478;
          #line 1366 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _result_3914
          = _M0L6_2atmpS3422 == _M0L11member__valS1620
            || Moonbit_array_length(_M0L6_2atmpS3422)
               == Moonbit_array_length(_M0L11member__valS1620)
               && 0
                  == memcmp(_M0L6_2atmpS3422, _M0L11member__valS1620, Moonbit_array_length(_M0L6_2atmpS3422) * 2);
          moonbit_decref(_M0L6_2atmpS3422);
          if (_result_3914) {
            int32_t _M0L6_2atmpS3425;
            int32_t _M0L6_2atmpS3424;
            moonbit_decref(_M0L6sortedS1621);
            _M0L6_2atmpS3425 = _M0L3lenS1622 - 1;
            _M0L6_2atmpS3424 = _M0L6_2atmpS3425 - _M0L1iS1624;
            _M0L4rankS1623->$0 = _M0L6_2atmpS3424;
            break;
          }
          _M0L6_2atmpS3426 = _M0L1iS1624 + 1;
          _M0L1iS1624 = _M0L6_2atmpS3426;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1621);
        }
        break;
      }
      _M0L3valS3427 = _M0L4rankS1623->$0;
      moonbit_decref(_M0L4rankS1623);
      return (int64_t)_M0L3valS3427;
    }
    join_1617:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database9zrevrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1604,
  moonbit_string_t _M0L3keyS1605,
  int32_t _M0L5startS1609,
  int32_t _M0L3endS1611
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1603;
  int32_t _M0L3lenS1606;
  moonbit_string_t* _M0L6_2atmpS3420;
  struct _M0TPB5ArrayGsE* _M0L6resultS1607;
  int32_t _M0L10start__idxS1608;
  int32_t _M0L8end__idxS1610;
  int32_t _M0L1iS1612;
  #line 1302 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1303 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6sortedS1603
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1604, _M0L3keyS1605);
  #line 1304 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1606 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1603);
  _M0L6_2atmpS3420 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1607
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1607)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1607->$0 = _M0L6_2atmpS3420;
  _M0L6resultS1607->$1 = 0;
  if (_M0L5startS1609 < 0) {
    _M0L10start__idxS1608 = _M0L3lenS1606 + _M0L5startS1609;
  } else {
    _M0L10start__idxS1608 = _M0L5startS1609;
  }
  if (_M0L3endS1611 < 0) {
    _M0L8end__idxS1610 = _M0L3lenS1606 + _M0L3endS1611;
  } else {
    _M0L8end__idxS1610 = _M0L3endS1611;
  }
  _M0L1iS1612 = _M0L10start__idxS1608;
  while (1) {
    int32_t _if__result_3916;
    if (_M0L1iS1612 <= _M0L8end__idxS1610) {
      _if__result_3916 = _M0L1iS1612 < _M0L3lenS1606;
    } else {
      _if__result_3916 = 0;
    }
    if (_if__result_3916) {
      int32_t _M0L6_2atmpS3418 = _M0L3lenS1606 - 1;
      int32_t _M0L8rev__idxS1613 = _M0L6_2atmpS3418 - _M0L1iS1612;
      int32_t _M0L6_2atmpS3419;
      if (_M0L8rev__idxS1613 >= 0) {
        struct _M0TUsfE* _M0L6_2atmpS3417;
        moonbit_string_t _M0L8_2afieldS3481;
        int32_t _M0L6_2acntS3840;
        moonbit_string_t _M0L6_2atmpS3416;
        #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3417
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1603, _M0L8rev__idxS1613);
        _M0L8_2afieldS3481 = _M0L6_2atmpS3417->$0;
        _M0L6_2acntS3840
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3417));
        if (_M0L6_2acntS3840 > 1) {
          int32_t _M0L11_2anew__cntS3841 = _M0L6_2acntS3840 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3417), _M0L11_2anew__cntS3841);
          moonbit_incref(_M0L8_2afieldS3481);
        } else if (_M0L6_2acntS3840 == 1) {
          #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2atmpS3417);
        }
        _M0L6_2atmpS3416 = _M0L8_2afieldS3481;
        #line 1311 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1607, _M0L6_2atmpS3416);
        moonbit_decref(_M0L6_2atmpS3416);
      }
      _M0L6_2atmpS3419 = _M0L1iS1612 + 1;
      _M0L1iS1612 = _M0L6_2atmpS3419;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1603);
    }
    break;
  }
  return _M0L6resultS1607;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database13zrangebyscore(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1594,
  moonbit_string_t _M0L3keyS1595,
  float _M0L10min__scoreS1600,
  float _M0L10max__scoreS1601
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1593;
  moonbit_string_t* _M0L6_2atmpS3415;
  struct _M0TPB5ArrayGsE* _M0L6resultS1596;
  int32_t _M0L7_2abindS1597;
  int32_t _M0L2__S1598;
  #line 1280 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1281 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6sortedS1593
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1594, _M0L3keyS1595);
  _M0L6_2atmpS3415 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1596
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1596)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1596->$0 = _M0L6_2atmpS3415;
  _M0L6resultS1596->$1 = 0;
  _M0L7_2abindS1597 = _M0L6sortedS1593->$1;
  _M0L2__S1598 = 0;
  while (1) {
    if (_M0L2__S1598 < _M0L7_2abindS1597) {
      struct _M0TUsfE** _M0L3bufS3414 = _M0L6sortedS1593->$0;
      struct _M0TUsfE* _M0L4itemS1599 =
        (struct _M0TUsfE*)_M0L3bufS3414[_M0L2__S1598];
      float _M0L6_2atmpS3411 = _M0L4itemS1599->$1;
      int32_t _if__result_3918;
      int32_t _M0L6_2atmpS3413;
      if (_M0L6_2atmpS3411 >= _M0L10min__scoreS1600) {
        float _M0L6_2atmpS3410 = _M0L4itemS1599->$1;
        _if__result_3918 = _M0L6_2atmpS3410 <= _M0L10max__scoreS1601;
      } else {
        _if__result_3918 = 0;
      }
      if (_if__result_3918) {
        moonbit_string_t _M0L6_2atmpS3412 = _M0L4itemS1599->$0;
        moonbit_incref(_M0L6_2atmpS3412);
        #line 1285 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1596, _M0L6_2atmpS3412);
        moonbit_decref(_M0L6_2atmpS3412);
      }
      _M0L6_2atmpS3413 = _M0L2__S1598 + 1;
      _M0L2__S1598 = _M0L6_2atmpS3413;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1593);
    }
    break;
  }
  return _M0L6resultS1596;
}

struct _M0TPB5ArrayGUsfEE* _M0MP38JIA2JIA29moonbitdb3lib8Database17get__sorted__zset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1572,
  moonbit_string_t _M0L3keyS1573
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1572, _M0L3keyS1573)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS3405 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_3919 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_3919)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _block_3919->$0 = _M0L6_2atmpS3405;
    _block_3919->$1 = 0;
    return _block_3919;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1576;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3409 =
      _M0L4selfS1572->$0;
    void* _M0L7_2abindS1588;
    struct _M0TUsfE** _M0L6_2atmpS3408;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1577;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1578;
    struct _M0TPB5ArrayGUsfEE* _result_3924;
    struct _M0TUsfE** _M0L6_2atmpS3406;
    struct _M0TPB5ArrayGUsfEE* _block_3925;
    moonbit_incref(_M0L4dataS3409);
    #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1588
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3409, _M0L3keyS1573);
    moonbit_decref(_M0L4dataS3409);
    if (_M0L7_2abindS1588 == 0) {
      if (_M0L7_2abindS1588) {
        moonbit_decref(_M0L7_2abindS1588);
      }
      goto join_1574;
    } else {
      void* _M0L7_2aSomeS1589 = _M0L7_2abindS1588;
      void* _M0L4_2axS1590 = _M0L7_2aSomeS1589;
      switch (Moonbit_object_tag(_M0L4_2axS1590)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1591 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1590;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3486 = _M0L7_2aZSetS1591->$0;
          int32_t _M0L6_2acntS3844 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1591));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1592;
          if (_M0L6_2acntS3844 > 1) {
            int32_t _M0L11_2anew__cntS3845 = _M0L6_2acntS3844 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1591), _M0L11_2anew__cntS3845);
            moonbit_incref(_M0L8_2afieldS3486);
          } else if (_M0L6_2acntS3844 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1591);
          }
          _M0L7_2azsetS1592 = _M0L8_2afieldS3486;
          _M0L4zsetS1576 = _M0L7_2azsetS1592;
          goto join_1575;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1590);
          goto join_1574;
          break;
        }
      }
    }
    join_1575:;
    _M0L6_2atmpS3408 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1577
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1577)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _M0L5itemsS1577->$0 = _M0L6_2atmpS3408;
    _M0L5itemsS1577->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1578 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1576);
    moonbit_decref(_M0L4zsetS1576);
    while (1) {
      moonbit_string_t _M0L1mS1580;
      float _M0L1sS1581;
      struct _M0TUsfE* _M0L7_2abindS1583;
      struct _M0TUsfE* _M0L8_2atupleS3407;
      #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1583 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1578);
      if (_M0L7_2abindS1583 == 0) {
        if (_M0L7_2abindS1583) {
          moonbit_decref(_M0L7_2abindS1583);
        }
        moonbit_decref(_M0L5_2aitS1578);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1584 = _M0L7_2abindS1583;
        struct _M0TUsfE* _M0L4_2axS1585 = _M0L7_2aSomeS1584;
        moonbit_string_t _M0L4_2amS1586 = _M0L4_2axS1585->$0;
        float _M0L4_2asS1587 = _M0L4_2axS1585->$1;
        int32_t _M0L6_2acntS3842 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1585));
        if (_M0L6_2acntS3842 > 1) {
          int32_t _M0L11_2anew__cntS3843 = _M0L6_2acntS3842 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1585), _M0L11_2anew__cntS3843);
          moonbit_incref(_M0L4_2amS1586);
        } else if (_M0L6_2acntS3842 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1585);
        }
        _M0L1mS1580 = _M0L4_2amS1586;
        _M0L1sS1581 = _M0L4_2asS1587;
        goto join_1579;
      }
      goto joinlet_3923;
      join_1579:;
      _M0L8_2atupleS3407
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3407)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
      _M0L8_2atupleS3407->$0 = _M0L1mS1580;
      _M0L8_2atupleS3407->$1 = _M0L1sS1581;
      #line 1271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1577, _M0L8_2atupleS3407);
      moonbit_decref(_M0L8_2atupleS3407);
      continue;
      joinlet_3923:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3924
    = _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(_M0L5itemsS1577);
    moonbit_decref(_M0L5itemsS1577);
    return _result_3924;
    join_1574:;
    _M0L6_2atmpS3406 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_3925
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_3925)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _block_3925->$0 = _M0L6_2atmpS3406;
    _block_3925->$1 = 0;
    return _block_3925;
  }
}

void* _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1561,
  moonbit_string_t _M0L3keyS1562,
  moonbit_string_t _M0L11member__valS1566
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1561, _M0L3keyS1562)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1565;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3404 =
      _M0L4selfS1561->$0;
    void* _M0L7_2abindS1567;
    void* _result_3928;
    moonbit_incref(_M0L4dataS3404);
    #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1567
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3404, _M0L3keyS1562);
    moonbit_decref(_M0L4dataS3404);
    if (_M0L7_2abindS1567 == 0) {
      if (_M0L7_2abindS1567) {
        moonbit_decref(_M0L7_2abindS1567);
      }
      goto join_1563;
    } else {
      void* _M0L7_2aSomeS1568 = _M0L7_2abindS1567;
      void* _M0L4_2axS1569 = _M0L7_2aSomeS1568;
      switch (Moonbit_object_tag(_M0L4_2axS1569)) {
        case 4: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1570 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1569;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3488 = _M0L7_2aZSetS1570->$0;
          int32_t _M0L6_2acntS3846 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1570));
          struct _M0TPB3MapGsfE* _M0L4_2azS1571;
          if (_M0L6_2acntS3846 > 1) {
            int32_t _M0L11_2anew__cntS3847 = _M0L6_2acntS3846 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1570), _M0L11_2anew__cntS3847);
            moonbit_incref(_M0L8_2afieldS3488);
          } else if (_M0L6_2acntS3846 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1570);
          }
          _M0L4_2azS1571 = _M0L8_2afieldS3488;
          _M0L1zS1565 = _M0L4_2azS1571;
          goto join_1564;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1569);
          goto join_1563;
          break;
        }
      }
    }
    join_1564:;
    #line 1239 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3928 = _M0MPB3Map3getGsfE(_M0L1zS1565, _M0L11member__valS1566);
    moonbit_decref(_M0L1zS1565);
    return _result_3928;
    join_1563:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1560
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  return _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L5itemsS1560);
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1552
) {
  int32_t _M0L3lenS1551;
  #line 1181 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1551 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1552);
  if (_M0L3lenS1551 <= 1) {
    moonbit_incref(_M0L3arrS1552);
    return _M0L3arrS1552;
  } else {
    int32_t _M0L3midS1553 = _M0L3lenS1551 / 2;
    struct _M0TUsfE** _M0L6_2atmpS3403 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1554 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS3402;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1555;
    int32_t _M0L1iS1556;
    int32_t _M0L1iS1558;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3400;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3401;
    struct _M0TPB5ArrayGUsfEE* _result_3931;
    Moonbit_object_header(_M0L4leftS1554)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _M0L4leftS1554->$0 = _M0L6_2atmpS3403;
    _M0L4leftS1554->$1 = 0;
    _M0L6_2atmpS3402 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1555
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1555)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _M0L5rightS1555->$0 = _M0L6_2atmpS3402;
    _M0L5rightS1555->$1 = 0;
    _M0L1iS1556 = 0;
    while (1) {
      if (_M0L1iS1556 < _M0L3midS1553) {
        struct _M0TUsfE* _M0L6_2atmpS3396;
        int32_t _M0L6_2atmpS3397;
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3396
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1552, _M0L1iS1556);
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1554, _M0L6_2atmpS3396);
        moonbit_decref(_M0L6_2atmpS3396);
        _M0L6_2atmpS3397 = _M0L1iS1556 + 1;
        _M0L1iS1556 = _M0L6_2atmpS3397;
        continue;
      }
      break;
    }
    _M0L1iS1558 = _M0L3midS1553;
    while (1) {
      if (_M0L1iS1558 < _M0L3lenS1551) {
        struct _M0TUsfE* _M0L6_2atmpS3398;
        int32_t _M0L6_2atmpS3399;
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3398
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1552, _M0L1iS1558);
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1555, _M0L6_2atmpS3398);
        moonbit_decref(_M0L6_2atmpS3398);
        _M0L6_2atmpS3399 = _M0L1iS1558 + 1;
        _M0L1iS1558 = _M0L6_2atmpS3399;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3400
    = _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L4leftS1554);
    moonbit_decref(_M0L4leftS1554);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3401
    = _M0FP38JIA2JIA29moonbitdb3lib11merge__sort(_M0L5rightS1555);
    moonbit_decref(_M0L5rightS1555);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3931
    = _M0FP38JIA2JIA29moonbitdb3lib5merge(_M0L6_2atmpS3400, _M0L6_2atmpS3401);
    moonbit_decref(_M0L6_2atmpS3400);
    moonbit_decref(_M0L6_2atmpS3401);
    return _result_3931;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP38JIA2JIA29moonbitdb3lib5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1546,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1547
) {
  struct _M0TUsfE** _M0L6_2atmpS3395;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1543;
  struct _M0TPB8MutLocalGiE* _M0L1iS1544;
  struct _M0TPB8MutLocalGiE* _M0L1jS1545;
  #line 1199 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3395 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1543
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1543)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
  _M0L6resultS1543->$0 = _M0L6_2atmpS3395;
  _M0L6resultS1543->$1 = 0;
  _M0L1iS1544
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1544)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1544->$0 = 0;
  _M0L1jS1545
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1545)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1545->$0 = 0;
  while (1) {
    int32_t _M0L3valS3367 = _M0L1iS1544->$0;
    int32_t _M0L6_2atmpS3368;
    int32_t _if__result_3933;
    #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3368 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1546);
    if (_M0L3valS3367 < _M0L6_2atmpS3368) {
      int32_t _M0L3valS3365 = _M0L1jS1545->$0;
      int32_t _M0L6_2atmpS3366;
      #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3366 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1547);
      _if__result_3933 = _M0L3valS3365 < _M0L6_2atmpS3366;
    } else {
      _if__result_3933 = 0;
    }
    if (_if__result_3933) {
      int32_t _M0L3valS3374 = _M0L1iS1544->$0;
      struct _M0TUsfE* _M0L6_2atmpS3373;
      float _M0L6_2atmpS3369;
      int32_t _M0L3valS3372;
      struct _M0TUsfE* _M0L6_2atmpS3371;
      float _M0L6_2atmpS3370;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3373
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1546, _M0L3valS3374);
      _M0L6_2atmpS3369 = _M0L6_2atmpS3373->$1;
      moonbit_decref(_M0L6_2atmpS3373);
      _M0L3valS3372 = _M0L1jS1545->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3371
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1547, _M0L3valS3372);
      _M0L6_2atmpS3370 = _M0L6_2atmpS3371->$1;
      moonbit_decref(_M0L6_2atmpS3371);
      if (_M0L6_2atmpS3369 <= _M0L6_2atmpS3370) {
        int32_t _M0L3valS3376 = _M0L1iS1544->$0;
        struct _M0TUsfE* _M0L6_2atmpS3375;
        int32_t _M0L3valS3378;
        int32_t _M0L6_2atmpS3377;
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3375
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1546, _M0L3valS3376);
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1543, _M0L6_2atmpS3375);
        moonbit_decref(_M0L6_2atmpS3375);
        _M0L3valS3378 = _M0L1iS1544->$0;
        _M0L6_2atmpS3377 = _M0L3valS3378 + 1;
        _M0L1iS1544->$0 = _M0L6_2atmpS3377;
      } else {
        int32_t _M0L3valS3380 = _M0L1jS1545->$0;
        struct _M0TUsfE* _M0L6_2atmpS3379;
        int32_t _M0L3valS3382;
        int32_t _M0L6_2atmpS3381;
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3379
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1547, _M0L3valS3380);
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1543, _M0L6_2atmpS3379);
        moonbit_decref(_M0L6_2atmpS3379);
        _M0L3valS3382 = _M0L1jS1545->$0;
        _M0L6_2atmpS3381 = _M0L3valS3382 + 1;
        _M0L1jS1545->$0 = _M0L6_2atmpS3381;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3383 = _M0L1iS1544->$0;
    int32_t _M0L6_2atmpS3384;
    #line 1212 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3384 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1546);
    if (_M0L3valS3383 < _M0L6_2atmpS3384) {
      int32_t _M0L3valS3386 = _M0L1iS1544->$0;
      struct _M0TUsfE* _M0L6_2atmpS3385;
      int32_t _M0L3valS3388;
      int32_t _M0L6_2atmpS3387;
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3385
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1546, _M0L3valS3386);
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1543, _M0L6_2atmpS3385);
      moonbit_decref(_M0L6_2atmpS3385);
      _M0L3valS3388 = _M0L1iS1544->$0;
      _M0L6_2atmpS3387 = _M0L3valS3388 + 1;
      _M0L1iS1544->$0 = _M0L6_2atmpS3387;
      continue;
    } else {
      moonbit_decref(_M0L1iS1544);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3389 = _M0L1jS1545->$0;
    int32_t _M0L6_2atmpS3390;
    #line 1216 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3390 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1547);
    if (_M0L3valS3389 < _M0L6_2atmpS3390) {
      int32_t _M0L3valS3392 = _M0L1jS1545->$0;
      struct _M0TUsfE* _M0L6_2atmpS3391;
      int32_t _M0L3valS3394;
      int32_t _M0L6_2atmpS3393;
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3391
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1547, _M0L3valS3392);
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1543, _M0L6_2atmpS3391);
      moonbit_decref(_M0L6_2atmpS3391);
      _M0L3valS3394 = _M0L1jS1545->$0;
      _M0L6_2atmpS3393 = _M0L3valS3394 + 1;
      _M0L1jS1545->$0 = _M0L6_2atmpS3393;
      continue;
    } else {
      moonbit_decref(_M0L1jS1545);
    }
    break;
  }
  return _M0L6resultS1543;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1528,
  moonbit_string_t _M0L3keyS1529,
  float _M0L5scoreS1542,
  moonbit_string_t _M0L11member__valS1541
) {
  int32_t _M0L6_2atmpS3359;
  struct _M0TPB3MapGsfE* _M0L4zsetS1530;
  struct _M0TPB3MapGsfE* _M0L1zS1534;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3364;
  void* _M0L7_2abindS1535;
  struct _M0TUsfE** _M0L7_2abindS1532;
  struct _M0TUsfE** _M0L6_2atmpS3363;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3362;
  int32_t _M0L7existedS1540;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3360;
  void* _M0L4ZSetS3361;
  #line 1140 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3359
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1528, _M0L3keyS1529);
  _M0L4dataS3364 = _M0L4selfS1528->$0;
  moonbit_incref(_M0L4dataS3364);
  #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1535
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3364, _M0L3keyS1529);
  moonbit_decref(_M0L4dataS3364);
  if (_M0L7_2abindS1535 == 0) {
    if (_M0L7_2abindS1535) {
      moonbit_decref(_M0L7_2abindS1535);
    }
    goto join_1531;
  } else {
    void* _M0L7_2aSomeS1536 = _M0L7_2abindS1535;
    void* _M0L4_2axS1537 = _M0L7_2aSomeS1536;
    switch (Moonbit_object_tag(_M0L4_2axS1537)) {
      case 4: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1538 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1537;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3491 = _M0L7_2aZSetS1538->$0;
        int32_t _M0L6_2acntS3848 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1538));
        struct _M0TPB3MapGsfE* _M0L4_2azS1539;
        if (_M0L6_2acntS3848 > 1) {
          int32_t _M0L11_2anew__cntS3849 = _M0L6_2acntS3848 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1538), _M0L11_2anew__cntS3849);
          moonbit_incref(_M0L8_2afieldS3491);
        } else if (_M0L6_2acntS3848 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aZSetS1538);
        }
        _M0L4_2azS1539 = _M0L8_2afieldS3491;
        _M0L1zS1534 = _M0L4_2azS1539;
        goto join_1533;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1537);
        goto join_1531;
        break;
      }
    }
  }
  goto joinlet_3937;
  join_1533:;
  _M0L4zsetS1530 = _M0L1zS1534;
  joinlet_3937:;
  goto joinlet_3936;
  join_1531:;
  _M0L7_2abindS1532 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3363 = _M0L7_2abindS1532;
  _M0L6_2atmpS3362
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3363, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4zsetS1530 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3362, 10ll);
  moonbit_decref(_M0L6_2atmpS3362.$0);
  joinlet_3936:;
  #line 1146 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7existedS1540
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1530, _M0L11member__valS1541);
  #line 1147 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1530, _M0L11member__valS1541, _M0L5scoreS1542);
  _M0L4dataS3360 = _M0L4selfS1528->$0;
  _M0L4ZSetS3361
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3361)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 4);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4ZSet*)_M0L4ZSetS3361)->$0
  = _M0L4zsetS1530;
  moonbit_incref(_M0L4dataS3360);
  #line 1148 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3360, _M0L3keyS1529, _M0L4ZSetS3361);
  moonbit_decref(_M0L4dataS3360);
  moonbit_decref(_M0L4ZSetS3361);
  return !_M0L7existedS1540;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6sunion(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1509,
  struct _M0TPB5ArrayGsE* _M0L4keysS1505
) {
  struct _M0TUsbE** _M0L7_2abindS1503;
  struct _M0TUsbE** _M0L6_2atmpS3358;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3357;
  struct _M0TPB3MapGsbE* _M0L6resultS1502;
  int32_t _M0L7_2abindS1504;
  int32_t _M0L2__S1506;
  moonbit_string_t* _M0L6_2atmpS3356;
  struct _M0TPB5ArrayGsE* _M0L3arrS1519;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1520;
  #line 1026 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1503 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3358 = _M0L7_2abindS1503;
  _M0L6_2atmpS3357
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3358, .$1 = 0, .$2 = 0
  };
  #line 1027 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6resultS1502 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3357, 10ll);
  moonbit_decref(_M0L6_2atmpS3357.$0);
  _M0L7_2abindS1504 = _M0L4keysS1505->$1;
  _M0L2__S1506 = 0;
  while (1) {
    if (_M0L2__S1506 < _M0L7_2abindS1504) {
      moonbit_string_t* _M0L3bufS3355 = _M0L4keysS1505->$0;
      moonbit_string_t _M0L3keyS1507 =
        (moonbit_string_t)_M0L3bufS3355[_M0L2__S1506];
      struct _M0TPB3MapGsbE* _M0L12current__setS1508;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1510;
      int32_t _M0L6_2atmpS3354;
      moonbit_incref(_M0L3keyS1507);
      #line 1029 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L12current__setS1508
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(_M0L4selfS1509, _M0L3keyS1507);
      moonbit_decref(_M0L3keyS1507);
      #line 1029 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L5_2aitS1510 = _M0MPB3Map5iter2GsbE(_M0L12current__setS1508);
      moonbit_decref(_M0L12current__setS1508);
      while (1) {
        moonbit_string_t _M0L1mS1512;
        struct _M0TUsbE* _M0L7_2abindS1514;
        #line 1030 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1514 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1510);
        if (_M0L7_2abindS1514 == 0) {
          if (_M0L7_2abindS1514) {
            moonbit_decref(_M0L7_2abindS1514);
          }
          moonbit_decref(_M0L5_2aitS1510);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1515 = _M0L7_2abindS1514;
          struct _M0TUsbE* _M0L4_2axS1516 = _M0L7_2aSomeS1515;
          moonbit_string_t _M0L8_2afieldS3494 = _M0L4_2axS1516->$0;
          int32_t _M0L6_2acntS3850 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1516));
          moonbit_string_t _M0L4_2amS1517;
          if (_M0L6_2acntS3850 > 1) {
            int32_t _M0L11_2anew__cntS3851 = _M0L6_2acntS3850 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1516), _M0L11_2anew__cntS3851);
            moonbit_incref(_M0L8_2afieldS3494);
          } else if (_M0L6_2acntS3850 == 1) {
            #line 1030 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L4_2axS1516);
          }
          _M0L4_2amS1517 = _M0L8_2afieldS3494;
          _M0L1mS1512 = _M0L4_2amS1517;
          goto join_1511;
        }
        goto joinlet_3940;
        join_1511:;
        #line 1031 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map3setGsbE(_M0L6resultS1502, _M0L1mS1512, 1);
        moonbit_decref(_M0L1mS1512);
        continue;
        joinlet_3940:;
        break;
      }
      _M0L6_2atmpS3354 = _M0L2__S1506 + 1;
      _M0L2__S1506 = _M0L6_2atmpS3354;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3356 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1519
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1519)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1519->$0 = _M0L6_2atmpS3356;
  _M0L3arrS1519->$1 = 0;
  #line 1034 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1520 = _M0MPB3Map5iter2GsbE(_M0L6resultS1502);
  moonbit_decref(_M0L6resultS1502);
  while (1) {
    moonbit_string_t _M0L1mS1522;
    struct _M0TUsbE* _M0L7_2abindS1524;
    #line 1035 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1524 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1520);
    if (_M0L7_2abindS1524 == 0) {
      if (_M0L7_2abindS1524) {
        moonbit_decref(_M0L7_2abindS1524);
      }
      moonbit_decref(_M0L5_2aitS1520);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1525 = _M0L7_2abindS1524;
      struct _M0TUsbE* _M0L4_2axS1526 = _M0L7_2aSomeS1525;
      moonbit_string_t _M0L8_2afieldS3493 = _M0L4_2axS1526->$0;
      int32_t _M0L6_2acntS3852 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1526));
      moonbit_string_t _M0L4_2amS1527;
      if (_M0L6_2acntS3852 > 1) {
        int32_t _M0L11_2anew__cntS3853 = _M0L6_2acntS3852 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1526), _M0L11_2anew__cntS3853);
        moonbit_incref(_M0L8_2afieldS3493);
      } else if (_M0L6_2acntS3852 == 1) {
        #line 1035 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1526);
      }
      _M0L4_2amS1527 = _M0L8_2afieldS3493;
      _M0L1mS1522 = _M0L4_2amS1527;
      goto join_1521;
    }
    goto joinlet_3942;
    join_1521:;
    #line 1036 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1519, _M0L1mS1522);
    moonbit_decref(_M0L1mS1522);
    continue;
    joinlet_3942:;
    break;
  }
  return _M0L3arrS1519;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6sinter(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1466,
  struct _M0TPB5ArrayGsE* _M0L4keysS1464
) {
  int32_t _M0L6_2atmpS3337;
  moonbit_string_t _M0L6_2atmpS3353;
  struct _M0TPB3MapGsbE* _M0L10first__setS1465;
  int32_t _M0L6_2atmpS3339;
  struct _M0TUsbE** _M0L7_2abindS1468;
  struct _M0TUsbE** _M0L6_2atmpS3352;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3349;
  int32_t _M0L6_2atmpS3351;
  int64_t _M0L6_2atmpS3350;
  struct _M0TPB3MapGsbE* _M0L6resultS1467;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1469;
  int32_t _M0L1iS1477;
  moonbit_string_t* _M0L6_2atmpS3348;
  struct _M0TPB5ArrayGsE* _M0L3arrS1493;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1494;
  #line 985 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 986 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3337 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1464);
  if (_M0L6_2atmpS3337 == 0) {
    moonbit_string_t* _M0L6_2atmpS3338 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3943 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3943)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3943->$0 = _M0L6_2atmpS3338;
    _block_3943->$1 = 0;
    return _block_3943;
  }
  #line 989 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3353 = _M0MPC15array5Array2atGsE(_M0L4keysS1464, 0);
  #line 989 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L10first__setS1465
  = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(_M0L4selfS1466, _M0L6_2atmpS3353);
  moonbit_decref(_M0L6_2atmpS3353);
  #line 990 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3339 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1465);
  if (_M0L6_2atmpS3339 == 0) {
    moonbit_string_t* _M0L6_2atmpS3340;
    struct _M0TPB5ArrayGsE* _block_3944;
    moonbit_decref(_M0L10first__setS1465);
    _M0L6_2atmpS3340 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_3944
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3944)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3944->$0 = _M0L6_2atmpS3340;
    _block_3944->$1 = 0;
    return _block_3944;
  }
  _M0L7_2abindS1468 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3352 = _M0L7_2abindS1468;
  _M0L6_2atmpS3349
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3352, .$1 = 0, .$2 = 0
  };
  #line 993 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3351 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1465);
  _M0L6_2atmpS3350 = (int64_t)_M0L6_2atmpS3351;
  #line 993 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6resultS1467 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3349, _M0L6_2atmpS3350);
  moonbit_decref(_M0L6_2atmpS3349.$0);
  #line 993 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1469 = _M0MPB3Map5iter2GsbE(_M0L10first__setS1465);
  moonbit_decref(_M0L10first__setS1465);
  while (1) {
    moonbit_string_t _M0L1mS1471;
    struct _M0TUsbE* _M0L7_2abindS1473;
    #line 994 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1473 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1469);
    if (_M0L7_2abindS1473 == 0) {
      if (_M0L7_2abindS1473) {
        moonbit_decref(_M0L7_2abindS1473);
      }
      moonbit_decref(_M0L5_2aitS1469);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1474 = _M0L7_2abindS1473;
      struct _M0TUsbE* _M0L4_2axS1475 = _M0L7_2aSomeS1474;
      moonbit_string_t _M0L8_2afieldS3501 = _M0L4_2axS1475->$0;
      int32_t _M0L6_2acntS3854 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1475));
      moonbit_string_t _M0L4_2amS1476;
      if (_M0L6_2acntS3854 > 1) {
        int32_t _M0L11_2anew__cntS3855 = _M0L6_2acntS3854 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1475), _M0L11_2anew__cntS3855);
        moonbit_incref(_M0L8_2afieldS3501);
      } else if (_M0L6_2acntS3854 == 1) {
        #line 994 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1475);
      }
      _M0L4_2amS1476 = _M0L8_2afieldS3501;
      _M0L1mS1471 = _M0L4_2amS1476;
      goto join_1470;
    }
    goto joinlet_3946;
    join_1470:;
    #line 995 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsbE(_M0L6resultS1467, _M0L1mS1471, 1);
    moonbit_decref(_M0L1mS1471);
    continue;
    joinlet_3946:;
    break;
  }
  _M0L1iS1477 = 1;
  while (1) {
    int32_t _M0L6_2atmpS3341;
    #line 997 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3341 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1464);
    if (_M0L1iS1477 < _M0L6_2atmpS3341) {
      moonbit_string_t _M0L6_2atmpS3346;
      struct _M0TPB3MapGsbE* _M0L12current__setS1478;
      moonbit_string_t* _M0L6_2atmpS3345;
      struct _M0TPB5ArrayGsE* _M0L10to__removeS1479;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1480;
      int32_t _M0L7_2abindS1488;
      int32_t _M0L2__S1489;
      int32_t _M0L6_2atmpS3347;
      #line 998 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3346
      = _M0MPC15array5Array2atGsE(_M0L4keysS1464, _M0L1iS1477);
      #line 998 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L12current__setS1478
      = _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(_M0L4selfS1466, _M0L6_2atmpS3346);
      moonbit_decref(_M0L6_2atmpS3346);
      _M0L6_2atmpS3345 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L10to__removeS1479
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L10to__removeS1479)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
      _M0L10to__removeS1479->$0 = _M0L6_2atmpS3345;
      _M0L10to__removeS1479->$1 = 0;
      #line 999 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L5_2aitS1480 = _M0MPB3Map5iter2GsbE(_M0L6resultS1467);
      while (1) {
        moonbit_string_t _M0L1mS1482;
        struct _M0TUsbE* _M0L7_2abindS1484;
        int32_t _M0L6_2atmpS3342;
        #line 1000 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1484 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1480);
        if (_M0L7_2abindS1484 == 0) {
          if (_M0L7_2abindS1484) {
            moonbit_decref(_M0L7_2abindS1484);
          }
          moonbit_decref(_M0L5_2aitS1480);
          moonbit_decref(_M0L12current__setS1478);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1485 = _M0L7_2abindS1484;
          struct _M0TUsbE* _M0L4_2axS1486 = _M0L7_2aSomeS1485;
          moonbit_string_t _M0L8_2afieldS3500 = _M0L4_2axS1486->$0;
          int32_t _M0L6_2acntS3856 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1486));
          moonbit_string_t _M0L4_2amS1487;
          if (_M0L6_2acntS3856 > 1) {
            int32_t _M0L11_2anew__cntS3857 = _M0L6_2acntS3856 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1486), _M0L11_2anew__cntS3857);
            moonbit_incref(_M0L8_2afieldS3500);
          } else if (_M0L6_2acntS3856 == 1) {
            #line 1000 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L4_2axS1486);
          }
          _M0L4_2amS1487 = _M0L8_2afieldS3500;
          _M0L1mS1482 = _M0L4_2amS1487;
          goto join_1481;
        }
        goto joinlet_3949;
        join_1481:;
        #line 1001 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3342
        = _M0MPB3Map8containsGsbE(_M0L12current__setS1478, _M0L1mS1482);
        if (!_M0L6_2atmpS3342) {
          #line 1002 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPC15array5Array4pushGsE(_M0L10to__removeS1479, _M0L1mS1482);
          moonbit_decref(_M0L1mS1482);
        } else {
          moonbit_decref(_M0L1mS1482);
        }
        continue;
        joinlet_3949:;
        break;
      }
      _M0L7_2abindS1488 = _M0L10to__removeS1479->$1;
      _M0L2__S1489 = 0;
      while (1) {
        if (_M0L2__S1489 < _M0L7_2abindS1488) {
          moonbit_string_t* _M0L3bufS3344 = _M0L10to__removeS1479->$0;
          moonbit_string_t _M0L1mS1490 =
            (moonbit_string_t)_M0L3bufS3344[_M0L2__S1489];
          int32_t _M0L6_2atmpS3343;
          moonbit_incref(_M0L1mS1490);
          #line 1006 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPB3Map6removeGsbE(_M0L6resultS1467, _M0L1mS1490);
          moonbit_decref(_M0L1mS1490);
          _M0L6_2atmpS3343 = _M0L2__S1489 + 1;
          _M0L2__S1489 = _M0L6_2atmpS3343;
          continue;
        } else {
          moonbit_decref(_M0L10to__removeS1479);
        }
        break;
      }
      _M0L6_2atmpS3347 = _M0L1iS1477 + 1;
      _M0L1iS1477 = _M0L6_2atmpS3347;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3348 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1493
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1493)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1493->$0 = _M0L6_2atmpS3348;
  _M0L3arrS1493->$1 = 0;
  #line 1009 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1494 = _M0MPB3Map5iter2GsbE(_M0L6resultS1467);
  moonbit_decref(_M0L6resultS1467);
  while (1) {
    moonbit_string_t _M0L1mS1496;
    struct _M0TUsbE* _M0L7_2abindS1498;
    #line 1010 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1498 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1494);
    if (_M0L7_2abindS1498 == 0) {
      if (_M0L7_2abindS1498) {
        moonbit_decref(_M0L7_2abindS1498);
      }
      moonbit_decref(_M0L5_2aitS1494);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1499 = _M0L7_2abindS1498;
      struct _M0TUsbE* _M0L4_2axS1500 = _M0L7_2aSomeS1499;
      moonbit_string_t _M0L8_2afieldS3497 = _M0L4_2axS1500->$0;
      int32_t _M0L6_2acntS3858 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1500));
      moonbit_string_t _M0L4_2amS1501;
      if (_M0L6_2acntS3858 > 1) {
        int32_t _M0L11_2anew__cntS3859 = _M0L6_2acntS3858 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1500), _M0L11_2anew__cntS3859);
        moonbit_incref(_M0L8_2afieldS3497);
      } else if (_M0L6_2acntS3858 == 1) {
        #line 1010 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1500);
      }
      _M0L4_2amS1501 = _M0L8_2afieldS3497;
      _M0L1mS1496 = _M0L4_2amS1501;
      goto join_1495;
    }
    goto joinlet_3952;
    join_1495:;
    #line 1011 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1493, _M0L1mS1496);
    moonbit_decref(_M0L1mS1496);
    continue;
    joinlet_3952:;
    break;
  }
  return _M0L3arrS1493;
}

struct _M0TPB3MapGsbE* _M0MP38JIA2JIA29moonbitdb3lib8Database17get__set__members(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1452,
  moonbit_string_t _M0L3keyS1453
) {
  #line 974 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 975 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1452, _M0L3keyS1453)
  ) {
    struct _M0TUsbE** _M0L7_2abindS1454 =
      (struct _M0TUsbE**)moonbit_empty_ref_array;
    struct _M0TUsbE** _M0L6_2atmpS3333 = _M0L7_2abindS1454;
    struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3332 =
      (struct _M0TPB9ArrayViewGUsbEE){.$0 = _M0L6_2atmpS3333,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGsbE* _result_3953;
    #line 976 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3953 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3332, 0ll);
    moonbit_decref(_M0L6_2atmpS3332.$0);
    return _result_3953;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1458;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3336 =
      _M0L4selfS1452->$0;
    void* _M0L7_2abindS1459;
    struct _M0TUsbE** _M0L7_2abindS1456;
    struct _M0TUsbE** _M0L6_2atmpS3335;
    struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3334;
    struct _M0TPB3MapGsbE* _result_3956;
    moonbit_incref(_M0L4dataS3336);
    #line 978 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1459
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3336, _M0L3keyS1453);
    moonbit_decref(_M0L4dataS3336);
    if (_M0L7_2abindS1459 == 0) {
      if (_M0L7_2abindS1459) {
        moonbit_decref(_M0L7_2abindS1459);
      }
      goto join_1455;
    } else {
      void* _M0L7_2aSomeS1460 = _M0L7_2abindS1459;
      void* _M0L4_2axS1461 = _M0L7_2aSomeS1460;
      switch (Moonbit_object_tag(_M0L4_2axS1461)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1462 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1461;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3502 = _M0L6_2aSetS1462->$0;
          int32_t _M0L6_2acntS3860 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1462));
          struct _M0TPB3MapGsbE* _M0L4_2asS1463;
          if (_M0L6_2acntS3860 > 1) {
            int32_t _M0L11_2anew__cntS3861 = _M0L6_2acntS3860 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1462), _M0L11_2anew__cntS3861);
            moonbit_incref(_M0L8_2afieldS3502);
          } else if (_M0L6_2acntS3860 == 1) {
            #line 978 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1462);
          }
          _M0L4_2asS1463 = _M0L8_2afieldS3502;
          _M0L1sS1458 = _M0L4_2asS1463;
          goto join_1457;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1461);
          goto join_1455;
          break;
        }
      }
    }
    join_1457:;
    return _M0L1sS1458;
    join_1455:;
    _M0L7_2abindS1456 = (struct _M0TUsbE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3335 = _M0L7_2abindS1456;
    _M0L6_2atmpS3334
    = (struct _M0TPB9ArrayViewGUsbEE){
      .$0 = _M0L6_2atmpS3335, .$1 = 0, .$2 = 0
    };
    #line 980 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3956 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3334, 0ll);
    moonbit_decref(_M0L6_2atmpS3334.$0);
    return _result_3956;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5scard(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1443,
  moonbit_string_t _M0L3keyS1444
) {
  #line 952 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 953 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1443, _M0L3keyS1444)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1446;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3331 =
      _M0L4selfS1443->$0;
    void* _M0L7_2abindS1447;
    int32_t _result_3958;
    moonbit_incref(_M0L4dataS3331);
    #line 956 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1447
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3331, _M0L3keyS1444);
    moonbit_decref(_M0L4dataS3331);
    if (_M0L7_2abindS1447 == 0) {
      if (_M0L7_2abindS1447) {
        moonbit_decref(_M0L7_2abindS1447);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1448 = _M0L7_2abindS1447;
      void* _M0L4_2axS1449 = _M0L7_2aSomeS1448;
      switch (Moonbit_object_tag(_M0L4_2axS1449)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1450 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1449;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3504 = _M0L6_2aSetS1450->$0;
          int32_t _M0L6_2acntS3862 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1450));
          struct _M0TPB3MapGsbE* _M0L4_2asS1451;
          if (_M0L6_2acntS3862 > 1) {
            int32_t _M0L11_2anew__cntS3863 = _M0L6_2acntS3862 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1450), _M0L11_2anew__cntS3863);
            moonbit_incref(_M0L8_2afieldS3504);
          } else if (_M0L6_2acntS3862 == 1) {
            #line 956 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1450);
          }
          _M0L4_2asS1451 = _M0L8_2afieldS3504;
          _M0L1sS1446 = _M0L4_2asS1451;
          goto join_1445;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1449);
          return 0;
          break;
        }
      }
    }
    join_1445:;
    #line 957 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3958 = _M0MPB3Map6lengthGsbE(_M0L1sS1446);
    moonbit_decref(_M0L1sS1446);
    return _result_3958;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database8smembers(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1424,
  moonbit_string_t _M0L3keyS1425
) {
  #line 917 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 918 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1424, _M0L3keyS1425)
  ) {
    moonbit_string_t* _M0L6_2atmpS3327 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3959 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3959)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3959->$0 = _M0L6_2atmpS3327;
    _block_3959->$1 = 0;
    return _block_3959;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1428;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3330 =
      _M0L4selfS1424->$0;
    void* _M0L7_2abindS1438;
    moonbit_string_t* _M0L6_2atmpS3329;
    struct _M0TPB5ArrayGsE* _M0L6resultS1429;
    struct _M0TPB4IterGUsbEE* _M0L5_2aitS1430;
    moonbit_string_t* _M0L6_2atmpS3328;
    struct _M0TPB5ArrayGsE* _block_3964;
    moonbit_incref(_M0L4dataS3330);
    #line 921 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1438
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3330, _M0L3keyS1425);
    moonbit_decref(_M0L4dataS3330);
    if (_M0L7_2abindS1438 == 0) {
      if (_M0L7_2abindS1438) {
        moonbit_decref(_M0L7_2abindS1438);
      }
      goto join_1426;
    } else {
      void* _M0L7_2aSomeS1439 = _M0L7_2abindS1438;
      void* _M0L4_2axS1440 = _M0L7_2aSomeS1439;
      switch (Moonbit_object_tag(_M0L4_2axS1440)) {
        case 3: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1441 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1440;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3507 = _M0L6_2aSetS1441->$0;
          int32_t _M0L6_2acntS3866 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1441));
          struct _M0TPB3MapGsbE* _M0L4_2asS1442;
          if (_M0L6_2acntS3866 > 1) {
            int32_t _M0L11_2anew__cntS3867 = _M0L6_2acntS3866 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1441), _M0L11_2anew__cntS3867);
            moonbit_incref(_M0L8_2afieldS3507);
          } else if (_M0L6_2acntS3866 == 1) {
            #line 921 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1441);
          }
          _M0L4_2asS1442 = _M0L8_2afieldS3507;
          _M0L1sS1428 = _M0L4_2asS1442;
          goto join_1427;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1440);
          goto join_1426;
          break;
        }
      }
    }
    join_1427:;
    _M0L6_2atmpS3329 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1429
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1429)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L6resultS1429->$0 = _M0L6_2atmpS3329;
    _M0L6resultS1429->$1 = 0;
    #line 923 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1430 = _M0MPB3Map5iter2GsbE(_M0L1sS1428);
    moonbit_decref(_M0L1sS1428);
    while (1) {
      moonbit_string_t _M0L1mS1432;
      struct _M0TUsbE* _M0L7_2abindS1434;
      #line 924 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1434 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1430);
      if (_M0L7_2abindS1434 == 0) {
        if (_M0L7_2abindS1434) {
          moonbit_decref(_M0L7_2abindS1434);
        }
        moonbit_decref(_M0L5_2aitS1430);
      } else {
        struct _M0TUsbE* _M0L7_2aSomeS1435 = _M0L7_2abindS1434;
        struct _M0TUsbE* _M0L4_2axS1436 = _M0L7_2aSomeS1435;
        moonbit_string_t _M0L8_2afieldS3506 = _M0L4_2axS1436->$0;
        int32_t _M0L6_2acntS3864 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1436));
        moonbit_string_t _M0L4_2amS1437;
        if (_M0L6_2acntS3864 > 1) {
          int32_t _M0L11_2anew__cntS3865 = _M0L6_2acntS3864 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1436), _M0L11_2anew__cntS3865);
          moonbit_incref(_M0L8_2afieldS3506);
        } else if (_M0L6_2acntS3864 == 1) {
          #line 924 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1436);
        }
        _M0L4_2amS1437 = _M0L8_2afieldS3506;
        _M0L1mS1432 = _M0L4_2amS1437;
        goto join_1431;
      }
      goto joinlet_3963;
      join_1431:;
      #line 925 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1429, _M0L1mS1432);
      moonbit_decref(_M0L1mS1432);
      continue;
      joinlet_3963:;
      break;
    }
    return _M0L6resultS1429;
    join_1426:;
    _M0L6_2atmpS3328 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_3964
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3964)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3964->$0 = _M0L6_2atmpS3328;
    _block_3964->$1 = 0;
    return _block_3964;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1411,
  moonbit_string_t _M0L3keyS1412,
  moonbit_string_t _M0L5valueS1423
) {
  int32_t _M0L6_2atmpS3321;
  struct _M0TPB3MapGsbE* _M0L3setS1413;
  struct _M0TPB3MapGsbE* _M0L1sS1417;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3326;
  void* _M0L7_2abindS1418;
  struct _M0TUsbE** _M0L7_2abindS1415;
  struct _M0TUsbE** _M0L6_2atmpS3325;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3324;
  #line 902 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3321
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1411, _M0L3keyS1412);
  _M0L4dataS3326 = _M0L4selfS1411->$0;
  moonbit_incref(_M0L4dataS3326);
  #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1418
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3326, _M0L3keyS1412);
  moonbit_decref(_M0L4dataS3326);
  if (_M0L7_2abindS1418 == 0) {
    if (_M0L7_2abindS1418) {
      moonbit_decref(_M0L7_2abindS1418);
    }
    goto join_1414;
  } else {
    void* _M0L7_2aSomeS1419 = _M0L7_2abindS1418;
    void* _M0L4_2axS1420 = _M0L7_2aSomeS1419;
    switch (Moonbit_object_tag(_M0L4_2axS1420)) {
      case 3: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1421 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1420;
        struct _M0TPB3MapGsbE* _M0L8_2afieldS3510 = _M0L6_2aSetS1421->$0;
        int32_t _M0L6_2acntS3868 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1421));
        struct _M0TPB3MapGsbE* _M0L4_2asS1422;
        if (_M0L6_2acntS3868 > 1) {
          int32_t _M0L11_2anew__cntS3869 = _M0L6_2acntS3868 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1421), _M0L11_2anew__cntS3869);
          moonbit_incref(_M0L8_2afieldS3510);
        } else if (_M0L6_2acntS3868 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2aSetS1421);
        }
        _M0L4_2asS1422 = _M0L8_2afieldS3510;
        _M0L1sS1417 = _M0L4_2asS1422;
        goto join_1416;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1420);
        goto join_1414;
        break;
      }
    }
  }
  goto joinlet_3966;
  join_1416:;
  _M0L3setS1413 = _M0L1sS1417;
  joinlet_3966:;
  goto joinlet_3965;
  join_1414:;
  _M0L7_2abindS1415 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3325 = _M0L7_2abindS1415;
  _M0L6_2atmpS3324
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3325, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3setS1413 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3324, 10ll);
  moonbit_decref(_M0L6_2atmpS3324.$0);
  joinlet_3965:;
  #line 908 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1413, _M0L5valueS1423)) {
    moonbit_decref(_M0L3setS1413);
    return 0;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3322;
    void* _M0L3SetS3323;
    #line 911 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1413, _M0L5valueS1423, 1);
    _M0L4dataS3322 = _M0L4selfS1411->$0;
    _M0L3SetS3323
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS3323)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 16, 3);
    ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue3Set*)_M0L3SetS3323)->$0
    = _M0L3setS1413;
    moonbit_incref(_M0L4dataS3322);
    #line 912 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3322, _M0L3keyS1412, _M0L3SetS3323);
    moonbit_decref(_M0L4dataS3322);
    moonbit_decref(_M0L3SetS3323);
    return 1;
  }
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib8Database6lrange(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1392,
  moonbit_string_t _M0L3keyS1393,
  int32_t _M0L5startS1400,
  int32_t _M0L3endS1402
) {
  #line 720 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 721 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1392, _M0L3keyS1393)
  ) {
    moonbit_string_t* _M0L6_2atmpS3315 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3967 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3967)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3967->$0 = _M0L6_2atmpS3315;
    _block_3967->$1 = 0;
    return _block_3967;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1396;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3320 =
      _M0L4selfS1392->$0;
    void* _M0L7_2abindS1406;
    struct _M0TPB5ArrayGsE* _M0L3arrS1397;
    int32_t _M0L3lenS1398;
    int32_t _M0L10start__idxS1399;
    int32_t _M0L8end__idxS1401;
    moonbit_string_t* _M0L6_2atmpS3319;
    struct _M0TPB5ArrayGsE* _M0L6resultS1403;
    int32_t _M0L1iS1404;
    moonbit_string_t* _M0L6_2atmpS3316;
    struct _M0TPB5ArrayGsE* _block_3972;
    moonbit_incref(_M0L4dataS3320);
    #line 724 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1406
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3320, _M0L3keyS1393);
    moonbit_decref(_M0L4dataS3320);
    if (_M0L7_2abindS1406 == 0) {
      if (_M0L7_2abindS1406) {
        moonbit_decref(_M0L7_2abindS1406);
      }
      goto join_1394;
    } else {
      void* _M0L7_2aSomeS1407 = _M0L7_2abindS1406;
      void* _M0L4_2axS1408 = _M0L7_2aSomeS1407;
      switch (Moonbit_object_tag(_M0L4_2axS1408)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1409 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1408;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3512 =
            _M0L7_2aListS1409->$0;
          int32_t _M0L6_2acntS3870 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1409));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2adequeS1410;
          if (_M0L6_2acntS3870 > 1) {
            int32_t _M0L11_2anew__cntS3871 = _M0L6_2acntS3870 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1409), _M0L11_2anew__cntS3871);
            moonbit_incref(_M0L8_2afieldS3512);
          } else if (_M0L6_2acntS3870 == 1) {
            #line 724 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1409);
          }
          _M0L8_2adequeS1410 = _M0L8_2afieldS3512;
          _M0L5dequeS1396 = _M0L8_2adequeS1410;
          goto join_1395;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1408);
          goto join_1394;
          break;
        }
      }
    }
    join_1395:;
    #line 726 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3arrS1397
    = _M0MP38JIA2JIA29moonbitdb3lib5Deque9to__array(_M0L5dequeS1396);
    moonbit_decref(_M0L5dequeS1396);
    #line 727 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3lenS1398 = _M0MPC15array5Array6lengthGsE(_M0L3arrS1397);
    if (_M0L5startS1400 < 0) {
      _M0L10start__idxS1399 = _M0L3lenS1398 + _M0L5startS1400;
    } else {
      _M0L10start__idxS1399 = _M0L5startS1400;
    }
    if (_M0L3endS1402 < 0) {
      _M0L8end__idxS1401 = _M0L3lenS1398 + _M0L3endS1402;
    } else {
      _M0L8end__idxS1401 = _M0L3endS1402;
    }
    _M0L6_2atmpS3319 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1403
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1403)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L6resultS1403->$0 = _M0L6_2atmpS3319;
    _M0L6resultS1403->$1 = 0;
    _M0L1iS1404 = _M0L10start__idxS1399;
    while (1) {
      int32_t _if__result_3971;
      if (_M0L1iS1404 <= _M0L8end__idxS1401) {
        if (_M0L1iS1404 >= 0) {
          _if__result_3971 = _M0L1iS1404 < _M0L3lenS1398;
        } else {
          _if__result_3971 = 0;
        }
      } else {
        _if__result_3971 = 0;
      }
      if (_if__result_3971) {
        moonbit_string_t _M0L6_2atmpS3317;
        int32_t _M0L6_2atmpS3318;
        #line 732 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3317
        = _M0MPC15array5Array2atGsE(_M0L3arrS1397, _M0L1iS1404);
        #line 732 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1403, _M0L6_2atmpS3317);
        moonbit_decref(_M0L6_2atmpS3317);
        _M0L6_2atmpS3318 = _M0L1iS1404 + 1;
        _M0L1iS1404 = _M0L6_2atmpS3318;
        continue;
      } else {
        moonbit_decref(_M0L3arrS1397);
      }
      break;
    }
    return _M0L6resultS1403;
    join_1394:;
    _M0L6_2atmpS3316 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_3972
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3972)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_3972->$0 = _M0L6_2atmpS3316;
    _block_3972->$1 = 0;
    return _block_3972;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1383,
  moonbit_string_t _M0L3keyS1384
) {
  #line 709 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1383, _M0L3keyS1384)
  ) {
    return 0;
  } else {
    struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1386;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3314 =
      _M0L4selfS1383->$0;
    void* _M0L7_2abindS1387;
    int32_t _result_3974;
    moonbit_incref(_M0L4dataS3314);
    #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1387
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3314, _M0L3keyS1384);
    moonbit_decref(_M0L4dataS3314);
    if (_M0L7_2abindS1387 == 0) {
      if (_M0L7_2abindS1387) {
        moonbit_decref(_M0L7_2abindS1387);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1388 = _M0L7_2abindS1387;
      void* _M0L4_2axS1389 = _M0L7_2aSomeS1388;
      switch (Moonbit_object_tag(_M0L4_2axS1389)) {
        case 2: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1390 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1389;
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3514 =
            _M0L7_2aListS1390->$0;
          int32_t _M0L6_2acntS3872 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1390));
          struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1391;
          if (_M0L6_2acntS3872 > 1) {
            int32_t _M0L11_2anew__cntS3873 = _M0L6_2acntS3872 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1390), _M0L11_2anew__cntS3873);
            moonbit_incref(_M0L8_2afieldS3514);
          } else if (_M0L6_2acntS3872 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1390);
          }
          _M0L4_2adS1391 = _M0L8_2afieldS3514;
          _M0L1dS1386 = _M0L4_2adS1391;
          goto join_1385;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1389);
          return 0;
          break;
        }
      }
    }
    join_1385:;
    #line 714 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3974 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L1dS1386);
    moonbit_decref(_M0L1dS1386);
    return _result_3974;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database5lpush(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1371,
  moonbit_string_t _M0L3keyS1372,
  moonbit_string_t _M0L5valueS1382
) {
  int32_t _M0L6_2atmpS3310;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L5dequeS1373;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L1dS1376;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3313;
  void* _M0L7_2abindS1377;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3311;
  void* _M0L4ListS3312;
  int32_t _result_3977;
  #line 657 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 658 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3310
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1371, _M0L3keyS1372);
  _M0L4dataS3313 = _M0L4selfS1371->$0;
  moonbit_incref(_M0L4dataS3313);
  #line 659 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1377
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3313, _M0L3keyS1372);
  moonbit_decref(_M0L4dataS3313);
  if (_M0L7_2abindS1377 == 0) {
    if (_M0L7_2abindS1377) {
      moonbit_decref(_M0L7_2abindS1377);
    }
    goto join_1374;
  } else {
    void* _M0L7_2aSomeS1378 = _M0L7_2abindS1377;
    void* _M0L4_2axS1379 = _M0L7_2aSomeS1378;
    switch (Moonbit_object_tag(_M0L4_2axS1379)) {
      case 2: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List* _M0L7_2aListS1380 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4_2axS1379;
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L8_2afieldS3517 =
          _M0L7_2aListS1380->$0;
        int32_t _M0L6_2acntS3874 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1380));
        struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4_2adS1381;
        if (_M0L6_2acntS3874 > 1) {
          int32_t _M0L11_2anew__cntS3875 = _M0L6_2acntS3874 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1380), _M0L11_2anew__cntS3875);
          moonbit_incref(_M0L8_2afieldS3517);
        } else if (_M0L6_2acntS3874 == 1) {
          #line 659 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aListS1380);
        }
        _M0L4_2adS1381 = _M0L8_2afieldS3517;
        _M0L1dS1376 = _M0L4_2adS1381;
        goto join_1375;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1379);
        goto join_1374;
        break;
      }
    }
  }
  goto joinlet_3976;
  join_1375:;
  _M0L5dequeS1373 = _M0L1dS1376;
  joinlet_3976:;
  goto joinlet_3975;
  join_1374:;
  #line 661 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5dequeS1373 = _M0MP38JIA2JIA29moonbitdb3lib5Deque3new();
  joinlet_3975:;
  #line 663 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib5Deque11push__front(_M0L5dequeS1373, _M0L5valueS1382);
  _M0L4dataS3311 = _M0L4selfS1371->$0;
  moonbit_incref(_M0L5dequeS1373);
  _M0L4ListS3312
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List));
  Moonbit_object_header(_M0L4ListS3312)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4List*)_M0L4ListS3312)->$0
  = _M0L5dequeS1373;
  moonbit_incref(_M0L4dataS3311);
  #line 664 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3311, _M0L3keyS1372, _M0L4ListS3312);
  moonbit_decref(_M0L4dataS3311);
  moonbit_decref(_M0L4ListS3312);
  #line 665 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3977 = _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(_M0L5dequeS1373);
  moonbit_decref(_M0L5dequeS1373);
  return _result_3977;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hlen(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1362,
  moonbit_string_t _M0L3keyS1363
) {
  #line 646 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 647 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1362, _M0L3keyS1363)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1365;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3309 =
      _M0L4selfS1362->$0;
    void* _M0L7_2abindS1366;
    int32_t _result_3979;
    moonbit_incref(_M0L4dataS3309);
    #line 650 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1366
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3309, _M0L3keyS1363);
    moonbit_decref(_M0L4dataS3309);
    if (_M0L7_2abindS1366 == 0) {
      if (_M0L7_2abindS1366) {
        moonbit_decref(_M0L7_2abindS1366);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1367 = _M0L7_2abindS1366;
      void* _M0L4_2axS1368 = _M0L7_2aSomeS1367;
      switch (Moonbit_object_tag(_M0L4_2axS1368)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1369 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1368;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3519 = _M0L7_2aHashS1369->$0;
          int32_t _M0L6_2acntS3876 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1369));
          struct _M0TPB3MapGssE* _M0L4_2ahS1370;
          if (_M0L6_2acntS3876 > 1) {
            int32_t _M0L11_2anew__cntS3877 = _M0L6_2acntS3876 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1369), _M0L11_2anew__cntS3877);
            moonbit_incref(_M0L8_2afieldS3519);
          } else if (_M0L6_2acntS3876 == 1) {
            #line 650 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1369);
          }
          _M0L4_2ahS1370 = _M0L8_2afieldS3519;
          _M0L1hS1365 = _M0L4_2ahS1370;
          goto join_1364;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1368);
          return 0;
          break;
        }
      }
    }
    join_1364:;
    #line 651 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3979 = _M0MPB3Map6lengthGssE(_M0L1hS1365);
    moonbit_decref(_M0L1hS1365);
    return _result_3979;
  }
}

struct _M0TPB3MapGssE* _M0MP38JIA2JIA29moonbitdb3lib8Database7hgetall(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1350,
  moonbit_string_t _M0L3keyS1351
) {
  #line 635 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 636 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1350, _M0L3keyS1351)
  ) {
    struct _M0TUssE** _M0L7_2abindS1352 =
      (struct _M0TUssE**)moonbit_empty_ref_array;
    struct _M0TUssE** _M0L6_2atmpS3305 = _M0L7_2abindS1352;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3304 =
      (struct _M0TPB9ArrayViewGUssEE){.$0 = _M0L6_2atmpS3305,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGssE* _result_3980;
    #line 637 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3980 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3304, 0ll);
    moonbit_decref(_M0L6_2atmpS3304.$0);
    return _result_3980;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1356;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3308 =
      _M0L4selfS1350->$0;
    void* _M0L7_2abindS1357;
    struct _M0TUssE** _M0L7_2abindS1354;
    struct _M0TUssE** _M0L6_2atmpS3307;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3306;
    struct _M0TPB3MapGssE* _result_3983;
    moonbit_incref(_M0L4dataS3308);
    #line 639 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1357
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3308, _M0L3keyS1351);
    moonbit_decref(_M0L4dataS3308);
    if (_M0L7_2abindS1357 == 0) {
      if (_M0L7_2abindS1357) {
        moonbit_decref(_M0L7_2abindS1357);
      }
      goto join_1353;
    } else {
      void* _M0L7_2aSomeS1358 = _M0L7_2abindS1357;
      void* _M0L4_2axS1359 = _M0L7_2aSomeS1358;
      switch (Moonbit_object_tag(_M0L4_2axS1359)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1360 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1359;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3521 = _M0L7_2aHashS1360->$0;
          int32_t _M0L6_2acntS3878 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1360));
          struct _M0TPB3MapGssE* _M0L4_2ahS1361;
          if (_M0L6_2acntS3878 > 1) {
            int32_t _M0L11_2anew__cntS3879 = _M0L6_2acntS3878 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1360), _M0L11_2anew__cntS3879);
            moonbit_incref(_M0L8_2afieldS3521);
          } else if (_M0L6_2acntS3878 == 1) {
            #line 639 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1360);
          }
          _M0L4_2ahS1361 = _M0L8_2afieldS3521;
          _M0L1hS1356 = _M0L4_2ahS1361;
          goto join_1355;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1359);
          goto join_1353;
          break;
        }
      }
    }
    join_1355:;
    return _M0L1hS1356;
    join_1353:;
    _M0L7_2abindS1354 = (struct _M0TUssE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3307 = _M0L7_2abindS1354;
    _M0L6_2atmpS3306
    = (struct _M0TPB9ArrayViewGUssEE){
      .$0 = _M0L6_2atmpS3307, .$1 = 0, .$2 = 0
    };
    #line 641 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3983 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3306, 0ll);
    moonbit_decref(_M0L6_2atmpS3306.$0);
    return _result_3983;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1339,
  moonbit_string_t _M0L3keyS1340,
  moonbit_string_t _M0L5fieldS1344
) {
  #line 606 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1339, _M0L3keyS1340)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1343;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3303 =
      _M0L4selfS1339->$0;
    void* _M0L7_2abindS1345;
    moonbit_string_t _result_3986;
    moonbit_incref(_M0L4dataS3303);
    #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1345
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3303, _M0L3keyS1340);
    moonbit_decref(_M0L4dataS3303);
    if (_M0L7_2abindS1345 == 0) {
      if (_M0L7_2abindS1345) {
        moonbit_decref(_M0L7_2abindS1345);
      }
      goto join_1341;
    } else {
      void* _M0L7_2aSomeS1346 = _M0L7_2abindS1345;
      void* _M0L4_2axS1347 = _M0L7_2aSomeS1346;
      switch (Moonbit_object_tag(_M0L4_2axS1347)) {
        case 1: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1348 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1347;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3523 = _M0L7_2aHashS1348->$0;
          int32_t _M0L6_2acntS3880 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1348));
          struct _M0TPB3MapGssE* _M0L4_2ahS1349;
          if (_M0L6_2acntS3880 > 1) {
            int32_t _M0L11_2anew__cntS3881 = _M0L6_2acntS3880 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1348), _M0L11_2anew__cntS3881);
            moonbit_incref(_M0L8_2afieldS3523);
          } else if (_M0L6_2acntS3880 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1348);
          }
          _M0L4_2ahS1349 = _M0L8_2afieldS3523;
          _M0L1hS1343 = _M0L4_2ahS1349;
          goto join_1342;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1347);
          goto join_1341;
          break;
        }
      }
    }
    join_1342:;
    #line 611 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3986 = _M0MPB3Map3getGssE(_M0L1hS1343, _M0L5fieldS1344);
    moonbit_decref(_M0L1hS1343);
    return _result_3986;
    join_1341:;
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1325,
  moonbit_string_t _M0L3keyS1326,
  moonbit_string_t _M0L5fieldS1337,
  moonbit_string_t _M0L5valueS1338
) {
  int32_t _M0L6_2atmpS3297;
  struct _M0TPB3MapGssE* _M0L4hashS1327;
  struct _M0TPB3MapGssE* _M0L1hS1331;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3302;
  void* _M0L7_2abindS1332;
  struct _M0TUssE** _M0L7_2abindS1329;
  struct _M0TUssE** _M0L6_2atmpS3301;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3300;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3298;
  void* _M0L4HashS3299;
  #line 596 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3297
  = _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1325, _M0L3keyS1326);
  _M0L4dataS3302 = _M0L4selfS1325->$0;
  moonbit_incref(_M0L4dataS3302);
  #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1332
  = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3302, _M0L3keyS1326);
  moonbit_decref(_M0L4dataS3302);
  if (_M0L7_2abindS1332 == 0) {
    if (_M0L7_2abindS1332) {
      moonbit_decref(_M0L7_2abindS1332);
    }
    goto join_1328;
  } else {
    void* _M0L7_2aSomeS1333 = _M0L7_2abindS1332;
    void* _M0L4_2axS1334 = _M0L7_2aSomeS1333;
    switch (Moonbit_object_tag(_M0L4_2axS1334)) {
      case 1: {
        struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1335 =
          (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1334;
        struct _M0TPB3MapGssE* _M0L8_2afieldS3526 = _M0L7_2aHashS1335->$0;
        int32_t _M0L6_2acntS3882 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1335));
        struct _M0TPB3MapGssE* _M0L4_2ahS1336;
        if (_M0L6_2acntS3882 > 1) {
          int32_t _M0L11_2anew__cntS3883 = _M0L6_2acntS3882 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1335), _M0L11_2anew__cntS3883);
          moonbit_incref(_M0L8_2afieldS3526);
        } else if (_M0L6_2acntS3882 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aHashS1335);
        }
        _M0L4_2ahS1336 = _M0L8_2afieldS3526;
        _M0L1hS1331 = _M0L4_2ahS1336;
        goto join_1330;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1334);
        goto join_1328;
        break;
      }
    }
  }
  goto joinlet_3988;
  join_1330:;
  _M0L4hashS1327 = _M0L1hS1331;
  joinlet_3988:;
  goto joinlet_3987;
  join_1328:;
  _M0L7_2abindS1329 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3301 = _M0L7_2abindS1329;
  _M0L6_2atmpS3300
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS3301, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4hashS1327 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3300, 10ll);
  moonbit_decref(_M0L6_2atmpS3300.$0);
  joinlet_3987:;
  #line 602 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1327, _M0L5fieldS1337, _M0L5valueS1338);
  _M0L4dataS3298 = _M0L4selfS1325->$0;
  _M0L4HashS3299
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS3299)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 22, 1);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue4Hash*)_M0L4HashS3299)->$0
  = _M0L4hashS1327;
  moonbit_incref(_M0L4dataS3298);
  #line 603 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3298, _M0L3keyS1326, _M0L4HashS3299);
  moonbit_decref(_M0L4dataS3298);
  moonbit_decref(_M0L4HashS3299);
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6exists(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1323,
  moonbit_string_t _M0L3keyS1324
) {
  #line 369 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 370 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1323, _M0L3keyS1324)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3296 =
      _M0L4selfS1323->$0;
    int32_t _result_3989;
    moonbit_incref(_M0L4dataS3296);
    #line 373 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3989
    = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3296, _M0L3keyS1324);
    moonbit_decref(_M0L4dataS3296);
    return _result_3989;
  }
}

moonbit_string_t _M0MP38JIA2JIA29moonbitdb3lib8Database3get(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1313,
  moonbit_string_t _M0L3keyS1314
) {
  #line 345 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 346 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1313, _M0L3keyS1314)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1317;
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3295 =
      _M0L4selfS1313->$0;
    void* _M0L7_2abindS1318;
    moonbit_incref(_M0L4dataS3295);
    #line 349 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1318
    = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3295, _M0L3keyS1314);
    moonbit_decref(_M0L4dataS3295);
    if (_M0L7_2abindS1318 == 0) {
      if (_M0L7_2abindS1318) {
        moonbit_decref(_M0L7_2abindS1318);
      }
      goto join_1315;
    } else {
      void* _M0L7_2aSomeS1319 = _M0L7_2abindS1318;
      void* _M0L4_2axS1320 = _M0L7_2aSomeS1319;
      switch (Moonbit_object_tag(_M0L4_2axS1320)) {
        case 0: {
          struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1321 =
            (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1320;
          moonbit_string_t _M0L8_2afieldS3529 = _M0L9_2aStringS1321->$0;
          int32_t _M0L6_2acntS3884 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1321));
          moonbit_string_t _M0L4_2asS1322;
          if (_M0L6_2acntS3884 > 1) {
            int32_t _M0L11_2anew__cntS3885 = _M0L6_2acntS3884 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1321), _M0L11_2anew__cntS3885);
            moonbit_incref(_M0L8_2afieldS3529);
          } else if (_M0L6_2acntS3884 == 1) {
            #line 349 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1321);
          }
          _M0L4_2asS1322 = _M0L8_2afieldS3529;
          _M0L1sS1317 = _M0L4_2asS1322;
          goto join_1316;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1320);
          goto join_1315;
          break;
        }
      }
    }
    join_1316:;
    return _M0L1sS1317;
    join_1315:;
    return 0;
  }
}

struct _M0TPB5ArrayGOsE* _M0MP38JIA2JIA29moonbitdb3lib8Database4mget(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1303,
  struct _M0TPB5ArrayGsE* _M0L4keysS1300
) {
  moonbit_string_t* _M0L6_2atmpS3294;
  struct _M0TPB5ArrayGOsE* _M0L6resultS1298;
  int32_t _M0L7_2abindS1299;
  int32_t _M0L2__S1301;
  #line 276 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3294 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1298
  = (struct _M0TPB5ArrayGOsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGOsE));
  Moonbit_object_header(_M0L6resultS1298)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
  _M0L6resultS1298->$0 = _M0L6_2atmpS3294;
  _M0L6resultS1298->$1 = 0;
  _M0L7_2abindS1299 = _M0L4keysS1300->$1;
  _M0L2__S1301 = 0;
  while (1) {
    if (_M0L2__S1301 < _M0L7_2abindS1299) {
      moonbit_string_t* _M0L3bufS3293 = _M0L4keysS1300->$0;
      moonbit_string_t _M0L3keyS1302 =
        (moonbit_string_t)_M0L3bufS3293[_M0L2__S1301];
      int32_t _M0L6_2atmpS3292;
      moonbit_incref(_M0L3keyS1302);
      #line 279 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      if (
        _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(_M0L4selfS1303, _M0L3keyS1302)
      ) {
        moonbit_string_t _M0L6_2atmpS3288;
        moonbit_decref(_M0L3keyS1302);
        _M0L6_2atmpS3288 = 0;
        #line 280 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1298, _M0L6_2atmpS3288);
        if (_M0L6_2atmpS3288) {
          moonbit_decref(_M0L6_2atmpS3288);
        }
      } else {
        moonbit_string_t _M0L1sS1306;
        struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3291 =
          _M0L4selfS1303->$0;
        void* _M0L7_2abindS1307;
        moonbit_string_t _M0L6_2atmpS3290;
        moonbit_string_t _M0L6_2atmpS3289;
        moonbit_incref(_M0L4dataS3291);
        #line 282 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1307
        = _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3291, _M0L3keyS1302);
        moonbit_decref(_M0L4dataS3291);
        moonbit_decref(_M0L3keyS1302);
        if (_M0L7_2abindS1307 == 0) {
          if (_M0L7_2abindS1307) {
            moonbit_decref(_M0L7_2abindS1307);
          }
          goto join_1304;
        } else {
          void* _M0L7_2aSomeS1308 = _M0L7_2abindS1307;
          void* _M0L4_2axS1309 = _M0L7_2aSomeS1308;
          switch (Moonbit_object_tag(_M0L4_2axS1309)) {
            case 0: {
              struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1310 =
                (struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L4_2axS1309;
              moonbit_string_t _M0L8_2afieldS3531 = _M0L9_2aStringS1310->$0;
              int32_t _M0L6_2acntS3886 =
                Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1310));
              moonbit_string_t _M0L4_2asS1311;
              if (_M0L6_2acntS3886 > 1) {
                int32_t _M0L11_2anew__cntS3887 = _M0L6_2acntS3886 - 1;
                Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1310), _M0L11_2anew__cntS3887);
                moonbit_incref(_M0L8_2afieldS3531);
              } else if (_M0L6_2acntS3886 == 1) {
                #line 282 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
                moonbit_free(_M0L9_2aStringS1310);
              }
              _M0L4_2asS1311 = _M0L8_2afieldS3531;
              _M0L1sS1306 = _M0L4_2asS1311;
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
        goto joinlet_3994;
        join_1305:;
        _M0L6_2atmpS3290 = _M0L1sS1306;
        #line 283 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1298, _M0L6_2atmpS3290);
        if (_M0L6_2atmpS3290) {
          moonbit_decref(_M0L6_2atmpS3290);
        }
        joinlet_3994:;
        goto joinlet_3993;
        join_1304:;
        _M0L6_2atmpS3289 = 0;
        #line 284 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1298, _M0L6_2atmpS3289);
        if (_M0L6_2atmpS3289) {
          moonbit_decref(_M0L6_2atmpS3289);
        }
        joinlet_3993:;
      }
      _M0L6_2atmpS3292 = _M0L2__S1301 + 1;
      _M0L2__S1301 = _M0L6_2atmpS3292;
      continue;
    }
    break;
  }
  return _M0L6resultS1298;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database4mset(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1296,
  struct _M0TPB5ArrayGsE* _M0L4keysS1294,
  struct _M0TPB5ArrayGsE* _M0L6valuesS1295
) {
  int32_t _M0L1iS1293;
  #line 269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L1iS1293 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3280;
    int32_t _if__result_3996;
    #line 270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3280 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1294);
    if (_M0L1iS1293 < _M0L6_2atmpS3280) {
      int32_t _M0L6_2atmpS3279;
      #line 270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3279 = _M0MPC15array5Array6lengthGsE(_M0L6valuesS1295);
      _if__result_3996 = _M0L1iS1293 < _M0L6_2atmpS3279;
    } else {
      _if__result_3996 = 0;
    }
    if (_if__result_3996) {
      struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3281 =
        _M0L4selfS1296->$0;
      moonbit_string_t _M0L6_2atmpS3282;
      moonbit_string_t _M0L6_2atmpS3284;
      void* _M0L6StringS3283;
      struct _M0TPB3MapGsiE* _M0L7expiresS3285;
      moonbit_string_t _M0L6_2atmpS3286;
      int32_t _M0L6_2atmpS3287;
      moonbit_incref(_M0L4dataS3281);
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3282
      = _M0MPC15array5Array2atGsE(_M0L4keysS1294, _M0L1iS1293);
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3284
      = _M0MPC15array5Array2atGsE(_M0L6valuesS1295, _M0L1iS1293);
      _M0L6StringS3283
      = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
      Moonbit_object_header(_M0L6StringS3283)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 28, 0);
      ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3283)->$0
      = _M0L6_2atmpS3284;
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3281, _M0L6_2atmpS3282, _M0L6StringS3283);
      moonbit_decref(_M0L4dataS3281);
      moonbit_decref(_M0L6_2atmpS3282);
      moonbit_decref(_M0L6StringS3283);
      _M0L7expiresS3285 = _M0L4selfS1296->$1;
      moonbit_incref(_M0L7expiresS3285);
      #line 272 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3286
      = _M0MPC15array5Array2atGsE(_M0L4keysS1294, _M0L1iS1293);
      #line 272 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS3285, _M0L6_2atmpS3286);
      moonbit_decref(_M0L7expiresS3285);
      moonbit_decref(_M0L6_2atmpS3286);
      _M0L6_2atmpS3287 = _M0L1iS1293 + 1;
      _M0L1iS1293 = _M0L6_2atmpS3287;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1290,
  moonbit_string_t _M0L3keyS1291
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3271;
  int32_t _M0L6_2atmpS3270;
  #line 226 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3271 = _M0L4selfS1290->$0;
  moonbit_incref(_M0L4dataS3271);
  #line 227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3270
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3271, _M0L3keyS1291);
  moonbit_decref(_M0L4dataS3271);
  if (!_M0L6_2atmpS3270) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS3272 = _M0L4selfS1290->$1;
    int32_t _result_3997;
    moonbit_incref(_M0L7expiresS3272);
    #line 229 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3997 = _M0MPB3Map8containsGsiE(_M0L7expiresS3272, _M0L3keyS1291);
    moonbit_decref(_M0L7expiresS3272);
    if (_result_3997) {
      struct _M0TPB3MapGsiE* _M0L7expiresS3278 = _M0L4selfS1290->$1;
      int64_t _M0L6_2atmpS3277;
      int32_t _M0L6_2atmpS3275;
      int32_t _M0L13current__timeS3276;
      int32_t _M0L9remainingS1292;
      moonbit_incref(_M0L7expiresS3278);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3277 = _M0MPB3Map3getGsiE(_M0L7expiresS3278, _M0L3keyS1291);
      moonbit_decref(_M0L7expiresS3278);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3275 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS3277);
      _M0L13current__timeS3276 = _M0L4selfS1290->$2;
      _M0L9remainingS1292 = _M0L6_2atmpS3275 - _M0L13current__timeS3276;
      if (_M0L9remainingS1292 <= 0) {
        struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3273 =
          _M0L4selfS1290->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS3274;
        moonbit_incref(_M0L4dataS3273);
        #line 232 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3273, _M0L3keyS1291);
        moonbit_decref(_M0L4dataS3273);
        _M0L7expiresS3274 = _M0L4selfS1290->$1;
        moonbit_incref(_M0L7expiresS3274);
        #line 233 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS3274, _M0L3keyS1291);
        moonbit_decref(_M0L7expiresS3274);
        return -2;
      } else {
        return _M0L9remainingS1292 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1287,
  moonbit_string_t _M0L3keyS1288,
  int32_t _M0L7secondsS1289
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3265;
  int32_t _result_3998;
  #line 208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3265 = _M0L4selfS1287->$0;
  moonbit_incref(_M0L4dataS3265);
  #line 209 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_3998
  = _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3265, _M0L3keyS1288);
  moonbit_decref(_M0L4dataS3265);
  if (_result_3998) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3266 = _M0L4selfS1287->$1;
    int32_t _M0L13current__timeS3268 = _M0L4selfS1287->$2;
    int32_t _M0L6_2atmpS3269 = _M0L7secondsS1289 * 1000;
    int32_t _M0L6_2atmpS3267 = _M0L13current__timeS3268 + _M0L6_2atmpS3269;
    moonbit_incref(_M0L7expiresS3266);
    #line 210 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS3266, _M0L3keyS1288, _M0L6_2atmpS3267);
    moonbit_decref(_M0L7expiresS3266);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database3set(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1284,
  moonbit_string_t _M0L3keyS1285,
  moonbit_string_t _M0L5valueS1286
) {
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3262;
  void* _M0L6StringS3263;
  struct _M0TPB3MapGsiE* _M0L7expiresS3264;
  #line 203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3262 = _M0L4selfS1284->$0;
  moonbit_incref(_M0L5valueS1286);
  _M0L6StringS3263
  = (void*)moonbit_malloc(sizeof(struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3263)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 28, 0);
  ((struct _M0DTP38JIA2JIA29moonbitdb3lib10RedisValue6String*)_M0L6StringS3263)->$0
  = _M0L5valueS1286;
  moonbit_incref(_M0L4dataS3262);
  #line 204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3262, _M0L3keyS1285, _M0L6StringS3263);
  moonbit_decref(_M0L4dataS3262);
  moonbit_decref(_M0L6StringS3263);
  _M0L7expiresS3264 = _M0L4selfS1284->$1;
  moonbit_incref(_M0L7expiresS3264);
  #line 205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3264, _M0L3keyS1285);
  moonbit_decref(_M0L7expiresS3264);
  return 0;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database14check__expired(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1279,
  moonbit_string_t _M0L3keyS1280
) {
  int32_t _M0L12expire__timeS1278;
  struct _M0TPB3MapGsiE* _M0L7expiresS3261;
  int64_t _M0L7_2abindS1281;
  int32_t _M0L13current__timeS3258;
  #line 188 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7expiresS3261 = _M0L4selfS1279->$1;
  moonbit_incref(_M0L7expiresS3261);
  #line 189 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1281 = _M0MPB3Map3getGsiE(_M0L7expiresS3261, _M0L3keyS1280);
  moonbit_decref(_M0L7expiresS3261);
  if (_M0L7_2abindS1281 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1282 = _M0L7_2abindS1281;
    int32_t _M0L15_2aexpire__timeS1283 = (int32_t)_M0L7_2aSomeS1282;
    _M0L12expire__timeS1278 = _M0L15_2aexpire__timeS1283;
    goto join_1277;
  }
  join_1277:;
  _M0L13current__timeS3258 = _M0L4selfS1279->$2;
  if (_M0L12expire__timeS1278 <= _M0L13current__timeS3258) {
    struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4dataS3259 =
      _M0L4selfS1279->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS3260;
    moonbit_incref(_M0L4dataS3259);
    #line 192 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4dataS3259, _M0L3keyS1280);
    moonbit_decref(_M0L4dataS3259);
    _M0L7expiresS3260 = _M0L4selfS1279->$1;
    moonbit_incref(_M0L7expiresS3260);
    #line 193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3260, _M0L3keyS1280);
    moonbit_decref(_M0L7expiresS3260);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L4selfS1275,
  int32_t _M0L2msS1276
) {
  int32_t _M0L13current__timeS3257;
  int32_t _M0L6_2atmpS3256;
  #line 184 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L13current__timeS3257 = _M0L4selfS1275->$2;
  _M0L6_2atmpS3256 = _M0L13current__timeS3257 + _M0L2msS1276;
  _M0L4selfS1275->$2 = _M0L6_2atmpS3256;
  return 0;
}

struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0MP38JIA2JIA29moonbitdb3lib8Database3new(
  
) {
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS1273;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS3255;
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L6_2atmpS3254;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS3250;
  struct _M0TUsiE** _M0L7_2abindS1274;
  struct _M0TUsiE** _M0L6_2atmpS3253;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3252;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3251;
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _block_4000;
  #line 176 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1273
  = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3255 = _M0L7_2abindS1273;
  _M0L6_2atmpS3254
  = (struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE){
    .$0 = _M0L6_2atmpS3255, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3250
  = _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L6_2atmpS3254, 1000ll);
  moonbit_decref(_M0L6_2atmpS3254.$0);
  _M0L7_2abindS1274 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3253 = _M0L7_2abindS1274;
  _M0L6_2atmpS3252
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3253, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3251 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3252, 1000ll);
  moonbit_decref(_M0L6_2atmpS3252.$0);
  _block_4000
  = (struct _M0TP38JIA2JIA29moonbitdb3lib8Database*)moonbit_malloc(sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib8Database));
  Moonbit_object_header(_block_4000)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 31, 0);
  _block_4000->$0 = _M0L6_2atmpS3250;
  _block_4000->$1 = _M0L6_2atmpS3251;
  _block_4000->$2 = 0;
  return _block_4000;
}

struct _M0TPB5ArrayGsE* _M0MP38JIA2JIA29moonbitdb3lib5Deque9to__array(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1266
) {
  moonbit_string_t* _M0L6_2atmpS3249;
  struct _M0TPB5ArrayGsE* _M0L6resultS1264;
  struct _M0TPB5ArrayGsE* _M0L5frontS3246;
  int32_t _M0L6_2atmpS3245;
  int32_t _M0L6_2atmpS3244;
  int32_t _M0L1iS1265;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1268;
  int32_t _M0L7_2abindS1269;
  int32_t _M0L2__S1270;
  #line 48 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3249 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1264
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1264)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1264->$0 = _M0L6_2atmpS3249;
  _M0L6resultS1264->$1 = 0;
  _M0L5frontS3246 = _M0L4selfS1266->$0;
  moonbit_incref(_M0L5frontS3246);
  #line 50 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3245 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3246);
  moonbit_decref(_M0L5frontS3246);
  _M0L6_2atmpS3244 = _M0L6_2atmpS3245 - 1;
  _M0L1iS1265 = _M0L6_2atmpS3244;
  while (1) {
    if (_M0L1iS1265 >= 0) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3242 = _M0L4selfS1266->$0;
      moonbit_string_t _M0L6_2atmpS3241;
      int32_t _M0L6_2atmpS3243;
      moonbit_incref(_M0L5frontS3242);
      #line 51 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3241
      = _M0MPC15array5Array2atGsE(_M0L5frontS3242, _M0L1iS1265);
      moonbit_decref(_M0L5frontS3242);
      #line 51 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1264, _M0L6_2atmpS3241);
      moonbit_decref(_M0L6_2atmpS3241);
      _M0L6_2atmpS3243 = _M0L1iS1265 - 1;
      _M0L1iS1265 = _M0L6_2atmpS3243;
      continue;
    }
    break;
  }
  _M0L7_2abindS1268 = _M0L4selfS1266->$1;
  _M0L7_2abindS1269 = _M0L7_2abindS1268->$1;
  moonbit_incref(_M0L7_2abindS1268);
  _M0L2__S1270 = 0;
  while (1) {
    if (_M0L2__S1270 < _M0L7_2abindS1269) {
      moonbit_string_t* _M0L3bufS3248 = _M0L7_2abindS1268->$0;
      moonbit_string_t _M0L4itemS1271 =
        (moonbit_string_t)_M0L3bufS3248[_M0L2__S1270];
      int32_t _M0L6_2atmpS3247;
      moonbit_incref(_M0L4itemS1271);
      #line 54 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1264, _M0L4itemS1271);
      moonbit_decref(_M0L4itemS1271);
      _M0L6_2atmpS3247 = _M0L2__S1270 + 1;
      _M0L2__S1270 = _M0L6_2atmpS3247;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1268);
    }
    break;
  }
  return _M0L6resultS1264;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque6length(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1263
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3240;
  int32_t _M0L6_2atmpS3237;
  struct _M0TPB5ArrayGsE* _M0L4backS3239;
  int32_t _M0L6_2atmpS3238;
  #line 44 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3240 = _M0L4selfS1263->$0;
  moonbit_incref(_M0L5frontS3240);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3237 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3240);
  moonbit_decref(_M0L5frontS3240);
  _M0L4backS3239 = _M0L4selfS1263->$1;
  moonbit_incref(_M0L4backS3239);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3238 = _M0MPC15array5Array6lengthGsE(_M0L4backS3239);
  moonbit_decref(_M0L4backS3239);
  return _M0L6_2atmpS3237 + _M0L6_2atmpS3238;
}

int32_t _M0MP38JIA2JIA29moonbitdb3lib5Deque11push__front(
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0L4selfS1261,
  moonbit_string_t _M0L5valueS1262
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3236;
  #line 10 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3236 = _M0L4selfS1261->$0;
  moonbit_incref(_M0L5frontS3236);
  #line 11 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPC15array5Array4pushGsE(_M0L5frontS3236, _M0L5valueS1262);
  moonbit_decref(_M0L5frontS3236);
  return 0;
}

struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _M0MP38JIA2JIA29moonbitdb3lib5Deque3new(
  
) {
  moonbit_string_t* _M0L6_2atmpS3235;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3232;
  moonbit_string_t* _M0L6_2atmpS3234;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3233;
  struct _M0TP38JIA2JIA29moonbitdb3lib5Deque* _block_4003;
  #line 6 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3235 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3232
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3232)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS3232->$0 = _M0L6_2atmpS3235;
  _M0L6_2atmpS3232->$1 = 0;
  _M0L6_2atmpS3234 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3233
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3233)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS3233->$0 = _M0L6_2atmpS3234;
  _M0L6_2atmpS3233->$1 = 0;
  _block_4003
  = (struct _M0TP38JIA2JIA29moonbitdb3lib5Deque*)moonbit_malloc(sizeof(struct _M0TP38JIA2JIA29moonbitdb3lib5Deque));
  Moonbit_object_header(_block_4003)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 35, 0);
  _block_4003->$0 = _M0L6_2atmpS3232;
  _block_4003->$1 = _M0L6_2atmpS3233;
  return _block_4003;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1260) {
  double _M0L6_2atmpS3231;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3231 = (double)_M0L4selfS1260;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3231);
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1252,
  int32_t _M0L5indexS1253
) {
  int32_t _M0L3lenS1251;
  int32_t _if__result_4004;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1251 = _M0L4selfS1252->$1;
  if (_M0L5indexS1253 >= 0) {
    _if__result_4004 = _M0L5indexS1253 < _M0L3lenS1251;
  } else {
    _if__result_4004 = 0;
  }
  if (_if__result_4004) {
    moonbit_string_t* _M0L6_2atmpS3228;
    moonbit_string_t _M0L6_2atmpS3557;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3228 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1252);
    _M0L6_2atmpS3557 = (moonbit_string_t)_M0L6_2atmpS3228[_M0L5indexS1253];
    moonbit_incref(_M0L6_2atmpS3557);
    moonbit_decref(_M0L6_2atmpS3228);
    return _M0L6_2atmpS3557;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS1255,
  int32_t _M0L5indexS1256
) {
  int32_t _M0L3lenS1254;
  int32_t _if__result_4005;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1254 = _M0L4selfS1255->$1;
  if (_M0L5indexS1256 >= 0) {
    _if__result_4005 = _M0L5indexS1256 < _M0L3lenS1254;
  } else {
    _if__result_4005 = 0;
  }
  if (_if__result_4005) {
    moonbit_string_t* _M0L6_2atmpS3229;
    moonbit_string_t _M0L6_2atmpS3558;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3229 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS1255);
    _M0L6_2atmpS3558 = (moonbit_string_t)_M0L6_2atmpS3229[_M0L5indexS1256];
    if (_M0L6_2atmpS3558) {
      moonbit_incref(_M0L6_2atmpS3558);
    }
    moonbit_decref(_M0L6_2atmpS3229);
    return _M0L6_2atmpS3558;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TUsfE* _M0MPC15array5Array2atGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS1258,
  int32_t _M0L5indexS1259
) {
  int32_t _M0L3lenS1257;
  int32_t _if__result_4006;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1257 = _M0L4selfS1258->$1;
  if (_M0L5indexS1259 >= 0) {
    _if__result_4006 = _M0L5indexS1259 < _M0L3lenS1257;
  } else {
    _if__result_4006 = 0;
  }
  if (_if__result_4006) {
    struct _M0TUsfE** _M0L6_2atmpS3230;
    struct _M0TUsfE* _M0L6_2atmpS3559;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3230 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1258);
    _M0L6_2atmpS3559 = (struct _M0TUsfE*)_M0L6_2atmpS3230[_M0L5indexS1259];
    if (_M0L6_2atmpS3559) {
      moonbit_incref(_M0L6_2atmpS3559);
    }
    moonbit_decref(_M0L6_2atmpS3230);
    return _M0L6_2atmpS3559;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1250) {
  moonbit_string_t _M0L6_2atmpS3227;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3227
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1250);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3227);
  moonbit_decref(_M0L6_2atmpS3227);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1249) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1249);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1236) {
  uint64_t _M0L4bitsS1237;
  uint64_t _M0L6_2atmpS3226;
  uint64_t _M0L6_2atmpS3225;
  int32_t _M0L8ieeeSignS1238;
  uint64_t _M0L12ieeeMantissaS1239;
  uint64_t _M0L6_2atmpS3224;
  uint64_t _M0L6_2atmpS3223;
  int32_t _M0L12ieeeExponentS1240;
  int32_t _if__result_4007;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1241;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1242;
  moonbit_string_t _result_4009;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1236 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  }
  _M0L4bitsS1237 = *(int64_t*)&_M0L3valS1236;
  _M0L6_2atmpS3226 = _M0L4bitsS1237 >> 63;
  _M0L6_2atmpS3225 = _M0L6_2atmpS3226 & 1ull;
  _M0L8ieeeSignS1238 = _M0L6_2atmpS3225 != 0ull;
  _M0L12ieeeMantissaS1239 = _M0L4bitsS1237 & 4503599627370495ull;
  _M0L6_2atmpS3224 = _M0L4bitsS1237 >> 52;
  _M0L6_2atmpS3223 = _M0L6_2atmpS3224 & 2047ull;
  _M0L12ieeeExponentS1240 = (int32_t)_M0L6_2atmpS3223;
  if (_M0L12ieeeExponentS1240 == 2047) {
    _if__result_4007 = 1;
  } else if (_M0L12ieeeExponentS1240 == 0) {
    _if__result_4007 = _M0L12ieeeMantissaS1239 == 0ull;
  } else {
    _if__result_4007 = 0;
  }
  if (_if__result_4007) {
    int32_t _M0L6_2atmpS3214 = _M0L12ieeeExponentS1240 != 0;
    int32_t _M0L6_2atmpS3215 = _M0L12ieeeMantissaS1239 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1238, _M0L6_2atmpS3214, _M0L6_2atmpS3215);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1241
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1239, _M0L12ieeeExponentS1240);
  if (_M0L7_2abindS1241 == 0) {
    uint32_t _M0L6_2atmpS3216;
    if (_M0L7_2abindS1241) {
      moonbit_decref(_M0L7_2abindS1241);
    }
    _M0L6_2atmpS3216 = *(uint32_t*)&_M0L12ieeeExponentS1240;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1242 = _M0FPB3d2d(_M0L12ieeeMantissaS1239, _M0L6_2atmpS3216);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1243 = _M0L7_2abindS1241;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1244 = _M0L7_2aSomeS1243;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1245 = _M0L4_2afS1244;
    while (1) {
      uint64_t _M0L8mantissaS3222 = _M0L1xS1245->$0;
      uint64_t _M0L1qS1246 = _M0L8mantissaS3222 / 10ull;
      uint64_t _M0L8mantissaS3220 = _M0L1xS1245->$0;
      uint64_t _M0L6_2atmpS3221 = 10ull * _M0L1qS1246;
      uint64_t _M0L1rS1247 = _M0L8mantissaS3220 - _M0L6_2atmpS3221;
      int32_t _M0L8exponentS3219;
      int32_t _M0L6_2atmpS3218;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3217;
      if (_M0L1rS1247 != 0ull) {
        _M0L1vS1242 = _M0L1xS1245;
        break;
      }
      _M0L8exponentS3219 = _M0L1xS1245->$1;
      moonbit_decref(_M0L1xS1245);
      _M0L6_2atmpS3218 = _M0L8exponentS3219 + 1;
      _M0L6_2atmpS3217
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3217)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3217->$0 = _M0L1qS1246;
      _M0L6_2atmpS3217->$1 = _M0L6_2atmpS3218;
      _M0L1xS1245 = _M0L6_2atmpS3217;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4009 = _M0FPB9to__chars(_M0L1vS1242, _M0L8ieeeSignS1238);
  moonbit_decref(_M0L1vS1242);
  return _result_4009;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1231,
  int32_t _M0L12ieeeExponentS1233
) {
  uint64_t _M0L2m2S1230;
  int32_t _M0L6_2atmpS3213;
  int32_t _M0L2e2S1232;
  int32_t _M0L6_2atmpS3212;
  uint64_t _M0L6_2atmpS3211;
  uint64_t _M0L4maskS1234;
  uint64_t _M0L8fractionS1235;
  int32_t _M0L6_2atmpS3210;
  uint64_t _M0L6_2atmpS3209;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3208;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1230 = 4503599627370496ull | _M0L12ieeeMantissaS1231;
  _M0L6_2atmpS3213 = _M0L12ieeeExponentS1233 - 1023;
  _M0L2e2S1232 = _M0L6_2atmpS3213 - 52;
  if (_M0L2e2S1232 > 0) {
    return 0;
  }
  if (_M0L2e2S1232 < -52) {
    return 0;
  }
  _M0L6_2atmpS3212 = -_M0L2e2S1232;
  _M0L6_2atmpS3211 = 1ull << (_M0L6_2atmpS3212 & 63);
  _M0L4maskS1234 = _M0L6_2atmpS3211 - 1ull;
  _M0L8fractionS1235 = _M0L2m2S1230 & _M0L4maskS1234;
  if (_M0L8fractionS1235 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3210 = -_M0L2e2S1232;
  _M0L6_2atmpS3209 = _M0L2m2S1230 >> (_M0L6_2atmpS3210 & 63);
  _M0L6_2atmpS3208
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3208)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3208->$0 = _M0L6_2atmpS3209;
  _M0L6_2atmpS3208->$1 = 0;
  return _M0L6_2atmpS3208;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1198,
  int32_t _M0L4signS1196
) {
  int32_t _M0L6_2atmpS3207;
  moonbit_bytes_t _M0L6resultS1194;
  int32_t _M0Lm5indexS1195;
  uint64_t _M0L6outputS1197;
  int32_t _M0L7olengthS1199;
  int32_t _M0L8exponentS3206;
  int32_t _M0L6_2atmpS3205;
  int32_t _M0Lm3expS1200;
  int32_t _M0L6_2atmpS3204;
  int32_t _M0L6_2atmpS3202;
  int32_t _M0L18scientificNotationS1201;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3207 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1194
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3207);
  _M0Lm5indexS1195 = 0;
  if (_M0L4signS1196) {
    int32_t _M0L6_2atmpS3076 = _M0Lm5indexS1195;
    int32_t _M0L6_2atmpS3077;
    if (
      _M0L6_2atmpS3076 < 0
      || _M0L6_2atmpS3076 >= Moonbit_array_length(_M0L6resultS1194)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1194[_M0L6_2atmpS3076] = 45;
    _M0L6_2atmpS3077 = _M0Lm5indexS1195;
    _M0Lm5indexS1195 = _M0L6_2atmpS3077 + 1;
  }
  _M0L6outputS1197 = _M0L1vS1198->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1199 = _M0FPB17decimal__length17(_M0L6outputS1197);
  _M0L8exponentS3206 = _M0L1vS1198->$1;
  _M0L6_2atmpS3205 = _M0L8exponentS3206 + _M0L7olengthS1199;
  _M0Lm3expS1200 = _M0L6_2atmpS3205 - 1;
  _M0L6_2atmpS3204 = _M0Lm3expS1200;
  if (_M0L6_2atmpS3204 >= -6) {
    int32_t _M0L6_2atmpS3203 = _M0Lm3expS1200;
    _M0L6_2atmpS3202 = _M0L6_2atmpS3203 < 21;
  } else {
    _M0L6_2atmpS3202 = 0;
  }
  _M0L18scientificNotationS1201 = !_M0L6_2atmpS3202;
  if (_M0L18scientificNotationS1201) {
    int32_t _M0L7_2abindS1202 = _M0L7olengthS1199 - 1;
    uint64_t _M0L6outputS1203;
    int32_t _M0L1iS1204 = 0;
    uint64_t _M0L6outputS1205 = _M0L6outputS1197;
    int32_t _M0L6_2atmpS3078;
    int32_t _M0L6_2atmpS3082;
    int32_t _M0L6_2atmpS3081;
    int32_t _M0L6_2atmpS3080;
    int32_t _M0L6_2atmpS3079;
    int32_t _M0L6_2atmpS3086;
    int32_t _M0L6_2atmpS3087;
    int32_t _M0L6_2atmpS3088;
    int32_t _M0L6_2atmpS3089;
    int32_t _M0L6_2atmpS3090;
    int32_t _M0L6_2atmpS3096;
    int32_t _M0L6_2atmpS3129;
    moonbit_string_t _result_4011;
    while (1) {
      if (_M0L1iS1204 < _M0L7_2abindS1202) {
        uint64_t _M0L1cS1206 = _M0L6outputS1205 % 10ull;
        int32_t _M0L6_2atmpS3135 = _M0Lm5indexS1195;
        int32_t _M0L6_2atmpS3134 = _M0L6_2atmpS3135 + _M0L7olengthS1199;
        int32_t _M0L6_2atmpS3130 = _M0L6_2atmpS3134 - _M0L1iS1204;
        int32_t _M0L6_2atmpS3133 = (int32_t)_M0L1cS1206;
        int32_t _M0L6_2atmpS3132 = 48 + _M0L6_2atmpS3133;
        int32_t _M0L6_2atmpS3131 = _M0L6_2atmpS3132 & 0xff;
        int32_t _M0L6_2atmpS3136;
        uint64_t _M0L6_2atmpS3137;
        if (
          _M0L6_2atmpS3130 < 0
          || _M0L6_2atmpS3130 >= Moonbit_array_length(_M0L6resultS1194)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1194[_M0L6_2atmpS3130] = _M0L6_2atmpS3131;
        _M0L6_2atmpS3136 = _M0L1iS1204 + 1;
        _M0L6_2atmpS3137 = _M0L6outputS1205 / 10ull;
        _M0L1iS1204 = _M0L6_2atmpS3136;
        _M0L6outputS1205 = _M0L6_2atmpS3137;
        continue;
      } else {
        _M0L6outputS1203 = _M0L6outputS1205;
      }
      break;
    }
    _M0L6_2atmpS3078 = _M0Lm5indexS1195;
    _M0L6_2atmpS3082 = (int32_t)_M0L6outputS1203;
    _M0L6_2atmpS3081 = _M0L6_2atmpS3082 % 10;
    _M0L6_2atmpS3080 = 48 + _M0L6_2atmpS3081;
    _M0L6_2atmpS3079 = _M0L6_2atmpS3080 & 0xff;
    if (
      _M0L6_2atmpS3078 < 0
      || _M0L6_2atmpS3078 >= Moonbit_array_length(_M0L6resultS1194)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1194[_M0L6_2atmpS3078] = _M0L6_2atmpS3079;
    if (_M0L7olengthS1199 > 1) {
      int32_t _M0L6_2atmpS3084 = _M0Lm5indexS1195;
      int32_t _M0L6_2atmpS3083 = _M0L6_2atmpS3084 + 1;
      if (
        _M0L6_2atmpS3083 < 0
        || _M0L6_2atmpS3083 >= Moonbit_array_length(_M0L6resultS1194)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1194[_M0L6_2atmpS3083] = 46;
    } else {
      int32_t _M0L6_2atmpS3085 = _M0Lm5indexS1195;
      _M0Lm5indexS1195 = _M0L6_2atmpS3085 - 1;
    }
    _M0L6_2atmpS3086 = _M0Lm5indexS1195;
    _M0L6_2atmpS3087 = _M0L7olengthS1199 + 1;
    _M0Lm5indexS1195 = _M0L6_2atmpS3086 + _M0L6_2atmpS3087;
    _M0L6_2atmpS3088 = _M0Lm5indexS1195;
    if (
      _M0L6_2atmpS3088 < 0
      || _M0L6_2atmpS3088 >= Moonbit_array_length(_M0L6resultS1194)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1194[_M0L6_2atmpS3088] = 101;
    _M0L6_2atmpS3089 = _M0Lm5indexS1195;
    _M0Lm5indexS1195 = _M0L6_2atmpS3089 + 1;
    _M0L6_2atmpS3090 = _M0Lm3expS1200;
    if (_M0L6_2atmpS3090 < 0) {
      int32_t _M0L6_2atmpS3091 = _M0Lm5indexS1195;
      int32_t _M0L6_2atmpS3092;
      int32_t _M0L6_2atmpS3093;
      if (
        _M0L6_2atmpS3091 < 0
        || _M0L6_2atmpS3091 >= Moonbit_array_length(_M0L6resultS1194)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1194[_M0L6_2atmpS3091] = 45;
      _M0L6_2atmpS3092 = _M0Lm5indexS1195;
      _M0Lm5indexS1195 = _M0L6_2atmpS3092 + 1;
      _M0L6_2atmpS3093 = _M0Lm3expS1200;
      _M0Lm3expS1200 = -_M0L6_2atmpS3093;
    } else {
      int32_t _M0L6_2atmpS3094 = _M0Lm5indexS1195;
      int32_t _M0L6_2atmpS3095;
      if (
        _M0L6_2atmpS3094 < 0
        || _M0L6_2atmpS3094 >= Moonbit_array_length(_M0L6resultS1194)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1194[_M0L6_2atmpS3094] = 43;
      _M0L6_2atmpS3095 = _M0Lm5indexS1195;
      _M0Lm5indexS1195 = _M0L6_2atmpS3095 + 1;
    }
    _M0L6_2atmpS3096 = _M0Lm3expS1200;
    if (_M0L6_2atmpS3096 >= 100) {
      int32_t _M0L6_2atmpS3112 = _M0Lm3expS1200;
      int32_t _M0L1aS1208 = _M0L6_2atmpS3112 / 100;
      int32_t _M0L6_2atmpS3111 = _M0Lm3expS1200;
      int32_t _M0L6_2atmpS3110 = _M0L6_2atmpS3111 / 10;
      int32_t _M0L1bS1209 = _M0L6_2atmpS3110 % 10;
      int32_t _M0L6_2atmpS3109 = _M0Lm3expS1200;
      int32_t _M0L1cS1210 = _M0L6_2atmpS3109 % 10;
      int32_t _M0L6_2atmpS3097 = _M0Lm5indexS1195;
      int32_t _M0L6_2atmpS3099 = 48 + _M0L1aS1208;
      int32_t _M0L6_2atmpS3098 = _M0L6_2atmpS3099 & 0xff;
      int32_t _M0L6_2atmpS3103;
      int32_t _M0L6_2atmpS3100;
      int32_t _M0L6_2atmpS3102;
      int32_t _M0L6_2atmpS3101;
      int32_t _M0L6_2atmpS3107;
      int32_t _M0L6_2atmpS3104;
      int32_t _M0L6_2atmpS3106;
      int32_t _M0L6_2atmpS3105;
      int32_t _M0L6_2atmpS3108;
      if (
        _M0L6_2atmpS3097 < 0
        || _M0L6_2atmpS3097 >= Moonbit_array_length(_M0L6resultS1194)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1194[_M0L6_2atmpS3097] = _M0L6_2atmpS3098;
      _M0L6_2atmpS3103 = _M0Lm5indexS1195;
      _M0L6_2atmpS3100 = _M0L6_2atmpS3103 + 1;
      _M0L6_2atmpS3102 = 48 + _M0L1bS1209;
      _M0L6_2atmpS3101 = _M0L6_2atmpS3102 & 0xff;
      if (
        _M0L6_2atmpS3100 < 0
        || _M0L6_2atmpS3100 >= Moonbit_array_length(_M0L6resultS1194)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1194[_M0L6_2atmpS3100] = _M0L6_2atmpS3101;
      _M0L6_2atmpS3107 = _M0Lm5indexS1195;
      _M0L6_2atmpS3104 = _M0L6_2atmpS3107 + 2;
      _M0L6_2atmpS3106 = 48 + _M0L1cS1210;
      _M0L6_2atmpS3105 = _M0L6_2atmpS3106 & 0xff;
      if (
        _M0L6_2atmpS3104 < 0
        || _M0L6_2atmpS3104 >= Moonbit_array_length(_M0L6resultS1194)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1194[_M0L6_2atmpS3104] = _M0L6_2atmpS3105;
      _M0L6_2atmpS3108 = _M0Lm5indexS1195;
      _M0Lm5indexS1195 = _M0L6_2atmpS3108 + 3;
    } else {
      int32_t _M0L6_2atmpS3113 = _M0Lm3expS1200;
      if (_M0L6_2atmpS3113 >= 10) {
        int32_t _M0L6_2atmpS3123 = _M0Lm3expS1200;
        int32_t _M0L1aS1211 = _M0L6_2atmpS3123 / 10;
        int32_t _M0L6_2atmpS3122 = _M0Lm3expS1200;
        int32_t _M0L1bS1212 = _M0L6_2atmpS3122 % 10;
        int32_t _M0L6_2atmpS3114 = _M0Lm5indexS1195;
        int32_t _M0L6_2atmpS3116 = 48 + _M0L1aS1211;
        int32_t _M0L6_2atmpS3115 = _M0L6_2atmpS3116 & 0xff;
        int32_t _M0L6_2atmpS3120;
        int32_t _M0L6_2atmpS3117;
        int32_t _M0L6_2atmpS3119;
        int32_t _M0L6_2atmpS3118;
        int32_t _M0L6_2atmpS3121;
        if (
          _M0L6_2atmpS3114 < 0
          || _M0L6_2atmpS3114 >= Moonbit_array_length(_M0L6resultS1194)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1194[_M0L6_2atmpS3114] = _M0L6_2atmpS3115;
        _M0L6_2atmpS3120 = _M0Lm5indexS1195;
        _M0L6_2atmpS3117 = _M0L6_2atmpS3120 + 1;
        _M0L6_2atmpS3119 = 48 + _M0L1bS1212;
        _M0L6_2atmpS3118 = _M0L6_2atmpS3119 & 0xff;
        if (
          _M0L6_2atmpS3117 < 0
          || _M0L6_2atmpS3117 >= Moonbit_array_length(_M0L6resultS1194)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1194[_M0L6_2atmpS3117] = _M0L6_2atmpS3118;
        _M0L6_2atmpS3121 = _M0Lm5indexS1195;
        _M0Lm5indexS1195 = _M0L6_2atmpS3121 + 2;
      } else {
        int32_t _M0L6_2atmpS3124 = _M0Lm5indexS1195;
        int32_t _M0L6_2atmpS3127 = _M0Lm3expS1200;
        int32_t _M0L6_2atmpS3126 = 48 + _M0L6_2atmpS3127;
        int32_t _M0L6_2atmpS3125 = _M0L6_2atmpS3126 & 0xff;
        int32_t _M0L6_2atmpS3128;
        if (
          _M0L6_2atmpS3124 < 0
          || _M0L6_2atmpS3124 >= Moonbit_array_length(_M0L6resultS1194)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1194[_M0L6_2atmpS3124] = _M0L6_2atmpS3125;
        _M0L6_2atmpS3128 = _M0Lm5indexS1195;
        _M0Lm5indexS1195 = _M0L6_2atmpS3128 + 1;
      }
    }
    _M0L6_2atmpS3129 = _M0Lm5indexS1195;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4011
    = _M0FPB19string__from__bytes(_M0L6resultS1194, 0, _M0L6_2atmpS3129);
    moonbit_decref(_M0L6resultS1194);
    return _result_4011;
  } else {
    int32_t _M0L6_2atmpS3138 = _M0Lm3expS1200;
    int32_t _M0L6_2atmpS3201;
    moonbit_string_t _result_4017;
    if (_M0L6_2atmpS3138 < 0) {
      int32_t _M0L6_2atmpS3139 = _M0Lm5indexS1195;
      int32_t _M0L6_2atmpS3141;
      int32_t _M0L6_2atmpS3140;
      int32_t _M0L6_2atmpS3142;
      int32_t _M0L1iS1213;
      int32_t _M0L6_2atmpS3157;
      int32_t _M0L6_2atmpS3159;
      int32_t _M0L6_2atmpS3158;
      int32_t _M0L7currentS1215;
      int32_t _M0L1iS1216;
      uint64_t _M0L6outputS1217;
      if (
        _M0L6_2atmpS3139 < 0
        || _M0L6_2atmpS3139 >= Moonbit_array_length(_M0L6resultS1194)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1194[_M0L6_2atmpS3139] = 48;
      _M0L6_2atmpS3141 = _M0Lm5indexS1195;
      _M0L6_2atmpS3140 = _M0L6_2atmpS3141 + 1;
      if (
        _M0L6_2atmpS3140 < 0
        || _M0L6_2atmpS3140 >= Moonbit_array_length(_M0L6resultS1194)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1194[_M0L6_2atmpS3140] = 46;
      _M0L6_2atmpS3142 = _M0Lm5indexS1195;
      _M0Lm5indexS1195 = _M0L6_2atmpS3142 + 2;
      _M0L1iS1213 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3143 = _M0Lm3expS1200;
        if (_M0L1iS1213 > _M0L6_2atmpS3143) {
          int32_t _M0L6_2atmpS3146 = _M0Lm5indexS1195;
          int32_t _M0L6_2atmpS3145 = _M0L6_2atmpS3146 - _M0L1iS1213;
          int32_t _M0L6_2atmpS3144 = _M0L6_2atmpS3145 - 1;
          int32_t _M0L6_2atmpS3147;
          if (
            _M0L6_2atmpS3144 < 0
            || _M0L6_2atmpS3144 >= Moonbit_array_length(_M0L6resultS1194)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1194[_M0L6_2atmpS3144] = 48;
          _M0L6_2atmpS3147 = _M0L1iS1213 - 1;
          _M0L1iS1213 = _M0L6_2atmpS3147;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3157 = _M0Lm5indexS1195;
      _M0L6_2atmpS3159 = _M0Lm3expS1200;
      _M0L6_2atmpS3158 = -1 - _M0L6_2atmpS3159;
      _M0L7currentS1215 = _M0L6_2atmpS3157 + _M0L6_2atmpS3158;
      _M0L1iS1216 = 0;
      _M0L6outputS1217 = _M0L6outputS1197;
      while (1) {
        if (_M0L1iS1216 < _M0L7olengthS1199) {
          int32_t _M0L6_2atmpS3154 = _M0L7currentS1215 + _M0L7olengthS1199;
          int32_t _M0L6_2atmpS3153 = _M0L6_2atmpS3154 - _M0L1iS1216;
          int32_t _M0L6_2atmpS3148 = _M0L6_2atmpS3153 - 1;
          uint64_t _M0L6_2atmpS3152 = _M0L6outputS1217 % 10ull;
          int32_t _M0L6_2atmpS3151 = (int32_t)_M0L6_2atmpS3152;
          int32_t _M0L6_2atmpS3150 = 48 + _M0L6_2atmpS3151;
          int32_t _M0L6_2atmpS3149 = _M0L6_2atmpS3150 & 0xff;
          int32_t _M0L6_2atmpS3155;
          uint64_t _M0L6_2atmpS3156;
          if (
            _M0L6_2atmpS3148 < 0
            || _M0L6_2atmpS3148 >= Moonbit_array_length(_M0L6resultS1194)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1194[_M0L6_2atmpS3148] = _M0L6_2atmpS3149;
          _M0L6_2atmpS3155 = _M0L1iS1216 + 1;
          _M0L6_2atmpS3156 = _M0L6outputS1217 / 10ull;
          _M0L1iS1216 = _M0L6_2atmpS3155;
          _M0L6outputS1217 = _M0L6_2atmpS3156;
          continue;
        }
        break;
      }
      _M0Lm5indexS1195 = _M0L7currentS1215 + _M0L7olengthS1199;
    } else {
      int32_t _M0L6_2atmpS3161 = _M0Lm3expS1200;
      int32_t _M0L6_2atmpS3160 = _M0L6_2atmpS3161 + 1;
      if (_M0L6_2atmpS3160 >= _M0L7olengthS1199) {
        int32_t _M0L1iS1219 = 0;
        uint64_t _M0L6outputS1220 = _M0L6outputS1197;
        int32_t _M0L6_2atmpS3172;
        int32_t _M0L6_2atmpS3177;
        int32_t _M0L7_2abindS1222;
        int32_t _M0L1iS1223;
        int32_t _M0L6_2atmpS3178;
        int32_t _M0L6_2atmpS3181;
        int32_t _M0L6_2atmpS3180;
        int32_t _M0L6_2atmpS3179;
        while (1) {
          if (_M0L1iS1219 < _M0L7olengthS1199) {
            int32_t _M0L6_2atmpS3169 = _M0Lm5indexS1195;
            int32_t _M0L6_2atmpS3168 = _M0L6_2atmpS3169 + _M0L7olengthS1199;
            int32_t _M0L6_2atmpS3167 = _M0L6_2atmpS3168 - _M0L1iS1219;
            int32_t _M0L6_2atmpS3162 = _M0L6_2atmpS3167 - 1;
            uint64_t _M0L6_2atmpS3166 = _M0L6outputS1220 % 10ull;
            int32_t _M0L6_2atmpS3165 = (int32_t)_M0L6_2atmpS3166;
            int32_t _M0L6_2atmpS3164 = 48 + _M0L6_2atmpS3165;
            int32_t _M0L6_2atmpS3163 = _M0L6_2atmpS3164 & 0xff;
            int32_t _M0L6_2atmpS3170;
            uint64_t _M0L6_2atmpS3171;
            if (
              _M0L6_2atmpS3162 < 0
              || _M0L6_2atmpS3162 >= Moonbit_array_length(_M0L6resultS1194)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1194[_M0L6_2atmpS3162] = _M0L6_2atmpS3163;
            _M0L6_2atmpS3170 = _M0L1iS1219 + 1;
            _M0L6_2atmpS3171 = _M0L6outputS1220 / 10ull;
            _M0L1iS1219 = _M0L6_2atmpS3170;
            _M0L6outputS1220 = _M0L6_2atmpS3171;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3172 = _M0Lm5indexS1195;
        _M0Lm5indexS1195 = _M0L6_2atmpS3172 + _M0L7olengthS1199;
        _M0L6_2atmpS3177 = _M0Lm3expS1200;
        _M0L7_2abindS1222 = _M0L6_2atmpS3177 + 1;
        _M0L1iS1223 = _M0L7olengthS1199;
        while (1) {
          if (_M0L1iS1223 < _M0L7_2abindS1222) {
            int32_t _M0L6_2atmpS3175 = _M0Lm5indexS1195;
            int32_t _M0L6_2atmpS3174 = _M0L6_2atmpS3175 + _M0L1iS1223;
            int32_t _M0L6_2atmpS3173 = _M0L6_2atmpS3174 - _M0L7olengthS1199;
            int32_t _M0L6_2atmpS3176;
            if (
              _M0L6_2atmpS3173 < 0
              || _M0L6_2atmpS3173 >= Moonbit_array_length(_M0L6resultS1194)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1194[_M0L6_2atmpS3173] = 48;
            _M0L6_2atmpS3176 = _M0L1iS1223 + 1;
            _M0L1iS1223 = _M0L6_2atmpS3176;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3178 = _M0Lm5indexS1195;
        _M0L6_2atmpS3181 = _M0Lm3expS1200;
        _M0L6_2atmpS3180 = _M0L6_2atmpS3181 + 1;
        _M0L6_2atmpS3179 = _M0L6_2atmpS3180 - _M0L7olengthS1199;
        _M0Lm5indexS1195 = _M0L6_2atmpS3178 + _M0L6_2atmpS3179;
      } else {
        int32_t _M0L6_2atmpS3198 = _M0Lm5indexS1195;
        int32_t _M0L6_2atmpS3197 = _M0L6_2atmpS3198 + 1;
        int32_t _M0L1iS1225 = 0;
        int32_t _M0L7currentS1226 = _M0L6_2atmpS3197;
        uint64_t _M0L6outputS1227 = _M0L6outputS1197;
        int32_t _M0L6_2atmpS3199;
        int32_t _M0L6_2atmpS3200;
        while (1) {
          if (_M0L1iS1225 < _M0L7olengthS1199) {
            int32_t _M0L6_2atmpS3193 = _M0L7olengthS1199 - _M0L1iS1225;
            int32_t _M0L6_2atmpS3191 = _M0L6_2atmpS3193 - 1;
            int32_t _M0L6_2atmpS3192 = _M0Lm3expS1200;
            int32_t _M0L7currentS1228;
            int32_t _M0L6_2atmpS3188;
            int32_t _M0L6_2atmpS3187;
            int32_t _M0L6_2atmpS3182;
            uint64_t _M0L6_2atmpS3186;
            int32_t _M0L6_2atmpS3185;
            int32_t _M0L6_2atmpS3184;
            int32_t _M0L6_2atmpS3183;
            int32_t _M0L6_2atmpS3189;
            uint64_t _M0L6_2atmpS3190;
            if (_M0L6_2atmpS3191 == _M0L6_2atmpS3192) {
              int32_t _M0L6_2atmpS3196 =
                _M0L7currentS1226 + _M0L7olengthS1199;
              int32_t _M0L6_2atmpS3195 = _M0L6_2atmpS3196 - _M0L1iS1225;
              int32_t _M0L6_2atmpS3194 = _M0L6_2atmpS3195 - 1;
              if (
                _M0L6_2atmpS3194 < 0
                || _M0L6_2atmpS3194 >= Moonbit_array_length(_M0L6resultS1194)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1194[_M0L6_2atmpS3194] = 46;
              _M0L7currentS1228 = _M0L7currentS1226 - 1;
            } else {
              _M0L7currentS1228 = _M0L7currentS1226;
            }
            _M0L6_2atmpS3188 = _M0L7currentS1228 + _M0L7olengthS1199;
            _M0L6_2atmpS3187 = _M0L6_2atmpS3188 - _M0L1iS1225;
            _M0L6_2atmpS3182 = _M0L6_2atmpS3187 - 1;
            _M0L6_2atmpS3186 = _M0L6outputS1227 % 10ull;
            _M0L6_2atmpS3185 = (int32_t)_M0L6_2atmpS3186;
            _M0L6_2atmpS3184 = 48 + _M0L6_2atmpS3185;
            _M0L6_2atmpS3183 = _M0L6_2atmpS3184 & 0xff;
            if (
              _M0L6_2atmpS3182 < 0
              || _M0L6_2atmpS3182 >= Moonbit_array_length(_M0L6resultS1194)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1194[_M0L6_2atmpS3182] = _M0L6_2atmpS3183;
            _M0L6_2atmpS3189 = _M0L1iS1225 + 1;
            _M0L6_2atmpS3190 = _M0L6outputS1227 / 10ull;
            _M0L1iS1225 = _M0L6_2atmpS3189;
            _M0L7currentS1226 = _M0L7currentS1228;
            _M0L6outputS1227 = _M0L6_2atmpS3190;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3199 = _M0Lm5indexS1195;
        _M0L6_2atmpS3200 = _M0L7olengthS1199 + 1;
        _M0Lm5indexS1195 = _M0L6_2atmpS3199 + _M0L6_2atmpS3200;
      }
    }
    _M0L6_2atmpS3201 = _M0Lm5indexS1195;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4017
    = _M0FPB19string__from__bytes(_M0L6resultS1194, 0, _M0L6_2atmpS3201);
    moonbit_decref(_M0L6resultS1194);
    return _result_4017;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1140,
  uint32_t _M0L12ieeeExponentS1139
) {
  int32_t _M0Lm2e2S1137;
  uint64_t _M0Lm2m2S1138;
  uint64_t _M0L6_2atmpS3075;
  uint64_t _M0L6_2atmpS3074;
  int32_t _M0L4evenS1141;
  uint64_t _M0L6_2atmpS3073;
  uint64_t _M0L2mvS1142;
  int32_t _M0L7mmShiftS1143;
  uint64_t _M0Lm2vrS1144;
  uint64_t _M0Lm2vpS1145;
  uint64_t _M0Lm2vmS1146;
  int32_t _M0Lm3e10S1147;
  int32_t _M0Lm17vmIsTrailingZerosS1148;
  int32_t _M0Lm17vrIsTrailingZerosS1149;
  int32_t _M0L6_2atmpS2975;
  int32_t _M0Lm7removedS1168;
  int32_t _M0Lm16lastRemovedDigitS1169;
  uint64_t _M0Lm6outputS1170;
  int32_t _M0L6_2atmpS3071;
  int32_t _M0L6_2atmpS3072;
  int32_t _M0L3expS1193;
  uint64_t _M0L6_2atmpS3070;
  struct _M0TPB17FloatingDecimal64* _block_4023;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1137 = 0;
  _M0Lm2m2S1138 = 0ull;
  if (_M0L12ieeeExponentS1139 == 0u) {
    _M0Lm2e2S1137 = -1076;
    _M0Lm2m2S1138 = _M0L12ieeeMantissaS1140;
  } else {
    int32_t _M0L6_2atmpS2974 = *(int32_t*)&_M0L12ieeeExponentS1139;
    int32_t _M0L6_2atmpS2973 = _M0L6_2atmpS2974 - 1023;
    int32_t _M0L6_2atmpS2972 = _M0L6_2atmpS2973 - 52;
    _M0Lm2e2S1137 = _M0L6_2atmpS2972 - 2;
    _M0Lm2m2S1138 = 4503599627370496ull | _M0L12ieeeMantissaS1140;
  }
  _M0L6_2atmpS3075 = _M0Lm2m2S1138;
  _M0L6_2atmpS3074 = _M0L6_2atmpS3075 & 1ull;
  _M0L4evenS1141 = _M0L6_2atmpS3074 == 0ull;
  _M0L6_2atmpS3073 = _M0Lm2m2S1138;
  _M0L2mvS1142 = 4ull * _M0L6_2atmpS3073;
  if (_M0L12ieeeMantissaS1140 != 0ull) {
    _M0L7mmShiftS1143 = 1;
  } else {
    _M0L7mmShiftS1143 = _M0L12ieeeExponentS1139 <= 1u;
  }
  _M0Lm2vrS1144 = 0ull;
  _M0Lm2vpS1145 = 0ull;
  _M0Lm2vmS1146 = 0ull;
  _M0Lm3e10S1147 = 0;
  _M0Lm17vmIsTrailingZerosS1148 = 0;
  _M0Lm17vrIsTrailingZerosS1149 = 0;
  _M0L6_2atmpS2975 = _M0Lm2e2S1137;
  if (_M0L6_2atmpS2975 >= 0) {
    int32_t _M0L6_2atmpS2997 = _M0Lm2e2S1137;
    int32_t _M0L6_2atmpS2993;
    int32_t _M0L6_2atmpS2996;
    int32_t _M0L6_2atmpS2995;
    int32_t _M0L6_2atmpS2994;
    int32_t _M0L1qS1150;
    int32_t _M0L6_2atmpS2992;
    int32_t _M0L6_2atmpS2991;
    int32_t _M0L1kS1151;
    int32_t _M0L6_2atmpS2990;
    int32_t _M0L6_2atmpS2989;
    int32_t _M0L6_2atmpS2988;
    int32_t _M0L1iS1152;
    struct _M0TPB8Pow5Pair _M0L4pow5S1153;
    uint64_t _M0L6_2atmpS2987;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1154;
    uint64_t _M0L8_2avrOutS1155;
    uint64_t _M0L8_2avpOutS1156;
    uint64_t _M0L8_2avmOutS1157;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2993 = _M0FPB9log10Pow2(_M0L6_2atmpS2997);
    _M0L6_2atmpS2996 = _M0Lm2e2S1137;
    _M0L6_2atmpS2995 = _M0L6_2atmpS2996 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2994 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2995);
    _M0L1qS1150 = _M0L6_2atmpS2993 - _M0L6_2atmpS2994;
    _M0Lm3e10S1147 = _M0L1qS1150;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2992 = _M0FPB8pow5bits(_M0L1qS1150);
    _M0L6_2atmpS2991 = 125 + _M0L6_2atmpS2992;
    _M0L1kS1151 = _M0L6_2atmpS2991 - 1;
    _M0L6_2atmpS2990 = _M0Lm2e2S1137;
    _M0L6_2atmpS2989 = -_M0L6_2atmpS2990;
    _M0L6_2atmpS2988 = _M0L6_2atmpS2989 + _M0L1qS1150;
    _M0L1iS1152 = _M0L6_2atmpS2988 + _M0L1kS1151;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1153 = _M0FPB22double__computeInvPow5(_M0L1qS1150);
    _M0L6_2atmpS2987 = _M0Lm2m2S1138;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1154
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2987, _M0L4pow5S1153, _M0L1iS1152, _M0L7mmShiftS1143);
    _M0L8_2avrOutS1155 = _M0L7_2abindS1154.$0;
    _M0L8_2avpOutS1156 = _M0L7_2abindS1154.$1;
    _M0L8_2avmOutS1157 = _M0L7_2abindS1154.$2;
    _M0Lm2vrS1144 = _M0L8_2avrOutS1155;
    _M0Lm2vpS1145 = _M0L8_2avpOutS1156;
    _M0Lm2vmS1146 = _M0L8_2avmOutS1157;
    if (_M0L1qS1150 <= 21) {
      int32_t _M0L6_2atmpS2983 = (int32_t)_M0L2mvS1142;
      uint64_t _M0L6_2atmpS2986 = _M0L2mvS1142 / 5ull;
      int32_t _M0L6_2atmpS2985 = (int32_t)_M0L6_2atmpS2986;
      int32_t _M0L6_2atmpS2984 = 5 * _M0L6_2atmpS2985;
      int32_t _M0L6mvMod5S1158 = _M0L6_2atmpS2983 - _M0L6_2atmpS2984;
      if (_M0L6mvMod5S1158 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1149
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1142, _M0L1qS1150);
      } else if (_M0L4evenS1141) {
        uint64_t _M0L6_2atmpS2977 = _M0L2mvS1142 - 1ull;
        uint64_t _M0L6_2atmpS2978;
        uint64_t _M0L6_2atmpS2976;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2978 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1143);
        _M0L6_2atmpS2976 = _M0L6_2atmpS2977 - _M0L6_2atmpS2978;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1148
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2976, _M0L1qS1150);
      } else {
        uint64_t _M0L6_2atmpS2979 = _M0Lm2vpS1145;
        uint64_t _M0L6_2atmpS2982 = _M0L2mvS1142 + 2ull;
        int32_t _M0L6_2atmpS2981;
        uint64_t _M0L6_2atmpS2980;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2981
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2982, _M0L1qS1150);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2980 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2981);
        _M0Lm2vpS1145 = _M0L6_2atmpS2979 - _M0L6_2atmpS2980;
      }
    }
  } else {
    int32_t _M0L6_2atmpS3011 = _M0Lm2e2S1137;
    int32_t _M0L6_2atmpS3010 = -_M0L6_2atmpS3011;
    int32_t _M0L6_2atmpS3005;
    int32_t _M0L6_2atmpS3009;
    int32_t _M0L6_2atmpS3008;
    int32_t _M0L6_2atmpS3007;
    int32_t _M0L6_2atmpS3006;
    int32_t _M0L1qS1159;
    int32_t _M0L6_2atmpS2998;
    int32_t _M0L6_2atmpS3004;
    int32_t _M0L6_2atmpS3003;
    int32_t _M0L1iS1160;
    int32_t _M0L6_2atmpS3002;
    int32_t _M0L1kS1161;
    int32_t _M0L1jS1162;
    struct _M0TPB8Pow5Pair _M0L4pow5S1163;
    uint64_t _M0L6_2atmpS3001;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1164;
    uint64_t _M0L8_2avrOutS1165;
    uint64_t _M0L8_2avpOutS1166;
    uint64_t _M0L8_2avmOutS1167;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3005 = _M0FPB9log10Pow5(_M0L6_2atmpS3010);
    _M0L6_2atmpS3009 = _M0Lm2e2S1137;
    _M0L6_2atmpS3008 = -_M0L6_2atmpS3009;
    _M0L6_2atmpS3007 = _M0L6_2atmpS3008 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3006 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3007);
    _M0L1qS1159 = _M0L6_2atmpS3005 - _M0L6_2atmpS3006;
    _M0L6_2atmpS2998 = _M0Lm2e2S1137;
    _M0Lm3e10S1147 = _M0L1qS1159 + _M0L6_2atmpS2998;
    _M0L6_2atmpS3004 = _M0Lm2e2S1137;
    _M0L6_2atmpS3003 = -_M0L6_2atmpS3004;
    _M0L1iS1160 = _M0L6_2atmpS3003 - _M0L1qS1159;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3002 = _M0FPB8pow5bits(_M0L1iS1160);
    _M0L1kS1161 = _M0L6_2atmpS3002 - 125;
    _M0L1jS1162 = _M0L1qS1159 - _M0L1kS1161;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1163 = _M0FPB19double__computePow5(_M0L1iS1160);
    _M0L6_2atmpS3001 = _M0Lm2m2S1138;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1164
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3001, _M0L4pow5S1163, _M0L1jS1162, _M0L7mmShiftS1143);
    _M0L8_2avrOutS1165 = _M0L7_2abindS1164.$0;
    _M0L8_2avpOutS1166 = _M0L7_2abindS1164.$1;
    _M0L8_2avmOutS1167 = _M0L7_2abindS1164.$2;
    _M0Lm2vrS1144 = _M0L8_2avrOutS1165;
    _M0Lm2vpS1145 = _M0L8_2avpOutS1166;
    _M0Lm2vmS1146 = _M0L8_2avmOutS1167;
    if (_M0L1qS1159 <= 1) {
      _M0Lm17vrIsTrailingZerosS1149 = 1;
      if (_M0L4evenS1141) {
        int32_t _M0L6_2atmpS2999;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2999 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1143);
        _M0Lm17vmIsTrailingZerosS1148 = _M0L6_2atmpS2999 == 1;
      } else {
        uint64_t _M0L6_2atmpS3000 = _M0Lm2vpS1145;
        _M0Lm2vpS1145 = _M0L6_2atmpS3000 - 1ull;
      }
    } else if (_M0L1qS1159 < 63) {
      #line 437 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1149
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1142, _M0L1qS1159);
    }
  }
  _M0Lm7removedS1168 = 0;
  _M0Lm16lastRemovedDigitS1169 = 0;
  _M0Lm6outputS1170 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1148 || _M0Lm17vrIsTrailingZerosS1149) {
    int32_t _if__result_4020;
    uint64_t _M0L6_2atmpS3041;
    uint64_t _M0L6_2atmpS3047;
    uint64_t _M0L6_2atmpS3048;
    int32_t _if__result_4021;
    int32_t _M0L6_2atmpS3044;
    int64_t _M0L6_2atmpS3043;
    uint64_t _M0L6_2atmpS3042;
    while (1) {
      uint64_t _M0L6_2atmpS3024 = _M0Lm2vpS1145;
      uint64_t _M0L7vpDiv10S1171 = _M0L6_2atmpS3024 / 10ull;
      uint64_t _M0L6_2atmpS3023 = _M0Lm2vmS1146;
      uint64_t _M0L7vmDiv10S1172 = _M0L6_2atmpS3023 / 10ull;
      uint64_t _M0L6_2atmpS3022;
      int32_t _M0L6_2atmpS3019;
      int32_t _M0L6_2atmpS3021;
      int32_t _M0L6_2atmpS3020;
      int32_t _M0L7vmMod10S1174;
      uint64_t _M0L6_2atmpS3018;
      uint64_t _M0L7vrDiv10S1175;
      uint64_t _M0L6_2atmpS3017;
      int32_t _M0L6_2atmpS3014;
      int32_t _M0L6_2atmpS3016;
      int32_t _M0L6_2atmpS3015;
      int32_t _M0L7vrMod10S1176;
      int32_t _M0L6_2atmpS3013;
      if (_M0L7vpDiv10S1171 <= _M0L7vmDiv10S1172) {
        break;
      }
      _M0L6_2atmpS3022 = _M0Lm2vmS1146;
      _M0L6_2atmpS3019 = (int32_t)_M0L6_2atmpS3022;
      _M0L6_2atmpS3021 = (int32_t)_M0L7vmDiv10S1172;
      _M0L6_2atmpS3020 = 10 * _M0L6_2atmpS3021;
      _M0L7vmMod10S1174 = _M0L6_2atmpS3019 - _M0L6_2atmpS3020;
      _M0L6_2atmpS3018 = _M0Lm2vrS1144;
      _M0L7vrDiv10S1175 = _M0L6_2atmpS3018 / 10ull;
      _M0L6_2atmpS3017 = _M0Lm2vrS1144;
      _M0L6_2atmpS3014 = (int32_t)_M0L6_2atmpS3017;
      _M0L6_2atmpS3016 = (int32_t)_M0L7vrDiv10S1175;
      _M0L6_2atmpS3015 = 10 * _M0L6_2atmpS3016;
      _M0L7vrMod10S1176 = _M0L6_2atmpS3014 - _M0L6_2atmpS3015;
      if (_M0Lm17vmIsTrailingZerosS1148) {
        _M0Lm17vmIsTrailingZerosS1148 = _M0L7vmMod10S1174 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1148 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1149) {
        int32_t _M0L6_2atmpS3012 = _M0Lm16lastRemovedDigitS1169;
        _M0Lm17vrIsTrailingZerosS1149 = _M0L6_2atmpS3012 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1149 = 0;
      }
      _M0Lm16lastRemovedDigitS1169 = _M0L7vrMod10S1176;
      _M0Lm2vrS1144 = _M0L7vrDiv10S1175;
      _M0Lm2vpS1145 = _M0L7vpDiv10S1171;
      _M0Lm2vmS1146 = _M0L7vmDiv10S1172;
      _M0L6_2atmpS3013 = _M0Lm7removedS1168;
      _M0Lm7removedS1168 = _M0L6_2atmpS3013 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1148) {
      while (1) {
        uint64_t _M0L6_2atmpS3037 = _M0Lm2vmS1146;
        uint64_t _M0L7vmDiv10S1177 = _M0L6_2atmpS3037 / 10ull;
        uint64_t _M0L6_2atmpS3036 = _M0Lm2vmS1146;
        int32_t _M0L6_2atmpS3033 = (int32_t)_M0L6_2atmpS3036;
        int32_t _M0L6_2atmpS3035 = (int32_t)_M0L7vmDiv10S1177;
        int32_t _M0L6_2atmpS3034 = 10 * _M0L6_2atmpS3035;
        int32_t _M0L7vmMod10S1178 = _M0L6_2atmpS3033 - _M0L6_2atmpS3034;
        uint64_t _M0L6_2atmpS3032;
        uint64_t _M0L7vpDiv10S1180;
        uint64_t _M0L6_2atmpS3031;
        uint64_t _M0L7vrDiv10S1181;
        uint64_t _M0L6_2atmpS3030;
        int32_t _M0L6_2atmpS3027;
        int32_t _M0L6_2atmpS3029;
        int32_t _M0L6_2atmpS3028;
        int32_t _M0L7vrMod10S1182;
        int32_t _M0L6_2atmpS3026;
        if (_M0L7vmMod10S1178 != 0) {
          break;
        }
        _M0L6_2atmpS3032 = _M0Lm2vpS1145;
        _M0L7vpDiv10S1180 = _M0L6_2atmpS3032 / 10ull;
        _M0L6_2atmpS3031 = _M0Lm2vrS1144;
        _M0L7vrDiv10S1181 = _M0L6_2atmpS3031 / 10ull;
        _M0L6_2atmpS3030 = _M0Lm2vrS1144;
        _M0L6_2atmpS3027 = (int32_t)_M0L6_2atmpS3030;
        _M0L6_2atmpS3029 = (int32_t)_M0L7vrDiv10S1181;
        _M0L6_2atmpS3028 = 10 * _M0L6_2atmpS3029;
        _M0L7vrMod10S1182 = _M0L6_2atmpS3027 - _M0L6_2atmpS3028;
        if (_M0Lm17vrIsTrailingZerosS1149) {
          int32_t _M0L6_2atmpS3025 = _M0Lm16lastRemovedDigitS1169;
          _M0Lm17vrIsTrailingZerosS1149 = _M0L6_2atmpS3025 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1149 = 0;
        }
        _M0Lm16lastRemovedDigitS1169 = _M0L7vrMod10S1182;
        _M0Lm2vrS1144 = _M0L7vrDiv10S1181;
        _M0Lm2vpS1145 = _M0L7vpDiv10S1180;
        _M0Lm2vmS1146 = _M0L7vmDiv10S1177;
        _M0L6_2atmpS3026 = _M0Lm7removedS1168;
        _M0Lm7removedS1168 = _M0L6_2atmpS3026 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1149) {
      int32_t _M0L6_2atmpS3040 = _M0Lm16lastRemovedDigitS1169;
      if (_M0L6_2atmpS3040 == 5) {
        uint64_t _M0L6_2atmpS3039 = _M0Lm2vrS1144;
        uint64_t _M0L6_2atmpS3038 = _M0L6_2atmpS3039 % 2ull;
        _if__result_4020 = _M0L6_2atmpS3038 == 0ull;
      } else {
        _if__result_4020 = 0;
      }
    } else {
      _if__result_4020 = 0;
    }
    if (_if__result_4020) {
      _M0Lm16lastRemovedDigitS1169 = 4;
    }
    _M0L6_2atmpS3041 = _M0Lm2vrS1144;
    _M0L6_2atmpS3047 = _M0Lm2vrS1144;
    _M0L6_2atmpS3048 = _M0Lm2vmS1146;
    if (_M0L6_2atmpS3047 == _M0L6_2atmpS3048) {
      if (!_M0L4evenS1141) {
        _if__result_4021 = 1;
      } else {
        int32_t _M0L6_2atmpS3046 = _M0Lm17vmIsTrailingZerosS1148;
        _if__result_4021 = !_M0L6_2atmpS3046;
      }
    } else {
      _if__result_4021 = 0;
    }
    if (_if__result_4021) {
      _M0L6_2atmpS3044 = 1;
    } else {
      int32_t _M0L6_2atmpS3045 = _M0Lm16lastRemovedDigitS1169;
      _M0L6_2atmpS3044 = _M0L6_2atmpS3045 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3043 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3044);
    _M0L6_2atmpS3042 = *(uint64_t*)&_M0L6_2atmpS3043;
    _M0Lm6outputS1170 = _M0L6_2atmpS3041 + _M0L6_2atmpS3042;
  } else {
    int32_t _M0Lm7roundUpS1183 = 0;
    uint64_t _M0L6_2atmpS3069 = _M0Lm2vpS1145;
    uint64_t _M0L8vpDiv100S1184 = _M0L6_2atmpS3069 / 100ull;
    uint64_t _M0L6_2atmpS3068 = _M0Lm2vmS1146;
    uint64_t _M0L8vmDiv100S1185 = _M0L6_2atmpS3068 / 100ull;
    uint64_t _M0L6_2atmpS3063;
    uint64_t _M0L6_2atmpS3066;
    uint64_t _M0L6_2atmpS3067;
    int32_t _M0L6_2atmpS3065;
    uint64_t _M0L6_2atmpS3064;
    if (_M0L8vpDiv100S1184 > _M0L8vmDiv100S1185) {
      uint64_t _M0L6_2atmpS3054 = _M0Lm2vrS1144;
      uint64_t _M0L8vrDiv100S1186 = _M0L6_2atmpS3054 / 100ull;
      uint64_t _M0L6_2atmpS3053 = _M0Lm2vrS1144;
      int32_t _M0L6_2atmpS3050 = (int32_t)_M0L6_2atmpS3053;
      int32_t _M0L6_2atmpS3052 = (int32_t)_M0L8vrDiv100S1186;
      int32_t _M0L6_2atmpS3051 = 100 * _M0L6_2atmpS3052;
      int32_t _M0L8vrMod100S1187 = _M0L6_2atmpS3050 - _M0L6_2atmpS3051;
      int32_t _M0L6_2atmpS3049;
      _M0Lm7roundUpS1183 = _M0L8vrMod100S1187 >= 50;
      _M0Lm2vrS1144 = _M0L8vrDiv100S1186;
      _M0Lm2vpS1145 = _M0L8vpDiv100S1184;
      _M0Lm2vmS1146 = _M0L8vmDiv100S1185;
      _M0L6_2atmpS3049 = _M0Lm7removedS1168;
      _M0Lm7removedS1168 = _M0L6_2atmpS3049 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3062 = _M0Lm2vpS1145;
      uint64_t _M0L7vpDiv10S1188 = _M0L6_2atmpS3062 / 10ull;
      uint64_t _M0L6_2atmpS3061 = _M0Lm2vmS1146;
      uint64_t _M0L7vmDiv10S1189 = _M0L6_2atmpS3061 / 10ull;
      uint64_t _M0L6_2atmpS3060;
      uint64_t _M0L7vrDiv10S1191;
      uint64_t _M0L6_2atmpS3059;
      int32_t _M0L6_2atmpS3056;
      int32_t _M0L6_2atmpS3058;
      int32_t _M0L6_2atmpS3057;
      int32_t _M0L7vrMod10S1192;
      int32_t _M0L6_2atmpS3055;
      if (_M0L7vpDiv10S1188 <= _M0L7vmDiv10S1189) {
        break;
      }
      _M0L6_2atmpS3060 = _M0Lm2vrS1144;
      _M0L7vrDiv10S1191 = _M0L6_2atmpS3060 / 10ull;
      _M0L6_2atmpS3059 = _M0Lm2vrS1144;
      _M0L6_2atmpS3056 = (int32_t)_M0L6_2atmpS3059;
      _M0L6_2atmpS3058 = (int32_t)_M0L7vrDiv10S1191;
      _M0L6_2atmpS3057 = 10 * _M0L6_2atmpS3058;
      _M0L7vrMod10S1192 = _M0L6_2atmpS3056 - _M0L6_2atmpS3057;
      _M0Lm7roundUpS1183 = _M0L7vrMod10S1192 >= 5;
      _M0Lm2vrS1144 = _M0L7vrDiv10S1191;
      _M0Lm2vpS1145 = _M0L7vpDiv10S1188;
      _M0Lm2vmS1146 = _M0L7vmDiv10S1189;
      _M0L6_2atmpS3055 = _M0Lm7removedS1168;
      _M0Lm7removedS1168 = _M0L6_2atmpS3055 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3063 = _M0Lm2vrS1144;
    _M0L6_2atmpS3066 = _M0Lm2vrS1144;
    _M0L6_2atmpS3067 = _M0Lm2vmS1146;
    _M0L6_2atmpS3065
    = _M0L6_2atmpS3066 == _M0L6_2atmpS3067 || _M0Lm7roundUpS1183;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3064 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3065);
    _M0Lm6outputS1170 = _M0L6_2atmpS3063 + _M0L6_2atmpS3064;
  }
  _M0L6_2atmpS3071 = _M0Lm3e10S1147;
  _M0L6_2atmpS3072 = _M0Lm7removedS1168;
  _M0L3expS1193 = _M0L6_2atmpS3071 + _M0L6_2atmpS3072;
  _M0L6_2atmpS3070 = _M0Lm6outputS1170;
  _block_4023
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4023)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_4023->$0 = _M0L6_2atmpS3070;
  _block_4023->$1 = _M0L3expS1193;
  return _block_4023;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1136) {
  #line 110 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1136) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1135) {
  #line 58 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1135) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1134) {
  #line 32 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1134) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1133) {
  #line 280 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L1vS1133 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1133 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1133 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1133 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1133 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1133 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1133 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1133 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1133 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1133 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1133 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1133 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1133 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1133 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1133 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1133 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1116) {
  int32_t _M0L6_2atmpS2971;
  int32_t _M0L6_2atmpS2970;
  int32_t _M0L4baseS1115;
  int32_t _M0L5base2S1117;
  int32_t _M0L6offsetS1118;
  int32_t _M0L6_2atmpS2969;
  uint64_t _M0L4mul0S1119;
  int32_t _M0L6_2atmpS2968;
  int32_t _M0L6_2atmpS2967;
  uint64_t _M0L4mul1S1120;
  uint64_t _M0L1mS1121;
  struct _M0TPB7Umul128 _M0L7_2abindS1122;
  uint64_t _M0L7_2alow1S1123;
  uint64_t _M0L8_2ahigh1S1124;
  struct _M0TPB7Umul128 _M0L7_2abindS1125;
  uint64_t _M0L7_2alow0S1126;
  uint64_t _M0L8_2ahigh0S1127;
  uint64_t _M0L3sumS1128;
  uint64_t _M0Lm5high1S1129;
  int32_t _M0L6_2atmpS2965;
  int32_t _M0L6_2atmpS2966;
  int32_t _M0L5deltaS1130;
  uint64_t _M0L6_2atmpS2964;
  uint64_t _M0L6_2atmpS2956;
  int32_t _M0L6_2atmpS2963;
  uint32_t _M0L6_2atmpS2960;
  int32_t _M0L6_2atmpS2962;
  int32_t _M0L6_2atmpS2961;
  uint32_t _M0L6_2atmpS2959;
  uint32_t _M0L6_2atmpS2958;
  uint64_t _M0L6_2atmpS2957;
  uint64_t _M0L1aS1131;
  uint64_t _M0L6_2atmpS2955;
  uint64_t _M0L1bS1132;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2971 = _M0L1iS1116 + 26;
  _M0L6_2atmpS2970 = _M0L6_2atmpS2971 - 1;
  _M0L4baseS1115 = _M0L6_2atmpS2970 / 26;
  _M0L5base2S1117 = _M0L4baseS1115 * 26;
  _M0L6offsetS1118 = _M0L5base2S1117 - _M0L1iS1116;
  _M0L6_2atmpS2969 = _M0L4baseS1115 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1119
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2969);
  _M0L6_2atmpS2968 = _M0L4baseS1115 * 2;
  _M0L6_2atmpS2967 = _M0L6_2atmpS2968 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1120
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2967);
  if (_M0L6offsetS1118 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1119,
                                      .$1 = _M0L4mul1S1120};
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1121
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1118);
  #line 249 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1122 = _M0FPB7umul128(_M0L1mS1121, _M0L4mul1S1120);
  _M0L7_2alow1S1123 = _M0L7_2abindS1122.$0;
  _M0L8_2ahigh1S1124 = _M0L7_2abindS1122.$1;
  #line 250 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1125 = _M0FPB7umul128(_M0L1mS1121, _M0L4mul0S1119);
  _M0L7_2alow0S1126 = _M0L7_2abindS1125.$0;
  _M0L8_2ahigh0S1127 = _M0L7_2abindS1125.$1;
  _M0L3sumS1128 = _M0L8_2ahigh0S1127 + _M0L7_2alow1S1123;
  _M0Lm5high1S1129 = _M0L8_2ahigh1S1124;
  if (_M0L3sumS1128 < _M0L8_2ahigh0S1127) {
    uint64_t _M0L6_2atmpS2954 = _M0Lm5high1S1129;
    _M0Lm5high1S1129 = _M0L6_2atmpS2954 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2965 = _M0FPB8pow5bits(_M0L5base2S1117);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2966 = _M0FPB8pow5bits(_M0L1iS1116);
  _M0L5deltaS1130 = _M0L6_2atmpS2965 - _M0L6_2atmpS2966;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2964
  = _M0FPB13shiftright128(_M0L7_2alow0S1126, _M0L3sumS1128, _M0L5deltaS1130);
  _M0L6_2atmpS2956 = _M0L6_2atmpS2964 + 1ull;
  _M0L6_2atmpS2963 = _M0L1iS1116 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2960
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2963);
  _M0L6_2atmpS2962 = _M0L1iS1116 % 16;
  _M0L6_2atmpS2961 = _M0L6_2atmpS2962 << 1;
  _M0L6_2atmpS2959 = _M0L6_2atmpS2960 >> (_M0L6_2atmpS2961 & 31);
  _M0L6_2atmpS2958 = _M0L6_2atmpS2959 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2957 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2958);
  _M0L1aS1131 = _M0L6_2atmpS2956 + _M0L6_2atmpS2957;
  _M0L6_2atmpS2955 = _M0Lm5high1S1129;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1132
  = _M0FPB13shiftright128(_M0L3sumS1128, _M0L6_2atmpS2955, _M0L5deltaS1130);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1131, .$1 = _M0L1bS1132};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1098) {
  int32_t _M0L4baseS1097;
  int32_t _M0L5base2S1099;
  int32_t _M0L6offsetS1100;
  int32_t _M0L6_2atmpS2953;
  uint64_t _M0L4mul0S1101;
  int32_t _M0L6_2atmpS2952;
  int32_t _M0L6_2atmpS2951;
  uint64_t _M0L4mul1S1102;
  uint64_t _M0L1mS1103;
  struct _M0TPB7Umul128 _M0L7_2abindS1104;
  uint64_t _M0L7_2alow1S1105;
  uint64_t _M0L8_2ahigh1S1106;
  struct _M0TPB7Umul128 _M0L7_2abindS1107;
  uint64_t _M0L7_2alow0S1108;
  uint64_t _M0L8_2ahigh0S1109;
  uint64_t _M0L3sumS1110;
  uint64_t _M0Lm5high1S1111;
  int32_t _M0L6_2atmpS2949;
  int32_t _M0L6_2atmpS2950;
  int32_t _M0L5deltaS1112;
  uint64_t _M0L6_2atmpS2941;
  int32_t _M0L6_2atmpS2948;
  uint32_t _M0L6_2atmpS2945;
  int32_t _M0L6_2atmpS2947;
  int32_t _M0L6_2atmpS2946;
  uint32_t _M0L6_2atmpS2944;
  uint32_t _M0L6_2atmpS2943;
  uint64_t _M0L6_2atmpS2942;
  uint64_t _M0L1aS1113;
  uint64_t _M0L6_2atmpS2940;
  uint64_t _M0L1bS1114;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1097 = _M0L1iS1098 / 26;
  _M0L5base2S1099 = _M0L4baseS1097 * 26;
  _M0L6offsetS1100 = _M0L1iS1098 - _M0L5base2S1099;
  _M0L6_2atmpS2953 = _M0L4baseS1097 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1101
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2953);
  _M0L6_2atmpS2952 = _M0L4baseS1097 * 2;
  _M0L6_2atmpS2951 = _M0L6_2atmpS2952 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1102
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2951);
  if (_M0L6offsetS1100 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1101,
                                      .$1 = _M0L4mul1S1102};
  }
  #line 222 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1103
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1100);
  #line 223 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1104 = _M0FPB7umul128(_M0L1mS1103, _M0L4mul1S1102);
  _M0L7_2alow1S1105 = _M0L7_2abindS1104.$0;
  _M0L8_2ahigh1S1106 = _M0L7_2abindS1104.$1;
  #line 224 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1107 = _M0FPB7umul128(_M0L1mS1103, _M0L4mul0S1101);
  _M0L7_2alow0S1108 = _M0L7_2abindS1107.$0;
  _M0L8_2ahigh0S1109 = _M0L7_2abindS1107.$1;
  _M0L3sumS1110 = _M0L8_2ahigh0S1109 + _M0L7_2alow1S1105;
  _M0Lm5high1S1111 = _M0L8_2ahigh1S1106;
  if (_M0L3sumS1110 < _M0L8_2ahigh0S1109) {
    uint64_t _M0L6_2atmpS2939 = _M0Lm5high1S1111;
    _M0Lm5high1S1111 = _M0L6_2atmpS2939 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2949 = _M0FPB8pow5bits(_M0L1iS1098);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2950 = _M0FPB8pow5bits(_M0L5base2S1099);
  _M0L5deltaS1112 = _M0L6_2atmpS2949 - _M0L6_2atmpS2950;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2941
  = _M0FPB13shiftright128(_M0L7_2alow0S1108, _M0L3sumS1110, _M0L5deltaS1112);
  _M0L6_2atmpS2948 = _M0L1iS1098 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2945
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2948);
  _M0L6_2atmpS2947 = _M0L1iS1098 % 16;
  _M0L6_2atmpS2946 = _M0L6_2atmpS2947 << 1;
  _M0L6_2atmpS2944 = _M0L6_2atmpS2945 >> (_M0L6_2atmpS2946 & 31);
  _M0L6_2atmpS2943 = _M0L6_2atmpS2944 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2942 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2943);
  _M0L1aS1113 = _M0L6_2atmpS2941 + _M0L6_2atmpS2942;
  _M0L6_2atmpS2940 = _M0Lm5high1S1111;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1114
  = _M0FPB13shiftright128(_M0L3sumS1110, _M0L6_2atmpS2940, _M0L5deltaS1112);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1113, .$1 = _M0L1bS1114};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1071,
  struct _M0TPB8Pow5Pair _M0L3mulS1068,
  int32_t _M0L1jS1084,
  int32_t _M0L7mmShiftS1086
) {
  uint64_t _M0L7_2amul0S1067;
  uint64_t _M0L7_2amul1S1069;
  uint64_t _M0L1mS1070;
  struct _M0TPB7Umul128 _M0L7_2abindS1072;
  uint64_t _M0L5_2aloS1073;
  uint64_t _M0L6_2atmpS1074;
  struct _M0TPB7Umul128 _M0L7_2abindS1075;
  uint64_t _M0L6_2alo2S1076;
  uint64_t _M0L6_2ahi2S1077;
  uint64_t _M0L3midS1078;
  uint64_t _M0L6_2atmpS2938;
  uint64_t _M0L2hiS1079;
  uint64_t _M0L3lo2S1080;
  uint64_t _M0L6_2atmpS2936;
  uint64_t _M0L6_2atmpS2937;
  uint64_t _M0L4mid2S1081;
  uint64_t _M0L6_2atmpS2935;
  uint64_t _M0L3hi2S1082;
  int32_t _M0L6_2atmpS2934;
  int32_t _M0L6_2atmpS2933;
  uint64_t _M0L2vpS1083;
  uint64_t _M0Lm2vmS1085;
  int32_t _M0L6_2atmpS2932;
  int32_t _M0L6_2atmpS2931;
  uint64_t _M0L2vrS1096;
  uint64_t _M0L6_2atmpS2930;
  #line 129 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2amul0S1067 = _M0L3mulS1068.$0;
  _M0L7_2amul1S1069 = _M0L3mulS1068.$1;
  _M0L1mS1070 = _M0L1mS1071 << 1;
  #line 137 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1072 = _M0FPB7umul128(_M0L1mS1070, _M0L7_2amul0S1067);
  _M0L5_2aloS1073 = _M0L7_2abindS1072.$0;
  _M0L6_2atmpS1074 = _M0L7_2abindS1072.$1;
  #line 138 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1075 = _M0FPB7umul128(_M0L1mS1070, _M0L7_2amul1S1069);
  _M0L6_2alo2S1076 = _M0L7_2abindS1075.$0;
  _M0L6_2ahi2S1077 = _M0L7_2abindS1075.$1;
  _M0L3midS1078 = _M0L6_2atmpS1074 + _M0L6_2alo2S1076;
  if (_M0L3midS1078 < _M0L6_2atmpS1074) {
    _M0L6_2atmpS2938 = 1ull;
  } else {
    _M0L6_2atmpS2938 = 0ull;
  }
  _M0L2hiS1079 = _M0L6_2ahi2S1077 + _M0L6_2atmpS2938;
  _M0L3lo2S1080 = _M0L5_2aloS1073 + _M0L7_2amul0S1067;
  _M0L6_2atmpS2936 = _M0L3midS1078 + _M0L7_2amul1S1069;
  if (_M0L3lo2S1080 < _M0L5_2aloS1073) {
    _M0L6_2atmpS2937 = 1ull;
  } else {
    _M0L6_2atmpS2937 = 0ull;
  }
  _M0L4mid2S1081 = _M0L6_2atmpS2936 + _M0L6_2atmpS2937;
  if (_M0L4mid2S1081 < _M0L3midS1078) {
    _M0L6_2atmpS2935 = 1ull;
  } else {
    _M0L6_2atmpS2935 = 0ull;
  }
  _M0L3hi2S1082 = _M0L2hiS1079 + _M0L6_2atmpS2935;
  _M0L6_2atmpS2934 = _M0L1jS1084 - 64;
  _M0L6_2atmpS2933 = _M0L6_2atmpS2934 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1083
  = _M0FPB13shiftright128(_M0L4mid2S1081, _M0L3hi2S1082, _M0L6_2atmpS2933);
  _M0Lm2vmS1085 = 0ull;
  if (_M0L7mmShiftS1086) {
    uint64_t _M0L3lo3S1087 = _M0L5_2aloS1073 - _M0L7_2amul0S1067;
    uint64_t _M0L6_2atmpS2920 = _M0L3midS1078 - _M0L7_2amul1S1069;
    uint64_t _M0L6_2atmpS2921;
    uint64_t _M0L4mid3S1088;
    uint64_t _M0L6_2atmpS2919;
    uint64_t _M0L3hi3S1089;
    int32_t _M0L6_2atmpS2918;
    int32_t _M0L6_2atmpS2917;
    if (_M0L5_2aloS1073 < _M0L3lo3S1087) {
      _M0L6_2atmpS2921 = 1ull;
    } else {
      _M0L6_2atmpS2921 = 0ull;
    }
    _M0L4mid3S1088 = _M0L6_2atmpS2920 - _M0L6_2atmpS2921;
    if (_M0L3midS1078 < _M0L4mid3S1088) {
      _M0L6_2atmpS2919 = 1ull;
    } else {
      _M0L6_2atmpS2919 = 0ull;
    }
    _M0L3hi3S1089 = _M0L2hiS1079 - _M0L6_2atmpS2919;
    _M0L6_2atmpS2918 = _M0L1jS1084 - 64;
    _M0L6_2atmpS2917 = _M0L6_2atmpS2918 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1085
    = _M0FPB13shiftright128(_M0L4mid3S1088, _M0L3hi3S1089, _M0L6_2atmpS2917);
  } else {
    uint64_t _M0L3lo3S1090 = _M0L5_2aloS1073 + _M0L5_2aloS1073;
    uint64_t _M0L6_2atmpS2928 = _M0L3midS1078 + _M0L3midS1078;
    uint64_t _M0L6_2atmpS2929;
    uint64_t _M0L4mid3S1091;
    uint64_t _M0L6_2atmpS2926;
    uint64_t _M0L6_2atmpS2927;
    uint64_t _M0L3hi3S1092;
    uint64_t _M0L3lo4S1093;
    uint64_t _M0L6_2atmpS2924;
    uint64_t _M0L6_2atmpS2925;
    uint64_t _M0L4mid4S1094;
    uint64_t _M0L6_2atmpS2923;
    uint64_t _M0L3hi4S1095;
    int32_t _M0L6_2atmpS2922;
    if (_M0L3lo3S1090 < _M0L5_2aloS1073) {
      _M0L6_2atmpS2929 = 1ull;
    } else {
      _M0L6_2atmpS2929 = 0ull;
    }
    _M0L4mid3S1091 = _M0L6_2atmpS2928 + _M0L6_2atmpS2929;
    _M0L6_2atmpS2926 = _M0L2hiS1079 + _M0L2hiS1079;
    if (_M0L4mid3S1091 < _M0L3midS1078) {
      _M0L6_2atmpS2927 = 1ull;
    } else {
      _M0L6_2atmpS2927 = 0ull;
    }
    _M0L3hi3S1092 = _M0L6_2atmpS2926 + _M0L6_2atmpS2927;
    _M0L3lo4S1093 = _M0L3lo3S1090 - _M0L7_2amul0S1067;
    _M0L6_2atmpS2924 = _M0L4mid3S1091 - _M0L7_2amul1S1069;
    if (_M0L3lo3S1090 < _M0L3lo4S1093) {
      _M0L6_2atmpS2925 = 1ull;
    } else {
      _M0L6_2atmpS2925 = 0ull;
    }
    _M0L4mid4S1094 = _M0L6_2atmpS2924 - _M0L6_2atmpS2925;
    if (_M0L4mid3S1091 < _M0L4mid4S1094) {
      _M0L6_2atmpS2923 = 1ull;
    } else {
      _M0L6_2atmpS2923 = 0ull;
    }
    _M0L3hi4S1095 = _M0L3hi3S1092 - _M0L6_2atmpS2923;
    _M0L6_2atmpS2922 = _M0L1jS1084 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1085
    = _M0FPB13shiftright128(_M0L4mid4S1094, _M0L3hi4S1095, _M0L6_2atmpS2922);
  }
  _M0L6_2atmpS2932 = _M0L1jS1084 - 64;
  _M0L6_2atmpS2931 = _M0L6_2atmpS2932 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1096
  = _M0FPB13shiftright128(_M0L3midS1078, _M0L2hiS1079, _M0L6_2atmpS2931);
  _M0L6_2atmpS2930 = _M0Lm2vmS1085;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1096,
                                                .$1 = _M0L2vpS1083,
                                                .$2 = _M0L6_2atmpS2930};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1065,
  int32_t _M0L1pS1066
) {
  uint64_t _M0L6_2atmpS2916;
  uint64_t _M0L6_2atmpS2915;
  uint64_t _M0L6_2atmpS2914;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2916 = 1ull << (_M0L1pS1066 & 63);
  _M0L6_2atmpS2915 = _M0L6_2atmpS2916 - 1ull;
  _M0L6_2atmpS2914 = _M0L5valueS1065 & _M0L6_2atmpS2915;
  return _M0L6_2atmpS2914 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1063,
  int32_t _M0L1pS1064
) {
  int32_t _M0L6_2atmpS2913;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2913 = _M0FPB10pow5Factor(_M0L5valueS1063);
  return _M0L6_2atmpS2913 >= _M0L1pS1064;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1058) {
  uint64_t _M0L6_2atmpS2904;
  uint64_t _M0L6_2atmpS2905;
  uint64_t _M0L6_2atmpS2906;
  uint64_t _M0L6_2atmpS2907;
  uint64_t _M0L6_2atmpS2912;
  int32_t _M0L5countS1059;
  uint64_t _M0L1vS1060;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2904 = _M0L5valueS1058 % 5ull;
  if (_M0L6_2atmpS2904 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2905 = _M0L5valueS1058 % 25ull;
  if (_M0L6_2atmpS2905 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2906 = _M0L5valueS1058 % 125ull;
  if (_M0L6_2atmpS2906 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2907 = _M0L5valueS1058 % 625ull;
  if (_M0L6_2atmpS2907 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS2912 = _M0L5valueS1058 / 625ull;
  _M0L5countS1059 = 4;
  _M0L1vS1060 = _M0L6_2atmpS2912;
  while (1) {
    if (_M0L1vS1060 > 0ull) {
      uint64_t _M0L6_2atmpS2908 = _M0L1vS1060 % 5ull;
      int32_t _M0L6_2atmpS2909;
      uint64_t _M0L6_2atmpS2910;
      if (_M0L6_2atmpS2908 != 0ull) {
        return _M0L5countS1059;
      }
      _M0L6_2atmpS2909 = _M0L5countS1059 + 1;
      _M0L6_2atmpS2910 = _M0L1vS1060 / 5ull;
      _M0L5countS1059 = _M0L6_2atmpS2909;
      _M0L1vS1060 = _M0L6_2atmpS2910;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1062;
      moonbit_string_t _M0L6_2atmpS2911;
      int32_t _result_4025;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1062
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1062, (moonbit_string_t)moonbit_string_literal_2.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1062, _M0L5valueS1058);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS2911
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1062);
      moonbit_decref(_M0L18_2astring__builderS1062);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_4025 = _M0FPC15abort5abortGiE(_M0L6_2atmpS2911);
      moonbit_decref(_M0L6_2atmpS2911);
      return _result_4025;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1057,
  uint64_t _M0L2hiS1055,
  int32_t _M0L4distS1056
) {
  int32_t _M0L6_2atmpS2903;
  uint64_t _M0L6_2atmpS2901;
  uint64_t _M0L6_2atmpS2902;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2903 = 64 - _M0L4distS1056;
  _M0L6_2atmpS2901 = _M0L2hiS1055 << (_M0L6_2atmpS2903 & 63);
  _M0L6_2atmpS2902 = _M0L2loS1057 >> (_M0L4distS1056 & 63);
  return _M0L6_2atmpS2901 | _M0L6_2atmpS2902;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS1045,
  uint64_t _M0L1bS1048
) {
  uint64_t _M0L3aLoS1044;
  uint64_t _M0L3aHiS1046;
  uint64_t _M0L3bLoS1047;
  uint64_t _M0L3bHiS1049;
  uint64_t _M0L1xS1050;
  uint64_t _M0L6_2atmpS2899;
  uint64_t _M0L6_2atmpS2900;
  uint64_t _M0L1yS1051;
  uint64_t _M0L6_2atmpS2897;
  uint64_t _M0L6_2atmpS2898;
  uint64_t _M0L1zS1052;
  uint64_t _M0L6_2atmpS2895;
  uint64_t _M0L6_2atmpS2896;
  uint64_t _M0L6_2atmpS2893;
  uint64_t _M0L6_2atmpS2894;
  uint64_t _M0L1wS1053;
  uint64_t _M0L2loS1054;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1044 = _M0L1aS1045 & 4294967295ull;
  _M0L3aHiS1046 = _M0L1aS1045 >> 32;
  _M0L3bLoS1047 = _M0L1bS1048 & 4294967295ull;
  _M0L3bHiS1049 = _M0L1bS1048 >> 32;
  _M0L1xS1050 = _M0L3aLoS1044 * _M0L3bLoS1047;
  _M0L6_2atmpS2899 = _M0L3aHiS1046 * _M0L3bLoS1047;
  _M0L6_2atmpS2900 = _M0L1xS1050 >> 32;
  _M0L1yS1051 = _M0L6_2atmpS2899 + _M0L6_2atmpS2900;
  _M0L6_2atmpS2897 = _M0L3aLoS1044 * _M0L3bHiS1049;
  _M0L6_2atmpS2898 = _M0L1yS1051 & 4294967295ull;
  _M0L1zS1052 = _M0L6_2atmpS2897 + _M0L6_2atmpS2898;
  _M0L6_2atmpS2895 = _M0L3aHiS1046 * _M0L3bHiS1049;
  _M0L6_2atmpS2896 = _M0L1yS1051 >> 32;
  _M0L6_2atmpS2893 = _M0L6_2atmpS2895 + _M0L6_2atmpS2896;
  _M0L6_2atmpS2894 = _M0L1zS1052 >> 32;
  _M0L1wS1053 = _M0L6_2atmpS2893 + _M0L6_2atmpS2894;
  _M0L2loS1054 = _M0L1aS1045 * _M0L1bS1048;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1054, .$1 = _M0L1wS1053};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1042,
  int32_t _M0L4fromS1039,
  int32_t _M0L2toS1038
) {
  int32_t _M0L3lenS1037;
  int32_t _M0L6_2atmpS2892;
  uint16_t* _M0L6bufferS1040;
  int32_t _M0L1iS1041;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1037 = _M0L2toS1038 - _M0L4fromS1039;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2892 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1040
  = (uint16_t*)moonbit_make_string(_M0L3lenS1037, _M0L6_2atmpS2892);
  _M0L1iS1041 = 0;
  while (1) {
    if (_M0L1iS1041 < _M0L3lenS1037) {
      int32_t _M0L6_2atmpS2890 = _M0L4fromS1039 + _M0L1iS1041;
      int32_t _M0L6_2atmpS2889;
      int32_t _M0L6_2atmpS2888;
      int32_t _M0L6_2atmpS2891;
      if (
        _M0L6_2atmpS2890 < 0
        || _M0L6_2atmpS2890 >= Moonbit_array_length(_M0L5bytesS1042)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2889 = (int32_t)_M0L5bytesS1042[_M0L6_2atmpS2890];
      _M0L6_2atmpS2888 = (uint16_t)_M0L6_2atmpS2889;
      if (
        _M0L1iS1041 < 0
        || _M0L1iS1041 >= Moonbit_array_length(_M0L6bufferS1040)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1040[_M0L1iS1041] = _M0L6_2atmpS2888;
      _M0L6_2atmpS2891 = _M0L1iS1041 + 1;
      _M0L1iS1041 = _M0L6_2atmpS2891;
      continue;
    }
    break;
  }
  return _M0L6bufferS1040;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1036) {
  int32_t _M0L6_2atmpS2887;
  uint32_t _M0L6_2atmpS2886;
  uint32_t _M0L6_2atmpS2885;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2887 = _M0L1eS1036 * 78913;
  _M0L6_2atmpS2886 = *(uint32_t*)&_M0L6_2atmpS2887;
  _M0L6_2atmpS2885 = _M0L6_2atmpS2886 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2885;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1035) {
  int32_t _M0L6_2atmpS2884;
  uint32_t _M0L6_2atmpS2883;
  uint32_t _M0L6_2atmpS2882;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2884 = _M0L1eS1035 * 732923;
  _M0L6_2atmpS2883 = *(uint32_t*)&_M0L6_2atmpS2884;
  _M0L6_2atmpS2882 = _M0L6_2atmpS2883 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2882;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1033,
  int32_t _M0L8exponentS1034,
  int32_t _M0L8mantissaS1031
) {
  moonbit_string_t _M0L1sS1032;
  moonbit_string_t _result_4028;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1031) {
    return (moonbit_string_t)moonbit_string_literal_3.data;
  }
  if (_M0L4signS1033) {
    _M0L1sS1032 = (moonbit_string_t)moonbit_string_literal_4.data;
  } else {
    _M0L1sS1032 = (moonbit_string_t)moonbit_string_literal_5.data;
  }
  if (_M0L8exponentS1034) {
    moonbit_string_t _result_4027;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4027
    = moonbit_add_string(_M0L1sS1032, (moonbit_string_t)moonbit_string_literal_6.data);
    moonbit_decref(_M0L1sS1032);
    return _result_4027;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4028
  = moonbit_add_string(_M0L1sS1032, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L1sS1032);
  return _result_4028;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1030) {
  int32_t _M0L6_2atmpS2881;
  uint32_t _M0L6_2atmpS2880;
  uint32_t _M0L6_2atmpS2879;
  int32_t _M0L6_2atmpS2878;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2881 = _M0L1eS1030 * 1217359;
  _M0L6_2atmpS2880 = *(uint32_t*)&_M0L6_2atmpS2881;
  _M0L6_2atmpS2879 = _M0L6_2atmpS2880 >> 19;
  _M0L6_2atmpS2878 = *(int32_t*)&_M0L6_2atmpS2879;
  return _M0L6_2atmpS2878 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1026) {
  int32_t _tmp_4029;
  uint32_t _M0L6_2atmpS2877;
  uint32_t _M0Lm3accS1024;
  int32_t _M0L7_2abindS1025;
  int32_t _M0L1iS1027;
  uint32_t _M0L6_2atmpS2876;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_4029 = 0;
  _M0L6_2atmpS2877 = *(uint32_t*)&_tmp_4029;
  _M0Lm3accS1024 = _M0L6_2atmpS2877 + 374761393u;
  _M0L7_2abindS1025 = Moonbit_array_length(_M0L4selfS1026);
  _M0L1iS1027 = 0;
  while (1) {
    if (_M0L1iS1027 < _M0L7_2abindS1025) {
      uint32_t _M0L6_2atmpS2871 = _M0Lm3accS1024;
      int32_t _M0L6_2atmpS2874;
      int32_t _M0L6_2atmpS2873;
      uint32_t _M0L1vS1028;
      uint32_t _M0L6_2atmpS2872;
      int32_t _M0L6_2atmpS2875;
      _M0Lm3accS1024 = _M0L6_2atmpS2871 + 4u;
      _M0L6_2atmpS2874 = _M0L4selfS1026[_M0L1iS1027];
      _M0L6_2atmpS2873 = (int32_t)_M0L6_2atmpS2874;
      _M0L1vS1028 = *(uint32_t*)&_M0L6_2atmpS2873;
      _M0L6_2atmpS2872 = _M0Lm3accS1024;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1024 = _M0FPB13consume4__acc(_M0L6_2atmpS2872, _M0L1vS1028);
      _M0L6_2atmpS2875 = _M0L1iS1027 + 1;
      _M0L1iS1027 = _M0L6_2atmpS2875;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2876 = _M0Lm3accS1024;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS2876);
}

struct _M0TUssE* _M0MPB5Iter24nextGssE(
  struct _M0TPB4IterGUssEE* _M0L4selfS1020
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUssEE(_M0L4selfS1020);
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L4selfS1021
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L4selfS1021);
}

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS1022
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsbEE(_M0L4selfS1022);
}

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS1023
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsfEE(_M0L4selfS1023);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(
  struct _M0TPB3MapGssE* _M0L4selfS1016
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGssE(_M0L4selfS1016);
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS1017
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS1017);
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1018
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsbE(_M0L4selfS1018);
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1019
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsfE(_M0L4selfS1019);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(
  struct _M0TPB3MapGssE* _M0L4selfS973
) {
  struct _M0TPB5EntryGssE* _M0L4headS2840;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS972;
  int32_t _M0L3lenS974;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS975;
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__* _closure_4031;
  struct _M0TWEOUssE* _M0L6_2atmpS2831;
  int64_t _M0L6_2atmpS2832;
  struct _M0TPB4IterGUssEE* _result_4032;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2840 = _M0L4selfS973->$5;
  if (_M0L4headS2840) {
    moonbit_incref(_M0L4headS2840);
  }
  _M0L11curr__entryS972
  = (struct _M0TPB8MutLocalGORPB5EntryGssEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE));
  Moonbit_object_header(_M0L11curr__entryS972)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 39, 0);
  _M0L11curr__entryS972->$0 = _M0L4headS2840;
  _M0L3lenS974 = _M0L4selfS973->$1;
  _M0L9remainingS975
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS975)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS975->$0 = _M0L3lenS974;
  _closure_4031
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__*)moonbit_malloc(sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__));
  Moonbit_object_header(_closure_4031)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 42, 0);
  _closure_4031->code = &_M0MPB3Map4iterGssEC2833l711;
  _closure_4031->$0 = _M0L9remainingS975;
  _closure_4031->$1 = _M0L11curr__entryS972;
  _M0L6_2atmpS2831 = (struct _M0TWEOUssE*)_closure_4031;
  _M0L6_2atmpS2832 = (int64_t)_M0L3lenS974;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4032 = _M0MPB4Iter3newGUssEE(_M0L6_2atmpS2831, _M0L6_2atmpS2832);
  moonbit_decref(_M0L6_2atmpS2831);
  return _result_4032;
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS984
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4headS2850;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L11curr__entryS983;
  int32_t _M0L3lenS985;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS986;
  struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__* _closure_4033;
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2841;
  int64_t _M0L6_2atmpS2842;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _result_4034;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2850 = _M0L4selfS984->$5;
  if (_M0L4headS2850) {
    moonbit_incref(_M0L4headS2850);
  }
  _M0L11curr__entryS983
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS983)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 46, 0);
  _M0L11curr__entryS983->$0 = _M0L4headS2850;
  _M0L3lenS985 = _M0L4selfS984->$1;
  _M0L9remainingS986
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS986)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS986->$0 = _M0L3lenS985;
  _closure_4033
  = (struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__*)moonbit_malloc(sizeof(struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__));
  Moonbit_object_header(_closure_4033)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 49, 0);
  _closure_4033->code
  = &_M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2843l711;
  _closure_4033->$0 = _M0L9remainingS986;
  _closure_4033->$1 = _M0L11curr__entryS983;
  _M0L6_2atmpS2841
  = (struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_closure_4033;
  _M0L6_2atmpS2842 = (int64_t)_M0L3lenS985;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4034
  = _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2841, _M0L6_2atmpS2842);
  moonbit_decref(_M0L6_2atmpS2841);
  return _result_4034;
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS995
) {
  struct _M0TPB5EntryGsbE* _M0L4headS2860;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS994;
  int32_t _M0L3lenS996;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS997;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__* _closure_4035;
  struct _M0TWEOUsbE* _M0L6_2atmpS2851;
  int64_t _M0L6_2atmpS2852;
  struct _M0TPB4IterGUsbEE* _result_4036;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2860 = _M0L4selfS995->$5;
  if (_M0L4headS2860) {
    moonbit_incref(_M0L4headS2860);
  }
  _M0L11curr__entryS994
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS994)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 53, 0);
  _M0L11curr__entryS994->$0 = _M0L4headS2860;
  _M0L3lenS996 = _M0L4selfS995->$1;
  _M0L9remainingS997
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS997)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS997->$0 = _M0L3lenS996;
  _closure_4035
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__));
  Moonbit_object_header(_closure_4035)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 56, 0);
  _closure_4035->code = &_M0MPB3Map4iterGsbEC2853l711;
  _closure_4035->$0 = _M0L9remainingS997;
  _closure_4035->$1 = _M0L11curr__entryS994;
  _M0L6_2atmpS2851 = (struct _M0TWEOUsbE*)_closure_4035;
  _M0L6_2atmpS2852 = (int64_t)_M0L3lenS996;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4036 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS2851, _M0L6_2atmpS2852);
  moonbit_decref(_M0L6_2atmpS2851);
  return _result_4036;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1006
) {
  struct _M0TPB5EntryGsfE* _M0L4headS2870;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1005;
  int32_t _M0L3lenS1007;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1008;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__* _closure_4037;
  struct _M0TWEOUsfE* _M0L6_2atmpS2861;
  int64_t _M0L6_2atmpS2862;
  struct _M0TPB4IterGUsfEE* _result_4038;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2870 = _M0L4selfS1006->$5;
  if (_M0L4headS2870) {
    moonbit_incref(_M0L4headS2870);
  }
  _M0L11curr__entryS1005
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS1005)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 60, 0);
  _M0L11curr__entryS1005->$0 = _M0L4headS2870;
  _M0L3lenS1007 = _M0L4selfS1006->$1;
  _M0L9remainingS1008
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1008)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1008->$0 = _M0L3lenS1007;
  _closure_4037
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__));
  Moonbit_object_header(_closure_4037)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 63, 0);
  _closure_4037->code = &_M0MPB3Map4iterGsfEC2863l711;
  _closure_4037->$0 = _M0L9remainingS1008;
  _closure_4037->$1 = _M0L11curr__entryS1005;
  _M0L6_2atmpS2861 = (struct _M0TWEOUsfE*)_closure_4037;
  _M0L6_2atmpS2862 = (int64_t)_M0L3lenS1007;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4038 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS2861, _M0L6_2atmpS2862);
  moonbit_decref(_M0L6_2atmpS2861);
  return _result_4038;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2863l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS2864
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__* _M0L14_2acasted__envS2865;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1005;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1008;
  int32_t _M0L3valS2866;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2865
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2863__l711__*)_M0L6_2aenvS2864;
  _M0L11curr__entryS1005 = _M0L14_2acasted__envS2865->$1;
  _M0L9remainingS1008 = _M0L14_2acasted__envS2865->$0;
  _M0L3valS2866 = _M0L9remainingS1008->$0;
  if (_M0L3valS2866 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1010 = _M0L11curr__entryS1005->$0;
    if (_M0L7_2abindS1010 == 0) {
      goto join_1009;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1011 = _M0L7_2abindS1010;
      struct _M0TPB5EntryGsfE* _M0L4_2axS1012 = _M0L7_2aSomeS1011;
      moonbit_string_t _M0L6_2akeyS1013 = _M0L4_2axS1012->$4;
      float _M0L8_2avalueS1014 = _M0L4_2axS1012->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1015 = _M0L4_2axS1012->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3564 = _M0L11curr__entryS1005->$0;
      int32_t _M0L3valS2868;
      int32_t _M0L6_2atmpS2867;
      struct _M0TUsfE* _M0L8_2atupleS2869;
      if (_M0L7_2anextS1015) {
        moonbit_incref(_M0L7_2anextS1015);
      }
      moonbit_incref(_M0L6_2akeyS1013);
      if (_M0L6_2aoldS3564) {
        moonbit_decref(_M0L6_2aoldS3564);
      }
      _M0L11curr__entryS1005->$0 = _M0L7_2anextS1015;
      _M0L3valS2868 = _M0L9remainingS1008->$0;
      _M0L6_2atmpS2867 = _M0L3valS2868 - 1;
      _M0L9remainingS1008->$0 = _M0L6_2atmpS2867;
      _M0L8_2atupleS2869
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS2869)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
      _M0L8_2atupleS2869->$0 = _M0L6_2akeyS1013;
      _M0L8_2atupleS2869->$1 = _M0L8_2avalueS1014;
      return _M0L8_2atupleS2869;
    }
  } else {
    goto join_1009;
  }
  join_1009:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2853l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS2854
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__* _M0L14_2acasted__envS2855;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS994;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS997;
  int32_t _M0L3valS2856;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2855
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2853__l711__*)_M0L6_2aenvS2854;
  _M0L11curr__entryS994 = _M0L14_2acasted__envS2855->$1;
  _M0L9remainingS997 = _M0L14_2acasted__envS2855->$0;
  _M0L3valS2856 = _M0L9remainingS997->$0;
  if (_M0L3valS2856 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS999 = _M0L11curr__entryS994->$0;
    if (_M0L7_2abindS999 == 0) {
      goto join_998;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1000 = _M0L7_2abindS999;
      struct _M0TPB5EntryGsbE* _M0L4_2axS1001 = _M0L7_2aSomeS1000;
      moonbit_string_t _M0L6_2akeyS1002 = _M0L4_2axS1001->$4;
      int32_t _M0L8_2avalueS1003 = _M0L4_2axS1001->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS1004 = _M0L4_2axS1001->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3568 = _M0L11curr__entryS994->$0;
      int32_t _M0L3valS2858;
      int32_t _M0L6_2atmpS2857;
      struct _M0TUsbE* _M0L8_2atupleS2859;
      if (_M0L7_2anextS1004) {
        moonbit_incref(_M0L7_2anextS1004);
      }
      moonbit_incref(_M0L6_2akeyS1002);
      if (_M0L6_2aoldS3568) {
        moonbit_decref(_M0L6_2aoldS3568);
      }
      _M0L11curr__entryS994->$0 = _M0L7_2anextS1004;
      _M0L3valS2858 = _M0L9remainingS997->$0;
      _M0L6_2atmpS2857 = _M0L3valS2858 - 1;
      _M0L9remainingS997->$0 = _M0L6_2atmpS2857;
      _M0L8_2atupleS2859
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS2859)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 67, 0);
      _M0L8_2atupleS2859->$0 = _M0L6_2akeyS1002;
      _M0L8_2atupleS2859->$1 = _M0L8_2avalueS1003;
      return _M0L8_2atupleS2859;
    }
  } else {
    goto join_998;
  }
  join_998:;
  return 0;
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP38JIA2JIA29moonbitdb3lib10RedisValueEC2843l711(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aenvS2844
) {
  struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__* _M0L14_2acasted__envS2845;
  struct _M0TPB8MutLocalGORPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L11curr__entryS983;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS986;
  int32_t _M0L3valS2846;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2845
  = (struct _M0R98Map_3a_3aiter_7c_5bString_2c_20JIA2JIA2_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2843__l711__*)_M0L6_2aenvS2844;
  _M0L11curr__entryS983 = _M0L14_2acasted__envS2845->$1;
  _M0L9remainingS986 = _M0L14_2acasted__envS2845->$0;
  _M0L3valS2846 = _M0L9remainingS986->$0;
  if (_M0L3valS2846 > 0) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS988 =
      _M0L11curr__entryS983->$0;
    if (_M0L7_2abindS988 == 0) {
      goto join_987;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS989 =
        _M0L7_2abindS988;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS990 =
        _M0L7_2aSomeS989;
      moonbit_string_t _M0L6_2akeyS991 = _M0L4_2axS990->$4;
      void* _M0L8_2avalueS992 = _M0L4_2axS990->$5;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS993 =
        _M0L4_2axS990->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3572 =
        _M0L11curr__entryS983->$0;
      int32_t _M0L3valS2848;
      int32_t _M0L6_2atmpS2847;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2atupleS2849;
      if (_M0L7_2anextS993) {
        moonbit_incref(_M0L7_2anextS993);
      }
      moonbit_incref(_M0L8_2avalueS992);
      moonbit_incref(_M0L6_2akeyS991);
      if (_M0L6_2aoldS3572) {
        moonbit_decref(_M0L6_2aoldS3572);
      }
      _M0L11curr__entryS983->$0 = _M0L7_2anextS993;
      _M0L3valS2848 = _M0L9remainingS986->$0;
      _M0L6_2atmpS2847 = _M0L3valS2848 - 1;
      _M0L9remainingS986->$0 = _M0L6_2atmpS2847;
      _M0L8_2atupleS2849
      = (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS2849)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 70, 0);
      _M0L8_2atupleS2849->$0 = _M0L6_2akeyS991;
      _M0L8_2atupleS2849->$1 = _M0L8_2avalueS992;
      return _M0L8_2atupleS2849;
    }
  } else {
    goto join_987;
  }
  join_987:;
  return 0;
}

struct _M0TUssE* _M0MPB3Map4iterGssEC2833l711(
  struct _M0TWEOUssE* _M0L6_2aenvS2834
) {
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__* _M0L14_2acasted__envS2835;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS972;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS975;
  int32_t _M0L3valS2836;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2835
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2833__l711__*)_M0L6_2aenvS2834;
  _M0L11curr__entryS972 = _M0L14_2acasted__envS2835->$1;
  _M0L9remainingS975 = _M0L14_2acasted__envS2835->$0;
  _M0L3valS2836 = _M0L9remainingS975->$0;
  if (_M0L3valS2836 > 0) {
    struct _M0TPB5EntryGssE* _M0L7_2abindS977 = _M0L11curr__entryS972->$0;
    if (_M0L7_2abindS977 == 0) {
      goto join_976;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS978 = _M0L7_2abindS977;
      struct _M0TPB5EntryGssE* _M0L4_2axS979 = _M0L7_2aSomeS978;
      moonbit_string_t _M0L6_2akeyS980 = _M0L4_2axS979->$4;
      moonbit_string_t _M0L8_2avalueS981 = _M0L4_2axS979->$5;
      struct _M0TPB5EntryGssE* _M0L7_2anextS982 = _M0L4_2axS979->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3577 = _M0L11curr__entryS972->$0;
      int32_t _M0L3valS2838;
      int32_t _M0L6_2atmpS2837;
      struct _M0TUssE* _M0L8_2atupleS2839;
      if (_M0L7_2anextS982) {
        moonbit_incref(_M0L7_2anextS982);
      }
      moonbit_incref(_M0L8_2avalueS981);
      moonbit_incref(_M0L6_2akeyS980);
      if (_M0L6_2aoldS3577) {
        moonbit_decref(_M0L6_2aoldS3577);
      }
      _M0L11curr__entryS972->$0 = _M0L7_2anextS982;
      _M0L3valS2838 = _M0L9remainingS975->$0;
      _M0L6_2atmpS2837 = _M0L3valS2838 - 1;
      _M0L9remainingS975->$0 = _M0L6_2atmpS2837;
      _M0L8_2atupleS2839
      = (struct _M0TUssE*)moonbit_malloc(sizeof(struct _M0TUssE));
      Moonbit_object_header(_M0L8_2atupleS2839)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
      _M0L8_2atupleS2839->$0 = _M0L6_2akeyS980;
      _M0L8_2atupleS2839->$1 = _M0L8_2avalueS981;
      return _M0L8_2atupleS2839;
    }
  } else {
    goto join_976;
  }
  join_976:;
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

int32_t _M0MPB3Map6removeGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS964,
  moonbit_string_t _M0L3keyS965
) {
  int32_t _M0L6_2atmpS2828;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2828 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS965);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsbE(_M0L4selfS964, _M0L3keyS965, _M0L6_2atmpS2828);
  return 0;
}

int32_t _M0MPB3Map6removeGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS966,
  moonbit_string_t _M0L3keyS967
) {
  int32_t _M0L6_2atmpS2829;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2829 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS967);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS966, _M0L3keyS967, _M0L6_2atmpS2829);
  return 0;
}

int32_t _M0MPB3Map6removeGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS968,
  moonbit_string_t _M0L3keyS969
) {
  int32_t _M0L6_2atmpS2830;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2830 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS969);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS968, _M0L3keyS969, _M0L6_2atmpS2830);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS940,
  moonbit_string_t _M0L3keyS944,
  int32_t _M0L4hashS943
) {
  int32_t _M0L14capacity__maskS2803;
  int32_t _M0L6_2atmpS2802;
  int32_t _M0L1iS937;
  int32_t _M0L3idxS938;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2803 = _M0L4selfS940->$3;
  _M0L6_2atmpS2802 = _M0L4hashS943 & _M0L14capacity__maskS2803;
  _M0L1iS937 = 0;
  _M0L3idxS938 = _M0L6_2atmpS2802;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2801 = _M0L4selfS940->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS939;
    if (
      _M0L3idxS938 < 0
      || _M0L3idxS938 >= Moonbit_array_length(_M0L7entriesS2801)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS939
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2801[_M0L3idxS938];
    if (_M0L7_2abindS939 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS941 = _M0L7_2abindS939;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS942 = _M0L7_2aSomeS941;
      int32_t _M0L4hashS2793 = _M0L8_2aentryS942->$3;
      int32_t _if__result_4044;
      int32_t _M0L3pslS2796;
      int32_t _M0L6_2atmpS2797;
      int32_t _M0L6_2atmpS2799;
      int32_t _M0L14capacity__maskS2800;
      int32_t _M0L6_2atmpS2798;
      if (_M0L4hashS2793 == _M0L4hashS943) {
        moonbit_string_t _M0L3keyS2792 = _M0L8_2aentryS942->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4044
        = _M0L3keyS2792 == _M0L3keyS944
          || Moonbit_array_length(_M0L3keyS2792)
             == Moonbit_array_length(_M0L3keyS944)
             && 0
                == memcmp(_M0L3keyS2792, _M0L3keyS944, Moonbit_array_length(_M0L3keyS2792) * 2);
      } else {
        _if__result_4044 = 0;
      }
      if (_if__result_4044) {
        int32_t _M0L4sizeS2795;
        int32_t _M0L6_2atmpS2794;
        moonbit_incref(_M0L8_2aentryS942);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsbE(_M0L4selfS940, _M0L8_2aentryS942);
        moonbit_decref(_M0L8_2aentryS942);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsbE(_M0L4selfS940, _M0L3idxS938);
        _M0L4sizeS2795 = _M0L4selfS940->$1;
        _M0L6_2atmpS2794 = _M0L4sizeS2795 - 1;
        _M0L4selfS940->$1 = _M0L6_2atmpS2794;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS942);
      }
      _M0L3pslS2796 = _M0L8_2aentryS942->$2;
      moonbit_decref(_M0L8_2aentryS942);
      if (_M0L1iS937 > _M0L3pslS2796) {
        break;
      }
      _M0L6_2atmpS2797 = _M0L1iS937 + 1;
      _M0L6_2atmpS2799 = _M0L3idxS938 + 1;
      _M0L14capacity__maskS2800 = _M0L4selfS940->$3;
      _M0L6_2atmpS2798 = _M0L6_2atmpS2799 & _M0L14capacity__maskS2800;
      _M0L1iS937 = _M0L6_2atmpS2797;
      _M0L3idxS938 = _M0L6_2atmpS2798;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS949,
  moonbit_string_t _M0L3keyS953,
  int32_t _M0L4hashS952
) {
  int32_t _M0L14capacity__maskS2815;
  int32_t _M0L6_2atmpS2814;
  int32_t _M0L1iS946;
  int32_t _M0L3idxS947;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2815 = _M0L4selfS949->$3;
  _M0L6_2atmpS2814 = _M0L4hashS952 & _M0L14capacity__maskS2815;
  _M0L1iS946 = 0;
  _M0L3idxS947 = _M0L6_2atmpS2814;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2813 = _M0L4selfS949->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS948;
    if (
      _M0L3idxS947 < 0
      || _M0L3idxS947 >= Moonbit_array_length(_M0L7entriesS2813)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS948
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2813[_M0L3idxS947];
    if (_M0L7_2abindS948 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS950 = _M0L7_2abindS948;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS951 = _M0L7_2aSomeS950;
      int32_t _M0L4hashS2805 = _M0L8_2aentryS951->$3;
      int32_t _if__result_4046;
      int32_t _M0L3pslS2808;
      int32_t _M0L6_2atmpS2809;
      int32_t _M0L6_2atmpS2811;
      int32_t _M0L14capacity__maskS2812;
      int32_t _M0L6_2atmpS2810;
      if (_M0L4hashS2805 == _M0L4hashS952) {
        moonbit_string_t _M0L3keyS2804 = _M0L8_2aentryS951->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4046
        = _M0L3keyS2804 == _M0L3keyS953
          || Moonbit_array_length(_M0L3keyS2804)
             == Moonbit_array_length(_M0L3keyS953)
             && 0
                == memcmp(_M0L3keyS2804, _M0L3keyS953, Moonbit_array_length(_M0L3keyS2804) * 2);
      } else {
        _if__result_4046 = 0;
      }
      if (_if__result_4046) {
        int32_t _M0L4sizeS2807;
        int32_t _M0L6_2atmpS2806;
        moonbit_incref(_M0L8_2aentryS951);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS949, _M0L8_2aentryS951);
        moonbit_decref(_M0L8_2aentryS951);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS949, _M0L3idxS947);
        _M0L4sizeS2807 = _M0L4selfS949->$1;
        _M0L6_2atmpS2806 = _M0L4sizeS2807 - 1;
        _M0L4selfS949->$1 = _M0L6_2atmpS2806;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS951);
      }
      _M0L3pslS2808 = _M0L8_2aentryS951->$2;
      moonbit_decref(_M0L8_2aentryS951);
      if (_M0L1iS946 > _M0L3pslS2808) {
        break;
      }
      _M0L6_2atmpS2809 = _M0L1iS946 + 1;
      _M0L6_2atmpS2811 = _M0L3idxS947 + 1;
      _M0L14capacity__maskS2812 = _M0L4selfS949->$3;
      _M0L6_2atmpS2810 = _M0L6_2atmpS2811 & _M0L14capacity__maskS2812;
      _M0L1iS946 = _M0L6_2atmpS2809;
      _M0L3idxS947 = _M0L6_2atmpS2810;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS958,
  moonbit_string_t _M0L3keyS962,
  int32_t _M0L4hashS961
) {
  int32_t _M0L14capacity__maskS2827;
  int32_t _M0L6_2atmpS2826;
  int32_t _M0L1iS955;
  int32_t _M0L3idxS956;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2827 = _M0L4selfS958->$3;
  _M0L6_2atmpS2826 = _M0L4hashS961 & _M0L14capacity__maskS2827;
  _M0L1iS955 = 0;
  _M0L3idxS956 = _M0L6_2atmpS2826;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2825 =
      _M0L4selfS958->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS957;
    if (
      _M0L3idxS956 < 0
      || _M0L3idxS956 >= Moonbit_array_length(_M0L7entriesS2825)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS957
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2825[
        _M0L3idxS956
      ];
    if (_M0L7_2abindS957 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS959 =
        _M0L7_2abindS957;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS960 =
        _M0L7_2aSomeS959;
      int32_t _M0L4hashS2817 = _M0L8_2aentryS960->$3;
      int32_t _if__result_4048;
      int32_t _M0L3pslS2820;
      int32_t _M0L6_2atmpS2821;
      int32_t _M0L6_2atmpS2823;
      int32_t _M0L14capacity__maskS2824;
      int32_t _M0L6_2atmpS2822;
      if (_M0L4hashS2817 == _M0L4hashS961) {
        moonbit_string_t _M0L3keyS2816 = _M0L8_2aentryS960->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4048
        = _M0L3keyS2816 == _M0L3keyS962
          || Moonbit_array_length(_M0L3keyS2816)
             == Moonbit_array_length(_M0L3keyS962)
             && 0
                == memcmp(_M0L3keyS2816, _M0L3keyS962, Moonbit_array_length(_M0L3keyS2816) * 2);
      } else {
        _if__result_4048 = 0;
      }
      if (_if__result_4048) {
        int32_t _M0L4sizeS2819;
        int32_t _M0L6_2atmpS2818;
        moonbit_incref(_M0L8_2aentryS960);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS958, _M0L8_2aentryS960);
        moonbit_decref(_M0L8_2aentryS960);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS958, _M0L3idxS956);
        _M0L4sizeS2819 = _M0L4selfS958->$1;
        _M0L6_2atmpS2818 = _M0L4sizeS2819 - 1;
        _M0L4selfS958->$1 = _M0L6_2atmpS2818;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS960);
      }
      _M0L3pslS2820 = _M0L8_2aentryS960->$2;
      moonbit_decref(_M0L8_2aentryS960);
      if (_M0L1iS955 > _M0L3pslS2820) {
        break;
      }
      _M0L6_2atmpS2821 = _M0L1iS955 + 1;
      _M0L6_2atmpS2823 = _M0L3idxS956 + 1;
      _M0L14capacity__maskS2824 = _M0L4selfS958->$3;
      _M0L6_2atmpS2822 = _M0L6_2atmpS2823 & _M0L14capacity__maskS2824;
      _M0L1iS955 = _M0L6_2atmpS2821;
      _M0L3idxS956 = _M0L6_2atmpS2822;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS909,
  int32_t _M0L3idxS916
) {
  int32_t _M0L3curS907;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS907 = _M0L3idxS916;
  _2afor_911:;
  while (1) {
    int32_t _M0L6_2atmpS2776 = _M0L3curS907 + 1;
    int32_t _M0L14capacity__maskS2777 = _M0L4selfS909->$3;
    int32_t _M0L4nextS908 = _M0L6_2atmpS2776 & _M0L14capacity__maskS2777;
    struct _M0TPB5EntryGsbE** _M0L7entriesS2775 = _M0L4selfS909->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS912;
    struct _M0TPB5EntryGsbE** _M0L7entriesS2771;
    struct _M0TPB5EntryGsbE* _M0L6_2atmpS2772;
    struct _M0TPB5EntryGsbE* _M0L6_2aoldS3591;
    int32_t _tmp_4051;
    if (
      _M0L4nextS908 < 0
      || _M0L4nextS908 >= Moonbit_array_length(_M0L7entriesS2775)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS912
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2775[_M0L4nextS908];
    if (_M0L7_2abindS912 == 0) {
      goto join_910;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS913 = _M0L7_2abindS912;
      struct _M0TPB5EntryGsbE* _M0L4_2axS914 = _M0L7_2aSomeS913;
      int32_t _M0L4_2axS915 = _M0L4_2axS914->$2;
      switch (_M0L4_2axS915) {
        case 0: {
          goto join_910;
          break;
        }
        default: {
          int32_t _M0L3pslS2774 = _M0L4_2axS914->$2;
          int32_t _M0L6_2atmpS2773 = _M0L3pslS2774 - 1;
          _M0L4_2axS914->$2 = _M0L6_2atmpS2773;
          moonbit_incref(_M0L4_2axS914);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsbE(_M0L4selfS909, _M0L4_2axS914, _M0L3curS907);
          moonbit_decref(_M0L4_2axS914);
          _M0L3curS907 = _M0L4nextS908;
          goto _2afor_911;
          break;
        }
      }
    }
    goto joinlet_4050;
    join_910:;
    _M0L7entriesS2771 = _M0L4selfS909->$0;
    _M0L6_2atmpS2772 = 0;
    if (
      _M0L3curS907 < 0
      || _M0L3curS907 >= Moonbit_array_length(_M0L7entriesS2771)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3591
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2771[_M0L3curS907];
    if (_M0L6_2aoldS3591) {
      moonbit_decref(_M0L6_2aoldS3591);
    }
    _M0L7entriesS2771[_M0L3curS907] = _M0L6_2atmpS2772;
    break;
    joinlet_4050:;
    _tmp_4051 = _M0L3curS907;
    _M0L3curS907 = _tmp_4051;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS919,
  int32_t _M0L3idxS926
) {
  int32_t _M0L3curS917;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS917 = _M0L3idxS926;
  _2afor_921:;
  while (1) {
    int32_t _M0L6_2atmpS2783 = _M0L3curS917 + 1;
    int32_t _M0L14capacity__maskS2784 = _M0L4selfS919->$3;
    int32_t _M0L4nextS918 = _M0L6_2atmpS2783 & _M0L14capacity__maskS2784;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2782 = _M0L4selfS919->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS922;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2778;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS2779;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS3595;
    int32_t _tmp_4054;
    if (
      _M0L4nextS918 < 0
      || _M0L4nextS918 >= Moonbit_array_length(_M0L7entriesS2782)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS922
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2782[_M0L4nextS918];
    if (_M0L7_2abindS922 == 0) {
      goto join_920;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS923 = _M0L7_2abindS922;
      struct _M0TPB5EntryGsiE* _M0L4_2axS924 = _M0L7_2aSomeS923;
      int32_t _M0L4_2axS925 = _M0L4_2axS924->$2;
      switch (_M0L4_2axS925) {
        case 0: {
          goto join_920;
          break;
        }
        default: {
          int32_t _M0L3pslS2781 = _M0L4_2axS924->$2;
          int32_t _M0L6_2atmpS2780 = _M0L3pslS2781 - 1;
          _M0L4_2axS924->$2 = _M0L6_2atmpS2780;
          moonbit_incref(_M0L4_2axS924);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsiE(_M0L4selfS919, _M0L4_2axS924, _M0L3curS917);
          moonbit_decref(_M0L4_2axS924);
          _M0L3curS917 = _M0L4nextS918;
          goto _2afor_921;
          break;
        }
      }
    }
    goto joinlet_4053;
    join_920:;
    _M0L7entriesS2778 = _M0L4selfS919->$0;
    _M0L6_2atmpS2779 = 0;
    if (
      _M0L3curS917 < 0
      || _M0L3curS917 >= Moonbit_array_length(_M0L7entriesS2778)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3595
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2778[_M0L3curS917];
    if (_M0L6_2aoldS3595) {
      moonbit_decref(_M0L6_2aoldS3595);
    }
    _M0L7entriesS2778[_M0L3curS917] = _M0L6_2atmpS2779;
    break;
    joinlet_4053:;
    _tmp_4054 = _M0L3curS917;
    _M0L3curS917 = _tmp_4054;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS929,
  int32_t _M0L3idxS936
) {
  int32_t _M0L3curS927;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS927 = _M0L3idxS936;
  _2afor_931:;
  while (1) {
    int32_t _M0L6_2atmpS2790 = _M0L3curS927 + 1;
    int32_t _M0L14capacity__maskS2791 = _M0L4selfS929->$3;
    int32_t _M0L4nextS928 = _M0L6_2atmpS2790 & _M0L14capacity__maskS2791;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2789 =
      _M0L4selfS929->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS932;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2785;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2786;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3599;
    int32_t _tmp_4057;
    if (
      _M0L4nextS928 < 0
      || _M0L4nextS928 >= Moonbit_array_length(_M0L7entriesS2789)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS932
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2789[
        _M0L4nextS928
      ];
    if (_M0L7_2abindS932 == 0) {
      goto join_930;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS933 =
        _M0L7_2abindS932;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2axS934 =
        _M0L7_2aSomeS933;
      int32_t _M0L4_2axS935 = _M0L4_2axS934->$2;
      switch (_M0L4_2axS935) {
        case 0: {
          goto join_930;
          break;
        }
        default: {
          int32_t _M0L3pslS2788 = _M0L4_2axS934->$2;
          int32_t _M0L6_2atmpS2787 = _M0L3pslS2788 - 1;
          _M0L4_2axS934->$2 = _M0L6_2atmpS2787;
          moonbit_incref(_M0L4_2axS934);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS929, _M0L4_2axS934, _M0L3curS927);
          moonbit_decref(_M0L4_2axS934);
          _M0L3curS927 = _M0L4nextS928;
          goto _2afor_931;
          break;
        }
      }
    }
    goto joinlet_4056;
    join_930:;
    _M0L7entriesS2785 = _M0L4selfS929->$0;
    _M0L6_2atmpS2786 = 0;
    if (
      _M0L3curS927 < 0
      || _M0L3curS927 >= Moonbit_array_length(_M0L7entriesS2785)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3599
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2785[
        _M0L3curS927
      ];
    if (_M0L6_2aoldS3599) {
      moonbit_decref(_M0L6_2aoldS3599);
    }
    _M0L7entriesS2785[_M0L3curS927] = _M0L6_2atmpS2786;
    break;
    joinlet_4056:;
    _tmp_4057 = _M0L3curS927;
    _M0L3curS927 = _tmp_4057;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS891,
  struct _M0TPB5EntryGsbE* _M0L5entryS890
) {
  int32_t _M0L7_2abindS889;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS892;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS889 = _M0L5entryS890->$0;
  switch (_M0L7_2abindS889) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L4nextS2750 = _M0L5entryS890->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3604 = _M0L4selfS891->$5;
      if (_M0L4nextS2750) {
        moonbit_incref(_M0L4nextS2750);
      }
      if (_M0L6_2aoldS3604) {
        moonbit_decref(_M0L6_2aoldS3604);
      }
      _M0L4selfS891->$5 = _M0L4nextS2750;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2754 = _M0L4selfS891->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2753;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2751;
      struct _M0TPB5EntryGsbE* _M0L4nextS2752;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3606;
      if (
        _M0L7_2abindS889 < 0
        || _M0L7_2abindS889 >= Moonbit_array_length(_M0L7entriesS2754)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2753
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2754[_M0L7_2abindS889];
      if (_M0L6_2atmpS2753) {
        moonbit_incref(_M0L6_2atmpS2753);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2751
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2753);
      if (_M0L6_2atmpS2753) {
        moonbit_decref(_M0L6_2atmpS2753);
      }
      _M0L4nextS2752 = _M0L5entryS890->$1;
      _M0L6_2aoldS3606 = _M0L6_2atmpS2751->$1;
      if (_M0L4nextS2752) {
        moonbit_incref(_M0L4nextS2752);
      }
      if (_M0L6_2aoldS3606) {
        moonbit_decref(_M0L6_2aoldS3606);
      }
      _M0L6_2atmpS2751->$1 = _M0L4nextS2752;
      moonbit_decref(_M0L6_2atmpS2751);
      break;
    }
  }
  _M0L7_2abindS892 = _M0L5entryS890->$1;
  if (_M0L7_2abindS892 == 0) {
    int32_t _M0L4prevS2755 = _M0L5entryS890->$0;
    _M0L4selfS891->$6 = _M0L4prevS2755;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS893 = _M0L7_2abindS892;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS894 = _M0L7_2aSomeS893;
    int32_t _M0L4prevS2756 = _M0L5entryS890->$0;
    _M0L7_2anextS894->$0 = _M0L4prevS2756;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS897,
  struct _M0TPB5EntryGsiE* _M0L5entryS896
) {
  int32_t _M0L7_2abindS895;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS898;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS895 = _M0L5entryS896->$0;
  switch (_M0L7_2abindS895) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L4nextS2757 = _M0L5entryS896->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3611 = _M0L4selfS897->$5;
      if (_M0L4nextS2757) {
        moonbit_incref(_M0L4nextS2757);
      }
      if (_M0L6_2aoldS3611) {
        moonbit_decref(_M0L6_2aoldS3611);
      }
      _M0L4selfS897->$5 = _M0L4nextS2757;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2761 = _M0L4selfS897->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2760;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2758;
      struct _M0TPB5EntryGsiE* _M0L4nextS2759;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3613;
      if (
        _M0L7_2abindS895 < 0
        || _M0L7_2abindS895 >= Moonbit_array_length(_M0L7entriesS2761)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2760
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2761[_M0L7_2abindS895];
      if (_M0L6_2atmpS2760) {
        moonbit_incref(_M0L6_2atmpS2760);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2758
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2760);
      if (_M0L6_2atmpS2760) {
        moonbit_decref(_M0L6_2atmpS2760);
      }
      _M0L4nextS2759 = _M0L5entryS896->$1;
      _M0L6_2aoldS3613 = _M0L6_2atmpS2758->$1;
      if (_M0L4nextS2759) {
        moonbit_incref(_M0L4nextS2759);
      }
      if (_M0L6_2aoldS3613) {
        moonbit_decref(_M0L6_2aoldS3613);
      }
      _M0L6_2atmpS2758->$1 = _M0L4nextS2759;
      moonbit_decref(_M0L6_2atmpS2758);
      break;
    }
  }
  _M0L7_2abindS898 = _M0L5entryS896->$1;
  if (_M0L7_2abindS898 == 0) {
    int32_t _M0L4prevS2762 = _M0L5entryS896->$0;
    _M0L4selfS897->$6 = _M0L4prevS2762;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS899 = _M0L7_2abindS898;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS900 = _M0L7_2aSomeS899;
    int32_t _M0L4prevS2763 = _M0L5entryS896->$0;
    _M0L7_2anextS900->$0 = _M0L4prevS2763;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS903,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS902
) {
  int32_t _M0L7_2abindS901;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS904;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS901 = _M0L5entryS902->$0;
  switch (_M0L7_2abindS901) {
    case -1: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4nextS2764 =
        _M0L5entryS902->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3618 =
        _M0L4selfS903->$5;
      if (_M0L4nextS2764) {
        moonbit_incref(_M0L4nextS2764);
      }
      if (_M0L6_2aoldS3618) {
        moonbit_decref(_M0L6_2aoldS3618);
      }
      _M0L4selfS903->$5 = _M0L4nextS2764;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2768 =
        _M0L4selfS903->$0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2767;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2765;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4nextS2766;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3620;
      if (
        _M0L7_2abindS901 < 0
        || _M0L7_2abindS901 >= Moonbit_array_length(_M0L7entriesS2768)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2767
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2768[
          _M0L7_2abindS901
        ];
      if (_M0L6_2atmpS2767) {
        moonbit_incref(_M0L6_2atmpS2767);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2765
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2767);
      if (_M0L6_2atmpS2767) {
        moonbit_decref(_M0L6_2atmpS2767);
      }
      _M0L4nextS2766 = _M0L5entryS902->$1;
      _M0L6_2aoldS3620 = _M0L6_2atmpS2765->$1;
      if (_M0L4nextS2766) {
        moonbit_incref(_M0L4nextS2766);
      }
      if (_M0L6_2aoldS3620) {
        moonbit_decref(_M0L6_2aoldS3620);
      }
      _M0L6_2atmpS2765->$1 = _M0L4nextS2766;
      moonbit_decref(_M0L6_2atmpS2765);
      break;
    }
  }
  _M0L7_2abindS904 = _M0L5entryS902->$1;
  if (_M0L7_2abindS904 == 0) {
    int32_t _M0L4prevS2769 = _M0L5entryS902->$0;
    _M0L4selfS903->$6 = _M0L4prevS2769;
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS905 =
      _M0L7_2abindS904;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS906 =
      _M0L7_2aSomeS905;
    int32_t _M0L4prevS2770 = _M0L5entryS902->$0;
    _M0L7_2anextS906->$0 = _M0L4prevS2770;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS858,
  moonbit_string_t _M0L3keyS854
) {
  int32_t _M0L4hashS853;
  int32_t _M0L14capacity__maskS2719;
  int32_t _M0L6_2atmpS2718;
  int32_t _M0L1iS855;
  int32_t _M0L3idxS856;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS853 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS854);
  _M0L14capacity__maskS2719 = _M0L4selfS858->$3;
  _M0L6_2atmpS2718 = _M0L4hashS853 & _M0L14capacity__maskS2719;
  _M0L1iS855 = 0;
  _M0L3idxS856 = _M0L6_2atmpS2718;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2717 =
      _M0L4selfS858->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS857;
    if (
      _M0L3idxS856 < 0
      || _M0L3idxS856 >= Moonbit_array_length(_M0L7entriesS2717)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS857
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2717[
        _M0L3idxS856
      ];
    if (_M0L7_2abindS857 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS859 =
        _M0L7_2abindS857;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS860 =
        _M0L7_2aSomeS859;
      int32_t _M0L4hashS2711 = _M0L8_2aentryS860->$3;
      int32_t _if__result_4059;
      int32_t _M0L3pslS2712;
      int32_t _M0L6_2atmpS2713;
      int32_t _M0L6_2atmpS2715;
      int32_t _M0L14capacity__maskS2716;
      int32_t _M0L6_2atmpS2714;
      if (_M0L4hashS2711 == _M0L4hashS853) {
        moonbit_string_t _M0L3keyS2710 = _M0L8_2aentryS860->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4059
        = _M0L3keyS2710 == _M0L3keyS854
          || Moonbit_array_length(_M0L3keyS2710)
             == Moonbit_array_length(_M0L3keyS854)
             && 0
                == memcmp(_M0L3keyS2710, _M0L3keyS854, Moonbit_array_length(_M0L3keyS2710) * 2);
      } else {
        _if__result_4059 = 0;
      }
      if (_if__result_4059) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS860);
      }
      _M0L3pslS2712 = _M0L8_2aentryS860->$2;
      moonbit_decref(_M0L8_2aentryS860);
      if (_M0L1iS855 > _M0L3pslS2712) {
        return 0;
      }
      _M0L6_2atmpS2713 = _M0L1iS855 + 1;
      _M0L6_2atmpS2715 = _M0L3idxS856 + 1;
      _M0L14capacity__maskS2716 = _M0L4selfS858->$3;
      _M0L6_2atmpS2714 = _M0L6_2atmpS2715 & _M0L14capacity__maskS2716;
      _M0L1iS855 = _M0L6_2atmpS2713;
      _M0L3idxS856 = _M0L6_2atmpS2714;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS867,
  moonbit_string_t _M0L3keyS863
) {
  int32_t _M0L4hashS862;
  int32_t _M0L14capacity__maskS2729;
  int32_t _M0L6_2atmpS2728;
  int32_t _M0L1iS864;
  int32_t _M0L3idxS865;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS862 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS863);
  _M0L14capacity__maskS2729 = _M0L4selfS867->$3;
  _M0L6_2atmpS2728 = _M0L4hashS862 & _M0L14capacity__maskS2729;
  _M0L1iS864 = 0;
  _M0L3idxS865 = _M0L6_2atmpS2728;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2727 = _M0L4selfS867->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS866;
    if (
      _M0L3idxS865 < 0
      || _M0L3idxS865 >= Moonbit_array_length(_M0L7entriesS2727)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS866
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2727[_M0L3idxS865];
    if (_M0L7_2abindS866 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS868 = _M0L7_2abindS866;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS869 = _M0L7_2aSomeS868;
      int32_t _M0L4hashS2721 = _M0L8_2aentryS869->$3;
      int32_t _if__result_4061;
      int32_t _M0L3pslS2722;
      int32_t _M0L6_2atmpS2723;
      int32_t _M0L6_2atmpS2725;
      int32_t _M0L14capacity__maskS2726;
      int32_t _M0L6_2atmpS2724;
      if (_M0L4hashS2721 == _M0L4hashS862) {
        moonbit_string_t _M0L3keyS2720 = _M0L8_2aentryS869->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4061
        = _M0L3keyS2720 == _M0L3keyS863
          || Moonbit_array_length(_M0L3keyS2720)
             == Moonbit_array_length(_M0L3keyS863)
             && 0
                == memcmp(_M0L3keyS2720, _M0L3keyS863, Moonbit_array_length(_M0L3keyS2720) * 2);
      } else {
        _if__result_4061 = 0;
      }
      if (_if__result_4061) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS869);
      }
      _M0L3pslS2722 = _M0L8_2aentryS869->$2;
      moonbit_decref(_M0L8_2aentryS869);
      if (_M0L1iS864 > _M0L3pslS2722) {
        return 0;
      }
      _M0L6_2atmpS2723 = _M0L1iS864 + 1;
      _M0L6_2atmpS2725 = _M0L3idxS865 + 1;
      _M0L14capacity__maskS2726 = _M0L4selfS867->$3;
      _M0L6_2atmpS2724 = _M0L6_2atmpS2725 & _M0L14capacity__maskS2726;
      _M0L1iS864 = _M0L6_2atmpS2723;
      _M0L3idxS865 = _M0L6_2atmpS2724;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS876,
  moonbit_string_t _M0L3keyS872
) {
  int32_t _M0L4hashS871;
  int32_t _M0L14capacity__maskS2739;
  int32_t _M0L6_2atmpS2738;
  int32_t _M0L1iS873;
  int32_t _M0L3idxS874;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS871 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS872);
  _M0L14capacity__maskS2739 = _M0L4selfS876->$3;
  _M0L6_2atmpS2738 = _M0L4hashS871 & _M0L14capacity__maskS2739;
  _M0L1iS873 = 0;
  _M0L3idxS874 = _M0L6_2atmpS2738;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2737 = _M0L4selfS876->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS875;
    if (
      _M0L3idxS874 < 0
      || _M0L3idxS874 >= Moonbit_array_length(_M0L7entriesS2737)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS875
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2737[_M0L3idxS874];
    if (_M0L7_2abindS875 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS877 = _M0L7_2abindS875;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS878 = _M0L7_2aSomeS877;
      int32_t _M0L4hashS2731 = _M0L8_2aentryS878->$3;
      int32_t _if__result_4063;
      int32_t _M0L3pslS2732;
      int32_t _M0L6_2atmpS2733;
      int32_t _M0L6_2atmpS2735;
      int32_t _M0L14capacity__maskS2736;
      int32_t _M0L6_2atmpS2734;
      if (_M0L4hashS2731 == _M0L4hashS871) {
        moonbit_string_t _M0L3keyS2730 = _M0L8_2aentryS878->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4063
        = _M0L3keyS2730 == _M0L3keyS872
          || Moonbit_array_length(_M0L3keyS2730)
             == Moonbit_array_length(_M0L3keyS872)
             && 0
                == memcmp(_M0L3keyS2730, _M0L3keyS872, Moonbit_array_length(_M0L3keyS2730) * 2);
      } else {
        _if__result_4063 = 0;
      }
      if (_if__result_4063) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS878);
      }
      _M0L3pslS2732 = _M0L8_2aentryS878->$2;
      moonbit_decref(_M0L8_2aentryS878);
      if (_M0L1iS873 > _M0L3pslS2732) {
        return 0;
      }
      _M0L6_2atmpS2733 = _M0L1iS873 + 1;
      _M0L6_2atmpS2735 = _M0L3idxS874 + 1;
      _M0L14capacity__maskS2736 = _M0L4selfS876->$3;
      _M0L6_2atmpS2734 = _M0L6_2atmpS2735 & _M0L14capacity__maskS2736;
      _M0L1iS873 = _M0L6_2atmpS2733;
      _M0L3idxS874 = _M0L6_2atmpS2734;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS885,
  moonbit_string_t _M0L3keyS881
) {
  int32_t _M0L4hashS880;
  int32_t _M0L14capacity__maskS2749;
  int32_t _M0L6_2atmpS2748;
  int32_t _M0L1iS882;
  int32_t _M0L3idxS883;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS880 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS881);
  _M0L14capacity__maskS2749 = _M0L4selfS885->$3;
  _M0L6_2atmpS2748 = _M0L4hashS880 & _M0L14capacity__maskS2749;
  _M0L1iS882 = 0;
  _M0L3idxS883 = _M0L6_2atmpS2748;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2747 = _M0L4selfS885->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS884;
    if (
      _M0L3idxS883 < 0
      || _M0L3idxS883 >= Moonbit_array_length(_M0L7entriesS2747)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS884
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2747[_M0L3idxS883];
    if (_M0L7_2abindS884 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS886 = _M0L7_2abindS884;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS887 = _M0L7_2aSomeS886;
      int32_t _M0L4hashS2741 = _M0L8_2aentryS887->$3;
      int32_t _if__result_4065;
      int32_t _M0L3pslS2742;
      int32_t _M0L6_2atmpS2743;
      int32_t _M0L6_2atmpS2745;
      int32_t _M0L14capacity__maskS2746;
      int32_t _M0L6_2atmpS2744;
      if (_M0L4hashS2741 == _M0L4hashS880) {
        moonbit_string_t _M0L3keyS2740 = _M0L8_2aentryS887->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4065
        = _M0L3keyS2740 == _M0L3keyS881
          || Moonbit_array_length(_M0L3keyS2740)
             == Moonbit_array_length(_M0L3keyS881)
             && 0
                == memcmp(_M0L3keyS2740, _M0L3keyS881, Moonbit_array_length(_M0L3keyS2740) * 2);
      } else {
        _if__result_4065 = 0;
      }
      if (_if__result_4065) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS887);
      }
      _M0L3pslS2742 = _M0L8_2aentryS887->$2;
      moonbit_decref(_M0L8_2aentryS887);
      if (_M0L1iS882 > _M0L3pslS2742) {
        return 0;
      }
      _M0L6_2atmpS2743 = _M0L1iS882 + 1;
      _M0L6_2atmpS2745 = _M0L3idxS883 + 1;
      _M0L14capacity__maskS2746 = _M0L4selfS885->$3;
      _M0L6_2atmpS2744 = _M0L6_2atmpS2745 & _M0L14capacity__maskS2746;
      _M0L1iS882 = _M0L6_2atmpS2743;
      _M0L3idxS883 = _M0L6_2atmpS2744;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS822,
  moonbit_string_t _M0L3keyS818
) {
  int32_t _M0L4hashS817;
  int32_t _M0L14capacity__maskS2669;
  int32_t _M0L6_2atmpS2668;
  int32_t _M0L1iS819;
  int32_t _M0L3idxS820;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS817 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS818);
  _M0L14capacity__maskS2669 = _M0L4selfS822->$3;
  _M0L6_2atmpS2668 = _M0L4hashS817 & _M0L14capacity__maskS2669;
  _M0L1iS819 = 0;
  _M0L3idxS820 = _M0L6_2atmpS2668;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2667 =
      _M0L4selfS822->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS821;
    if (
      _M0L3idxS820 < 0
      || _M0L3idxS820 >= Moonbit_array_length(_M0L7entriesS2667)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS821
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2667[
        _M0L3idxS820
      ];
    if (_M0L7_2abindS821 == 0) {
      void* _M0L6_2atmpS2656 = 0;
      return _M0L6_2atmpS2656;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS823 =
        _M0L7_2abindS821;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L8_2aentryS824 =
        _M0L7_2aSomeS823;
      int32_t _M0L4hashS2658 = _M0L8_2aentryS824->$3;
      int32_t _if__result_4067;
      int32_t _M0L3pslS2661;
      int32_t _M0L6_2atmpS2663;
      int32_t _M0L6_2atmpS2665;
      int32_t _M0L14capacity__maskS2666;
      int32_t _M0L6_2atmpS2664;
      if (_M0L4hashS2658 == _M0L4hashS817) {
        moonbit_string_t _M0L3keyS2657 = _M0L8_2aentryS824->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4067
        = _M0L3keyS2657 == _M0L3keyS818
          || Moonbit_array_length(_M0L3keyS2657)
             == Moonbit_array_length(_M0L3keyS818)
             && 0
                == memcmp(_M0L3keyS2657, _M0L3keyS818, Moonbit_array_length(_M0L3keyS2657) * 2);
      } else {
        _if__result_4067 = 0;
      }
      if (_if__result_4067) {
        void* _M0L5valueS2660 = _M0L8_2aentryS824->$5;
        void* _M0L6_2atmpS2659;
        moonbit_incref(_M0L5valueS2660);
        _M0L6_2atmpS2659 = _M0L5valueS2660;
        return _M0L6_2atmpS2659;
      } else {
        moonbit_incref(_M0L8_2aentryS824);
      }
      _M0L3pslS2661 = _M0L8_2aentryS824->$2;
      moonbit_decref(_M0L8_2aentryS824);
      if (_M0L1iS819 > _M0L3pslS2661) {
        void* _M0L6_2atmpS2662 = 0;
        return _M0L6_2atmpS2662;
      }
      _M0L6_2atmpS2663 = _M0L1iS819 + 1;
      _M0L6_2atmpS2665 = _M0L3idxS820 + 1;
      _M0L14capacity__maskS2666 = _M0L4selfS822->$3;
      _M0L6_2atmpS2664 = _M0L6_2atmpS2665 & _M0L14capacity__maskS2666;
      _M0L1iS819 = _M0L6_2atmpS2663;
      _M0L3idxS820 = _M0L6_2atmpS2664;
      continue;
    }
    break;
  }
}

moonbit_string_t _M0MPB3Map3getGssE(
  struct _M0TPB3MapGssE* _M0L4selfS831,
  moonbit_string_t _M0L3keyS827
) {
  int32_t _M0L4hashS826;
  int32_t _M0L14capacity__maskS2683;
  int32_t _M0L6_2atmpS2682;
  int32_t _M0L1iS828;
  int32_t _M0L3idxS829;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS826 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS827);
  _M0L14capacity__maskS2683 = _M0L4selfS831->$3;
  _M0L6_2atmpS2682 = _M0L4hashS826 & _M0L14capacity__maskS2683;
  _M0L1iS828 = 0;
  _M0L3idxS829 = _M0L6_2atmpS2682;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2681 = _M0L4selfS831->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS830;
    if (
      _M0L3idxS829 < 0
      || _M0L3idxS829 >= Moonbit_array_length(_M0L7entriesS2681)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS830
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2681[_M0L3idxS829];
    if (_M0L7_2abindS830 == 0) {
      moonbit_string_t _M0L6_2atmpS2670 = 0;
      return _M0L6_2atmpS2670;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS832 = _M0L7_2abindS830;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS833 = _M0L7_2aSomeS832;
      int32_t _M0L4hashS2672 = _M0L8_2aentryS833->$3;
      int32_t _if__result_4069;
      int32_t _M0L3pslS2675;
      int32_t _M0L6_2atmpS2677;
      int32_t _M0L6_2atmpS2679;
      int32_t _M0L14capacity__maskS2680;
      int32_t _M0L6_2atmpS2678;
      if (_M0L4hashS2672 == _M0L4hashS826) {
        moonbit_string_t _M0L3keyS2671 = _M0L8_2aentryS833->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4069
        = _M0L3keyS2671 == _M0L3keyS827
          || Moonbit_array_length(_M0L3keyS2671)
             == Moonbit_array_length(_M0L3keyS827)
             && 0
                == memcmp(_M0L3keyS2671, _M0L3keyS827, Moonbit_array_length(_M0L3keyS2671) * 2);
      } else {
        _if__result_4069 = 0;
      }
      if (_if__result_4069) {
        moonbit_string_t _M0L5valueS2674 = _M0L8_2aentryS833->$5;
        moonbit_string_t _M0L6_2atmpS2673;
        moonbit_incref(_M0L5valueS2674);
        _M0L6_2atmpS2673 = _M0L5valueS2674;
        return _M0L6_2atmpS2673;
      } else {
        moonbit_incref(_M0L8_2aentryS833);
      }
      _M0L3pslS2675 = _M0L8_2aentryS833->$2;
      moonbit_decref(_M0L8_2aentryS833);
      if (_M0L1iS828 > _M0L3pslS2675) {
        moonbit_string_t _M0L6_2atmpS2676 = 0;
        return _M0L6_2atmpS2676;
      }
      _M0L6_2atmpS2677 = _M0L1iS828 + 1;
      _M0L6_2atmpS2679 = _M0L3idxS829 + 1;
      _M0L14capacity__maskS2680 = _M0L4selfS831->$3;
      _M0L6_2atmpS2678 = _M0L6_2atmpS2679 & _M0L14capacity__maskS2680;
      _M0L1iS828 = _M0L6_2atmpS2677;
      _M0L3idxS829 = _M0L6_2atmpS2678;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS840,
  moonbit_string_t _M0L3keyS836
) {
  int32_t _M0L4hashS835;
  int32_t _M0L14capacity__maskS2697;
  int32_t _M0L6_2atmpS2696;
  int32_t _M0L1iS837;
  int32_t _M0L3idxS838;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS835 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS836);
  _M0L14capacity__maskS2697 = _M0L4selfS840->$3;
  _M0L6_2atmpS2696 = _M0L4hashS835 & _M0L14capacity__maskS2697;
  _M0L1iS837 = 0;
  _M0L3idxS838 = _M0L6_2atmpS2696;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2695 = _M0L4selfS840->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS839;
    if (
      _M0L3idxS838 < 0
      || _M0L3idxS838 >= Moonbit_array_length(_M0L7entriesS2695)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS839
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2695[_M0L3idxS838];
    if (_M0L7_2abindS839 == 0) {
      void* _M0L4NoneS2684 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS2684;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS841 = _M0L7_2abindS839;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS842 = _M0L7_2aSomeS841;
      int32_t _M0L4hashS2686 = _M0L8_2aentryS842->$3;
      int32_t _if__result_4071;
      int32_t _M0L3pslS2689;
      int32_t _M0L6_2atmpS2691;
      int32_t _M0L6_2atmpS2693;
      int32_t _M0L14capacity__maskS2694;
      int32_t _M0L6_2atmpS2692;
      if (_M0L4hashS2686 == _M0L4hashS835) {
        moonbit_string_t _M0L3keyS2685 = _M0L8_2aentryS842->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4071
        = _M0L3keyS2685 == _M0L3keyS836
          || Moonbit_array_length(_M0L3keyS2685)
             == Moonbit_array_length(_M0L3keyS836)
             && 0
                == memcmp(_M0L3keyS2685, _M0L3keyS836, Moonbit_array_length(_M0L3keyS2685) * 2);
      } else {
        _if__result_4071 = 0;
      }
      if (_if__result_4071) {
        float _M0L5valueS2688 = _M0L8_2aentryS842->$5;
        void* _M0L4SomeS2687 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS2687)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS2687)->$0
        = _M0L5valueS2688;
        return _M0L4SomeS2687;
      } else {
        moonbit_incref(_M0L8_2aentryS842);
      }
      _M0L3pslS2689 = _M0L8_2aentryS842->$2;
      moonbit_decref(_M0L8_2aentryS842);
      if (_M0L1iS837 > _M0L3pslS2689) {
        void* _M0L4NoneS2690 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS2690;
      }
      _M0L6_2atmpS2691 = _M0L1iS837 + 1;
      _M0L6_2atmpS2693 = _M0L3idxS838 + 1;
      _M0L14capacity__maskS2694 = _M0L4selfS840->$3;
      _M0L6_2atmpS2692 = _M0L6_2atmpS2693 & _M0L14capacity__maskS2694;
      _M0L1iS837 = _M0L6_2atmpS2691;
      _M0L3idxS838 = _M0L6_2atmpS2692;
      continue;
    }
    break;
  }
}

int64_t _M0MPB3Map3getGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS849,
  moonbit_string_t _M0L3keyS845
) {
  int32_t _M0L4hashS844;
  int32_t _M0L14capacity__maskS2709;
  int32_t _M0L6_2atmpS2708;
  int32_t _M0L1iS846;
  int32_t _M0L3idxS847;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS844 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS845);
  _M0L14capacity__maskS2709 = _M0L4selfS849->$3;
  _M0L6_2atmpS2708 = _M0L4hashS844 & _M0L14capacity__maskS2709;
  _M0L1iS846 = 0;
  _M0L3idxS847 = _M0L6_2atmpS2708;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2707 = _M0L4selfS849->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS848;
    if (
      _M0L3idxS847 < 0
      || _M0L3idxS847 >= Moonbit_array_length(_M0L7entriesS2707)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS848
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2707[_M0L3idxS847];
    if (_M0L7_2abindS848 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS850 = _M0L7_2abindS848;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS851 = _M0L7_2aSomeS850;
      int32_t _M0L4hashS2699 = _M0L8_2aentryS851->$3;
      int32_t _if__result_4073;
      int32_t _M0L3pslS2702;
      int32_t _M0L6_2atmpS2703;
      int32_t _M0L6_2atmpS2705;
      int32_t _M0L14capacity__maskS2706;
      int32_t _M0L6_2atmpS2704;
      if (_M0L4hashS2699 == _M0L4hashS844) {
        moonbit_string_t _M0L3keyS2698 = _M0L8_2aentryS851->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4073
        = _M0L3keyS2698 == _M0L3keyS845
          || Moonbit_array_length(_M0L3keyS2698)
             == Moonbit_array_length(_M0L3keyS845)
             && 0
                == memcmp(_M0L3keyS2698, _M0L3keyS845, Moonbit_array_length(_M0L3keyS2698) * 2);
      } else {
        _if__result_4073 = 0;
      }
      if (_if__result_4073) {
        int32_t _M0L5valueS2701 = _M0L8_2aentryS851->$5;
        int64_t _M0L6_2atmpS2700 = (int64_t)_M0L5valueS2701;
        return _M0L6_2atmpS2700;
      } else {
        moonbit_incref(_M0L8_2aentryS851);
      }
      _M0L3pslS2702 = _M0L8_2aentryS851->$2;
      moonbit_decref(_M0L8_2aentryS851);
      if (_M0L1iS846 > _M0L3pslS2702) {
        return 4294967296ll;
      }
      _M0L6_2atmpS2703 = _M0L1iS846 + 1;
      _M0L6_2atmpS2705 = _M0L3idxS847 + 1;
      _M0L14capacity__maskS2706 = _M0L4selfS849->$3;
      _M0L6_2atmpS2704 = _M0L6_2atmpS2705 & _M0L14capacity__maskS2706;
      _M0L1iS846 = _M0L6_2atmpS2703;
      _M0L3idxS847 = _M0L6_2atmpS2704;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB3Map3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE _M0L3arrS763,
  int64_t _M0L8capacityS765
) {
  int32_t _M0L3endS2610;
  int32_t _M0L5startS2611;
  int32_t _M0L6lengthS762;
  int32_t _M0L8capacityS764;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1mS768;
  int32_t _M0L3endS2607;
  int32_t _M0L5startS2608;
  int32_t _M0L7_2abindS769;
  int32_t _M0L2__S770;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2610 = _M0L3arrS763.$2;
  _M0L5startS2611 = _M0L3arrS763.$1;
  _M0L6lengthS762 = _M0L3endS2610 - _M0L5startS2611;
  if (_M0L8capacityS765 == 4294967296ll) {
    if (_M0L6lengthS762 == 0) {
      _M0L8capacityS764 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS764 = _M0FPB21capacity__for__length(_M0L6lengthS762);
    }
  } else {
    int64_t _M0L7_2aSomeS766 = _M0L8capacityS765;
    int32_t _M0L11_2acapacityS767 = (int32_t)_M0L7_2aSomeS766;
    int32_t _M0L6_2atmpS2609;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2609 = _M0FPB21capacity__for__length(_M0L6lengthS762);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS764
    = _M0MPC13int3Int3max(_M0L11_2acapacityS767, _M0L6_2atmpS2609);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS768
  = _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L8capacityS764);
  _M0L3endS2607 = _M0L3arrS763.$2;
  _M0L5startS2608 = _M0L3arrS763.$1;
  _M0L7_2abindS769 = _M0L3endS2607 - _M0L5startS2608;
  _M0L2__S770 = 0;
  while (1) {
    if (_M0L2__S770 < _M0L7_2abindS769) {
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L3bufS2604 =
        _M0L3arrS763.$0;
      int32_t _M0L5startS2606 = _M0L3arrS763.$1;
      int32_t _M0L6_2atmpS2605 = _M0L5startS2606 + _M0L2__S770;
      struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1eS771 =
        (struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L3bufS2604[
          _M0L6_2atmpS2605
        ];
      moonbit_string_t _M0L6_2atmpS2601 = _M0L1eS771->$0;
      void* _M0L6_2atmpS2602 = _M0L1eS771->$1;
      int32_t _M0L6_2atmpS2603;
      moonbit_incref(_M0L6_2atmpS2602);
      moonbit_incref(_M0L6_2atmpS2601);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L1mS768, _M0L6_2atmpS2601, _M0L6_2atmpS2602);
      moonbit_decref(_M0L6_2atmpS2601);
      moonbit_decref(_M0L6_2atmpS2602);
      _M0L6_2atmpS2603 = _M0L2__S770 + 1;
      _M0L2__S770 = _M0L6_2atmpS2603;
      continue;
    }
    break;
  }
  return _M0L1mS768;
}

struct _M0TPB3MapGsiE* _M0MPB3Map3MapGsiE(
  struct _M0TPB9ArrayViewGUsiEE _M0L3arrS774,
  int64_t _M0L8capacityS776
) {
  int32_t _M0L3endS2621;
  int32_t _M0L5startS2622;
  int32_t _M0L6lengthS773;
  int32_t _M0L8capacityS775;
  struct _M0TPB3MapGsiE* _M0L1mS779;
  int32_t _M0L3endS2618;
  int32_t _M0L5startS2619;
  int32_t _M0L7_2abindS780;
  int32_t _M0L2__S781;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2621 = _M0L3arrS774.$2;
  _M0L5startS2622 = _M0L3arrS774.$1;
  _M0L6lengthS773 = _M0L3endS2621 - _M0L5startS2622;
  if (_M0L8capacityS776 == 4294967296ll) {
    if (_M0L6lengthS773 == 0) {
      _M0L8capacityS775 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS775 = _M0FPB21capacity__for__length(_M0L6lengthS773);
    }
  } else {
    int64_t _M0L7_2aSomeS777 = _M0L8capacityS776;
    int32_t _M0L11_2acapacityS778 = (int32_t)_M0L7_2aSomeS777;
    int32_t _M0L6_2atmpS2620;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2620 = _M0FPB21capacity__for__length(_M0L6lengthS773);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS775
    = _M0MPC13int3Int3max(_M0L11_2acapacityS778, _M0L6_2atmpS2620);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS779 = _M0FPB8new__mapGsiE(_M0L8capacityS775);
  _M0L3endS2618 = _M0L3arrS774.$2;
  _M0L5startS2619 = _M0L3arrS774.$1;
  _M0L7_2abindS780 = _M0L3endS2618 - _M0L5startS2619;
  _M0L2__S781 = 0;
  while (1) {
    if (_M0L2__S781 < _M0L7_2abindS780) {
      struct _M0TUsiE** _M0L3bufS2615 = _M0L3arrS774.$0;
      int32_t _M0L5startS2617 = _M0L3arrS774.$1;
      int32_t _M0L6_2atmpS2616 = _M0L5startS2617 + _M0L2__S781;
      struct _M0TUsiE* _M0L1eS782 =
        (struct _M0TUsiE*)_M0L3bufS2615[_M0L6_2atmpS2616];
      moonbit_string_t _M0L6_2atmpS2612 = _M0L1eS782->$0;
      int32_t _M0L6_2atmpS2613 = _M0L1eS782->$1;
      int32_t _M0L6_2atmpS2614;
      moonbit_incref(_M0L6_2atmpS2612);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS779, _M0L6_2atmpS2612, _M0L6_2atmpS2613);
      moonbit_decref(_M0L6_2atmpS2612);
      _M0L6_2atmpS2614 = _M0L2__S781 + 1;
      _M0L2__S781 = _M0L6_2atmpS2614;
      continue;
    }
    break;
  }
  return _M0L1mS779;
}

struct _M0TPB3MapGssE* _M0MPB3Map3MapGssE(
  struct _M0TPB9ArrayViewGUssEE _M0L3arrS785,
  int64_t _M0L8capacityS787
) {
  int32_t _M0L3endS2632;
  int32_t _M0L5startS2633;
  int32_t _M0L6lengthS784;
  int32_t _M0L8capacityS786;
  struct _M0TPB3MapGssE* _M0L1mS790;
  int32_t _M0L3endS2629;
  int32_t _M0L5startS2630;
  int32_t _M0L7_2abindS791;
  int32_t _M0L2__S792;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2632 = _M0L3arrS785.$2;
  _M0L5startS2633 = _M0L3arrS785.$1;
  _M0L6lengthS784 = _M0L3endS2632 - _M0L5startS2633;
  if (_M0L8capacityS787 == 4294967296ll) {
    if (_M0L6lengthS784 == 0) {
      _M0L8capacityS786 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS786 = _M0FPB21capacity__for__length(_M0L6lengthS784);
    }
  } else {
    int64_t _M0L7_2aSomeS788 = _M0L8capacityS787;
    int32_t _M0L11_2acapacityS789 = (int32_t)_M0L7_2aSomeS788;
    int32_t _M0L6_2atmpS2631;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2631 = _M0FPB21capacity__for__length(_M0L6lengthS784);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS786
    = _M0MPC13int3Int3max(_M0L11_2acapacityS789, _M0L6_2atmpS2631);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS790 = _M0FPB8new__mapGssE(_M0L8capacityS786);
  _M0L3endS2629 = _M0L3arrS785.$2;
  _M0L5startS2630 = _M0L3arrS785.$1;
  _M0L7_2abindS791 = _M0L3endS2629 - _M0L5startS2630;
  _M0L2__S792 = 0;
  while (1) {
    if (_M0L2__S792 < _M0L7_2abindS791) {
      struct _M0TUssE** _M0L3bufS2626 = _M0L3arrS785.$0;
      int32_t _M0L5startS2628 = _M0L3arrS785.$1;
      int32_t _M0L6_2atmpS2627 = _M0L5startS2628 + _M0L2__S792;
      struct _M0TUssE* _M0L1eS793 =
        (struct _M0TUssE*)_M0L3bufS2626[_M0L6_2atmpS2627];
      moonbit_string_t _M0L6_2atmpS2623 = _M0L1eS793->$0;
      moonbit_string_t _M0L6_2atmpS2624 = _M0L1eS793->$1;
      int32_t _M0L6_2atmpS2625;
      moonbit_incref(_M0L6_2atmpS2624);
      moonbit_incref(_M0L6_2atmpS2623);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS790, _M0L6_2atmpS2623, _M0L6_2atmpS2624);
      moonbit_decref(_M0L6_2atmpS2623);
      moonbit_decref(_M0L6_2atmpS2624);
      _M0L6_2atmpS2625 = _M0L2__S792 + 1;
      _M0L2__S792 = _M0L6_2atmpS2625;
      continue;
    }
    break;
  }
  return _M0L1mS790;
}

struct _M0TPB3MapGsbE* _M0MPB3Map3MapGsbE(
  struct _M0TPB9ArrayViewGUsbEE _M0L3arrS796,
  int64_t _M0L8capacityS798
) {
  int32_t _M0L3endS2643;
  int32_t _M0L5startS2644;
  int32_t _M0L6lengthS795;
  int32_t _M0L8capacityS797;
  struct _M0TPB3MapGsbE* _M0L1mS801;
  int32_t _M0L3endS2640;
  int32_t _M0L5startS2641;
  int32_t _M0L7_2abindS802;
  int32_t _M0L2__S803;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2643 = _M0L3arrS796.$2;
  _M0L5startS2644 = _M0L3arrS796.$1;
  _M0L6lengthS795 = _M0L3endS2643 - _M0L5startS2644;
  if (_M0L8capacityS798 == 4294967296ll) {
    if (_M0L6lengthS795 == 0) {
      _M0L8capacityS797 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS797 = _M0FPB21capacity__for__length(_M0L6lengthS795);
    }
  } else {
    int64_t _M0L7_2aSomeS799 = _M0L8capacityS798;
    int32_t _M0L11_2acapacityS800 = (int32_t)_M0L7_2aSomeS799;
    int32_t _M0L6_2atmpS2642;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2642 = _M0FPB21capacity__for__length(_M0L6lengthS795);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS797
    = _M0MPC13int3Int3max(_M0L11_2acapacityS800, _M0L6_2atmpS2642);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS801 = _M0FPB8new__mapGsbE(_M0L8capacityS797);
  _M0L3endS2640 = _M0L3arrS796.$2;
  _M0L5startS2641 = _M0L3arrS796.$1;
  _M0L7_2abindS802 = _M0L3endS2640 - _M0L5startS2641;
  _M0L2__S803 = 0;
  while (1) {
    if (_M0L2__S803 < _M0L7_2abindS802) {
      struct _M0TUsbE** _M0L3bufS2637 = _M0L3arrS796.$0;
      int32_t _M0L5startS2639 = _M0L3arrS796.$1;
      int32_t _M0L6_2atmpS2638 = _M0L5startS2639 + _M0L2__S803;
      struct _M0TUsbE* _M0L1eS804 =
        (struct _M0TUsbE*)_M0L3bufS2637[_M0L6_2atmpS2638];
      moonbit_string_t _M0L6_2atmpS2634 = _M0L1eS804->$0;
      int32_t _M0L6_2atmpS2635 = _M0L1eS804->$1;
      int32_t _M0L6_2atmpS2636;
      moonbit_incref(_M0L6_2atmpS2634);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS801, _M0L6_2atmpS2634, _M0L6_2atmpS2635);
      moonbit_decref(_M0L6_2atmpS2634);
      _M0L6_2atmpS2636 = _M0L2__S803 + 1;
      _M0L2__S803 = _M0L6_2atmpS2636;
      continue;
    }
    break;
  }
  return _M0L1mS801;
}

struct _M0TPB3MapGsfE* _M0MPB3Map3MapGsfE(
  struct _M0TPB9ArrayViewGUsfEE _M0L3arrS807,
  int64_t _M0L8capacityS809
) {
  int32_t _M0L3endS2654;
  int32_t _M0L5startS2655;
  int32_t _M0L6lengthS806;
  int32_t _M0L8capacityS808;
  struct _M0TPB3MapGsfE* _M0L1mS812;
  int32_t _M0L3endS2651;
  int32_t _M0L5startS2652;
  int32_t _M0L7_2abindS813;
  int32_t _M0L2__S814;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2654 = _M0L3arrS807.$2;
  _M0L5startS2655 = _M0L3arrS807.$1;
  _M0L6lengthS806 = _M0L3endS2654 - _M0L5startS2655;
  if (_M0L8capacityS809 == 4294967296ll) {
    if (_M0L6lengthS806 == 0) {
      _M0L8capacityS808 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS808 = _M0FPB21capacity__for__length(_M0L6lengthS806);
    }
  } else {
    int64_t _M0L7_2aSomeS810 = _M0L8capacityS809;
    int32_t _M0L11_2acapacityS811 = (int32_t)_M0L7_2aSomeS810;
    int32_t _M0L6_2atmpS2653;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2653 = _M0FPB21capacity__for__length(_M0L6lengthS806);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS808
    = _M0MPC13int3Int3max(_M0L11_2acapacityS811, _M0L6_2atmpS2653);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS812 = _M0FPB8new__mapGsfE(_M0L8capacityS808);
  _M0L3endS2651 = _M0L3arrS807.$2;
  _M0L5startS2652 = _M0L3arrS807.$1;
  _M0L7_2abindS813 = _M0L3endS2651 - _M0L5startS2652;
  _M0L2__S814 = 0;
  while (1) {
    if (_M0L2__S814 < _M0L7_2abindS813) {
      struct _M0TUsfE** _M0L3bufS2648 = _M0L3arrS807.$0;
      int32_t _M0L5startS2650 = _M0L3arrS807.$1;
      int32_t _M0L6_2atmpS2649 = _M0L5startS2650 + _M0L2__S814;
      struct _M0TUsfE* _M0L1eS815 =
        (struct _M0TUsfE*)_M0L3bufS2648[_M0L6_2atmpS2649];
      moonbit_string_t _M0L6_2atmpS2645 = _M0L1eS815->$0;
      float _M0L6_2atmpS2646 = _M0L1eS815->$1;
      int32_t _M0L6_2atmpS2647;
      moonbit_incref(_M0L6_2atmpS2645);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS812, _M0L6_2atmpS2645, _M0L6_2atmpS2646);
      moonbit_decref(_M0L6_2atmpS2645);
      _M0L6_2atmpS2647 = _M0L2__S814 + 1;
      _M0L2__S814 = _M0L6_2atmpS2647;
      continue;
    }
    break;
  }
  return _M0L1mS812;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS747,
  moonbit_string_t _M0L3keyS748,
  moonbit_string_t _M0L5valueS749
) {
  int32_t _M0L6_2atmpS2596;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2596 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS748);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS747, _M0L3keyS748, _M0L5valueS749, _M0L6_2atmpS2596);
  return 0;
}

int32_t _M0MPB3Map3setGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS750,
  moonbit_string_t _M0L3keyS751,
  void* _M0L5valueS752
) {
  int32_t _M0L6_2atmpS2597;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2597 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS751);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS750, _M0L3keyS751, _M0L5valueS752, _M0L6_2atmpS2597);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS753,
  moonbit_string_t _M0L3keyS754,
  int32_t _M0L5valueS755
) {
  int32_t _M0L6_2atmpS2598;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2598 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS754);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS753, _M0L3keyS754, _M0L5valueS755, _M0L6_2atmpS2598);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS756,
  moonbit_string_t _M0L3keyS757,
  int32_t _M0L5valueS758
) {
  int32_t _M0L6_2atmpS2599;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2599 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS757);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS756, _M0L3keyS757, _M0L5valueS758, _M0L6_2atmpS2599);
  return 0;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS759,
  moonbit_string_t _M0L3keyS760,
  float _M0L5valueS761
) {
  int32_t _M0L6_2atmpS2600;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2600 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS760);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS759, _M0L3keyS760, _M0L5valueS761, _M0L6_2atmpS2600);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS670,
  moonbit_string_t _M0L3keyS676,
  moonbit_string_t _M0L5valueS677,
  int32_t _M0L4hashS672
) {
  int32_t _M0L14capacity__maskS2523;
  int32_t _M0L6_2atmpS2522;
  int32_t _M0L3pslS667;
  int32_t _M0L3idxS668;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2523 = _M0L4selfS670->$3;
  _M0L6_2atmpS2522 = _M0L4hashS672 & _M0L14capacity__maskS2523;
  _M0L3pslS667 = 0;
  _M0L3idxS668 = _M0L6_2atmpS2522;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2521 = _M0L4selfS670->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS669;
    if (
      _M0L3idxS668 < 0
      || _M0L3idxS668 >= Moonbit_array_length(_M0L7entriesS2521)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS669
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2521[_M0L3idxS668];
    if (_M0L7_2abindS669 == 0) {
      int32_t _M0L4sizeS2506 = _M0L4selfS670->$1;
      int32_t _M0L8grow__atS2507 = _M0L4selfS670->$4;
      int32_t _M0L7_2abindS673;
      struct _M0TPB5EntryGssE* _M0L7_2abindS674;
      struct _M0TPB5EntryGssE* _M0L5entryS675;
      if (_M0L4sizeS2506 >= _M0L8grow__atS2507) {
        int32_t _M0L14capacity__maskS2509;
        int32_t _M0L6_2atmpS2508;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS670);
        _M0L14capacity__maskS2509 = _M0L4selfS670->$3;
        _M0L6_2atmpS2508 = _M0L4hashS672 & _M0L14capacity__maskS2509;
        _M0L3pslS667 = 0;
        _M0L3idxS668 = _M0L6_2atmpS2508;
        continue;
      }
      _M0L7_2abindS673 = _M0L4selfS670->$6;
      _M0L7_2abindS674 = 0;
      moonbit_incref(_M0L3keyS676);
      moonbit_incref(_M0L5valueS677);
      _M0L5entryS675
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS675)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 78, 0);
      _M0L5entryS675->$0 = _M0L7_2abindS673;
      _M0L5entryS675->$1 = _M0L7_2abindS674;
      _M0L5entryS675->$2 = _M0L3pslS667;
      _M0L5entryS675->$3 = _M0L4hashS672;
      _M0L5entryS675->$4 = _M0L3keyS676;
      _M0L5entryS675->$5 = _M0L5valueS677;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS670, _M0L3idxS668, _M0L5entryS675);
      moonbit_decref(_M0L5entryS675);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS678 = _M0L7_2abindS669;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS679 = _M0L7_2aSomeS678;
      int32_t _M0L4hashS2511 = _M0L14_2acurr__entryS679->$3;
      int32_t _if__result_4080;
      int32_t _M0L3pslS2512;
      int32_t _M0L6_2atmpS2517;
      int32_t _M0L6_2atmpS2519;
      int32_t _M0L14capacity__maskS2520;
      int32_t _M0L6_2atmpS2518;
      if (_M0L4hashS2511 == _M0L4hashS672) {
        moonbit_string_t _M0L3keyS2510 = _M0L14_2acurr__entryS679->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4080
        = _M0L3keyS2510 == _M0L3keyS676
          || Moonbit_array_length(_M0L3keyS2510)
             == Moonbit_array_length(_M0L3keyS676)
             && 0
                == memcmp(_M0L3keyS2510, _M0L3keyS676, Moonbit_array_length(_M0L3keyS2510) * 2);
      } else {
        _if__result_4080 = 0;
      }
      if (_if__result_4080) {
        moonbit_string_t _M0L6_2aoldS3667 = _M0L14_2acurr__entryS679->$5;
        moonbit_incref(_M0L5valueS677);
        moonbit_decref(_M0L6_2aoldS3667);
        _M0L14_2acurr__entryS679->$5 = _M0L5valueS677;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS679);
      }
      _M0L3pslS2512 = _M0L14_2acurr__entryS679->$2;
      if (_M0L3pslS667 > _M0L3pslS2512) {
        int32_t _M0L4sizeS2513 = _M0L4selfS670->$1;
        int32_t _M0L8grow__atS2514 = _M0L4selfS670->$4;
        int32_t _M0L7_2abindS680;
        struct _M0TPB5EntryGssE* _M0L7_2abindS681;
        struct _M0TPB5EntryGssE* _M0L5entryS682;
        if (_M0L4sizeS2513 >= _M0L8grow__atS2514) {
          int32_t _M0L14capacity__maskS2516;
          int32_t _M0L6_2atmpS2515;
          moonbit_decref(_M0L14_2acurr__entryS679);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS670);
          _M0L14capacity__maskS2516 = _M0L4selfS670->$3;
          _M0L6_2atmpS2515 = _M0L4hashS672 & _M0L14capacity__maskS2516;
          _M0L3pslS667 = 0;
          _M0L3idxS668 = _M0L6_2atmpS2515;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS670, _M0L3idxS668, _M0L14_2acurr__entryS679);
        moonbit_decref(_M0L14_2acurr__entryS679);
        _M0L7_2abindS680 = _M0L4selfS670->$6;
        _M0L7_2abindS681 = 0;
        moonbit_incref(_M0L3keyS676);
        moonbit_incref(_M0L5valueS677);
        _M0L5entryS682
        = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
        Moonbit_object_header(_M0L5entryS682)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 78, 0);
        _M0L5entryS682->$0 = _M0L7_2abindS680;
        _M0L5entryS682->$1 = _M0L7_2abindS681;
        _M0L5entryS682->$2 = _M0L3pslS667;
        _M0L5entryS682->$3 = _M0L4hashS672;
        _M0L5entryS682->$4 = _M0L3keyS676;
        _M0L5entryS682->$5 = _M0L5valueS677;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS670, _M0L3idxS668, _M0L5entryS682);
        moonbit_decref(_M0L5entryS682);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS679);
      }
      _M0L6_2atmpS2517 = _M0L3pslS667 + 1;
      _M0L6_2atmpS2519 = _M0L3idxS668 + 1;
      _M0L14capacity__maskS2520 = _M0L4selfS670->$3;
      _M0L6_2atmpS2518 = _M0L6_2atmpS2519 & _M0L14capacity__maskS2520;
      _M0L3pslS667 = _M0L6_2atmpS2517;
      _M0L3idxS668 = _M0L6_2atmpS2518;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS686,
  moonbit_string_t _M0L3keyS692,
  void* _M0L5valueS693,
  int32_t _M0L4hashS688
) {
  int32_t _M0L14capacity__maskS2541;
  int32_t _M0L6_2atmpS2540;
  int32_t _M0L3pslS683;
  int32_t _M0L3idxS684;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2541 = _M0L4selfS686->$3;
  _M0L6_2atmpS2540 = _M0L4hashS688 & _M0L14capacity__maskS2541;
  _M0L3pslS683 = 0;
  _M0L3idxS684 = _M0L6_2atmpS2540;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2539 =
      _M0L4selfS686->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS685;
    if (
      _M0L3idxS684 < 0
      || _M0L3idxS684 >= Moonbit_array_length(_M0L7entriesS2539)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS685
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2539[
        _M0L3idxS684
      ];
    if (_M0L7_2abindS685 == 0) {
      int32_t _M0L4sizeS2524 = _M0L4selfS686->$1;
      int32_t _M0L8grow__atS2525 = _M0L4selfS686->$4;
      int32_t _M0L7_2abindS689;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS690;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS691;
      if (_M0L4sizeS2524 >= _M0L8grow__atS2525) {
        int32_t _M0L14capacity__maskS2527;
        int32_t _M0L6_2atmpS2526;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS686);
        _M0L14capacity__maskS2527 = _M0L4selfS686->$3;
        _M0L6_2atmpS2526 = _M0L4hashS688 & _M0L14capacity__maskS2527;
        _M0L3pslS683 = 0;
        _M0L3idxS684 = _M0L6_2atmpS2526;
        continue;
      }
      _M0L7_2abindS689 = _M0L4selfS686->$6;
      _M0L7_2abindS690 = 0;
      moonbit_incref(_M0L3keyS692);
      moonbit_incref(_M0L5valueS693);
      _M0L5entryS691
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L5entryS691)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 83, 0);
      _M0L5entryS691->$0 = _M0L7_2abindS689;
      _M0L5entryS691->$1 = _M0L7_2abindS690;
      _M0L5entryS691->$2 = _M0L3pslS683;
      _M0L5entryS691->$3 = _M0L4hashS688;
      _M0L5entryS691->$4 = _M0L3keyS692;
      _M0L5entryS691->$5 = _M0L5valueS693;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS686, _M0L3idxS684, _M0L5entryS691);
      moonbit_decref(_M0L5entryS691);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS694 =
        _M0L7_2abindS685;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS695 =
        _M0L7_2aSomeS694;
      int32_t _M0L4hashS2529 = _M0L14_2acurr__entryS695->$3;
      int32_t _if__result_4082;
      int32_t _M0L3pslS2530;
      int32_t _M0L6_2atmpS2535;
      int32_t _M0L6_2atmpS2537;
      int32_t _M0L14capacity__maskS2538;
      int32_t _M0L6_2atmpS2536;
      if (_M0L4hashS2529 == _M0L4hashS688) {
        moonbit_string_t _M0L3keyS2528 = _M0L14_2acurr__entryS695->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4082
        = _M0L3keyS2528 == _M0L3keyS692
          || Moonbit_array_length(_M0L3keyS2528)
             == Moonbit_array_length(_M0L3keyS692)
             && 0
                == memcmp(_M0L3keyS2528, _M0L3keyS692, Moonbit_array_length(_M0L3keyS2528) * 2);
      } else {
        _if__result_4082 = 0;
      }
      if (_if__result_4082) {
        void* _M0L6_2aoldS3671 = _M0L14_2acurr__entryS695->$5;
        moonbit_incref(_M0L5valueS693);
        moonbit_decref(_M0L6_2aoldS3671);
        _M0L14_2acurr__entryS695->$5 = _M0L5valueS693;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS695);
      }
      _M0L3pslS2530 = _M0L14_2acurr__entryS695->$2;
      if (_M0L3pslS683 > _M0L3pslS2530) {
        int32_t _M0L4sizeS2531 = _M0L4selfS686->$1;
        int32_t _M0L8grow__atS2532 = _M0L4selfS686->$4;
        int32_t _M0L7_2abindS696;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS697;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS698;
        if (_M0L4sizeS2531 >= _M0L8grow__atS2532) {
          int32_t _M0L14capacity__maskS2534;
          int32_t _M0L6_2atmpS2533;
          moonbit_decref(_M0L14_2acurr__entryS695);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS686);
          _M0L14capacity__maskS2534 = _M0L4selfS686->$3;
          _M0L6_2atmpS2533 = _M0L4hashS688 & _M0L14capacity__maskS2534;
          _M0L3pslS683 = 0;
          _M0L3idxS684 = _M0L6_2atmpS2533;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS686, _M0L3idxS684, _M0L14_2acurr__entryS695);
        moonbit_decref(_M0L14_2acurr__entryS695);
        _M0L7_2abindS696 = _M0L4selfS686->$6;
        _M0L7_2abindS697 = 0;
        moonbit_incref(_M0L3keyS692);
        moonbit_incref(_M0L5valueS693);
        _M0L5entryS698
        = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
        Moonbit_object_header(_M0L5entryS698)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 83, 0);
        _M0L5entryS698->$0 = _M0L7_2abindS696;
        _M0L5entryS698->$1 = _M0L7_2abindS697;
        _M0L5entryS698->$2 = _M0L3pslS683;
        _M0L5entryS698->$3 = _M0L4hashS688;
        _M0L5entryS698->$4 = _M0L3keyS692;
        _M0L5entryS698->$5 = _M0L5valueS693;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS686, _M0L3idxS684, _M0L5entryS698);
        moonbit_decref(_M0L5entryS698);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS695);
      }
      _M0L6_2atmpS2535 = _M0L3pslS683 + 1;
      _M0L6_2atmpS2537 = _M0L3idxS684 + 1;
      _M0L14capacity__maskS2538 = _M0L4selfS686->$3;
      _M0L6_2atmpS2536 = _M0L6_2atmpS2537 & _M0L14capacity__maskS2538;
      _M0L3pslS683 = _M0L6_2atmpS2535;
      _M0L3idxS684 = _M0L6_2atmpS2536;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS702,
  moonbit_string_t _M0L3keyS708,
  int32_t _M0L5valueS709,
  int32_t _M0L4hashS704
) {
  int32_t _M0L14capacity__maskS2559;
  int32_t _M0L6_2atmpS2558;
  int32_t _M0L3pslS699;
  int32_t _M0L3idxS700;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2559 = _M0L4selfS702->$3;
  _M0L6_2atmpS2558 = _M0L4hashS704 & _M0L14capacity__maskS2559;
  _M0L3pslS699 = 0;
  _M0L3idxS700 = _M0L6_2atmpS2558;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2557 = _M0L4selfS702->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS701;
    if (
      _M0L3idxS700 < 0
      || _M0L3idxS700 >= Moonbit_array_length(_M0L7entriesS2557)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS701
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2557[_M0L3idxS700];
    if (_M0L7_2abindS701 == 0) {
      int32_t _M0L4sizeS2542 = _M0L4selfS702->$1;
      int32_t _M0L8grow__atS2543 = _M0L4selfS702->$4;
      int32_t _M0L7_2abindS705;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS706;
      struct _M0TPB5EntryGsiE* _M0L5entryS707;
      if (_M0L4sizeS2542 >= _M0L8grow__atS2543) {
        int32_t _M0L14capacity__maskS2545;
        int32_t _M0L6_2atmpS2544;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS702);
        _M0L14capacity__maskS2545 = _M0L4selfS702->$3;
        _M0L6_2atmpS2544 = _M0L4hashS704 & _M0L14capacity__maskS2545;
        _M0L3pslS699 = 0;
        _M0L3idxS700 = _M0L6_2atmpS2544;
        continue;
      }
      _M0L7_2abindS705 = _M0L4selfS702->$6;
      _M0L7_2abindS706 = 0;
      moonbit_incref(_M0L3keyS708);
      _M0L5entryS707
      = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
      Moonbit_object_header(_M0L5entryS707)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 88, 0);
      _M0L5entryS707->$0 = _M0L7_2abindS705;
      _M0L5entryS707->$1 = _M0L7_2abindS706;
      _M0L5entryS707->$2 = _M0L3pslS699;
      _M0L5entryS707->$3 = _M0L4hashS704;
      _M0L5entryS707->$4 = _M0L3keyS708;
      _M0L5entryS707->$5 = _M0L5valueS709;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS702, _M0L3idxS700, _M0L5entryS707);
      moonbit_decref(_M0L5entryS707);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS710 = _M0L7_2abindS701;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS711 = _M0L7_2aSomeS710;
      int32_t _M0L4hashS2547 = _M0L14_2acurr__entryS711->$3;
      int32_t _if__result_4084;
      int32_t _M0L3pslS2548;
      int32_t _M0L6_2atmpS2553;
      int32_t _M0L6_2atmpS2555;
      int32_t _M0L14capacity__maskS2556;
      int32_t _M0L6_2atmpS2554;
      if (_M0L4hashS2547 == _M0L4hashS704) {
        moonbit_string_t _M0L3keyS2546 = _M0L14_2acurr__entryS711->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4084
        = _M0L3keyS2546 == _M0L3keyS708
          || Moonbit_array_length(_M0L3keyS2546)
             == Moonbit_array_length(_M0L3keyS708)
             && 0
                == memcmp(_M0L3keyS2546, _M0L3keyS708, Moonbit_array_length(_M0L3keyS2546) * 2);
      } else {
        _if__result_4084 = 0;
      }
      if (_if__result_4084) {
        _M0L14_2acurr__entryS711->$5 = _M0L5valueS709;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS711);
      }
      _M0L3pslS2548 = _M0L14_2acurr__entryS711->$2;
      if (_M0L3pslS699 > _M0L3pslS2548) {
        int32_t _M0L4sizeS2549 = _M0L4selfS702->$1;
        int32_t _M0L8grow__atS2550 = _M0L4selfS702->$4;
        int32_t _M0L7_2abindS712;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS713;
        struct _M0TPB5EntryGsiE* _M0L5entryS714;
        if (_M0L4sizeS2549 >= _M0L8grow__atS2550) {
          int32_t _M0L14capacity__maskS2552;
          int32_t _M0L6_2atmpS2551;
          moonbit_decref(_M0L14_2acurr__entryS711);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS702);
          _M0L14capacity__maskS2552 = _M0L4selfS702->$3;
          _M0L6_2atmpS2551 = _M0L4hashS704 & _M0L14capacity__maskS2552;
          _M0L3pslS699 = 0;
          _M0L3idxS700 = _M0L6_2atmpS2551;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS702, _M0L3idxS700, _M0L14_2acurr__entryS711);
        moonbit_decref(_M0L14_2acurr__entryS711);
        _M0L7_2abindS712 = _M0L4selfS702->$6;
        _M0L7_2abindS713 = 0;
        moonbit_incref(_M0L3keyS708);
        _M0L5entryS714
        = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
        Moonbit_object_header(_M0L5entryS714)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 88, 0);
        _M0L5entryS714->$0 = _M0L7_2abindS712;
        _M0L5entryS714->$1 = _M0L7_2abindS713;
        _M0L5entryS714->$2 = _M0L3pslS699;
        _M0L5entryS714->$3 = _M0L4hashS704;
        _M0L5entryS714->$4 = _M0L3keyS708;
        _M0L5entryS714->$5 = _M0L5valueS709;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS702, _M0L3idxS700, _M0L5entryS714);
        moonbit_decref(_M0L5entryS714);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS711);
      }
      _M0L6_2atmpS2553 = _M0L3pslS699 + 1;
      _M0L6_2atmpS2555 = _M0L3idxS700 + 1;
      _M0L14capacity__maskS2556 = _M0L4selfS702->$3;
      _M0L6_2atmpS2554 = _M0L6_2atmpS2555 & _M0L14capacity__maskS2556;
      _M0L3pslS699 = _M0L6_2atmpS2553;
      _M0L3idxS700 = _M0L6_2atmpS2554;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS718,
  moonbit_string_t _M0L3keyS724,
  int32_t _M0L5valueS725,
  int32_t _M0L4hashS720
) {
  int32_t _M0L14capacity__maskS2577;
  int32_t _M0L6_2atmpS2576;
  int32_t _M0L3pslS715;
  int32_t _M0L3idxS716;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2577 = _M0L4selfS718->$3;
  _M0L6_2atmpS2576 = _M0L4hashS720 & _M0L14capacity__maskS2577;
  _M0L3pslS715 = 0;
  _M0L3idxS716 = _M0L6_2atmpS2576;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2575 = _M0L4selfS718->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS717;
    if (
      _M0L3idxS716 < 0
      || _M0L3idxS716 >= Moonbit_array_length(_M0L7entriesS2575)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS717
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2575[_M0L3idxS716];
    if (_M0L7_2abindS717 == 0) {
      int32_t _M0L4sizeS2560 = _M0L4selfS718->$1;
      int32_t _M0L8grow__atS2561 = _M0L4selfS718->$4;
      int32_t _M0L7_2abindS721;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS722;
      struct _M0TPB5EntryGsbE* _M0L5entryS723;
      if (_M0L4sizeS2560 >= _M0L8grow__atS2561) {
        int32_t _M0L14capacity__maskS2563;
        int32_t _M0L6_2atmpS2562;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS718);
        _M0L14capacity__maskS2563 = _M0L4selfS718->$3;
        _M0L6_2atmpS2562 = _M0L4hashS720 & _M0L14capacity__maskS2563;
        _M0L3pslS715 = 0;
        _M0L3idxS716 = _M0L6_2atmpS2562;
        continue;
      }
      _M0L7_2abindS721 = _M0L4selfS718->$6;
      _M0L7_2abindS722 = 0;
      moonbit_incref(_M0L3keyS724);
      _M0L5entryS723
      = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
      Moonbit_object_header(_M0L5entryS723)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 92, 0);
      _M0L5entryS723->$0 = _M0L7_2abindS721;
      _M0L5entryS723->$1 = _M0L7_2abindS722;
      _M0L5entryS723->$2 = _M0L3pslS715;
      _M0L5entryS723->$3 = _M0L4hashS720;
      _M0L5entryS723->$4 = _M0L3keyS724;
      _M0L5entryS723->$5 = _M0L5valueS725;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS718, _M0L3idxS716, _M0L5entryS723);
      moonbit_decref(_M0L5entryS723);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS726 = _M0L7_2abindS717;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS727 = _M0L7_2aSomeS726;
      int32_t _M0L4hashS2565 = _M0L14_2acurr__entryS727->$3;
      int32_t _if__result_4086;
      int32_t _M0L3pslS2566;
      int32_t _M0L6_2atmpS2571;
      int32_t _M0L6_2atmpS2573;
      int32_t _M0L14capacity__maskS2574;
      int32_t _M0L6_2atmpS2572;
      if (_M0L4hashS2565 == _M0L4hashS720) {
        moonbit_string_t _M0L3keyS2564 = _M0L14_2acurr__entryS727->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4086
        = _M0L3keyS2564 == _M0L3keyS724
          || Moonbit_array_length(_M0L3keyS2564)
             == Moonbit_array_length(_M0L3keyS724)
             && 0
                == memcmp(_M0L3keyS2564, _M0L3keyS724, Moonbit_array_length(_M0L3keyS2564) * 2);
      } else {
        _if__result_4086 = 0;
      }
      if (_if__result_4086) {
        _M0L14_2acurr__entryS727->$5 = _M0L5valueS725;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS727);
      }
      _M0L3pslS2566 = _M0L14_2acurr__entryS727->$2;
      if (_M0L3pslS715 > _M0L3pslS2566) {
        int32_t _M0L4sizeS2567 = _M0L4selfS718->$1;
        int32_t _M0L8grow__atS2568 = _M0L4selfS718->$4;
        int32_t _M0L7_2abindS728;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS729;
        struct _M0TPB5EntryGsbE* _M0L5entryS730;
        if (_M0L4sizeS2567 >= _M0L8grow__atS2568) {
          int32_t _M0L14capacity__maskS2570;
          int32_t _M0L6_2atmpS2569;
          moonbit_decref(_M0L14_2acurr__entryS727);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS718);
          _M0L14capacity__maskS2570 = _M0L4selfS718->$3;
          _M0L6_2atmpS2569 = _M0L4hashS720 & _M0L14capacity__maskS2570;
          _M0L3pslS715 = 0;
          _M0L3idxS716 = _M0L6_2atmpS2569;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS718, _M0L3idxS716, _M0L14_2acurr__entryS727);
        moonbit_decref(_M0L14_2acurr__entryS727);
        _M0L7_2abindS728 = _M0L4selfS718->$6;
        _M0L7_2abindS729 = 0;
        moonbit_incref(_M0L3keyS724);
        _M0L5entryS730
        = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
        Moonbit_object_header(_M0L5entryS730)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 92, 0);
        _M0L5entryS730->$0 = _M0L7_2abindS728;
        _M0L5entryS730->$1 = _M0L7_2abindS729;
        _M0L5entryS730->$2 = _M0L3pslS715;
        _M0L5entryS730->$3 = _M0L4hashS720;
        _M0L5entryS730->$4 = _M0L3keyS724;
        _M0L5entryS730->$5 = _M0L5valueS725;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS718, _M0L3idxS716, _M0L5entryS730);
        moonbit_decref(_M0L5entryS730);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS727);
      }
      _M0L6_2atmpS2571 = _M0L3pslS715 + 1;
      _M0L6_2atmpS2573 = _M0L3idxS716 + 1;
      _M0L14capacity__maskS2574 = _M0L4selfS718->$3;
      _M0L6_2atmpS2572 = _M0L6_2atmpS2573 & _M0L14capacity__maskS2574;
      _M0L3pslS715 = _M0L6_2atmpS2571;
      _M0L3idxS716 = _M0L6_2atmpS2572;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS734,
  moonbit_string_t _M0L3keyS740,
  float _M0L5valueS741,
  int32_t _M0L4hashS736
) {
  int32_t _M0L14capacity__maskS2595;
  int32_t _M0L6_2atmpS2594;
  int32_t _M0L3pslS731;
  int32_t _M0L3idxS732;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2595 = _M0L4selfS734->$3;
  _M0L6_2atmpS2594 = _M0L4hashS736 & _M0L14capacity__maskS2595;
  _M0L3pslS731 = 0;
  _M0L3idxS732 = _M0L6_2atmpS2594;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2593 = _M0L4selfS734->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS733;
    if (
      _M0L3idxS732 < 0
      || _M0L3idxS732 >= Moonbit_array_length(_M0L7entriesS2593)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS733
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2593[_M0L3idxS732];
    if (_M0L7_2abindS733 == 0) {
      int32_t _M0L4sizeS2578 = _M0L4selfS734->$1;
      int32_t _M0L8grow__atS2579 = _M0L4selfS734->$4;
      int32_t _M0L7_2abindS737;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS738;
      struct _M0TPB5EntryGsfE* _M0L5entryS739;
      if (_M0L4sizeS2578 >= _M0L8grow__atS2579) {
        int32_t _M0L14capacity__maskS2581;
        int32_t _M0L6_2atmpS2580;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS734);
        _M0L14capacity__maskS2581 = _M0L4selfS734->$3;
        _M0L6_2atmpS2580 = _M0L4hashS736 & _M0L14capacity__maskS2581;
        _M0L3pslS731 = 0;
        _M0L3idxS732 = _M0L6_2atmpS2580;
        continue;
      }
      _M0L7_2abindS737 = _M0L4selfS734->$6;
      _M0L7_2abindS738 = 0;
      moonbit_incref(_M0L3keyS740);
      _M0L5entryS739
      = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
      Moonbit_object_header(_M0L5entryS739)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
      _M0L5entryS739->$0 = _M0L7_2abindS737;
      _M0L5entryS739->$1 = _M0L7_2abindS738;
      _M0L5entryS739->$2 = _M0L3pslS731;
      _M0L5entryS739->$3 = _M0L4hashS736;
      _M0L5entryS739->$4 = _M0L3keyS740;
      _M0L5entryS739->$5 = _M0L5valueS741;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS734, _M0L3idxS732, _M0L5entryS739);
      moonbit_decref(_M0L5entryS739);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS742 = _M0L7_2abindS733;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS743 = _M0L7_2aSomeS742;
      int32_t _M0L4hashS2583 = _M0L14_2acurr__entryS743->$3;
      int32_t _if__result_4088;
      int32_t _M0L3pslS2584;
      int32_t _M0L6_2atmpS2589;
      int32_t _M0L6_2atmpS2591;
      int32_t _M0L14capacity__maskS2592;
      int32_t _M0L6_2atmpS2590;
      if (_M0L4hashS2583 == _M0L4hashS736) {
        moonbit_string_t _M0L3keyS2582 = _M0L14_2acurr__entryS743->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4088
        = _M0L3keyS2582 == _M0L3keyS740
          || Moonbit_array_length(_M0L3keyS2582)
             == Moonbit_array_length(_M0L3keyS740)
             && 0
                == memcmp(_M0L3keyS2582, _M0L3keyS740, Moonbit_array_length(_M0L3keyS2582) * 2);
      } else {
        _if__result_4088 = 0;
      }
      if (_if__result_4088) {
        _M0L14_2acurr__entryS743->$5 = _M0L5valueS741;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS743);
      }
      _M0L3pslS2584 = _M0L14_2acurr__entryS743->$2;
      if (_M0L3pslS731 > _M0L3pslS2584) {
        int32_t _M0L4sizeS2585 = _M0L4selfS734->$1;
        int32_t _M0L8grow__atS2586 = _M0L4selfS734->$4;
        int32_t _M0L7_2abindS744;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS745;
        struct _M0TPB5EntryGsfE* _M0L5entryS746;
        if (_M0L4sizeS2585 >= _M0L8grow__atS2586) {
          int32_t _M0L14capacity__maskS2588;
          int32_t _M0L6_2atmpS2587;
          moonbit_decref(_M0L14_2acurr__entryS743);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS734);
          _M0L14capacity__maskS2588 = _M0L4selfS734->$3;
          _M0L6_2atmpS2587 = _M0L4hashS736 & _M0L14capacity__maskS2588;
          _M0L3pslS731 = 0;
          _M0L3idxS732 = _M0L6_2atmpS2587;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS734, _M0L3idxS732, _M0L14_2acurr__entryS743);
        moonbit_decref(_M0L14_2acurr__entryS743);
        _M0L7_2abindS744 = _M0L4selfS734->$6;
        _M0L7_2abindS745 = 0;
        moonbit_incref(_M0L3keyS740);
        _M0L5entryS746
        = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
        Moonbit_object_header(_M0L5entryS746)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
        _M0L5entryS746->$0 = _M0L7_2abindS744;
        _M0L5entryS746->$1 = _M0L7_2abindS745;
        _M0L5entryS746->$2 = _M0L3pslS731;
        _M0L5entryS746->$3 = _M0L4hashS736;
        _M0L5entryS746->$4 = _M0L3keyS740;
        _M0L5entryS746->$5 = _M0L5valueS741;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS734, _M0L3idxS732, _M0L5entryS746);
        moonbit_decref(_M0L5entryS746);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS743);
      }
      _M0L6_2atmpS2589 = _M0L3pslS731 + 1;
      _M0L6_2atmpS2591 = _M0L3idxS732 + 1;
      _M0L14capacity__maskS2592 = _M0L4selfS734->$3;
      _M0L6_2atmpS2590 = _M0L6_2atmpS2591 & _M0L14capacity__maskS2592;
      _M0L3pslS731 = _M0L6_2atmpS2589;
      _M0L3idxS732 = _M0L6_2atmpS2590;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE* _M0L4selfS628) {
  struct _M0TPB5EntryGssE* _M0L9old__headS627;
  int32_t _M0L8capacityS2473;
  int32_t _M0L13new__capacityS629;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2467;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS2466;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS3687;
  int32_t _M0L6_2atmpS2468;
  int32_t _M0L8capacityS2470;
  int32_t _M0L6_2atmpS2469;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2471;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3686;
  struct _M0TPB5EntryGssE* _M0L1xS630;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS627 = _M0L4selfS628->$5;
  _M0L8capacityS2473 = _M0L4selfS628->$2;
  _M0L13new__capacityS629 = _M0L8capacityS2473 << 1;
  _M0L6_2atmpS2467 = 0;
  _M0L6_2atmpS2466
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS629, _M0L6_2atmpS2467);
  _M0L6_2aoldS3687 = _M0L4selfS628->$0;
  if (_M0L9old__headS627) {
    moonbit_incref(_M0L9old__headS627);
  }
  moonbit_decref(_M0L6_2aoldS3687);
  _M0L4selfS628->$0 = _M0L6_2atmpS2466;
  _M0L4selfS628->$2 = _M0L13new__capacityS629;
  _M0L6_2atmpS2468 = _M0L13new__capacityS629 - 1;
  _M0L4selfS628->$3 = _M0L6_2atmpS2468;
  _M0L8capacityS2470 = _M0L4selfS628->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2469 = _M0FPB21calc__grow__threshold(_M0L8capacityS2470);
  _M0L4selfS628->$4 = _M0L6_2atmpS2469;
  _M0L4selfS628->$1 = 0;
  _M0L6_2atmpS2471 = 0;
  _M0L6_2aoldS3686 = _M0L4selfS628->$5;
  if (_M0L6_2aoldS3686) {
    moonbit_decref(_M0L6_2aoldS3686);
  }
  _M0L4selfS628->$5 = _M0L6_2atmpS2471;
  _M0L4selfS628->$6 = -1;
  _M0L1xS630 = _M0L9old__headS627;
  while (1) {
    if (_M0L1xS630 == 0) {
      if (_M0L1xS630) {
        moonbit_decref(_M0L1xS630);
      }
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS632 = _M0L1xS630;
      struct _M0TPB5EntryGssE* _M0L4_2aeS633 = _M0L7_2aSomeS632;
      struct _M0TPB5EntryGssE* _M0L15next__in__chainS634 = _M0L4_2aeS633->$1;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2472 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3684 = _M0L4_2aeS633->$1;
      if (_M0L15next__in__chainS634) {
        moonbit_incref(_M0L15next__in__chainS634);
      }
      if (_M0L6_2aoldS3684) {
        moonbit_decref(_M0L6_2aoldS3684);
      }
      _M0L4_2aeS633->$1 = _M0L6_2atmpS2472;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGssE(_M0L4selfS628, _M0L4_2aeS633);
      moonbit_decref(_M0L4_2aeS633);
      _M0L1xS630 = _M0L15next__in__chainS634;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS636
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L9old__headS635;
  int32_t _M0L8capacityS2481;
  int32_t _M0L13new__capacityS637;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2475;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2atmpS2474;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L6_2aoldS3692;
  int32_t _M0L6_2atmpS2476;
  int32_t _M0L8capacityS2478;
  int32_t _M0L6_2atmpS2477;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2479;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3691;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1xS638;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS635 = _M0L4selfS636->$5;
  _M0L8capacityS2481 = _M0L4selfS636->$2;
  _M0L13new__capacityS637 = _M0L8capacityS2481 << 1;
  _M0L6_2atmpS2475 = 0;
  _M0L6_2atmpS2474
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS637, _M0L6_2atmpS2475);
  _M0L6_2aoldS3692 = _M0L4selfS636->$0;
  if (_M0L9old__headS635) {
    moonbit_incref(_M0L9old__headS635);
  }
  moonbit_decref(_M0L6_2aoldS3692);
  _M0L4selfS636->$0 = _M0L6_2atmpS2474;
  _M0L4selfS636->$2 = _M0L13new__capacityS637;
  _M0L6_2atmpS2476 = _M0L13new__capacityS637 - 1;
  _M0L4selfS636->$3 = _M0L6_2atmpS2476;
  _M0L8capacityS2478 = _M0L4selfS636->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2477 = _M0FPB21calc__grow__threshold(_M0L8capacityS2478);
  _M0L4selfS636->$4 = _M0L6_2atmpS2477;
  _M0L4selfS636->$1 = 0;
  _M0L6_2atmpS2479 = 0;
  _M0L6_2aoldS3691 = _M0L4selfS636->$5;
  if (_M0L6_2aoldS3691) {
    moonbit_decref(_M0L6_2aoldS3691);
  }
  _M0L4selfS636->$5 = _M0L6_2atmpS2479;
  _M0L4selfS636->$6 = -1;
  _M0L1xS638 = _M0L9old__headS635;
  while (1) {
    if (_M0L1xS638 == 0) {
      if (_M0L1xS638) {
        moonbit_decref(_M0L1xS638);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS640 =
        _M0L1xS638;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4_2aeS641 =
        _M0L7_2aSomeS640;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L15next__in__chainS642 =
        _M0L4_2aeS641->$1;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2480 =
        0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3689 =
        _M0L4_2aeS641->$1;
      if (_M0L15next__in__chainS642) {
        moonbit_incref(_M0L15next__in__chainS642);
      }
      if (_M0L6_2aoldS3689) {
        moonbit_decref(_M0L6_2aoldS3689);
      }
      _M0L4_2aeS641->$1 = _M0L6_2atmpS2480;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS636, _M0L4_2aeS641);
      moonbit_decref(_M0L4_2aeS641);
      _M0L1xS638 = _M0L15next__in__chainS642;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE* _M0L4selfS644) {
  struct _M0TPB5EntryGsiE* _M0L9old__headS643;
  int32_t _M0L8capacityS2489;
  int32_t _M0L13new__capacityS645;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2483;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS2482;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS3697;
  int32_t _M0L6_2atmpS2484;
  int32_t _M0L8capacityS2486;
  int32_t _M0L6_2atmpS2485;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2487;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3696;
  struct _M0TPB5EntryGsiE* _M0L1xS646;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS643 = _M0L4selfS644->$5;
  _M0L8capacityS2489 = _M0L4selfS644->$2;
  _M0L13new__capacityS645 = _M0L8capacityS2489 << 1;
  _M0L6_2atmpS2483 = 0;
  _M0L6_2atmpS2482
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS645, _M0L6_2atmpS2483);
  _M0L6_2aoldS3697 = _M0L4selfS644->$0;
  if (_M0L9old__headS643) {
    moonbit_incref(_M0L9old__headS643);
  }
  moonbit_decref(_M0L6_2aoldS3697);
  _M0L4selfS644->$0 = _M0L6_2atmpS2482;
  _M0L4selfS644->$2 = _M0L13new__capacityS645;
  _M0L6_2atmpS2484 = _M0L13new__capacityS645 - 1;
  _M0L4selfS644->$3 = _M0L6_2atmpS2484;
  _M0L8capacityS2486 = _M0L4selfS644->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2485 = _M0FPB21calc__grow__threshold(_M0L8capacityS2486);
  _M0L4selfS644->$4 = _M0L6_2atmpS2485;
  _M0L4selfS644->$1 = 0;
  _M0L6_2atmpS2487 = 0;
  _M0L6_2aoldS3696 = _M0L4selfS644->$5;
  if (_M0L6_2aoldS3696) {
    moonbit_decref(_M0L6_2aoldS3696);
  }
  _M0L4selfS644->$5 = _M0L6_2atmpS2487;
  _M0L4selfS644->$6 = -1;
  _M0L1xS646 = _M0L9old__headS643;
  while (1) {
    if (_M0L1xS646 == 0) {
      if (_M0L1xS646) {
        moonbit_decref(_M0L1xS646);
      }
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS648 = _M0L1xS646;
      struct _M0TPB5EntryGsiE* _M0L4_2aeS649 = _M0L7_2aSomeS648;
      struct _M0TPB5EntryGsiE* _M0L15next__in__chainS650 = _M0L4_2aeS649->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2488 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3694 = _M0L4_2aeS649->$1;
      if (_M0L15next__in__chainS650) {
        moonbit_incref(_M0L15next__in__chainS650);
      }
      if (_M0L6_2aoldS3694) {
        moonbit_decref(_M0L6_2aoldS3694);
      }
      _M0L4_2aeS649->$1 = _M0L6_2atmpS2488;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsiE(_M0L4selfS644, _M0L4_2aeS649);
      moonbit_decref(_M0L4_2aeS649);
      _M0L1xS646 = _M0L15next__in__chainS650;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE* _M0L4selfS652) {
  struct _M0TPB5EntryGsbE* _M0L9old__headS651;
  int32_t _M0L8capacityS2497;
  int32_t _M0L13new__capacityS653;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2491;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS2490;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS3702;
  int32_t _M0L6_2atmpS2492;
  int32_t _M0L8capacityS2494;
  int32_t _M0L6_2atmpS2493;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2495;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3701;
  struct _M0TPB5EntryGsbE* _M0L1xS654;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS651 = _M0L4selfS652->$5;
  _M0L8capacityS2497 = _M0L4selfS652->$2;
  _M0L13new__capacityS653 = _M0L8capacityS2497 << 1;
  _M0L6_2atmpS2491 = 0;
  _M0L6_2atmpS2490
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS653, _M0L6_2atmpS2491);
  _M0L6_2aoldS3702 = _M0L4selfS652->$0;
  if (_M0L9old__headS651) {
    moonbit_incref(_M0L9old__headS651);
  }
  moonbit_decref(_M0L6_2aoldS3702);
  _M0L4selfS652->$0 = _M0L6_2atmpS2490;
  _M0L4selfS652->$2 = _M0L13new__capacityS653;
  _M0L6_2atmpS2492 = _M0L13new__capacityS653 - 1;
  _M0L4selfS652->$3 = _M0L6_2atmpS2492;
  _M0L8capacityS2494 = _M0L4selfS652->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2493 = _M0FPB21calc__grow__threshold(_M0L8capacityS2494);
  _M0L4selfS652->$4 = _M0L6_2atmpS2493;
  _M0L4selfS652->$1 = 0;
  _M0L6_2atmpS2495 = 0;
  _M0L6_2aoldS3701 = _M0L4selfS652->$5;
  if (_M0L6_2aoldS3701) {
    moonbit_decref(_M0L6_2aoldS3701);
  }
  _M0L4selfS652->$5 = _M0L6_2atmpS2495;
  _M0L4selfS652->$6 = -1;
  _M0L1xS654 = _M0L9old__headS651;
  while (1) {
    if (_M0L1xS654 == 0) {
      if (_M0L1xS654) {
        moonbit_decref(_M0L1xS654);
      }
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS656 = _M0L1xS654;
      struct _M0TPB5EntryGsbE* _M0L4_2aeS657 = _M0L7_2aSomeS656;
      struct _M0TPB5EntryGsbE* _M0L15next__in__chainS658 = _M0L4_2aeS657->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2496 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3699 = _M0L4_2aeS657->$1;
      if (_M0L15next__in__chainS658) {
        moonbit_incref(_M0L15next__in__chainS658);
      }
      if (_M0L6_2aoldS3699) {
        moonbit_decref(_M0L6_2aoldS3699);
      }
      _M0L4_2aeS657->$1 = _M0L6_2atmpS2496;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsbE(_M0L4selfS652, _M0L4_2aeS657);
      moonbit_decref(_M0L4_2aeS657);
      _M0L1xS654 = _M0L15next__in__chainS658;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE* _M0L4selfS660) {
  struct _M0TPB5EntryGsfE* _M0L9old__headS659;
  int32_t _M0L8capacityS2505;
  int32_t _M0L13new__capacityS661;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2499;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS2498;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS3707;
  int32_t _M0L6_2atmpS2500;
  int32_t _M0L8capacityS2502;
  int32_t _M0L6_2atmpS2501;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2503;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3706;
  struct _M0TPB5EntryGsfE* _M0L1xS662;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS659 = _M0L4selfS660->$5;
  _M0L8capacityS2505 = _M0L4selfS660->$2;
  _M0L13new__capacityS661 = _M0L8capacityS2505 << 1;
  _M0L6_2atmpS2499 = 0;
  _M0L6_2atmpS2498
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS661, _M0L6_2atmpS2499);
  _M0L6_2aoldS3707 = _M0L4selfS660->$0;
  if (_M0L9old__headS659) {
    moonbit_incref(_M0L9old__headS659);
  }
  moonbit_decref(_M0L6_2aoldS3707);
  _M0L4selfS660->$0 = _M0L6_2atmpS2498;
  _M0L4selfS660->$2 = _M0L13new__capacityS661;
  _M0L6_2atmpS2500 = _M0L13new__capacityS661 - 1;
  _M0L4selfS660->$3 = _M0L6_2atmpS2500;
  _M0L8capacityS2502 = _M0L4selfS660->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2501 = _M0FPB21calc__grow__threshold(_M0L8capacityS2502);
  _M0L4selfS660->$4 = _M0L6_2atmpS2501;
  _M0L4selfS660->$1 = 0;
  _M0L6_2atmpS2503 = 0;
  _M0L6_2aoldS3706 = _M0L4selfS660->$5;
  if (_M0L6_2aoldS3706) {
    moonbit_decref(_M0L6_2aoldS3706);
  }
  _M0L4selfS660->$5 = _M0L6_2atmpS2503;
  _M0L4selfS660->$6 = -1;
  _M0L1xS662 = _M0L9old__headS659;
  while (1) {
    if (_M0L1xS662 == 0) {
      if (_M0L1xS662) {
        moonbit_decref(_M0L1xS662);
      }
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS664 = _M0L1xS662;
      struct _M0TPB5EntryGsfE* _M0L4_2aeS665 = _M0L7_2aSomeS664;
      struct _M0TPB5EntryGsfE* _M0L15next__in__chainS666 = _M0L4_2aeS665->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2504 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3704 = _M0L4_2aeS665->$1;
      if (_M0L15next__in__chainS666) {
        moonbit_incref(_M0L15next__in__chainS666);
      }
      if (_M0L6_2aoldS3704) {
        moonbit_decref(_M0L6_2aoldS3704);
      }
      _M0L4_2aeS665->$1 = _M0L6_2atmpS2504;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsfE(_M0L4selfS660, _M0L4_2aeS665);
      moonbit_decref(_M0L4_2aeS665);
      _M0L1xS662 = _M0L15next__in__chainS666;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS587,
  struct _M0TPB5EntryGssE* _M0L5outerS583
) {
  int32_t _M0L4hashS582;
  int32_t _M0L14capacity__maskS2425;
  int32_t _M0L6_2atmpS2424;
  int32_t _M0L3pslS584;
  int32_t _M0L3idxS585;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS582 = _M0L5outerS583->$3;
  _M0L14capacity__maskS2425 = _M0L4selfS587->$3;
  _M0L6_2atmpS2424 = _M0L4hashS582 & _M0L14capacity__maskS2425;
  _M0L3pslS584 = 0;
  _M0L3idxS585 = _M0L6_2atmpS2424;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2423 = _M0L4selfS587->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS586;
    if (
      _M0L3idxS585 < 0
      || _M0L3idxS585 >= Moonbit_array_length(_M0L7entriesS2423)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS586
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2423[_M0L3idxS585];
    if (_M0L7_2abindS586 == 0) {
      int32_t _M0L4tailS2416;
      _M0L5outerS583->$2 = _M0L3pslS584;
      _M0L4tailS2416 = _M0L4selfS587->$6;
      _M0L5outerS583->$0 = _M0L4tailS2416;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS587, _M0L3idxS585, _M0L5outerS583);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS588 = _M0L7_2abindS586;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS589 = _M0L7_2aSomeS588;
      int32_t _M0L3pslS2417 = _M0L7_2acurrS589->$2;
      if (_M0L3pslS584 > _M0L3pslS2417) {
        int32_t _M0L4tailS2418;
        moonbit_incref(_M0L7_2acurrS589);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS587, _M0L3idxS585, _M0L7_2acurrS589);
        moonbit_decref(_M0L7_2acurrS589);
        _M0L5outerS583->$2 = _M0L3pslS584;
        _M0L4tailS2418 = _M0L4selfS587->$6;
        _M0L5outerS583->$0 = _M0L4tailS2418;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS587, _M0L3idxS585, _M0L5outerS583);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2419 = _M0L3pslS584 + 1;
        int32_t _M0L6_2atmpS2421 = _M0L3idxS585 + 1;
        int32_t _M0L14capacity__maskS2422 = _M0L4selfS587->$3;
        int32_t _M0L6_2atmpS2420 =
          _M0L6_2atmpS2421 & _M0L14capacity__maskS2422;
        _M0L3pslS584 = _M0L6_2atmpS2419;
        _M0L3idxS585 = _M0L6_2atmpS2420;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS596,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5outerS592
) {
  int32_t _M0L4hashS591;
  int32_t _M0L14capacity__maskS2435;
  int32_t _M0L6_2atmpS2434;
  int32_t _M0L3pslS593;
  int32_t _M0L3idxS594;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS591 = _M0L5outerS592->$3;
  _M0L14capacity__maskS2435 = _M0L4selfS596->$3;
  _M0L6_2atmpS2434 = _M0L4hashS591 & _M0L14capacity__maskS2435;
  _M0L3pslS593 = 0;
  _M0L3idxS594 = _M0L6_2atmpS2434;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2433 =
      _M0L4selfS596->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS595;
    if (
      _M0L3idxS594 < 0
      || _M0L3idxS594 >= Moonbit_array_length(_M0L7entriesS2433)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS595
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2433[
        _M0L3idxS594
      ];
    if (_M0L7_2abindS595 == 0) {
      int32_t _M0L4tailS2426;
      _M0L5outerS592->$2 = _M0L3pslS593;
      _M0L4tailS2426 = _M0L4selfS596->$6;
      _M0L5outerS592->$0 = _M0L4tailS2426;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS596, _M0L3idxS594, _M0L5outerS592);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS597 =
        _M0L7_2abindS595;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2acurrS598 =
        _M0L7_2aSomeS597;
      int32_t _M0L3pslS2427 = _M0L7_2acurrS598->$2;
      if (_M0L3pslS593 > _M0L3pslS2427) {
        int32_t _M0L4tailS2428;
        moonbit_incref(_M0L7_2acurrS598);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS596, _M0L3idxS594, _M0L7_2acurrS598);
        moonbit_decref(_M0L7_2acurrS598);
        _M0L5outerS592->$2 = _M0L3pslS593;
        _M0L4tailS2428 = _M0L4selfS596->$6;
        _M0L5outerS592->$0 = _M0L4tailS2428;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS596, _M0L3idxS594, _M0L5outerS592);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2429 = _M0L3pslS593 + 1;
        int32_t _M0L6_2atmpS2431 = _M0L3idxS594 + 1;
        int32_t _M0L14capacity__maskS2432 = _M0L4selfS596->$3;
        int32_t _M0L6_2atmpS2430 =
          _M0L6_2atmpS2431 & _M0L14capacity__maskS2432;
        _M0L3pslS593 = _M0L6_2atmpS2429;
        _M0L3idxS594 = _M0L6_2atmpS2430;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS605,
  struct _M0TPB5EntryGsiE* _M0L5outerS601
) {
  int32_t _M0L4hashS600;
  int32_t _M0L14capacity__maskS2445;
  int32_t _M0L6_2atmpS2444;
  int32_t _M0L3pslS602;
  int32_t _M0L3idxS603;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS600 = _M0L5outerS601->$3;
  _M0L14capacity__maskS2445 = _M0L4selfS605->$3;
  _M0L6_2atmpS2444 = _M0L4hashS600 & _M0L14capacity__maskS2445;
  _M0L3pslS602 = 0;
  _M0L3idxS603 = _M0L6_2atmpS2444;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2443 = _M0L4selfS605->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS604;
    if (
      _M0L3idxS603 < 0
      || _M0L3idxS603 >= Moonbit_array_length(_M0L7entriesS2443)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS604
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2443[_M0L3idxS603];
    if (_M0L7_2abindS604 == 0) {
      int32_t _M0L4tailS2436;
      _M0L5outerS601->$2 = _M0L3pslS602;
      _M0L4tailS2436 = _M0L4selfS605->$6;
      _M0L5outerS601->$0 = _M0L4tailS2436;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS605, _M0L3idxS603, _M0L5outerS601);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS606 = _M0L7_2abindS604;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS607 = _M0L7_2aSomeS606;
      int32_t _M0L3pslS2437 = _M0L7_2acurrS607->$2;
      if (_M0L3pslS602 > _M0L3pslS2437) {
        int32_t _M0L4tailS2438;
        moonbit_incref(_M0L7_2acurrS607);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS605, _M0L3idxS603, _M0L7_2acurrS607);
        moonbit_decref(_M0L7_2acurrS607);
        _M0L5outerS601->$2 = _M0L3pslS602;
        _M0L4tailS2438 = _M0L4selfS605->$6;
        _M0L5outerS601->$0 = _M0L4tailS2438;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS605, _M0L3idxS603, _M0L5outerS601);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2439 = _M0L3pslS602 + 1;
        int32_t _M0L6_2atmpS2441 = _M0L3idxS603 + 1;
        int32_t _M0L14capacity__maskS2442 = _M0L4selfS605->$3;
        int32_t _M0L6_2atmpS2440 =
          _M0L6_2atmpS2441 & _M0L14capacity__maskS2442;
        _M0L3pslS602 = _M0L6_2atmpS2439;
        _M0L3idxS603 = _M0L6_2atmpS2440;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS614,
  struct _M0TPB5EntryGsbE* _M0L5outerS610
) {
  int32_t _M0L4hashS609;
  int32_t _M0L14capacity__maskS2455;
  int32_t _M0L6_2atmpS2454;
  int32_t _M0L3pslS611;
  int32_t _M0L3idxS612;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS609 = _M0L5outerS610->$3;
  _M0L14capacity__maskS2455 = _M0L4selfS614->$3;
  _M0L6_2atmpS2454 = _M0L4hashS609 & _M0L14capacity__maskS2455;
  _M0L3pslS611 = 0;
  _M0L3idxS612 = _M0L6_2atmpS2454;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2453 = _M0L4selfS614->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS613;
    if (
      _M0L3idxS612 < 0
      || _M0L3idxS612 >= Moonbit_array_length(_M0L7entriesS2453)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS613
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2453[_M0L3idxS612];
    if (_M0L7_2abindS613 == 0) {
      int32_t _M0L4tailS2446;
      _M0L5outerS610->$2 = _M0L3pslS611;
      _M0L4tailS2446 = _M0L4selfS614->$6;
      _M0L5outerS610->$0 = _M0L4tailS2446;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS615 = _M0L7_2abindS613;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS616 = _M0L7_2aSomeS615;
      int32_t _M0L3pslS2447 = _M0L7_2acurrS616->$2;
      if (_M0L3pslS611 > _M0L3pslS2447) {
        int32_t _M0L4tailS2448;
        moonbit_incref(_M0L7_2acurrS616);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS614, _M0L3idxS612, _M0L7_2acurrS616);
        moonbit_decref(_M0L7_2acurrS616);
        _M0L5outerS610->$2 = _M0L3pslS611;
        _M0L4tailS2448 = _M0L4selfS614->$6;
        _M0L5outerS610->$0 = _M0L4tailS2448;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2449 = _M0L3pslS611 + 1;
        int32_t _M0L6_2atmpS2451 = _M0L3idxS612 + 1;
        int32_t _M0L14capacity__maskS2452 = _M0L4selfS614->$3;
        int32_t _M0L6_2atmpS2450 =
          _M0L6_2atmpS2451 & _M0L14capacity__maskS2452;
        _M0L3pslS611 = _M0L6_2atmpS2449;
        _M0L3idxS612 = _M0L6_2atmpS2450;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS623,
  struct _M0TPB5EntryGsfE* _M0L5outerS619
) {
  int32_t _M0L4hashS618;
  int32_t _M0L14capacity__maskS2465;
  int32_t _M0L6_2atmpS2464;
  int32_t _M0L3pslS620;
  int32_t _M0L3idxS621;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS618 = _M0L5outerS619->$3;
  _M0L14capacity__maskS2465 = _M0L4selfS623->$3;
  _M0L6_2atmpS2464 = _M0L4hashS618 & _M0L14capacity__maskS2465;
  _M0L3pslS620 = 0;
  _M0L3idxS621 = _M0L6_2atmpS2464;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2463 = _M0L4selfS623->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS622;
    if (
      _M0L3idxS621 < 0
      || _M0L3idxS621 >= Moonbit_array_length(_M0L7entriesS2463)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS622
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2463[_M0L3idxS621];
    if (_M0L7_2abindS622 == 0) {
      int32_t _M0L4tailS2456;
      _M0L5outerS619->$2 = _M0L3pslS620;
      _M0L4tailS2456 = _M0L4selfS623->$6;
      _M0L5outerS619->$0 = _M0L4tailS2456;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS624 = _M0L7_2abindS622;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS625 = _M0L7_2aSomeS624;
      int32_t _M0L3pslS2457 = _M0L7_2acurrS625->$2;
      if (_M0L3pslS620 > _M0L3pslS2457) {
        int32_t _M0L4tailS2458;
        moonbit_incref(_M0L7_2acurrS625);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS623, _M0L3idxS621, _M0L7_2acurrS625);
        moonbit_decref(_M0L7_2acurrS625);
        _M0L5outerS619->$2 = _M0L3pslS620;
        _M0L4tailS2458 = _M0L4selfS623->$6;
        _M0L5outerS619->$0 = _M0L4tailS2458;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2459 = _M0L3pslS620 + 1;
        int32_t _M0L6_2atmpS2461 = _M0L3idxS621 + 1;
        int32_t _M0L14capacity__maskS2462 = _M0L4selfS623->$3;
        int32_t _M0L6_2atmpS2460 =
          _M0L6_2atmpS2461 & _M0L14capacity__maskS2462;
        _M0L3pslS620 = _M0L6_2atmpS2459;
        _M0L3idxS621 = _M0L6_2atmpS2460;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE* _M0L4selfS536,
  int32_t _M0L3idxS541,
  struct _M0TPB5EntryGssE* _M0L5entryS540
) {
  int32_t _M0L3pslS2351;
  int32_t _M0L6_2atmpS2347;
  int32_t _M0L6_2atmpS2349;
  int32_t _M0L14capacity__maskS2350;
  int32_t _M0L6_2atmpS2348;
  int32_t _M0L3pslS532;
  int32_t _M0L3idxS533;
  struct _M0TPB5EntryGssE* _M0L5entryS534;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2351 = _M0L5entryS540->$2;
  _M0L6_2atmpS2347 = _M0L3pslS2351 + 1;
  _M0L6_2atmpS2349 = _M0L3idxS541 + 1;
  _M0L14capacity__maskS2350 = _M0L4selfS536->$3;
  _M0L6_2atmpS2348 = _M0L6_2atmpS2349 & _M0L14capacity__maskS2350;
  moonbit_incref(_M0L5entryS540);
  _M0L3pslS532 = _M0L6_2atmpS2347;
  _M0L3idxS533 = _M0L6_2atmpS2348;
  _M0L5entryS534 = _M0L5entryS540;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2346 = _M0L4selfS536->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS535;
    if (
      _M0L3idxS533 < 0
      || _M0L3idxS533 >= Moonbit_array_length(_M0L7entriesS2346)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS535
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2346[_M0L3idxS533];
    if (_M0L7_2abindS535 == 0) {
      _M0L5entryS534->$2 = _M0L3pslS532;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS536, _M0L5entryS534, _M0L3idxS533);
      moonbit_decref(_M0L5entryS534);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS538 = _M0L7_2abindS535;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS539 = _M0L7_2aSomeS538;
      int32_t _M0L3pslS2336 = _M0L14_2acurr__entryS539->$2;
      if (_M0L3pslS532 > _M0L3pslS2336) {
        int32_t _M0L3pslS2341;
        int32_t _M0L6_2atmpS2337;
        int32_t _M0L6_2atmpS2339;
        int32_t _M0L14capacity__maskS2340;
        int32_t _M0L6_2atmpS2338;
        _M0L5entryS534->$2 = _M0L3pslS532;
        moonbit_incref(_M0L14_2acurr__entryS539);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS536, _M0L5entryS534, _M0L3idxS533);
        moonbit_decref(_M0L5entryS534);
        _M0L3pslS2341 = _M0L14_2acurr__entryS539->$2;
        _M0L6_2atmpS2337 = _M0L3pslS2341 + 1;
        _M0L6_2atmpS2339 = _M0L3idxS533 + 1;
        _M0L14capacity__maskS2340 = _M0L4selfS536->$3;
        _M0L6_2atmpS2338 = _M0L6_2atmpS2339 & _M0L14capacity__maskS2340;
        _M0L3pslS532 = _M0L6_2atmpS2337;
        _M0L3idxS533 = _M0L6_2atmpS2338;
        _M0L5entryS534 = _M0L14_2acurr__entryS539;
        continue;
      } else {
        int32_t _M0L6_2atmpS2342 = _M0L3pslS532 + 1;
        int32_t _M0L6_2atmpS2344 = _M0L3idxS533 + 1;
        int32_t _M0L14capacity__maskS2345 = _M0L4selfS536->$3;
        int32_t _M0L6_2atmpS2343 =
          _M0L6_2atmpS2344 & _M0L14capacity__maskS2345;
        struct _M0TPB5EntryGssE* _tmp_4100 = _M0L5entryS534;
        _M0L3pslS532 = _M0L6_2atmpS2342;
        _M0L3idxS533 = _M0L6_2atmpS2343;
        _M0L5entryS534 = _tmp_4100;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS546,
  int32_t _M0L3idxS551,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS550
) {
  int32_t _M0L3pslS2367;
  int32_t _M0L6_2atmpS2363;
  int32_t _M0L6_2atmpS2365;
  int32_t _M0L14capacity__maskS2366;
  int32_t _M0L6_2atmpS2364;
  int32_t _M0L3pslS542;
  int32_t _M0L3idxS543;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS544;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2367 = _M0L5entryS550->$2;
  _M0L6_2atmpS2363 = _M0L3pslS2367 + 1;
  _M0L6_2atmpS2365 = _M0L3idxS551 + 1;
  _M0L14capacity__maskS2366 = _M0L4selfS546->$3;
  _M0L6_2atmpS2364 = _M0L6_2atmpS2365 & _M0L14capacity__maskS2366;
  moonbit_incref(_M0L5entryS550);
  _M0L3pslS542 = _M0L6_2atmpS2363;
  _M0L3idxS543 = _M0L6_2atmpS2364;
  _M0L5entryS544 = _M0L5entryS550;
  while (1) {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2362 =
      _M0L4selfS546->$0;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS545;
    if (
      _M0L3idxS543 < 0
      || _M0L3idxS543 >= Moonbit_array_length(_M0L7entriesS2362)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS545
    = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2362[
        _M0L3idxS543
      ];
    if (_M0L7_2abindS545 == 0) {
      _M0L5entryS544->$2 = _M0L3pslS542;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS546, _M0L5entryS544, _M0L3idxS543);
      moonbit_decref(_M0L5entryS544);
      break;
    } else {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS548 =
        _M0L7_2abindS545;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS549 =
        _M0L7_2aSomeS548;
      int32_t _M0L3pslS2352 = _M0L14_2acurr__entryS549->$2;
      if (_M0L3pslS542 > _M0L3pslS2352) {
        int32_t _M0L3pslS2357;
        int32_t _M0L6_2atmpS2353;
        int32_t _M0L6_2atmpS2355;
        int32_t _M0L14capacity__maskS2356;
        int32_t _M0L6_2atmpS2354;
        _M0L5entryS544->$2 = _M0L3pslS542;
        moonbit_incref(_M0L14_2acurr__entryS549);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(_M0L4selfS546, _M0L5entryS544, _M0L3idxS543);
        moonbit_decref(_M0L5entryS544);
        _M0L3pslS2357 = _M0L14_2acurr__entryS549->$2;
        _M0L6_2atmpS2353 = _M0L3pslS2357 + 1;
        _M0L6_2atmpS2355 = _M0L3idxS543 + 1;
        _M0L14capacity__maskS2356 = _M0L4selfS546->$3;
        _M0L6_2atmpS2354 = _M0L6_2atmpS2355 & _M0L14capacity__maskS2356;
        _M0L3pslS542 = _M0L6_2atmpS2353;
        _M0L3idxS543 = _M0L6_2atmpS2354;
        _M0L5entryS544 = _M0L14_2acurr__entryS549;
        continue;
      } else {
        int32_t _M0L6_2atmpS2358 = _M0L3pslS542 + 1;
        int32_t _M0L6_2atmpS2360 = _M0L3idxS543 + 1;
        int32_t _M0L14capacity__maskS2361 = _M0L4selfS546->$3;
        int32_t _M0L6_2atmpS2359 =
          _M0L6_2atmpS2360 & _M0L14capacity__maskS2361;
        struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _tmp_4102 =
          _M0L5entryS544;
        _M0L3pslS542 = _M0L6_2atmpS2358;
        _M0L3idxS543 = _M0L6_2atmpS2359;
        _M0L5entryS544 = _tmp_4102;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS556,
  int32_t _M0L3idxS561,
  struct _M0TPB5EntryGsiE* _M0L5entryS560
) {
  int32_t _M0L3pslS2383;
  int32_t _M0L6_2atmpS2379;
  int32_t _M0L6_2atmpS2381;
  int32_t _M0L14capacity__maskS2382;
  int32_t _M0L6_2atmpS2380;
  int32_t _M0L3pslS552;
  int32_t _M0L3idxS553;
  struct _M0TPB5EntryGsiE* _M0L5entryS554;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2383 = _M0L5entryS560->$2;
  _M0L6_2atmpS2379 = _M0L3pslS2383 + 1;
  _M0L6_2atmpS2381 = _M0L3idxS561 + 1;
  _M0L14capacity__maskS2382 = _M0L4selfS556->$3;
  _M0L6_2atmpS2380 = _M0L6_2atmpS2381 & _M0L14capacity__maskS2382;
  moonbit_incref(_M0L5entryS560);
  _M0L3pslS552 = _M0L6_2atmpS2379;
  _M0L3idxS553 = _M0L6_2atmpS2380;
  _M0L5entryS554 = _M0L5entryS560;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2378 = _M0L4selfS556->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS555;
    if (
      _M0L3idxS553 < 0
      || _M0L3idxS553 >= Moonbit_array_length(_M0L7entriesS2378)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS555
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2378[_M0L3idxS553];
    if (_M0L7_2abindS555 == 0) {
      _M0L5entryS554->$2 = _M0L3pslS552;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS556, _M0L5entryS554, _M0L3idxS553);
      moonbit_decref(_M0L5entryS554);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS558 = _M0L7_2abindS555;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS559 = _M0L7_2aSomeS558;
      int32_t _M0L3pslS2368 = _M0L14_2acurr__entryS559->$2;
      if (_M0L3pslS552 > _M0L3pslS2368) {
        int32_t _M0L3pslS2373;
        int32_t _M0L6_2atmpS2369;
        int32_t _M0L6_2atmpS2371;
        int32_t _M0L14capacity__maskS2372;
        int32_t _M0L6_2atmpS2370;
        _M0L5entryS554->$2 = _M0L3pslS552;
        moonbit_incref(_M0L14_2acurr__entryS559);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS556, _M0L5entryS554, _M0L3idxS553);
        moonbit_decref(_M0L5entryS554);
        _M0L3pslS2373 = _M0L14_2acurr__entryS559->$2;
        _M0L6_2atmpS2369 = _M0L3pslS2373 + 1;
        _M0L6_2atmpS2371 = _M0L3idxS553 + 1;
        _M0L14capacity__maskS2372 = _M0L4selfS556->$3;
        _M0L6_2atmpS2370 = _M0L6_2atmpS2371 & _M0L14capacity__maskS2372;
        _M0L3pslS552 = _M0L6_2atmpS2369;
        _M0L3idxS553 = _M0L6_2atmpS2370;
        _M0L5entryS554 = _M0L14_2acurr__entryS559;
        continue;
      } else {
        int32_t _M0L6_2atmpS2374 = _M0L3pslS552 + 1;
        int32_t _M0L6_2atmpS2376 = _M0L3idxS553 + 1;
        int32_t _M0L14capacity__maskS2377 = _M0L4selfS556->$3;
        int32_t _M0L6_2atmpS2375 =
          _M0L6_2atmpS2376 & _M0L14capacity__maskS2377;
        struct _M0TPB5EntryGsiE* _tmp_4104 = _M0L5entryS554;
        _M0L3pslS552 = _M0L6_2atmpS2374;
        _M0L3idxS553 = _M0L6_2atmpS2375;
        _M0L5entryS554 = _tmp_4104;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS566,
  int32_t _M0L3idxS571,
  struct _M0TPB5EntryGsbE* _M0L5entryS570
) {
  int32_t _M0L3pslS2399;
  int32_t _M0L6_2atmpS2395;
  int32_t _M0L6_2atmpS2397;
  int32_t _M0L14capacity__maskS2398;
  int32_t _M0L6_2atmpS2396;
  int32_t _M0L3pslS562;
  int32_t _M0L3idxS563;
  struct _M0TPB5EntryGsbE* _M0L5entryS564;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2399 = _M0L5entryS570->$2;
  _M0L6_2atmpS2395 = _M0L3pslS2399 + 1;
  _M0L6_2atmpS2397 = _M0L3idxS571 + 1;
  _M0L14capacity__maskS2398 = _M0L4selfS566->$3;
  _M0L6_2atmpS2396 = _M0L6_2atmpS2397 & _M0L14capacity__maskS2398;
  moonbit_incref(_M0L5entryS570);
  _M0L3pslS562 = _M0L6_2atmpS2395;
  _M0L3idxS563 = _M0L6_2atmpS2396;
  _M0L5entryS564 = _M0L5entryS570;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2394 = _M0L4selfS566->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS565;
    if (
      _M0L3idxS563 < 0
      || _M0L3idxS563 >= Moonbit_array_length(_M0L7entriesS2394)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS565
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2394[_M0L3idxS563];
    if (_M0L7_2abindS565 == 0) {
      _M0L5entryS564->$2 = _M0L3pslS562;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS566, _M0L5entryS564, _M0L3idxS563);
      moonbit_decref(_M0L5entryS564);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS568 = _M0L7_2abindS565;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS569 = _M0L7_2aSomeS568;
      int32_t _M0L3pslS2384 = _M0L14_2acurr__entryS569->$2;
      if (_M0L3pslS562 > _M0L3pslS2384) {
        int32_t _M0L3pslS2389;
        int32_t _M0L6_2atmpS2385;
        int32_t _M0L6_2atmpS2387;
        int32_t _M0L14capacity__maskS2388;
        int32_t _M0L6_2atmpS2386;
        _M0L5entryS564->$2 = _M0L3pslS562;
        moonbit_incref(_M0L14_2acurr__entryS569);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS566, _M0L5entryS564, _M0L3idxS563);
        moonbit_decref(_M0L5entryS564);
        _M0L3pslS2389 = _M0L14_2acurr__entryS569->$2;
        _M0L6_2atmpS2385 = _M0L3pslS2389 + 1;
        _M0L6_2atmpS2387 = _M0L3idxS563 + 1;
        _M0L14capacity__maskS2388 = _M0L4selfS566->$3;
        _M0L6_2atmpS2386 = _M0L6_2atmpS2387 & _M0L14capacity__maskS2388;
        _M0L3pslS562 = _M0L6_2atmpS2385;
        _M0L3idxS563 = _M0L6_2atmpS2386;
        _M0L5entryS564 = _M0L14_2acurr__entryS569;
        continue;
      } else {
        int32_t _M0L6_2atmpS2390 = _M0L3pslS562 + 1;
        int32_t _M0L6_2atmpS2392 = _M0L3idxS563 + 1;
        int32_t _M0L14capacity__maskS2393 = _M0L4selfS566->$3;
        int32_t _M0L6_2atmpS2391 =
          _M0L6_2atmpS2392 & _M0L14capacity__maskS2393;
        struct _M0TPB5EntryGsbE* _tmp_4106 = _M0L5entryS564;
        _M0L3pslS562 = _M0L6_2atmpS2390;
        _M0L3idxS563 = _M0L6_2atmpS2391;
        _M0L5entryS564 = _tmp_4106;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS576,
  int32_t _M0L3idxS581,
  struct _M0TPB5EntryGsfE* _M0L5entryS580
) {
  int32_t _M0L3pslS2415;
  int32_t _M0L6_2atmpS2411;
  int32_t _M0L6_2atmpS2413;
  int32_t _M0L14capacity__maskS2414;
  int32_t _M0L6_2atmpS2412;
  int32_t _M0L3pslS572;
  int32_t _M0L3idxS573;
  struct _M0TPB5EntryGsfE* _M0L5entryS574;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2415 = _M0L5entryS580->$2;
  _M0L6_2atmpS2411 = _M0L3pslS2415 + 1;
  _M0L6_2atmpS2413 = _M0L3idxS581 + 1;
  _M0L14capacity__maskS2414 = _M0L4selfS576->$3;
  _M0L6_2atmpS2412 = _M0L6_2atmpS2413 & _M0L14capacity__maskS2414;
  moonbit_incref(_M0L5entryS580);
  _M0L3pslS572 = _M0L6_2atmpS2411;
  _M0L3idxS573 = _M0L6_2atmpS2412;
  _M0L5entryS574 = _M0L5entryS580;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2410 = _M0L4selfS576->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS575;
    if (
      _M0L3idxS573 < 0
      || _M0L3idxS573 >= Moonbit_array_length(_M0L7entriesS2410)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS575
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2410[_M0L3idxS573];
    if (_M0L7_2abindS575 == 0) {
      _M0L5entryS574->$2 = _M0L3pslS572;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS576, _M0L5entryS574, _M0L3idxS573);
      moonbit_decref(_M0L5entryS574);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS578 = _M0L7_2abindS575;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS579 = _M0L7_2aSomeS578;
      int32_t _M0L3pslS2400 = _M0L14_2acurr__entryS579->$2;
      if (_M0L3pslS572 > _M0L3pslS2400) {
        int32_t _M0L3pslS2405;
        int32_t _M0L6_2atmpS2401;
        int32_t _M0L6_2atmpS2403;
        int32_t _M0L14capacity__maskS2404;
        int32_t _M0L6_2atmpS2402;
        _M0L5entryS574->$2 = _M0L3pslS572;
        moonbit_incref(_M0L14_2acurr__entryS579);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS576, _M0L5entryS574, _M0L3idxS573);
        moonbit_decref(_M0L5entryS574);
        _M0L3pslS2405 = _M0L14_2acurr__entryS579->$2;
        _M0L6_2atmpS2401 = _M0L3pslS2405 + 1;
        _M0L6_2atmpS2403 = _M0L3idxS573 + 1;
        _M0L14capacity__maskS2404 = _M0L4selfS576->$3;
        _M0L6_2atmpS2402 = _M0L6_2atmpS2403 & _M0L14capacity__maskS2404;
        _M0L3pslS572 = _M0L6_2atmpS2401;
        _M0L3idxS573 = _M0L6_2atmpS2402;
        _M0L5entryS574 = _M0L14_2acurr__entryS579;
        continue;
      } else {
        int32_t _M0L6_2atmpS2406 = _M0L3pslS572 + 1;
        int32_t _M0L6_2atmpS2408 = _M0L3idxS573 + 1;
        int32_t _M0L14capacity__maskS2409 = _M0L4selfS576->$3;
        int32_t _M0L6_2atmpS2407 =
          _M0L6_2atmpS2408 & _M0L14capacity__maskS2409;
        struct _M0TPB5EntryGsfE* _tmp_4108 = _M0L5entryS574;
        _M0L3pslS572 = _M0L6_2atmpS2406;
        _M0L3idxS573 = _M0L6_2atmpS2407;
        _M0L5entryS574 = _tmp_4108;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS502,
  struct _M0TPB5EntryGssE* _M0L5entryS504,
  int32_t _M0L8new__idxS503
) {
  struct _M0TPB5EntryGssE** _M0L7entriesS2326;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2327;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3730;
  struct _M0TPB5EntryGssE* _M0L7_2abindS505;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2326 = _M0L4selfS502->$0;
  _M0L6_2atmpS2327 = _M0L5entryS504;
  if (
    _M0L8new__idxS503 < 0
    || _M0L8new__idxS503 >= Moonbit_array_length(_M0L7entriesS2326)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3730
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2326[_M0L8new__idxS503];
  if (_M0L6_2atmpS2327) {
    moonbit_incref(_M0L6_2atmpS2327);
  }
  if (_M0L6_2aoldS3730) {
    moonbit_decref(_M0L6_2aoldS3730);
  }
  _M0L7entriesS2326[_M0L8new__idxS503] = _M0L6_2atmpS2327;
  _M0L7_2abindS505 = _M0L5entryS504->$1;
  if (_M0L7_2abindS505 == 0) {
    _M0L4selfS502->$6 = _M0L8new__idxS503;
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS506 = _M0L7_2abindS505;
    struct _M0TPB5EntryGssE* _M0L7_2anextS507 = _M0L7_2aSomeS506;
    _M0L7_2anextS507->$0 = _M0L8new__idxS503;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS508,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS510,
  int32_t _M0L8new__idxS509
) {
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2328;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2329;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3733;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS511;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2328 = _M0L4selfS508->$0;
  _M0L6_2atmpS2329 = _M0L5entryS510;
  if (
    _M0L8new__idxS509 < 0
    || _M0L8new__idxS509 >= Moonbit_array_length(_M0L7entriesS2328)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3733
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2328[
      _M0L8new__idxS509
    ];
  if (_M0L6_2atmpS2329) {
    moonbit_incref(_M0L6_2atmpS2329);
  }
  if (_M0L6_2aoldS3733) {
    moonbit_decref(_M0L6_2aoldS3733);
  }
  _M0L7entriesS2328[_M0L8new__idxS509] = _M0L6_2atmpS2329;
  _M0L7_2abindS511 = _M0L5entryS510->$1;
  if (_M0L7_2abindS511 == 0) {
    _M0L4selfS508->$6 = _M0L8new__idxS509;
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS512 =
      _M0L7_2abindS511;
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2anextS513 =
      _M0L7_2aSomeS512;
    _M0L7_2anextS513->$0 = _M0L8new__idxS509;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS514,
  struct _M0TPB5EntryGsiE* _M0L5entryS516,
  int32_t _M0L8new__idxS515
) {
  struct _M0TPB5EntryGsiE** _M0L7entriesS2330;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2331;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3736;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS517;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2330 = _M0L4selfS514->$0;
  _M0L6_2atmpS2331 = _M0L5entryS516;
  if (
    _M0L8new__idxS515 < 0
    || _M0L8new__idxS515 >= Moonbit_array_length(_M0L7entriesS2330)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3736
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2330[_M0L8new__idxS515];
  if (_M0L6_2atmpS2331) {
    moonbit_incref(_M0L6_2atmpS2331);
  }
  if (_M0L6_2aoldS3736) {
    moonbit_decref(_M0L6_2aoldS3736);
  }
  _M0L7entriesS2330[_M0L8new__idxS515] = _M0L6_2atmpS2331;
  _M0L7_2abindS517 = _M0L5entryS516->$1;
  if (_M0L7_2abindS517 == 0) {
    _M0L4selfS514->$6 = _M0L8new__idxS515;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS518 = _M0L7_2abindS517;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS519 = _M0L7_2aSomeS518;
    _M0L7_2anextS519->$0 = _M0L8new__idxS515;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS520,
  struct _M0TPB5EntryGsbE* _M0L5entryS522,
  int32_t _M0L8new__idxS521
) {
  struct _M0TPB5EntryGsbE** _M0L7entriesS2332;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2333;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3739;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS523;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2332 = _M0L4selfS520->$0;
  _M0L6_2atmpS2333 = _M0L5entryS522;
  if (
    _M0L8new__idxS521 < 0
    || _M0L8new__idxS521 >= Moonbit_array_length(_M0L7entriesS2332)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3739
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2332[_M0L8new__idxS521];
  if (_M0L6_2atmpS2333) {
    moonbit_incref(_M0L6_2atmpS2333);
  }
  if (_M0L6_2aoldS3739) {
    moonbit_decref(_M0L6_2aoldS3739);
  }
  _M0L7entriesS2332[_M0L8new__idxS521] = _M0L6_2atmpS2333;
  _M0L7_2abindS523 = _M0L5entryS522->$1;
  if (_M0L7_2abindS523 == 0) {
    _M0L4selfS520->$6 = _M0L8new__idxS521;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS524 = _M0L7_2abindS523;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS525 = _M0L7_2aSomeS524;
    _M0L7_2anextS525->$0 = _M0L8new__idxS521;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS526,
  struct _M0TPB5EntryGsfE* _M0L5entryS528,
  int32_t _M0L8new__idxS527
) {
  struct _M0TPB5EntryGsfE** _M0L7entriesS2334;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2335;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3742;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS529;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2334 = _M0L4selfS526->$0;
  _M0L6_2atmpS2335 = _M0L5entryS528;
  if (
    _M0L8new__idxS527 < 0
    || _M0L8new__idxS527 >= Moonbit_array_length(_M0L7entriesS2334)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3742
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2334[_M0L8new__idxS527];
  if (_M0L6_2atmpS2335) {
    moonbit_incref(_M0L6_2atmpS2335);
  }
  if (_M0L6_2aoldS3742) {
    moonbit_decref(_M0L6_2aoldS3742);
  }
  _M0L7entriesS2334[_M0L8new__idxS527] = _M0L6_2atmpS2335;
  _M0L7_2abindS529 = _M0L5entryS528->$1;
  if (_M0L7_2abindS529 == 0) {
    _M0L4selfS526->$6 = _M0L8new__idxS527;
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS530 = _M0L7_2abindS529;
    struct _M0TPB5EntryGsfE* _M0L7_2anextS531 = _M0L7_2aSomeS530;
    _M0L7_2anextS531->$0 = _M0L8new__idxS527;
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS483,
  int32_t _M0L3idxS485,
  struct _M0TPB5EntryGssE* _M0L5entryS484
) {
  int32_t _M0L7_2abindS482;
  struct _M0TPB5EntryGssE** _M0L7entriesS2286;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2287;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3744;
  int32_t _M0L4sizeS2289;
  int32_t _M0L6_2atmpS2288;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS482 = _M0L4selfS483->$6;
  switch (_M0L7_2abindS482) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2281 = _M0L5entryS484;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3746 = _M0L4selfS483->$5;
      if (_M0L6_2atmpS2281) {
        moonbit_incref(_M0L6_2atmpS2281);
      }
      if (_M0L6_2aoldS3746) {
        moonbit_decref(_M0L6_2aoldS3746);
      }
      _M0L4selfS483->$5 = _M0L6_2atmpS2281;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS2285 = _M0L4selfS483->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2284;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2282;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2283;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3747;
      if (
        _M0L7_2abindS482 < 0
        || _M0L7_2abindS482 >= Moonbit_array_length(_M0L7entriesS2285)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2284
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS2285[_M0L7_2abindS482];
      if (_M0L6_2atmpS2284) {
        moonbit_incref(_M0L6_2atmpS2284);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2282
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS2284);
      if (_M0L6_2atmpS2284) {
        moonbit_decref(_M0L6_2atmpS2284);
      }
      _M0L6_2atmpS2283 = _M0L5entryS484;
      _M0L6_2aoldS3747 = _M0L6_2atmpS2282->$1;
      if (_M0L6_2atmpS2283) {
        moonbit_incref(_M0L6_2atmpS2283);
      }
      if (_M0L6_2aoldS3747) {
        moonbit_decref(_M0L6_2aoldS3747);
      }
      _M0L6_2atmpS2282->$1 = _M0L6_2atmpS2283;
      moonbit_decref(_M0L6_2atmpS2282);
      break;
    }
  }
  _M0L4selfS483->$6 = _M0L3idxS485;
  _M0L7entriesS2286 = _M0L4selfS483->$0;
  _M0L6_2atmpS2287 = _M0L5entryS484;
  if (
    _M0L3idxS485 < 0
    || _M0L3idxS485 >= Moonbit_array_length(_M0L7entriesS2286)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3744
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2286[_M0L3idxS485];
  if (_M0L6_2atmpS2287) {
    moonbit_incref(_M0L6_2atmpS2287);
  }
  if (_M0L6_2aoldS3744) {
    moonbit_decref(_M0L6_2aoldS3744);
  }
  _M0L7entriesS2286[_M0L3idxS485] = _M0L6_2atmpS2287;
  _M0L4sizeS2289 = _M0L4selfS483->$1;
  _M0L6_2atmpS2288 = _M0L4sizeS2289 + 1;
  _M0L4selfS483->$1 = _M0L6_2atmpS2288;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS487,
  int32_t _M0L3idxS489,
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L5entryS488
) {
  int32_t _M0L7_2abindS486;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2295;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2296;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3750;
  int32_t _M0L4sizeS2298;
  int32_t _M0L6_2atmpS2297;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS486 = _M0L4selfS487->$6;
  switch (_M0L7_2abindS486) {
    case -1: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2290 =
        _M0L5entryS488;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3752 =
        _M0L4selfS487->$5;
      if (_M0L6_2atmpS2290) {
        moonbit_incref(_M0L6_2atmpS2290);
      }
      if (_M0L6_2aoldS3752) {
        moonbit_decref(_M0L6_2aoldS3752);
      }
      _M0L4selfS487->$5 = _M0L6_2atmpS2290;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7entriesS2294 =
        _M0L4selfS487->$0;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2293;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2291;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2292;
      struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2aoldS3753;
      if (
        _M0L7_2abindS486 < 0
        || _M0L7_2abindS486 >= Moonbit_array_length(_M0L7entriesS2294)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2293
      = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2294[
          _M0L7_2abindS486
        ];
      if (_M0L6_2atmpS2293) {
        moonbit_incref(_M0L6_2atmpS2293);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2291
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2293);
      if (_M0L6_2atmpS2293) {
        moonbit_decref(_M0L6_2atmpS2293);
      }
      _M0L6_2atmpS2292 = _M0L5entryS488;
      _M0L6_2aoldS3753 = _M0L6_2atmpS2291->$1;
      if (_M0L6_2atmpS2292) {
        moonbit_incref(_M0L6_2atmpS2292);
      }
      if (_M0L6_2aoldS3753) {
        moonbit_decref(_M0L6_2aoldS3753);
      }
      _M0L6_2atmpS2291->$1 = _M0L6_2atmpS2292;
      moonbit_decref(_M0L6_2atmpS2291);
      break;
    }
  }
  _M0L4selfS487->$6 = _M0L3idxS489;
  _M0L7entriesS2295 = _M0L4selfS487->$0;
  _M0L6_2atmpS2296 = _M0L5entryS488;
  if (
    _M0L3idxS489 < 0
    || _M0L3idxS489 >= Moonbit_array_length(_M0L7entriesS2295)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3750
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)_M0L7entriesS2295[
      _M0L3idxS489
    ];
  if (_M0L6_2atmpS2296) {
    moonbit_incref(_M0L6_2atmpS2296);
  }
  if (_M0L6_2aoldS3750) {
    moonbit_decref(_M0L6_2aoldS3750);
  }
  _M0L7entriesS2295[_M0L3idxS489] = _M0L6_2atmpS2296;
  _M0L4sizeS2298 = _M0L4selfS487->$1;
  _M0L6_2atmpS2297 = _M0L4sizeS2298 + 1;
  _M0L4selfS487->$1 = _M0L6_2atmpS2297;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS491,
  int32_t _M0L3idxS493,
  struct _M0TPB5EntryGsiE* _M0L5entryS492
) {
  int32_t _M0L7_2abindS490;
  struct _M0TPB5EntryGsiE** _M0L7entriesS2304;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2305;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3756;
  int32_t _M0L4sizeS2307;
  int32_t _M0L6_2atmpS2306;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS490 = _M0L4selfS491->$6;
  switch (_M0L7_2abindS490) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2299 = _M0L5entryS492;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3758 = _M0L4selfS491->$5;
      if (_M0L6_2atmpS2299) {
        moonbit_incref(_M0L6_2atmpS2299);
      }
      if (_M0L6_2aoldS3758) {
        moonbit_decref(_M0L6_2aoldS3758);
      }
      _M0L4selfS491->$5 = _M0L6_2atmpS2299;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2303 = _M0L4selfS491->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2302;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2300;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2301;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3759;
      if (
        _M0L7_2abindS490 < 0
        || _M0L7_2abindS490 >= Moonbit_array_length(_M0L7entriesS2303)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2302
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2303[_M0L7_2abindS490];
      if (_M0L6_2atmpS2302) {
        moonbit_incref(_M0L6_2atmpS2302);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2300
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2302);
      if (_M0L6_2atmpS2302) {
        moonbit_decref(_M0L6_2atmpS2302);
      }
      _M0L6_2atmpS2301 = _M0L5entryS492;
      _M0L6_2aoldS3759 = _M0L6_2atmpS2300->$1;
      if (_M0L6_2atmpS2301) {
        moonbit_incref(_M0L6_2atmpS2301);
      }
      if (_M0L6_2aoldS3759) {
        moonbit_decref(_M0L6_2aoldS3759);
      }
      _M0L6_2atmpS2300->$1 = _M0L6_2atmpS2301;
      moonbit_decref(_M0L6_2atmpS2300);
      break;
    }
  }
  _M0L4selfS491->$6 = _M0L3idxS493;
  _M0L7entriesS2304 = _M0L4selfS491->$0;
  _M0L6_2atmpS2305 = _M0L5entryS492;
  if (
    _M0L3idxS493 < 0
    || _M0L3idxS493 >= Moonbit_array_length(_M0L7entriesS2304)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3756
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2304[_M0L3idxS493];
  if (_M0L6_2atmpS2305) {
    moonbit_incref(_M0L6_2atmpS2305);
  }
  if (_M0L6_2aoldS3756) {
    moonbit_decref(_M0L6_2aoldS3756);
  }
  _M0L7entriesS2304[_M0L3idxS493] = _M0L6_2atmpS2305;
  _M0L4sizeS2307 = _M0L4selfS491->$1;
  _M0L6_2atmpS2306 = _M0L4sizeS2307 + 1;
  _M0L4selfS491->$1 = _M0L6_2atmpS2306;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS495,
  int32_t _M0L3idxS497,
  struct _M0TPB5EntryGsbE* _M0L5entryS496
) {
  int32_t _M0L7_2abindS494;
  struct _M0TPB5EntryGsbE** _M0L7entriesS2313;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2314;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3762;
  int32_t _M0L4sizeS2316;
  int32_t _M0L6_2atmpS2315;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS494 = _M0L4selfS495->$6;
  switch (_M0L7_2abindS494) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2308 = _M0L5entryS496;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3764 = _M0L4selfS495->$5;
      if (_M0L6_2atmpS2308) {
        moonbit_incref(_M0L6_2atmpS2308);
      }
      if (_M0L6_2aoldS3764) {
        moonbit_decref(_M0L6_2aoldS3764);
      }
      _M0L4selfS495->$5 = _M0L6_2atmpS2308;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2312 = _M0L4selfS495->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2311;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2309;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2310;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3765;
      if (
        _M0L7_2abindS494 < 0
        || _M0L7_2abindS494 >= Moonbit_array_length(_M0L7entriesS2312)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2311
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2312[_M0L7_2abindS494];
      if (_M0L6_2atmpS2311) {
        moonbit_incref(_M0L6_2atmpS2311);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2309
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2311);
      if (_M0L6_2atmpS2311) {
        moonbit_decref(_M0L6_2atmpS2311);
      }
      _M0L6_2atmpS2310 = _M0L5entryS496;
      _M0L6_2aoldS3765 = _M0L6_2atmpS2309->$1;
      if (_M0L6_2atmpS2310) {
        moonbit_incref(_M0L6_2atmpS2310);
      }
      if (_M0L6_2aoldS3765) {
        moonbit_decref(_M0L6_2aoldS3765);
      }
      _M0L6_2atmpS2309->$1 = _M0L6_2atmpS2310;
      moonbit_decref(_M0L6_2atmpS2309);
      break;
    }
  }
  _M0L4selfS495->$6 = _M0L3idxS497;
  _M0L7entriesS2313 = _M0L4selfS495->$0;
  _M0L6_2atmpS2314 = _M0L5entryS496;
  if (
    _M0L3idxS497 < 0
    || _M0L3idxS497 >= Moonbit_array_length(_M0L7entriesS2313)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3762
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2313[_M0L3idxS497];
  if (_M0L6_2atmpS2314) {
    moonbit_incref(_M0L6_2atmpS2314);
  }
  if (_M0L6_2aoldS3762) {
    moonbit_decref(_M0L6_2aoldS3762);
  }
  _M0L7entriesS2313[_M0L3idxS497] = _M0L6_2atmpS2314;
  _M0L4sizeS2316 = _M0L4selfS495->$1;
  _M0L6_2atmpS2315 = _M0L4sizeS2316 + 1;
  _M0L4selfS495->$1 = _M0L6_2atmpS2315;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS499,
  int32_t _M0L3idxS501,
  struct _M0TPB5EntryGsfE* _M0L5entryS500
) {
  int32_t _M0L7_2abindS498;
  struct _M0TPB5EntryGsfE** _M0L7entriesS2322;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2323;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3768;
  int32_t _M0L4sizeS2325;
  int32_t _M0L6_2atmpS2324;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS498 = _M0L4selfS499->$6;
  switch (_M0L7_2abindS498) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2317 = _M0L5entryS500;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3770 = _M0L4selfS499->$5;
      if (_M0L6_2atmpS2317) {
        moonbit_incref(_M0L6_2atmpS2317);
      }
      if (_M0L6_2aoldS3770) {
        moonbit_decref(_M0L6_2aoldS3770);
      }
      _M0L4selfS499->$5 = _M0L6_2atmpS2317;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2321 = _M0L4selfS499->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2320;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2318;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2319;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3771;
      if (
        _M0L7_2abindS498 < 0
        || _M0L7_2abindS498 >= Moonbit_array_length(_M0L7entriesS2321)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2320
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2321[_M0L7_2abindS498];
      if (_M0L6_2atmpS2320) {
        moonbit_incref(_M0L6_2atmpS2320);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2318
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2320);
      if (_M0L6_2atmpS2320) {
        moonbit_decref(_M0L6_2atmpS2320);
      }
      _M0L6_2atmpS2319 = _M0L5entryS500;
      _M0L6_2aoldS3771 = _M0L6_2atmpS2318->$1;
      if (_M0L6_2atmpS2319) {
        moonbit_incref(_M0L6_2atmpS2319);
      }
      if (_M0L6_2aoldS3771) {
        moonbit_decref(_M0L6_2aoldS3771);
      }
      _M0L6_2atmpS2318->$1 = _M0L6_2atmpS2319;
      moonbit_decref(_M0L6_2atmpS2318);
      break;
    }
  }
  _M0L4selfS499->$6 = _M0L3idxS501;
  _M0L7entriesS2322 = _M0L4selfS499->$0;
  _M0L6_2atmpS2323 = _M0L5entryS500;
  if (
    _M0L3idxS501 < 0
    || _M0L3idxS501 >= Moonbit_array_length(_M0L7entriesS2322)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3768
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2322[_M0L3idxS501];
  if (_M0L6_2atmpS2323) {
    moonbit_incref(_M0L6_2atmpS2323);
  }
  if (_M0L6_2aoldS3768) {
    moonbit_decref(_M0L6_2aoldS3768);
  }
  _M0L7entriesS2322[_M0L3idxS501] = _M0L6_2atmpS2323;
  _M0L4sizeS2325 = _M0L4selfS499->$1;
  _M0L6_2atmpS2324 = _M0L4sizeS2325 + 1;
  _M0L4selfS499->$1 = _M0L6_2atmpS2324;
  return 0;
}

int32_t _M0MPC13int3Int3max(int32_t _M0L4selfS480, int32_t _M0L5otherS481) {
  #line 75 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS480 > _M0L5otherS481) {
    return _M0L4selfS480;
  } else {
    return _M0L5otherS481;
  }
}

int32_t _M0FPB21capacity__for__length(int32_t _M0L6lengthS479) {
  int32_t _M0Lm8capacityS478;
  int32_t _M0L6_2atmpS2279;
  int32_t _M0L6_2atmpS2278;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS478 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS479);
  _M0L6_2atmpS2279 = _M0Lm8capacityS478;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2278 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2279);
  if (_M0L6lengthS479 > _M0L6_2atmpS2278) {
    int32_t _M0L6_2atmpS2280 = _M0Lm8capacityS478;
    _M0Lm8capacityS478 = _M0L6_2atmpS2280 * 2;
  }
  return _M0Lm8capacityS478;
}

struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0FPB8new__mapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE(
  int32_t _M0L8capacityS449
) {
  int32_t _M0L8capacityS448;
  int32_t _M0L7_2abindS450;
  int32_t _M0L7_2abindS451;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6_2atmpS2273;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE** _M0L7_2abindS452;
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2abindS453;
  struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _block_4109;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS448
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS449);
  _M0L7_2abindS450 = _M0L8capacityS448 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS451 = _M0FPB21calc__grow__threshold(_M0L8capacityS448);
  _M0L6_2atmpS2273 = 0;
  _M0L7_2abindS452
  = (struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS448, _M0L6_2atmpS2273);
  _M0L7_2abindS453 = 0;
  _block_4109
  = (struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP38JIA2JIA29moonbitdb3lib10RedisValueE));
  Moonbit_object_header(_block_4109)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 100, 0);
  _block_4109->$0 = _M0L7_2abindS452;
  _block_4109->$1 = 0;
  _block_4109->$2 = _M0L8capacityS448;
  _block_4109->$3 = _M0L7_2abindS450;
  _block_4109->$4 = _M0L7_2abindS451;
  _block_4109->$5 = _M0L7_2abindS453;
  _block_4109->$6 = -1;
  return _block_4109;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS455) {
  int32_t _M0L8capacityS454;
  int32_t _M0L7_2abindS456;
  int32_t _M0L7_2abindS457;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2274;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS458;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS459;
  struct _M0TPB3MapGsiE* _block_4110;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS454
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS455);
  _M0L7_2abindS456 = _M0L8capacityS454 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS457 = _M0FPB21calc__grow__threshold(_M0L8capacityS454);
  _M0L6_2atmpS2274 = 0;
  _M0L7_2abindS458
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS454, _M0L6_2atmpS2274);
  _M0L7_2abindS459 = 0;
  _block_4110
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_4110)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 104, 0);
  _block_4110->$0 = _M0L7_2abindS458;
  _block_4110->$1 = 0;
  _block_4110->$2 = _M0L8capacityS454;
  _block_4110->$3 = _M0L7_2abindS456;
  _block_4110->$4 = _M0L7_2abindS457;
  _block_4110->$5 = _M0L7_2abindS459;
  _block_4110->$6 = -1;
  return _block_4110;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS461) {
  int32_t _M0L8capacityS460;
  int32_t _M0L7_2abindS462;
  int32_t _M0L7_2abindS463;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2275;
  struct _M0TPB5EntryGssE** _M0L7_2abindS464;
  struct _M0TPB5EntryGssE* _M0L7_2abindS465;
  struct _M0TPB3MapGssE* _block_4111;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS460
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS461);
  _M0L7_2abindS462 = _M0L8capacityS460 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS463 = _M0FPB21calc__grow__threshold(_M0L8capacityS460);
  _M0L6_2atmpS2275 = 0;
  _M0L7_2abindS464
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS460, _M0L6_2atmpS2275);
  _M0L7_2abindS465 = 0;
  _block_4111
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_4111)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 108, 0);
  _block_4111->$0 = _M0L7_2abindS464;
  _block_4111->$1 = 0;
  _block_4111->$2 = _M0L8capacityS460;
  _block_4111->$3 = _M0L7_2abindS462;
  _block_4111->$4 = _M0L7_2abindS463;
  _block_4111->$5 = _M0L7_2abindS465;
  _block_4111->$6 = -1;
  return _block_4111;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS467) {
  int32_t _M0L8capacityS466;
  int32_t _M0L7_2abindS468;
  int32_t _M0L7_2abindS469;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2276;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS470;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS471;
  struct _M0TPB3MapGsbE* _block_4112;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS466
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS467);
  _M0L7_2abindS468 = _M0L8capacityS466 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS469 = _M0FPB21calc__grow__threshold(_M0L8capacityS466);
  _M0L6_2atmpS2276 = 0;
  _M0L7_2abindS470
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS466, _M0L6_2atmpS2276);
  _M0L7_2abindS471 = 0;
  _block_4112
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_4112)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 112, 0);
  _block_4112->$0 = _M0L7_2abindS470;
  _block_4112->$1 = 0;
  _block_4112->$2 = _M0L8capacityS466;
  _block_4112->$3 = _M0L7_2abindS468;
  _block_4112->$4 = _M0L7_2abindS469;
  _block_4112->$5 = _M0L7_2abindS471;
  _block_4112->$6 = -1;
  return _block_4112;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS473) {
  int32_t _M0L8capacityS472;
  int32_t _M0L7_2abindS474;
  int32_t _M0L7_2abindS475;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2277;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS476;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS477;
  struct _M0TPB3MapGsfE* _block_4113;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS472
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS473);
  _M0L7_2abindS474 = _M0L8capacityS472 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS475 = _M0FPB21calc__grow__threshold(_M0L8capacityS472);
  _M0L6_2atmpS2277 = 0;
  _M0L7_2abindS476
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS472, _M0L6_2atmpS2277);
  _M0L7_2abindS477 = 0;
  _block_4113
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_4113)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 116, 0);
  _block_4113->$0 = _M0L7_2abindS476;
  _block_4113->$1 = 0;
  _block_4113->$2 = _M0L8capacityS472;
  _block_4113->$3 = _M0L7_2abindS474;
  _block_4113->$4 = _M0L7_2abindS475;
  _block_4113->$5 = _M0L7_2abindS477;
  _block_4113->$6 = -1;
  return _block_4113;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS447) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS447 >= 0) {
    int32_t _M0L6_2atmpS2272;
    int32_t _M0L6_2atmpS2271;
    int32_t _M0L6_2atmpS2270;
    int32_t _M0L6_2atmpS2269;
    if (_M0L4selfS447 <= 1) {
      return 1;
    }
    if (_M0L4selfS447 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2272 = _M0L4selfS447 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2271 = moonbit_clz32(_M0L6_2atmpS2272);
    _M0L6_2atmpS2270 = _M0L6_2atmpS2271 - 1;
    _M0L6_2atmpS2269 = 2147483647 >> (_M0L6_2atmpS2270 & 31);
    return _M0L6_2atmpS2269 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS446) {
  int32_t _M0L6_2atmpS2268;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2268 = _M0L8capacityS446 * 13;
  return _M0L6_2atmpS2268 / 16;
}

int32_t _M0MPC16option6Option6unwrapGiE(int64_t _M0L4selfS434) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS434 == 4294967296ll) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    int64_t _M0L7_2aSomeS435 = _M0L4selfS434;
    return (int32_t)_M0L7_2aSomeS435;
  }
}

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE* _M0L4selfS436
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS436 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS437 = _M0L4selfS436;
    if (_M0L7_2aSomeS437) {
      moonbit_incref(_M0L7_2aSomeS437);
    }
    return _M0L7_2aSomeS437;
  }
}

struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L4selfS438
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS438 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2aSomeS439 =
      _M0L4selfS438;
    if (_M0L7_2aSomeS439) {
      moonbit_incref(_M0L7_2aSomeS439);
    }
    return _M0L7_2aSomeS439;
  }
}

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE* _M0L4selfS440
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS440 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS441 = _M0L4selfS440;
    if (_M0L7_2aSomeS441) {
      moonbit_incref(_M0L7_2aSomeS441);
    }
    return _M0L7_2aSomeS441;
  }
}

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE* _M0L4selfS442
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS442 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS443 = _M0L4selfS442;
    if (_M0L7_2aSomeS443) {
      moonbit_incref(_M0L7_2aSomeS443);
    }
    return _M0L7_2aSomeS443;
  }
}

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE* _M0L4selfS444
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS444 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS445 = _M0L4selfS444;
    if (_M0L7_2aSomeS445) {
      moonbit_incref(_M0L7_2aSomeS445);
    }
    return _M0L7_2aSomeS445;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS430,
  int32_t _M0L5indexS431
) {
  uint64_t* _M0L6_2atmpS2266;
  uint64_t _result_4114;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS430);
  _M0L6_2atmpS2266 = _M0L4selfS430;
  if (
    _M0L5indexS431 < 0
    || _M0L5indexS431 >= Moonbit_array_length(_M0L6_2atmpS2266)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4114 = (uint64_t)_M0L6_2atmpS2266[_M0L5indexS431];
  moonbit_decref(_M0L6_2atmpS2266);
  return _result_4114;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS432,
  int32_t _M0L5indexS433
) {
  uint32_t* _M0L6_2atmpS2267;
  uint32_t _result_4115;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS432);
  _M0L6_2atmpS2267 = _M0L4selfS432;
  if (
    _M0L5indexS433 < 0
    || _M0L5indexS433 >= Moonbit_array_length(_M0L6_2atmpS2267)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4115 = (uint32_t)_M0L6_2atmpS2267[_M0L5indexS433];
  moonbit_decref(_M0L6_2atmpS2267);
  return _result_4115;
}

moonbit_string_t _M0IPC16uint646UInt64PB4Show10to__string(
  uint64_t _M0L4selfS429
) {
  #line 50 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 51 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS429, 10);
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS428) {
  #line 35 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 36 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS428, 10);
}

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t _M0L4selfS427) {
  #line 26 "/home/developer/.moon/lib/core/builtin/show.mbt"
  if (_M0L4selfS427) {
    return (moonbit_string_t)moonbit_string_literal_8.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_9.data;
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS426) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS426;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS417,
  moonbit_string_t _M0L5valueS419
) {
  int32_t _M0L3lenS2251;
  moonbit_string_t* _M0L6_2atmpS2253;
  int32_t _M0L6_2atmpS2252;
  int32_t _M0L6lengthS418;
  moonbit_string_t* _M0L3bufS2254;
  moonbit_string_t _M0L6_2aoldS3774;
  int32_t _M0L6_2atmpS2255;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2251 = _M0L4selfS417->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2253 = _M0MPC15array5Array6bufferGsE(_M0L4selfS417);
  _M0L6_2atmpS2252 = Moonbit_array_length(_M0L6_2atmpS2253);
  moonbit_decref(_M0L6_2atmpS2253);
  if (_M0L3lenS2251 == _M0L6_2atmpS2252) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS417);
  }
  _M0L6lengthS418 = _M0L4selfS417->$1;
  _M0L3bufS2254 = _M0L4selfS417->$0;
  _M0L6_2aoldS3774 = (moonbit_string_t)_M0L3bufS2254[_M0L6lengthS418];
  moonbit_incref(_M0L5valueS419);
  moonbit_decref(_M0L6_2aoldS3774);
  _M0L3bufS2254[_M0L6lengthS418] = _M0L5valueS419;
  _M0L6_2atmpS2255 = _M0L6lengthS418 + 1;
  _M0L4selfS417->$1 = _M0L6_2atmpS2255;
  return 0;
}

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS420,
  moonbit_string_t _M0L5valueS422
) {
  int32_t _M0L3lenS2256;
  moonbit_string_t* _M0L6_2atmpS2258;
  int32_t _M0L6_2atmpS2257;
  int32_t _M0L6lengthS421;
  moonbit_string_t* _M0L3bufS2259;
  moonbit_string_t _M0L6_2aoldS3776;
  int32_t _M0L6_2atmpS2260;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2256 = _M0L4selfS420->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2258 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS420);
  _M0L6_2atmpS2257 = Moonbit_array_length(_M0L6_2atmpS2258);
  moonbit_decref(_M0L6_2atmpS2258);
  if (_M0L3lenS2256 == _M0L6_2atmpS2257) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGOsE(_M0L4selfS420);
  }
  _M0L6lengthS421 = _M0L4selfS420->$1;
  _M0L3bufS2259 = _M0L4selfS420->$0;
  _M0L6_2aoldS3776 = (moonbit_string_t)_M0L3bufS2259[_M0L6lengthS421];
  if (_M0L5valueS422) {
    moonbit_incref(_M0L5valueS422);
  }
  if (_M0L6_2aoldS3776) {
    moonbit_decref(_M0L6_2aoldS3776);
  }
  _M0L3bufS2259[_M0L6lengthS421] = _M0L5valueS422;
  _M0L6_2atmpS2260 = _M0L6lengthS421 + 1;
  _M0L4selfS420->$1 = _M0L6_2atmpS2260;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS423,
  struct _M0TUsfE* _M0L5valueS425
) {
  int32_t _M0L3lenS2261;
  struct _M0TUsfE** _M0L6_2atmpS2263;
  int32_t _M0L6_2atmpS2262;
  int32_t _M0L6lengthS424;
  struct _M0TUsfE** _M0L3bufS2264;
  struct _M0TUsfE* _M0L6_2aoldS3778;
  int32_t _M0L6_2atmpS2265;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2261 = _M0L4selfS423->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2263 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS423);
  _M0L6_2atmpS2262 = Moonbit_array_length(_M0L6_2atmpS2263);
  moonbit_decref(_M0L6_2atmpS2263);
  if (_M0L3lenS2261 == _M0L6_2atmpS2262) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS423);
  }
  _M0L6lengthS424 = _M0L4selfS423->$1;
  _M0L3bufS2264 = _M0L4selfS423->$0;
  _M0L6_2aoldS3778 = (struct _M0TUsfE*)_M0L3bufS2264[_M0L6lengthS424];
  moonbit_incref(_M0L5valueS425);
  if (_M0L6_2aoldS3778) {
    moonbit_decref(_M0L6_2aoldS3778);
  }
  _M0L3bufS2264[_M0L6lengthS424] = _M0L5valueS425;
  _M0L6_2atmpS2265 = _M0L6lengthS424 + 1;
  _M0L4selfS423->$1 = _M0L6_2atmpS2265;
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
  moonbit_string_t* _M0L6_2aoldS3780;
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
  _M0L6_2aoldS3780 = _M0L4selfS391->$0;
  moonbit_decref(_M0L6_2aoldS3780);
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
  moonbit_string_t* _M0L6_2aoldS3782;
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
  _M0L6_2aoldS3782 = _M0L4selfS397->$0;
  moonbit_decref(_M0L6_2aoldS3782);
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
  struct _M0TUsfE** _M0L6_2aoldS3784;
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
  _M0L6_2aoldS3784 = _M0L4selfS403->$0;
  moonbit_decref(_M0L6_2aoldS3784);
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
  int32_t _M0L3endS2249;
  int32_t _M0L5startS2250;
  int32_t _M0L8str__lenS383;
  int32_t _M0L3lenS2242;
  int32_t _M0L6_2atmpS2241;
  uint16_t* _M0L4dataS2243;
  int32_t _M0L3lenS2244;
  moonbit_string_t _M0L6_2atmpS2245;
  int32_t _M0L6_2atmpS2246;
  int32_t _M0L3lenS2248;
  int32_t _M0L6_2atmpS2247;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2249 = _M0L3strS384.$2;
  _M0L5startS2250 = _M0L3strS384.$1;
  _M0L8str__lenS383 = _M0L3endS2249 - _M0L5startS2250;
  _M0L3lenS2242 = _M0L4selfS385->$1;
  _M0L6_2atmpS2241 = _M0L3lenS2242 + _M0L8str__lenS383;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS385, _M0L6_2atmpS2241);
  _M0L4dataS2243 = _M0L4selfS385->$0;
  _M0L3lenS2244 = _M0L4selfS385->$1;
  moonbit_incref(_M0L4dataS2243);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2245 = _M0MPC16string10StringView4data(_M0L3strS384);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2246 = _M0MPC16string10StringView13start__offset(_M0L3strS384);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2243, _M0L3lenS2244, _M0L6_2atmpS2245, _M0L6_2atmpS2246, _M0L8str__lenS383);
  moonbit_decref(_M0L4dataS2243);
  moonbit_decref(_M0L6_2atmpS2245);
  _M0L3lenS2248 = _M0L4selfS385->$1;
  _M0L6_2atmpS2247 = _M0L3lenS2248 + _M0L8str__lenS383;
  _M0L4selfS385->$1 = _M0L6_2atmpS2247;
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS380
) {
  moonbit_string_t* _M0L8_2afieldS3787;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3787 = _M0L4selfS380->$0;
  moonbit_incref(_M0L8_2afieldS3787);
  return _M0L8_2afieldS3787;
}

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS381
) {
  moonbit_string_t* _M0L8_2afieldS3788;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3788 = _M0L4selfS381->$0;
  moonbit_incref(_M0L8_2afieldS3788);
  return _M0L8_2afieldS3788;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS382
) {
  struct _M0TUsfE** _M0L8_2afieldS3789;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3789 = _M0L4selfS382->$0;
  moonbit_incref(_M0L8_2afieldS3789);
  return _M0L8_2afieldS3789;
}

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(
  struct _M0TWEOUssE* _M0L1fS364,
  int64_t _M0L10size__hintS361
) {
  int64_t _M0L10size__hintS360;
  struct _M0TPB4IterGUssEE* _block_4116;
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
  _block_4116
  = (struct _M0TPB4IterGUssEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUssEE));
  Moonbit_object_header(_block_4116)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 120, 0);
  _block_4116->$0 = _M0L1fS364;
  _block_4116->$1 = _M0L10size__hintS360;
  return _block_4116;
}

struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L1fS369,
  int64_t _M0L10size__hintS366
) {
  int64_t _M0L10size__hintS365;
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _block_4117;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS366 == 4294967296ll) {
    _M0L10size__hintS365 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS367 = _M0L10size__hintS366;
    int32_t _M0L4_2anS368 = (int32_t)_M0L7_2aSomeS367;
    if (_M0L4_2anS368 > 0) {
      _M0L10size__hintS365 = (int64_t)_M0L4_2anS368;
    } else {
      _M0L10size__hintS365
      = _M0MPB4Iter3newN6constrS9988GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
    }
  }
  moonbit_incref(_M0L1fS369);
  _block_4117
  = (struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_block_4117)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 123, 0);
  _block_4117->$0 = _M0L1fS369;
  _block_4117->$1 = _M0L10size__hintS365;
  return _block_4117;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS374,
  int64_t _M0L10size__hintS371
) {
  int64_t _M0L10size__hintS370;
  struct _M0TPB4IterGUsbEE* _block_4118;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS371 == 4294967296ll) {
    _M0L10size__hintS370 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS372 = _M0L10size__hintS371;
    int32_t _M0L4_2anS373 = (int32_t)_M0L7_2aSomeS372;
    if (_M0L4_2anS373 > 0) {
      _M0L10size__hintS370 = (int64_t)_M0L4_2anS373;
    } else {
      _M0L10size__hintS370 = _M0MPB4Iter3newN6constrS9988GUsbEE;
    }
  }
  moonbit_incref(_M0L1fS374);
  _block_4118
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_4118)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 126, 0);
  _block_4118->$0 = _M0L1fS374;
  _block_4118->$1 = _M0L10size__hintS370;
  return _block_4118;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS379,
  int64_t _M0L10size__hintS376
) {
  int64_t _M0L10size__hintS375;
  struct _M0TPB4IterGUsfEE* _block_4119;
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
  _block_4119
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_4119)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 0);
  _block_4119->$0 = _M0L1fS379;
  _block_4119->$1 = _M0L10size__hintS375;
  return _block_4119;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS352,
  int32_t _M0L5radixS351
) {
  int32_t _if__result_4120;
  uint16_t* _M0L6bufferS353;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS351 < 2) {
    _if__result_4120 = 1;
  } else {
    _if__result_4120 = _M0L5radixS351 > 36;
  }
  if (_if__result_4120) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_10.data);
  }
  if (_M0L4selfS352 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
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
  int32_t _M0L6_2atmpS2240;
  uint64_t _M0L3numS327;
  int32_t _M0L6offsetS328;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2240 = _M0L10total__lenS350 - _M0L12digit__startS338;
  _M0L3numS327 = _M0L3numS349;
  _M0L6offsetS328 = _M0L6_2atmpS2240;
  while (1) {
    if (_M0L3numS327 >= 10000ull) {
      uint64_t _M0L1tS329 = _M0L3numS327 / 10000ull;
      uint64_t _M0L6_2atmpS2217 = _M0L3numS327 % 10000ull;
      int32_t _M0L1rS330 = (int32_t)_M0L6_2atmpS2217;
      int32_t _M0L2d1S331 = _M0L1rS330 / 100;
      int32_t _M0L2d2S332 = _M0L1rS330 % 100;
      int32_t _M0L6_2atmpS2216 = _M0L2d1S331 / 10;
      int32_t _M0L6_2atmpS2215 = 48 + _M0L6_2atmpS2216;
      int32_t _M0L6d1__hiS333 = (uint16_t)_M0L6_2atmpS2215;
      int32_t _M0L6_2atmpS2214 = _M0L2d1S331 % 10;
      int32_t _M0L6_2atmpS2213 = 48 + _M0L6_2atmpS2214;
      int32_t _M0L6d1__loS334 = (uint16_t)_M0L6_2atmpS2213;
      int32_t _M0L6_2atmpS2212 = _M0L2d2S332 / 10;
      int32_t _M0L6_2atmpS2211 = 48 + _M0L6_2atmpS2212;
      int32_t _M0L6d2__hiS335 = (uint16_t)_M0L6_2atmpS2211;
      int32_t _M0L6_2atmpS2210 = _M0L2d2S332 % 10;
      int32_t _M0L6_2atmpS2209 = 48 + _M0L6_2atmpS2210;
      int32_t _M0L6d2__loS336 = (uint16_t)_M0L6_2atmpS2209;
      int32_t _M0L6_2atmpS2201 = _M0L12digit__startS338 + _M0L6offsetS328;
      int32_t _M0L6_2atmpS2200 = _M0L6_2atmpS2201 - 4;
      int32_t _M0L6_2atmpS2203;
      int32_t _M0L6_2atmpS2202;
      int32_t _M0L6_2atmpS2205;
      int32_t _M0L6_2atmpS2204;
      int32_t _M0L6_2atmpS2207;
      int32_t _M0L6_2atmpS2206;
      int32_t _M0L6_2atmpS2208;
      _M0L6bufferS337[_M0L6_2atmpS2200] = _M0L6d1__hiS333;
      _M0L6_2atmpS2203 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2202 = _M0L6_2atmpS2203 - 3;
      _M0L6bufferS337[_M0L6_2atmpS2202] = _M0L6d1__loS334;
      _M0L6_2atmpS2205 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2204 = _M0L6_2atmpS2205 - 2;
      _M0L6bufferS337[_M0L6_2atmpS2204] = _M0L6d2__hiS335;
      _M0L6_2atmpS2207 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2206 = _M0L6_2atmpS2207 - 1;
      _M0L6bufferS337[_M0L6_2atmpS2206] = _M0L6d2__loS336;
      _M0L6_2atmpS2208 = _M0L6offsetS328 - 4;
      _M0L3numS327 = _M0L1tS329;
      _M0L6offsetS328 = _M0L6_2atmpS2208;
      continue;
    } else {
      int32_t _M0L6_2atmpS2239 = (int32_t)_M0L3numS327;
      int32_t _M0L9remainingS340 = _M0L6_2atmpS2239;
      int32_t _M0L6offsetS341 = _M0L6offsetS328;
      while (1) {
        if (_M0L9remainingS340 >= 100) {
          int32_t _M0L1tS342 = _M0L9remainingS340 / 100;
          int32_t _M0L1dS343 = _M0L9remainingS340 % 100;
          int32_t _M0L6_2atmpS2226 = _M0L1dS343 / 10;
          int32_t _M0L6_2atmpS2225 = 48 + _M0L6_2atmpS2226;
          int32_t _M0L5d__hiS344 = (uint16_t)_M0L6_2atmpS2225;
          int32_t _M0L6_2atmpS2224 = _M0L1dS343 % 10;
          int32_t _M0L6_2atmpS2223 = 48 + _M0L6_2atmpS2224;
          int32_t _M0L5d__loS345 = (uint16_t)_M0L6_2atmpS2223;
          int32_t _M0L6_2atmpS2219 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2218 = _M0L6_2atmpS2219 - 2;
          int32_t _M0L6_2atmpS2221;
          int32_t _M0L6_2atmpS2220;
          int32_t _M0L6_2atmpS2222;
          _M0L6bufferS337[_M0L6_2atmpS2218] = _M0L5d__hiS344;
          _M0L6_2atmpS2221 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2220 = _M0L6_2atmpS2221 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2220] = _M0L5d__loS345;
          _M0L6_2atmpS2222 = _M0L6offsetS341 - 2;
          _M0L9remainingS340 = _M0L1tS342;
          _M0L6offsetS341 = _M0L6_2atmpS2222;
          continue;
        } else if (_M0L9remainingS340 >= 10) {
          int32_t _M0L6_2atmpS2234 = _M0L9remainingS340 / 10;
          int32_t _M0L6_2atmpS2233 = 48 + _M0L6_2atmpS2234;
          int32_t _M0L5d__hiS347 = (uint16_t)_M0L6_2atmpS2233;
          int32_t _M0L6_2atmpS2232 = _M0L9remainingS340 % 10;
          int32_t _M0L6_2atmpS2231 = 48 + _M0L6_2atmpS2232;
          int32_t _M0L5d__loS348 = (uint16_t)_M0L6_2atmpS2231;
          int32_t _M0L6_2atmpS2228 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2227 = _M0L6_2atmpS2228 - 2;
          int32_t _M0L6_2atmpS2230;
          int32_t _M0L6_2atmpS2229;
          _M0L6bufferS337[_M0L6_2atmpS2227] = _M0L5d__hiS347;
          _M0L6_2atmpS2230 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2229 = _M0L6_2atmpS2230 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2229] = _M0L5d__loS348;
        } else {
          int32_t _M0L6_2atmpS2238 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2235 = _M0L6_2atmpS2238 - 1;
          int32_t _M0L6_2atmpS2237 = 48 + _M0L9remainingS340;
          int32_t _M0L6_2atmpS2236 = (uint16_t)_M0L6_2atmpS2237;
          _M0L6bufferS337[_M0L6_2atmpS2235] = _M0L6_2atmpS2236;
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
  int32_t _M0L6_2atmpS2185;
  int32_t _M0L6_2atmpS2184;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS310 = _M0MPC13int3Int10to__uint64(_M0L5radixS311);
  _M0L6_2atmpS2185 = _M0L5radixS311 - 1;
  _M0L6_2atmpS2184 = _M0L5radixS311 & _M0L6_2atmpS2185;
  if (_M0L6_2atmpS2184 == 0) {
    int32_t _M0L5shiftS312;
    uint64_t _M0L4maskS313;
    int32_t _M0L6_2atmpS2192;
    int32_t _M0L6offsetS314;
    uint64_t _M0L1nS315;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS312 = moonbit_ctz32(_M0L5radixS311);
    _M0L4maskS313 = _M0L4baseS310 - 1ull;
    _M0L6_2atmpS2192 = _M0L10total__lenS320 - _M0L12digit__startS318;
    _M0L6offsetS314 = _M0L6_2atmpS2192;
    _M0L1nS315 = _M0L3numS321;
    while (1) {
      if (_M0L1nS315 > 0ull) {
        uint64_t _M0L6_2atmpS2191 = _M0L1nS315 & _M0L4maskS313;
        int32_t _M0L5digitS316 = (int32_t)_M0L6_2atmpS2191;
        int32_t _M0L6_2atmpS2188 = _M0L12digit__startS318 + _M0L6offsetS314;
        int32_t _M0L6_2atmpS2186 = _M0L6_2atmpS2188 - 1;
        int32_t _M0L6_2atmpS2187 =
          ((moonbit_string_t)moonbit_string_literal_11.data)[_M0L5digitS316];
        int32_t _M0L6_2atmpS2189;
        uint64_t _M0L6_2atmpS2190;
        _M0L6bufferS317[_M0L6_2atmpS2186] = _M0L6_2atmpS2187;
        _M0L6_2atmpS2189 = _M0L6offsetS314 - 1;
        _M0L6_2atmpS2190 = _M0L1nS315 >> (_M0L5shiftS312 & 63);
        _M0L6offsetS314 = _M0L6_2atmpS2189;
        _M0L1nS315 = _M0L6_2atmpS2190;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2199 = _M0L10total__lenS320 - _M0L12digit__startS318;
    int32_t _M0L6offsetS322 = _M0L6_2atmpS2199;
    uint64_t _M0L1nS323 = _M0L3numS321;
    while (1) {
      if (_M0L1nS323 > 0ull) {
        uint64_t _M0L1qS324 = _M0L1nS323 / _M0L4baseS310;
        uint64_t _M0L6_2atmpS2198 = _M0L1qS324 * _M0L4baseS310;
        uint64_t _M0L6_2atmpS2197 = _M0L1nS323 - _M0L6_2atmpS2198;
        int32_t _M0L5digitS325 = (int32_t)_M0L6_2atmpS2197;
        int32_t _M0L6_2atmpS2195 = _M0L12digit__startS318 + _M0L6offsetS322;
        int32_t _M0L6_2atmpS2193 = _M0L6_2atmpS2195 - 1;
        int32_t _M0L6_2atmpS2194 =
          ((moonbit_string_t)moonbit_string_literal_11.data)[_M0L5digitS325];
        int32_t _M0L6_2atmpS2196;
        _M0L6bufferS317[_M0L6_2atmpS2193] = _M0L6_2atmpS2194;
        _M0L6_2atmpS2196 = _M0L6offsetS322 - 1;
        _M0L6offsetS322 = _M0L6_2atmpS2196;
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
  int32_t _M0L6_2atmpS2183;
  int32_t _M0L6offsetS299;
  uint64_t _M0L1nS300;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2183 = _M0L10total__lenS308 - _M0L12digit__startS305;
  _M0L6offsetS299 = _M0L6_2atmpS2183;
  _M0L1nS300 = _M0L3numS309;
  while (1) {
    if (_M0L6offsetS299 >= 2) {
      uint64_t _M0L6_2atmpS2180 = _M0L1nS300 & 255ull;
      int32_t _M0L9byte__valS301 = (int32_t)_M0L6_2atmpS2180;
      int32_t _M0L2hiS302 = _M0L9byte__valS301 / 16;
      int32_t _M0L2loS303 = _M0L9byte__valS301 % 16;
      int32_t _M0L6_2atmpS2174 = _M0L12digit__startS305 + _M0L6offsetS299;
      int32_t _M0L6_2atmpS2172 = _M0L6_2atmpS2174 - 2;
      int32_t _M0L6_2atmpS2173 =
        ((moonbit_string_t)moonbit_string_literal_11.data)[_M0L2hiS302];
      int32_t _M0L6_2atmpS2177;
      int32_t _M0L6_2atmpS2175;
      int32_t _M0L6_2atmpS2176;
      int32_t _M0L6_2atmpS2178;
      uint64_t _M0L6_2atmpS2179;
      _M0L6bufferS304[_M0L6_2atmpS2172] = _M0L6_2atmpS2173;
      _M0L6_2atmpS2177 = _M0L12digit__startS305 + _M0L6offsetS299;
      _M0L6_2atmpS2175 = _M0L6_2atmpS2177 - 1;
      _M0L6_2atmpS2176
      = ((moonbit_string_t)moonbit_string_literal_11.data)[
        _M0L2loS303
      ];
      _M0L6bufferS304[_M0L6_2atmpS2175] = _M0L6_2atmpS2176;
      _M0L6_2atmpS2178 = _M0L6offsetS299 - 2;
      _M0L6_2atmpS2179 = _M0L1nS300 >> 8;
      _M0L6offsetS299 = _M0L6_2atmpS2178;
      _M0L1nS300 = _M0L6_2atmpS2179;
      continue;
    } else if (_M0L6offsetS299 == 1) {
      uint64_t _M0L6_2atmpS2182 = _M0L1nS300 & 15ull;
      int32_t _M0L6nibbleS307 = (int32_t)_M0L6_2atmpS2182;
      int32_t _M0L6_2atmpS2181 =
        ((moonbit_string_t)moonbit_string_literal_11.data)[_M0L6nibbleS307];
      _M0L6bufferS304[_M0L12digit__startS305] = _M0L6_2atmpS2181;
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
      uint64_t _M0L6_2atmpS2170 = _M0L3numS296 / _M0L4baseS294;
      int32_t _M0L6_2atmpS2171 = _M0L5countS297 + 1;
      _M0L3numS296 = _M0L6_2atmpS2170;
      _M0L5countS297 = _M0L6_2atmpS2171;
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
    int32_t _M0L6_2atmpS2169;
    int32_t _M0L6_2atmpS2168;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS292 = moonbit_clz64(_M0L5valueS291);
    _M0L6_2atmpS2169 = 63 - _M0L14leading__zerosS292;
    _M0L6_2atmpS2168 = _M0L6_2atmpS2169 / 4;
    return _M0L6_2atmpS2168 + 1;
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
  int32_t _if__result_4127;
  int32_t _M0L12is__negativeS275;
  uint32_t _M0L3numS276;
  uint16_t* _M0L6bufferS277;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS273 < 2) {
    _if__result_4127 = 1;
  } else {
    _if__result_4127 = _M0L5radixS273 > 36;
  }
  if (_if__result_4127) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_10.data);
  }
  if (_M0L4selfS274 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  }
  _M0L12is__negativeS275 = _M0L4selfS274 < 0;
  if (_M0L12is__negativeS275) {
    int32_t _M0L6_2atmpS2167 = -_M0L4selfS274;
    _M0L3numS276 = *(uint32_t*)&_M0L6_2atmpS2167;
  } else {
    _M0L3numS276 = *(uint32_t*)&_M0L4selfS274;
  }
  switch (_M0L5radixS273) {
    case 10: {
      int32_t _M0L10digit__lenS278;
      int32_t _M0L6_2atmpS2164;
      int32_t _M0L10total__lenS279;
      uint16_t* _M0L6bufferS280;
      int32_t _M0L12digit__startS281;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS278 = _M0FPB12dec__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2164 = 1;
      } else {
        _M0L6_2atmpS2164 = 0;
      }
      _M0L10total__lenS279 = _M0L10digit__lenS278 + _M0L6_2atmpS2164;
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
      int32_t _M0L6_2atmpS2165;
      int32_t _M0L10total__lenS283;
      uint16_t* _M0L6bufferS284;
      int32_t _M0L12digit__startS285;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS282 = _M0FPB12hex__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2165 = 1;
      } else {
        _M0L6_2atmpS2165 = 0;
      }
      _M0L10total__lenS283 = _M0L10digit__lenS282 + _M0L6_2atmpS2165;
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
      int32_t _M0L6_2atmpS2166;
      int32_t _M0L10total__lenS287;
      uint16_t* _M0L6bufferS288;
      int32_t _M0L12digit__startS289;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS286
      = _M0FPB14radix__count32(_M0L3numS276, _M0L5radixS273);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2166 = 1;
      } else {
        _M0L6_2atmpS2166 = 0;
      }
      _M0L10total__lenS287 = _M0L10digit__lenS286 + _M0L6_2atmpS2166;
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
      uint32_t _M0L6_2atmpS2162 = _M0L3numS270 / _M0L4baseS268;
      int32_t _M0L6_2atmpS2163 = _M0L5countS271 + 1;
      _M0L3numS270 = _M0L6_2atmpS2162;
      _M0L5countS271 = _M0L6_2atmpS2163;
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
    int32_t _M0L6_2atmpS2161;
    int32_t _M0L6_2atmpS2160;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS266 = moonbit_clz32(_M0L5valueS265);
    _M0L6_2atmpS2161 = 31 - _M0L14leading__zerosS266;
    _M0L6_2atmpS2160 = _M0L6_2atmpS2161 / 4;
    return _M0L6_2atmpS2160 + 1;
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
  int32_t _M0L6_2atmpS2159;
  uint32_t _M0L3numS240;
  int32_t _M0L6offsetS241;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2159 = _M0L10total__lenS263 - _M0L12digit__startS251;
  _M0L3numS240 = _M0L3numS262;
  _M0L6offsetS241 = _M0L6_2atmpS2159;
  while (1) {
    if (_M0L3numS240 >= 10000u) {
      uint32_t _M0L1tS242 = _M0L3numS240 / 10000u;
      uint32_t _M0L6_2atmpS2136 = _M0L3numS240 % 10000u;
      int32_t _M0L1rS243 = *(int32_t*)&_M0L6_2atmpS2136;
      int32_t _M0L2d1S244 = _M0L1rS243 / 100;
      int32_t _M0L2d2S245 = _M0L1rS243 % 100;
      int32_t _M0L6_2atmpS2135 = _M0L2d1S244 / 10;
      int32_t _M0L6_2atmpS2134 = 48 + _M0L6_2atmpS2135;
      int32_t _M0L6d1__hiS246 = (uint16_t)_M0L6_2atmpS2134;
      int32_t _M0L6_2atmpS2133 = _M0L2d1S244 % 10;
      int32_t _M0L6_2atmpS2132 = 48 + _M0L6_2atmpS2133;
      int32_t _M0L6d1__loS247 = (uint16_t)_M0L6_2atmpS2132;
      int32_t _M0L6_2atmpS2131 = _M0L2d2S245 / 10;
      int32_t _M0L6_2atmpS2130 = 48 + _M0L6_2atmpS2131;
      int32_t _M0L6d2__hiS248 = (uint16_t)_M0L6_2atmpS2130;
      int32_t _M0L6_2atmpS2129 = _M0L2d2S245 % 10;
      int32_t _M0L6_2atmpS2128 = 48 + _M0L6_2atmpS2129;
      int32_t _M0L6d2__loS249 = (uint16_t)_M0L6_2atmpS2128;
      int32_t _M0L6_2atmpS2120 = _M0L12digit__startS251 + _M0L6offsetS241;
      int32_t _M0L6_2atmpS2119 = _M0L6_2atmpS2120 - 4;
      int32_t _M0L6_2atmpS2122;
      int32_t _M0L6_2atmpS2121;
      int32_t _M0L6_2atmpS2124;
      int32_t _M0L6_2atmpS2123;
      int32_t _M0L6_2atmpS2126;
      int32_t _M0L6_2atmpS2125;
      int32_t _M0L6_2atmpS2127;
      _M0L6bufferS250[_M0L6_2atmpS2119] = _M0L6d1__hiS246;
      _M0L6_2atmpS2122 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2121 = _M0L6_2atmpS2122 - 3;
      _M0L6bufferS250[_M0L6_2atmpS2121] = _M0L6d1__loS247;
      _M0L6_2atmpS2124 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2123 = _M0L6_2atmpS2124 - 2;
      _M0L6bufferS250[_M0L6_2atmpS2123] = _M0L6d2__hiS248;
      _M0L6_2atmpS2126 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2125 = _M0L6_2atmpS2126 - 1;
      _M0L6bufferS250[_M0L6_2atmpS2125] = _M0L6d2__loS249;
      _M0L6_2atmpS2127 = _M0L6offsetS241 - 4;
      _M0L3numS240 = _M0L1tS242;
      _M0L6offsetS241 = _M0L6_2atmpS2127;
      continue;
    } else {
      int32_t _M0L6_2atmpS2158 = *(int32_t*)&_M0L3numS240;
      int32_t _M0L9remainingS253 = _M0L6_2atmpS2158;
      int32_t _M0L6offsetS254 = _M0L6offsetS241;
      while (1) {
        if (_M0L9remainingS253 >= 100) {
          int32_t _M0L1tS255 = _M0L9remainingS253 / 100;
          int32_t _M0L1dS256 = _M0L9remainingS253 % 100;
          int32_t _M0L6_2atmpS2145 = _M0L1dS256 / 10;
          int32_t _M0L6_2atmpS2144 = 48 + _M0L6_2atmpS2145;
          int32_t _M0L5d__hiS257 = (uint16_t)_M0L6_2atmpS2144;
          int32_t _M0L6_2atmpS2143 = _M0L1dS256 % 10;
          int32_t _M0L6_2atmpS2142 = 48 + _M0L6_2atmpS2143;
          int32_t _M0L5d__loS258 = (uint16_t)_M0L6_2atmpS2142;
          int32_t _M0L6_2atmpS2138 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2137 = _M0L6_2atmpS2138 - 2;
          int32_t _M0L6_2atmpS2140;
          int32_t _M0L6_2atmpS2139;
          int32_t _M0L6_2atmpS2141;
          _M0L6bufferS250[_M0L6_2atmpS2137] = _M0L5d__hiS257;
          _M0L6_2atmpS2140 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2139 = _M0L6_2atmpS2140 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2139] = _M0L5d__loS258;
          _M0L6_2atmpS2141 = _M0L6offsetS254 - 2;
          _M0L9remainingS253 = _M0L1tS255;
          _M0L6offsetS254 = _M0L6_2atmpS2141;
          continue;
        } else if (_M0L9remainingS253 >= 10) {
          int32_t _M0L6_2atmpS2153 = _M0L9remainingS253 / 10;
          int32_t _M0L6_2atmpS2152 = 48 + _M0L6_2atmpS2153;
          int32_t _M0L5d__hiS260 = (uint16_t)_M0L6_2atmpS2152;
          int32_t _M0L6_2atmpS2151 = _M0L9remainingS253 % 10;
          int32_t _M0L6_2atmpS2150 = 48 + _M0L6_2atmpS2151;
          int32_t _M0L5d__loS261 = (uint16_t)_M0L6_2atmpS2150;
          int32_t _M0L6_2atmpS2147 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2146 = _M0L6_2atmpS2147 - 2;
          int32_t _M0L6_2atmpS2149;
          int32_t _M0L6_2atmpS2148;
          _M0L6bufferS250[_M0L6_2atmpS2146] = _M0L5d__hiS260;
          _M0L6_2atmpS2149 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2148 = _M0L6_2atmpS2149 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2148] = _M0L5d__loS261;
        } else {
          int32_t _M0L6_2atmpS2157 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2154 = _M0L6_2atmpS2157 - 1;
          int32_t _M0L6_2atmpS2156 = 48 + _M0L9remainingS253;
          int32_t _M0L6_2atmpS2155 = (uint16_t)_M0L6_2atmpS2156;
          _M0L6bufferS250[_M0L6_2atmpS2154] = _M0L6_2atmpS2155;
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
  int32_t _M0L6_2atmpS2104;
  int32_t _M0L6_2atmpS2103;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS223 = *(uint32_t*)&_M0L5radixS224;
  _M0L6_2atmpS2104 = _M0L5radixS224 - 1;
  _M0L6_2atmpS2103 = _M0L5radixS224 & _M0L6_2atmpS2104;
  if (_M0L6_2atmpS2103 == 0) {
    int32_t _M0L5shiftS225;
    uint32_t _M0L4maskS226;
    int32_t _M0L6_2atmpS2111;
    int32_t _M0L6offsetS227;
    uint32_t _M0L1nS228;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS225 = moonbit_ctz32(_M0L5radixS224);
    _M0L4maskS226 = _M0L4baseS223 - 1u;
    _M0L6_2atmpS2111 = _M0L10total__lenS233 - _M0L12digit__startS231;
    _M0L6offsetS227 = _M0L6_2atmpS2111;
    _M0L1nS228 = _M0L3numS234;
    while (1) {
      if (_M0L1nS228 > 0u) {
        uint32_t _M0L6_2atmpS2110 = _M0L1nS228 & _M0L4maskS226;
        int32_t _M0L5digitS229 = *(int32_t*)&_M0L6_2atmpS2110;
        int32_t _M0L6_2atmpS2107 = _M0L12digit__startS231 + _M0L6offsetS227;
        int32_t _M0L6_2atmpS2105 = _M0L6_2atmpS2107 - 1;
        int32_t _M0L6_2atmpS2106 =
          ((moonbit_string_t)moonbit_string_literal_11.data)[_M0L5digitS229];
        int32_t _M0L6_2atmpS2108;
        uint32_t _M0L6_2atmpS2109;
        _M0L6bufferS230[_M0L6_2atmpS2105] = _M0L6_2atmpS2106;
        _M0L6_2atmpS2108 = _M0L6offsetS227 - 1;
        _M0L6_2atmpS2109 = _M0L1nS228 >> (_M0L5shiftS225 & 31);
        _M0L6offsetS227 = _M0L6_2atmpS2108;
        _M0L1nS228 = _M0L6_2atmpS2109;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2118 = _M0L10total__lenS233 - _M0L12digit__startS231;
    int32_t _M0L6offsetS235 = _M0L6_2atmpS2118;
    uint32_t _M0L1nS236 = _M0L3numS234;
    while (1) {
      if (_M0L1nS236 > 0u) {
        uint32_t _M0L1qS237 = _M0L1nS236 / _M0L4baseS223;
        uint32_t _M0L6_2atmpS2117 = _M0L1qS237 * _M0L4baseS223;
        uint32_t _M0L6_2atmpS2116 = _M0L1nS236 - _M0L6_2atmpS2117;
        int32_t _M0L5digitS238 = *(int32_t*)&_M0L6_2atmpS2116;
        int32_t _M0L6_2atmpS2114 = _M0L12digit__startS231 + _M0L6offsetS235;
        int32_t _M0L6_2atmpS2112 = _M0L6_2atmpS2114 - 1;
        int32_t _M0L6_2atmpS2113 =
          ((moonbit_string_t)moonbit_string_literal_11.data)[_M0L5digitS238];
        int32_t _M0L6_2atmpS2115;
        _M0L6bufferS230[_M0L6_2atmpS2112] = _M0L6_2atmpS2113;
        _M0L6_2atmpS2115 = _M0L6offsetS235 - 1;
        _M0L6offsetS235 = _M0L6_2atmpS2115;
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
  int32_t _M0L6_2atmpS2102;
  int32_t _M0L6offsetS212;
  uint32_t _M0L1nS213;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2102 = _M0L10total__lenS221 - _M0L12digit__startS218;
  _M0L6offsetS212 = _M0L6_2atmpS2102;
  _M0L1nS213 = _M0L3numS222;
  while (1) {
    if (_M0L6offsetS212 >= 2) {
      uint32_t _M0L6_2atmpS2099 = _M0L1nS213 & 255u;
      int32_t _M0L9byte__valS214 = *(int32_t*)&_M0L6_2atmpS2099;
      int32_t _M0L2hiS215 = _M0L9byte__valS214 / 16;
      int32_t _M0L2loS216 = _M0L9byte__valS214 % 16;
      int32_t _M0L6_2atmpS2093 = _M0L12digit__startS218 + _M0L6offsetS212;
      int32_t _M0L6_2atmpS2091 = _M0L6_2atmpS2093 - 2;
      int32_t _M0L6_2atmpS2092 =
        ((moonbit_string_t)moonbit_string_literal_11.data)[_M0L2hiS215];
      int32_t _M0L6_2atmpS2096;
      int32_t _M0L6_2atmpS2094;
      int32_t _M0L6_2atmpS2095;
      int32_t _M0L6_2atmpS2097;
      uint32_t _M0L6_2atmpS2098;
      _M0L6bufferS217[_M0L6_2atmpS2091] = _M0L6_2atmpS2092;
      _M0L6_2atmpS2096 = _M0L12digit__startS218 + _M0L6offsetS212;
      _M0L6_2atmpS2094 = _M0L6_2atmpS2096 - 1;
      _M0L6_2atmpS2095
      = ((moonbit_string_t)moonbit_string_literal_11.data)[
        _M0L2loS216
      ];
      _M0L6bufferS217[_M0L6_2atmpS2094] = _M0L6_2atmpS2095;
      _M0L6_2atmpS2097 = _M0L6offsetS212 - 2;
      _M0L6_2atmpS2098 = _M0L1nS213 >> 8;
      _M0L6offsetS212 = _M0L6_2atmpS2097;
      _M0L1nS213 = _M0L6_2atmpS2098;
      continue;
    } else if (_M0L6offsetS212 == 1) {
      uint32_t _M0L6_2atmpS2101 = _M0L1nS213 & 15u;
      int32_t _M0L6nibbleS220 = *(int32_t*)&_M0L6_2atmpS2101;
      int32_t _M0L6_2atmpS2100 =
        ((moonbit_string_t)moonbit_string_literal_11.data)[_M0L6nibbleS220];
      _M0L6bufferS217[_M0L12digit__startS218] = _M0L6_2atmpS2100;
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
    int64_t _M0L6_2atmpS2083;
    if (_M0L4_2anS193 > 0) {
      int32_t _M0L6_2atmpS2084 = _M0L4_2anS193 - 1;
      _M0L6_2atmpS2083 = (int64_t)_M0L6_2atmpS2084;
    } else {
      _M0L6_2atmpS2083 = _M0MPB4Iter4nextN6constrS9980GUssEE;
    }
    _M0L4selfS189->$1 = _M0L6_2atmpS2083;
  }
  return _M0L6resultS190;
}

struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE* _M0L4selfS195
) {
  struct _M0TWEOUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L7_2afuncS194;
  struct _M0TUsRP38JIA2JIA29moonbitdb3lib10RedisValueE* _M0L6resultS196;
  int64_t _M0L7_2abindS197;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS194 = _M0L4selfS195->$0;
  moonbit_incref(_M0L7_2afuncS194);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS196 = _M0L7_2afuncS194->code(_M0L7_2afuncS194);
  moonbit_decref(_M0L7_2afuncS194);
  _M0L7_2abindS197 = _M0L4selfS195->$1;
  if (_M0L6resultS196 == 0) {
    _M0L4selfS195->$1
    = _M0MPB4Iter4nextN6constrS9981GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
  } else if (_M0L7_2abindS197 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS198 = _M0L7_2abindS197;
    int32_t _M0L4_2anS199 = (int32_t)_M0L7_2aSomeS198;
    int64_t _M0L6_2atmpS2085;
    if (_M0L4_2anS199 > 0) {
      int32_t _M0L6_2atmpS2086 = _M0L4_2anS199 - 1;
      _M0L6_2atmpS2085 = (int64_t)_M0L6_2atmpS2086;
    } else {
      _M0L6_2atmpS2085
      = _M0MPB4Iter4nextN6constrS9980GUsRP38JIA2JIA29moonbitdb3lib10RedisValueEE;
    }
    _M0L4selfS195->$1 = _M0L6_2atmpS2085;
  }
  return _M0L6resultS196;
}

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS201
) {
  struct _M0TWEOUsbE* _M0L7_2afuncS200;
  struct _M0TUsbE* _M0L6resultS202;
  int64_t _M0L7_2abindS203;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS200 = _M0L4selfS201->$0;
  moonbit_incref(_M0L7_2afuncS200);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS202 = _M0L7_2afuncS200->code(_M0L7_2afuncS200);
  moonbit_decref(_M0L7_2afuncS200);
  _M0L7_2abindS203 = _M0L4selfS201->$1;
  if (_M0L6resultS202 == 0) {
    _M0L4selfS201->$1 = _M0MPB4Iter4nextN6constrS9981GUsbEE;
  } else if (_M0L7_2abindS203 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS204 = _M0L7_2abindS203;
    int32_t _M0L4_2anS205 = (int32_t)_M0L7_2aSomeS204;
    int64_t _M0L6_2atmpS2087;
    if (_M0L4_2anS205 > 0) {
      int32_t _M0L6_2atmpS2088 = _M0L4_2anS205 - 1;
      _M0L6_2atmpS2087 = (int64_t)_M0L6_2atmpS2088;
    } else {
      _M0L6_2atmpS2087 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS201->$1 = _M0L6_2atmpS2087;
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
    int64_t _M0L6_2atmpS2089;
    if (_M0L4_2anS211 > 0) {
      int32_t _M0L6_2atmpS2090 = _M0L4_2anS211 - 1;
      _M0L6_2atmpS2089 = (int64_t)_M0L6_2atmpS2090;
    } else {
      _M0L6_2atmpS2089 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS207->$1 = _M0L6_2atmpS2089;
  }
  return _M0L6resultS208;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS179,
  struct _M0TPB6Logger _M0L6loggerS178
) {
  moonbit_string_t _M0L6_2atmpS2078;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2078 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS179);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS178.$0->$method_0(_M0L6loggerS178.$1, _M0L6_2atmpS2078);
  moonbit_decref(_M0L6_2atmpS2078);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS181,
  struct _M0TPB6Logger _M0L6loggerS180
) {
  moonbit_string_t _M0L6_2atmpS2079;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2079 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS181);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS180.$0->$method_0(_M0L6loggerS180.$1, _M0L6_2atmpS2079);
  moonbit_decref(_M0L6_2atmpS2079);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS183,
  struct _M0TPB6Logger _M0L6loggerS182
) {
  moonbit_string_t _M0L6_2atmpS2080;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2080 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS183);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS182.$0->$method_0(_M0L6loggerS182.$1, _M0L6_2atmpS2080);
  moonbit_decref(_M0L6_2atmpS2080);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGbE(
  int32_t _M0L4selfS185,
  struct _M0TPB6Logger _M0L6loggerS184
) {
  moonbit_string_t _M0L6_2atmpS2081;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2081 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS185);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS184.$0->$method_0(_M0L6loggerS184.$1, _M0L6_2atmpS2081);
  moonbit_decref(_M0L6_2atmpS2081);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS187,
  struct _M0TPB6Logger _M0L6loggerS186
) {
  moonbit_string_t _M0L6_2atmpS2082;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2082 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS187);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS186.$0->$method_0(_M0L6loggerS186.$1, _M0L6_2atmpS2082);
  moonbit_decref(_M0L6_2atmpS2082);
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
  moonbit_string_t _M0L8_2afieldS3794;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS3794 = _M0L4selfS176.$0;
  moonbit_incref(_M0L8_2afieldS3794);
  return _M0L8_2afieldS3794;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS172,
  moonbit_string_t _M0L5valueS173,
  int32_t _M0L5startS174,
  int32_t _M0L3lenS175
) {
  int32_t _M0L6_2atmpS2077;
  int64_t _M0L6_2atmpS2076;
  struct _M0TPC16string10StringView _M0L6_2atmpS2075;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2077 = _M0L5startS174 + _M0L3lenS175;
  _M0L6_2atmpS2076 = (int64_t)_M0L6_2atmpS2077;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2075
  = _M0MPC16string6String11sub_2einner(_M0L5valueS173, _M0L5startS174, _M0L6_2atmpS2076);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS172, _M0L6_2atmpS2075);
  moonbit_decref(_M0L6_2atmpS2075.$0);
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
  int32_t _if__result_4134;
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
      _if__result_4134 = _M0L3endS166 <= _M0L3lenS164;
    } else {
      _if__result_4134 = 0;
    }
  } else {
    _if__result_4134 = 0;
  }
  if (_if__result_4134) {
    if (_M0L5startS170 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2072 = _M0L4selfS165[_M0L5startS170];
      int32_t _M0L6_2atmpS2071;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2071
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2072);
      if (!_M0L6_2atmpS2071) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS166 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2074 = _M0L4selfS165[_M0L3endS166];
      int32_t _M0L6_2atmpS2073;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2073
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2074);
      if (!_M0L6_2atmpS2073) {
        
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
  struct _M0TPB6Logger _M0L6_2atmpS2070;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS163);
  _M0L6_2atmpS2070
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS163
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS162.$0->$method_0(_M0L4showS162.$1, _M0L6_2atmpS2070);
  if (_M0L6_2atmpS2070.$1) {
    moonbit_decref(_M0L6_2atmpS2070.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS161,
  struct _M0TPB4Show _M0L4showS160
) {
  struct _M0TPB6Logger _M0L6_2atmpS2069;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS161);
  _M0L6_2atmpS2069
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS161
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS160.$0->$method_0(_M0L4showS160.$1, _M0L6_2atmpS2069);
  if (_M0L6_2atmpS2069.$1) {
    moonbit_decref(_M0L6_2atmpS2069.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS159) {
  uint32_t _M0L6_2atmpS2068;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2068 = _M0FPB14avalanche__acc(_M0L3accS159);
  return *(int32_t*)&_M0L6_2atmpS2068;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS158) {
  uint32_t _M0Lm3accS157;
  uint32_t _M0L6_2atmpS2057;
  uint32_t _M0L6_2atmpS2059;
  uint32_t _M0L6_2atmpS2058;
  uint32_t _M0L6_2atmpS2060;
  uint32_t _M0L6_2atmpS2061;
  uint32_t _M0L6_2atmpS2063;
  uint32_t _M0L6_2atmpS2062;
  uint32_t _M0L6_2atmpS2064;
  uint32_t _M0L6_2atmpS2065;
  uint32_t _M0L6_2atmpS2067;
  uint32_t _M0L6_2atmpS2066;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS157 = _M0L3accS158;
  _M0L6_2atmpS2057 = _M0Lm3accS157;
  _M0L6_2atmpS2059 = _M0Lm3accS157;
  _M0L6_2atmpS2058 = _M0L6_2atmpS2059 >> 15;
  _M0Lm3accS157 = _M0L6_2atmpS2057 ^ _M0L6_2atmpS2058;
  _M0L6_2atmpS2060 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2060 * 2246822519u;
  _M0L6_2atmpS2061 = _M0Lm3accS157;
  _M0L6_2atmpS2063 = _M0Lm3accS157;
  _M0L6_2atmpS2062 = _M0L6_2atmpS2063 >> 13;
  _M0Lm3accS157 = _M0L6_2atmpS2061 ^ _M0L6_2atmpS2062;
  _M0L6_2atmpS2064 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2064 * 3266489917u;
  _M0L6_2atmpS2065 = _M0Lm3accS157;
  _M0L6_2atmpS2067 = _M0Lm3accS157;
  _M0L6_2atmpS2066 = _M0L6_2atmpS2067 >> 16;
  _M0Lm3accS157 = _M0L6_2atmpS2065 ^ _M0L6_2atmpS2066;
  return _M0Lm3accS157;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS156) {
  int64_t _M0L6_2atmpS2056;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2056 = (int64_t)_M0L4selfS156;
  return *(uint64_t*)&_M0L6_2atmpS2056;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS155,
  moonbit_string_t _M0L3strS154
) {
  int32_t _M0L8str__lenS153;
  int32_t _M0L3lenS2051;
  int32_t _M0L6_2atmpS2050;
  uint16_t* _M0L4dataS2052;
  int32_t _M0L3lenS2053;
  int32_t _M0L3lenS2055;
  int32_t _M0L6_2atmpS2054;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS153 = Moonbit_array_length(_M0L3strS154);
  _M0L3lenS2051 = _M0L4selfS155->$1;
  _M0L6_2atmpS2050 = _M0L3lenS2051 + _M0L8str__lenS153;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS155, _M0L6_2atmpS2050);
  _M0L4dataS2052 = _M0L4selfS155->$0;
  _M0L3lenS2053 = _M0L4selfS155->$1;
  moonbit_incref(_M0L4dataS2052);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2052, _M0L3lenS2053, _M0L3strS154, 0, _M0L8str__lenS153);
  moonbit_decref(_M0L4dataS2052);
  _M0L3lenS2055 = _M0L4selfS155->$1;
  _M0L6_2atmpS2054 = _M0L3lenS2055 + _M0L8str__lenS153;
  _M0L4selfS155->$1 = _M0L6_2atmpS2054;
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
      int32_t _M0L6_2atmpS2047 = _M0L3strS150[_M0L1iS147];
      int32_t _M0L6_2atmpS2048;
      int32_t _M0L6_2atmpS2049;
      if (
        _M0L1jS148 < 0 || _M0L1jS148 >= Moonbit_array_length(_M0L4selfS149)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS149[_M0L1jS148] = _M0L6_2atmpS2047;
      _M0L6_2atmpS2048 = _M0L1iS147 + 1;
      _M0L6_2atmpS2049 = _M0L1jS148 + 1;
      _M0L1iS147 = _M0L6_2atmpS2048;
      _M0L1jS148 = _M0L6_2atmpS2049;
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
    int32_t _M0L3lenS2026 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2025 = _M0L3lenS2026 + 1;
    uint16_t* _M0L4dataS2027;
    int32_t _M0L3lenS2028;
    int32_t _M0L6_2atmpS2029;
    int32_t _M0L3lenS2031;
    int32_t _M0L6_2atmpS2030;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2025);
    _M0L4dataS2027 = _M0L4selfS141->$0;
    _M0L3lenS2028 = _M0L4selfS141->$1;
    moonbit_incref(_M0L4dataS2027);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2029 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS139);
    if (
      _M0L3lenS2028 < 0
      || _M0L3lenS2028 >= Moonbit_array_length(_M0L4dataS2027)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2027[_M0L3lenS2028] = _M0L6_2atmpS2029;
    moonbit_decref(_M0L4dataS2027);
    _M0L3lenS2031 = _M0L4selfS141->$1;
    _M0L6_2atmpS2030 = _M0L3lenS2031 + 1;
    _M0L4selfS141->$1 = _M0L6_2atmpS2030;
  } else if (_M0L4codeS139 <= 1114111u) {
    int32_t _M0L3lenS2033 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2032 = _M0L3lenS2033 + 2;
    uint32_t _M0L4codeS142;
    uint16_t* _M0L4dataS2034;
    int32_t _M0L3lenS2035;
    uint32_t _M0L6_2atmpS2038;
    uint32_t _M0L6_2atmpS2037;
    int32_t _M0L6_2atmpS2036;
    uint16_t* _M0L4dataS2039;
    int32_t _M0L3lenS2044;
    int32_t _M0L6_2atmpS2040;
    uint32_t _M0L6_2atmpS2043;
    uint32_t _M0L6_2atmpS2042;
    int32_t _M0L6_2atmpS2041;
    int32_t _M0L3lenS2046;
    int32_t _M0L6_2atmpS2045;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2032);
    _M0L4codeS142 = _M0L4codeS139 - 65536u;
    _M0L4dataS2034 = _M0L4selfS141->$0;
    _M0L3lenS2035 = _M0L4selfS141->$1;
    _M0L6_2atmpS2038 = _M0L4codeS142 >> 10;
    _M0L6_2atmpS2037 = 55296u + _M0L6_2atmpS2038;
    moonbit_incref(_M0L4dataS2034);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2036 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2037);
    if (
      _M0L3lenS2035 < 0
      || _M0L3lenS2035 >= Moonbit_array_length(_M0L4dataS2034)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2034[_M0L3lenS2035] = _M0L6_2atmpS2036;
    moonbit_decref(_M0L4dataS2034);
    _M0L4dataS2039 = _M0L4selfS141->$0;
    _M0L3lenS2044 = _M0L4selfS141->$1;
    _M0L6_2atmpS2040 = _M0L3lenS2044 + 1;
    _M0L6_2atmpS2043 = _M0L4codeS142 & 1023u;
    _M0L6_2atmpS2042 = 56320u + _M0L6_2atmpS2043;
    moonbit_incref(_M0L4dataS2039);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2041 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2042);
    if (
      _M0L6_2atmpS2040 < 0
      || _M0L6_2atmpS2040 >= Moonbit_array_length(_M0L4dataS2039)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2039[_M0L6_2atmpS2040] = _M0L6_2atmpS2041;
    moonbit_decref(_M0L4dataS2039);
    _M0L3lenS2046 = _M0L4selfS141->$1;
    _M0L6_2atmpS2045 = _M0L3lenS2046 + 2;
    _M0L4selfS141->$1 = _M0L6_2atmpS2045;
  } else {
    #line 108 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_12.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS133,
  int32_t _M0L8requiredS134
) {
  uint16_t* _M0L4dataS2024;
  int32_t _M0L12current__lenS132;
  int32_t _M0L13enough__spaceS135;
  int32_t _M0L13enough__spaceS136;
  uint16_t* _M0L4dataS2020;
  int32_t _M0L6_2atmpS2021;
  int32_t _M0L3lenS2022;
  uint16_t* _M0L9new__dataS138;
  uint16_t* _M0L6_2aoldS3799;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS2024 = _M0L4selfS133->$0;
  _M0L12current__lenS132 = Moonbit_array_length(_M0L4dataS2024);
  if (_M0L8requiredS134 <= _M0L12current__lenS132) {
    return 0;
  }
  _M0L13enough__spaceS136 = _M0L12current__lenS132;
  while (1) {
    if (_M0L13enough__spaceS136 < _M0L8requiredS134) {
      int32_t _M0L6_2atmpS2023 = _M0L13enough__spaceS136 * 2;
      _M0L13enough__spaceS136 = _M0L6_2atmpS2023;
      continue;
    } else {
      _M0L13enough__spaceS135 = _M0L13enough__spaceS136;
    }
    break;
  }
  _M0L4dataS2020 = _M0L4selfS133->$0;
  moonbit_incref(_M0L4dataS2020);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2021 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS2022 = _M0L4selfS133->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS138
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2020, _M0L13enough__spaceS135, _M0L6_2atmpS2021, _M0L3lenS2022, 0, 0);
  moonbit_decref(_M0L4dataS2020);
  _M0L6_2aoldS3799 = _M0L4selfS133->$0;
  moonbit_decref(_M0L6_2aoldS3799);
  _M0L4selfS133->$0 = _M0L9new__dataS138;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS131) {
  int32_t _M0L6_2atmpS2019;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2019 = *(int32_t*)&_M0L4selfS131;
  return (uint16_t)_M0L6_2atmpS2019;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS130) {
  int32_t _M0L6_2atmpS2018;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2018 = _M0L4selfS130;
  return *(uint32_t*)&_M0L6_2atmpS2018;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS128
) {
  int32_t _M0L3lenS2010;
  uint16_t* _M0L4dataS2012;
  int32_t _M0L6_2atmpS2011;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS2010 = _M0L4selfS128->$1;
  _M0L4dataS2012 = _M0L4selfS128->$0;
  _M0L6_2atmpS2011 = Moonbit_array_length(_M0L4dataS2012);
  if (_M0L3lenS2010 == _M0L6_2atmpS2011) {
    uint16_t* _M0L4dataS2013 = _M0L4selfS128->$0;
    moonbit_incref(_M0L4dataS2013);
    return _M0L4dataS2013;
  } else {
    uint16_t* _M0L4dataS2014 = _M0L4selfS128->$0;
    int32_t _M0L3lenS2015 = _M0L4selfS128->$1;
    int32_t _M0L6_2atmpS2016;
    int32_t _M0L3lenS2017;
    uint16_t* _M0L4dataS129;
    moonbit_incref(_M0L4dataS2014);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2016 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS2017 = _M0L4selfS128->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS129
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2014, _M0L3lenS2015, _M0L6_2atmpS2016, _M0L3lenS2017, 0, 0);
    moonbit_decref(_M0L4dataS2014);
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
  int32_t _if__result_4137;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS121 >= 0) {
    if (_M0L3lenS122 >= 0) {
      if (_M0L11src__offsetS123 >= 0) {
        if (_M0L11dst__offsetS124 >= 0) {
          int32_t _M0L6_2atmpS2006 = _M0L11src__offsetS123 + _M0L3lenS122;
          int32_t _M0L6_2atmpS2007 = Moonbit_array_length(_M0L3srcS125);
          if (_M0L6_2atmpS2006 <= _M0L6_2atmpS2007) {
            int32_t _M0L6_2atmpS2005 = _M0L11dst__offsetS124 + _M0L3lenS122;
            _if__result_4137 = _M0L6_2atmpS2005 <= _M0L13allocate__lenS121;
          } else {
            _if__result_4137 = 0;
          }
        } else {
          _if__result_4137 = 0;
        }
      } else {
        _if__result_4137 = 0;
      }
    } else {
      _if__result_4137 = 0;
    }
  } else {
    _if__result_4137 = 0;
  }
  if (_if__result_4137) {
    moonbit_incref(_M0L3srcS125);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS125, _M0L13allocate__lenS121, _M0L4initS126, _M0L11src__offsetS123, _M0L11dst__offsetS124, _M0L3lenS122);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS127;
    int32_t _M0L6_2atmpS2009;
    moonbit_string_t _M0L6_2atmpS2008;
    uint16_t* _result_4138;
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L18_2astring__builderS127
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_13.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L13allocate__lenS121);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_14.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11src__offsetS123);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_15.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11dst__offsetS124);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_16.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L3lenS122);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_17.data);
    _M0L6_2atmpS2009 = Moonbit_array_length(_M0L3srcS125);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L6_2atmpS2009);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS2008
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS127);
    moonbit_decref(_M0L18_2astring__builderS127);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_4138 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS2008);
    moonbit_decref(_M0L6_2atmpS2008);
    return _result_4138;
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
  struct _M0TPB13StringBuilder* _block_4139;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS112 < 1) {
    _M0L7initialS111 = 1;
  } else {
    int32_t _M0L6_2atmpS2004 = _M0L10size__hintS112 + 1;
    _M0L7initialS111 = _M0L6_2atmpS2004 / 2;
  }
  _M0L4dataS113 = (uint16_t*)moonbit_make_string(_M0L7initialS111, 0);
  _block_4139
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4139)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 132, 0);
  _block_4139->$0 = _M0L4dataS113;
  _block_4139->$1 = 0;
  return _block_4139;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS97,
  int32_t _M0L13allocate__lenS93,
  int32_t _M0L3lenS94,
  int32_t _M0L11src__offsetS95,
  int32_t _M0L11dst__offsetS96
) {
  int32_t _if__result_4140;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS93 >= 0) {
    if (_M0L3lenS94 >= 0) {
      if (_M0L11src__offsetS95 >= 0) {
        if (_M0L11dst__offsetS96 >= 0) {
          int32_t _M0L6_2atmpS1990 = _M0L11src__offsetS95 + _M0L3lenS94;
          int32_t _M0L6_2atmpS1991;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS1991
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
          if (_M0L6_2atmpS1990 <= _M0L6_2atmpS1991) {
            int32_t _M0L6_2atmpS1989 = _M0L11dst__offsetS96 + _M0L3lenS94;
            _if__result_4140 = _M0L6_2atmpS1989 <= _M0L13allocate__lenS93;
          } else {
            _if__result_4140 = 0;
          }
        } else {
          _if__result_4140 = 0;
        }
      } else {
        _if__result_4140 = 0;
      }
    } else {
      _if__result_4140 = 0;
    }
  } else {
    _if__result_4140 = 0;
  }
  if (_if__result_4140) {
    moonbit_incref(_M0L3srcS97);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS93, (moonbit_string_t)moonbit_string_literal_5.data, _M0L3srcS97, _M0L11src__offsetS95, _M0L11dst__offsetS96, _M0L3lenS94);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS98;
    int32_t _M0L6_2atmpS1993;
    moonbit_string_t _M0L6_2atmpS1992;
    moonbit_string_t* _result_4141;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS98
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_13.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L13allocate__lenS93);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_14.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11src__offsetS95);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_15.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11dst__offsetS96);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_16.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L3lenS94);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_17.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1993 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L6_2atmpS1993);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1992
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS98);
    moonbit_decref(_M0L18_2astring__builderS98);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4141
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS1992);
    moonbit_decref(_M0L6_2atmpS1992);
    return _result_4141;
  }
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t* _M0L3srcS103,
  int32_t _M0L13allocate__lenS99,
  int32_t _M0L3lenS100,
  int32_t _M0L11src__offsetS101,
  int32_t _M0L11dst__offsetS102
) {
  int32_t _if__result_4142;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS99 >= 0) {
    if (_M0L3lenS100 >= 0) {
      if (_M0L11src__offsetS101 >= 0) {
        if (_M0L11dst__offsetS102 >= 0) {
          int32_t _M0L6_2atmpS1995 = _M0L11src__offsetS101 + _M0L3lenS100;
          int32_t _M0L6_2atmpS1996;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS1996
          = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
          if (_M0L6_2atmpS1995 <= _M0L6_2atmpS1996) {
            int32_t _M0L6_2atmpS1994 = _M0L11dst__offsetS102 + _M0L3lenS100;
            _if__result_4142 = _M0L6_2atmpS1994 <= _M0L13allocate__lenS99;
          } else {
            _if__result_4142 = 0;
          }
        } else {
          _if__result_4142 = 0;
        }
      } else {
        _if__result_4142 = 0;
      }
    } else {
      _if__result_4142 = 0;
    }
  } else {
    _if__result_4142 = 0;
  }
  if (_if__result_4142) {
    moonbit_incref(_M0L3srcS103);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS99, 0, _M0L3srcS103, _M0L11src__offsetS101, _M0L11dst__offsetS102, _M0L3lenS100);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS104;
    int32_t _M0L6_2atmpS1998;
    moonbit_string_t _M0L6_2atmpS1997;
    moonbit_string_t* _result_4143;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS104
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_13.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L13allocate__lenS99);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_14.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11src__offsetS101);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_15.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11dst__offsetS102);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_16.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L3lenS100);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_17.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1998 = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L6_2atmpS1998);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1997
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS104);
    moonbit_decref(_M0L18_2astring__builderS104);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4143
    = _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(_M0L6_2atmpS1997);
    moonbit_decref(_M0L6_2atmpS1997);
    return _result_4143;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS109,
  int32_t _M0L13allocate__lenS105,
  int32_t _M0L3lenS106,
  int32_t _M0L11src__offsetS107,
  int32_t _M0L11dst__offsetS108
) {
  int32_t _if__result_4144;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS105 >= 0) {
    if (_M0L3lenS106 >= 0) {
      if (_M0L11src__offsetS107 >= 0) {
        if (_M0L11dst__offsetS108 >= 0) {
          int32_t _M0L6_2atmpS2000 = _M0L11src__offsetS107 + _M0L3lenS106;
          int32_t _M0L6_2atmpS2001;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2001
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
          if (_M0L6_2atmpS2000 <= _M0L6_2atmpS2001) {
            int32_t _M0L6_2atmpS1999 = _M0L11dst__offsetS108 + _M0L3lenS106;
            _if__result_4144 = _M0L6_2atmpS1999 <= _M0L13allocate__lenS105;
          } else {
            _if__result_4144 = 0;
          }
        } else {
          _if__result_4144 = 0;
        }
      } else {
        _if__result_4144 = 0;
      }
    } else {
      _if__result_4144 = 0;
    }
  } else {
    _if__result_4144 = 0;
  }
  if (_if__result_4144) {
    moonbit_incref(_M0L3srcS109);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS105, 0, _M0L3srcS109, _M0L11src__offsetS107, _M0L11dst__offsetS108, _M0L3lenS106);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS110;
    int32_t _M0L6_2atmpS2003;
    moonbit_string_t _M0L6_2atmpS2002;
    struct _M0TUsfE** _result_4145;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS110
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_13.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L13allocate__lenS105);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_14.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11src__offsetS107);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_15.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11dst__offsetS108);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_16.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L3lenS106);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_17.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2003 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L6_2atmpS2003);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2002
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS110);
    moonbit_decref(_M0L18_2astring__builderS110);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4145
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS2002);
    moonbit_decref(_M0L6_2atmpS2002);
    return _result_4145;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS84,
  moonbit_string_t _M0L3objS83
) {
  struct _M0TPB6Logger _M0L6_2atmpS1984;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS84);
  _M0L6_2atmpS1984
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS84
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS83, _M0L6_2atmpS1984);
  if (_M0L6_2atmpS1984.$1) {
    moonbit_decref(_M0L6_2atmpS1984.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS86,
  float _M0L3objS85
) {
  struct _M0TPB6Logger _M0L6_2atmpS1985;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS86);
  _M0L6_2atmpS1985
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS86
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS85, _M0L6_2atmpS1985);
  if (_M0L6_2atmpS1985.$1) {
    moonbit_decref(_M0L6_2atmpS1985.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS88,
  int32_t _M0L3objS87
) {
  struct _M0TPB6Logger _M0L6_2atmpS1986;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS88);
  _M0L6_2atmpS1986
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS88
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS87, _M0L6_2atmpS1986);
  if (_M0L6_2atmpS1986.$1) {
    moonbit_decref(_M0L6_2atmpS1986.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder* _M0L4selfS90,
  int32_t _M0L3objS89
) {
  struct _M0TPB6Logger _M0L6_2atmpS1987;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS90);
  _M0L6_2atmpS1987
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS90
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGbE(_M0L3objS89, _M0L6_2atmpS1987);
  if (_M0L6_2atmpS1987.$1) {
    moonbit_decref(_M0L6_2atmpS1987.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS92,
  uint64_t _M0L3objS91
) {
  struct _M0TPB6Logger _M0L6_2atmpS1988;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS92);
  _M0L6_2atmpS1988
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS92
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS91, _M0L6_2atmpS1988);
  if (_M0L6_2atmpS1988.$1) {
    moonbit_decref(_M0L6_2atmpS1988.$1);
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
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS66, (moonbit_string_t)moonbit_string_literal_5.data);
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
  int32_t _if__result_4146;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS14 == _M0L3srcS15) {
    _if__result_4146 = _M0L11dst__offsetS16 < _M0L11src__offsetS17;
  } else {
    _if__result_4146 = 0;
  }
  if (_if__result_4146) {
    int32_t _M0L1iS18 = 0;
    while (1) {
      if (_M0L1iS18 < _M0L3lenS19) {
        int32_t _M0L6_2atmpS1948 = _M0L11dst__offsetS16 + _M0L1iS18;
        int32_t _M0L6_2atmpS1950 = _M0L11src__offsetS17 + _M0L1iS18;
        int32_t _M0L6_2atmpS1949;
        int32_t _M0L6_2atmpS1951;
        if (
          _M0L6_2atmpS1950 < 0
          || _M0L6_2atmpS1950 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1949 = (int32_t)_M0L3srcS15[_M0L6_2atmpS1950];
        if (
          _M0L6_2atmpS1948 < 0
          || _M0L6_2atmpS1948 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS1948] = _M0L6_2atmpS1949;
        _M0L6_2atmpS1951 = _M0L1iS18 + 1;
        _M0L1iS18 = _M0L6_2atmpS1951;
        continue;
      } else {
        moonbit_decref(_M0L3srcS15);
        moonbit_decref(_M0L3dstS14);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1956 = _M0L3lenS19 - 1;
    int32_t _M0L1iS21 = _M0L6_2atmpS1956;
    while (1) {
      if (_M0L1iS21 >= 0) {
        int32_t _M0L6_2atmpS1952 = _M0L11dst__offsetS16 + _M0L1iS21;
        int32_t _M0L6_2atmpS1954 = _M0L11src__offsetS17 + _M0L1iS21;
        int32_t _M0L6_2atmpS1953;
        int32_t _M0L6_2atmpS1955;
        if (
          _M0L6_2atmpS1954 < 0
          || _M0L6_2atmpS1954 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1953 = (int32_t)_M0L3srcS15[_M0L6_2atmpS1954];
        if (
          _M0L6_2atmpS1952 < 0
          || _M0L6_2atmpS1952 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS1952] = _M0L6_2atmpS1953;
        _M0L6_2atmpS1955 = _M0L1iS21 - 1;
        _M0L1iS21 = _M0L6_2atmpS1955;
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
  int32_t _if__result_4149;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_4149 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_4149 = 0;
  }
  if (_if__result_4149) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS1957 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS1959 = _M0L11src__offsetS26 + _M0L1iS27;
        moonbit_string_t _M0L6_2atmpS1958;
        moonbit_string_t _M0L6_2aoldS3805;
        int32_t _M0L6_2atmpS1960;
        if (
          _M0L6_2atmpS1959 < 0
          || _M0L6_2atmpS1959 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1958 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS1959];
        if (
          _M0L6_2atmpS1957 < 0
          || _M0L6_2atmpS1957 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3805 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS1957];
        moonbit_incref(_M0L6_2atmpS1958);
        moonbit_decref(_M0L6_2aoldS3805);
        _M0L3dstS23[_M0L6_2atmpS1957] = _M0L6_2atmpS1958;
        _M0L6_2atmpS1960 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS1960;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1965 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS1965;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS1961 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS1963 = _M0L11src__offsetS26 + _M0L1iS30;
        moonbit_string_t _M0L6_2atmpS1962;
        moonbit_string_t _M0L6_2aoldS3807;
        int32_t _M0L6_2atmpS1964;
        if (
          _M0L6_2atmpS1963 < 0
          || _M0L6_2atmpS1963 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1962 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS1963];
        if (
          _M0L6_2atmpS1961 < 0
          || _M0L6_2atmpS1961 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3807 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS1961];
        moonbit_incref(_M0L6_2atmpS1962);
        moonbit_decref(_M0L6_2aoldS3807);
        _M0L3dstS23[_M0L6_2atmpS1961] = _M0L6_2atmpS1962;
        _M0L6_2atmpS1964 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS1964;
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
  int32_t _if__result_4152;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_4152 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_4152 = 0;
  }
  if (_if__result_4152) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS1966 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS1968 = _M0L11src__offsetS35 + _M0L1iS36;
        moonbit_string_t _M0L6_2atmpS1967;
        moonbit_string_t _M0L6_2aoldS3809;
        int32_t _M0L6_2atmpS1969;
        if (
          _M0L6_2atmpS1968 < 0
          || _M0L6_2atmpS1968 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1967 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1968];
        if (
          _M0L6_2atmpS1966 < 0
          || _M0L6_2atmpS1966 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3809 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1966];
        if (_M0L6_2atmpS1967) {
          moonbit_incref(_M0L6_2atmpS1967);
        }
        if (_M0L6_2aoldS3809) {
          moonbit_decref(_M0L6_2aoldS3809);
        }
        _M0L3dstS32[_M0L6_2atmpS1966] = _M0L6_2atmpS1967;
        _M0L6_2atmpS1969 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS1969;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1974 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS1974;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS1970 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS1972 = _M0L11src__offsetS35 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS1971;
        moonbit_string_t _M0L6_2aoldS3811;
        int32_t _M0L6_2atmpS1973;
        if (
          _M0L6_2atmpS1972 < 0
          || _M0L6_2atmpS1972 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1971 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1972];
        if (
          _M0L6_2atmpS1970 < 0
          || _M0L6_2atmpS1970 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3811 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1970];
        if (_M0L6_2atmpS1971) {
          moonbit_incref(_M0L6_2atmpS1971);
        }
        if (_M0L6_2aoldS3811) {
          moonbit_decref(_M0L6_2aoldS3811);
        }
        _M0L3dstS32[_M0L6_2atmpS1970] = _M0L6_2atmpS1971;
        _M0L6_2atmpS1973 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS1973;
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
  int32_t _if__result_4155;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_4155 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_4155 = 0;
  }
  if (_if__result_4155) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS1975 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS1977 = _M0L11src__offsetS44 + _M0L1iS45;
        struct _M0TUsfE* _M0L6_2atmpS1976;
        struct _M0TUsfE* _M0L6_2aoldS3813;
        int32_t _M0L6_2atmpS1978;
        if (
          _M0L6_2atmpS1977 < 0
          || _M0L6_2atmpS1977 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1976 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS1977];
        if (
          _M0L6_2atmpS1975 < 0
          || _M0L6_2atmpS1975 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3813 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS1975];
        if (_M0L6_2atmpS1976) {
          moonbit_incref(_M0L6_2atmpS1976);
        }
        if (_M0L6_2aoldS3813) {
          moonbit_decref(_M0L6_2aoldS3813);
        }
        _M0L3dstS41[_M0L6_2atmpS1975] = _M0L6_2atmpS1976;
        _M0L6_2atmpS1978 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS1978;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1983 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS1983;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS1979 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS1981 = _M0L11src__offsetS44 + _M0L1iS48;
        struct _M0TUsfE* _M0L6_2atmpS1980;
        struct _M0TUsfE* _M0L6_2aoldS3815;
        int32_t _M0L6_2atmpS1982;
        if (
          _M0L6_2atmpS1981 < 0
          || _M0L6_2atmpS1981 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1980 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS1981];
        if (
          _M0L6_2atmpS1979 < 0
          || _M0L6_2atmpS1979 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3815 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS1979];
        if (_M0L6_2atmpS1980) {
          moonbit_incref(_M0L6_2atmpS1980);
        }
        if (_M0L6_2aoldS3815) {
          moonbit_decref(_M0L6_2aoldS3815);
        }
        _M0L3dstS41[_M0L6_2atmpS1979] = _M0L6_2atmpS1980;
        _M0L6_2atmpS1982 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS1982;
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
  uint32_t _M0L6_2atmpS1947;
  uint32_t _M0L6_2atmpS1946;
  uint32_t _M0L6_2atmpS1945;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1947 = _M0L5inputS10 * 3266489917u;
  _M0L6_2atmpS1946 = _M0L3accS9 + _M0L6_2atmpS1947;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1945 = _M0FPB4rotl(_M0L6_2atmpS1946, 17);
  return _M0L6_2atmpS1945 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS7, int32_t _M0L1rS8) {
  uint32_t _M0L6_2atmpS1942;
  int32_t _M0L6_2atmpS1944;
  uint32_t _M0L6_2atmpS1943;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1942 = _M0L1xS7 << (_M0L1rS8 & 31);
  _M0L6_2atmpS1944 = 32 - _M0L1rS8;
  _M0L6_2atmpS1943 = _M0L1xS7 >> (_M0L6_2atmpS1944 & 31);
  return _M0L6_2atmpS1942 | _M0L6_2atmpS1943;
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
  void* _M0L11_2aobj__ptrS1810,
  struct _M0TPB4Show _M0L8_2aparamS1809
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1808 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1810;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS1808, _M0L8_2aparamS1809);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1807,
  struct _M0TPB4Show _M0L8_2aparamS1806
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1805 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1807;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS1805, _M0L8_2aparamS1806);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1804,
  int32_t _M0L8_2aparamS1803
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1802 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1804;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1802, _M0L8_2aparamS1803);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1801,
  struct _M0TPC16string10StringView _M0L8_2aparamS1800
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1799 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1801;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1799, _M0L8_2aparamS1800);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1798,
  moonbit_string_t _M0L8_2aparamS1795,
  int32_t _M0L8_2aparamS1796,
  int32_t _M0L8_2aparamS1797
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1794 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1798;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1794, _M0L8_2aparamS1795, _M0L8_2aparamS1796, _M0L8_2aparamS1797);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1793,
  moonbit_string_t _M0L8_2aparamS1792
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1791 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1793;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1791, _M0L8_2aparamS1792);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP38JIA2JIA29moonbitdb3lib8Database* _M0L2dbS1674;
  struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _M0L6_2atmpS1937;
  struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _M0L6_2atmpS1938;
  struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _M0L6_2atmpS1939;
  struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _M0L6_2atmpS1940;
  struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _M0L6_2atmpS1941;
  struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product** _M0L6_2atmpS1936;
  struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples14shopping__cart7ProductE* _M0L8productsS1675;
  int32_t _M0L7_2abindS1676;
  int32_t _M0L2__S1677;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1685;
  int32_t _M0L6_2atmpS1827;
  moonbit_string_t _M0L6_2atmpS1826;
  moonbit_string_t _M0L9cart__keyS1686;
  int32_t _M0L6_2atmpS1828;
  int32_t _M0L6_2atmpS1829;
  int32_t _M0L6_2atmpS1830;
  struct _M0TPB3MapGssE* _M0L11cart__itemsS1687;
  struct _M0TPB8MutLocalGfE* _M0L5totalS1688;
  struct _M0TPB4IterGUssEE* _M0L5_2aitS1689;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1706;
  float _M0L3valS1841;
  moonbit_string_t _M0L6_2atmpS1840;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1707;
  int32_t _M0L6_2atmpS1843;
  moonbit_string_t _M0L6_2atmpS1842;
  moonbit_string_t _M0L12history__keyS1708;
  moonbit_string_t* _M0L6_2atmpS1935;
  struct _M0TPB5ArrayGsE* _M0L6viewedS1709;
  int32_t _M0L7_2abindS1710;
  int32_t _M0L2__S1711;
  struct _M0TPB5ArrayGsE* _M0L6recentS1714;
  int32_t _M0L1iS1715;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1721;
  int32_t _M0L6_2atmpS1854;
  moonbit_string_t _M0L6_2atmpS1853;
  moonbit_string_t _M0L8fav__keyS1722;
  int32_t _M0L6_2atmpS1855;
  int32_t _M0L6_2atmpS1856;
  int32_t _M0L6_2atmpS1857;
  int32_t _M0L6_2atmpS1858;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1723;
  int32_t _M0L6_2atmpS1860;
  moonbit_string_t _M0L6_2atmpS1859;
  struct _M0TPB5ArrayGsE* _M0L4favsS1724;
  int32_t _M0L7_2abindS1725;
  int32_t _M0L2__S1726;
  moonbit_string_t _M0L10user2__favS1732;
  int32_t _M0L6_2atmpS1866;
  int32_t _M0L6_2atmpS1867;
  int32_t _M0L6_2atmpS1868;
  moonbit_string_t* _M0L6_2atmpS1934;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1933;
  struct _M0TPB5ArrayGsE* _M0L6commonS1733;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1734;
  int32_t _M0L6_2atmpS1870;
  moonbit_string_t _M0L6_2atmpS1869;
  int32_t _M0L7_2abindS1735;
  int32_t _M0L2__S1736;
  int32_t _M0L6_2atmpS1876;
  int32_t _M0L6_2atmpS1877;
  int32_t _M0L6_2atmpS1878;
  int32_t _M0L6_2atmpS1879;
  int32_t _M0L6_2atmpS1880;
  int32_t _M0L6_2atmpS1881;
  int32_t _M0L6_2atmpS1882;
  struct _M0TPB5ArrayGsE* _M0L8hardwareS1742;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1743;
  int32_t _M0L6_2atmpS1884;
  moonbit_string_t _M0L6_2atmpS1883;
  int32_t _M0L7_2abindS1744;
  int32_t _M0L2__S1745;
  moonbit_string_t* _M0L6_2atmpS1932;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1931;
  struct _M0TPB5ArrayGsE* _M0L15hw__and__periphS1751;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1752;
  int32_t _M0L6_2atmpS1891;
  moonbit_string_t _M0L6_2atmpS1890;
  moonbit_string_t* _M0L6_2atmpS1930;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1929;
  struct _M0TPB5ArrayGsE* _M0L9all__tagsS1753;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1754;
  int32_t _M0L6_2atmpS1893;
  moonbit_string_t _M0L6_2atmpS1892;
  moonbit_string_t _M0L10sales__keyS1755;
  int32_t _M0L6_2atmpS1894;
  int32_t _M0L6_2atmpS1895;
  int32_t _M0L6_2atmpS1896;
  int32_t _M0L6_2atmpS1897;
  int32_t _M0L6_2atmpS1898;
  struct _M0TPB5ArrayGsE* _M0L4top3S1756;
  int32_t _M0L1iS1757;
  struct _M0TPB5ArrayGsE* _M0L10mid__salesS1770;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1771;
  int32_t _M0L6_2atmpS1906;
  moonbit_string_t _M0L6_2atmpS1905;
  moonbit_string_t _M0L11session__idS1772;
  int32_t _M0L6_2atmpS1907;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1773;
  moonbit_string_t _M0L6_2atmpS1908;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1774;
  moonbit_string_t _M0L6_2atmpS1911;
  moonbit_string_t _M0L6_2atmpS1910;
  moonbit_string_t _M0L6_2atmpS1909;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1775;
  int32_t _M0L6_2atmpS1913;
  moonbit_string_t _M0L6_2atmpS1912;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1776;
  int32_t _M0L6_2atmpS1915;
  moonbit_string_t _M0L6_2atmpS1914;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1777;
  int32_t _M0L6_2atmpS1917;
  moonbit_string_t _M0L6_2atmpS1916;
  moonbit_string_t* _M0L6_2atmpS1928;
  struct _M0TPB5ArrayGsE* _M0L9hot__keysS1778;
  moonbit_string_t* _M0L6_2atmpS1927;
  struct _M0TPB5ArrayGsE* _M0L9hot__valsS1779;
  int32_t _M0L6_2atmpS1918;
  struct _M0TPB5ArrayGOsE* _M0L13batch__resultS1780;
  int32_t _M0L1iS1781;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1790;
  int32_t _M0L6_2atmpS1926;
  moonbit_string_t _M0L6_2atmpS1925;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 66 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L2dbS1674 = _M0MP38JIA2JIA29moonbitdb3lib8Database3new();
  #line 68 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_18.data);
  #line 69 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_19.data);
  #line 70 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_18.data);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1937
  = _M0MP48JIA2JIA29moonbitdb8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, 0x1.8cp+6f, 100);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1938
  = _M0MP48JIA2JIA29moonbitdb8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_22.data, (moonbit_string_t)moonbit_string_literal_23.data, 0x1.76ep+11f, 50);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1939
  = _M0MP48JIA2JIA29moonbitdb8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_24.data, (moonbit_string_t)moonbit_string_literal_25.data, 0x1.8fp+8f, 200);
  #line 76 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1940
  = _M0MP48JIA2JIA29moonbitdb8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_26.data, (moonbit_string_t)moonbit_string_literal_27.data, 0x1.f3cp+10f, 30);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1941
  = _M0MP48JIA2JIA29moonbitdb8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_28.data, (moonbit_string_t)moonbit_string_literal_29.data, 0x1.02p+7f, 500);
  _M0L6_2atmpS1936
  = (struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product**)moonbit_make_ref_array_raw(5);
  _M0L6_2atmpS1936[0] = _M0L6_2atmpS1937;
  _M0L6_2atmpS1936[1] = _M0L6_2atmpS1938;
  _M0L6_2atmpS1936[2] = _M0L6_2atmpS1939;
  _M0L6_2atmpS1936[3] = _M0L6_2atmpS1940;
  _M0L6_2atmpS1936[4] = _M0L6_2atmpS1941;
  _M0L8productsS1675
  = (struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples14shopping__cart7ProductE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48JIA2JIA29moonbitdb8examples14shopping__cart7ProductE));
  Moonbit_object_header(_M0L8productsS1675)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 135, 0);
  _M0L8productsS1675->$0 = _M0L6_2atmpS1936;
  _M0L8productsS1675->$1 = 5;
  #line 80 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_30.data);
  #line 81 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  _M0L7_2abindS1676 = _M0L8productsS1675->$1;
  _M0L2__S1677 = 0;
  while (1) {
    if (_M0L2__S1677 < _M0L7_2abindS1676) {
      struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product** _M0L3bufS1825 =
        _M0L8productsS1675->$0;
      struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product* _M0L7productS1678 =
        (struct _M0TP48JIA2JIA29moonbitdb8examples14shopping__cart7Product*)_M0L3bufS1825[
          _M0L2__S1677
        ];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1680;
      moonbit_string_t _M0L2idS1823;
      moonbit_string_t _M0L3keyS1679;
      moonbit_string_t _M0L4nameS1812;
      int32_t _M0L6_2atmpS1811;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1681;
      float _M0L5priceS1815;
      moonbit_string_t _M0L6_2atmpS1814;
      int32_t _M0L6_2atmpS1813;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1682;
      int32_t _M0L5stockS1818;
      moonbit_string_t _M0L6_2atmpS1817;
      int32_t _M0L6_2atmpS1816;
      int32_t _M0L6_2atmpS1819;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1683;
      moonbit_string_t _M0L4nameS1821;
      moonbit_string_t _M0L8_2afieldS3827;
      int32_t _M0L6_2acntS3888;
      moonbit_string_t _M0L2idS1822;
      moonbit_string_t _M0L6_2atmpS1820;
      int32_t _M0L6_2atmpS1824;
      moonbit_incref(_M0L7productS1678);
      #line 83 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1680
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 83 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1680, (moonbit_string_t)moonbit_string_literal_32.data);
      _M0L2idS1823 = _M0L7productS1678->$0;
      moonbit_incref(_M0L2idS1823);
      #line 83 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1680, _M0L2idS1823);
      moonbit_decref(_M0L2idS1823);
      #line 83 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L3keyS1679
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1680);
      moonbit_decref(_M0L18_2astring__builderS1680);
      _M0L4nameS1812 = _M0L7productS1678->$1;
      moonbit_incref(_M0L4nameS1812);
      #line 84 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1811
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1674, _M0L3keyS1679, (moonbit_string_t)moonbit_string_literal_33.data, _M0L4nameS1812);
      moonbit_decref(_M0L4nameS1812);
      #line 85 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1681
      = _M0MPB13StringBuilder21StringBuilder_2einner(0);
      _M0L5priceS1815 = _M0L7productS1678->$2;
      #line 85 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1681, _M0L5priceS1815);
      #line 85 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1814
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1681);
      moonbit_decref(_M0L18_2astring__builderS1681);
      #line 85 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1813
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1674, _M0L3keyS1679, (moonbit_string_t)moonbit_string_literal_34.data, _M0L6_2atmpS1814);
      moonbit_decref(_M0L6_2atmpS1814);
      #line 86 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1682
      = _M0MPB13StringBuilder21StringBuilder_2einner(0);
      _M0L5stockS1818 = _M0L7productS1678->$3;
      #line 86 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1682, _M0L5stockS1818);
      #line 86 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1817
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1682);
      moonbit_decref(_M0L18_2astring__builderS1682);
      #line 86 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1816
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1674, _M0L3keyS1679, (moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS1817);
      moonbit_decref(_M0L6_2atmpS1817);
      #line 87 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1819
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(_M0L2dbS1674, _M0L3keyS1679, 3600);
      moonbit_decref(_M0L3keyS1679);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1683
      = _M0MPB13StringBuilder21StringBuilder_2einner(35);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1683, (moonbit_string_t)moonbit_string_literal_36.data);
      _M0L4nameS1821 = _M0L7productS1678->$1;
      moonbit_incref(_M0L4nameS1821);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1683, _M0L4nameS1821);
      moonbit_decref(_M0L4nameS1821);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1683, (moonbit_string_t)moonbit_string_literal_37.data);
      _M0L8_2afieldS3827 = _M0L7productS1678->$0;
      _M0L6_2acntS3888
      = Moonbit_rc_count(Moonbit_object_header(_M0L7productS1678));
      if (_M0L6_2acntS3888 > 1) {
        int32_t _M0L11_2anew__cntS3890 = _M0L6_2acntS3888 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L7productS1678), _M0L11_2anew__cntS3890);
        moonbit_incref(_M0L8_2afieldS3827);
      } else if (_M0L6_2acntS3888 == 1) {
        moonbit_string_t _M0L8_2afieldS3889 = _M0L7productS1678->$1;
        moonbit_decref(_M0L8_2afieldS3889);
        #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        moonbit_free(_M0L7productS1678);
      }
      _M0L2idS1822 = _M0L8_2afieldS3827;
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1683, _M0L2idS1822);
      moonbit_decref(_M0L2idS1822);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1683, (moonbit_string_t)moonbit_string_literal_38.data);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1820
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1683);
      moonbit_decref(_M0L18_2astring__builderS1683);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1820);
      moonbit_decref(_M0L6_2atmpS1820);
      _M0L6_2atmpS1824 = _M0L2__S1677 + 1;
      _M0L2__S1677 = _M0L6_2atmpS1824;
      continue;
    } else {
      moonbit_decref(_M0L8productsS1675);
    }
    break;
  }
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1685
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1685, (moonbit_string_t)moonbit_string_literal_39.data);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1827
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(_M0L2dbS1674);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1685, _M0L6_2atmpS1827);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1826
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1685);
  moonbit_decref(_M0L18_2astring__builderS1685);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1826);
  moonbit_decref(_M0L6_2atmpS1826);
  #line 92 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_40.data);
  #line 93 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  _M0L9cart__keyS1686 = (moonbit_string_t)moonbit_string_literal_41.data;
  #line 95 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1828
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1674, _M0L9cart__keyS1686, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_42.data);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1829
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1674, _M0L9cart__keyS1686, (moonbit_string_t)moonbit_string_literal_24.data, (moonbit_string_t)moonbit_string_literal_43.data);
  #line 97 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1830
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hset(_M0L2dbS1674, _M0L9cart__keyS1686, (moonbit_string_t)moonbit_string_literal_28.data, (moonbit_string_t)moonbit_string_literal_44.data);
  #line 98 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_45.data);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L11cart__itemsS1687
  = _M0MP38JIA2JIA29moonbitdb3lib8Database7hgetall(_M0L2dbS1674, _M0L9cart__keyS1686);
  _M0L5totalS1688
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L5totalS1688)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5totalS1688->$0 = 0x0p+0f;
  #line 100 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L5_2aitS1689 = _M0MPB3Map5iter2GssE(_M0L11cart__itemsS1687);
  moonbit_decref(_M0L11cart__itemsS1687);
  while (1) {
    moonbit_string_t _M0L3pidS1691;
    moonbit_string_t _M0L8qty__strS1692;
    struct _M0TUssE* _M0L7_2abindS1701;
    int32_t _M0L3qtyS1693;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1695;
    moonbit_string_t _M0L6_2atmpS1839;
    moonbit_string_t _M0L6_2atmpS1838;
    moonbit_string_t _M0L6_2atmpS1837;
    float _M0L5priceS1694;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1697;
    moonbit_string_t _M0L6_2atmpS1836;
    moonbit_string_t _M0L6_2atmpS1835;
    moonbit_string_t _M0L4nameS1696;
    float _M0L6_2atmpS1834;
    float _M0L8subtotalS1698;
    float _M0L3valS1832;
    float _M0L6_2atmpS1831;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1699;
    moonbit_string_t _M0L6_2atmpS1833;
    #line 101 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L7_2abindS1701 = _M0MPB5Iter24nextGssE(_M0L5_2aitS1689);
    if (_M0L7_2abindS1701 == 0) {
      if (_M0L7_2abindS1701) {
        moonbit_decref(_M0L7_2abindS1701);
      }
      moonbit_decref(_M0L5_2aitS1689);
    } else {
      struct _M0TUssE* _M0L7_2aSomeS1702 = _M0L7_2abindS1701;
      struct _M0TUssE* _M0L4_2axS1703 = _M0L7_2aSomeS1702;
      moonbit_string_t _M0L6_2apidS1704 = _M0L4_2axS1703->$0;
      moonbit_string_t _M0L8_2afieldS3825 = _M0L4_2axS1703->$1;
      int32_t _M0L6_2acntS3891 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1703));
      moonbit_string_t _M0L11_2aqty__strS1705;
      if (_M0L6_2acntS3891 > 1) {
        int32_t _M0L11_2anew__cntS3892 = _M0L6_2acntS3891 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1703), _M0L11_2anew__cntS3892);
        moonbit_incref(_M0L8_2afieldS3825);
        moonbit_incref(_M0L6_2apidS1704);
      } else if (_M0L6_2acntS3891 == 1) {
        #line 101 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        moonbit_free(_M0L4_2axS1703);
      }
      _M0L11_2aqty__strS1705 = _M0L8_2afieldS3825;
      _M0L3pidS1691 = _M0L6_2apidS1704;
      _M0L8qty__strS1692 = _M0L11_2aqty__strS1705;
      goto join_1690;
    }
    goto joinlet_4160;
    join_1690:;
    #line 102 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L3qtyS1693
    = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart10parse__int(_M0L8qty__strS1692);
    moonbit_decref(_M0L8qty__strS1692);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L18_2astring__builderS1695
    = _M0MPB13StringBuilder21StringBuilder_2einner(8);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1695, (moonbit_string_t)moonbit_string_literal_32.data);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1695, _M0L3pidS1691);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1839
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1695);
    moonbit_decref(_M0L18_2astring__builderS1695);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1838
    = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1674, _M0L6_2atmpS1839, (moonbit_string_t)moonbit_string_literal_34.data);
    moonbit_decref(_M0L6_2atmpS1839);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1837
    = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(_M0L6_2atmpS1838);
    if (_M0L6_2atmpS1838) {
      moonbit_decref(_M0L6_2atmpS1838);
    }
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L5priceS1694
    = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart12parse__float(_M0L6_2atmpS1837);
    moonbit_decref(_M0L6_2atmpS1837);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L18_2astring__builderS1697
    = _M0MPB13StringBuilder21StringBuilder_2einner(8);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1697, (moonbit_string_t)moonbit_string_literal_32.data);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1697, _M0L3pidS1691);
    moonbit_decref(_M0L3pidS1691);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1836
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1697);
    moonbit_decref(_M0L18_2astring__builderS1697);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1835
    = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1674, _M0L6_2atmpS1836, (moonbit_string_t)moonbit_string_literal_33.data);
    moonbit_decref(_M0L6_2atmpS1836);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L4nameS1696
    = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(_M0L6_2atmpS1835);
    if (_M0L6_2atmpS1835) {
      moonbit_decref(_M0L6_2atmpS1835);
    }
    _M0L6_2atmpS1834 = (float)_M0L3qtyS1693;
    _M0L8subtotalS1698 = _M0L5priceS1694 * _M0L6_2atmpS1834;
    _M0L3valS1832 = _M0L5totalS1688->$0;
    _M0L6_2atmpS1831 = _M0L3valS1832 + _M0L8subtotalS1698;
    _M0L5totalS1688->$0 = _M0L6_2atmpS1831;
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L18_2astring__builderS1699
    = _M0MPB13StringBuilder21StringBuilder_2einner(12);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1699, (moonbit_string_t)moonbit_string_literal_46.data);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1699, _M0L4nameS1696);
    moonbit_decref(_M0L4nameS1696);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1699, (moonbit_string_t)moonbit_string_literal_47.data);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1699, _M0L3qtyS1693);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1699, (moonbit_string_t)moonbit_string_literal_48.data);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1699, _M0L8subtotalS1698);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1833
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1699);
    moonbit_decref(_M0L18_2astring__builderS1699);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0FPB7printlnGsE(_M0L6_2atmpS1833);
    moonbit_decref(_M0L6_2atmpS1833);
    continue;
    joinlet_4160:;
    break;
  }
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1706
  = _M0MPB13StringBuilder21StringBuilder_2einner(21);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1706, (moonbit_string_t)moonbit_string_literal_49.data);
  _M0L3valS1841 = _M0L5totalS1688->$0;
  moonbit_decref(_M0L5totalS1688);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1706, _M0L3valS1841);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1840
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1706);
  moonbit_decref(_M0L18_2astring__builderS1706);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1840);
  moonbit_decref(_M0L6_2atmpS1840);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1707
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1707, (moonbit_string_t)moonbit_string_literal_50.data);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1843
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4hlen(_M0L2dbS1674, _M0L9cart__keyS1686);
  moonbit_decref(_M0L9cart__keyS1686);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1707, _M0L6_2atmpS1843);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1842
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1707);
  moonbit_decref(_M0L18_2astring__builderS1707);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1842);
  moonbit_decref(_M0L6_2atmpS1842);
  #line 112 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_51.data);
  #line 113 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  _M0L12history__keyS1708 = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS1935 = (moonbit_string_t*)moonbit_make_ref_array_raw(7);
  _M0L6_2atmpS1935[0] = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS1935[1] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS1935[2] = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS1935[3] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS1935[4] = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS1935[5] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS1935[6] = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6viewedS1709
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6viewedS1709)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6viewedS1709->$0 = _M0L6_2atmpS1935;
  _M0L6viewedS1709->$1 = 7;
  _M0L7_2abindS1710 = _M0L6viewedS1709->$1;
  _M0L2__S1711 = 0;
  while (1) {
    if (_M0L2__S1711 < _M0L7_2abindS1710) {
      moonbit_string_t* _M0L3bufS1846 = _M0L6viewedS1709->$0;
      moonbit_string_t _M0L3pidS1712 =
        (moonbit_string_t)_M0L3bufS1846[_M0L2__S1711];
      int32_t _M0L6_2atmpS1844;
      int32_t _M0L6_2atmpS1845;
      moonbit_incref(_M0L3pidS1712);
      #line 117 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1844
      = _M0MP38JIA2JIA29moonbitdb3lib8Database5lpush(_M0L2dbS1674, _M0L12history__keyS1708, _M0L3pidS1712);
      moonbit_decref(_M0L3pidS1712);
      _M0L6_2atmpS1845 = _M0L2__S1711 + 1;
      _M0L2__S1711 = _M0L6_2atmpS1845;
      continue;
    } else {
      moonbit_decref(_M0L6viewedS1709);
    }
    break;
  }
  #line 119 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_53.data);
  #line 120 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6recentS1714
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6lrange(_M0L2dbS1674, _M0L12history__keyS1708, 0, 4);
  _M0L1iS1715 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1847;
    #line 121 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1847 = _M0MPC15array5Array6lengthGsE(_M0L6recentS1714);
    if (_M0L1iS1715 < _M0L6_2atmpS1847) {
      moonbit_string_t _M0L3pidS1716;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1718;
      moonbit_string_t _M0L6_2atmpS1851;
      moonbit_string_t _M0L6_2atmpS1850;
      moonbit_string_t _M0L4nameS1717;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1719;
      int32_t _M0L6_2atmpS1849;
      moonbit_string_t _M0L6_2atmpS1848;
      int32_t _M0L6_2atmpS1852;
      #line 122 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L3pidS1716
      = _M0MPC15array5Array2atGsE(_M0L6recentS1714, _M0L1iS1715);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1718
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1718, (moonbit_string_t)moonbit_string_literal_32.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1718, _M0L3pidS1716);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1851
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1718);
      moonbit_decref(_M0L18_2astring__builderS1718);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1850
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1674, _M0L6_2atmpS1851, (moonbit_string_t)moonbit_string_literal_33.data);
      moonbit_decref(_M0L6_2atmpS1851);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS1717
      = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(_M0L6_2atmpS1850);
      if (_M0L6_2atmpS1850) {
        moonbit_decref(_M0L6_2atmpS1850);
      }
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1719
      = _M0MPB13StringBuilder21StringBuilder_2einner(9);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1719, (moonbit_string_t)moonbit_string_literal_46.data);
      _M0L6_2atmpS1849 = _M0L1iS1715 + 1;
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1719, _M0L6_2atmpS1849);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1719, (moonbit_string_t)moonbit_string_literal_54.data);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1719, _M0L4nameS1717);
      moonbit_decref(_M0L4nameS1717);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1719, (moonbit_string_t)moonbit_string_literal_55.data);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1719, _M0L3pidS1716);
      moonbit_decref(_M0L3pidS1716);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1719, (moonbit_string_t)moonbit_string_literal_56.data);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1848
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1719);
      moonbit_decref(_M0L18_2astring__builderS1719);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1848);
      moonbit_decref(_M0L6_2atmpS1848);
      _M0L6_2atmpS1852 = _M0L1iS1715 + 1;
      _M0L1iS1715 = _M0L6_2atmpS1852;
      continue;
    } else {
      moonbit_decref(_M0L6recentS1714);
    }
    break;
  }
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1721
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1721, (moonbit_string_t)moonbit_string_literal_57.data);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1854
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4llen(_M0L2dbS1674, _M0L12history__keyS1708);
  moonbit_decref(_M0L12history__keyS1708);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1721, _M0L6_2atmpS1854);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1853
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1721);
  moonbit_decref(_M0L18_2astring__builderS1721);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1853);
  moonbit_decref(_M0L6_2atmpS1853);
  #line 128 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_58.data);
  #line 129 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  _M0L8fav__keyS1722 = (moonbit_string_t)moonbit_string_literal_59.data;
  #line 131 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1855
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, _M0L8fav__keyS1722, (moonbit_string_t)moonbit_string_literal_20.data);
  #line 132 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1856
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, _M0L8fav__keyS1722, (moonbit_string_t)moonbit_string_literal_22.data);
  #line 133 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1857
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, _M0L8fav__keyS1722, (moonbit_string_t)moonbit_string_literal_26.data);
  #line 134 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1858
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, _M0L8fav__keyS1722, (moonbit_string_t)moonbit_string_literal_20.data);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1723
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1723, (moonbit_string_t)moonbit_string_literal_60.data);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1860
  = _M0MP38JIA2JIA29moonbitdb3lib8Database5scard(_M0L2dbS1674, _M0L8fav__keyS1722);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1723, _M0L6_2atmpS1860);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1859
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1723);
  moonbit_decref(_M0L18_2astring__builderS1723);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1859);
  moonbit_decref(_M0L6_2atmpS1859);
  #line 136 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L4favsS1724
  = _M0MP38JIA2JIA29moonbitdb3lib8Database8smembers(_M0L2dbS1674, _M0L8fav__keyS1722);
  _M0L7_2abindS1725 = _M0L4favsS1724->$1;
  _M0L2__S1726 = 0;
  while (1) {
    if (_M0L2__S1726 < _M0L7_2abindS1725) {
      moonbit_string_t* _M0L3bufS1865 = _M0L4favsS1724->$0;
      moonbit_string_t _M0L3pidS1727 =
        (moonbit_string_t)_M0L3bufS1865[_M0L2__S1726];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1729;
      moonbit_string_t _M0L6_2atmpS1863;
      moonbit_string_t _M0L6_2atmpS1862;
      moonbit_string_t _M0L4nameS1728;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1730;
      moonbit_string_t _M0L6_2atmpS1861;
      int32_t _M0L6_2atmpS1864;
      moonbit_incref(_M0L3pidS1727);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1729
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1729, (moonbit_string_t)moonbit_string_literal_32.data);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1729, _M0L3pidS1727);
      moonbit_decref(_M0L3pidS1727);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1863
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1729);
      moonbit_decref(_M0L18_2astring__builderS1729);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1862
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1674, _M0L6_2atmpS1863, (moonbit_string_t)moonbit_string_literal_33.data);
      moonbit_decref(_M0L6_2atmpS1863);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS1728
      = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(_M0L6_2atmpS1862);
      if (_M0L6_2atmpS1862) {
        moonbit_decref(_M0L6_2atmpS1862);
      }
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1730
      = _M0MPB13StringBuilder21StringBuilder_2einner(6);
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1730, (moonbit_string_t)moonbit_string_literal_61.data);
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1730, _M0L4nameS1728);
      moonbit_decref(_M0L4nameS1728);
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1861
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1730);
      moonbit_decref(_M0L18_2astring__builderS1730);
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1861);
      moonbit_decref(_M0L6_2atmpS1861);
      _M0L6_2atmpS1864 = _M0L2__S1726 + 1;
      _M0L2__S1726 = _M0L6_2atmpS1864;
      continue;
    } else {
      moonbit_decref(_M0L4favsS1724);
    }
    break;
  }
  #line 142 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_62.data);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  _M0L10user2__favS1732 = (moonbit_string_t)moonbit_string_literal_63.data;
  #line 145 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1866
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, _M0L10user2__favS1732, (moonbit_string_t)moonbit_string_literal_20.data);
  #line 146 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1867
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, _M0L10user2__favS1732, (moonbit_string_t)moonbit_string_literal_24.data);
  #line 147 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1868
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, _M0L10user2__favS1732, (moonbit_string_t)moonbit_string_literal_28.data);
  _M0L6_2atmpS1934 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1934[0] = _M0L8fav__keyS1722;
  _M0L6_2atmpS1934[1] = _M0L10user2__favS1732;
  _M0L6_2atmpS1933
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1933)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS1933->$0 = _M0L6_2atmpS1934;
  _M0L6_2atmpS1933->$1 = 2;
  #line 148 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6commonS1733
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6sinter(_M0L2dbS1674, _M0L6_2atmpS1933);
  moonbit_decref(_M0L6_2atmpS1933);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1734
  = _M0MPB13StringBuilder21StringBuilder_2einner(32);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1734, (moonbit_string_t)moonbit_string_literal_64.data);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1870 = _M0MPC15array5Array6lengthGsE(_M0L6commonS1733);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1734, _M0L6_2atmpS1870);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1734, (moonbit_string_t)moonbit_string_literal_65.data);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1869
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1734);
  moonbit_decref(_M0L18_2astring__builderS1734);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1869);
  moonbit_decref(_M0L6_2atmpS1869);
  _M0L7_2abindS1735 = _M0L6commonS1733->$1;
  _M0L2__S1736 = 0;
  while (1) {
    if (_M0L2__S1736 < _M0L7_2abindS1735) {
      moonbit_string_t* _M0L3bufS1875 = _M0L6commonS1733->$0;
      moonbit_string_t _M0L3pidS1737 =
        (moonbit_string_t)_M0L3bufS1875[_M0L2__S1736];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1739;
      moonbit_string_t _M0L6_2atmpS1873;
      moonbit_string_t _M0L6_2atmpS1872;
      moonbit_string_t _M0L4nameS1738;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1740;
      moonbit_string_t _M0L6_2atmpS1871;
      int32_t _M0L6_2atmpS1874;
      moonbit_incref(_M0L3pidS1737);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1739
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1739, (moonbit_string_t)moonbit_string_literal_32.data);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1739, _M0L3pidS1737);
      moonbit_decref(_M0L3pidS1737);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1873
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1739);
      moonbit_decref(_M0L18_2astring__builderS1739);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1872
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1674, _M0L6_2atmpS1873, (moonbit_string_t)moonbit_string_literal_33.data);
      moonbit_decref(_M0L6_2atmpS1873);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS1738
      = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(_M0L6_2atmpS1872);
      if (_M0L6_2atmpS1872) {
        moonbit_decref(_M0L6_2atmpS1872);
      }
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1740
      = _M0MPB13StringBuilder21StringBuilder_2einner(6);
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1740, (moonbit_string_t)moonbit_string_literal_61.data);
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1740, _M0L4nameS1738);
      moonbit_decref(_M0L4nameS1738);
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1871
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1740);
      moonbit_decref(_M0L18_2astring__builderS1740);
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1871);
      moonbit_decref(_M0L6_2atmpS1871);
      _M0L6_2atmpS1874 = _M0L2__S1736 + 1;
      _M0L2__S1736 = _M0L6_2atmpS1874;
      continue;
    } else {
      moonbit_decref(_M0L6commonS1733);
    }
    break;
  }
  #line 155 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_66.data);
  #line 156 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  #line 157 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1876
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, (moonbit_string_t)moonbit_string_literal_67.data, (moonbit_string_t)moonbit_string_literal_20.data);
  #line 158 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1877
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, (moonbit_string_t)moonbit_string_literal_68.data, (moonbit_string_t)moonbit_string_literal_22.data);
  #line 159 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1878
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, (moonbit_string_t)moonbit_string_literal_68.data, (moonbit_string_t)moonbit_string_literal_24.data);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1879
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, (moonbit_string_t)moonbit_string_literal_68.data, (moonbit_string_t)moonbit_string_literal_26.data);
  #line 161 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1880
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, (moonbit_string_t)moonbit_string_literal_69.data, (moonbit_string_t)moonbit_string_literal_24.data);
  #line 162 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1881
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, (moonbit_string_t)moonbit_string_literal_69.data, (moonbit_string_t)moonbit_string_literal_28.data);
  #line 163 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1882
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4sadd(_M0L2dbS1674, (moonbit_string_t)moonbit_string_literal_70.data, (moonbit_string_t)moonbit_string_literal_26.data);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L8hardwareS1742
  = _M0MP38JIA2JIA29moonbitdb3lib8Database8smembers(_M0L2dbS1674, (moonbit_string_t)moonbit_string_literal_68.data);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1743
  = _M0MPB13StringBuilder21StringBuilder_2einner(27);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1743, (moonbit_string_t)moonbit_string_literal_71.data);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1884 = _M0MPC15array5Array6lengthGsE(_M0L8hardwareS1742);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1743, _M0L6_2atmpS1884);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1883
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1743);
  moonbit_decref(_M0L18_2astring__builderS1743);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1883);
  moonbit_decref(_M0L6_2atmpS1883);
  _M0L7_2abindS1744 = _M0L8hardwareS1742->$1;
  _M0L2__S1745 = 0;
  while (1) {
    if (_M0L2__S1745 < _M0L7_2abindS1744) {
      moonbit_string_t* _M0L3bufS1889 = _M0L8hardwareS1742->$0;
      moonbit_string_t _M0L3pidS1746 =
        (moonbit_string_t)_M0L3bufS1889[_M0L2__S1745];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1748;
      moonbit_string_t _M0L6_2atmpS1887;
      moonbit_string_t _M0L6_2atmpS1886;
      moonbit_string_t _M0L4nameS1747;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1749;
      moonbit_string_t _M0L6_2atmpS1885;
      int32_t _M0L6_2atmpS1888;
      moonbit_incref(_M0L3pidS1746);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1748
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1748, (moonbit_string_t)moonbit_string_literal_32.data);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1748, _M0L3pidS1746);
      moonbit_decref(_M0L3pidS1746);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1887
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1748);
      moonbit_decref(_M0L18_2astring__builderS1748);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1886
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1674, _M0L6_2atmpS1887, (moonbit_string_t)moonbit_string_literal_33.data);
      moonbit_decref(_M0L6_2atmpS1887);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS1747
      = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(_M0L6_2atmpS1886);
      if (_M0L6_2atmpS1886) {
        moonbit_decref(_M0L6_2atmpS1886);
      }
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1749
      = _M0MPB13StringBuilder21StringBuilder_2einner(6);
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1749, (moonbit_string_t)moonbit_string_literal_61.data);
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1749, _M0L4nameS1747);
      moonbit_decref(_M0L4nameS1747);
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1885
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1749);
      moonbit_decref(_M0L18_2astring__builderS1749);
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1885);
      moonbit_decref(_M0L6_2atmpS1885);
      _M0L6_2atmpS1888 = _M0L2__S1745 + 1;
      _M0L2__S1745 = _M0L6_2atmpS1888;
      continue;
    } else {
      moonbit_decref(_M0L8hardwareS1742);
    }
    break;
  }
  _M0L6_2atmpS1932 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1932[0] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS1932[1] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS1931
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1931)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS1931->$0 = _M0L6_2atmpS1932;
  _M0L6_2atmpS1931->$1 = 2;
  #line 170 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L15hw__and__periphS1751
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6sinter(_M0L2dbS1674, _M0L6_2atmpS1931);
  moonbit_decref(_M0L6_2atmpS1931);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1752
  = _M0MPB13StringBuilder21StringBuilder_2einner(36);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1752, (moonbit_string_t)moonbit_string_literal_72.data);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1891
  = _M0MPC15array5Array6lengthGsE(_M0L15hw__and__periphS1751);
  moonbit_decref(_M0L15hw__and__periphS1751);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1752, _M0L6_2atmpS1891);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1752, (moonbit_string_t)moonbit_string_literal_65.data);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1890
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1752);
  moonbit_decref(_M0L18_2astring__builderS1752);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1890);
  moonbit_decref(_M0L6_2atmpS1890);
  _M0L6_2atmpS1930 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS1930[0] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS1930[1] = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS1930[2] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS1929
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1929)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS1929->$0 = _M0L6_2atmpS1930;
  _M0L6_2atmpS1929->$1 = 3;
  #line 172 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L9all__tagsS1753
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6sunion(_M0L2dbS1674, _M0L6_2atmpS1929);
  moonbit_decref(_M0L6_2atmpS1929);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1754
  = _M0MPB13StringBuilder21StringBuilder_2einner(34);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1754, (moonbit_string_t)moonbit_string_literal_73.data);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1893 = _M0MPC15array5Array6lengthGsE(_M0L9all__tagsS1753);
  moonbit_decref(_M0L9all__tagsS1753);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1754, _M0L6_2atmpS1893);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1754, (moonbit_string_t)moonbit_string_literal_65.data);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1892
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1754);
  moonbit_decref(_M0L18_2astring__builderS1754);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1892);
  moonbit_decref(_M0L6_2atmpS1892);
  #line 175 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_74.data);
  #line 176 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  _M0L10sales__keyS1755 = (moonbit_string_t)moonbit_string_literal_75.data;
  #line 178 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1894
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1674, _M0L10sales__keyS1755, 0x1.2cp+7f, (moonbit_string_t)moonbit_string_literal_20.data);
  #line 179 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1895
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1674, _M0L10sales__keyS1755, 0x1.4p+6f, (moonbit_string_t)moonbit_string_literal_22.data);
  #line 180 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1896
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1674, _M0L10sales__keyS1755, 0x1.4p+8f, (moonbit_string_t)moonbit_string_literal_24.data);
  #line 181 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1897
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1674, _M0L10sales__keyS1755, 0x1.68p+5f, (moonbit_string_t)moonbit_string_literal_26.data);
  #line 182 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1898
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4zadd(_M0L2dbS1674, _M0L10sales__keyS1755, 0x1.f4p+8f, (moonbit_string_t)moonbit_string_literal_28.data);
  #line 183 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_76.data);
  #line 184 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L4top3S1756
  = _M0MP38JIA2JIA29moonbitdb3lib8Database9zrevrange(_M0L2dbS1674, _M0L10sales__keyS1755, 0, 2);
  _M0L1iS1757 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1899;
    #line 185 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1899 = _M0MPC15array5Array6lengthGsE(_M0L4top3S1756);
    if (_M0L1iS1757 < _M0L6_2atmpS1899) {
      moonbit_string_t _M0L3pidS1758;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1760;
      moonbit_string_t _M0L6_2atmpS1903;
      moonbit_string_t _M0L6_2atmpS1902;
      moonbit_string_t _M0L4nameS1759;
      void* _M0L6_2atmpS1901;
      moonbit_string_t _M0L5salesS1761;
      int32_t _M0L1rS1764;
      int32_t _M0L4rankS1762;
      int64_t _M0L7_2abindS1765;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1768;
      moonbit_string_t _M0L6_2atmpS1900;
      int32_t _M0L6_2atmpS1904;
      #line 186 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L3pidS1758 = _M0MPC15array5Array2atGsE(_M0L4top3S1756, _M0L1iS1757);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1760
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1760, (moonbit_string_t)moonbit_string_literal_32.data);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1760, _M0L3pidS1758);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1903
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1760);
      moonbit_decref(_M0L18_2astring__builderS1760);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1902
      = _M0MP38JIA2JIA29moonbitdb3lib8Database4hget(_M0L2dbS1674, _M0L6_2atmpS1903, (moonbit_string_t)moonbit_string_literal_33.data);
      moonbit_decref(_M0L6_2atmpS1903);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS1759
      = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(_M0L6_2atmpS1902);
      if (_M0L6_2atmpS1902) {
        moonbit_decref(_M0L6_2atmpS1902);
      }
      #line 188 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1901
      = _M0MP38JIA2JIA29moonbitdb3lib8Database6zscore(_M0L2dbS1674, _M0L10sales__keyS1755, _M0L3pidS1758);
      #line 188 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L5salesS1761
      = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart16show__opt__float(_M0L6_2atmpS1901);
      moonbit_decref(_M0L6_2atmpS1901);
      #line 189 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L7_2abindS1765
      = _M0MP38JIA2JIA29moonbitdb3lib8Database8zrevrank(_M0L2dbS1674, _M0L10sales__keyS1755, _M0L3pidS1758);
      moonbit_decref(_M0L3pidS1758);
      if (_M0L7_2abindS1765 == 4294967296ll) {
        _M0L4rankS1762 = 0;
      } else {
        int64_t _M0L7_2aSomeS1766 = _M0L7_2abindS1765;
        int32_t _M0L4_2arS1767 = (int32_t)_M0L7_2aSomeS1766;
        _M0L1rS1764 = _M0L4_2arS1767;
        goto join_1763;
      }
      goto joinlet_4167;
      join_1763:;
      _M0L4rankS1762 = _M0L1rS1764 + 1;
      joinlet_4167:;
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1768
      = _M0MPB13StringBuilder21StringBuilder_2einner(19);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1768, (moonbit_string_t)moonbit_string_literal_77.data);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1768, _M0L4rankS1762);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1768, (moonbit_string_t)moonbit_string_literal_78.data);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1768, _M0L4nameS1759);
      moonbit_decref(_M0L4nameS1759);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1768, (moonbit_string_t)moonbit_string_literal_79.data);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1768, _M0L5salesS1761);
      moonbit_decref(_M0L5salesS1761);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1768, (moonbit_string_t)moonbit_string_literal_65.data);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1900
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1768);
      moonbit_decref(_M0L18_2astring__builderS1768);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1900);
      moonbit_decref(_M0L6_2atmpS1900);
      _M0L6_2atmpS1904 = _M0L1iS1757 + 1;
      _M0L1iS1757 = _M0L6_2atmpS1904;
      continue;
    } else {
      moonbit_decref(_M0L4top3S1756);
    }
    break;
  }
  #line 195 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L10mid__salesS1770
  = _M0MP38JIA2JIA29moonbitdb3lib8Database13zrangebyscore(_M0L2dbS1674, _M0L10sales__keyS1755, 0x1.9p+6f, 0x1.2cp+8f);
  moonbit_decref(_M0L10sales__keyS1755);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1771
  = _M0MPB13StringBuilder21StringBuilder_2einner(39);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1771, (moonbit_string_t)moonbit_string_literal_80.data);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1906 = _M0MPC15array5Array6lengthGsE(_M0L10mid__salesS1770);
  moonbit_decref(_M0L10mid__salesS1770);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1771, _M0L6_2atmpS1906);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1771, (moonbit_string_t)moonbit_string_literal_65.data);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1905
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1771);
  moonbit_decref(_M0L18_2astring__builderS1771);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1905);
  moonbit_decref(_M0L6_2atmpS1905);
  #line 198 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_81.data);
  #line 199 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  _M0L11session__idS1772 = (moonbit_string_t)moonbit_string_literal_82.data;
  #line 201 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database3set(_M0L2dbS1674, _M0L11session__idS1772, (moonbit_string_t)moonbit_string_literal_83.data);
  #line 202 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1907
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6expire(_M0L2dbS1674, _M0L11session__idS1772, 1800);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1773
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1773, (moonbit_string_t)moonbit_string_literal_84.data);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1773, _M0L11session__idS1772);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1908
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1773);
  moonbit_decref(_M0L18_2astring__builderS1773);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1908);
  moonbit_decref(_M0L6_2atmpS1908);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1774
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1774, (moonbit_string_t)moonbit_string_literal_85.data);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1911
  = _M0MP38JIA2JIA29moonbitdb3lib8Database3get(_M0L2dbS1674, _M0L11session__idS1772);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1910
  = _M0FP48JIA2JIA29moonbitdb8examples14shopping__cart9show__opt(_M0L6_2atmpS1911);
  if (_M0L6_2atmpS1911) {
    moonbit_decref(_M0L6_2atmpS1911);
  }
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1774, _M0L6_2atmpS1910);
  moonbit_decref(_M0L6_2atmpS1910);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1909
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1774);
  moonbit_decref(_M0L18_2astring__builderS1774);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1909);
  moonbit_decref(_M0L6_2atmpS1909);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1775
  = _M0MPB13StringBuilder21StringBuilder_2einner(20);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1775, (moonbit_string_t)moonbit_string_literal_86.data);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1913
  = _M0MP38JIA2JIA29moonbitdb3lib8Database3ttl(_M0L2dbS1674, _M0L11session__idS1772);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1775, _M0L6_2atmpS1913);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1775, (moonbit_string_t)moonbit_string_literal_87.data);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1912
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1775);
  moonbit_decref(_M0L18_2astring__builderS1775);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1912);
  moonbit_decref(_M0L6_2atmpS1912);
  #line 206 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(_M0L2dbS1674, 1000000);
  #line 207 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_88.data);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1776
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1776, (moonbit_string_t)moonbit_string_literal_89.data);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1915
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6exists(_M0L2dbS1674, _M0L11session__idS1772);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1776, _M0L6_2atmpS1915);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1914
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1776);
  moonbit_decref(_M0L18_2astring__builderS1776);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1914);
  moonbit_decref(_M0L6_2atmpS1914);
  #line 209 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database13advance__time(_M0L2dbS1674, 1000000);
  #line 210 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_90.data);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1777
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1777, (moonbit_string_t)moonbit_string_literal_89.data);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1917
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6exists(_M0L2dbS1674, _M0L11session__idS1772);
  moonbit_decref(_M0L11session__idS1772);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1777, _M0L6_2atmpS1917);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1916
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1777);
  moonbit_decref(_M0L18_2astring__builderS1777);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1916);
  moonbit_decref(_M0L6_2atmpS1916);
  #line 213 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_91.data);
  #line 214 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  #line 215 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MP38JIA2JIA29moonbitdb3lib8Database7flushdb(_M0L2dbS1674);
  _M0L6_2atmpS1928 = (moonbit_string_t*)moonbit_make_ref_array_raw(5);
  _M0L6_2atmpS1928[0] = (moonbit_string_t)moonbit_string_literal_92.data;
  _M0L6_2atmpS1928[1] = (moonbit_string_t)moonbit_string_literal_93.data;
  _M0L6_2atmpS1928[2] = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L6_2atmpS1928[3] = (moonbit_string_t)moonbit_string_literal_95.data;
  _M0L6_2atmpS1928[4] = (moonbit_string_t)moonbit_string_literal_96.data;
  _M0L9hot__keysS1778
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9hot__keysS1778)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L9hot__keysS1778->$0 = _M0L6_2atmpS1928;
  _M0L9hot__keysS1778->$1 = 5;
  _M0L6_2atmpS1927 = (moonbit_string_t*)moonbit_make_ref_array_raw(5);
  _M0L6_2atmpS1927[0] = (moonbit_string_t)moonbit_string_literal_97.data;
  _M0L6_2atmpS1927[1] = (moonbit_string_t)moonbit_string_literal_98.data;
  _M0L6_2atmpS1927[2] = (moonbit_string_t)moonbit_string_literal_99.data;
  _M0L6_2atmpS1927[3] = (moonbit_string_t)moonbit_string_literal_100.data;
  _M0L6_2atmpS1927[4] = (moonbit_string_t)moonbit_string_literal_101.data;
  _M0L9hot__valsS1779
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9hot__valsS1779)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L9hot__valsS1779->$0 = _M0L6_2atmpS1927;
  _M0L9hot__valsS1779->$1 = 5;
  #line 218 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1918
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4mset(_M0L2dbS1674, _M0L9hot__keysS1778, _M0L9hot__valsS1779);
  moonbit_decref(_M0L9hot__valsS1779);
  #line 219 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_102.data);
  #line 220 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L13batch__resultS1780
  = _M0MP38JIA2JIA29moonbitdb3lib8Database4mget(_M0L2dbS1674, _M0L9hot__keysS1778);
  #line 221 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_103.data);
  _M0L1iS1781 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1919;
    #line 222 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS1919
    = _M0MPC15array5Array6lengthGOsE(_M0L13batch__resultS1780);
    if (_M0L1iS1781 < _M0L6_2atmpS1919) {
      moonbit_string_t _M0L1vS1783;
      moonbit_string_t _M0L7_2abindS1785;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1784;
      moonbit_string_t _M0L6_2atmpS1921;
      moonbit_string_t _M0L6_2atmpS1920;
      int32_t _M0L6_2atmpS1924;
      #line 223 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L7_2abindS1785
      = _M0MPC15array5Array2atGOsE(_M0L13batch__resultS1780, _M0L1iS1781);
      if (_M0L7_2abindS1785 == 0) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1788;
        moonbit_string_t _M0L6_2atmpS1923;
        moonbit_string_t _M0L6_2atmpS1922;
        if (_M0L7_2abindS1785) {
          moonbit_decref(_M0L7_2abindS1785);
        }
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0L18_2astring__builderS1788
        = _M0MPB13StringBuilder21StringBuilder_2einner(12);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1788, (moonbit_string_t)moonbit_string_literal_46.data);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0L6_2atmpS1923
        = _M0MPC15array5Array2atGsE(_M0L9hot__keysS1778, _M0L1iS1781);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1788, _M0L6_2atmpS1923);
        moonbit_decref(_M0L6_2atmpS1923);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1788, (moonbit_string_t)moonbit_string_literal_104.data);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0L6_2atmpS1922
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1788);
        moonbit_decref(_M0L18_2astring__builderS1788);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0FPB7printlnGsE(_M0L6_2atmpS1922);
        moonbit_decref(_M0L6_2atmpS1922);
      } else {
        moonbit_string_t _M0L7_2aSomeS1786 = _M0L7_2abindS1785;
        moonbit_string_t _M0L4_2avS1787 = _M0L7_2aSomeS1786;
        _M0L1vS1783 = _M0L4_2avS1787;
        goto join_1782;
      }
      goto joinlet_4169;
      join_1782:;
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1784
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1784, (moonbit_string_t)moonbit_string_literal_46.data);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1921
      = _M0MPC15array5Array2atGsE(_M0L9hot__keysS1778, _M0L1iS1781);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1784, _M0L6_2atmpS1921);
      moonbit_decref(_M0L6_2atmpS1921);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1784, (moonbit_string_t)moonbit_string_literal_105.data);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1784, _M0L1vS1783);
      moonbit_decref(_M0L1vS1783);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS1920
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1784);
      moonbit_decref(_M0L18_2astring__builderS1784);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1920);
      moonbit_decref(_M0L6_2atmpS1920);
      joinlet_4169:;
      _M0L6_2atmpS1924 = _M0L1iS1781 + 1;
      _M0L1iS1781 = _M0L6_2atmpS1924;
      continue;
    } else {
      moonbit_decref(_M0L13batch__resultS1780);
      moonbit_decref(_M0L9hot__keysS1778);
    }
    break;
  }
  #line 229 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_106.data);
  #line 230 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_31.data);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1790
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1790, (moonbit_string_t)moonbit_string_literal_107.data);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1926
  = _M0MP38JIA2JIA29moonbitdb3lib8Database6dbsize(_M0L2dbS1674);
  moonbit_decref(_M0L2dbS1674);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1790, _M0L6_2atmpS1926);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS1925
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1790);
  moonbit_decref(_M0L18_2astring__builderS1790);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1925);
  moonbit_decref(_M0L6_2atmpS1925);
  #line 233 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_108.data);
  #line 234 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_109.data);
  #line 235 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_18.data);
  return 0;
}