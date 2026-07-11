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
struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE;

struct _M0TPB9ArrayViewGUsfEE;

struct _M0TWEOUssE;

struct _M0TUsiE;

struct _M0TPB3MapGsfE;

struct _M0TUsbE;

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsiEE;

struct _M0TPB17FloatingDecimal64;

struct _M0TPB5EntryGssE;

struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE;

struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE;

struct _M0TUssE;

struct _M0TP39moonbitdb9moonbitdb3lib8Database;

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__;

struct _M0BTPB6Logger;

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List;

struct _M0TPB9ArrayViewGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE;

struct _M0TPB6Logger;

struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE;

struct _M0TUsfE;

struct _M0TP39moonbitdb9moonbitdb3lib5Deque;

struct _M0TPB8MutLocalGORPB5EntryGsfEE;

struct _M0TPB5EntryGsbE;

struct _M0TPB8MutLocalGORPB5EntryGssEE;

struct _M0DTPC16option6OptionGfE4Some;

struct _M0TPB19MulShiftAll64Result;

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__;

struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE;

struct _M0TPB5ArrayGOsE;

struct _M0TWEOUsbE;

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set;

struct _M0TPB5EntryGsfE;

struct _M0TPB8MutLocalGiE;

struct _M0TPB5ArrayGUsfEE;

struct _M0TPB3MapGssE;

struct _M0TPB4Show;

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__;

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String;

struct _M0BTPB4Show;

struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__;

struct _M0TPC16string10StringView;

struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB8MutLocalGORPB5EntryGsbEE;

struct _M0TPB3MapGsbE;

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet;

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

struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE {
  int32_t $0;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  void* $5;
  
};

struct _M0TPB9ArrayViewGUsfEE {
  struct _M0TUsfE** $0;
  int32_t $1;
  int32_t $2;
  
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

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash {
  struct _M0TPB3MapGssE* $0;
  
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

struct _M0TPB5EntryGssE {
  int32_t $0;
  struct _M0TPB5EntryGssE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  moonbit_string_t $5;
  
};

struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE {
  struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE* $0;
  int64_t $1;
  
};

struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE {
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* $5;
  int32_t $6;
  
};

struct _M0TUssE {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0TP39moonbitdb9moonbitdb3lib8Database {
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* $0;
  struct _M0TPB3MapGsiE* $1;
  int32_t $2;
  
};

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
};

struct _M0BTPB6Logger {
  int32_t(* $method_0)(void*, moonbit_string_t);
  int32_t(* $method_1)(void*, moonbit_string_t, int32_t, int32_t);
  int32_t(* $method_2)(void*, struct _M0TPC16string10StringView);
  int32_t(* $method_3)(void*, int32_t);
  int32_t(* $method_4)(void*, struct _M0TPB4Show);
  int32_t(* $method_5)(void*, struct _M0TPB4Show);
  
};

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List {
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* $0;
  
};

struct _M0TPB9ArrayViewGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE {
  struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE** $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE {
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* $0;
  
};

struct _M0TUsfE {
  moonbit_string_t $0;
  float $1;
  
};

struct _M0TP39moonbitdb9moonbitdb3lib5Deque {
  struct _M0TPB5ArrayGsE* $0;
  struct _M0TPB5ArrayGsE* $1;
  
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

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
};

struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE {
  moonbit_string_t $0;
  void* $1;
  
};

struct _M0TPB5ArrayGOsE {
  moonbit_string_t* $0;
  int32_t $1;
  
};

struct _M0TWEOUsbE {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  
};

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set {
  struct _M0TPB3MapGsbE* $0;
  
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

struct _M0TPB4IterGUsfEE {
  struct _M0TWEOUsfE* $0;
  int64_t $1;
  
};

struct _M0TWEOUsfE {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  
};

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__ {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* $1;
  
};

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String {
  moonbit_string_t $0;
  
};

struct _M0BTPB4Show {
  int32_t(* $method_0)(void*, struct _M0TPB6Logger);
  moonbit_string_t(* $method_1)(void*);
  
};

struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__ {
  struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE*(* code)(
    struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE* $1;
  
};

struct _M0TPC16string10StringView {
  moonbit_string_t $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE {
  struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE*(* code)(
    struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE*
  );
  
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

struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet {
  struct _M0TPB3MapGsfE* $0;
  
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

moonbit_string_t _M0FP49moonbitdb9moonbitdb8examples12basic__usage16show__opt__float(
  void*
);

moonbit_string_t _M0FP49moonbitdb9moonbitdb8examples12basic__usage14show__opt__int(
  int64_t
);

moonbit_string_t _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(
  moonbit_string_t
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4echo(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4ping();

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6dbsize(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*
);

int64_t _M0MP39moonbitdb9moonbitdb3lib8Database5zrank(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5ArrayGUsfEE* _M0MP39moonbitdb9moonbitdb3lib8Database17get__sorted__zset(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

void* _M0MP39moonbitdb9moonbitdb3lib8Database6zscore(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database5zcard(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP39moonbitdb9moonbitdb3lib8Database6zrange(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPB5ArrayGUsfEE* _M0FP39moonbitdb9moonbitdb3lib15sort__by__score(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPB5ArrayGUsfEE* _M0FP39moonbitdb9moonbitdb3lib11merge__sort(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPB5ArrayGUsfEE* _M0FP39moonbitdb9moonbitdb3lib5merge(
  struct _M0TPB5ArrayGUsfEE*,
  struct _M0TPB5ArrayGUsfEE*
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4zadd(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  float,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database9sismember(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database5scard(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP39moonbitdb9moonbitdb3lib8Database8smembers(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4sadd(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database6lindex(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0MP39moonbitdb9moonbitdb3lib8Database6lrange(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4llen(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4rpop(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4lpop(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database5rpush(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4hlen(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

struct _M0TPB3MapGssE* _M0MP39moonbitdb9moonbitdb3lib8Database7hgetall(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4hget(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4hset(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t
);

int64_t _M0MP39moonbitdb9moonbitdb3lib8Database4decr(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

int64_t _M0MP39moonbitdb9moonbitdb3lib8Database4incr(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0FP39moonbitdb9moonbitdb3lib15int__to__string(int32_t);

int64_t _M0FP39moonbitdb9moonbitdb3lib10parse__int(moonbit_string_t);

int64_t _M0FP39moonbitdb9moonbitdb3lib17uint16__to__digit(int32_t);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6strlen(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6append(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database8type__of(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6exists(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database3get(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4mdel(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB5ArrayGOsE* _M0MP39moonbitdb9moonbitdb3lib8Database4mget(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4mset(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB5ArrayGsE*
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database3ttl(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6expire(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database3set(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  moonbit_string_t
);

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database13advance__time(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database*,
  int32_t
);

struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0MP39moonbitdb9moonbitdb3lib8Database3new(
  
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib5Deque7get__at(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque*,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0MP39moonbitdb9moonbitdb3lib5Deque9to__array(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque*
);

int32_t _M0MP39moonbitdb9moonbitdb3lib5Deque6length(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque*
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib5Deque9pop__back(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque*
);

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib5Deque10pop__front(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque*
);

int32_t _M0MP39moonbitdb9moonbitdb3lib5Deque10push__back(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque*,
  moonbit_string_t
);

struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0MP39moonbitdb9moonbitdb3lib5Deque3new(
  
);

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float);

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC15array5Array3popGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE*,
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

struct _M0TUssE* _M0MPB5Iter24nextGssE(struct _M0TPB4IterGUssEE*);

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(struct _M0TPB4IterGUsbEE*);

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(struct _M0TPB4IterGUsfEE*);

struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE*
);

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*
);

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*
);

struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP39moonbitdb9moonbitdb3lib10RedisValueEC2867l711(
  struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE*
);

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2857l711(struct _M0TWEOUsfE*);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2847l711(struct _M0TWEOUsbE*);

struct _M0TUssE* _M0MPB3Map4iterGssEC2837l711(struct _M0TWEOUssE*);

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map18remove__with__hashGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map11shift__backGsiE(struct _M0TPB3MapGsiE*, int32_t);

int32_t _M0MPB3Map11shift__backGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map8containsGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map8containsGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

void* _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  moonbit_string_t
);

moonbit_string_t _M0MPB3Map3getGssE(struct _M0TPB3MapGssE*, moonbit_string_t);

void* _M0MPB3Map3getGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

int64_t _M0MPB3Map3getGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPB3Map3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE,
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

int32_t _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
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

int32_t _M0MPB3Map15set__with__hashGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
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

int32_t _M0MPB3Map4growGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*
);

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE*);

int32_t _M0MPB3Map20rehash__place__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*
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

int32_t _M0MPB3Map10push__awayGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  int32_t,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*
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

int32_t _M0MPB3Map10set__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
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

int32_t _M0MPB3Map20add__entry__to__tailGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  int32_t,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*
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

struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0FPB8new__mapGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  int32_t
);

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t);

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t);

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t);

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

int32_t _M0MPC16option6Option6unwrapGiE(int64_t);

struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*
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

int32_t _M0MPC15array5Array6lengthGOsE(struct _M0TPB5ArrayGOsE*);

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

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

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(struct _M0TWEOUsfE*, int64_t);

struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE*,
  int64_t
);

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

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(struct _M0TPB4IterGUsfEE*);

struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE*
);

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

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_40 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 71, 69, 
    84, 32, 110, 97, 109, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_15 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 111, 
    110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_14 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 0, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_10 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 55, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_66 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 76, 80, 
    79, 80, 32, 116, 97, 115, 107, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_57 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 44, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_84 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 90, 82, 
    65, 78, 75, 32, 112, 108, 97, 121, 101, 114, 51, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 108, 101, 
    97, 100, 101, 114, 98, 111, 97, 114, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_27 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 114, 97, 
    100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 98, 101, 
    116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_23 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 110, 
    102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_22 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 78, 97, 78, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_5 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_81 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 112, 108, 
    97, 121, 101, 114, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_54 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 72, 71, 
    69, 84, 32, 117, 115, 101, 114, 58, 49, 32, 117, 115, 101, 114, 110, 
    97, 109, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_2 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 79, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_101 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 77, 71, 
    69, 84, 32, 107, 49, 44, 107, 50, 44, 107, 51, 44, 107, 52, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_82 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 90, 67, 
    65, 82, 68, 32, 108, 101, 97, 100, 101, 114, 98, 111, 97, 114, 100, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_4 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_3 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_109 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 72, 101, 
    108, 108, 111, 32, 77, 111, 111, 110, 66, 105, 116, 68, 66, 33, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_92 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 51, 53, 
    31186, 21518, 32, 69, 88, 73, 83, 84, 83, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_64 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 76, 76, 
    69, 78, 32, 116, 97, 115, 107, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_33 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 44, 32, 
    108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_26 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 102, 97, 
    108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_106 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 68, 66, 
    83, 73, 90, 69, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_41 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 71, 69, 
    84, 32, 97, 103, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_30 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 98, 111, 
    117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 97, 105, 
    108, 101, 100, 58, 32, 97, 108, 108, 111, 99, 97, 116, 101, 95, 108, 
    101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_83 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 83, 
    67, 79, 82, 69, 32, 112, 108, 97, 121, 101, 114, 50, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_50 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 117, 115, 
    101, 114, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_112 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 84, 89, 
    80, 69, 32, 116, 97, 115, 107, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_110 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 61, 
    61, 61, 32, 84, 89, 80, 69, 32, 26816, 26597, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_73 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 109, 111, 
    111, 110, 98, 105, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_72 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 112, 114, 
    111, 103, 114, 97, 109, 109, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_63 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 84, 97, 
    115, 107, 32, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_61 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 84, 97, 
    115, 107, 32, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_114 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 84, 89, 
    80, 69, 32, 110, 111, 110, 101, 120, 105, 115, 116, 101, 110, 116, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_102 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 32, 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_19 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 115, 101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_77 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 10, 61, 
    61, 61, 32, 83, 111, 114, 116, 101, 100, 32, 83, 101, 116, 32, 25805,
    20316, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_9 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_7 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_28 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 
    118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_49 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 117, 115, 
    101, 114, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_100 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 107, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_68 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 76, 73, 
    78, 68, 69, 88, 32, 116, 97, 115, 107, 115, 32, 48, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_34 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_90 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 115, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_87 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 115, 101, 
    115, 115, 105, 111, 110, 58, 117, 115, 101, 114, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_43 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 65, 80, 
    80, 69, 78, 68, 32, 110, 97, 109, 101, 32, 39, 32, 83, 109, 105, 
    116, 104, 39, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_39 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 50, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_80 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 112, 108, 
    97, 121, 101, 114, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_75 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 32, 109, 111, 111, 110, 98, 105, 116, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_42 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 83, 84, 
    82, 76, 69, 78, 32, 110, 97, 109, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_31 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_37 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 65, 108, 
    105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_16 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 115, 116, 
    114, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_60 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 116, 97, 
    115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_59 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 61, 
    61, 61, 32, 76, 105, 115, 116, 32, 25805, 20316, 32, 61, 61, 61, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_52 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 101, 109, 
    97, 105, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_48 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 61, 
    61, 61, 32, 72, 97, 115, 104, 32, 25805, 20316, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_21 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 73, 108, 
    108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 116, 69, 
    120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_44 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 83, 
    109, 105, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_53 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 97, 108, 
    105, 99, 101, 64, 101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 
    109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_108 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 67, 
    72, 79, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_97 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 118, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_91 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 50, 48, 
    31186, 21518, 32, 84, 84, 76, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_32 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    100, 115, 116, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_29 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 105, 110, 
    118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 111, 105, 
    110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_13 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_51 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 97, 108, 
    105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_45 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 23, 71, 69, 
    84, 32, 110, 97, 109, 101, 32, 97, 102, 116, 101, 114, 32, 97, 112, 
    112, 101, 110, 100, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_8 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_35 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 61, 61, 
    61, 32, 83, 116, 114, 105, 110, 103, 32, 25805, 20316, 32, 61, 61, 
    61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 122, 115, 
    101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_95 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 107, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_89 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 84, 84, 
    76, 32, 115, 101, 115, 115, 105, 111, 110, 58, 117, 115, 101, 114, 
    49, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_56 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 72, 71, 
    69, 84, 65, 76, 76, 32, 102, 105, 101, 108, 100, 115, 58, 32, 91, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_103 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 93, 32, 
    61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_107 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 80, 73, 
    78, 71, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_86 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 10, 61, 
    61, 61, 32, 75, 101, 121, 32, 36807, 26399, 26426, 21046, 32, 61, 
    61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_12 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 57, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_65 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 76, 82, 
    65, 78, 71, 69, 32, 116, 97, 115, 107, 115, 32, 48, 32, 45, 49, 58, 
    32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_55 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 72, 76, 
    69, 78, 32, 117, 115, 101, 114, 58, 49, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_111 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 84, 89, 
    80, 69, 32, 117, 115, 101, 114, 58, 49, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_62 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 84, 97, 
    115, 107, 32, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_76 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 77, 
    69, 77, 66, 69, 82, 83, 58, 32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_115 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 110, 111, 
    110, 101, 120, 105, 115, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_98 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 118, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_105 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 10, 61, 
    61, 61, 32, 26381, 21153, 22120, 20449, 24687, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_96 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 107, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 82, 80, 
    79, 80, 32, 116, 97, 115, 107, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_36 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 97, 
    109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_58 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_38 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 97, 103, 101, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_74 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 83, 67, 
    65, 82, 68, 32, 116, 97, 103, 115, 58, 112, 111, 115, 116, 58, 49, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_70 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 116, 97, 
    103, 115, 58, 112, 111, 115, 116, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_25 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 114, 
    117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_94 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 107, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_93 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 10, 61, 
    61, 61, 32, 25209, 37327, 25805, 20316, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_47 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 68, 69, 
    67, 82, 32, 97, 103, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_71 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 101, 
    99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_1 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 40, 110, 
    105, 108, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_46 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 73, 78, 
    67, 82, 32, 97, 103, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_11 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_6 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_113 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 84, 89, 
    80, 69, 32, 108, 101, 97, 100, 101, 114, 98, 111, 97, 114, 100, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_85 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 90, 82, 
    65, 78, 71, 69, 32, 48, 32, 50, 58, 32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 108, 105, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_116 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 10, 9989, 
    32, 22522, 26412, 20351, 29992, 31034, 20363, 36816, 34892, 23436, 
    25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_79 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 112, 108, 
    97, 121, 101, 114, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_104 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 77, 68, 
    69, 76, 32, 21024, 38500, 25968, 37327, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_17 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 104, 97, 
    115, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_99 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 118, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_69 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 10, 61, 
    61, 61, 32, 83, 101, 116, 32, 25805, 20316, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_24 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 48, 46, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_88 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 97, 98, 
    99, 49, 50, 51, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0)
  };

uint32_t const moonbit_layout_table_data[131] =
  {
    sizeof(struct _M0TPB5ArrayGUsfEE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGUsfEE, $0) / 4, sizeof(struct _M0TUsfE) / 4,
    1, offsetof(struct _M0TUsfE, $0) / 4, sizeof(struct _M0TPB5ArrayGsE) / 4,
    1, offsetof(struct _M0TPB5ArrayGsE, $0) / 4,
    sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet) / 4, 
    1,
    offsetof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet, $0) / 4,
    sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set) / 4, 
    1,
    offsetof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set, $0) / 4,
    sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List) / 4, 
    1,
    offsetof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List, $0) / 4,
    sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash) / 4, 
    1,
    offsetof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash, $0) / 4,
    sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String) / 4, 
    1,
    offsetof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String, $0)
    / 4, sizeof(struct _M0TPB5ArrayGOsE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGOsE, $0) / 4,
    sizeof(struct _M0TP39moonbitdb9moonbitdb3lib8Database) / 4, 2,
    offsetof(struct _M0TP39moonbitdb9moonbitdb3lib8Database, $0) / 4,
    offsetof(struct _M0TP39moonbitdb9moonbitdb3lib8Database, $1) / 4,
    sizeof(struct _M0TP39moonbitdb9moonbitdb3lib5Deque) / 4, 2,
    offsetof(struct _M0TP39moonbitdb9moonbitdb3lib5Deque, $0) / 4,
    offsetof(struct _M0TP39moonbitdb9moonbitdb3lib5Deque, $1) / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGssEE, $0) / 4,
    sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__)
    / 4, 2,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__, $0)
    / 4,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__)
    / 4, 2,
    offsetof(struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__, $0)
    / 4,
    offsetof(struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__, $1)
    / 4, sizeof(struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE) / 4,
    2,
    offsetof(struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE, $0) / 4,
    offsetof(struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE, $1) / 4,
    sizeof(struct _M0TUsbE) / 4, 1, offsetof(struct _M0TUsbE, $0) / 4,
    sizeof(struct _M0TUssE) / 4, 2, offsetof(struct _M0TUssE, $0) / 4,
    offsetof(struct _M0TUssE, $1) / 4,
    sizeof(struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE) / 4,
    3,
    offsetof(struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE, $1)
    / 4,
    offsetof(struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE, $4)
    / 4,
    offsetof(struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE, $5)
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
    sizeof(struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE) / 4,
    2,
    offsetof(struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE, $0)
    / 4,
    offsetof(struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE, $5)
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
    sizeof(struct _M0TPB4IterGUsfEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsfEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE, $0)
    / 4, sizeof(struct _M0TPB13StringBuilder) / 4, 1,
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

int64_t _M0MPB4Iter4nextN6constrS9980GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsRP39moonbitdb9moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsRP39moonbitdb9moonbitdb3lib10RedisValueEE =
  0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUssEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsbEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsfEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsRP39moonbitdb9moonbitdb3lib10RedisValueEE =
  0ll;

moonbit_string_t _M0FP49moonbitdb9moonbitdb8examples12basic__usage16show__opt__float(
  void* _M0L3optS1754
) {
  float _M0L1vS1752;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1753;
  moonbit_string_t _result_3944;
  #line 15 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  switch (Moonbit_object_tag(_M0L3optS1754)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1755 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1754;
      float _M0L4_2avS1756 = _M0L7_2aSomeS1755->$0;
      _M0L1vS1752 = _M0L4_2avS1756;
      goto join_1751;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_0.data;
      break;
    }
  }
  join_1751:;
  #line 17 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1753
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 17 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1753, _M0L1vS1752);
  #line 17 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _result_3944
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1753);
  moonbit_decref(_M0L18_2astring__builderS1753);
  return _result_3944;
}

moonbit_string_t _M0FP49moonbitdb9moonbitdb8examples12basic__usage14show__opt__int(
  int64_t _M0L3optS1748
) {
  int32_t _M0L1vS1746;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1747;
  moonbit_string_t _result_3946;
  #line 8 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  if (_M0L3optS1748 == 4294967296ll) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int64_t _M0L7_2aSomeS1749 = _M0L3optS1748;
    int32_t _M0L4_2avS1750 = (int32_t)_M0L7_2aSomeS1749;
    _M0L1vS1746 = _M0L4_2avS1750;
    goto join_1745;
  }
  join_1745:;
  #line 10 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1747
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 10 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1747, _M0L1vS1746);
  #line 10 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _result_3946
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1747);
  moonbit_decref(_M0L18_2astring__builderS1747);
  return _result_3946;
}

moonbit_string_t _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(
  moonbit_string_t _M0L3optS1742
) {
  moonbit_string_t _M0L1vS1740;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1741;
  moonbit_string_t _result_3948;
  #line 1 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  if (_M0L3optS1742 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS1743 = _M0L3optS1742;
    moonbit_string_t _M0L4_2avS1744 = _M0L7_2aSomeS1743;
    moonbit_incref(_M0L4_2avS1744);
    _M0L1vS1740 = _M0L4_2avS1744;
    goto join_1739;
  }
  join_1739:;
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1741
  = _M0MPB13StringBuilder21StringBuilder_2einner(2);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1741, (moonbit_string_t)moonbit_string_literal_1.data);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1741, _M0L1vS1740);
  moonbit_decref(_M0L1vS1740);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1741, (moonbit_string_t)moonbit_string_literal_1.data);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _result_3948
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1741);
  moonbit_decref(_M0L18_2astring__builderS1741);
  return _result_3948;
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4echo(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L12_2adiscard__S1738,
  moonbit_string_t _M0L7messageS1737
) {
  #line 1560 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  moonbit_incref(_M0L7messageS1737);
  return _M0L7messageS1737;
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4ping() {
  #line 1556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  return (moonbit_string_t)moonbit_string_literal_2.data;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6dbsize(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1729
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1727;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3501;
  struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0L5_2aitS1728;
  int32_t _result_3951;
  #line 1484 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5countS1727
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1727)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1727->$0 = 0;
  _M0L4dataS3501 = _M0L4selfS1729->$0;
  moonbit_incref(_M0L4dataS3501);
  #line 1485 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5_2aitS1728
  = _M0MPB3Map5iter2GsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3501);
  moonbit_decref(_M0L4dataS3501);
  while (1) {
    moonbit_string_t _M0L3keyS1731;
    struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS1733;
    int32_t _M0L6_2atmpS3498;
    #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1733
    = _M0MPB5Iter24nextGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L5_2aitS1728);
    if (_M0L7_2abindS1733 == 0) {
      if (_M0L7_2abindS1733) {
        moonbit_decref(_M0L7_2abindS1733);
      }
      moonbit_decref(_M0L5_2aitS1728);
    } else {
      struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1734 =
        _M0L7_2abindS1733;
      struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4_2axS1735 =
        _M0L7_2aSomeS1734;
      moonbit_string_t _M0L8_2afieldS3502 = _M0L4_2axS1735->$0;
      int32_t _M0L6_2acntS3875 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1735));
      moonbit_string_t _M0L6_2akeyS1736;
      if (_M0L6_2acntS3875 > 1) {
        int32_t _M0L11_2anew__cntS3877 = _M0L6_2acntS3875 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1735), _M0L11_2anew__cntS3877);
        moonbit_incref(_M0L8_2afieldS3502);
      } else if (_M0L6_2acntS3875 == 1) {
        void* _M0L8_2afieldS3876 = _M0L4_2axS1735->$1;
        moonbit_decref(_M0L8_2afieldS3876);
        #line 1486 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_free(_M0L4_2axS1735);
      }
      _M0L6_2akeyS1736 = _M0L8_2afieldS3502;
      _M0L3keyS1731 = _M0L6_2akeyS1736;
      goto join_1730;
    }
    goto joinlet_3950;
    join_1730:;
    #line 1487 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3498
    = _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1729, _M0L3keyS1731);
    moonbit_decref(_M0L3keyS1731);
    if (!_M0L6_2atmpS3498) {
      int32_t _M0L3valS3500 = _M0L5countS1727->$0;
      int32_t _M0L6_2atmpS3499 = _M0L3valS3500 + 1;
      _M0L5countS1727->$0 = _M0L6_2atmpS3499;
    }
    continue;
    joinlet_3950:;
    break;
  }
  _result_3951 = _M0L5countS1727->$0;
  moonbit_decref(_M0L5countS1727);
  return _result_3951;
}

int64_t _M0MP39moonbitdb9moonbitdb3lib8Database5zrank(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1712,
  moonbit_string_t _M0L3keyS1713,
  moonbit_string_t _M0L11member__valS1717
) {
  #line 1328 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1329 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1712, _M0L3keyS1713)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1716;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3497 =
      _M0L4selfS1712->$0;
    void* _M0L7_2abindS1722;
    int32_t _M0L6_2atmpS3491;
    moonbit_incref(_M0L4dataS3497);
    #line 1332 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1722
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3497, _M0L3keyS1713);
    moonbit_decref(_M0L4dataS3497);
    if (_M0L7_2abindS1722 == 0) {
      if (_M0L7_2abindS1722) {
        moonbit_decref(_M0L7_2abindS1722);
      }
      goto join_1714;
    } else {
      void* _M0L7_2aSomeS1723 = _M0L7_2abindS1722;
      void* _M0L4_2axS1724 = _M0L7_2aSomeS1723;
      switch (Moonbit_object_tag(_M0L4_2axS1724)) {
        case 4: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1725 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1724;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3505 = _M0L7_2aZSetS1725->$0;
          int32_t _M0L6_2acntS3880 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1725));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1726;
          if (_M0L6_2acntS3880 > 1) {
            int32_t _M0L11_2anew__cntS3881 = _M0L6_2acntS3880 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1725), _M0L11_2anew__cntS3881);
            moonbit_incref(_M0L8_2afieldS3505);
          } else if (_M0L6_2acntS3880 == 1) {
            #line 1332 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1725);
          }
          _M0L7_2azsetS1726 = _M0L8_2afieldS3505;
          _M0L4zsetS1716 = _M0L7_2azsetS1726;
          goto join_1715;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1724);
          goto join_1714;
          break;
        }
      }
    }
    join_1715:;
    #line 1334 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3491
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1716, _M0L11member__valS1717);
    moonbit_decref(_M0L4zsetS1716);
    if (!_M0L6_2atmpS3491) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1718;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1719;
      int32_t _M0L1iS1720;
      int32_t _M0L3valS3496;
      #line 1337 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6sortedS1718
      = _M0MP39moonbitdb9moonbitdb3lib8Database17get__sorted__zset(_M0L4selfS1712, _M0L3keyS1713);
      _M0L4rankS1719
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1719)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1719->$0 = 0;
      _M0L1iS1720 = 0;
      while (1) {
        int32_t _M0L6_2atmpS3492;
        #line 1339 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3492 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1718);
        if (_M0L1iS1720 < _M0L6_2atmpS3492) {
          struct _M0TUsfE* _M0L6_2atmpS3494;
          moonbit_string_t _M0L8_2afieldS3504;
          int32_t _M0L6_2acntS3878;
          moonbit_string_t _M0L6_2atmpS3493;
          int32_t _result_3955;
          int32_t _M0L6_2atmpS3495;
          #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0L6_2atmpS3494
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1718, _M0L1iS1720);
          _M0L8_2afieldS3504 = _M0L6_2atmpS3494->$0;
          _M0L6_2acntS3878
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3494));
          if (_M0L6_2acntS3878 > 1) {
            int32_t _M0L11_2anew__cntS3879 = _M0L6_2acntS3878 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3494), _M0L11_2anew__cntS3879);
            moonbit_incref(_M0L8_2afieldS3504);
          } else if (_M0L6_2acntS3878 == 1) {
            #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2atmpS3494);
          }
          _M0L6_2atmpS3493 = _M0L8_2afieldS3504;
          #line 1340 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _result_3955
          = _M0L6_2atmpS3493 == _M0L11member__valS1717
            || Moonbit_array_length(_M0L6_2atmpS3493)
               == Moonbit_array_length(_M0L11member__valS1717)
               && 0
                  == memcmp(_M0L6_2atmpS3493, _M0L11member__valS1717, Moonbit_array_length(_M0L6_2atmpS3493) * 2);
          moonbit_decref(_M0L6_2atmpS3493);
          if (_result_3955) {
            moonbit_decref(_M0L6sortedS1718);
            _M0L4rankS1719->$0 = _M0L1iS1720;
            break;
          }
          _M0L6_2atmpS3495 = _M0L1iS1720 + 1;
          _M0L1iS1720 = _M0L6_2atmpS3495;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1718);
        }
        break;
      }
      _M0L3valS3496 = _M0L4rankS1719->$0;
      moonbit_decref(_M0L4rankS1719);
      return (int64_t)_M0L3valS3496;
    }
    join_1714:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0MP39moonbitdb9moonbitdb3lib8Database17get__sorted__zset(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1691,
  moonbit_string_t _M0L3keyS1692
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1691, _M0L3keyS1692)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS3486 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_3956 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_3956)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_3956->$0 = _M0L6_2atmpS3486;
    _block_3956->$1 = 0;
    return _block_3956;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1695;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3490 =
      _M0L4selfS1691->$0;
    void* _M0L7_2abindS1707;
    struct _M0TUsfE** _M0L6_2atmpS3489;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1696;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1697;
    struct _M0TPB5ArrayGUsfEE* _result_3961;
    struct _M0TUsfE** _M0L6_2atmpS3487;
    struct _M0TPB5ArrayGUsfEE* _block_3962;
    moonbit_incref(_M0L4dataS3490);
    #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1707
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3490, _M0L3keyS1692);
    moonbit_decref(_M0L4dataS3490);
    if (_M0L7_2abindS1707 == 0) {
      if (_M0L7_2abindS1707) {
        moonbit_decref(_M0L7_2abindS1707);
      }
      goto join_1693;
    } else {
      void* _M0L7_2aSomeS1708 = _M0L7_2abindS1707;
      void* _M0L4_2axS1709 = _M0L7_2aSomeS1708;
      switch (Moonbit_object_tag(_M0L4_2axS1709)) {
        case 4: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1710 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1709;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3508 = _M0L7_2aZSetS1710->$0;
          int32_t _M0L6_2acntS3884 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1710));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1711;
          if (_M0L6_2acntS3884 > 1) {
            int32_t _M0L11_2anew__cntS3885 = _M0L6_2acntS3884 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1710), _M0L11_2anew__cntS3885);
            moonbit_incref(_M0L8_2afieldS3508);
          } else if (_M0L6_2acntS3884 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1710);
          }
          _M0L7_2azsetS1711 = _M0L8_2afieldS3508;
          _M0L4zsetS1695 = _M0L7_2azsetS1711;
          goto join_1694;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1709);
          goto join_1693;
          break;
        }
      }
    }
    join_1694:;
    _M0L6_2atmpS3489 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1696
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1696)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L5itemsS1696->$0 = _M0L6_2atmpS3489;
    _M0L5itemsS1696->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1697 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1695);
    moonbit_decref(_M0L4zsetS1695);
    while (1) {
      moonbit_string_t _M0L1mS1699;
      float _M0L1sS1700;
      struct _M0TUsfE* _M0L7_2abindS1702;
      struct _M0TUsfE* _M0L8_2atupleS3488;
      #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1702 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1697);
      if (_M0L7_2abindS1702 == 0) {
        if (_M0L7_2abindS1702) {
          moonbit_decref(_M0L7_2abindS1702);
        }
        moonbit_decref(_M0L5_2aitS1697);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1703 = _M0L7_2abindS1702;
        struct _M0TUsfE* _M0L4_2axS1704 = _M0L7_2aSomeS1703;
        moonbit_string_t _M0L4_2amS1705 = _M0L4_2axS1704->$0;
        float _M0L4_2asS1706 = _M0L4_2axS1704->$1;
        int32_t _M0L6_2acntS3882 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1704));
        if (_M0L6_2acntS3882 > 1) {
          int32_t _M0L11_2anew__cntS3883 = _M0L6_2acntS3882 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1704), _M0L11_2anew__cntS3883);
          moonbit_incref(_M0L4_2amS1705);
        } else if (_M0L6_2acntS3882 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1704);
        }
        _M0L1mS1699 = _M0L4_2amS1705;
        _M0L1sS1700 = _M0L4_2asS1706;
        goto join_1698;
      }
      goto joinlet_3960;
      join_1698:;
      _M0L8_2atupleS3488
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3488)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
      _M0L8_2atupleS3488->$0 = _M0L1mS1699;
      _M0L8_2atupleS3488->$1 = _M0L1sS1700;
      #line 1271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1696, _M0L8_2atupleS3488);
      moonbit_decref(_M0L8_2atupleS3488);
      continue;
      joinlet_3960:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3961
    = _M0FP39moonbitdb9moonbitdb3lib15sort__by__score(_M0L5itemsS1696);
    moonbit_decref(_M0L5itemsS1696);
    return _result_3961;
    join_1693:;
    _M0L6_2atmpS3487 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_3962
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_3962)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_3962->$0 = _M0L6_2atmpS3487;
    _block_3962->$1 = 0;
    return _block_3962;
  }
}

void* _M0MP39moonbitdb9moonbitdb3lib8Database6zscore(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1680,
  moonbit_string_t _M0L3keyS1681,
  moonbit_string_t _M0L11member__valS1685
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1680, _M0L3keyS1681)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1684;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3485 =
      _M0L4selfS1680->$0;
    void* _M0L7_2abindS1686;
    void* _result_3965;
    moonbit_incref(_M0L4dataS3485);
    #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1686
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3485, _M0L3keyS1681);
    moonbit_decref(_M0L4dataS3485);
    if (_M0L7_2abindS1686 == 0) {
      if (_M0L7_2abindS1686) {
        moonbit_decref(_M0L7_2abindS1686);
      }
      goto join_1682;
    } else {
      void* _M0L7_2aSomeS1687 = _M0L7_2abindS1686;
      void* _M0L4_2axS1688 = _M0L7_2aSomeS1687;
      switch (Moonbit_object_tag(_M0L4_2axS1688)) {
        case 4: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1689 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1688;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3510 = _M0L7_2aZSetS1689->$0;
          int32_t _M0L6_2acntS3886 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1689));
          struct _M0TPB3MapGsfE* _M0L4_2azS1690;
          if (_M0L6_2acntS3886 > 1) {
            int32_t _M0L11_2anew__cntS3887 = _M0L6_2acntS3886 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1689), _M0L11_2anew__cntS3887);
            moonbit_incref(_M0L8_2afieldS3510);
          } else if (_M0L6_2acntS3886 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1689);
          }
          _M0L4_2azS1690 = _M0L8_2afieldS3510;
          _M0L1zS1684 = _M0L4_2azS1690;
          goto join_1683;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1688);
          goto join_1682;
          break;
        }
      }
    }
    join_1683:;
    #line 1239 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3965 = _M0MPB3Map3getGsfE(_M0L1zS1684, _M0L11member__valS1685);
    moonbit_decref(_M0L1zS1684);
    return _result_3965;
    join_1682:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database5zcard(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1671,
  moonbit_string_t _M0L3keyS1672
) {
  #line 1223 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1224 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1671, _M0L3keyS1672)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1674;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3484 =
      _M0L4selfS1671->$0;
    void* _M0L7_2abindS1675;
    int32_t _result_3967;
    moonbit_incref(_M0L4dataS3484);
    #line 1227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1675
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3484, _M0L3keyS1672);
    moonbit_decref(_M0L4dataS3484);
    if (_M0L7_2abindS1675 == 0) {
      if (_M0L7_2abindS1675) {
        moonbit_decref(_M0L7_2abindS1675);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1676 = _M0L7_2abindS1675;
      void* _M0L4_2axS1677 = _M0L7_2aSomeS1676;
      switch (Moonbit_object_tag(_M0L4_2axS1677)) {
        case 4: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1678 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1677;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3512 = _M0L7_2aZSetS1678->$0;
          int32_t _M0L6_2acntS3888 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1678));
          struct _M0TPB3MapGsfE* _M0L4_2azS1679;
          if (_M0L6_2acntS3888 > 1) {
            int32_t _M0L11_2anew__cntS3889 = _M0L6_2acntS3888 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1678), _M0L11_2anew__cntS3889);
            moonbit_incref(_M0L8_2afieldS3512);
          } else if (_M0L6_2acntS3888 == 1) {
            #line 1227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1678);
          }
          _M0L4_2azS1679 = _M0L8_2afieldS3512;
          _M0L1zS1674 = _M0L4_2azS1679;
          goto join_1673;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1677);
          return 0;
          break;
        }
      }
    }
    join_1673:;
    #line 1228 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3967 = _M0MPB3Map6lengthGsfE(_M0L1zS1674);
    moonbit_decref(_M0L1zS1674);
    return _result_3967;
  }
}

struct _M0TPB5ArrayGsE* _M0MP39moonbitdb9moonbitdb3lib8Database6zrange(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1641,
  moonbit_string_t _M0L3keyS1642,
  int32_t _M0L5startS1661,
  int32_t _M0L3endS1663
) {
  #line 1152 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1153 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1641, _M0L3keyS1642)
  ) {
    moonbit_string_t* _M0L6_2atmpS3475 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3968 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3968)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_3968->$0 = _M0L6_2atmpS3475;
    _block_3968->$1 = 0;
    return _block_3968;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1645;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3483 =
      _M0L4selfS1641->$0;
    void* _M0L7_2abindS1666;
    struct _M0TUsfE** _M0L6_2atmpS3482;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1646;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1647;
    struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1657;
    int32_t _M0L3lenS1658;
    moonbit_string_t* _M0L6_2atmpS3481;
    struct _M0TPB5ArrayGsE* _M0L6resultS1659;
    int32_t _M0L10start__idxS1660;
    int32_t _M0L8end__idxS1662;
    int32_t _M0L1iS1664;
    moonbit_string_t* _M0L6_2atmpS3476;
    struct _M0TPB5ArrayGsE* _block_3975;
    moonbit_incref(_M0L4dataS3483);
    #line 1156 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1666
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3483, _M0L3keyS1642);
    moonbit_decref(_M0L4dataS3483);
    if (_M0L7_2abindS1666 == 0) {
      if (_M0L7_2abindS1666) {
        moonbit_decref(_M0L7_2abindS1666);
      }
      goto join_1643;
    } else {
      void* _M0L7_2aSomeS1667 = _M0L7_2abindS1666;
      void* _M0L4_2axS1668 = _M0L7_2aSomeS1667;
      switch (Moonbit_object_tag(_M0L4_2axS1668)) {
        case 4: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1669 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1668;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3516 = _M0L7_2aZSetS1669->$0;
          int32_t _M0L6_2acntS3894 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1669));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1670;
          if (_M0L6_2acntS3894 > 1) {
            int32_t _M0L11_2anew__cntS3895 = _M0L6_2acntS3894 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1669), _M0L11_2anew__cntS3895);
            moonbit_incref(_M0L8_2afieldS3516);
          } else if (_M0L6_2acntS3894 == 1) {
            #line 1156 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aZSetS1669);
          }
          _M0L7_2azsetS1670 = _M0L8_2afieldS3516;
          _M0L4zsetS1645 = _M0L7_2azsetS1670;
          goto join_1644;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1668);
          goto join_1643;
          break;
        }
      }
    }
    join_1644:;
    _M0L6_2atmpS3482 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1646
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1646)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L5itemsS1646->$0 = _M0L6_2atmpS3482;
    _M0L5itemsS1646->$1 = 0;
    #line 1158 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1647 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1645);
    moonbit_decref(_M0L4zsetS1645);
    while (1) {
      moonbit_string_t _M0L1mS1649;
      float _M0L1sS1650;
      struct _M0TUsfE* _M0L7_2abindS1652;
      struct _M0TUsfE* _M0L8_2atupleS3477;
      #line 1159 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1652 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1647);
      if (_M0L7_2abindS1652 == 0) {
        if (_M0L7_2abindS1652) {
          moonbit_decref(_M0L7_2abindS1652);
        }
        moonbit_decref(_M0L5_2aitS1647);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1653 = _M0L7_2abindS1652;
        struct _M0TUsfE* _M0L4_2axS1654 = _M0L7_2aSomeS1653;
        moonbit_string_t _M0L4_2amS1655 = _M0L4_2axS1654->$0;
        float _M0L4_2asS1656 = _M0L4_2axS1654->$1;
        int32_t _M0L6_2acntS3890 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1654));
        if (_M0L6_2acntS3890 > 1) {
          int32_t _M0L11_2anew__cntS3891 = _M0L6_2acntS3890 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1654), _M0L11_2anew__cntS3891);
          moonbit_incref(_M0L4_2amS1655);
        } else if (_M0L6_2acntS3890 == 1) {
          #line 1159 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1654);
        }
        _M0L1mS1649 = _M0L4_2amS1655;
        _M0L1sS1650 = _M0L4_2asS1656;
        goto join_1648;
      }
      goto joinlet_3972;
      join_1648:;
      _M0L8_2atupleS3477
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3477)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
      _M0L8_2atupleS3477->$0 = _M0L1mS1649;
      _M0L8_2atupleS3477->$1 = _M0L1sS1650;
      #line 1160 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1646, _M0L8_2atupleS3477);
      moonbit_decref(_M0L8_2atupleS3477);
      continue;
      joinlet_3972:;
      break;
    }
    #line 1162 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6sortedS1657
    = _M0FP39moonbitdb9moonbitdb3lib15sort__by__score(_M0L5itemsS1646);
    moonbit_decref(_M0L5itemsS1646);
    #line 1163 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3lenS1658 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1657);
    _M0L6_2atmpS3481 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1659
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1659)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L6resultS1659->$0 = _M0L6_2atmpS3481;
    _M0L6resultS1659->$1 = 0;
    if (_M0L5startS1661 < 0) {
      _M0L10start__idxS1660 = _M0L3lenS1658 + _M0L5startS1661;
    } else {
      _M0L10start__idxS1660 = _M0L5startS1661;
    }
    if (_M0L3endS1663 < 0) {
      _M0L8end__idxS1662 = _M0L3lenS1658 + _M0L3endS1663;
    } else {
      _M0L8end__idxS1662 = _M0L3endS1663;
    }
    _M0L1iS1664 = _M0L10start__idxS1660;
    while (1) {
      int32_t _if__result_3974;
      if (_M0L1iS1664 <= _M0L8end__idxS1662) {
        _if__result_3974 = _M0L1iS1664 < _M0L3lenS1658;
      } else {
        _if__result_3974 = 0;
      }
      if (_if__result_3974) {
        struct _M0TUsfE* _M0L6_2atmpS3479;
        moonbit_string_t _M0L8_2afieldS3514;
        int32_t _M0L6_2acntS3892;
        moonbit_string_t _M0L6_2atmpS3478;
        int32_t _M0L6_2atmpS3480;
        #line 1168 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3479
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1657, _M0L1iS1664);
        _M0L8_2afieldS3514 = _M0L6_2atmpS3479->$0;
        _M0L6_2acntS3892
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3479));
        if (_M0L6_2acntS3892 > 1) {
          int32_t _M0L11_2anew__cntS3893 = _M0L6_2acntS3892 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3479), _M0L11_2anew__cntS3893);
          moonbit_incref(_M0L8_2afieldS3514);
        } else if (_M0L6_2acntS3892 == 1) {
          #line 1168 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2atmpS3479);
        }
        _M0L6_2atmpS3478 = _M0L8_2afieldS3514;
        #line 1168 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1659, _M0L6_2atmpS3478);
        moonbit_decref(_M0L6_2atmpS3478);
        _M0L6_2atmpS3480 = _M0L1iS1664 + 1;
        _M0L1iS1664 = _M0L6_2atmpS3480;
        continue;
      } else {
        moonbit_decref(_M0L6sortedS1657);
      }
      break;
    }
    return _M0L6resultS1659;
    join_1643:;
    _M0L6_2atmpS3476 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_3975
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3975)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_3975->$0 = _M0L6_2atmpS3476;
    _block_3975->$1 = 0;
    return _block_3975;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP39moonbitdb9moonbitdb3lib15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1640
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  return _M0FP39moonbitdb9moonbitdb3lib11merge__sort(_M0L5itemsS1640);
}

struct _M0TPB5ArrayGUsfEE* _M0FP39moonbitdb9moonbitdb3lib11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1632
) {
  int32_t _M0L3lenS1631;
  #line 1181 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1631 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1632);
  if (_M0L3lenS1631 <= 1) {
    moonbit_incref(_M0L3arrS1632);
    return _M0L3arrS1632;
  } else {
    int32_t _M0L3midS1633 = _M0L3lenS1631 / 2;
    struct _M0TUsfE** _M0L6_2atmpS3474 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1634 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS3473;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1635;
    int32_t _M0L1iS1636;
    int32_t _M0L1iS1638;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3471;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3472;
    struct _M0TPB5ArrayGUsfEE* _result_3978;
    Moonbit_object_header(_M0L4leftS1634)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4leftS1634->$0 = _M0L6_2atmpS3474;
    _M0L4leftS1634->$1 = 0;
    _M0L6_2atmpS3473 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1635
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1635)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L5rightS1635->$0 = _M0L6_2atmpS3473;
    _M0L5rightS1635->$1 = 0;
    _M0L1iS1636 = 0;
    while (1) {
      if (_M0L1iS1636 < _M0L3midS1633) {
        struct _M0TUsfE* _M0L6_2atmpS3467;
        int32_t _M0L6_2atmpS3468;
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3467
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1632, _M0L1iS1636);
        #line 1190 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1634, _M0L6_2atmpS3467);
        moonbit_decref(_M0L6_2atmpS3467);
        _M0L6_2atmpS3468 = _M0L1iS1636 + 1;
        _M0L1iS1636 = _M0L6_2atmpS3468;
        continue;
      }
      break;
    }
    _M0L1iS1638 = _M0L3midS1633;
    while (1) {
      if (_M0L1iS1638 < _M0L3lenS1631) {
        struct _M0TUsfE* _M0L6_2atmpS3469;
        int32_t _M0L6_2atmpS3470;
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3469
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1632, _M0L1iS1638);
        #line 1193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1635, _M0L6_2atmpS3469);
        moonbit_decref(_M0L6_2atmpS3469);
        _M0L6_2atmpS3470 = _M0L1iS1638 + 1;
        _M0L1iS1638 = _M0L6_2atmpS3470;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3471
    = _M0FP39moonbitdb9moonbitdb3lib11merge__sort(_M0L4leftS1634);
    moonbit_decref(_M0L4leftS1634);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3472
    = _M0FP39moonbitdb9moonbitdb3lib11merge__sort(_M0L5rightS1635);
    moonbit_decref(_M0L5rightS1635);
    #line 1195 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3978
    = _M0FP39moonbitdb9moonbitdb3lib5merge(_M0L6_2atmpS3471, _M0L6_2atmpS3472);
    moonbit_decref(_M0L6_2atmpS3471);
    moonbit_decref(_M0L6_2atmpS3472);
    return _result_3978;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP39moonbitdb9moonbitdb3lib5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1626,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1627
) {
  struct _M0TUsfE** _M0L6_2atmpS3466;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1623;
  struct _M0TPB8MutLocalGiE* _M0L1iS1624;
  struct _M0TPB8MutLocalGiE* _M0L1jS1625;
  #line 1199 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3466 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1623
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1623)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1623->$0 = _M0L6_2atmpS3466;
  _M0L6resultS1623->$1 = 0;
  _M0L1iS1624
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1624)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1624->$0 = 0;
  _M0L1jS1625
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1625)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1625->$0 = 0;
  while (1) {
    int32_t _M0L3valS3438 = _M0L1iS1624->$0;
    int32_t _M0L6_2atmpS3439;
    int32_t _if__result_3980;
    #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3439 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1626);
    if (_M0L3valS3438 < _M0L6_2atmpS3439) {
      int32_t _M0L3valS3436 = _M0L1jS1625->$0;
      int32_t _M0L6_2atmpS3437;
      #line 1203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3437 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1627);
      _if__result_3980 = _M0L3valS3436 < _M0L6_2atmpS3437;
    } else {
      _if__result_3980 = 0;
    }
    if (_if__result_3980) {
      int32_t _M0L3valS3445 = _M0L1iS1624->$0;
      struct _M0TUsfE* _M0L6_2atmpS3444;
      float _M0L6_2atmpS3440;
      int32_t _M0L3valS3443;
      struct _M0TUsfE* _M0L6_2atmpS3442;
      float _M0L6_2atmpS3441;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3444
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1626, _M0L3valS3445);
      _M0L6_2atmpS3440 = _M0L6_2atmpS3444->$1;
      moonbit_decref(_M0L6_2atmpS3444);
      _M0L3valS3443 = _M0L1jS1625->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3442
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1627, _M0L3valS3443);
      _M0L6_2atmpS3441 = _M0L6_2atmpS3442->$1;
      moonbit_decref(_M0L6_2atmpS3442);
      if (_M0L6_2atmpS3440 <= _M0L6_2atmpS3441) {
        int32_t _M0L3valS3447 = _M0L1iS1624->$0;
        struct _M0TUsfE* _M0L6_2atmpS3446;
        int32_t _M0L3valS3449;
        int32_t _M0L6_2atmpS3448;
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3446
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1626, _M0L3valS3447);
        #line 1205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1623, _M0L6_2atmpS3446);
        moonbit_decref(_M0L6_2atmpS3446);
        _M0L3valS3449 = _M0L1iS1624->$0;
        _M0L6_2atmpS3448 = _M0L3valS3449 + 1;
        _M0L1iS1624->$0 = _M0L6_2atmpS3448;
      } else {
        int32_t _M0L3valS3451 = _M0L1jS1625->$0;
        struct _M0TUsfE* _M0L6_2atmpS3450;
        int32_t _M0L3valS3453;
        int32_t _M0L6_2atmpS3452;
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3450
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1627, _M0L3valS3451);
        #line 1208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1623, _M0L6_2atmpS3450);
        moonbit_decref(_M0L6_2atmpS3450);
        _M0L3valS3453 = _M0L1jS1625->$0;
        _M0L6_2atmpS3452 = _M0L3valS3453 + 1;
        _M0L1jS1625->$0 = _M0L6_2atmpS3452;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3454 = _M0L1iS1624->$0;
    int32_t _M0L6_2atmpS3455;
    #line 1212 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3455 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1626);
    if (_M0L3valS3454 < _M0L6_2atmpS3455) {
      int32_t _M0L3valS3457 = _M0L1iS1624->$0;
      struct _M0TUsfE* _M0L6_2atmpS3456;
      int32_t _M0L3valS3459;
      int32_t _M0L6_2atmpS3458;
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3456
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1626, _M0L3valS3457);
      #line 1213 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1623, _M0L6_2atmpS3456);
      moonbit_decref(_M0L6_2atmpS3456);
      _M0L3valS3459 = _M0L1iS1624->$0;
      _M0L6_2atmpS3458 = _M0L3valS3459 + 1;
      _M0L1iS1624->$0 = _M0L6_2atmpS3458;
      continue;
    } else {
      moonbit_decref(_M0L1iS1624);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3460 = _M0L1jS1625->$0;
    int32_t _M0L6_2atmpS3461;
    #line 1216 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3461 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1627);
    if (_M0L3valS3460 < _M0L6_2atmpS3461) {
      int32_t _M0L3valS3463 = _M0L1jS1625->$0;
      struct _M0TUsfE* _M0L6_2atmpS3462;
      int32_t _M0L3valS3465;
      int32_t _M0L6_2atmpS3464;
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3462
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1627, _M0L3valS3463);
      #line 1217 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1623, _M0L6_2atmpS3462);
      moonbit_decref(_M0L6_2atmpS3462);
      _M0L3valS3465 = _M0L1jS1625->$0;
      _M0L6_2atmpS3464 = _M0L3valS3465 + 1;
      _M0L1jS1625->$0 = _M0L6_2atmpS3464;
      continue;
    } else {
      moonbit_decref(_M0L1jS1625);
    }
    break;
  }
  return _M0L6resultS1623;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4zadd(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1608,
  moonbit_string_t _M0L3keyS1609,
  float _M0L5scoreS1622,
  moonbit_string_t _M0L11member__valS1621
) {
  int32_t _M0L6_2atmpS3430;
  struct _M0TPB3MapGsfE* _M0L4zsetS1610;
  struct _M0TPB3MapGsfE* _M0L1zS1614;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3435;
  void* _M0L7_2abindS1615;
  struct _M0TUsfE** _M0L7_2abindS1612;
  struct _M0TUsfE** _M0L6_2atmpS3434;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3433;
  int32_t _M0L7existedS1620;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3431;
  void* _M0L4ZSetS3432;
  #line 1140 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3430
  = _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1608, _M0L3keyS1609);
  _M0L4dataS3435 = _M0L4selfS1608->$0;
  moonbit_incref(_M0L4dataS3435);
  #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1615
  = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3435, _M0L3keyS1609);
  moonbit_decref(_M0L4dataS3435);
  if (_M0L7_2abindS1615 == 0) {
    if (_M0L7_2abindS1615) {
      moonbit_decref(_M0L7_2abindS1615);
    }
    goto join_1611;
  } else {
    void* _M0L7_2aSomeS1616 = _M0L7_2abindS1615;
    void* _M0L4_2axS1617 = _M0L7_2aSomeS1616;
    switch (Moonbit_object_tag(_M0L4_2axS1617)) {
      case 4: {
        struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet* _M0L7_2aZSetS1618 =
          (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet*)_M0L4_2axS1617;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3519 = _M0L7_2aZSetS1618->$0;
        int32_t _M0L6_2acntS3896 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1618));
        struct _M0TPB3MapGsfE* _M0L4_2azS1619;
        if (_M0L6_2acntS3896 > 1) {
          int32_t _M0L11_2anew__cntS3897 = _M0L6_2acntS3896 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1618), _M0L11_2anew__cntS3897);
          moonbit_incref(_M0L8_2afieldS3519);
        } else if (_M0L6_2acntS3896 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aZSetS1618);
        }
        _M0L4_2azS1619 = _M0L8_2afieldS3519;
        _M0L1zS1614 = _M0L4_2azS1619;
        goto join_1613;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1617);
        goto join_1611;
        break;
      }
    }
  }
  goto joinlet_3984;
  join_1613:;
  _M0L4zsetS1610 = _M0L1zS1614;
  joinlet_3984:;
  goto joinlet_3983;
  join_1611:;
  _M0L7_2abindS1612 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3434 = _M0L7_2abindS1612;
  _M0L6_2atmpS3433
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3434, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4zsetS1610 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3433, 10ll);
  moonbit_decref(_M0L6_2atmpS3433.$0);
  joinlet_3983:;
  #line 1146 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7existedS1620
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1610, _M0L11member__valS1621);
  #line 1147 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1610, _M0L11member__valS1621, _M0L5scoreS1622);
  _M0L4dataS3431 = _M0L4selfS1608->$0;
  _M0L4ZSetS3432
  = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3432)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 4);
  ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4ZSet*)_M0L4ZSetS3432)->$0
  = _M0L4zsetS1610;
  moonbit_incref(_M0L4dataS3431);
  #line 1148 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3431, _M0L3keyS1609, _M0L4ZSetS3432);
  moonbit_decref(_M0L4dataS3431);
  moonbit_decref(_M0L4ZSetS3432);
  return !_M0L7existedS1620;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database9sismember(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1598,
  moonbit_string_t _M0L3keyS1599,
  moonbit_string_t _M0L5valueS1602
) {
  #line 963 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 964 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1598, _M0L3keyS1599)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1601;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3429 =
      _M0L4selfS1598->$0;
    void* _M0L7_2abindS1603;
    int32_t _result_3986;
    moonbit_incref(_M0L4dataS3429);
    #line 967 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1603
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3429, _M0L3keyS1599);
    moonbit_decref(_M0L4dataS3429);
    if (_M0L7_2abindS1603 == 0) {
      if (_M0L7_2abindS1603) {
        moonbit_decref(_M0L7_2abindS1603);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1604 = _M0L7_2abindS1603;
      void* _M0L4_2axS1605 = _M0L7_2aSomeS1604;
      switch (Moonbit_object_tag(_M0L4_2axS1605)) {
        case 3: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1606 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1605;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3521 = _M0L6_2aSetS1606->$0;
          int32_t _M0L6_2acntS3898 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1606));
          struct _M0TPB3MapGsbE* _M0L4_2asS1607;
          if (_M0L6_2acntS3898 > 1) {
            int32_t _M0L11_2anew__cntS3899 = _M0L6_2acntS3898 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1606), _M0L11_2anew__cntS3899);
            moonbit_incref(_M0L8_2afieldS3521);
          } else if (_M0L6_2acntS3898 == 1) {
            #line 967 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1606);
          }
          _M0L4_2asS1607 = _M0L8_2afieldS3521;
          _M0L1sS1601 = _M0L4_2asS1607;
          goto join_1600;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1605);
          return 0;
          break;
        }
      }
    }
    join_1600:;
    #line 968 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3986 = _M0MPB3Map8containsGsbE(_M0L1sS1601, _M0L5valueS1602);
    moonbit_decref(_M0L1sS1601);
    return _result_3986;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database5scard(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1589,
  moonbit_string_t _M0L3keyS1590
) {
  #line 952 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 953 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1589, _M0L3keyS1590)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1592;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3428 =
      _M0L4selfS1589->$0;
    void* _M0L7_2abindS1593;
    int32_t _result_3988;
    moonbit_incref(_M0L4dataS3428);
    #line 956 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1593
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3428, _M0L3keyS1590);
    moonbit_decref(_M0L4dataS3428);
    if (_M0L7_2abindS1593 == 0) {
      if (_M0L7_2abindS1593) {
        moonbit_decref(_M0L7_2abindS1593);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1594 = _M0L7_2abindS1593;
      void* _M0L4_2axS1595 = _M0L7_2aSomeS1594;
      switch (Moonbit_object_tag(_M0L4_2axS1595)) {
        case 3: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1596 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1595;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3523 = _M0L6_2aSetS1596->$0;
          int32_t _M0L6_2acntS3900 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1596));
          struct _M0TPB3MapGsbE* _M0L4_2asS1597;
          if (_M0L6_2acntS3900 > 1) {
            int32_t _M0L11_2anew__cntS3901 = _M0L6_2acntS3900 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1596), _M0L11_2anew__cntS3901);
            moonbit_incref(_M0L8_2afieldS3523);
          } else if (_M0L6_2acntS3900 == 1) {
            #line 956 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1596);
          }
          _M0L4_2asS1597 = _M0L8_2afieldS3523;
          _M0L1sS1592 = _M0L4_2asS1597;
          goto join_1591;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1595);
          return 0;
          break;
        }
      }
    }
    join_1591:;
    #line 957 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_3988 = _M0MPB3Map6lengthGsbE(_M0L1sS1592);
    moonbit_decref(_M0L1sS1592);
    return _result_3988;
  }
}

struct _M0TPB5ArrayGsE* _M0MP39moonbitdb9moonbitdb3lib8Database8smembers(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1570,
  moonbit_string_t _M0L3keyS1571
) {
  #line 917 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 918 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1570, _M0L3keyS1571)
  ) {
    moonbit_string_t* _M0L6_2atmpS3424 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3989 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3989)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_3989->$0 = _M0L6_2atmpS3424;
    _block_3989->$1 = 0;
    return _block_3989;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1574;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3427 =
      _M0L4selfS1570->$0;
    void* _M0L7_2abindS1584;
    moonbit_string_t* _M0L6_2atmpS3426;
    struct _M0TPB5ArrayGsE* _M0L6resultS1575;
    struct _M0TPB4IterGUsbEE* _M0L5_2aitS1576;
    moonbit_string_t* _M0L6_2atmpS3425;
    struct _M0TPB5ArrayGsE* _block_3994;
    moonbit_incref(_M0L4dataS3427);
    #line 921 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1584
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3427, _M0L3keyS1571);
    moonbit_decref(_M0L4dataS3427);
    if (_M0L7_2abindS1584 == 0) {
      if (_M0L7_2abindS1584) {
        moonbit_decref(_M0L7_2abindS1584);
      }
      goto join_1572;
    } else {
      void* _M0L7_2aSomeS1585 = _M0L7_2abindS1584;
      void* _M0L4_2axS1586 = _M0L7_2aSomeS1585;
      switch (Moonbit_object_tag(_M0L4_2axS1586)) {
        case 3: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1587 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1586;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3526 = _M0L6_2aSetS1587->$0;
          int32_t _M0L6_2acntS3904 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1587));
          struct _M0TPB3MapGsbE* _M0L4_2asS1588;
          if (_M0L6_2acntS3904 > 1) {
            int32_t _M0L11_2anew__cntS3905 = _M0L6_2acntS3904 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1587), _M0L11_2anew__cntS3905);
            moonbit_incref(_M0L8_2afieldS3526);
          } else if (_M0L6_2acntS3904 == 1) {
            #line 921 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L6_2aSetS1587);
          }
          _M0L4_2asS1588 = _M0L8_2afieldS3526;
          _M0L1sS1574 = _M0L4_2asS1588;
          goto join_1573;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1586);
          goto join_1572;
          break;
        }
      }
    }
    join_1573:;
    _M0L6_2atmpS3426 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1575
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1575)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L6resultS1575->$0 = _M0L6_2atmpS3426;
    _M0L6resultS1575->$1 = 0;
    #line 923 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L5_2aitS1576 = _M0MPB3Map5iter2GsbE(_M0L1sS1574);
    moonbit_decref(_M0L1sS1574);
    while (1) {
      moonbit_string_t _M0L1mS1578;
      struct _M0TUsbE* _M0L7_2abindS1580;
      #line 924 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L7_2abindS1580 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1576);
      if (_M0L7_2abindS1580 == 0) {
        if (_M0L7_2abindS1580) {
          moonbit_decref(_M0L7_2abindS1580);
        }
        moonbit_decref(_M0L5_2aitS1576);
      } else {
        struct _M0TUsbE* _M0L7_2aSomeS1581 = _M0L7_2abindS1580;
        struct _M0TUsbE* _M0L4_2axS1582 = _M0L7_2aSomeS1581;
        moonbit_string_t _M0L8_2afieldS3525 = _M0L4_2axS1582->$0;
        int32_t _M0L6_2acntS3902 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1582));
        moonbit_string_t _M0L4_2amS1583;
        if (_M0L6_2acntS3902 > 1) {
          int32_t _M0L11_2anew__cntS3903 = _M0L6_2acntS3902 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1582), _M0L11_2anew__cntS3903);
          moonbit_incref(_M0L8_2afieldS3525);
        } else if (_M0L6_2acntS3902 == 1) {
          #line 924 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L4_2axS1582);
        }
        _M0L4_2amS1583 = _M0L8_2afieldS3525;
        _M0L1mS1578 = _M0L4_2amS1583;
        goto join_1577;
      }
      goto joinlet_3993;
      join_1577:;
      #line 925 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1575, _M0L1mS1578);
      moonbit_decref(_M0L1mS1578);
      continue;
      joinlet_3993:;
      break;
    }
    return _M0L6resultS1575;
    join_1572:;
    _M0L6_2atmpS3425 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_3994
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3994)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_3994->$0 = _M0L6_2atmpS3425;
    _block_3994->$1 = 0;
    return _block_3994;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4sadd(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1557,
  moonbit_string_t _M0L3keyS1558,
  moonbit_string_t _M0L5valueS1569
) {
  int32_t _M0L6_2atmpS3418;
  struct _M0TPB3MapGsbE* _M0L3setS1559;
  struct _M0TPB3MapGsbE* _M0L1sS1563;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3423;
  void* _M0L7_2abindS1564;
  struct _M0TUsbE** _M0L7_2abindS1561;
  struct _M0TUsbE** _M0L6_2atmpS3422;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3421;
  #line 902 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3418
  = _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1557, _M0L3keyS1558);
  _M0L4dataS3423 = _M0L4selfS1557->$0;
  moonbit_incref(_M0L4dataS3423);
  #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1564
  = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3423, _M0L3keyS1558);
  moonbit_decref(_M0L4dataS3423);
  if (_M0L7_2abindS1564 == 0) {
    if (_M0L7_2abindS1564) {
      moonbit_decref(_M0L7_2abindS1564);
    }
    goto join_1560;
  } else {
    void* _M0L7_2aSomeS1565 = _M0L7_2abindS1564;
    void* _M0L4_2axS1566 = _M0L7_2aSomeS1565;
    switch (Moonbit_object_tag(_M0L4_2axS1566)) {
      case 3: {
        struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set* _M0L6_2aSetS1567 =
          (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set*)_M0L4_2axS1566;
        struct _M0TPB3MapGsbE* _M0L8_2afieldS3529 = _M0L6_2aSetS1567->$0;
        int32_t _M0L6_2acntS3906 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1567));
        struct _M0TPB3MapGsbE* _M0L4_2asS1568;
        if (_M0L6_2acntS3906 > 1) {
          int32_t _M0L11_2anew__cntS3907 = _M0L6_2acntS3906 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1567), _M0L11_2anew__cntS3907);
          moonbit_incref(_M0L8_2afieldS3529);
        } else if (_M0L6_2acntS3906 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L6_2aSetS1567);
        }
        _M0L4_2asS1568 = _M0L8_2afieldS3529;
        _M0L1sS1563 = _M0L4_2asS1568;
        goto join_1562;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1566);
        goto join_1560;
        break;
      }
    }
  }
  goto joinlet_3996;
  join_1562:;
  _M0L3setS1559 = _M0L1sS1563;
  joinlet_3996:;
  goto joinlet_3995;
  join_1560:;
  _M0L7_2abindS1561 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3422 = _M0L7_2abindS1561;
  _M0L6_2atmpS3421
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3422, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3setS1559 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3421, 10ll);
  moonbit_decref(_M0L6_2atmpS3421.$0);
  joinlet_3995:;
  #line 908 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1559, _M0L5valueS1569)) {
    moonbit_decref(_M0L3setS1559);
    return 0;
  } else {
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3419;
    void* _M0L3SetS3420;
    #line 911 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1559, _M0L5valueS1569, 1);
    _M0L4dataS3419 = _M0L4selfS1557->$0;
    _M0L3SetS3420
    = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS3420)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 12, 3);
    ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue3Set*)_M0L3SetS3420)->$0
    = _M0L3setS1559;
    moonbit_incref(_M0L4dataS3419);
    #line 912 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3419, _M0L3keyS1558, _M0L3SetS3420);
    moonbit_decref(_M0L4dataS3419);
    moonbit_decref(_M0L3SetS3420);
    return 1;
  }
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database6lindex(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1546,
  moonbit_string_t _M0L3keyS1547,
  int32_t _M0L5indexS1551
) {
  #line 741 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 742 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1546, _M0L3keyS1547)
  ) {
    return 0;
  } else {
    struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L5dequeS1550;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3417 =
      _M0L4selfS1546->$0;
    void* _M0L7_2abindS1552;
    moonbit_incref(_M0L4dataS3417);
    #line 745 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1552
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3417, _M0L3keyS1547);
    moonbit_decref(_M0L4dataS3417);
    if (_M0L7_2abindS1552 == 0) {
      if (_M0L7_2abindS1552) {
        moonbit_decref(_M0L7_2abindS1552);
      }
      goto join_1548;
    } else {
      void* _M0L7_2aSomeS1553 = _M0L7_2abindS1552;
      void* _M0L4_2axS1554 = _M0L7_2aSomeS1553;
      switch (Moonbit_object_tag(_M0L4_2axS1554)) {
        case 2: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List* _M0L7_2aListS1555 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4_2axS1554;
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2afieldS3531 =
            _M0L7_2aListS1555->$0;
          int32_t _M0L6_2acntS3908 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1555));
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2adequeS1556;
          if (_M0L6_2acntS3908 > 1) {
            int32_t _M0L11_2anew__cntS3909 = _M0L6_2acntS3908 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1555), _M0L11_2anew__cntS3909);
            moonbit_incref(_M0L8_2afieldS3531);
          } else if (_M0L6_2acntS3908 == 1) {
            #line 745 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1555);
          }
          _M0L8_2adequeS1556 = _M0L8_2afieldS3531;
          _M0L5dequeS1550 = _M0L8_2adequeS1556;
          goto join_1549;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1554);
          goto join_1548;
          break;
        }
      }
    }
    join_1549:;
    if (_M0L5indexS1551 < 0) {
      int32_t _M0L6_2atmpS3416;
      int32_t _M0L6_2atmpS3415;
      moonbit_string_t _result_3999;
      #line 748 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3416
      = _M0MP39moonbitdb9moonbitdb3lib5Deque6length(_M0L5dequeS1550);
      _M0L6_2atmpS3415 = _M0L6_2atmpS3416 + _M0L5indexS1551;
      #line 748 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _result_3999
      = _M0MP39moonbitdb9moonbitdb3lib5Deque7get__at(_M0L5dequeS1550, _M0L6_2atmpS3415);
      moonbit_decref(_M0L5dequeS1550);
      return _result_3999;
    } else {
      moonbit_string_t _result_4000;
      #line 750 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _result_4000
      = _M0MP39moonbitdb9moonbitdb3lib5Deque7get__at(_M0L5dequeS1550, _M0L5indexS1551);
      moonbit_decref(_M0L5dequeS1550);
      return _result_4000;
    }
    join_1548:;
    return 0;
  }
}

struct _M0TPB5ArrayGsE* _M0MP39moonbitdb9moonbitdb3lib8Database6lrange(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1527,
  moonbit_string_t _M0L3keyS1528,
  int32_t _M0L5startS1535,
  int32_t _M0L3endS1537
) {
  #line 720 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 721 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1527, _M0L3keyS1528)
  ) {
    moonbit_string_t* _M0L6_2atmpS3409 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4001 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4001)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_4001->$0 = _M0L6_2atmpS3409;
    _block_4001->$1 = 0;
    return _block_4001;
  } else {
    struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L5dequeS1531;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3414 =
      _M0L4selfS1527->$0;
    void* _M0L7_2abindS1541;
    struct _M0TPB5ArrayGsE* _M0L3arrS1532;
    int32_t _M0L3lenS1533;
    int32_t _M0L10start__idxS1534;
    int32_t _M0L8end__idxS1536;
    moonbit_string_t* _M0L6_2atmpS3413;
    struct _M0TPB5ArrayGsE* _M0L6resultS1538;
    int32_t _M0L1iS1539;
    moonbit_string_t* _M0L6_2atmpS3410;
    struct _M0TPB5ArrayGsE* _block_4006;
    moonbit_incref(_M0L4dataS3414);
    #line 724 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1541
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3414, _M0L3keyS1528);
    moonbit_decref(_M0L4dataS3414);
    if (_M0L7_2abindS1541 == 0) {
      if (_M0L7_2abindS1541) {
        moonbit_decref(_M0L7_2abindS1541);
      }
      goto join_1529;
    } else {
      void* _M0L7_2aSomeS1542 = _M0L7_2abindS1541;
      void* _M0L4_2axS1543 = _M0L7_2aSomeS1542;
      switch (Moonbit_object_tag(_M0L4_2axS1543)) {
        case 2: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List* _M0L7_2aListS1544 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4_2axS1543;
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2afieldS3533 =
            _M0L7_2aListS1544->$0;
          int32_t _M0L6_2acntS3910 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1544));
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2adequeS1545;
          if (_M0L6_2acntS3910 > 1) {
            int32_t _M0L11_2anew__cntS3911 = _M0L6_2acntS3910 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1544), _M0L11_2anew__cntS3911);
            moonbit_incref(_M0L8_2afieldS3533);
          } else if (_M0L6_2acntS3910 == 1) {
            #line 724 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1544);
          }
          _M0L8_2adequeS1545 = _M0L8_2afieldS3533;
          _M0L5dequeS1531 = _M0L8_2adequeS1545;
          goto join_1530;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1543);
          goto join_1529;
          break;
        }
      }
    }
    join_1530:;
    #line 726 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3arrS1532
    = _M0MP39moonbitdb9moonbitdb3lib5Deque9to__array(_M0L5dequeS1531);
    moonbit_decref(_M0L5dequeS1531);
    #line 727 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3lenS1533 = _M0MPC15array5Array6lengthGsE(_M0L3arrS1532);
    if (_M0L5startS1535 < 0) {
      _M0L10start__idxS1534 = _M0L3lenS1533 + _M0L5startS1535;
    } else {
      _M0L10start__idxS1534 = _M0L5startS1535;
    }
    if (_M0L3endS1537 < 0) {
      _M0L8end__idxS1536 = _M0L3lenS1533 + _M0L3endS1537;
    } else {
      _M0L8end__idxS1536 = _M0L3endS1537;
    }
    _M0L6_2atmpS3413 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1538
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1538)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L6resultS1538->$0 = _M0L6_2atmpS3413;
    _M0L6resultS1538->$1 = 0;
    _M0L1iS1539 = _M0L10start__idxS1534;
    while (1) {
      int32_t _if__result_4005;
      if (_M0L1iS1539 <= _M0L8end__idxS1536) {
        if (_M0L1iS1539 >= 0) {
          _if__result_4005 = _M0L1iS1539 < _M0L3lenS1533;
        } else {
          _if__result_4005 = 0;
        }
      } else {
        _if__result_4005 = 0;
      }
      if (_if__result_4005) {
        moonbit_string_t _M0L6_2atmpS3411;
        int32_t _M0L6_2atmpS3412;
        #line 732 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3411
        = _M0MPC15array5Array2atGsE(_M0L3arrS1532, _M0L1iS1539);
        #line 732 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1538, _M0L6_2atmpS3411);
        moonbit_decref(_M0L6_2atmpS3411);
        _M0L6_2atmpS3412 = _M0L1iS1539 + 1;
        _M0L1iS1539 = _M0L6_2atmpS3412;
        continue;
      } else {
        moonbit_decref(_M0L3arrS1532);
      }
      break;
    }
    return _M0L6resultS1538;
    join_1529:;
    _M0L6_2atmpS3410 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4006
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4006)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_4006->$0 = _M0L6_2atmpS3410;
    _block_4006->$1 = 0;
    return _block_4006;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4llen(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1518,
  moonbit_string_t _M0L3keyS1519
) {
  #line 709 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1518, _M0L3keyS1519)
  ) {
    return 0;
  } else {
    struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L1dS1521;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3408 =
      _M0L4selfS1518->$0;
    void* _M0L7_2abindS1522;
    int32_t _result_4008;
    moonbit_incref(_M0L4dataS3408);
    #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1522
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3408, _M0L3keyS1519);
    moonbit_decref(_M0L4dataS3408);
    if (_M0L7_2abindS1522 == 0) {
      if (_M0L7_2abindS1522) {
        moonbit_decref(_M0L7_2abindS1522);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1523 = _M0L7_2abindS1522;
      void* _M0L4_2axS1524 = _M0L7_2aSomeS1523;
      switch (Moonbit_object_tag(_M0L4_2axS1524)) {
        case 2: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List* _M0L7_2aListS1525 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4_2axS1524;
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2afieldS3535 =
            _M0L7_2aListS1525->$0;
          int32_t _M0L6_2acntS3912 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1525));
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L4_2adS1526;
          if (_M0L6_2acntS3912 > 1) {
            int32_t _M0L11_2anew__cntS3913 = _M0L6_2acntS3912 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1525), _M0L11_2anew__cntS3913);
            moonbit_incref(_M0L8_2afieldS3535);
          } else if (_M0L6_2acntS3912 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1525);
          }
          _M0L4_2adS1526 = _M0L8_2afieldS3535;
          _M0L1dS1521 = _M0L4_2adS1526;
          goto join_1520;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1524);
          return 0;
          break;
        }
      }
    }
    join_1520:;
    #line 714 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_4008 = _M0MP39moonbitdb9moonbitdb3lib5Deque6length(_M0L1dS1521);
    moonbit_decref(_M0L1dS1521);
    return _result_4008;
  }
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4rpop(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1507,
  moonbit_string_t _M0L3keyS1508
) {
  #line 694 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 695 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1507, _M0L3keyS1508)
  ) {
    return 0;
  } else {
    struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L5dequeS1511;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3407 =
      _M0L4selfS1507->$0;
    void* _M0L7_2abindS1513;
    moonbit_string_t _M0L3valS1512;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3405;
    void* _M0L4ListS3406;
    moonbit_incref(_M0L4dataS3407);
    #line 698 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1513
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3407, _M0L3keyS1508);
    moonbit_decref(_M0L4dataS3407);
    if (_M0L7_2abindS1513 == 0) {
      if (_M0L7_2abindS1513) {
        moonbit_decref(_M0L7_2abindS1513);
      }
      goto join_1509;
    } else {
      void* _M0L7_2aSomeS1514 = _M0L7_2abindS1513;
      void* _M0L4_2axS1515 = _M0L7_2aSomeS1514;
      switch (Moonbit_object_tag(_M0L4_2axS1515)) {
        case 2: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List* _M0L7_2aListS1516 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4_2axS1515;
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2afieldS3538 =
            _M0L7_2aListS1516->$0;
          int32_t _M0L6_2acntS3914 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1516));
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2adequeS1517;
          if (_M0L6_2acntS3914 > 1) {
            int32_t _M0L11_2anew__cntS3915 = _M0L6_2acntS3914 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1516), _M0L11_2anew__cntS3915);
            moonbit_incref(_M0L8_2afieldS3538);
          } else if (_M0L6_2acntS3914 == 1) {
            #line 698 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1516);
          }
          _M0L8_2adequeS1517 = _M0L8_2afieldS3538;
          _M0L5dequeS1511 = _M0L8_2adequeS1517;
          goto join_1510;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1515);
          goto join_1509;
          break;
        }
      }
    }
    join_1510:;
    #line 700 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3valS1512
    = _M0MP39moonbitdb9moonbitdb3lib5Deque9pop__back(_M0L5dequeS1511);
    _M0L4dataS3405 = _M0L4selfS1507->$0;
    _M0L4ListS3406
    = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3406)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4ListS3406)->$0
    = _M0L5dequeS1511;
    moonbit_incref(_M0L4dataS3405);
    #line 701 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3405, _M0L3keyS1508, _M0L4ListS3406);
    moonbit_decref(_M0L4dataS3405);
    moonbit_decref(_M0L4ListS3406);
    return _M0L3valS1512;
    join_1509:;
    return 0;
  }
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4lpop(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1496,
  moonbit_string_t _M0L3keyS1497
) {
  #line 679 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 680 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1496, _M0L3keyS1497)
  ) {
    return 0;
  } else {
    struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L5dequeS1500;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3404 =
      _M0L4selfS1496->$0;
    void* _M0L7_2abindS1502;
    moonbit_string_t _M0L3valS1501;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3402;
    void* _M0L4ListS3403;
    moonbit_incref(_M0L4dataS3404);
    #line 683 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1502
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3404, _M0L3keyS1497);
    moonbit_decref(_M0L4dataS3404);
    if (_M0L7_2abindS1502 == 0) {
      if (_M0L7_2abindS1502) {
        moonbit_decref(_M0L7_2abindS1502);
      }
      goto join_1498;
    } else {
      void* _M0L7_2aSomeS1503 = _M0L7_2abindS1502;
      void* _M0L4_2axS1504 = _M0L7_2aSomeS1503;
      switch (Moonbit_object_tag(_M0L4_2axS1504)) {
        case 2: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List* _M0L7_2aListS1505 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4_2axS1504;
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2afieldS3541 =
            _M0L7_2aListS1505->$0;
          int32_t _M0L6_2acntS3916 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1505));
          struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2adequeS1506;
          if (_M0L6_2acntS3916 > 1) {
            int32_t _M0L11_2anew__cntS3917 = _M0L6_2acntS3916 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1505), _M0L11_2anew__cntS3917);
            moonbit_incref(_M0L8_2afieldS3541);
          } else if (_M0L6_2acntS3916 == 1) {
            #line 683 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aListS1505);
          }
          _M0L8_2adequeS1506 = _M0L8_2afieldS3541;
          _M0L5dequeS1500 = _M0L8_2adequeS1506;
          goto join_1499;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1504);
          goto join_1498;
          break;
        }
      }
    }
    join_1499:;
    #line 685 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L3valS1501
    = _M0MP39moonbitdb9moonbitdb3lib5Deque10pop__front(_M0L5dequeS1500);
    _M0L4dataS3402 = _M0L4selfS1496->$0;
    _M0L4ListS3403
    = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3403)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4ListS3403)->$0
    = _M0L5dequeS1500;
    moonbit_incref(_M0L4dataS3402);
    #line 686 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3402, _M0L3keyS1497, _M0L4ListS3403);
    moonbit_decref(_M0L4dataS3402);
    moonbit_decref(_M0L4ListS3403);
    return _M0L3valS1501;
    join_1498:;
    return 0;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database5rpush(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1484,
  moonbit_string_t _M0L3keyS1485,
  moonbit_string_t _M0L5valueS1495
) {
  int32_t _M0L6_2atmpS3398;
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L5dequeS1486;
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L1dS1489;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3401;
  void* _M0L7_2abindS1490;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3399;
  void* _M0L4ListS3400;
  int32_t _result_4015;
  #line 668 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 669 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3398
  = _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1484, _M0L3keyS1485);
  _M0L4dataS3401 = _M0L4selfS1484->$0;
  moonbit_incref(_M0L4dataS3401);
  #line 670 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1490
  = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3401, _M0L3keyS1485);
  moonbit_decref(_M0L4dataS3401);
  if (_M0L7_2abindS1490 == 0) {
    if (_M0L7_2abindS1490) {
      moonbit_decref(_M0L7_2abindS1490);
    }
    goto join_1487;
  } else {
    void* _M0L7_2aSomeS1491 = _M0L7_2abindS1490;
    void* _M0L4_2axS1492 = _M0L7_2aSomeS1491;
    switch (Moonbit_object_tag(_M0L4_2axS1492)) {
      case 2: {
        struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List* _M0L7_2aListS1493 =
          (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4_2axS1492;
        struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L8_2afieldS3544 =
          _M0L7_2aListS1493->$0;
        int32_t _M0L6_2acntS3918 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1493));
        struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L4_2adS1494;
        if (_M0L6_2acntS3918 > 1) {
          int32_t _M0L11_2anew__cntS3919 = _M0L6_2acntS3918 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1493), _M0L11_2anew__cntS3919);
          moonbit_incref(_M0L8_2afieldS3544);
        } else if (_M0L6_2acntS3918 == 1) {
          #line 670 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aListS1493);
        }
        _M0L4_2adS1494 = _M0L8_2afieldS3544;
        _M0L1dS1489 = _M0L4_2adS1494;
        goto join_1488;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1492);
        goto join_1487;
        break;
      }
    }
  }
  goto joinlet_4014;
  join_1488:;
  _M0L5dequeS1486 = _M0L1dS1489;
  joinlet_4014:;
  goto joinlet_4013;
  join_1487:;
  #line 672 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5dequeS1486 = _M0MP39moonbitdb9moonbitdb3lib5Deque3new();
  joinlet_4013:;
  #line 674 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MP39moonbitdb9moonbitdb3lib5Deque10push__back(_M0L5dequeS1486, _M0L5valueS1495);
  _M0L4dataS3399 = _M0L4selfS1484->$0;
  moonbit_incref(_M0L5dequeS1486);
  _M0L4ListS3400
  = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List));
  Moonbit_object_header(_M0L4ListS3400)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
  ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4List*)_M0L4ListS3400)->$0
  = _M0L5dequeS1486;
  moonbit_incref(_M0L4dataS3399);
  #line 675 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3399, _M0L3keyS1485, _M0L4ListS3400);
  moonbit_decref(_M0L4dataS3399);
  moonbit_decref(_M0L4ListS3400);
  #line 676 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_4015 = _M0MP39moonbitdb9moonbitdb3lib5Deque6length(_M0L5dequeS1486);
  moonbit_decref(_M0L5dequeS1486);
  return _result_4015;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4hlen(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1475,
  moonbit_string_t _M0L3keyS1476
) {
  #line 646 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 647 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1475, _M0L3keyS1476)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1478;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3397 =
      _M0L4selfS1475->$0;
    void* _M0L7_2abindS1479;
    int32_t _result_4017;
    moonbit_incref(_M0L4dataS3397);
    #line 650 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1479
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3397, _M0L3keyS1476);
    moonbit_decref(_M0L4dataS3397);
    if (_M0L7_2abindS1479 == 0) {
      if (_M0L7_2abindS1479) {
        moonbit_decref(_M0L7_2abindS1479);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1480 = _M0L7_2abindS1479;
      void* _M0L4_2axS1481 = _M0L7_2aSomeS1480;
      switch (Moonbit_object_tag(_M0L4_2axS1481)) {
        case 1: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1482 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1481;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3546 = _M0L7_2aHashS1482->$0;
          int32_t _M0L6_2acntS3920 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1482));
          struct _M0TPB3MapGssE* _M0L4_2ahS1483;
          if (_M0L6_2acntS3920 > 1) {
            int32_t _M0L11_2anew__cntS3921 = _M0L6_2acntS3920 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1482), _M0L11_2anew__cntS3921);
            moonbit_incref(_M0L8_2afieldS3546);
          } else if (_M0L6_2acntS3920 == 1) {
            #line 650 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1482);
          }
          _M0L4_2ahS1483 = _M0L8_2afieldS3546;
          _M0L1hS1478 = _M0L4_2ahS1483;
          goto join_1477;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1481);
          return 0;
          break;
        }
      }
    }
    join_1477:;
    #line 651 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_4017 = _M0MPB3Map6lengthGssE(_M0L1hS1478);
    moonbit_decref(_M0L1hS1478);
    return _result_4017;
  }
}

struct _M0TPB3MapGssE* _M0MP39moonbitdb9moonbitdb3lib8Database7hgetall(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1463,
  moonbit_string_t _M0L3keyS1464
) {
  #line 635 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 636 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1463, _M0L3keyS1464)
  ) {
    struct _M0TUssE** _M0L7_2abindS1465 =
      (struct _M0TUssE**)moonbit_empty_ref_array;
    struct _M0TUssE** _M0L6_2atmpS3393 = _M0L7_2abindS1465;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3392 =
      (struct _M0TPB9ArrayViewGUssEE){.$0 = _M0L6_2atmpS3393,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGssE* _result_4018;
    #line 637 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_4018 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3392, 0ll);
    moonbit_decref(_M0L6_2atmpS3392.$0);
    return _result_4018;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1469;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3396 =
      _M0L4selfS1463->$0;
    void* _M0L7_2abindS1470;
    struct _M0TUssE** _M0L7_2abindS1467;
    struct _M0TUssE** _M0L6_2atmpS3395;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3394;
    struct _M0TPB3MapGssE* _result_4021;
    moonbit_incref(_M0L4dataS3396);
    #line 639 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1470
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3396, _M0L3keyS1464);
    moonbit_decref(_M0L4dataS3396);
    if (_M0L7_2abindS1470 == 0) {
      if (_M0L7_2abindS1470) {
        moonbit_decref(_M0L7_2abindS1470);
      }
      goto join_1466;
    } else {
      void* _M0L7_2aSomeS1471 = _M0L7_2abindS1470;
      void* _M0L4_2axS1472 = _M0L7_2aSomeS1471;
      switch (Moonbit_object_tag(_M0L4_2axS1472)) {
        case 1: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1473 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1472;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3548 = _M0L7_2aHashS1473->$0;
          int32_t _M0L6_2acntS3922 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1473));
          struct _M0TPB3MapGssE* _M0L4_2ahS1474;
          if (_M0L6_2acntS3922 > 1) {
            int32_t _M0L11_2anew__cntS3923 = _M0L6_2acntS3922 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1473), _M0L11_2anew__cntS3923);
            moonbit_incref(_M0L8_2afieldS3548);
          } else if (_M0L6_2acntS3922 == 1) {
            #line 639 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1473);
          }
          _M0L4_2ahS1474 = _M0L8_2afieldS3548;
          _M0L1hS1469 = _M0L4_2ahS1474;
          goto join_1468;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1472);
          goto join_1466;
          break;
        }
      }
    }
    join_1468:;
    return _M0L1hS1469;
    join_1466:;
    _M0L7_2abindS1467 = (struct _M0TUssE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3395 = _M0L7_2abindS1467;
    _M0L6_2atmpS3394
    = (struct _M0TPB9ArrayViewGUssEE){
      .$0 = _M0L6_2atmpS3395, .$1 = 0, .$2 = 0
    };
    #line 641 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_4021 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3394, 0ll);
    moonbit_decref(_M0L6_2atmpS3394.$0);
    return _result_4021;
  }
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database4hget(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1452,
  moonbit_string_t _M0L3keyS1453,
  moonbit_string_t _M0L5fieldS1457
) {
  #line 606 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1452, _M0L3keyS1453)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1456;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3391 =
      _M0L4selfS1452->$0;
    void* _M0L7_2abindS1458;
    moonbit_string_t _result_4024;
    moonbit_incref(_M0L4dataS3391);
    #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1458
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3391, _M0L3keyS1453);
    moonbit_decref(_M0L4dataS3391);
    if (_M0L7_2abindS1458 == 0) {
      if (_M0L7_2abindS1458) {
        moonbit_decref(_M0L7_2abindS1458);
      }
      goto join_1454;
    } else {
      void* _M0L7_2aSomeS1459 = _M0L7_2abindS1458;
      void* _M0L4_2axS1460 = _M0L7_2aSomeS1459;
      switch (Moonbit_object_tag(_M0L4_2axS1460)) {
        case 1: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1461 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1460;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3550 = _M0L7_2aHashS1461->$0;
          int32_t _M0L6_2acntS3924 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1461));
          struct _M0TPB3MapGssE* _M0L4_2ahS1462;
          if (_M0L6_2acntS3924 > 1) {
            int32_t _M0L11_2anew__cntS3925 = _M0L6_2acntS3924 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1461), _M0L11_2anew__cntS3925);
            moonbit_incref(_M0L8_2afieldS3550);
          } else if (_M0L6_2acntS3924 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L7_2aHashS1461);
          }
          _M0L4_2ahS1462 = _M0L8_2afieldS3550;
          _M0L1hS1456 = _M0L4_2ahS1462;
          goto join_1455;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1460);
          goto join_1454;
          break;
        }
      }
    }
    join_1455:;
    #line 611 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_4024 = _M0MPB3Map3getGssE(_M0L1hS1456, _M0L5fieldS1457);
    moonbit_decref(_M0L1hS1456);
    return _result_4024;
    join_1454:;
    return 0;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4hset(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1438,
  moonbit_string_t _M0L3keyS1439,
  moonbit_string_t _M0L5fieldS1450,
  moonbit_string_t _M0L5valueS1451
) {
  int32_t _M0L6_2atmpS3385;
  struct _M0TPB3MapGssE* _M0L4hashS1440;
  struct _M0TPB3MapGssE* _M0L1hS1444;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3390;
  void* _M0L7_2abindS1445;
  struct _M0TUssE** _M0L7_2abindS1442;
  struct _M0TUssE** _M0L6_2atmpS3389;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3388;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3386;
  void* _M0L4HashS3387;
  #line 596 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3385
  = _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1438, _M0L3keyS1439);
  _M0L4dataS3390 = _M0L4selfS1438->$0;
  moonbit_incref(_M0L4dataS3390);
  #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1445
  = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3390, _M0L3keyS1439);
  moonbit_decref(_M0L4dataS3390);
  if (_M0L7_2abindS1445 == 0) {
    if (_M0L7_2abindS1445) {
      moonbit_decref(_M0L7_2abindS1445);
    }
    goto join_1441;
  } else {
    void* _M0L7_2aSomeS1446 = _M0L7_2abindS1445;
    void* _M0L4_2axS1447 = _M0L7_2aSomeS1446;
    switch (Moonbit_object_tag(_M0L4_2axS1447)) {
      case 1: {
        struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash* _M0L7_2aHashS1448 =
          (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash*)_M0L4_2axS1447;
        struct _M0TPB3MapGssE* _M0L8_2afieldS3553 = _M0L7_2aHashS1448->$0;
        int32_t _M0L6_2acntS3926 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1448));
        struct _M0TPB3MapGssE* _M0L4_2ahS1449;
        if (_M0L6_2acntS3926 > 1) {
          int32_t _M0L11_2anew__cntS3927 = _M0L6_2acntS3926 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1448), _M0L11_2anew__cntS3927);
          moonbit_incref(_M0L8_2afieldS3553);
        } else if (_M0L6_2acntS3926 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L7_2aHashS1448);
        }
        _M0L4_2ahS1449 = _M0L8_2afieldS3553;
        _M0L1hS1444 = _M0L4_2ahS1449;
        goto join_1443;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1447);
        goto join_1441;
        break;
      }
    }
  }
  goto joinlet_4026;
  join_1443:;
  _M0L4hashS1440 = _M0L1hS1444;
  joinlet_4026:;
  goto joinlet_4025;
  join_1441:;
  _M0L7_2abindS1442 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3389 = _M0L7_2abindS1442;
  _M0L6_2atmpS3388
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS3389, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4hashS1440 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3388, 10ll);
  moonbit_decref(_M0L6_2atmpS3388.$0);
  joinlet_4025:;
  #line 602 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1440, _M0L5fieldS1450, _M0L5valueS1451);
  _M0L4dataS3386 = _M0L4selfS1438->$0;
  _M0L4HashS3387
  = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS3387)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 18, 1);
  ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue4Hash*)_M0L4HashS3387)->$0
  = _M0L4hashS1440;
  moonbit_incref(_M0L4dataS3386);
  #line 603 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3386, _M0L3keyS1439, _M0L4HashS3387);
  moonbit_decref(_M0L4dataS3386);
  moonbit_decref(_M0L4HashS3387);
  return 0;
}

int64_t _M0MP39moonbitdb9moonbitdb3lib8Database4decr(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1422,
  moonbit_string_t _M0L3keyS1423
) {
  int32_t _M0L6_2atmpS3378;
  moonbit_string_t _M0L1sS1426;
  moonbit_string_t _M0L7currentS1424;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3384;
  void* _M0L7_2abindS1427;
  int32_t _M0L1nS1433;
  int64_t _M0L7_2abindS1435;
  int32_t _M0L6_2atmpS3383;
  moonbit_string_t _M0L8new__valS1434;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3379;
  void* _M0L6StringS3380;
  struct _M0TPB3MapGsiE* _M0L7expiresS3381;
  int32_t _M0L6_2atmpS3382;
  #line 579 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 580 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3378
  = _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1422, _M0L3keyS1423);
  _M0L4dataS3384 = _M0L4selfS1422->$0;
  moonbit_incref(_M0L4dataS3384);
  #line 581 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1427
  = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3384, _M0L3keyS1423);
  moonbit_decref(_M0L4dataS3384);
  if (_M0L7_2abindS1427 == 0) {
    if (_M0L7_2abindS1427) {
      moonbit_decref(_M0L7_2abindS1427);
    }
    _M0L7currentS1424 = (moonbit_string_t)moonbit_string_literal_3.data;
  } else {
    void* _M0L7_2aSomeS1428 = _M0L7_2abindS1427;
    void* _M0L4_2axS1429 = _M0L7_2aSomeS1428;
    switch (Moonbit_object_tag(_M0L4_2axS1429)) {
      case 0: {
        struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1430 =
          (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L4_2axS1429;
        moonbit_string_t _M0L8_2afieldS3557 = _M0L9_2aStringS1430->$0;
        int32_t _M0L6_2acntS3928 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1430));
        moonbit_string_t _M0L4_2asS1431;
        if (_M0L6_2acntS3928 > 1) {
          int32_t _M0L11_2anew__cntS3929 = _M0L6_2acntS3928 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1430), _M0L11_2anew__cntS3929);
          moonbit_incref(_M0L8_2afieldS3557);
        } else if (_M0L6_2acntS3928 == 1) {
          #line 581 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L9_2aStringS1430);
        }
        _M0L4_2asS1431 = _M0L8_2afieldS3557;
        _M0L1sS1426 = _M0L4_2asS1431;
        goto join_1425;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1429);
        _M0L7currentS1424 = (moonbit_string_t)moonbit_string_literal_3.data;
        break;
      }
    }
  }
  goto joinlet_4027;
  join_1425:;
  _M0L7currentS1424 = _M0L1sS1426;
  joinlet_4027:;
  #line 585 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1435
  = _M0FP39moonbitdb9moonbitdb3lib10parse__int(_M0L7currentS1424);
  moonbit_decref(_M0L7currentS1424);
  if (_M0L7_2abindS1435 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1436 = _M0L7_2abindS1435;
    int32_t _M0L4_2anS1437 = (int32_t)_M0L7_2aSomeS1436;
    _M0L1nS1433 = _M0L4_2anS1437;
    goto join_1432;
  }
  join_1432:;
  _M0L6_2atmpS3383 = _M0L1nS1433 - 1;
  #line 587 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L8new__valS1434
  = _M0FP39moonbitdb9moonbitdb3lib15int__to__string(_M0L6_2atmpS3383);
  _M0L4dataS3379 = _M0L4selfS1422->$0;
  _M0L6StringS3380
  = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3380)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L6StringS3380)->$0
  = _M0L8new__valS1434;
  moonbit_incref(_M0L4dataS3379);
  #line 588 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3379, _M0L3keyS1423, _M0L6StringS3380);
  moonbit_decref(_M0L4dataS3379);
  moonbit_decref(_M0L6StringS3380);
  _M0L7expiresS3381 = _M0L4selfS1422->$1;
  moonbit_incref(_M0L7expiresS3381);
  #line 589 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3381, _M0L3keyS1423);
  moonbit_decref(_M0L7expiresS3381);
  _M0L6_2atmpS3382 = _M0L1nS1433 - 1;
  return (int64_t)_M0L6_2atmpS3382;
}

int64_t _M0MP39moonbitdb9moonbitdb3lib8Database4incr(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1406,
  moonbit_string_t _M0L3keyS1407
) {
  int32_t _M0L6_2atmpS3371;
  moonbit_string_t _M0L1sS1410;
  moonbit_string_t _M0L7currentS1408;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3377;
  void* _M0L7_2abindS1411;
  int32_t _M0L1nS1417;
  int64_t _M0L7_2abindS1419;
  int32_t _M0L6_2atmpS3376;
  moonbit_string_t _M0L8new__valS1418;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3372;
  void* _M0L6StringS3373;
  struct _M0TPB3MapGsiE* _M0L7expiresS3374;
  int32_t _M0L6_2atmpS3375;
  #line 562 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 563 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3371
  = _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1406, _M0L3keyS1407);
  _M0L4dataS3377 = _M0L4selfS1406->$0;
  moonbit_incref(_M0L4dataS3377);
  #line 564 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1411
  = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3377, _M0L3keyS1407);
  moonbit_decref(_M0L4dataS3377);
  if (_M0L7_2abindS1411 == 0) {
    if (_M0L7_2abindS1411) {
      moonbit_decref(_M0L7_2abindS1411);
    }
    _M0L7currentS1408 = (moonbit_string_t)moonbit_string_literal_3.data;
  } else {
    void* _M0L7_2aSomeS1412 = _M0L7_2abindS1411;
    void* _M0L4_2axS1413 = _M0L7_2aSomeS1412;
    switch (Moonbit_object_tag(_M0L4_2axS1413)) {
      case 0: {
        struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1414 =
          (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L4_2axS1413;
        moonbit_string_t _M0L8_2afieldS3561 = _M0L9_2aStringS1414->$0;
        int32_t _M0L6_2acntS3930 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1414));
        moonbit_string_t _M0L4_2asS1415;
        if (_M0L6_2acntS3930 > 1) {
          int32_t _M0L11_2anew__cntS3931 = _M0L6_2acntS3930 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1414), _M0L11_2anew__cntS3931);
          moonbit_incref(_M0L8_2afieldS3561);
        } else if (_M0L6_2acntS3930 == 1) {
          #line 564 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_free(_M0L9_2aStringS1414);
        }
        _M0L4_2asS1415 = _M0L8_2afieldS3561;
        _M0L1sS1410 = _M0L4_2asS1415;
        goto join_1409;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1413);
        _M0L7currentS1408 = (moonbit_string_t)moonbit_string_literal_3.data;
        break;
      }
    }
  }
  goto joinlet_4029;
  join_1409:;
  _M0L7currentS1408 = _M0L1sS1410;
  joinlet_4029:;
  #line 568 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1419
  = _M0FP39moonbitdb9moonbitdb3lib10parse__int(_M0L7currentS1408);
  moonbit_decref(_M0L7currentS1408);
  if (_M0L7_2abindS1419 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1420 = _M0L7_2abindS1419;
    int32_t _M0L4_2anS1421 = (int32_t)_M0L7_2aSomeS1420;
    _M0L1nS1417 = _M0L4_2anS1421;
    goto join_1416;
  }
  join_1416:;
  _M0L6_2atmpS3376 = _M0L1nS1417 + 1;
  #line 570 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L8new__valS1418
  = _M0FP39moonbitdb9moonbitdb3lib15int__to__string(_M0L6_2atmpS3376);
  _M0L4dataS3372 = _M0L4selfS1406->$0;
  _M0L6StringS3373
  = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3373)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L6StringS3373)->$0
  = _M0L8new__valS1418;
  moonbit_incref(_M0L4dataS3372);
  #line 571 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3372, _M0L3keyS1407, _M0L6StringS3373);
  moonbit_decref(_M0L4dataS3372);
  moonbit_decref(_M0L6StringS3373);
  _M0L7expiresS3374 = _M0L4selfS1406->$1;
  moonbit_incref(_M0L7expiresS3374);
  #line 572 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3374, _M0L3keyS1407);
  moonbit_decref(_M0L7expiresS3374);
  _M0L6_2atmpS3375 = _M0L1nS1417 + 1;
  return (int64_t)_M0L6_2atmpS3375;
}

moonbit_string_t _M0FP39moonbitdb9moonbitdb3lib15int__to__string(
  int32_t _M0L1nS1397
) {
  #line 526 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (_M0L1nS1397 == 0) {
    return (moonbit_string_t)moonbit_string_literal_3.data;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L3numS1398 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    moonbit_string_t* _M0L6_2atmpS3370;
    struct _M0TPB5ArrayGsE* _M0L5charsS1399;
    int32_t _M0L3valS3356;
    moonbit_string_t* _M0L6_2atmpS3369;
    struct _M0TPB5ArrayGsE* _M0L6resultS1402;
    int32_t _M0L6_2atmpS3366;
    int32_t _M0L6_2atmpS3365;
    int32_t _M0L1iS1403;
    moonbit_string_t _M0L7_2abindS1405;
    int32_t _M0L6_2atmpS3368;
    struct _M0TPC16string10StringView _M0L6_2atmpS3367;
    moonbit_string_t _result_4033;
    Moonbit_object_header(_M0L3numS1398)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L3numS1398->$0 = _M0L1nS1397;
    _M0L6_2atmpS3370 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L5charsS1399
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L5charsS1399)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L5charsS1399->$0 = _M0L6_2atmpS3370;
    _M0L5charsS1399->$1 = 0;
    _M0L3valS3356 = _M0L3numS1398->$0;
    if (_M0L3valS3356 < 0) {
      int32_t _M0L3valS3358 = _M0L3numS1398->$0;
      int32_t _M0L6_2atmpS3357 = -_M0L3valS3358;
      _M0L3numS1398->$0 = _M0L6_2atmpS3357;
    }
    while (1) {
      int32_t _M0L3valS3359 = _M0L3numS1398->$0;
      if (_M0L3valS3359 > 0) {
        int32_t _M0L3valS3360 = _M0L3numS1398->$0;
        int32_t _M0L7_2abindS1400 = _M0L3valS3360 % 10;
        int32_t _M0L3valS3362;
        int32_t _M0L6_2atmpS3361;
        switch (_M0L7_2abindS1400) {
          case 0: {
            #line 537 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_3.data);
            break;
          }
          
          case 1: {
            #line 538 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_4.data);
            break;
          }
          
          case 2: {
            #line 539 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_5.data);
            break;
          }
          
          case 3: {
            #line 540 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_6.data);
            break;
          }
          
          case 4: {
            #line 541 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_7.data);
            break;
          }
          
          case 5: {
            #line 542 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_8.data);
            break;
          }
          
          case 6: {
            #line 543 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_9.data);
            break;
          }
          
          case 7: {
            #line 544 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_10.data);
            break;
          }
          
          case 8: {
            #line 545 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_11.data);
            break;
          }
          
          case 9: {
            #line 546 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_12.data);
            break;
          }
          default: {
            #line 547 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_3.data);
            break;
          }
        }
        _M0L3valS3362 = _M0L3numS1398->$0;
        _M0L6_2atmpS3361 = _M0L3valS3362 / 10;
        _M0L3numS1398->$0 = _M0L6_2atmpS3361;
        continue;
      } else {
        moonbit_decref(_M0L3numS1398);
      }
      break;
    }
    if (_M0L1nS1397 < 0) {
      #line 552 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5charsS1399, (moonbit_string_t)moonbit_string_literal_13.data);
    }
    _M0L6_2atmpS3369 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1402
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1402)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L6resultS1402->$0 = _M0L6_2atmpS3369;
    _M0L6resultS1402->$1 = 0;
    #line 555 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3366 = _M0MPC15array5Array6lengthGsE(_M0L5charsS1399);
    _M0L6_2atmpS3365 = _M0L6_2atmpS3366 - 1;
    _M0L1iS1403 = _M0L6_2atmpS3365;
    while (1) {
      if (_M0L1iS1403 >= 0) {
        moonbit_string_t _M0L6_2atmpS3363;
        int32_t _M0L6_2atmpS3364;
        #line 556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L6_2atmpS3363
        = _M0MPC15array5Array2atGsE(_M0L5charsS1399, _M0L1iS1403);
        #line 556 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1402, _M0L6_2atmpS3363);
        moonbit_decref(_M0L6_2atmpS3363);
        _M0L6_2atmpS3364 = _M0L1iS1403 - 1;
        _M0L1iS1403 = _M0L6_2atmpS3364;
        continue;
      } else {
        moonbit_decref(_M0L5charsS1399);
      }
      break;
    }
    _M0L7_2abindS1405 = (moonbit_string_t)moonbit_string_literal_14.data;
    _M0L6_2atmpS3368 = Moonbit_array_length(_M0L7_2abindS1405);
    _M0L6_2atmpS3367
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS1405, .$1 = 0, .$2 = _M0L6_2atmpS3368
    };
    #line 558 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_4033
    = _M0MPC15array5Array4joinGsE(_M0L6resultS1402, _M0L6_2atmpS3367);
    moonbit_decref(_M0L6resultS1402);
    moonbit_decref(_M0L6_2atmpS3367.$0);
    return _result_4033;
  }
}

int64_t _M0FP39moonbitdb9moonbitdb3lib10parse__int(
  moonbit_string_t _M0L1sS1386
) {
  int32_t _M0L6_2atmpS3343;
  #line 503 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3343 = Moonbit_array_length(_M0L1sS1386);
  if (_M0L6_2atmpS3343 == 0) {
    return 4294967296ll;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L6resultS1387 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    struct _M0TPB8MutLocalGiE* _M0L4signS1388;
    struct _M0TPB8MutLocalGiE* _M0L5startS1389;
    int32_t _M0L6_2atmpS3344;
    int32_t _M0L3valS3352;
    int32_t _M0L1iS1390;
    int32_t _M0L3valS3354;
    int32_t _M0L3valS3355;
    int32_t _M0L6_2atmpS3353;
    Moonbit_object_header(_M0L6resultS1387)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L6resultS1387->$0 = 0;
    _M0L4signS1388
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L4signS1388)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L4signS1388->$0 = 1;
    _M0L5startS1389
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L5startS1389)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L5startS1389->$0 = 0;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1386)) {
      #line 510 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3344 = _M0L1sS1386[0];
    if (_M0L6_2atmpS3344 == 45) {
      _M0L4signS1388->$0 = -1;
      _M0L5startS1389->$0 = 1;
    } else {
      int32_t _M0L6_2atmpS3345;
      if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1386)) {
        #line 513 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3345 = _M0L1sS1386[0];
      if (_M0L6_2atmpS3345 == 43) {
        _M0L5startS1389->$0 = 1;
      }
    }
    _M0L3valS3352 = _M0L5startS1389->$0;
    moonbit_decref(_M0L5startS1389);
    _M0L1iS1390 = _M0L3valS3352;
    while (1) {
      int32_t _M0L6_2atmpS3346 = Moonbit_array_length(_M0L1sS1386);
      if (_M0L1iS1390 < _M0L6_2atmpS3346) {
        int32_t _M0L5digitS1392;
        int32_t _M0L6_2atmpS3350;
        int64_t _M0L7_2abindS1393;
        int32_t _M0L3valS3349;
        int32_t _M0L6_2atmpS3348;
        int32_t _M0L6_2atmpS3347;
        int32_t _M0L6_2atmpS3351;
        if (
          _M0L1iS1390 < 0 || _M0L1iS1390 >= Moonbit_array_length(_M0L1sS1386)
        ) {
          #line 517 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3350 = _M0L1sS1386[_M0L1iS1390];
        #line 517 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1393
        = _M0FP39moonbitdb9moonbitdb3lib17uint16__to__digit(_M0L6_2atmpS3350);
        if (_M0L7_2abindS1393 == 4294967296ll) {
          moonbit_decref(_M0L4signS1388);
          moonbit_decref(_M0L6resultS1387);
          return 4294967296ll;
        } else {
          int64_t _M0L7_2aSomeS1394 = _M0L7_2abindS1393;
          int32_t _M0L8_2adigitS1395 = (int32_t)_M0L7_2aSomeS1394;
          _M0L5digitS1392 = _M0L8_2adigitS1395;
          goto join_1391;
        }
        goto joinlet_4035;
        join_1391:;
        _M0L3valS3349 = _M0L6resultS1387->$0;
        _M0L6_2atmpS3348 = _M0L3valS3349 * 10;
        _M0L6_2atmpS3347 = _M0L6_2atmpS3348 + _M0L5digitS1392;
        _M0L6resultS1387->$0 = _M0L6_2atmpS3347;
        joinlet_4035:;
        _M0L6_2atmpS3351 = _M0L1iS1390 + 1;
        _M0L1iS1390 = _M0L6_2atmpS3351;
        continue;
      }
      break;
    }
    _M0L3valS3354 = _M0L6resultS1387->$0;
    moonbit_decref(_M0L6resultS1387);
    _M0L3valS3355 = _M0L4signS1388->$0;
    moonbit_decref(_M0L4signS1388);
    _M0L6_2atmpS3353 = _M0L3valS3354 * _M0L3valS3355;
    return (int64_t)_M0L6_2atmpS3353;
  }
}

int64_t _M0FP39moonbitdb9moonbitdb3lib17uint16__to__digit(
  int32_t _M0L1cS1385
) {
  #line 471 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  switch (_M0L1cS1385) {
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

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6strlen(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1376,
  moonbit_string_t _M0L3keyS1377
) {
  #line 460 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 461 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1376, _M0L3keyS1377)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1379;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3342 =
      _M0L4selfS1376->$0;
    void* _M0L7_2abindS1380;
    int32_t _result_4037;
    moonbit_incref(_M0L4dataS3342);
    #line 464 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1380
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3342, _M0L3keyS1377);
    moonbit_decref(_M0L4dataS3342);
    if (_M0L7_2abindS1380 == 0) {
      if (_M0L7_2abindS1380) {
        moonbit_decref(_M0L7_2abindS1380);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1381 = _M0L7_2abindS1380;
      void* _M0L4_2axS1382 = _M0L7_2aSomeS1381;
      switch (Moonbit_object_tag(_M0L4_2axS1382)) {
        case 0: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1383 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L4_2axS1382;
          moonbit_string_t _M0L8_2afieldS3563 = _M0L9_2aStringS1383->$0;
          int32_t _M0L6_2acntS3932 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1383));
          moonbit_string_t _M0L4_2asS1384;
          if (_M0L6_2acntS3932 > 1) {
            int32_t _M0L11_2anew__cntS3933 = _M0L6_2acntS3932 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1383), _M0L11_2anew__cntS3933);
            moonbit_incref(_M0L8_2afieldS3563);
          } else if (_M0L6_2acntS3932 == 1) {
            #line 464 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1383);
          }
          _M0L4_2asS1384 = _M0L8_2afieldS3563;
          _M0L1sS1379 = _M0L4_2asS1384;
          goto join_1378;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1382);
          return 0;
          break;
        }
      }
    }
    join_1378:;
    _result_4037 = Moonbit_array_length(_M0L1sS1379);
    moonbit_decref(_M0L1sS1379);
    return _result_4037;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6append(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1363,
  moonbit_string_t _M0L3keyS1364,
  moonbit_string_t _M0L5valueS1366
) {
  #line 444 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 445 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1363, _M0L3keyS1364)
  ) {
    moonbit_string_t _M0L8new__valS1365 = _M0L5valueS1366;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3337 =
      _M0L4selfS1363->$0;
    void* _M0L6StringS3338;
    moonbit_incref(_M0L8new__valS1365);
    _M0L6StringS3338
    = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String));
    Moonbit_object_header(_M0L6StringS3338)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
    ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L6StringS3338)->$0
    = _M0L8new__valS1365;
    moonbit_incref(_M0L4dataS3337);
    #line 447 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3337, _M0L3keyS1364, _M0L6StringS3338);
    moonbit_decref(_M0L4dataS3337);
    moonbit_decref(_M0L6StringS3338);
    return Moonbit_array_length(_M0L8new__valS1365);
  } else {
    moonbit_string_t _M0L1sS1369;
    moonbit_string_t _M0L7currentS1367;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3341 =
      _M0L4selfS1363->$0;
    void* _M0L7_2abindS1370;
    moonbit_string_t _M0L8new__valS1375;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3339;
    void* _M0L6StringS3340;
    int32_t _result_4039;
    moonbit_incref(_M0L4dataS3341);
    #line 450 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1370
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3341, _M0L3keyS1364);
    moonbit_decref(_M0L4dataS3341);
    if (_M0L7_2abindS1370 == 0) {
      if (_M0L7_2abindS1370) {
        moonbit_decref(_M0L7_2abindS1370);
      }
      _M0L7currentS1367 = (moonbit_string_t)moonbit_string_literal_14.data;
    } else {
      void* _M0L7_2aSomeS1371 = _M0L7_2abindS1370;
      void* _M0L4_2axS1372 = _M0L7_2aSomeS1371;
      switch (Moonbit_object_tag(_M0L4_2axS1372)) {
        case 0: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1373 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L4_2axS1372;
          moonbit_string_t _M0L8_2afieldS3567 = _M0L9_2aStringS1373->$0;
          int32_t _M0L6_2acntS3934 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1373));
          moonbit_string_t _M0L4_2asS1374;
          if (_M0L6_2acntS3934 > 1) {
            int32_t _M0L11_2anew__cntS3935 = _M0L6_2acntS3934 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1373), _M0L11_2anew__cntS3935);
            moonbit_incref(_M0L8_2afieldS3567);
          } else if (_M0L6_2acntS3934 == 1) {
            #line 450 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1373);
          }
          _M0L4_2asS1374 = _M0L8_2afieldS3567;
          _M0L1sS1369 = _M0L4_2asS1374;
          goto join_1368;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1372);
          _M0L7currentS1367
          = (moonbit_string_t)moonbit_string_literal_14.data;
          break;
        }
      }
    }
    goto joinlet_4038;
    join_1368:;
    _M0L7currentS1367 = _M0L1sS1369;
    joinlet_4038:;
    #line 454 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L8new__valS1375
    = moonbit_add_string(_M0L7currentS1367, _M0L5valueS1366);
    moonbit_decref(_M0L7currentS1367);
    _M0L4dataS3339 = _M0L4selfS1363->$0;
    moonbit_incref(_M0L8new__valS1375);
    _M0L6StringS3340
    = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String));
    Moonbit_object_header(_M0L6StringS3340)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
    ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L6StringS3340)->$0
    = _M0L8new__valS1375;
    moonbit_incref(_M0L4dataS3339);
    #line 455 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3339, _M0L3keyS1364, _M0L6StringS3340);
    moonbit_decref(_M0L4dataS3339);
    moonbit_decref(_M0L6StringS3340);
    _result_4039 = Moonbit_array_length(_M0L8new__valS1375);
    moonbit_decref(_M0L8new__valS1375);
    return _result_4039;
  }
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database8type__of(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1358,
  moonbit_string_t _M0L3keyS1359
) {
  #line 429 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 430 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1358, _M0L3keyS1359)
  ) {
    return (moonbit_string_t)moonbit_string_literal_15.data;
  } else {
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3336 =
      _M0L4selfS1358->$0;
    void* _M0L7_2abindS1360;
    moonbit_incref(_M0L4dataS3336);
    #line 433 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1360
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3336, _M0L3keyS1359);
    moonbit_decref(_M0L4dataS3336);
    if (_M0L7_2abindS1360 == 0) {
      if (_M0L7_2abindS1360) {
        moonbit_decref(_M0L7_2abindS1360);
      }
      return (moonbit_string_t)moonbit_string_literal_15.data;
    } else {
      void* _M0L7_2aSomeS1361 = _M0L7_2abindS1360;
      void* _M0L4_2axS1362 = _M0L7_2aSomeS1361;
      switch (Moonbit_object_tag(_M0L4_2axS1362)) {
        case 0: {
          moonbit_decref(_M0L4_2axS1362);
          return (moonbit_string_t)moonbit_string_literal_16.data;
          break;
        }
        
        case 1: {
          moonbit_decref(_M0L4_2axS1362);
          return (moonbit_string_t)moonbit_string_literal_17.data;
          break;
        }
        
        case 2: {
          moonbit_decref(_M0L4_2axS1362);
          return (moonbit_string_t)moonbit_string_literal_18.data;
          break;
        }
        
        case 3: {
          moonbit_decref(_M0L4_2axS1362);
          return (moonbit_string_t)moonbit_string_literal_19.data;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1362);
          return (moonbit_string_t)moonbit_string_literal_20.data;
          break;
        }
      }
    }
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6exists(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1356,
  moonbit_string_t _M0L3keyS1357
) {
  #line 369 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 370 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1356, _M0L3keyS1357)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3335 =
      _M0L4selfS1356->$0;
    int32_t _result_4040;
    moonbit_incref(_M0L4dataS3335);
    #line 373 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_4040
    = _M0MPB3Map8containsGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3335, _M0L3keyS1357);
    moonbit_decref(_M0L4dataS3335);
    return _result_4040;
  }
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib8Database3get(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1346,
  moonbit_string_t _M0L3keyS1347
) {
  #line 345 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 346 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  if (
    _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1346, _M0L3keyS1347)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1350;
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3334 =
      _M0L4selfS1346->$0;
    void* _M0L7_2abindS1351;
    moonbit_incref(_M0L4dataS3334);
    #line 349 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L7_2abindS1351
    = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3334, _M0L3keyS1347);
    moonbit_decref(_M0L4dataS3334);
    if (_M0L7_2abindS1351 == 0) {
      if (_M0L7_2abindS1351) {
        moonbit_decref(_M0L7_2abindS1351);
      }
      goto join_1348;
    } else {
      void* _M0L7_2aSomeS1352 = _M0L7_2abindS1351;
      void* _M0L4_2axS1353 = _M0L7_2aSomeS1352;
      switch (Moonbit_object_tag(_M0L4_2axS1353)) {
        case 0: {
          struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1354 =
            (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L4_2axS1353;
          moonbit_string_t _M0L8_2afieldS3571 = _M0L9_2aStringS1354->$0;
          int32_t _M0L6_2acntS3936 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1354));
          moonbit_string_t _M0L4_2asS1355;
          if (_M0L6_2acntS3936 > 1) {
            int32_t _M0L11_2anew__cntS3937 = _M0L6_2acntS3936 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1354), _M0L11_2anew__cntS3937);
            moonbit_incref(_M0L8_2afieldS3571);
          } else if (_M0L6_2acntS3936 == 1) {
            #line 349 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
            moonbit_free(_M0L9_2aStringS1354);
          }
          _M0L4_2asS1355 = _M0L8_2afieldS3571;
          _M0L1sS1350 = _M0L4_2asS1355;
          goto join_1349;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1353);
          goto join_1348;
          break;
        }
      }
    }
    join_1349:;
    return _M0L1sS1350;
    join_1348:;
    return 0;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4mdel(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1343,
  struct _M0TPB5ArrayGsE* _M0L4keysS1340
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1338;
  int32_t _M0L7_2abindS1339;
  int32_t _M0L2__S1341;
  int32_t _result_4044;
  #line 291 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5countS1338
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1338)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1338->$0 = 0;
  _M0L7_2abindS1339 = _M0L4keysS1340->$1;
  _M0L2__S1341 = 0;
  while (1) {
    if (_M0L2__S1341 < _M0L7_2abindS1339) {
      moonbit_string_t* _M0L3bufS3333 = _M0L4keysS1340->$0;
      moonbit_string_t _M0L3keyS1342 =
        (moonbit_string_t)_M0L3bufS3333[_M0L2__S1341];
      int32_t _M0L6_2atmpS3326;
      int32_t _M0L6_2atmpS3332;
      moonbit_incref(_M0L3keyS1342);
      #line 294 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3326
      = _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1343, _M0L3keyS1342);
      if (!_M0L6_2atmpS3326) {
        struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3331 =
          _M0L4selfS1343->$0;
        int32_t _M0L7existedS1344;
        moonbit_incref(_M0L4dataS3331);
        #line 295 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7existedS1344
        = _M0MPB3Map8containsGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3331, _M0L3keyS1342);
        moonbit_decref(_M0L4dataS3331);
        if (_M0L7existedS1344) {
          struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3327 =
            _M0L4selfS1343->$0;
          struct _M0TPB3MapGsiE* _M0L7expiresS3328;
          int32_t _M0L3valS3330;
          int32_t _M0L6_2atmpS3329;
          moonbit_incref(_M0L4dataS3327);
          #line 297 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPB3Map6removeGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3327, _M0L3keyS1342);
          moonbit_decref(_M0L4dataS3327);
          _M0L7expiresS3328 = _M0L4selfS1343->$1;
          moonbit_incref(_M0L7expiresS3328);
          #line 298 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
          _M0MPB3Map6removeGsiE(_M0L7expiresS3328, _M0L3keyS1342);
          moonbit_decref(_M0L7expiresS3328);
          moonbit_decref(_M0L3keyS1342);
          _M0L3valS3330 = _M0L5countS1338->$0;
          _M0L6_2atmpS3329 = _M0L3valS3330 + 1;
          _M0L5countS1338->$0 = _M0L6_2atmpS3329;
        } else {
          moonbit_decref(_M0L3keyS1342);
        }
      } else {
        moonbit_decref(_M0L3keyS1342);
      }
      _M0L6_2atmpS3332 = _M0L2__S1341 + 1;
      _M0L2__S1341 = _M0L6_2atmpS3332;
      continue;
    }
    break;
  }
  _result_4044 = _M0L5countS1338->$0;
  moonbit_decref(_M0L5countS1338);
  return _result_4044;
}

struct _M0TPB5ArrayGOsE* _M0MP39moonbitdb9moonbitdb3lib8Database4mget(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1328,
  struct _M0TPB5ArrayGsE* _M0L4keysS1325
) {
  moonbit_string_t* _M0L6_2atmpS3325;
  struct _M0TPB5ArrayGOsE* _M0L6resultS1323;
  int32_t _M0L7_2abindS1324;
  int32_t _M0L2__S1326;
  #line 276 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3325 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1323
  = (struct _M0TPB5ArrayGOsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGOsE));
  Moonbit_object_header(_M0L6resultS1323)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 24, 0);
  _M0L6resultS1323->$0 = _M0L6_2atmpS3325;
  _M0L6resultS1323->$1 = 0;
  _M0L7_2abindS1324 = _M0L4keysS1325->$1;
  _M0L2__S1326 = 0;
  while (1) {
    if (_M0L2__S1326 < _M0L7_2abindS1324) {
      moonbit_string_t* _M0L3bufS3324 = _M0L4keysS1325->$0;
      moonbit_string_t _M0L3keyS1327 =
        (moonbit_string_t)_M0L3bufS3324[_M0L2__S1326];
      int32_t _M0L6_2atmpS3323;
      moonbit_incref(_M0L3keyS1327);
      #line 279 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      if (
        _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(_M0L4selfS1328, _M0L3keyS1327)
      ) {
        moonbit_string_t _M0L6_2atmpS3319;
        moonbit_decref(_M0L3keyS1327);
        _M0L6_2atmpS3319 = 0;
        #line 280 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1323, _M0L6_2atmpS3319);
        if (_M0L6_2atmpS3319) {
          moonbit_decref(_M0L6_2atmpS3319);
        }
      } else {
        moonbit_string_t _M0L1sS1331;
        struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3322 =
          _M0L4selfS1328->$0;
        void* _M0L7_2abindS1332;
        moonbit_string_t _M0L6_2atmpS3321;
        moonbit_string_t _M0L6_2atmpS3320;
        moonbit_incref(_M0L4dataS3322);
        #line 282 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L7_2abindS1332
        = _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3322, _M0L3keyS1327);
        moonbit_decref(_M0L4dataS3322);
        moonbit_decref(_M0L3keyS1327);
        if (_M0L7_2abindS1332 == 0) {
          if (_M0L7_2abindS1332) {
            moonbit_decref(_M0L7_2abindS1332);
          }
          goto join_1329;
        } else {
          void* _M0L7_2aSomeS1333 = _M0L7_2abindS1332;
          void* _M0L4_2axS1334 = _M0L7_2aSomeS1333;
          switch (Moonbit_object_tag(_M0L4_2axS1334)) {
            case 0: {
              struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String* _M0L9_2aStringS1335 =
                (struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L4_2axS1334;
              moonbit_string_t _M0L8_2afieldS3578 = _M0L9_2aStringS1335->$0;
              int32_t _M0L6_2acntS3938 =
                Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1335));
              moonbit_string_t _M0L4_2asS1336;
              if (_M0L6_2acntS3938 > 1) {
                int32_t _M0L11_2anew__cntS3939 = _M0L6_2acntS3938 - 1;
                Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1335), _M0L11_2anew__cntS3939);
                moonbit_incref(_M0L8_2afieldS3578);
              } else if (_M0L6_2acntS3938 == 1) {
                #line 282 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
                moonbit_free(_M0L9_2aStringS1335);
              }
              _M0L4_2asS1336 = _M0L8_2afieldS3578;
              _M0L1sS1331 = _M0L4_2asS1336;
              goto join_1330;
              break;
            }
            default: {
              moonbit_decref(_M0L4_2axS1334);
              goto join_1329;
              break;
            }
          }
        }
        goto joinlet_4047;
        join_1330:;
        _M0L6_2atmpS3321 = _M0L1sS1331;
        #line 283 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1323, _M0L6_2atmpS3321);
        if (_M0L6_2atmpS3321) {
          moonbit_decref(_M0L6_2atmpS3321);
        }
        joinlet_4047:;
        goto joinlet_4046;
        join_1329:;
        _M0L6_2atmpS3320 = 0;
        #line 284 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1323, _M0L6_2atmpS3320);
        if (_M0L6_2atmpS3320) {
          moonbit_decref(_M0L6_2atmpS3320);
        }
        joinlet_4046:;
      }
      _M0L6_2atmpS3323 = _M0L2__S1326 + 1;
      _M0L2__S1326 = _M0L6_2atmpS3323;
      continue;
    }
    break;
  }
  return _M0L6resultS1323;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database4mset(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1321,
  struct _M0TPB5ArrayGsE* _M0L4keysS1319,
  struct _M0TPB5ArrayGsE* _M0L6valuesS1320
) {
  int32_t _M0L1iS1318;
  #line 269 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L1iS1318 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3311;
    int32_t _if__result_4049;
    #line 270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L6_2atmpS3311 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1319);
    if (_M0L1iS1318 < _M0L6_2atmpS3311) {
      int32_t _M0L6_2atmpS3310;
      #line 270 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3310 = _M0MPC15array5Array6lengthGsE(_M0L6valuesS1320);
      _if__result_4049 = _M0L1iS1318 < _M0L6_2atmpS3310;
    } else {
      _if__result_4049 = 0;
    }
    if (_if__result_4049) {
      struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3312 =
        _M0L4selfS1321->$0;
      moonbit_string_t _M0L6_2atmpS3313;
      moonbit_string_t _M0L6_2atmpS3315;
      void* _M0L6StringS3314;
      struct _M0TPB3MapGsiE* _M0L7expiresS3316;
      moonbit_string_t _M0L6_2atmpS3317;
      int32_t _M0L6_2atmpS3318;
      moonbit_incref(_M0L4dataS3312);
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3313
      = _M0MPC15array5Array2atGsE(_M0L4keysS1319, _M0L1iS1318);
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3315
      = _M0MPC15array5Array2atGsE(_M0L6valuesS1320, _M0L1iS1318);
      _M0L6StringS3314
      = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String));
      Moonbit_object_header(_M0L6StringS3314)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
      ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L6StringS3314)->$0
      = _M0L6_2atmpS3315;
      #line 271 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3312, _M0L6_2atmpS3313, _M0L6StringS3314);
      moonbit_decref(_M0L4dataS3312);
      moonbit_decref(_M0L6_2atmpS3313);
      moonbit_decref(_M0L6StringS3314);
      _M0L7expiresS3316 = _M0L4selfS1321->$1;
      moonbit_incref(_M0L7expiresS3316);
      #line 272 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3317
      = _M0MPC15array5Array2atGsE(_M0L4keysS1319, _M0L1iS1318);
      #line 272 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS3316, _M0L6_2atmpS3317);
      moonbit_decref(_M0L7expiresS3316);
      moonbit_decref(_M0L6_2atmpS3317);
      _M0L6_2atmpS3318 = _M0L1iS1318 + 1;
      _M0L1iS1318 = _M0L6_2atmpS3318;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database3ttl(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1315,
  moonbit_string_t _M0L3keyS1316
) {
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3302;
  int32_t _M0L6_2atmpS3301;
  #line 226 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3302 = _M0L4selfS1315->$0;
  moonbit_incref(_M0L4dataS3302);
  #line 227 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3301
  = _M0MPB3Map8containsGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3302, _M0L3keyS1316);
  moonbit_decref(_M0L4dataS3302);
  if (!_M0L6_2atmpS3301) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS3303 = _M0L4selfS1315->$1;
    int32_t _result_4050;
    moonbit_incref(_M0L7expiresS3303);
    #line 229 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _result_4050 = _M0MPB3Map8containsGsiE(_M0L7expiresS3303, _M0L3keyS1316);
    moonbit_decref(_M0L7expiresS3303);
    if (_result_4050) {
      struct _M0TPB3MapGsiE* _M0L7expiresS3309 = _M0L4selfS1315->$1;
      int64_t _M0L6_2atmpS3308;
      int32_t _M0L6_2atmpS3306;
      int32_t _M0L13current__timeS3307;
      int32_t _M0L9remainingS1317;
      moonbit_incref(_M0L7expiresS3309);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3308 = _M0MPB3Map3getGsiE(_M0L7expiresS3309, _M0L3keyS1316);
      moonbit_decref(_M0L7expiresS3309);
      #line 230 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3306 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS3308);
      _M0L13current__timeS3307 = _M0L4selfS1315->$2;
      _M0L9remainingS1317 = _M0L6_2atmpS3306 - _M0L13current__timeS3307;
      if (_M0L9remainingS1317 <= 0) {
        struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3304 =
          _M0L4selfS1315->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS3305;
        moonbit_incref(_M0L4dataS3304);
        #line 232 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3304, _M0L3keyS1316);
        moonbit_decref(_M0L4dataS3304);
        _M0L7expiresS3305 = _M0L4selfS1315->$1;
        moonbit_incref(_M0L7expiresS3305);
        #line 233 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS3305, _M0L3keyS1316);
        moonbit_decref(_M0L7expiresS3305);
        return -2;
      } else {
        return _M0L9remainingS1317 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database6expire(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1312,
  moonbit_string_t _M0L3keyS1313,
  int32_t _M0L7secondsS1314
) {
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3296;
  int32_t _result_4051;
  #line 208 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3296 = _M0L4selfS1312->$0;
  moonbit_incref(_M0L4dataS3296);
  #line 209 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_4051
  = _M0MPB3Map8containsGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3296, _M0L3keyS1313);
  moonbit_decref(_M0L4dataS3296);
  if (_result_4051) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3297 = _M0L4selfS1312->$1;
    int32_t _M0L13current__timeS3299 = _M0L4selfS1312->$2;
    int32_t _M0L6_2atmpS3300 = _M0L7secondsS1314 * 1000;
    int32_t _M0L6_2atmpS3298 = _M0L13current__timeS3299 + _M0L6_2atmpS3300;
    moonbit_incref(_M0L7expiresS3297);
    #line 210 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS3297, _M0L3keyS1313, _M0L6_2atmpS3298);
    moonbit_decref(_M0L7expiresS3297);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database3set(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1309,
  moonbit_string_t _M0L3keyS1310,
  moonbit_string_t _M0L5valueS1311
) {
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3293;
  void* _M0L6StringS3294;
  struct _M0TPB3MapGsiE* _M0L7expiresS3295;
  #line 203 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4dataS3293 = _M0L4selfS1309->$0;
  moonbit_incref(_M0L5valueS1311);
  _M0L6StringS3294
  = (void*)moonbit_malloc(sizeof(struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3294)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP39moonbitdb9moonbitdb3lib10RedisValue6String*)_M0L6StringS3294)->$0
  = _M0L5valueS1311;
  moonbit_incref(_M0L4dataS3293);
  #line 204 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3293, _M0L3keyS1310, _M0L6StringS3294);
  moonbit_decref(_M0L4dataS3293);
  moonbit_decref(_M0L6StringS3294);
  _M0L7expiresS3295 = _M0L4selfS1309->$1;
  moonbit_incref(_M0L7expiresS3295);
  #line 205 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3295, _M0L3keyS1310);
  moonbit_decref(_M0L7expiresS3295);
  return 0;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database14check__expired(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1304,
  moonbit_string_t _M0L3keyS1305
) {
  int32_t _M0L12expire__timeS1303;
  struct _M0TPB3MapGsiE* _M0L7expiresS3292;
  int64_t _M0L7_2abindS1306;
  int32_t _M0L13current__timeS3289;
  #line 188 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7expiresS3292 = _M0L4selfS1304->$1;
  moonbit_incref(_M0L7expiresS3292);
  #line 189 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1306 = _M0MPB3Map3getGsiE(_M0L7expiresS3292, _M0L3keyS1305);
  moonbit_decref(_M0L7expiresS3292);
  if (_M0L7_2abindS1306 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1307 = _M0L7_2abindS1306;
    int32_t _M0L15_2aexpire__timeS1308 = (int32_t)_M0L7_2aSomeS1307;
    _M0L12expire__timeS1303 = _M0L15_2aexpire__timeS1308;
    goto join_1302;
  }
  join_1302:;
  _M0L13current__timeS3289 = _M0L4selfS1304->$2;
  if (_M0L12expire__timeS1303 <= _M0L13current__timeS3289) {
    struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4dataS3290 =
      _M0L4selfS1304->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS3291;
    moonbit_incref(_M0L4dataS3290);
    #line 192 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4dataS3290, _M0L3keyS1305);
    moonbit_decref(_M0L4dataS3290);
    _M0L7expiresS3291 = _M0L4selfS1304->$1;
    moonbit_incref(_M0L7expiresS3291);
    #line 193 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3291, _M0L3keyS1305);
    moonbit_decref(_M0L7expiresS3291);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP39moonbitdb9moonbitdb3lib8Database13advance__time(
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L4selfS1300,
  int32_t _M0L2msS1301
) {
  int32_t _M0L13current__timeS3288;
  int32_t _M0L6_2atmpS3287;
  #line 184 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L13current__timeS3288 = _M0L4selfS1300->$2;
  _M0L6_2atmpS3287 = _M0L13current__timeS3288 + _M0L2msS1301;
  _M0L4selfS1300->$2 = _M0L6_2atmpS3287;
  return 0;
}

struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0MP39moonbitdb9moonbitdb3lib8Database3new(
  
) {
  struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7_2abindS1298;
  struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L6_2atmpS3286;
  struct _M0TPB9ArrayViewGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE _M0L6_2atmpS3285;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS3281;
  struct _M0TUsiE** _M0L7_2abindS1299;
  struct _M0TUsiE** _M0L6_2atmpS3284;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3283;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3282;
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _block_4053;
  #line 176 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L7_2abindS1298
  = (struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3286 = _M0L7_2abindS1298;
  _M0L6_2atmpS3285
  = (struct _M0TPB9ArrayViewGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE){
    .$0 = _M0L6_2atmpS3286, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3281
  = _M0MPB3Map3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L6_2atmpS3285, 1000ll);
  moonbit_decref(_M0L6_2atmpS3285.$0);
  _M0L7_2abindS1299 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3284 = _M0L7_2abindS1299;
  _M0L6_2atmpS3283
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3284, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3282 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3283, 1000ll);
  moonbit_decref(_M0L6_2atmpS3283.$0);
  _block_4053
  = (struct _M0TP39moonbitdb9moonbitdb3lib8Database*)moonbit_malloc(sizeof(struct _M0TP39moonbitdb9moonbitdb3lib8Database));
  Moonbit_object_header(_block_4053)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 27, 0);
  _block_4053->$0 = _M0L6_2atmpS3281;
  _block_4053->$1 = _M0L6_2atmpS3282;
  _block_4053->$2 = 0;
  return _block_4053;
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib5Deque7get__at(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L4selfS1295,
  int32_t _M0L5indexS1296
) {
  int32_t _M0L3lenS1294;
  int32_t _if__result_4054;
  #line 59 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  #line 60 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L3lenS1294 = _M0MP39moonbitdb9moonbitdb3lib5Deque6length(_M0L4selfS1295);
  if (_M0L5indexS1296 < 0) {
    _if__result_4054 = 1;
  } else {
    _if__result_4054 = _M0L5indexS1296 >= _M0L3lenS1294;
  }
  if (_if__result_4054) {
    return 0;
  } else {
    struct _M0TPB5ArrayGsE* _M0L5frontS3280 = _M0L4selfS1295->$0;
    int32_t _M0L10front__lenS1297;
    moonbit_incref(_M0L5frontS3280);
    #line 64 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
    _M0L10front__lenS1297 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3280);
    moonbit_decref(_M0L5frontS3280);
    if (_M0L5indexS1296 < _M0L10front__lenS1297) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3274 = _M0L4selfS1295->$0;
      int32_t _M0L6_2atmpS3276 = _M0L10front__lenS1297 - 1;
      int32_t _M0L6_2atmpS3275 = _M0L6_2atmpS3276 - _M0L5indexS1296;
      moonbit_string_t _M0L6_2atmpS3273;
      moonbit_incref(_M0L5frontS3274);
      #line 66 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3273
      = _M0MPC15array5Array2atGsE(_M0L5frontS3274, _M0L6_2atmpS3275);
      moonbit_decref(_M0L5frontS3274);
      return _M0L6_2atmpS3273;
    } else {
      struct _M0TPB5ArrayGsE* _M0L4backS3278 = _M0L4selfS1295->$1;
      int32_t _M0L6_2atmpS3279 = _M0L5indexS1296 - _M0L10front__lenS1297;
      moonbit_string_t _M0L6_2atmpS3277;
      moonbit_incref(_M0L4backS3278);
      #line 68 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3277
      = _M0MPC15array5Array2atGsE(_M0L4backS3278, _M0L6_2atmpS3279);
      moonbit_decref(_M0L4backS3278);
      return _M0L6_2atmpS3277;
    }
  }
}

struct _M0TPB5ArrayGsE* _M0MP39moonbitdb9moonbitdb3lib5Deque9to__array(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L4selfS1287
) {
  moonbit_string_t* _M0L6_2atmpS3272;
  struct _M0TPB5ArrayGsE* _M0L6resultS1285;
  struct _M0TPB5ArrayGsE* _M0L5frontS3269;
  int32_t _M0L6_2atmpS3268;
  int32_t _M0L6_2atmpS3267;
  int32_t _M0L1iS1286;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1289;
  int32_t _M0L7_2abindS1290;
  int32_t _M0L2__S1291;
  #line 48 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3272 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1285
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1285)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6resultS1285->$0 = _M0L6_2atmpS3272;
  _M0L6resultS1285->$1 = 0;
  _M0L5frontS3269 = _M0L4selfS1287->$0;
  moonbit_incref(_M0L5frontS3269);
  #line 50 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3268 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3269);
  moonbit_decref(_M0L5frontS3269);
  _M0L6_2atmpS3267 = _M0L6_2atmpS3268 - 1;
  _M0L1iS1286 = _M0L6_2atmpS3267;
  while (1) {
    if (_M0L1iS1286 >= 0) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3265 = _M0L4selfS1287->$0;
      moonbit_string_t _M0L6_2atmpS3264;
      int32_t _M0L6_2atmpS3266;
      moonbit_incref(_M0L5frontS3265);
      #line 51 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3264
      = _M0MPC15array5Array2atGsE(_M0L5frontS3265, _M0L1iS1286);
      moonbit_decref(_M0L5frontS3265);
      #line 51 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1285, _M0L6_2atmpS3264);
      moonbit_decref(_M0L6_2atmpS3264);
      _M0L6_2atmpS3266 = _M0L1iS1286 - 1;
      _M0L1iS1286 = _M0L6_2atmpS3266;
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
      moonbit_string_t* _M0L3bufS3271 = _M0L7_2abindS1289->$0;
      moonbit_string_t _M0L4itemS1292 =
        (moonbit_string_t)_M0L3bufS3271[_M0L2__S1291];
      int32_t _M0L6_2atmpS3270;
      moonbit_incref(_M0L4itemS1292);
      #line 54 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1285, _M0L4itemS1292);
      moonbit_decref(_M0L4itemS1292);
      _M0L6_2atmpS3270 = _M0L2__S1291 + 1;
      _M0L2__S1291 = _M0L6_2atmpS3270;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1289);
    }
    break;
  }
  return _M0L6resultS1285;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib5Deque6length(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L4selfS1284
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3263;
  int32_t _M0L6_2atmpS3260;
  struct _M0TPB5ArrayGsE* _M0L4backS3262;
  int32_t _M0L6_2atmpS3261;
  #line 44 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3263 = _M0L4selfS1284->$0;
  moonbit_incref(_M0L5frontS3263);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3260 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3263);
  moonbit_decref(_M0L5frontS3263);
  _M0L4backS3262 = _M0L4selfS1284->$1;
  moonbit_incref(_M0L4backS3262);
  #line 45 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3261 = _M0MPC15array5Array6lengthGsE(_M0L4backS3262);
  moonbit_decref(_M0L4backS3262);
  return _M0L6_2atmpS3260 + _M0L6_2atmpS3261;
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib5Deque9pop__back(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L4selfS1277
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3254;
  int32_t _M0L6_2atmpS3253;
  struct _M0TPB5ArrayGsE* _M0L4backS3259;
  moonbit_string_t _result_4059;
  #line 31 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4backS3254 = _M0L4selfS1277->$1;
  moonbit_incref(_M0L4backS3254);
  #line 32 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3253 = _M0MPC15array5Array6lengthGsE(_M0L4backS3254);
  moonbit_decref(_M0L4backS3254);
  if (_M0L6_2atmpS3253 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3256 = _M0L4selfS1277->$0;
      int32_t _M0L6_2atmpS3255;
      moonbit_incref(_M0L5frontS3256);
      #line 33 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3255 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3256);
      moonbit_decref(_M0L5frontS3256);
      if (_M0L6_2atmpS3255 > 0) {
        struct _M0TPB5ArrayGsE* _M0L5frontS3258 = _M0L4selfS1277->$0;
        moonbit_string_t _M0L4itemS1278;
        moonbit_string_t _M0L1vS1280;
        struct _M0TPB5ArrayGsE* _M0L4backS3257;
        moonbit_incref(_M0L5frontS3258);
        #line 34 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L4itemS1278 = _M0MPC15array5Array3popGsE(_M0L5frontS3258);
        moonbit_decref(_M0L5frontS3258);
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
        goto joinlet_4058;
        join_1279:;
        _M0L4backS3257 = _M0L4selfS1277->$1;
        moonbit_incref(_M0L4backS3257);
        #line 36 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4backS3257, _M0L1vS1280);
        moonbit_decref(_M0L4backS3257);
        moonbit_decref(_M0L1vS1280);
        joinlet_4058:;
        continue;
      }
      break;
    }
  }
  _M0L4backS3259 = _M0L4selfS1277->$1;
  moonbit_incref(_M0L4backS3259);
  #line 41 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_4059 = _M0MPC15array5Array3popGsE(_M0L4backS3259);
  moonbit_decref(_M0L4backS3259);
  return _result_4059;
}

moonbit_string_t _M0MP39moonbitdb9moonbitdb3lib5Deque10pop__front(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L4selfS1270
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3247;
  int32_t _M0L6_2atmpS3246;
  struct _M0TPB5ArrayGsE* _M0L5frontS3252;
  moonbit_string_t _result_4062;
  #line 18 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L5frontS3247 = _M0L4selfS1270->$0;
  moonbit_incref(_M0L5frontS3247);
  #line 19 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3246 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3247);
  moonbit_decref(_M0L5frontS3247);
  if (_M0L6_2atmpS3246 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L4backS3249 = _M0L4selfS1270->$1;
      int32_t _M0L6_2atmpS3248;
      moonbit_incref(_M0L4backS3249);
      #line 20 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
      _M0L6_2atmpS3248 = _M0MPC15array5Array6lengthGsE(_M0L4backS3249);
      moonbit_decref(_M0L4backS3249);
      if (_M0L6_2atmpS3248 > 0) {
        struct _M0TPB5ArrayGsE* _M0L4backS3251 = _M0L4selfS1270->$1;
        moonbit_string_t _M0L4itemS1271;
        moonbit_string_t _M0L1vS1273;
        struct _M0TPB5ArrayGsE* _M0L5frontS3250;
        moonbit_incref(_M0L4backS3251);
        #line 21 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0L4itemS1271 = _M0MPC15array5Array3popGsE(_M0L4backS3251);
        moonbit_decref(_M0L4backS3251);
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
        goto joinlet_4061;
        join_1272:;
        _M0L5frontS3250 = _M0L4selfS1270->$0;
        moonbit_incref(_M0L5frontS3250);
        #line 23 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
        _M0MPC15array5Array4pushGsE(_M0L5frontS3250, _M0L1vS1273);
        moonbit_decref(_M0L5frontS3250);
        moonbit_decref(_M0L1vS1273);
        joinlet_4061:;
        continue;
      }
      break;
    }
  }
  _M0L5frontS3252 = _M0L4selfS1270->$0;
  moonbit_incref(_M0L5frontS3252);
  #line 28 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _result_4062 = _M0MPC15array5Array3popGsE(_M0L5frontS3252);
  moonbit_decref(_M0L5frontS3252);
  return _result_4062;
}

int32_t _M0MP39moonbitdb9moonbitdb3lib5Deque10push__back(
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0L4selfS1268,
  moonbit_string_t _M0L5valueS1269
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3245;
  #line 14 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L4backS3245 = _M0L4selfS1268->$1;
  moonbit_incref(_M0L4backS3245);
  #line 15 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0MPC15array5Array4pushGsE(_M0L4backS3245, _M0L5valueS1269);
  moonbit_decref(_M0L4backS3245);
  return 0;
}

struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _M0MP39moonbitdb9moonbitdb3lib5Deque3new(
  
) {
  moonbit_string_t* _M0L6_2atmpS3244;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3241;
  moonbit_string_t* _M0L6_2atmpS3243;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3242;
  struct _M0TP39moonbitdb9moonbitdb3lib5Deque* _block_4063;
  #line 6 "/home/developer/Documents2/moonbitDB/lib/database.mbt"
  _M0L6_2atmpS3244 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3241
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3241)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6_2atmpS3241->$0 = _M0L6_2atmpS3244;
  _M0L6_2atmpS3241->$1 = 0;
  _M0L6_2atmpS3243 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3242
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3242)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6_2atmpS3242->$0 = _M0L6_2atmpS3243;
  _M0L6_2atmpS3242->$1 = 0;
  _block_4063
  = (struct _M0TP39moonbitdb9moonbitdb3lib5Deque*)moonbit_malloc(sizeof(struct _M0TP39moonbitdb9moonbitdb3lib5Deque));
  Moonbit_object_header(_block_4063)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 31, 0);
  _block_4063->$0 = _M0L6_2atmpS3241;
  _block_4063->$1 = _M0L6_2atmpS3242;
  return _block_4063;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1267) {
  double _M0L6_2atmpS3240;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3240 = (double)_M0L4selfS1267;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3240);
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1265,
  struct _M0TPC16string10StringView _M0L9separatorS1266
) {
  moonbit_string_t* _M0L3bufS3238;
  int32_t _M0L3lenS3239;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3237;
  moonbit_string_t _result_4064;
  #line 2184 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS3238 = _M0L4selfS1265->$0;
  _M0L3lenS3239 = _M0L4selfS1265->$1;
  moonbit_incref(_M0L3bufS3238);
  _M0L6_2atmpS3237
  = (struct _M0TPB9ArrayViewGsE){
    .$0 = _M0L3bufS3238, .$1 = 0, .$2 = _M0L3lenS3239
  };
  #line 2188 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _result_4064
  = _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS3237, _M0L9separatorS1266);
  moonbit_decref(_M0L6_2atmpS3237.$0);
  return _result_4064;
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
    moonbit_string_t* _M0L3bufS3236 = _M0L4selfS1262->$0;
    moonbit_string_t _M0L1vS1264 =
      (moonbit_string_t)_M0L3bufS3236[_M0L5indexS1263];
    moonbit_string_t* _M0L3bufS3235 = _M0L4selfS1262->$0;
    moonbit_string_t _M0L6_2aoldS3618;
    if (
      _M0L5indexS1263 < 0
      || _M0L5indexS1263 >= Moonbit_array_length(_M0L3bufS3235)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3618 = (moonbit_string_t)_M0L3bufS3235[_M0L5indexS1263];
    moonbit_incref(_M0L1vS1264);
    moonbit_decref(_M0L6_2aoldS3618);
    if (
      _M0L5indexS1263 < 0
      || _M0L5indexS1263 >= Moonbit_array_length(_M0L3bufS3235)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3235[_M0L5indexS1263]
    = (moonbit_string_t)moonbit_string_literal_14.data;
    _M0L4selfS1262->$1 = _M0L5indexS1263;
    return _M0L1vS1264;
  }
}

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS1253,
  int32_t _M0L5indexS1254
) {
  int32_t _M0L3lenS1252;
  int32_t _if__result_4065;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1252 = _M0L4selfS1253->$1;
  if (_M0L5indexS1254 >= 0) {
    _if__result_4065 = _M0L5indexS1254 < _M0L3lenS1252;
  } else {
    _if__result_4065 = 0;
  }
  if (_if__result_4065) {
    moonbit_string_t* _M0L6_2atmpS3232;
    moonbit_string_t _M0L6_2atmpS3622;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3232 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS1253);
    _M0L6_2atmpS3622 = (moonbit_string_t)_M0L6_2atmpS3232[_M0L5indexS1254];
    if (_M0L6_2atmpS3622) {
      moonbit_incref(_M0L6_2atmpS3622);
    }
    moonbit_decref(_M0L6_2atmpS3232);
    return _M0L6_2atmpS3622;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1256,
  int32_t _M0L5indexS1257
) {
  int32_t _M0L3lenS1255;
  int32_t _if__result_4066;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1255 = _M0L4selfS1256->$1;
  if (_M0L5indexS1257 >= 0) {
    _if__result_4066 = _M0L5indexS1257 < _M0L3lenS1255;
  } else {
    _if__result_4066 = 0;
  }
  if (_if__result_4066) {
    moonbit_string_t* _M0L6_2atmpS3233;
    moonbit_string_t _M0L6_2atmpS3623;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3233 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1256);
    _M0L6_2atmpS3623 = (moonbit_string_t)_M0L6_2atmpS3233[_M0L5indexS1257];
    moonbit_incref(_M0L6_2atmpS3623);
    moonbit_decref(_M0L6_2atmpS3233);
    return _M0L6_2atmpS3623;
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
  int32_t _if__result_4067;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1258 = _M0L4selfS1259->$1;
  if (_M0L5indexS1260 >= 0) {
    _if__result_4067 = _M0L5indexS1260 < _M0L3lenS1258;
  } else {
    _if__result_4067 = 0;
  }
  if (_if__result_4067) {
    struct _M0TUsfE** _M0L6_2atmpS3234;
    struct _M0TUsfE* _M0L6_2atmpS3624;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3234 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1259);
    _M0L6_2atmpS3624 = (struct _M0TUsfE*)_M0L6_2atmpS3234[_M0L5indexS1260];
    if (_M0L6_2atmpS3624) {
      moonbit_incref(_M0L6_2atmpS3624);
    }
    moonbit_decref(_M0L6_2atmpS3234);
    return _M0L6_2atmpS3624;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1251) {
  moonbit_string_t _M0L6_2atmpS3231;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3231
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1251);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3231);
  moonbit_decref(_M0L6_2atmpS3231);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1250) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1250);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1237) {
  uint64_t _M0L4bitsS1238;
  uint64_t _M0L6_2atmpS3230;
  uint64_t _M0L6_2atmpS3229;
  int32_t _M0L8ieeeSignS1239;
  uint64_t _M0L12ieeeMantissaS1240;
  uint64_t _M0L6_2atmpS3228;
  uint64_t _M0L6_2atmpS3227;
  int32_t _M0L12ieeeExponentS1241;
  int32_t _if__result_4068;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1242;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1243;
  moonbit_string_t _result_4070;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1237 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_3.data;
  }
  _M0L4bitsS1238 = *(int64_t*)&_M0L3valS1237;
  _M0L6_2atmpS3230 = _M0L4bitsS1238 >> 63;
  _M0L6_2atmpS3229 = _M0L6_2atmpS3230 & 1ull;
  _M0L8ieeeSignS1239 = _M0L6_2atmpS3229 != 0ull;
  _M0L12ieeeMantissaS1240 = _M0L4bitsS1238 & 4503599627370495ull;
  _M0L6_2atmpS3228 = _M0L4bitsS1238 >> 52;
  _M0L6_2atmpS3227 = _M0L6_2atmpS3228 & 2047ull;
  _M0L12ieeeExponentS1241 = (int32_t)_M0L6_2atmpS3227;
  if (_M0L12ieeeExponentS1241 == 2047) {
    _if__result_4068 = 1;
  } else if (_M0L12ieeeExponentS1241 == 0) {
    _if__result_4068 = _M0L12ieeeMantissaS1240 == 0ull;
  } else {
    _if__result_4068 = 0;
  }
  if (_if__result_4068) {
    int32_t _M0L6_2atmpS3218 = _M0L12ieeeExponentS1241 != 0;
    int32_t _M0L6_2atmpS3219 = _M0L12ieeeMantissaS1240 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1239, _M0L6_2atmpS3218, _M0L6_2atmpS3219);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1242
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1240, _M0L12ieeeExponentS1241);
  if (_M0L7_2abindS1242 == 0) {
    uint32_t _M0L6_2atmpS3220;
    if (_M0L7_2abindS1242) {
      moonbit_decref(_M0L7_2abindS1242);
    }
    _M0L6_2atmpS3220 = *(uint32_t*)&_M0L12ieeeExponentS1241;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1243 = _M0FPB3d2d(_M0L12ieeeMantissaS1240, _M0L6_2atmpS3220);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1244 = _M0L7_2abindS1242;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1245 = _M0L7_2aSomeS1244;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1246 = _M0L4_2afS1245;
    while (1) {
      uint64_t _M0L8mantissaS3226 = _M0L1xS1246->$0;
      uint64_t _M0L1qS1247 = _M0L8mantissaS3226 / 10ull;
      uint64_t _M0L8mantissaS3224 = _M0L1xS1246->$0;
      uint64_t _M0L6_2atmpS3225 = 10ull * _M0L1qS1247;
      uint64_t _M0L1rS1248 = _M0L8mantissaS3224 - _M0L6_2atmpS3225;
      int32_t _M0L8exponentS3223;
      int32_t _M0L6_2atmpS3222;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3221;
      if (_M0L1rS1248 != 0ull) {
        _M0L1vS1243 = _M0L1xS1246;
        break;
      }
      _M0L8exponentS3223 = _M0L1xS1246->$1;
      moonbit_decref(_M0L1xS1246);
      _M0L6_2atmpS3222 = _M0L8exponentS3223 + 1;
      _M0L6_2atmpS3221
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3221)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3221->$0 = _M0L1qS1247;
      _M0L6_2atmpS3221->$1 = _M0L6_2atmpS3222;
      _M0L1xS1246 = _M0L6_2atmpS3221;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4070 = _M0FPB9to__chars(_M0L1vS1243, _M0L8ieeeSignS1239);
  moonbit_decref(_M0L1vS1243);
  return _result_4070;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1232,
  int32_t _M0L12ieeeExponentS1234
) {
  uint64_t _M0L2m2S1231;
  int32_t _M0L6_2atmpS3217;
  int32_t _M0L2e2S1233;
  int32_t _M0L6_2atmpS3216;
  uint64_t _M0L6_2atmpS3215;
  uint64_t _M0L4maskS1235;
  uint64_t _M0L8fractionS1236;
  int32_t _M0L6_2atmpS3214;
  uint64_t _M0L6_2atmpS3213;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3212;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1231 = 4503599627370496ull | _M0L12ieeeMantissaS1232;
  _M0L6_2atmpS3217 = _M0L12ieeeExponentS1234 - 1023;
  _M0L2e2S1233 = _M0L6_2atmpS3217 - 52;
  if (_M0L2e2S1233 > 0) {
    return 0;
  }
  if (_M0L2e2S1233 < -52) {
    return 0;
  }
  _M0L6_2atmpS3216 = -_M0L2e2S1233;
  _M0L6_2atmpS3215 = 1ull << (_M0L6_2atmpS3216 & 63);
  _M0L4maskS1235 = _M0L6_2atmpS3215 - 1ull;
  _M0L8fractionS1236 = _M0L2m2S1231 & _M0L4maskS1235;
  if (_M0L8fractionS1236 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3214 = -_M0L2e2S1233;
  _M0L6_2atmpS3213 = _M0L2m2S1231 >> (_M0L6_2atmpS3214 & 63);
  _M0L6_2atmpS3212
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3212)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3212->$0 = _M0L6_2atmpS3213;
  _M0L6_2atmpS3212->$1 = 0;
  return _M0L6_2atmpS3212;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1199,
  int32_t _M0L4signS1197
) {
  int32_t _M0L6_2atmpS3211;
  moonbit_bytes_t _M0L6resultS1195;
  int32_t _M0Lm5indexS1196;
  uint64_t _M0L6outputS1198;
  int32_t _M0L7olengthS1200;
  int32_t _M0L8exponentS3210;
  int32_t _M0L6_2atmpS3209;
  int32_t _M0Lm3expS1201;
  int32_t _M0L6_2atmpS3208;
  int32_t _M0L6_2atmpS3206;
  int32_t _M0L18scientificNotationS1202;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3211 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1195
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3211);
  _M0Lm5indexS1196 = 0;
  if (_M0L4signS1197) {
    int32_t _M0L6_2atmpS3080 = _M0Lm5indexS1196;
    int32_t _M0L6_2atmpS3081;
    if (
      _M0L6_2atmpS3080 < 0
      || _M0L6_2atmpS3080 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3080] = 45;
    _M0L6_2atmpS3081 = _M0Lm5indexS1196;
    _M0Lm5indexS1196 = _M0L6_2atmpS3081 + 1;
  }
  _M0L6outputS1198 = _M0L1vS1199->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1200 = _M0FPB17decimal__length17(_M0L6outputS1198);
  _M0L8exponentS3210 = _M0L1vS1199->$1;
  _M0L6_2atmpS3209 = _M0L8exponentS3210 + _M0L7olengthS1200;
  _M0Lm3expS1201 = _M0L6_2atmpS3209 - 1;
  _M0L6_2atmpS3208 = _M0Lm3expS1201;
  if (_M0L6_2atmpS3208 >= -6) {
    int32_t _M0L6_2atmpS3207 = _M0Lm3expS1201;
    _M0L6_2atmpS3206 = _M0L6_2atmpS3207 < 21;
  } else {
    _M0L6_2atmpS3206 = 0;
  }
  _M0L18scientificNotationS1202 = !_M0L6_2atmpS3206;
  if (_M0L18scientificNotationS1202) {
    int32_t _M0L7_2abindS1203 = _M0L7olengthS1200 - 1;
    uint64_t _M0L6outputS1204;
    int32_t _M0L1iS1205 = 0;
    uint64_t _M0L6outputS1206 = _M0L6outputS1198;
    int32_t _M0L6_2atmpS3082;
    int32_t _M0L6_2atmpS3086;
    int32_t _M0L6_2atmpS3085;
    int32_t _M0L6_2atmpS3084;
    int32_t _M0L6_2atmpS3083;
    int32_t _M0L6_2atmpS3090;
    int32_t _M0L6_2atmpS3091;
    int32_t _M0L6_2atmpS3092;
    int32_t _M0L6_2atmpS3093;
    int32_t _M0L6_2atmpS3094;
    int32_t _M0L6_2atmpS3100;
    int32_t _M0L6_2atmpS3133;
    moonbit_string_t _result_4072;
    while (1) {
      if (_M0L1iS1205 < _M0L7_2abindS1203) {
        uint64_t _M0L1cS1207 = _M0L6outputS1206 % 10ull;
        int32_t _M0L6_2atmpS3139 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3138 = _M0L6_2atmpS3139 + _M0L7olengthS1200;
        int32_t _M0L6_2atmpS3134 = _M0L6_2atmpS3138 - _M0L1iS1205;
        int32_t _M0L6_2atmpS3137 = (int32_t)_M0L1cS1207;
        int32_t _M0L6_2atmpS3136 = 48 + _M0L6_2atmpS3137;
        int32_t _M0L6_2atmpS3135 = _M0L6_2atmpS3136 & 0xff;
        int32_t _M0L6_2atmpS3140;
        uint64_t _M0L6_2atmpS3141;
        if (
          _M0L6_2atmpS3134 < 0
          || _M0L6_2atmpS3134 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3134] = _M0L6_2atmpS3135;
        _M0L6_2atmpS3140 = _M0L1iS1205 + 1;
        _M0L6_2atmpS3141 = _M0L6outputS1206 / 10ull;
        _M0L1iS1205 = _M0L6_2atmpS3140;
        _M0L6outputS1206 = _M0L6_2atmpS3141;
        continue;
      } else {
        _M0L6outputS1204 = _M0L6outputS1206;
      }
      break;
    }
    _M0L6_2atmpS3082 = _M0Lm5indexS1196;
    _M0L6_2atmpS3086 = (int32_t)_M0L6outputS1204;
    _M0L6_2atmpS3085 = _M0L6_2atmpS3086 % 10;
    _M0L6_2atmpS3084 = 48 + _M0L6_2atmpS3085;
    _M0L6_2atmpS3083 = _M0L6_2atmpS3084 & 0xff;
    if (
      _M0L6_2atmpS3082 < 0
      || _M0L6_2atmpS3082 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3082] = _M0L6_2atmpS3083;
    if (_M0L7olengthS1200 > 1) {
      int32_t _M0L6_2atmpS3088 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3087 = _M0L6_2atmpS3088 + 1;
      if (
        _M0L6_2atmpS3087 < 0
        || _M0L6_2atmpS3087 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3087] = 46;
    } else {
      int32_t _M0L6_2atmpS3089 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3089 - 1;
    }
    _M0L6_2atmpS3090 = _M0Lm5indexS1196;
    _M0L6_2atmpS3091 = _M0L7olengthS1200 + 1;
    _M0Lm5indexS1196 = _M0L6_2atmpS3090 + _M0L6_2atmpS3091;
    _M0L6_2atmpS3092 = _M0Lm5indexS1196;
    if (
      _M0L6_2atmpS3092 < 0
      || _M0L6_2atmpS3092 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3092] = 101;
    _M0L6_2atmpS3093 = _M0Lm5indexS1196;
    _M0Lm5indexS1196 = _M0L6_2atmpS3093 + 1;
    _M0L6_2atmpS3094 = _M0Lm3expS1201;
    if (_M0L6_2atmpS3094 < 0) {
      int32_t _M0L6_2atmpS3095 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3096;
      int32_t _M0L6_2atmpS3097;
      if (
        _M0L6_2atmpS3095 < 0
        || _M0L6_2atmpS3095 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3095] = 45;
      _M0L6_2atmpS3096 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3096 + 1;
      _M0L6_2atmpS3097 = _M0Lm3expS1201;
      _M0Lm3expS1201 = -_M0L6_2atmpS3097;
    } else {
      int32_t _M0L6_2atmpS3098 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3099;
      if (
        _M0L6_2atmpS3098 < 0
        || _M0L6_2atmpS3098 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3098] = 43;
      _M0L6_2atmpS3099 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3099 + 1;
    }
    _M0L6_2atmpS3100 = _M0Lm3expS1201;
    if (_M0L6_2atmpS3100 >= 100) {
      int32_t _M0L6_2atmpS3116 = _M0Lm3expS1201;
      int32_t _M0L1aS1209 = _M0L6_2atmpS3116 / 100;
      int32_t _M0L6_2atmpS3115 = _M0Lm3expS1201;
      int32_t _M0L6_2atmpS3114 = _M0L6_2atmpS3115 / 10;
      int32_t _M0L1bS1210 = _M0L6_2atmpS3114 % 10;
      int32_t _M0L6_2atmpS3113 = _M0Lm3expS1201;
      int32_t _M0L1cS1211 = _M0L6_2atmpS3113 % 10;
      int32_t _M0L6_2atmpS3101 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3103 = 48 + _M0L1aS1209;
      int32_t _M0L6_2atmpS3102 = _M0L6_2atmpS3103 & 0xff;
      int32_t _M0L6_2atmpS3107;
      int32_t _M0L6_2atmpS3104;
      int32_t _M0L6_2atmpS3106;
      int32_t _M0L6_2atmpS3105;
      int32_t _M0L6_2atmpS3111;
      int32_t _M0L6_2atmpS3108;
      int32_t _M0L6_2atmpS3110;
      int32_t _M0L6_2atmpS3109;
      int32_t _M0L6_2atmpS3112;
      if (
        _M0L6_2atmpS3101 < 0
        || _M0L6_2atmpS3101 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3101] = _M0L6_2atmpS3102;
      _M0L6_2atmpS3107 = _M0Lm5indexS1196;
      _M0L6_2atmpS3104 = _M0L6_2atmpS3107 + 1;
      _M0L6_2atmpS3106 = 48 + _M0L1bS1210;
      _M0L6_2atmpS3105 = _M0L6_2atmpS3106 & 0xff;
      if (
        _M0L6_2atmpS3104 < 0
        || _M0L6_2atmpS3104 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3104] = _M0L6_2atmpS3105;
      _M0L6_2atmpS3111 = _M0Lm5indexS1196;
      _M0L6_2atmpS3108 = _M0L6_2atmpS3111 + 2;
      _M0L6_2atmpS3110 = 48 + _M0L1cS1211;
      _M0L6_2atmpS3109 = _M0L6_2atmpS3110 & 0xff;
      if (
        _M0L6_2atmpS3108 < 0
        || _M0L6_2atmpS3108 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3108] = _M0L6_2atmpS3109;
      _M0L6_2atmpS3112 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3112 + 3;
    } else {
      int32_t _M0L6_2atmpS3117 = _M0Lm3expS1201;
      if (_M0L6_2atmpS3117 >= 10) {
        int32_t _M0L6_2atmpS3127 = _M0Lm3expS1201;
        int32_t _M0L1aS1212 = _M0L6_2atmpS3127 / 10;
        int32_t _M0L6_2atmpS3126 = _M0Lm3expS1201;
        int32_t _M0L1bS1213 = _M0L6_2atmpS3126 % 10;
        int32_t _M0L6_2atmpS3118 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3120 = 48 + _M0L1aS1212;
        int32_t _M0L6_2atmpS3119 = _M0L6_2atmpS3120 & 0xff;
        int32_t _M0L6_2atmpS3124;
        int32_t _M0L6_2atmpS3121;
        int32_t _M0L6_2atmpS3123;
        int32_t _M0L6_2atmpS3122;
        int32_t _M0L6_2atmpS3125;
        if (
          _M0L6_2atmpS3118 < 0
          || _M0L6_2atmpS3118 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3118] = _M0L6_2atmpS3119;
        _M0L6_2atmpS3124 = _M0Lm5indexS1196;
        _M0L6_2atmpS3121 = _M0L6_2atmpS3124 + 1;
        _M0L6_2atmpS3123 = 48 + _M0L1bS1213;
        _M0L6_2atmpS3122 = _M0L6_2atmpS3123 & 0xff;
        if (
          _M0L6_2atmpS3121 < 0
          || _M0L6_2atmpS3121 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3121] = _M0L6_2atmpS3122;
        _M0L6_2atmpS3125 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3125 + 2;
      } else {
        int32_t _M0L6_2atmpS3128 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3131 = _M0Lm3expS1201;
        int32_t _M0L6_2atmpS3130 = 48 + _M0L6_2atmpS3131;
        int32_t _M0L6_2atmpS3129 = _M0L6_2atmpS3130 & 0xff;
        int32_t _M0L6_2atmpS3132;
        if (
          _M0L6_2atmpS3128 < 0
          || _M0L6_2atmpS3128 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3128] = _M0L6_2atmpS3129;
        _M0L6_2atmpS3132 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3132 + 1;
      }
    }
    _M0L6_2atmpS3133 = _M0Lm5indexS1196;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4072
    = _M0FPB19string__from__bytes(_M0L6resultS1195, 0, _M0L6_2atmpS3133);
    moonbit_decref(_M0L6resultS1195);
    return _result_4072;
  } else {
    int32_t _M0L6_2atmpS3142 = _M0Lm3expS1201;
    int32_t _M0L6_2atmpS3205;
    moonbit_string_t _result_4078;
    if (_M0L6_2atmpS3142 < 0) {
      int32_t _M0L6_2atmpS3143 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3145;
      int32_t _M0L6_2atmpS3144;
      int32_t _M0L6_2atmpS3146;
      int32_t _M0L1iS1214;
      int32_t _M0L6_2atmpS3161;
      int32_t _M0L6_2atmpS3163;
      int32_t _M0L6_2atmpS3162;
      int32_t _M0L7currentS1216;
      int32_t _M0L1iS1217;
      uint64_t _M0L6outputS1218;
      if (
        _M0L6_2atmpS3143 < 0
        || _M0L6_2atmpS3143 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3143] = 48;
      _M0L6_2atmpS3145 = _M0Lm5indexS1196;
      _M0L6_2atmpS3144 = _M0L6_2atmpS3145 + 1;
      if (
        _M0L6_2atmpS3144 < 0
        || _M0L6_2atmpS3144 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3144] = 46;
      _M0L6_2atmpS3146 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3146 + 2;
      _M0L1iS1214 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3147 = _M0Lm3expS1201;
        if (_M0L1iS1214 > _M0L6_2atmpS3147) {
          int32_t _M0L6_2atmpS3150 = _M0Lm5indexS1196;
          int32_t _M0L6_2atmpS3149 = _M0L6_2atmpS3150 - _M0L1iS1214;
          int32_t _M0L6_2atmpS3148 = _M0L6_2atmpS3149 - 1;
          int32_t _M0L6_2atmpS3151;
          if (
            _M0L6_2atmpS3148 < 0
            || _M0L6_2atmpS3148 >= Moonbit_array_length(_M0L6resultS1195)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1195[_M0L6_2atmpS3148] = 48;
          _M0L6_2atmpS3151 = _M0L1iS1214 - 1;
          _M0L1iS1214 = _M0L6_2atmpS3151;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3161 = _M0Lm5indexS1196;
      _M0L6_2atmpS3163 = _M0Lm3expS1201;
      _M0L6_2atmpS3162 = -1 - _M0L6_2atmpS3163;
      _M0L7currentS1216 = _M0L6_2atmpS3161 + _M0L6_2atmpS3162;
      _M0L1iS1217 = 0;
      _M0L6outputS1218 = _M0L6outputS1198;
      while (1) {
        if (_M0L1iS1217 < _M0L7olengthS1200) {
          int32_t _M0L6_2atmpS3158 = _M0L7currentS1216 + _M0L7olengthS1200;
          int32_t _M0L6_2atmpS3157 = _M0L6_2atmpS3158 - _M0L1iS1217;
          int32_t _M0L6_2atmpS3152 = _M0L6_2atmpS3157 - 1;
          uint64_t _M0L6_2atmpS3156 = _M0L6outputS1218 % 10ull;
          int32_t _M0L6_2atmpS3155 = (int32_t)_M0L6_2atmpS3156;
          int32_t _M0L6_2atmpS3154 = 48 + _M0L6_2atmpS3155;
          int32_t _M0L6_2atmpS3153 = _M0L6_2atmpS3154 & 0xff;
          int32_t _M0L6_2atmpS3159;
          uint64_t _M0L6_2atmpS3160;
          if (
            _M0L6_2atmpS3152 < 0
            || _M0L6_2atmpS3152 >= Moonbit_array_length(_M0L6resultS1195)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1195[_M0L6_2atmpS3152] = _M0L6_2atmpS3153;
          _M0L6_2atmpS3159 = _M0L1iS1217 + 1;
          _M0L6_2atmpS3160 = _M0L6outputS1218 / 10ull;
          _M0L1iS1217 = _M0L6_2atmpS3159;
          _M0L6outputS1218 = _M0L6_2atmpS3160;
          continue;
        }
        break;
      }
      _M0Lm5indexS1196 = _M0L7currentS1216 + _M0L7olengthS1200;
    } else {
      int32_t _M0L6_2atmpS3165 = _M0Lm3expS1201;
      int32_t _M0L6_2atmpS3164 = _M0L6_2atmpS3165 + 1;
      if (_M0L6_2atmpS3164 >= _M0L7olengthS1200) {
        int32_t _M0L1iS1220 = 0;
        uint64_t _M0L6outputS1221 = _M0L6outputS1198;
        int32_t _M0L6_2atmpS3176;
        int32_t _M0L6_2atmpS3181;
        int32_t _M0L7_2abindS1223;
        int32_t _M0L1iS1224;
        int32_t _M0L6_2atmpS3182;
        int32_t _M0L6_2atmpS3185;
        int32_t _M0L6_2atmpS3184;
        int32_t _M0L6_2atmpS3183;
        while (1) {
          if (_M0L1iS1220 < _M0L7olengthS1200) {
            int32_t _M0L6_2atmpS3173 = _M0Lm5indexS1196;
            int32_t _M0L6_2atmpS3172 = _M0L6_2atmpS3173 + _M0L7olengthS1200;
            int32_t _M0L6_2atmpS3171 = _M0L6_2atmpS3172 - _M0L1iS1220;
            int32_t _M0L6_2atmpS3166 = _M0L6_2atmpS3171 - 1;
            uint64_t _M0L6_2atmpS3170 = _M0L6outputS1221 % 10ull;
            int32_t _M0L6_2atmpS3169 = (int32_t)_M0L6_2atmpS3170;
            int32_t _M0L6_2atmpS3168 = 48 + _M0L6_2atmpS3169;
            int32_t _M0L6_2atmpS3167 = _M0L6_2atmpS3168 & 0xff;
            int32_t _M0L6_2atmpS3174;
            uint64_t _M0L6_2atmpS3175;
            if (
              _M0L6_2atmpS3166 < 0
              || _M0L6_2atmpS3166 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3166] = _M0L6_2atmpS3167;
            _M0L6_2atmpS3174 = _M0L1iS1220 + 1;
            _M0L6_2atmpS3175 = _M0L6outputS1221 / 10ull;
            _M0L1iS1220 = _M0L6_2atmpS3174;
            _M0L6outputS1221 = _M0L6_2atmpS3175;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3176 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3176 + _M0L7olengthS1200;
        _M0L6_2atmpS3181 = _M0Lm3expS1201;
        _M0L7_2abindS1223 = _M0L6_2atmpS3181 + 1;
        _M0L1iS1224 = _M0L7olengthS1200;
        while (1) {
          if (_M0L1iS1224 < _M0L7_2abindS1223) {
            int32_t _M0L6_2atmpS3179 = _M0Lm5indexS1196;
            int32_t _M0L6_2atmpS3178 = _M0L6_2atmpS3179 + _M0L1iS1224;
            int32_t _M0L6_2atmpS3177 = _M0L6_2atmpS3178 - _M0L7olengthS1200;
            int32_t _M0L6_2atmpS3180;
            if (
              _M0L6_2atmpS3177 < 0
              || _M0L6_2atmpS3177 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3177] = 48;
            _M0L6_2atmpS3180 = _M0L1iS1224 + 1;
            _M0L1iS1224 = _M0L6_2atmpS3180;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3182 = _M0Lm5indexS1196;
        _M0L6_2atmpS3185 = _M0Lm3expS1201;
        _M0L6_2atmpS3184 = _M0L6_2atmpS3185 + 1;
        _M0L6_2atmpS3183 = _M0L6_2atmpS3184 - _M0L7olengthS1200;
        _M0Lm5indexS1196 = _M0L6_2atmpS3182 + _M0L6_2atmpS3183;
      } else {
        int32_t _M0L6_2atmpS3202 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3201 = _M0L6_2atmpS3202 + 1;
        int32_t _M0L1iS1226 = 0;
        int32_t _M0L7currentS1227 = _M0L6_2atmpS3201;
        uint64_t _M0L6outputS1228 = _M0L6outputS1198;
        int32_t _M0L6_2atmpS3203;
        int32_t _M0L6_2atmpS3204;
        while (1) {
          if (_M0L1iS1226 < _M0L7olengthS1200) {
            int32_t _M0L6_2atmpS3197 = _M0L7olengthS1200 - _M0L1iS1226;
            int32_t _M0L6_2atmpS3195 = _M0L6_2atmpS3197 - 1;
            int32_t _M0L6_2atmpS3196 = _M0Lm3expS1201;
            int32_t _M0L7currentS1229;
            int32_t _M0L6_2atmpS3192;
            int32_t _M0L6_2atmpS3191;
            int32_t _M0L6_2atmpS3186;
            uint64_t _M0L6_2atmpS3190;
            int32_t _M0L6_2atmpS3189;
            int32_t _M0L6_2atmpS3188;
            int32_t _M0L6_2atmpS3187;
            int32_t _M0L6_2atmpS3193;
            uint64_t _M0L6_2atmpS3194;
            if (_M0L6_2atmpS3195 == _M0L6_2atmpS3196) {
              int32_t _M0L6_2atmpS3200 =
                _M0L7currentS1227 + _M0L7olengthS1200;
              int32_t _M0L6_2atmpS3199 = _M0L6_2atmpS3200 - _M0L1iS1226;
              int32_t _M0L6_2atmpS3198 = _M0L6_2atmpS3199 - 1;
              if (
                _M0L6_2atmpS3198 < 0
                || _M0L6_2atmpS3198 >= Moonbit_array_length(_M0L6resultS1195)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1195[_M0L6_2atmpS3198] = 46;
              _M0L7currentS1229 = _M0L7currentS1227 - 1;
            } else {
              _M0L7currentS1229 = _M0L7currentS1227;
            }
            _M0L6_2atmpS3192 = _M0L7currentS1229 + _M0L7olengthS1200;
            _M0L6_2atmpS3191 = _M0L6_2atmpS3192 - _M0L1iS1226;
            _M0L6_2atmpS3186 = _M0L6_2atmpS3191 - 1;
            _M0L6_2atmpS3190 = _M0L6outputS1228 % 10ull;
            _M0L6_2atmpS3189 = (int32_t)_M0L6_2atmpS3190;
            _M0L6_2atmpS3188 = 48 + _M0L6_2atmpS3189;
            _M0L6_2atmpS3187 = _M0L6_2atmpS3188 & 0xff;
            if (
              _M0L6_2atmpS3186 < 0
              || _M0L6_2atmpS3186 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3186] = _M0L6_2atmpS3187;
            _M0L6_2atmpS3193 = _M0L1iS1226 + 1;
            _M0L6_2atmpS3194 = _M0L6outputS1228 / 10ull;
            _M0L1iS1226 = _M0L6_2atmpS3193;
            _M0L7currentS1227 = _M0L7currentS1229;
            _M0L6outputS1228 = _M0L6_2atmpS3194;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3203 = _M0Lm5indexS1196;
        _M0L6_2atmpS3204 = _M0L7olengthS1200 + 1;
        _M0Lm5indexS1196 = _M0L6_2atmpS3203 + _M0L6_2atmpS3204;
      }
    }
    _M0L6_2atmpS3205 = _M0Lm5indexS1196;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4078
    = _M0FPB19string__from__bytes(_M0L6resultS1195, 0, _M0L6_2atmpS3205);
    moonbit_decref(_M0L6resultS1195);
    return _result_4078;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1141,
  uint32_t _M0L12ieeeExponentS1140
) {
  int32_t _M0Lm2e2S1138;
  uint64_t _M0Lm2m2S1139;
  uint64_t _M0L6_2atmpS3079;
  uint64_t _M0L6_2atmpS3078;
  int32_t _M0L4evenS1142;
  uint64_t _M0L6_2atmpS3077;
  uint64_t _M0L2mvS1143;
  int32_t _M0L7mmShiftS1144;
  uint64_t _M0Lm2vrS1145;
  uint64_t _M0Lm2vpS1146;
  uint64_t _M0Lm2vmS1147;
  int32_t _M0Lm3e10S1148;
  int32_t _M0Lm17vmIsTrailingZerosS1149;
  int32_t _M0Lm17vrIsTrailingZerosS1150;
  int32_t _M0L6_2atmpS2979;
  int32_t _M0Lm7removedS1169;
  int32_t _M0Lm16lastRemovedDigitS1170;
  uint64_t _M0Lm6outputS1171;
  int32_t _M0L6_2atmpS3075;
  int32_t _M0L6_2atmpS3076;
  int32_t _M0L3expS1194;
  uint64_t _M0L6_2atmpS3074;
  struct _M0TPB17FloatingDecimal64* _block_4084;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1138 = 0;
  _M0Lm2m2S1139 = 0ull;
  if (_M0L12ieeeExponentS1140 == 0u) {
    _M0Lm2e2S1138 = -1076;
    _M0Lm2m2S1139 = _M0L12ieeeMantissaS1141;
  } else {
    int32_t _M0L6_2atmpS2978 = *(int32_t*)&_M0L12ieeeExponentS1140;
    int32_t _M0L6_2atmpS2977 = _M0L6_2atmpS2978 - 1023;
    int32_t _M0L6_2atmpS2976 = _M0L6_2atmpS2977 - 52;
    _M0Lm2e2S1138 = _M0L6_2atmpS2976 - 2;
    _M0Lm2m2S1139 = 4503599627370496ull | _M0L12ieeeMantissaS1141;
  }
  _M0L6_2atmpS3079 = _M0Lm2m2S1139;
  _M0L6_2atmpS3078 = _M0L6_2atmpS3079 & 1ull;
  _M0L4evenS1142 = _M0L6_2atmpS3078 == 0ull;
  _M0L6_2atmpS3077 = _M0Lm2m2S1139;
  _M0L2mvS1143 = 4ull * _M0L6_2atmpS3077;
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
  _M0L6_2atmpS2979 = _M0Lm2e2S1138;
  if (_M0L6_2atmpS2979 >= 0) {
    int32_t _M0L6_2atmpS3001 = _M0Lm2e2S1138;
    int32_t _M0L6_2atmpS2997;
    int32_t _M0L6_2atmpS3000;
    int32_t _M0L6_2atmpS2999;
    int32_t _M0L6_2atmpS2998;
    int32_t _M0L1qS1151;
    int32_t _M0L6_2atmpS2996;
    int32_t _M0L6_2atmpS2995;
    int32_t _M0L1kS1152;
    int32_t _M0L6_2atmpS2994;
    int32_t _M0L6_2atmpS2993;
    int32_t _M0L6_2atmpS2992;
    int32_t _M0L1iS1153;
    struct _M0TPB8Pow5Pair _M0L4pow5S1154;
    uint64_t _M0L6_2atmpS2991;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1155;
    uint64_t _M0L8_2avrOutS1156;
    uint64_t _M0L8_2avpOutS1157;
    uint64_t _M0L8_2avmOutS1158;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2997 = _M0FPB9log10Pow2(_M0L6_2atmpS3001);
    _M0L6_2atmpS3000 = _M0Lm2e2S1138;
    _M0L6_2atmpS2999 = _M0L6_2atmpS3000 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2998 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2999);
    _M0L1qS1151 = _M0L6_2atmpS2997 - _M0L6_2atmpS2998;
    _M0Lm3e10S1148 = _M0L1qS1151;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS2996 = _M0FPB8pow5bits(_M0L1qS1151);
    _M0L6_2atmpS2995 = 125 + _M0L6_2atmpS2996;
    _M0L1kS1152 = _M0L6_2atmpS2995 - 1;
    _M0L6_2atmpS2994 = _M0Lm2e2S1138;
    _M0L6_2atmpS2993 = -_M0L6_2atmpS2994;
    _M0L6_2atmpS2992 = _M0L6_2atmpS2993 + _M0L1qS1151;
    _M0L1iS1153 = _M0L6_2atmpS2992 + _M0L1kS1152;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1154 = _M0FPB22double__computeInvPow5(_M0L1qS1151);
    _M0L6_2atmpS2991 = _M0Lm2m2S1139;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1155
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2991, _M0L4pow5S1154, _M0L1iS1153, _M0L7mmShiftS1144);
    _M0L8_2avrOutS1156 = _M0L7_2abindS1155.$0;
    _M0L8_2avpOutS1157 = _M0L7_2abindS1155.$1;
    _M0L8_2avmOutS1158 = _M0L7_2abindS1155.$2;
    _M0Lm2vrS1145 = _M0L8_2avrOutS1156;
    _M0Lm2vpS1146 = _M0L8_2avpOutS1157;
    _M0Lm2vmS1147 = _M0L8_2avmOutS1158;
    if (_M0L1qS1151 <= 21) {
      int32_t _M0L6_2atmpS2987 = (int32_t)_M0L2mvS1143;
      uint64_t _M0L6_2atmpS2990 = _M0L2mvS1143 / 5ull;
      int32_t _M0L6_2atmpS2989 = (int32_t)_M0L6_2atmpS2990;
      int32_t _M0L6_2atmpS2988 = 5 * _M0L6_2atmpS2989;
      int32_t _M0L6mvMod5S1159 = _M0L6_2atmpS2987 - _M0L6_2atmpS2988;
      if (_M0L6mvMod5S1159 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1150
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1143, _M0L1qS1151);
      } else if (_M0L4evenS1142) {
        uint64_t _M0L6_2atmpS2981 = _M0L2mvS1143 - 1ull;
        uint64_t _M0L6_2atmpS2982;
        uint64_t _M0L6_2atmpS2980;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2982 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1144);
        _M0L6_2atmpS2980 = _M0L6_2atmpS2981 - _M0L6_2atmpS2982;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1149
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2980, _M0L1qS1151);
      } else {
        uint64_t _M0L6_2atmpS2983 = _M0Lm2vpS1146;
        uint64_t _M0L6_2atmpS2986 = _M0L2mvS1143 + 2ull;
        int32_t _M0L6_2atmpS2985;
        uint64_t _M0L6_2atmpS2984;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2985
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2986, _M0L1qS1151);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS2984 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2985);
        _M0Lm2vpS1146 = _M0L6_2atmpS2983 - _M0L6_2atmpS2984;
      }
    }
  } else {
    int32_t _M0L6_2atmpS3015 = _M0Lm2e2S1138;
    int32_t _M0L6_2atmpS3014 = -_M0L6_2atmpS3015;
    int32_t _M0L6_2atmpS3009;
    int32_t _M0L6_2atmpS3013;
    int32_t _M0L6_2atmpS3012;
    int32_t _M0L6_2atmpS3011;
    int32_t _M0L6_2atmpS3010;
    int32_t _M0L1qS1160;
    int32_t _M0L6_2atmpS3002;
    int32_t _M0L6_2atmpS3008;
    int32_t _M0L6_2atmpS3007;
    int32_t _M0L1iS1161;
    int32_t _M0L6_2atmpS3006;
    int32_t _M0L1kS1162;
    int32_t _M0L1jS1163;
    struct _M0TPB8Pow5Pair _M0L4pow5S1164;
    uint64_t _M0L6_2atmpS3005;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1165;
    uint64_t _M0L8_2avrOutS1166;
    uint64_t _M0L8_2avpOutS1167;
    uint64_t _M0L8_2avmOutS1168;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3009 = _M0FPB9log10Pow5(_M0L6_2atmpS3014);
    _M0L6_2atmpS3013 = _M0Lm2e2S1138;
    _M0L6_2atmpS3012 = -_M0L6_2atmpS3013;
    _M0L6_2atmpS3011 = _M0L6_2atmpS3012 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3010 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3011);
    _M0L1qS1160 = _M0L6_2atmpS3009 - _M0L6_2atmpS3010;
    _M0L6_2atmpS3002 = _M0Lm2e2S1138;
    _M0Lm3e10S1148 = _M0L1qS1160 + _M0L6_2atmpS3002;
    _M0L6_2atmpS3008 = _M0Lm2e2S1138;
    _M0L6_2atmpS3007 = -_M0L6_2atmpS3008;
    _M0L1iS1161 = _M0L6_2atmpS3007 - _M0L1qS1160;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3006 = _M0FPB8pow5bits(_M0L1iS1161);
    _M0L1kS1162 = _M0L6_2atmpS3006 - 125;
    _M0L1jS1163 = _M0L1qS1160 - _M0L1kS1162;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1164 = _M0FPB19double__computePow5(_M0L1iS1161);
    _M0L6_2atmpS3005 = _M0Lm2m2S1139;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1165
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3005, _M0L4pow5S1164, _M0L1jS1163, _M0L7mmShiftS1144);
    _M0L8_2avrOutS1166 = _M0L7_2abindS1165.$0;
    _M0L8_2avpOutS1167 = _M0L7_2abindS1165.$1;
    _M0L8_2avmOutS1168 = _M0L7_2abindS1165.$2;
    _M0Lm2vrS1145 = _M0L8_2avrOutS1166;
    _M0Lm2vpS1146 = _M0L8_2avpOutS1167;
    _M0Lm2vmS1147 = _M0L8_2avmOutS1168;
    if (_M0L1qS1160 <= 1) {
      _M0Lm17vrIsTrailingZerosS1150 = 1;
      if (_M0L4evenS1142) {
        int32_t _M0L6_2atmpS3003;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3003 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1144);
        _M0Lm17vmIsTrailingZerosS1149 = _M0L6_2atmpS3003 == 1;
      } else {
        uint64_t _M0L6_2atmpS3004 = _M0Lm2vpS1146;
        _M0Lm2vpS1146 = _M0L6_2atmpS3004 - 1ull;
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
    int32_t _if__result_4081;
    uint64_t _M0L6_2atmpS3045;
    uint64_t _M0L6_2atmpS3051;
    uint64_t _M0L6_2atmpS3052;
    int32_t _if__result_4082;
    int32_t _M0L6_2atmpS3048;
    int64_t _M0L6_2atmpS3047;
    uint64_t _M0L6_2atmpS3046;
    while (1) {
      uint64_t _M0L6_2atmpS3028 = _M0Lm2vpS1146;
      uint64_t _M0L7vpDiv10S1172 = _M0L6_2atmpS3028 / 10ull;
      uint64_t _M0L6_2atmpS3027 = _M0Lm2vmS1147;
      uint64_t _M0L7vmDiv10S1173 = _M0L6_2atmpS3027 / 10ull;
      uint64_t _M0L6_2atmpS3026;
      int32_t _M0L6_2atmpS3023;
      int32_t _M0L6_2atmpS3025;
      int32_t _M0L6_2atmpS3024;
      int32_t _M0L7vmMod10S1175;
      uint64_t _M0L6_2atmpS3022;
      uint64_t _M0L7vrDiv10S1176;
      uint64_t _M0L6_2atmpS3021;
      int32_t _M0L6_2atmpS3018;
      int32_t _M0L6_2atmpS3020;
      int32_t _M0L6_2atmpS3019;
      int32_t _M0L7vrMod10S1177;
      int32_t _M0L6_2atmpS3017;
      if (_M0L7vpDiv10S1172 <= _M0L7vmDiv10S1173) {
        break;
      }
      _M0L6_2atmpS3026 = _M0Lm2vmS1147;
      _M0L6_2atmpS3023 = (int32_t)_M0L6_2atmpS3026;
      _M0L6_2atmpS3025 = (int32_t)_M0L7vmDiv10S1173;
      _M0L6_2atmpS3024 = 10 * _M0L6_2atmpS3025;
      _M0L7vmMod10S1175 = _M0L6_2atmpS3023 - _M0L6_2atmpS3024;
      _M0L6_2atmpS3022 = _M0Lm2vrS1145;
      _M0L7vrDiv10S1176 = _M0L6_2atmpS3022 / 10ull;
      _M0L6_2atmpS3021 = _M0Lm2vrS1145;
      _M0L6_2atmpS3018 = (int32_t)_M0L6_2atmpS3021;
      _M0L6_2atmpS3020 = (int32_t)_M0L7vrDiv10S1176;
      _M0L6_2atmpS3019 = 10 * _M0L6_2atmpS3020;
      _M0L7vrMod10S1177 = _M0L6_2atmpS3018 - _M0L6_2atmpS3019;
      if (_M0Lm17vmIsTrailingZerosS1149) {
        _M0Lm17vmIsTrailingZerosS1149 = _M0L7vmMod10S1175 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1149 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1150) {
        int32_t _M0L6_2atmpS3016 = _M0Lm16lastRemovedDigitS1170;
        _M0Lm17vrIsTrailingZerosS1150 = _M0L6_2atmpS3016 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1150 = 0;
      }
      _M0Lm16lastRemovedDigitS1170 = _M0L7vrMod10S1177;
      _M0Lm2vrS1145 = _M0L7vrDiv10S1176;
      _M0Lm2vpS1146 = _M0L7vpDiv10S1172;
      _M0Lm2vmS1147 = _M0L7vmDiv10S1173;
      _M0L6_2atmpS3017 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS3017 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1149) {
      while (1) {
        uint64_t _M0L6_2atmpS3041 = _M0Lm2vmS1147;
        uint64_t _M0L7vmDiv10S1178 = _M0L6_2atmpS3041 / 10ull;
        uint64_t _M0L6_2atmpS3040 = _M0Lm2vmS1147;
        int32_t _M0L6_2atmpS3037 = (int32_t)_M0L6_2atmpS3040;
        int32_t _M0L6_2atmpS3039 = (int32_t)_M0L7vmDiv10S1178;
        int32_t _M0L6_2atmpS3038 = 10 * _M0L6_2atmpS3039;
        int32_t _M0L7vmMod10S1179 = _M0L6_2atmpS3037 - _M0L6_2atmpS3038;
        uint64_t _M0L6_2atmpS3036;
        uint64_t _M0L7vpDiv10S1181;
        uint64_t _M0L6_2atmpS3035;
        uint64_t _M0L7vrDiv10S1182;
        uint64_t _M0L6_2atmpS3034;
        int32_t _M0L6_2atmpS3031;
        int32_t _M0L6_2atmpS3033;
        int32_t _M0L6_2atmpS3032;
        int32_t _M0L7vrMod10S1183;
        int32_t _M0L6_2atmpS3030;
        if (_M0L7vmMod10S1179 != 0) {
          break;
        }
        _M0L6_2atmpS3036 = _M0Lm2vpS1146;
        _M0L7vpDiv10S1181 = _M0L6_2atmpS3036 / 10ull;
        _M0L6_2atmpS3035 = _M0Lm2vrS1145;
        _M0L7vrDiv10S1182 = _M0L6_2atmpS3035 / 10ull;
        _M0L6_2atmpS3034 = _M0Lm2vrS1145;
        _M0L6_2atmpS3031 = (int32_t)_M0L6_2atmpS3034;
        _M0L6_2atmpS3033 = (int32_t)_M0L7vrDiv10S1182;
        _M0L6_2atmpS3032 = 10 * _M0L6_2atmpS3033;
        _M0L7vrMod10S1183 = _M0L6_2atmpS3031 - _M0L6_2atmpS3032;
        if (_M0Lm17vrIsTrailingZerosS1150) {
          int32_t _M0L6_2atmpS3029 = _M0Lm16lastRemovedDigitS1170;
          _M0Lm17vrIsTrailingZerosS1150 = _M0L6_2atmpS3029 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1150 = 0;
        }
        _M0Lm16lastRemovedDigitS1170 = _M0L7vrMod10S1183;
        _M0Lm2vrS1145 = _M0L7vrDiv10S1182;
        _M0Lm2vpS1146 = _M0L7vpDiv10S1181;
        _M0Lm2vmS1147 = _M0L7vmDiv10S1178;
        _M0L6_2atmpS3030 = _M0Lm7removedS1169;
        _M0Lm7removedS1169 = _M0L6_2atmpS3030 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1150) {
      int32_t _M0L6_2atmpS3044 = _M0Lm16lastRemovedDigitS1170;
      if (_M0L6_2atmpS3044 == 5) {
        uint64_t _M0L6_2atmpS3043 = _M0Lm2vrS1145;
        uint64_t _M0L6_2atmpS3042 = _M0L6_2atmpS3043 % 2ull;
        _if__result_4081 = _M0L6_2atmpS3042 == 0ull;
      } else {
        _if__result_4081 = 0;
      }
    } else {
      _if__result_4081 = 0;
    }
    if (_if__result_4081) {
      _M0Lm16lastRemovedDigitS1170 = 4;
    }
    _M0L6_2atmpS3045 = _M0Lm2vrS1145;
    _M0L6_2atmpS3051 = _M0Lm2vrS1145;
    _M0L6_2atmpS3052 = _M0Lm2vmS1147;
    if (_M0L6_2atmpS3051 == _M0L6_2atmpS3052) {
      if (!_M0L4evenS1142) {
        _if__result_4082 = 1;
      } else {
        int32_t _M0L6_2atmpS3050 = _M0Lm17vmIsTrailingZerosS1149;
        _if__result_4082 = !_M0L6_2atmpS3050;
      }
    } else {
      _if__result_4082 = 0;
    }
    if (_if__result_4082) {
      _M0L6_2atmpS3048 = 1;
    } else {
      int32_t _M0L6_2atmpS3049 = _M0Lm16lastRemovedDigitS1170;
      _M0L6_2atmpS3048 = _M0L6_2atmpS3049 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3047 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3048);
    _M0L6_2atmpS3046 = *(uint64_t*)&_M0L6_2atmpS3047;
    _M0Lm6outputS1171 = _M0L6_2atmpS3045 + _M0L6_2atmpS3046;
  } else {
    int32_t _M0Lm7roundUpS1184 = 0;
    uint64_t _M0L6_2atmpS3073 = _M0Lm2vpS1146;
    uint64_t _M0L8vpDiv100S1185 = _M0L6_2atmpS3073 / 100ull;
    uint64_t _M0L6_2atmpS3072 = _M0Lm2vmS1147;
    uint64_t _M0L8vmDiv100S1186 = _M0L6_2atmpS3072 / 100ull;
    uint64_t _M0L6_2atmpS3067;
    uint64_t _M0L6_2atmpS3070;
    uint64_t _M0L6_2atmpS3071;
    int32_t _M0L6_2atmpS3069;
    uint64_t _M0L6_2atmpS3068;
    if (_M0L8vpDiv100S1185 > _M0L8vmDiv100S1186) {
      uint64_t _M0L6_2atmpS3058 = _M0Lm2vrS1145;
      uint64_t _M0L8vrDiv100S1187 = _M0L6_2atmpS3058 / 100ull;
      uint64_t _M0L6_2atmpS3057 = _M0Lm2vrS1145;
      int32_t _M0L6_2atmpS3054 = (int32_t)_M0L6_2atmpS3057;
      int32_t _M0L6_2atmpS3056 = (int32_t)_M0L8vrDiv100S1187;
      int32_t _M0L6_2atmpS3055 = 100 * _M0L6_2atmpS3056;
      int32_t _M0L8vrMod100S1188 = _M0L6_2atmpS3054 - _M0L6_2atmpS3055;
      int32_t _M0L6_2atmpS3053;
      _M0Lm7roundUpS1184 = _M0L8vrMod100S1188 >= 50;
      _M0Lm2vrS1145 = _M0L8vrDiv100S1187;
      _M0Lm2vpS1146 = _M0L8vpDiv100S1185;
      _M0Lm2vmS1147 = _M0L8vmDiv100S1186;
      _M0L6_2atmpS3053 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS3053 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3066 = _M0Lm2vpS1146;
      uint64_t _M0L7vpDiv10S1189 = _M0L6_2atmpS3066 / 10ull;
      uint64_t _M0L6_2atmpS3065 = _M0Lm2vmS1147;
      uint64_t _M0L7vmDiv10S1190 = _M0L6_2atmpS3065 / 10ull;
      uint64_t _M0L6_2atmpS3064;
      uint64_t _M0L7vrDiv10S1192;
      uint64_t _M0L6_2atmpS3063;
      int32_t _M0L6_2atmpS3060;
      int32_t _M0L6_2atmpS3062;
      int32_t _M0L6_2atmpS3061;
      int32_t _M0L7vrMod10S1193;
      int32_t _M0L6_2atmpS3059;
      if (_M0L7vpDiv10S1189 <= _M0L7vmDiv10S1190) {
        break;
      }
      _M0L6_2atmpS3064 = _M0Lm2vrS1145;
      _M0L7vrDiv10S1192 = _M0L6_2atmpS3064 / 10ull;
      _M0L6_2atmpS3063 = _M0Lm2vrS1145;
      _M0L6_2atmpS3060 = (int32_t)_M0L6_2atmpS3063;
      _M0L6_2atmpS3062 = (int32_t)_M0L7vrDiv10S1192;
      _M0L6_2atmpS3061 = 10 * _M0L6_2atmpS3062;
      _M0L7vrMod10S1193 = _M0L6_2atmpS3060 - _M0L6_2atmpS3061;
      _M0Lm7roundUpS1184 = _M0L7vrMod10S1193 >= 5;
      _M0Lm2vrS1145 = _M0L7vrDiv10S1192;
      _M0Lm2vpS1146 = _M0L7vpDiv10S1189;
      _M0Lm2vmS1147 = _M0L7vmDiv10S1190;
      _M0L6_2atmpS3059 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS3059 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3067 = _M0Lm2vrS1145;
    _M0L6_2atmpS3070 = _M0Lm2vrS1145;
    _M0L6_2atmpS3071 = _M0Lm2vmS1147;
    _M0L6_2atmpS3069
    = _M0L6_2atmpS3070 == _M0L6_2atmpS3071 || _M0Lm7roundUpS1184;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3068 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3069);
    _M0Lm6outputS1171 = _M0L6_2atmpS3067 + _M0L6_2atmpS3068;
  }
  _M0L6_2atmpS3075 = _M0Lm3e10S1148;
  _M0L6_2atmpS3076 = _M0Lm7removedS1169;
  _M0L3expS1194 = _M0L6_2atmpS3075 + _M0L6_2atmpS3076;
  _M0L6_2atmpS3074 = _M0Lm6outputS1171;
  _block_4084
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4084)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_4084->$0 = _M0L6_2atmpS3074;
  _block_4084->$1 = _M0L3expS1194;
  return _block_4084;
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
  int32_t _M0L6_2atmpS2975;
  int32_t _M0L6_2atmpS2974;
  int32_t _M0L4baseS1116;
  int32_t _M0L5base2S1118;
  int32_t _M0L6offsetS1119;
  int32_t _M0L6_2atmpS2973;
  uint64_t _M0L4mul0S1120;
  int32_t _M0L6_2atmpS2972;
  int32_t _M0L6_2atmpS2971;
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
  int32_t _M0L6_2atmpS2969;
  int32_t _M0L6_2atmpS2970;
  int32_t _M0L5deltaS1131;
  uint64_t _M0L6_2atmpS2968;
  uint64_t _M0L6_2atmpS2960;
  int32_t _M0L6_2atmpS2967;
  uint32_t _M0L6_2atmpS2964;
  int32_t _M0L6_2atmpS2966;
  int32_t _M0L6_2atmpS2965;
  uint32_t _M0L6_2atmpS2963;
  uint32_t _M0L6_2atmpS2962;
  uint64_t _M0L6_2atmpS2961;
  uint64_t _M0L1aS1132;
  uint64_t _M0L6_2atmpS2959;
  uint64_t _M0L1bS1133;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2975 = _M0L1iS1117 + 26;
  _M0L6_2atmpS2974 = _M0L6_2atmpS2975 - 1;
  _M0L4baseS1116 = _M0L6_2atmpS2974 / 26;
  _M0L5base2S1118 = _M0L4baseS1116 * 26;
  _M0L6offsetS1119 = _M0L5base2S1118 - _M0L1iS1117;
  _M0L6_2atmpS2973 = _M0L4baseS1116 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1120
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2973);
  _M0L6_2atmpS2972 = _M0L4baseS1116 * 2;
  _M0L6_2atmpS2971 = _M0L6_2atmpS2972 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1121
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2971);
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
    uint64_t _M0L6_2atmpS2958 = _M0Lm5high1S1130;
    _M0Lm5high1S1130 = _M0L6_2atmpS2958 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2969 = _M0FPB8pow5bits(_M0L5base2S1118);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2970 = _M0FPB8pow5bits(_M0L1iS1117);
  _M0L5deltaS1131 = _M0L6_2atmpS2969 - _M0L6_2atmpS2970;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2968
  = _M0FPB13shiftright128(_M0L7_2alow0S1127, _M0L3sumS1129, _M0L5deltaS1131);
  _M0L6_2atmpS2960 = _M0L6_2atmpS2968 + 1ull;
  _M0L6_2atmpS2967 = _M0L1iS1117 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2964
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2967);
  _M0L6_2atmpS2966 = _M0L1iS1117 % 16;
  _M0L6_2atmpS2965 = _M0L6_2atmpS2966 << 1;
  _M0L6_2atmpS2963 = _M0L6_2atmpS2964 >> (_M0L6_2atmpS2965 & 31);
  _M0L6_2atmpS2962 = _M0L6_2atmpS2963 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2961 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2962);
  _M0L1aS1132 = _M0L6_2atmpS2960 + _M0L6_2atmpS2961;
  _M0L6_2atmpS2959 = _M0Lm5high1S1130;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1133
  = _M0FPB13shiftright128(_M0L3sumS1129, _M0L6_2atmpS2959, _M0L5deltaS1131);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1132, .$1 = _M0L1bS1133};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1099) {
  int32_t _M0L4baseS1098;
  int32_t _M0L5base2S1100;
  int32_t _M0L6offsetS1101;
  int32_t _M0L6_2atmpS2957;
  uint64_t _M0L4mul0S1102;
  int32_t _M0L6_2atmpS2956;
  int32_t _M0L6_2atmpS2955;
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
  int32_t _M0L6_2atmpS2953;
  int32_t _M0L6_2atmpS2954;
  int32_t _M0L5deltaS1113;
  uint64_t _M0L6_2atmpS2945;
  int32_t _M0L6_2atmpS2952;
  uint32_t _M0L6_2atmpS2949;
  int32_t _M0L6_2atmpS2951;
  int32_t _M0L6_2atmpS2950;
  uint32_t _M0L6_2atmpS2948;
  uint32_t _M0L6_2atmpS2947;
  uint64_t _M0L6_2atmpS2946;
  uint64_t _M0L1aS1114;
  uint64_t _M0L6_2atmpS2944;
  uint64_t _M0L1bS1115;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1098 = _M0L1iS1099 / 26;
  _M0L5base2S1100 = _M0L4baseS1098 * 26;
  _M0L6offsetS1101 = _M0L1iS1099 - _M0L5base2S1100;
  _M0L6_2atmpS2957 = _M0L4baseS1098 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1102
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2957);
  _M0L6_2atmpS2956 = _M0L4baseS1098 * 2;
  _M0L6_2atmpS2955 = _M0L6_2atmpS2956 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1103
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2955);
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
    uint64_t _M0L6_2atmpS2943 = _M0Lm5high1S1112;
    _M0Lm5high1S1112 = _M0L6_2atmpS2943 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2953 = _M0FPB8pow5bits(_M0L1iS1099);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2954 = _M0FPB8pow5bits(_M0L5base2S1100);
  _M0L5deltaS1113 = _M0L6_2atmpS2953 - _M0L6_2atmpS2954;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2945
  = _M0FPB13shiftright128(_M0L7_2alow0S1109, _M0L3sumS1111, _M0L5deltaS1113);
  _M0L6_2atmpS2952 = _M0L1iS1099 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2949
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2952);
  _M0L6_2atmpS2951 = _M0L1iS1099 % 16;
  _M0L6_2atmpS2950 = _M0L6_2atmpS2951 << 1;
  _M0L6_2atmpS2948 = _M0L6_2atmpS2949 >> (_M0L6_2atmpS2950 & 31);
  _M0L6_2atmpS2947 = _M0L6_2atmpS2948 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2946 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2947);
  _M0L1aS1114 = _M0L6_2atmpS2945 + _M0L6_2atmpS2946;
  _M0L6_2atmpS2944 = _M0Lm5high1S1112;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1115
  = _M0FPB13shiftright128(_M0L3sumS1111, _M0L6_2atmpS2944, _M0L5deltaS1113);
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
  uint64_t _M0L6_2atmpS2942;
  uint64_t _M0L2hiS1080;
  uint64_t _M0L3lo2S1081;
  uint64_t _M0L6_2atmpS2940;
  uint64_t _M0L6_2atmpS2941;
  uint64_t _M0L4mid2S1082;
  uint64_t _M0L6_2atmpS2939;
  uint64_t _M0L3hi2S1083;
  int32_t _M0L6_2atmpS2938;
  int32_t _M0L6_2atmpS2937;
  uint64_t _M0L2vpS1084;
  uint64_t _M0Lm2vmS1086;
  int32_t _M0L6_2atmpS2936;
  int32_t _M0L6_2atmpS2935;
  uint64_t _M0L2vrS1097;
  uint64_t _M0L6_2atmpS2934;
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
    _M0L6_2atmpS2942 = 1ull;
  } else {
    _M0L6_2atmpS2942 = 0ull;
  }
  _M0L2hiS1080 = _M0L6_2ahi2S1078 + _M0L6_2atmpS2942;
  _M0L3lo2S1081 = _M0L5_2aloS1074 + _M0L7_2amul0S1068;
  _M0L6_2atmpS2940 = _M0L3midS1079 + _M0L7_2amul1S1070;
  if (_M0L3lo2S1081 < _M0L5_2aloS1074) {
    _M0L6_2atmpS2941 = 1ull;
  } else {
    _M0L6_2atmpS2941 = 0ull;
  }
  _M0L4mid2S1082 = _M0L6_2atmpS2940 + _M0L6_2atmpS2941;
  if (_M0L4mid2S1082 < _M0L3midS1079) {
    _M0L6_2atmpS2939 = 1ull;
  } else {
    _M0L6_2atmpS2939 = 0ull;
  }
  _M0L3hi2S1083 = _M0L2hiS1080 + _M0L6_2atmpS2939;
  _M0L6_2atmpS2938 = _M0L1jS1085 - 64;
  _M0L6_2atmpS2937 = _M0L6_2atmpS2938 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1084
  = _M0FPB13shiftright128(_M0L4mid2S1082, _M0L3hi2S1083, _M0L6_2atmpS2937);
  _M0Lm2vmS1086 = 0ull;
  if (_M0L7mmShiftS1087) {
    uint64_t _M0L3lo3S1088 = _M0L5_2aloS1074 - _M0L7_2amul0S1068;
    uint64_t _M0L6_2atmpS2924 = _M0L3midS1079 - _M0L7_2amul1S1070;
    uint64_t _M0L6_2atmpS2925;
    uint64_t _M0L4mid3S1089;
    uint64_t _M0L6_2atmpS2923;
    uint64_t _M0L3hi3S1090;
    int32_t _M0L6_2atmpS2922;
    int32_t _M0L6_2atmpS2921;
    if (_M0L5_2aloS1074 < _M0L3lo3S1088) {
      _M0L6_2atmpS2925 = 1ull;
    } else {
      _M0L6_2atmpS2925 = 0ull;
    }
    _M0L4mid3S1089 = _M0L6_2atmpS2924 - _M0L6_2atmpS2925;
    if (_M0L3midS1079 < _M0L4mid3S1089) {
      _M0L6_2atmpS2923 = 1ull;
    } else {
      _M0L6_2atmpS2923 = 0ull;
    }
    _M0L3hi3S1090 = _M0L2hiS1080 - _M0L6_2atmpS2923;
    _M0L6_2atmpS2922 = _M0L1jS1085 - 64;
    _M0L6_2atmpS2921 = _M0L6_2atmpS2922 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1086
    = _M0FPB13shiftright128(_M0L4mid3S1089, _M0L3hi3S1090, _M0L6_2atmpS2921);
  } else {
    uint64_t _M0L3lo3S1091 = _M0L5_2aloS1074 + _M0L5_2aloS1074;
    uint64_t _M0L6_2atmpS2932 = _M0L3midS1079 + _M0L3midS1079;
    uint64_t _M0L6_2atmpS2933;
    uint64_t _M0L4mid3S1092;
    uint64_t _M0L6_2atmpS2930;
    uint64_t _M0L6_2atmpS2931;
    uint64_t _M0L3hi3S1093;
    uint64_t _M0L3lo4S1094;
    uint64_t _M0L6_2atmpS2928;
    uint64_t _M0L6_2atmpS2929;
    uint64_t _M0L4mid4S1095;
    uint64_t _M0L6_2atmpS2927;
    uint64_t _M0L3hi4S1096;
    int32_t _M0L6_2atmpS2926;
    if (_M0L3lo3S1091 < _M0L5_2aloS1074) {
      _M0L6_2atmpS2933 = 1ull;
    } else {
      _M0L6_2atmpS2933 = 0ull;
    }
    _M0L4mid3S1092 = _M0L6_2atmpS2932 + _M0L6_2atmpS2933;
    _M0L6_2atmpS2930 = _M0L2hiS1080 + _M0L2hiS1080;
    if (_M0L4mid3S1092 < _M0L3midS1079) {
      _M0L6_2atmpS2931 = 1ull;
    } else {
      _M0L6_2atmpS2931 = 0ull;
    }
    _M0L3hi3S1093 = _M0L6_2atmpS2930 + _M0L6_2atmpS2931;
    _M0L3lo4S1094 = _M0L3lo3S1091 - _M0L7_2amul0S1068;
    _M0L6_2atmpS2928 = _M0L4mid3S1092 - _M0L7_2amul1S1070;
    if (_M0L3lo3S1091 < _M0L3lo4S1094) {
      _M0L6_2atmpS2929 = 1ull;
    } else {
      _M0L6_2atmpS2929 = 0ull;
    }
    _M0L4mid4S1095 = _M0L6_2atmpS2928 - _M0L6_2atmpS2929;
    if (_M0L4mid3S1092 < _M0L4mid4S1095) {
      _M0L6_2atmpS2927 = 1ull;
    } else {
      _M0L6_2atmpS2927 = 0ull;
    }
    _M0L3hi4S1096 = _M0L3hi3S1093 - _M0L6_2atmpS2927;
    _M0L6_2atmpS2926 = _M0L1jS1085 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1086
    = _M0FPB13shiftright128(_M0L4mid4S1095, _M0L3hi4S1096, _M0L6_2atmpS2926);
  }
  _M0L6_2atmpS2936 = _M0L1jS1085 - 64;
  _M0L6_2atmpS2935 = _M0L6_2atmpS2936 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1097
  = _M0FPB13shiftright128(_M0L3midS1079, _M0L2hiS1080, _M0L6_2atmpS2935);
  _M0L6_2atmpS2934 = _M0Lm2vmS1086;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1097,
                                                .$1 = _M0L2vpS1084,
                                                .$2 = _M0L6_2atmpS2934};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1066,
  int32_t _M0L1pS1067
) {
  uint64_t _M0L6_2atmpS2920;
  uint64_t _M0L6_2atmpS2919;
  uint64_t _M0L6_2atmpS2918;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2920 = 1ull << (_M0L1pS1067 & 63);
  _M0L6_2atmpS2919 = _M0L6_2atmpS2920 - 1ull;
  _M0L6_2atmpS2918 = _M0L5valueS1066 & _M0L6_2atmpS2919;
  return _M0L6_2atmpS2918 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1064,
  int32_t _M0L1pS1065
) {
  int32_t _M0L6_2atmpS2917;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2917 = _M0FPB10pow5Factor(_M0L5valueS1064);
  return _M0L6_2atmpS2917 >= _M0L1pS1065;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1059) {
  uint64_t _M0L6_2atmpS2908;
  uint64_t _M0L6_2atmpS2909;
  uint64_t _M0L6_2atmpS2910;
  uint64_t _M0L6_2atmpS2911;
  uint64_t _M0L6_2atmpS2916;
  int32_t _M0L5countS1060;
  uint64_t _M0L1vS1061;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2908 = _M0L5valueS1059 % 5ull;
  if (_M0L6_2atmpS2908 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2909 = _M0L5valueS1059 % 25ull;
  if (_M0L6_2atmpS2909 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2910 = _M0L5valueS1059 % 125ull;
  if (_M0L6_2atmpS2910 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2911 = _M0L5valueS1059 % 625ull;
  if (_M0L6_2atmpS2911 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS2916 = _M0L5valueS1059 / 625ull;
  _M0L5countS1060 = 4;
  _M0L1vS1061 = _M0L6_2atmpS2916;
  while (1) {
    if (_M0L1vS1061 > 0ull) {
      uint64_t _M0L6_2atmpS2912 = _M0L1vS1061 % 5ull;
      int32_t _M0L6_2atmpS2913;
      uint64_t _M0L6_2atmpS2914;
      if (_M0L6_2atmpS2912 != 0ull) {
        return _M0L5countS1060;
      }
      _M0L6_2atmpS2913 = _M0L5countS1060 + 1;
      _M0L6_2atmpS2914 = _M0L1vS1061 / 5ull;
      _M0L5countS1060 = _M0L6_2atmpS2913;
      _M0L1vS1061 = _M0L6_2atmpS2914;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1063;
      moonbit_string_t _M0L6_2atmpS2915;
      int32_t _result_4086;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1063
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1063, (moonbit_string_t)moonbit_string_literal_21.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1063, _M0L5valueS1059);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS2915
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1063);
      moonbit_decref(_M0L18_2astring__builderS1063);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_4086 = _M0FPC15abort5abortGiE(_M0L6_2atmpS2915);
      moonbit_decref(_M0L6_2atmpS2915);
      return _result_4086;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1058,
  uint64_t _M0L2hiS1056,
  int32_t _M0L4distS1057
) {
  int32_t _M0L6_2atmpS2907;
  uint64_t _M0L6_2atmpS2905;
  uint64_t _M0L6_2atmpS2906;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2907 = 64 - _M0L4distS1057;
  _M0L6_2atmpS2905 = _M0L2hiS1056 << (_M0L6_2atmpS2907 & 63);
  _M0L6_2atmpS2906 = _M0L2loS1058 >> (_M0L4distS1057 & 63);
  return _M0L6_2atmpS2905 | _M0L6_2atmpS2906;
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
  uint64_t _M0L6_2atmpS2903;
  uint64_t _M0L6_2atmpS2904;
  uint64_t _M0L1yS1052;
  uint64_t _M0L6_2atmpS2901;
  uint64_t _M0L6_2atmpS2902;
  uint64_t _M0L1zS1053;
  uint64_t _M0L6_2atmpS2899;
  uint64_t _M0L6_2atmpS2900;
  uint64_t _M0L6_2atmpS2897;
  uint64_t _M0L6_2atmpS2898;
  uint64_t _M0L1wS1054;
  uint64_t _M0L2loS1055;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1045 = _M0L1aS1046 & 4294967295ull;
  _M0L3aHiS1047 = _M0L1aS1046 >> 32;
  _M0L3bLoS1048 = _M0L1bS1049 & 4294967295ull;
  _M0L3bHiS1050 = _M0L1bS1049 >> 32;
  _M0L1xS1051 = _M0L3aLoS1045 * _M0L3bLoS1048;
  _M0L6_2atmpS2903 = _M0L3aHiS1047 * _M0L3bLoS1048;
  _M0L6_2atmpS2904 = _M0L1xS1051 >> 32;
  _M0L1yS1052 = _M0L6_2atmpS2903 + _M0L6_2atmpS2904;
  _M0L6_2atmpS2901 = _M0L3aLoS1045 * _M0L3bHiS1050;
  _M0L6_2atmpS2902 = _M0L1yS1052 & 4294967295ull;
  _M0L1zS1053 = _M0L6_2atmpS2901 + _M0L6_2atmpS2902;
  _M0L6_2atmpS2899 = _M0L3aHiS1047 * _M0L3bHiS1050;
  _M0L6_2atmpS2900 = _M0L1yS1052 >> 32;
  _M0L6_2atmpS2897 = _M0L6_2atmpS2899 + _M0L6_2atmpS2900;
  _M0L6_2atmpS2898 = _M0L1zS1053 >> 32;
  _M0L1wS1054 = _M0L6_2atmpS2897 + _M0L6_2atmpS2898;
  _M0L2loS1055 = _M0L1aS1046 * _M0L1bS1049;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1055, .$1 = _M0L1wS1054};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1043,
  int32_t _M0L4fromS1040,
  int32_t _M0L2toS1039
) {
  int32_t _M0L3lenS1038;
  int32_t _M0L6_2atmpS2896;
  uint16_t* _M0L6bufferS1041;
  int32_t _M0L1iS1042;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1038 = _M0L2toS1039 - _M0L4fromS1040;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2896 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1041
  = (uint16_t*)moonbit_make_string(_M0L3lenS1038, _M0L6_2atmpS2896);
  _M0L1iS1042 = 0;
  while (1) {
    if (_M0L1iS1042 < _M0L3lenS1038) {
      int32_t _M0L6_2atmpS2894 = _M0L4fromS1040 + _M0L1iS1042;
      int32_t _M0L6_2atmpS2893;
      int32_t _M0L6_2atmpS2892;
      int32_t _M0L6_2atmpS2895;
      if (
        _M0L6_2atmpS2894 < 0
        || _M0L6_2atmpS2894 >= Moonbit_array_length(_M0L5bytesS1043)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2893 = (int32_t)_M0L5bytesS1043[_M0L6_2atmpS2894];
      _M0L6_2atmpS2892 = (uint16_t)_M0L6_2atmpS2893;
      if (
        _M0L1iS1042 < 0
        || _M0L1iS1042 >= Moonbit_array_length(_M0L6bufferS1041)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1041[_M0L1iS1042] = _M0L6_2atmpS2892;
      _M0L6_2atmpS2895 = _M0L1iS1042 + 1;
      _M0L1iS1042 = _M0L6_2atmpS2895;
      continue;
    }
    break;
  }
  return _M0L6bufferS1041;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1037) {
  int32_t _M0L6_2atmpS2891;
  uint32_t _M0L6_2atmpS2890;
  uint32_t _M0L6_2atmpS2889;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2891 = _M0L1eS1037 * 78913;
  _M0L6_2atmpS2890 = *(uint32_t*)&_M0L6_2atmpS2891;
  _M0L6_2atmpS2889 = _M0L6_2atmpS2890 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2889;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1036) {
  int32_t _M0L6_2atmpS2888;
  uint32_t _M0L6_2atmpS2887;
  uint32_t _M0L6_2atmpS2886;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2888 = _M0L1eS1036 * 732923;
  _M0L6_2atmpS2887 = *(uint32_t*)&_M0L6_2atmpS2888;
  _M0L6_2atmpS2886 = _M0L6_2atmpS2887 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2886;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1034,
  int32_t _M0L8exponentS1035,
  int32_t _M0L8mantissaS1032
) {
  moonbit_string_t _M0L1sS1033;
  moonbit_string_t _result_4089;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1032) {
    return (moonbit_string_t)moonbit_string_literal_22.data;
  }
  if (_M0L4signS1034) {
    _M0L1sS1033 = (moonbit_string_t)moonbit_string_literal_13.data;
  } else {
    _M0L1sS1033 = (moonbit_string_t)moonbit_string_literal_14.data;
  }
  if (_M0L8exponentS1035) {
    moonbit_string_t _result_4088;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4088
    = moonbit_add_string(_M0L1sS1033, (moonbit_string_t)moonbit_string_literal_23.data);
    moonbit_decref(_M0L1sS1033);
    return _result_4088;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4089
  = moonbit_add_string(_M0L1sS1033, (moonbit_string_t)moonbit_string_literal_24.data);
  moonbit_decref(_M0L1sS1033);
  return _result_4089;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1031) {
  int32_t _M0L6_2atmpS2885;
  uint32_t _M0L6_2atmpS2884;
  uint32_t _M0L6_2atmpS2883;
  int32_t _M0L6_2atmpS2882;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2885 = _M0L1eS1031 * 1217359;
  _M0L6_2atmpS2884 = *(uint32_t*)&_M0L6_2atmpS2885;
  _M0L6_2atmpS2883 = _M0L6_2atmpS2884 >> 19;
  _M0L6_2atmpS2882 = *(int32_t*)&_M0L6_2atmpS2883;
  return _M0L6_2atmpS2882 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1027) {
  int32_t _tmp_4090;
  uint32_t _M0L6_2atmpS2881;
  uint32_t _M0Lm3accS1025;
  int32_t _M0L7_2abindS1026;
  int32_t _M0L1iS1028;
  uint32_t _M0L6_2atmpS2880;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_4090 = 0;
  _M0L6_2atmpS2881 = *(uint32_t*)&_tmp_4090;
  _M0Lm3accS1025 = _M0L6_2atmpS2881 + 374761393u;
  _M0L7_2abindS1026 = Moonbit_array_length(_M0L4selfS1027);
  _M0L1iS1028 = 0;
  while (1) {
    if (_M0L1iS1028 < _M0L7_2abindS1026) {
      uint32_t _M0L6_2atmpS2875 = _M0Lm3accS1025;
      int32_t _M0L6_2atmpS2878;
      int32_t _M0L6_2atmpS2877;
      uint32_t _M0L1vS1029;
      uint32_t _M0L6_2atmpS2876;
      int32_t _M0L6_2atmpS2879;
      _M0Lm3accS1025 = _M0L6_2atmpS2875 + 4u;
      _M0L6_2atmpS2878 = _M0L4selfS1027[_M0L1iS1028];
      _M0L6_2atmpS2877 = (int32_t)_M0L6_2atmpS2878;
      _M0L1vS1029 = *(uint32_t*)&_M0L6_2atmpS2877;
      _M0L6_2atmpS2876 = _M0Lm3accS1025;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1025 = _M0FPB13consume4__acc(_M0L6_2atmpS2876, _M0L1vS1029);
      _M0L6_2atmpS2879 = _M0L1iS1028 + 1;
      _M0L1iS1028 = _M0L6_2atmpS2879;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2880 = _M0Lm3accS1025;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS2880);
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

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS1023
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsfEE(_M0L4selfS1023);
}

struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPB5Iter24nextGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0L4selfS1024
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE(_M0L4selfS1024);
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

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1019
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsfE(_M0L4selfS1019);
}

struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0MPB3Map5iter2GsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS1020
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS1020);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(
  struct _M0TPB3MapGssE* _M0L4selfS974
) {
  struct _M0TPB5EntryGssE* _M0L4headS2844;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS973;
  int32_t _M0L3lenS975;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS976;
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__* _closure_4092;
  struct _M0TWEOUssE* _M0L6_2atmpS2835;
  int64_t _M0L6_2atmpS2836;
  struct _M0TPB4IterGUssEE* _result_4093;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2844 = _M0L4selfS974->$5;
  if (_M0L4headS2844) {
    moonbit_incref(_M0L4headS2844);
  }
  _M0L11curr__entryS973
  = (struct _M0TPB8MutLocalGORPB5EntryGssEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE));
  Moonbit_object_header(_M0L11curr__entryS973)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 35, 0);
  _M0L11curr__entryS973->$0 = _M0L4headS2844;
  _M0L3lenS975 = _M0L4selfS974->$1;
  _M0L9remainingS976
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS976)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS976->$0 = _M0L3lenS975;
  _closure_4092
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__*)moonbit_malloc(sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__));
  Moonbit_object_header(_closure_4092)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 38, 0);
  _closure_4092->code = &_M0MPB3Map4iterGssEC2837l711;
  _closure_4092->$0 = _M0L9remainingS976;
  _closure_4092->$1 = _M0L11curr__entryS973;
  _M0L6_2atmpS2835 = (struct _M0TWEOUssE*)_closure_4092;
  _M0L6_2atmpS2836 = (int64_t)_M0L3lenS975;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4093 = _M0MPB4Iter3newGUssEE(_M0L6_2atmpS2835, _M0L6_2atmpS2836);
  moonbit_decref(_M0L6_2atmpS2835);
  return _result_4093;
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS985
) {
  struct _M0TPB5EntryGsbE* _M0L4headS2854;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS984;
  int32_t _M0L3lenS986;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS987;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__* _closure_4094;
  struct _M0TWEOUsbE* _M0L6_2atmpS2845;
  int64_t _M0L6_2atmpS2846;
  struct _M0TPB4IterGUsbEE* _result_4095;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2854 = _M0L4selfS985->$5;
  if (_M0L4headS2854) {
    moonbit_incref(_M0L4headS2854);
  }
  _M0L11curr__entryS984
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS984)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 42, 0);
  _M0L11curr__entryS984->$0 = _M0L4headS2854;
  _M0L3lenS986 = _M0L4selfS985->$1;
  _M0L9remainingS987
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS987)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS987->$0 = _M0L3lenS986;
  _closure_4094
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__));
  Moonbit_object_header(_closure_4094)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 45, 0);
  _closure_4094->code = &_M0MPB3Map4iterGsbEC2847l711;
  _closure_4094->$0 = _M0L9remainingS987;
  _closure_4094->$1 = _M0L11curr__entryS984;
  _M0L6_2atmpS2845 = (struct _M0TWEOUsbE*)_closure_4094;
  _M0L6_2atmpS2846 = (int64_t)_M0L3lenS986;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4095 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS2845, _M0L6_2atmpS2846);
  moonbit_decref(_M0L6_2atmpS2845);
  return _result_4095;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS996
) {
  struct _M0TPB5EntryGsfE* _M0L4headS2864;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS995;
  int32_t _M0L3lenS997;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS998;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__* _closure_4096;
  struct _M0TWEOUsfE* _M0L6_2atmpS2855;
  int64_t _M0L6_2atmpS2856;
  struct _M0TPB4IterGUsfEE* _result_4097;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2864 = _M0L4selfS996->$5;
  if (_M0L4headS2864) {
    moonbit_incref(_M0L4headS2864);
  }
  _M0L11curr__entryS995
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS995)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 49, 0);
  _M0L11curr__entryS995->$0 = _M0L4headS2864;
  _M0L3lenS997 = _M0L4selfS996->$1;
  _M0L9remainingS998
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS998)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS998->$0 = _M0L3lenS997;
  _closure_4096
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__));
  Moonbit_object_header(_closure_4096)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 52, 0);
  _closure_4096->code = &_M0MPB3Map4iterGsfEC2857l711;
  _closure_4096->$0 = _M0L9remainingS998;
  _closure_4096->$1 = _M0L11curr__entryS995;
  _M0L6_2atmpS2855 = (struct _M0TWEOUsfE*)_closure_4096;
  _M0L6_2atmpS2856 = (int64_t)_M0L3lenS997;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4097 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS2855, _M0L6_2atmpS2856);
  moonbit_decref(_M0L6_2atmpS2855);
  return _result_4097;
}

struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0MPB3Map4iterGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS1007
) {
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4headS2874;
  struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0L11curr__entryS1006;
  int32_t _M0L3lenS1008;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1009;
  struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__* _closure_4098;
  struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2865;
  int64_t _M0L6_2atmpS2866;
  struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _result_4099;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2874 = _M0L4selfS1007->$5;
  if (_M0L4headS2874) {
    moonbit_incref(_M0L4headS2874);
  }
  _M0L11curr__entryS1006
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS1006)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 56, 0);
  _M0L11curr__entryS1006->$0 = _M0L4headS2874;
  _M0L3lenS1008 = _M0L4selfS1007->$1;
  _M0L9remainingS1009
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1009)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1009->$0 = _M0L3lenS1008;
  _closure_4098
  = (struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__*)moonbit_malloc(sizeof(struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__));
  Moonbit_object_header(_closure_4098)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 59, 0);
  _closure_4098->code
  = &_M0MPB3Map4iterGsRP39moonbitdb9moonbitdb3lib10RedisValueEC2867l711;
  _closure_4098->$0 = _M0L9remainingS1009;
  _closure_4098->$1 = _M0L11curr__entryS1006;
  _M0L6_2atmpS2865
  = (struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_closure_4098;
  _M0L6_2atmpS2866 = (int64_t)_M0L3lenS1008;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4099
  = _M0MPB4Iter3newGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2865, _M0L6_2atmpS2866);
  moonbit_decref(_M0L6_2atmpS2865);
  return _result_4099;
}

struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPB3Map4iterGsRP39moonbitdb9moonbitdb3lib10RedisValueEC2867l711(
  struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aenvS2868
) {
  struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__* _M0L14_2acasted__envS2869;
  struct _M0TPB8MutLocalGORPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0L11curr__entryS1006;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1009;
  int32_t _M0L3valS2870;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2869
  = (struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitdb_2fmoonbitdb_2flib_2fRedisValue_5d_7c_2eanon__u2867__l711__*)_M0L6_2aenvS2868;
  _M0L11curr__entryS1006 = _M0L14_2acasted__envS2869->$1;
  _M0L9remainingS1009 = _M0L14_2acasted__envS2869->$0;
  _M0L3valS2870 = _M0L9remainingS1009->$0;
  if (_M0L3valS2870 > 0) {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS1011 =
      _M0L11curr__entryS1006->$0;
    if (_M0L7_2abindS1011 == 0) {
      goto join_1010;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS1012 =
        _M0L7_2abindS1011;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4_2axS1013 =
        _M0L7_2aSomeS1012;
      moonbit_string_t _M0L6_2akeyS1014 = _M0L4_2axS1013->$4;
      void* _M0L8_2avalueS1015 = _M0L4_2axS1013->$5;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2anextS1016 =
        _M0L4_2axS1013->$1;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3629 =
        _M0L11curr__entryS1006->$0;
      int32_t _M0L3valS2872;
      int32_t _M0L6_2atmpS2871;
      struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L8_2atupleS2873;
      if (_M0L7_2anextS1016) {
        moonbit_incref(_M0L7_2anextS1016);
      }
      moonbit_incref(_M0L8_2avalueS1015);
      moonbit_incref(_M0L6_2akeyS1014);
      if (_M0L6_2aoldS3629) {
        moonbit_decref(_M0L6_2aoldS3629);
      }
      _M0L11curr__entryS1006->$0 = _M0L7_2anextS1016;
      _M0L3valS2872 = _M0L9remainingS1009->$0;
      _M0L6_2atmpS2871 = _M0L3valS2872 - 1;
      _M0L9remainingS1009->$0 = _M0L6_2atmpS2871;
      _M0L8_2atupleS2873
      = (struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS2873)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 63, 0);
      _M0L8_2atupleS2873->$0 = _M0L6_2akeyS1014;
      _M0L8_2atupleS2873->$1 = _M0L8_2avalueS1015;
      return _M0L8_2atupleS2873;
    }
  } else {
    goto join_1010;
  }
  join_1010:;
  return 0;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2857l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS2858
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__* _M0L14_2acasted__envS2859;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS995;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS998;
  int32_t _M0L3valS2860;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2859
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2857__l711__*)_M0L6_2aenvS2858;
  _M0L11curr__entryS995 = _M0L14_2acasted__envS2859->$1;
  _M0L9remainingS998 = _M0L14_2acasted__envS2859->$0;
  _M0L3valS2860 = _M0L9remainingS998->$0;
  if (_M0L3valS2860 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1000 = _M0L11curr__entryS995->$0;
    if (_M0L7_2abindS1000 == 0) {
      goto join_999;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1001 = _M0L7_2abindS1000;
      struct _M0TPB5EntryGsfE* _M0L4_2axS1002 = _M0L7_2aSomeS1001;
      moonbit_string_t _M0L6_2akeyS1003 = _M0L4_2axS1002->$4;
      float _M0L8_2avalueS1004 = _M0L4_2axS1002->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1005 = _M0L4_2axS1002->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3634 = _M0L11curr__entryS995->$0;
      int32_t _M0L3valS2862;
      int32_t _M0L6_2atmpS2861;
      struct _M0TUsfE* _M0L8_2atupleS2863;
      if (_M0L7_2anextS1005) {
        moonbit_incref(_M0L7_2anextS1005);
      }
      moonbit_incref(_M0L6_2akeyS1003);
      if (_M0L6_2aoldS3634) {
        moonbit_decref(_M0L6_2aoldS3634);
      }
      _M0L11curr__entryS995->$0 = _M0L7_2anextS1005;
      _M0L3valS2862 = _M0L9remainingS998->$0;
      _M0L6_2atmpS2861 = _M0L3valS2862 - 1;
      _M0L9remainingS998->$0 = _M0L6_2atmpS2861;
      _M0L8_2atupleS2863
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS2863)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
      _M0L8_2atupleS2863->$0 = _M0L6_2akeyS1003;
      _M0L8_2atupleS2863->$1 = _M0L8_2avalueS1004;
      return _M0L8_2atupleS2863;
    }
  } else {
    goto join_999;
  }
  join_999:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2847l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS2848
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__* _M0L14_2acasted__envS2849;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS984;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS987;
  int32_t _M0L3valS2850;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2849
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2847__l711__*)_M0L6_2aenvS2848;
  _M0L11curr__entryS984 = _M0L14_2acasted__envS2849->$1;
  _M0L9remainingS987 = _M0L14_2acasted__envS2849->$0;
  _M0L3valS2850 = _M0L9remainingS987->$0;
  if (_M0L3valS2850 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS989 = _M0L11curr__entryS984->$0;
    if (_M0L7_2abindS989 == 0) {
      goto join_988;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS990 = _M0L7_2abindS989;
      struct _M0TPB5EntryGsbE* _M0L4_2axS991 = _M0L7_2aSomeS990;
      moonbit_string_t _M0L6_2akeyS992 = _M0L4_2axS991->$4;
      int32_t _M0L8_2avalueS993 = _M0L4_2axS991->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS994 = _M0L4_2axS991->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3638 = _M0L11curr__entryS984->$0;
      int32_t _M0L3valS2852;
      int32_t _M0L6_2atmpS2851;
      struct _M0TUsbE* _M0L8_2atupleS2853;
      if (_M0L7_2anextS994) {
        moonbit_incref(_M0L7_2anextS994);
      }
      moonbit_incref(_M0L6_2akeyS992);
      if (_M0L6_2aoldS3638) {
        moonbit_decref(_M0L6_2aoldS3638);
      }
      _M0L11curr__entryS984->$0 = _M0L7_2anextS994;
      _M0L3valS2852 = _M0L9remainingS987->$0;
      _M0L6_2atmpS2851 = _M0L3valS2852 - 1;
      _M0L9remainingS987->$0 = _M0L6_2atmpS2851;
      _M0L8_2atupleS2853
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS2853)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 67, 0);
      _M0L8_2atupleS2853->$0 = _M0L6_2akeyS992;
      _M0L8_2atupleS2853->$1 = _M0L8_2avalueS993;
      return _M0L8_2atupleS2853;
    }
  } else {
    goto join_988;
  }
  join_988:;
  return 0;
}

struct _M0TUssE* _M0MPB3Map4iterGssEC2837l711(
  struct _M0TWEOUssE* _M0L6_2aenvS2838
) {
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__* _M0L14_2acasted__envS2839;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS973;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS976;
  int32_t _M0L3valS2840;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2839
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2837__l711__*)_M0L6_2aenvS2838;
  _M0L11curr__entryS973 = _M0L14_2acasted__envS2839->$1;
  _M0L9remainingS976 = _M0L14_2acasted__envS2839->$0;
  _M0L3valS2840 = _M0L9remainingS976->$0;
  if (_M0L3valS2840 > 0) {
    struct _M0TPB5EntryGssE* _M0L7_2abindS978 = _M0L11curr__entryS973->$0;
    if (_M0L7_2abindS978 == 0) {
      goto join_977;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS979 = _M0L7_2abindS978;
      struct _M0TPB5EntryGssE* _M0L4_2axS980 = _M0L7_2aSomeS979;
      moonbit_string_t _M0L6_2akeyS981 = _M0L4_2axS980->$4;
      moonbit_string_t _M0L8_2avalueS982 = _M0L4_2axS980->$5;
      struct _M0TPB5EntryGssE* _M0L7_2anextS983 = _M0L4_2axS980->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3642 = _M0L11curr__entryS973->$0;
      int32_t _M0L3valS2842;
      int32_t _M0L6_2atmpS2841;
      struct _M0TUssE* _M0L8_2atupleS2843;
      if (_M0L7_2anextS983) {
        moonbit_incref(_M0L7_2anextS983);
      }
      moonbit_incref(_M0L8_2avalueS982);
      moonbit_incref(_M0L6_2akeyS981);
      if (_M0L6_2aoldS3642) {
        moonbit_decref(_M0L6_2aoldS3642);
      }
      _M0L11curr__entryS973->$0 = _M0L7_2anextS983;
      _M0L3valS2842 = _M0L9remainingS976->$0;
      _M0L6_2atmpS2841 = _M0L3valS2842 - 1;
      _M0L9remainingS976->$0 = _M0L6_2atmpS2841;
      _M0L8_2atupleS2843
      = (struct _M0TUssE*)moonbit_malloc(sizeof(struct _M0TUssE));
      Moonbit_object_header(_M0L8_2atupleS2843)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 70, 0);
      _M0L8_2atupleS2843->$0 = _M0L6_2akeyS981;
      _M0L8_2atupleS2843->$1 = _M0L8_2avalueS982;
      return _M0L8_2atupleS2843;
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
  int32_t _M0L6_2atmpS2833;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2833 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS967);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS966, _M0L3keyS967, _M0L6_2atmpS2833);
  return 0;
}

int32_t _M0MPB3Map6removeGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS968,
  moonbit_string_t _M0L3keyS969
) {
  int32_t _M0L6_2atmpS2834;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2834 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS969);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS968, _M0L3keyS969, _M0L6_2atmpS2834);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS951,
  moonbit_string_t _M0L3keyS955,
  int32_t _M0L4hashS954
) {
  int32_t _M0L14capacity__maskS2820;
  int32_t _M0L6_2atmpS2819;
  int32_t _M0L1iS948;
  int32_t _M0L3idxS949;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2820 = _M0L4selfS951->$3;
  _M0L6_2atmpS2819 = _M0L4hashS954 & _M0L14capacity__maskS2820;
  _M0L1iS948 = 0;
  _M0L3idxS949 = _M0L6_2atmpS2819;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2818 = _M0L4selfS951->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS950;
    if (
      _M0L3idxS949 < 0
      || _M0L3idxS949 >= Moonbit_array_length(_M0L7entriesS2818)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS950
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2818[_M0L3idxS949];
    if (_M0L7_2abindS950 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS952 = _M0L7_2abindS950;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS953 = _M0L7_2aSomeS952;
      int32_t _M0L4hashS2810 = _M0L8_2aentryS953->$3;
      int32_t _if__result_4105;
      int32_t _M0L3pslS2813;
      int32_t _M0L6_2atmpS2814;
      int32_t _M0L6_2atmpS2816;
      int32_t _M0L14capacity__maskS2817;
      int32_t _M0L6_2atmpS2815;
      if (_M0L4hashS2810 == _M0L4hashS954) {
        moonbit_string_t _M0L3keyS2809 = _M0L8_2aentryS953->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4105
        = _M0L3keyS2809 == _M0L3keyS955
          || Moonbit_array_length(_M0L3keyS2809)
             == Moonbit_array_length(_M0L3keyS955)
             && 0
                == memcmp(_M0L3keyS2809, _M0L3keyS955, Moonbit_array_length(_M0L3keyS2809) * 2);
      } else {
        _if__result_4105 = 0;
      }
      if (_if__result_4105) {
        int32_t _M0L4sizeS2812;
        int32_t _M0L6_2atmpS2811;
        moonbit_incref(_M0L8_2aentryS953);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS951, _M0L8_2aentryS953);
        moonbit_decref(_M0L8_2aentryS953);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS951, _M0L3idxS949);
        _M0L4sizeS2812 = _M0L4selfS951->$1;
        _M0L6_2atmpS2811 = _M0L4sizeS2812 - 1;
        _M0L4selfS951->$1 = _M0L6_2atmpS2811;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS953);
      }
      _M0L3pslS2813 = _M0L8_2aentryS953->$2;
      moonbit_decref(_M0L8_2aentryS953);
      if (_M0L1iS948 > _M0L3pslS2813) {
        break;
      }
      _M0L6_2atmpS2814 = _M0L1iS948 + 1;
      _M0L6_2atmpS2816 = _M0L3idxS949 + 1;
      _M0L14capacity__maskS2817 = _M0L4selfS951->$3;
      _M0L6_2atmpS2815 = _M0L6_2atmpS2816 & _M0L14capacity__maskS2817;
      _M0L1iS948 = _M0L6_2atmpS2814;
      _M0L3idxS949 = _M0L6_2atmpS2815;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS960,
  moonbit_string_t _M0L3keyS964,
  int32_t _M0L4hashS963
) {
  int32_t _M0L14capacity__maskS2832;
  int32_t _M0L6_2atmpS2831;
  int32_t _M0L1iS957;
  int32_t _M0L3idxS958;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2832 = _M0L4selfS960->$3;
  _M0L6_2atmpS2831 = _M0L4hashS963 & _M0L14capacity__maskS2832;
  _M0L1iS957 = 0;
  _M0L3idxS958 = _M0L6_2atmpS2831;
  while (1) {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2830 =
      _M0L4selfS960->$0;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS959;
    if (
      _M0L3idxS958 < 0
      || _M0L3idxS958 >= Moonbit_array_length(_M0L7entriesS2830)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS959
    = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2830[
        _M0L3idxS958
      ];
    if (_M0L7_2abindS959 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS961 =
        _M0L7_2abindS959;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L8_2aentryS962 =
        _M0L7_2aSomeS961;
      int32_t _M0L4hashS2822 = _M0L8_2aentryS962->$3;
      int32_t _if__result_4107;
      int32_t _M0L3pslS2825;
      int32_t _M0L6_2atmpS2826;
      int32_t _M0L6_2atmpS2828;
      int32_t _M0L14capacity__maskS2829;
      int32_t _M0L6_2atmpS2827;
      if (_M0L4hashS2822 == _M0L4hashS963) {
        moonbit_string_t _M0L3keyS2821 = _M0L8_2aentryS962->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4107
        = _M0L3keyS2821 == _M0L3keyS964
          || Moonbit_array_length(_M0L3keyS2821)
             == Moonbit_array_length(_M0L3keyS964)
             && 0
                == memcmp(_M0L3keyS2821, _M0L3keyS964, Moonbit_array_length(_M0L3keyS2821) * 2);
      } else {
        _if__result_4107 = 0;
      }
      if (_if__result_4107) {
        int32_t _M0L4sizeS2824;
        int32_t _M0L6_2atmpS2823;
        moonbit_incref(_M0L8_2aentryS962);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS960, _M0L8_2aentryS962);
        moonbit_decref(_M0L8_2aentryS962);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS960, _M0L3idxS958);
        _M0L4sizeS2824 = _M0L4selfS960->$1;
        _M0L6_2atmpS2823 = _M0L4sizeS2824 - 1;
        _M0L4selfS960->$1 = _M0L6_2atmpS2823;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS962);
      }
      _M0L3pslS2825 = _M0L8_2aentryS962->$2;
      moonbit_decref(_M0L8_2aentryS962);
      if (_M0L1iS957 > _M0L3pslS2825) {
        break;
      }
      _M0L6_2atmpS2826 = _M0L1iS957 + 1;
      _M0L6_2atmpS2828 = _M0L3idxS958 + 1;
      _M0L14capacity__maskS2829 = _M0L4selfS960->$3;
      _M0L6_2atmpS2827 = _M0L6_2atmpS2828 & _M0L14capacity__maskS2829;
      _M0L1iS957 = _M0L6_2atmpS2826;
      _M0L3idxS958 = _M0L6_2atmpS2827;
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
    int32_t _M0L6_2atmpS2800 = _M0L3curS928 + 1;
    int32_t _M0L14capacity__maskS2801 = _M0L4selfS930->$3;
    int32_t _M0L4nextS929 = _M0L6_2atmpS2800 & _M0L14capacity__maskS2801;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2799 = _M0L4selfS930->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS933;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2795;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS2796;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS3653;
    int32_t _tmp_4110;
    if (
      _M0L4nextS929 < 0
      || _M0L4nextS929 >= Moonbit_array_length(_M0L7entriesS2799)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS933
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2799[_M0L4nextS929];
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
          int32_t _M0L3pslS2798 = _M0L4_2axS935->$2;
          int32_t _M0L6_2atmpS2797 = _M0L3pslS2798 - 1;
          _M0L4_2axS935->$2 = _M0L6_2atmpS2797;
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
    goto joinlet_4109;
    join_931:;
    _M0L7entriesS2795 = _M0L4selfS930->$0;
    _M0L6_2atmpS2796 = 0;
    if (
      _M0L3curS928 < 0
      || _M0L3curS928 >= Moonbit_array_length(_M0L7entriesS2795)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3653
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2795[_M0L3curS928];
    if (_M0L6_2aoldS3653) {
      moonbit_decref(_M0L6_2aoldS3653);
    }
    _M0L7entriesS2795[_M0L3curS928] = _M0L6_2atmpS2796;
    break;
    joinlet_4109:;
    _tmp_4110 = _M0L3curS928;
    _M0L3curS928 = _tmp_4110;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS940,
  int32_t _M0L3idxS947
) {
  int32_t _M0L3curS938;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS938 = _M0L3idxS947;
  _2afor_942:;
  while (1) {
    int32_t _M0L6_2atmpS2807 = _M0L3curS938 + 1;
    int32_t _M0L14capacity__maskS2808 = _M0L4selfS940->$3;
    int32_t _M0L4nextS939 = _M0L6_2atmpS2807 & _M0L14capacity__maskS2808;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2806 =
      _M0L4selfS940->$0;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS943;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2802;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2803;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3657;
    int32_t _tmp_4113;
    if (
      _M0L4nextS939 < 0
      || _M0L4nextS939 >= Moonbit_array_length(_M0L7entriesS2806)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS943
    = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2806[
        _M0L4nextS939
      ];
    if (_M0L7_2abindS943 == 0) {
      goto join_941;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS944 =
        _M0L7_2abindS943;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4_2axS945 =
        _M0L7_2aSomeS944;
      int32_t _M0L4_2axS946 = _M0L4_2axS945->$2;
      switch (_M0L4_2axS946) {
        case 0: {
          goto join_941;
          break;
        }
        default: {
          int32_t _M0L3pslS2805 = _M0L4_2axS945->$2;
          int32_t _M0L6_2atmpS2804 = _M0L3pslS2805 - 1;
          _M0L4_2axS945->$2 = _M0L6_2atmpS2804;
          moonbit_incref(_M0L4_2axS945);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS940, _M0L4_2axS945, _M0L3curS938);
          moonbit_decref(_M0L4_2axS945);
          _M0L3curS938 = _M0L4nextS939;
          goto _2afor_942;
          break;
        }
      }
    }
    goto joinlet_4112;
    join_941:;
    _M0L7entriesS2802 = _M0L4selfS940->$0;
    _M0L6_2atmpS2803 = 0;
    if (
      _M0L3curS938 < 0
      || _M0L3curS938 >= Moonbit_array_length(_M0L7entriesS2802)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3657
    = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2802[
        _M0L3curS938
      ];
    if (_M0L6_2aoldS3657) {
      moonbit_decref(_M0L6_2aoldS3657);
    }
    _M0L7entriesS2802[_M0L3curS938] = _M0L6_2atmpS2803;
    break;
    joinlet_4112:;
    _tmp_4113 = _M0L3curS938;
    _M0L3curS938 = _tmp_4113;
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
      struct _M0TPB5EntryGsiE* _M0L4nextS2781 = _M0L5entryS917->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3662 = _M0L4selfS918->$5;
      if (_M0L4nextS2781) {
        moonbit_incref(_M0L4nextS2781);
      }
      if (_M0L6_2aoldS3662) {
        moonbit_decref(_M0L6_2aoldS3662);
      }
      _M0L4selfS918->$5 = _M0L4nextS2781;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2785 = _M0L4selfS918->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2784;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2782;
      struct _M0TPB5EntryGsiE* _M0L4nextS2783;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3664;
      if (
        _M0L7_2abindS916 < 0
        || _M0L7_2abindS916 >= Moonbit_array_length(_M0L7entriesS2785)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2784
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2785[_M0L7_2abindS916];
      if (_M0L6_2atmpS2784) {
        moonbit_incref(_M0L6_2atmpS2784);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2782
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2784);
      if (_M0L6_2atmpS2784) {
        moonbit_decref(_M0L6_2atmpS2784);
      }
      _M0L4nextS2783 = _M0L5entryS917->$1;
      _M0L6_2aoldS3664 = _M0L6_2atmpS2782->$1;
      if (_M0L4nextS2783) {
        moonbit_incref(_M0L4nextS2783);
      }
      if (_M0L6_2aoldS3664) {
        moonbit_decref(_M0L6_2aoldS3664);
      }
      _M0L6_2atmpS2782->$1 = _M0L4nextS2783;
      moonbit_decref(_M0L6_2atmpS2782);
      break;
    }
  }
  _M0L7_2abindS919 = _M0L5entryS917->$1;
  if (_M0L7_2abindS919 == 0) {
    int32_t _M0L4prevS2786 = _M0L5entryS917->$0;
    _M0L4selfS918->$6 = _M0L4prevS2786;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS920 = _M0L7_2abindS919;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS921 = _M0L7_2aSomeS920;
    int32_t _M0L4prevS2787 = _M0L5entryS917->$0;
    _M0L7_2anextS921->$0 = _M0L4prevS2787;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS924,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L5entryS923
) {
  int32_t _M0L7_2abindS922;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS925;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS922 = _M0L5entryS923->$0;
  switch (_M0L7_2abindS922) {
    case -1: {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4nextS2788 =
        _M0L5entryS923->$1;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3669 =
        _M0L4selfS924->$5;
      if (_M0L4nextS2788) {
        moonbit_incref(_M0L4nextS2788);
      }
      if (_M0L6_2aoldS3669) {
        moonbit_decref(_M0L6_2aoldS3669);
      }
      _M0L4selfS924->$5 = _M0L4nextS2788;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2792 =
        _M0L4selfS924->$0;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2791;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2789;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4nextS2790;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3671;
      if (
        _M0L7_2abindS922 < 0
        || _M0L7_2abindS922 >= Moonbit_array_length(_M0L7entriesS2792)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2791
      = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2792[
          _M0L7_2abindS922
        ];
      if (_M0L6_2atmpS2791) {
        moonbit_incref(_M0L6_2atmpS2791);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2789
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2791);
      if (_M0L6_2atmpS2791) {
        moonbit_decref(_M0L6_2atmpS2791);
      }
      _M0L4nextS2790 = _M0L5entryS923->$1;
      _M0L6_2aoldS3671 = _M0L6_2atmpS2789->$1;
      if (_M0L4nextS2790) {
        moonbit_incref(_M0L4nextS2790);
      }
      if (_M0L6_2aoldS3671) {
        moonbit_decref(_M0L6_2aoldS3671);
      }
      _M0L6_2atmpS2789->$1 = _M0L4nextS2790;
      moonbit_decref(_M0L6_2atmpS2789);
      break;
    }
  }
  _M0L7_2abindS925 = _M0L5entryS923->$1;
  if (_M0L7_2abindS925 == 0) {
    int32_t _M0L4prevS2793 = _M0L5entryS923->$0;
    _M0L4selfS924->$6 = _M0L4prevS2793;
  } else {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS926 =
      _M0L7_2abindS925;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2anextS927 =
      _M0L7_2aSomeS926;
    int32_t _M0L4prevS2794 = _M0L5entryS923->$0;
    _M0L7_2anextS927->$0 = _M0L4prevS2794;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS885,
  moonbit_string_t _M0L3keyS881
) {
  int32_t _M0L4hashS880;
  int32_t _M0L14capacity__maskS2750;
  int32_t _M0L6_2atmpS2749;
  int32_t _M0L1iS882;
  int32_t _M0L3idxS883;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS880 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS881);
  _M0L14capacity__maskS2750 = _M0L4selfS885->$3;
  _M0L6_2atmpS2749 = _M0L4hashS880 & _M0L14capacity__maskS2750;
  _M0L1iS882 = 0;
  _M0L3idxS883 = _M0L6_2atmpS2749;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2748 = _M0L4selfS885->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS884;
    if (
      _M0L3idxS883 < 0
      || _M0L3idxS883 >= Moonbit_array_length(_M0L7entriesS2748)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS884
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2748[_M0L3idxS883];
    if (_M0L7_2abindS884 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS886 = _M0L7_2abindS884;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS887 = _M0L7_2aSomeS886;
      int32_t _M0L4hashS2742 = _M0L8_2aentryS887->$3;
      int32_t _if__result_4115;
      int32_t _M0L3pslS2743;
      int32_t _M0L6_2atmpS2744;
      int32_t _M0L6_2atmpS2746;
      int32_t _M0L14capacity__maskS2747;
      int32_t _M0L6_2atmpS2745;
      if (_M0L4hashS2742 == _M0L4hashS880) {
        moonbit_string_t _M0L3keyS2741 = _M0L8_2aentryS887->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4115
        = _M0L3keyS2741 == _M0L3keyS881
          || Moonbit_array_length(_M0L3keyS2741)
             == Moonbit_array_length(_M0L3keyS881)
             && 0
                == memcmp(_M0L3keyS2741, _M0L3keyS881, Moonbit_array_length(_M0L3keyS2741) * 2);
      } else {
        _if__result_4115 = 0;
      }
      if (_if__result_4115) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS887);
      }
      _M0L3pslS2743 = _M0L8_2aentryS887->$2;
      moonbit_decref(_M0L8_2aentryS887);
      if (_M0L1iS882 > _M0L3pslS2743) {
        return 0;
      }
      _M0L6_2atmpS2744 = _M0L1iS882 + 1;
      _M0L6_2atmpS2746 = _M0L3idxS883 + 1;
      _M0L14capacity__maskS2747 = _M0L4selfS885->$3;
      _M0L6_2atmpS2745 = _M0L6_2atmpS2746 & _M0L14capacity__maskS2747;
      _M0L1iS882 = _M0L6_2atmpS2744;
      _M0L3idxS883 = _M0L6_2atmpS2745;
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
  int32_t _M0L14capacity__maskS2760;
  int32_t _M0L6_2atmpS2759;
  int32_t _M0L1iS891;
  int32_t _M0L3idxS892;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS889 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS890);
  _M0L14capacity__maskS2760 = _M0L4selfS894->$3;
  _M0L6_2atmpS2759 = _M0L4hashS889 & _M0L14capacity__maskS2760;
  _M0L1iS891 = 0;
  _M0L3idxS892 = _M0L6_2atmpS2759;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2758 = _M0L4selfS894->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS893;
    if (
      _M0L3idxS892 < 0
      || _M0L3idxS892 >= Moonbit_array_length(_M0L7entriesS2758)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS893
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2758[_M0L3idxS892];
    if (_M0L7_2abindS893 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS895 = _M0L7_2abindS893;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS896 = _M0L7_2aSomeS895;
      int32_t _M0L4hashS2752 = _M0L8_2aentryS896->$3;
      int32_t _if__result_4117;
      int32_t _M0L3pslS2753;
      int32_t _M0L6_2atmpS2754;
      int32_t _M0L6_2atmpS2756;
      int32_t _M0L14capacity__maskS2757;
      int32_t _M0L6_2atmpS2755;
      if (_M0L4hashS2752 == _M0L4hashS889) {
        moonbit_string_t _M0L3keyS2751 = _M0L8_2aentryS896->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4117
        = _M0L3keyS2751 == _M0L3keyS890
          || Moonbit_array_length(_M0L3keyS2751)
             == Moonbit_array_length(_M0L3keyS890)
             && 0
                == memcmp(_M0L3keyS2751, _M0L3keyS890, Moonbit_array_length(_M0L3keyS2751) * 2);
      } else {
        _if__result_4117 = 0;
      }
      if (_if__result_4117) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS896);
      }
      _M0L3pslS2753 = _M0L8_2aentryS896->$2;
      moonbit_decref(_M0L8_2aentryS896);
      if (_M0L1iS891 > _M0L3pslS2753) {
        return 0;
      }
      _M0L6_2atmpS2754 = _M0L1iS891 + 1;
      _M0L6_2atmpS2756 = _M0L3idxS892 + 1;
      _M0L14capacity__maskS2757 = _M0L4selfS894->$3;
      _M0L6_2atmpS2755 = _M0L6_2atmpS2756 & _M0L14capacity__maskS2757;
      _M0L1iS891 = _M0L6_2atmpS2754;
      _M0L3idxS892 = _M0L6_2atmpS2755;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS903,
  moonbit_string_t _M0L3keyS899
) {
  int32_t _M0L4hashS898;
  int32_t _M0L14capacity__maskS2770;
  int32_t _M0L6_2atmpS2769;
  int32_t _M0L1iS900;
  int32_t _M0L3idxS901;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS898 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS899);
  _M0L14capacity__maskS2770 = _M0L4selfS903->$3;
  _M0L6_2atmpS2769 = _M0L4hashS898 & _M0L14capacity__maskS2770;
  _M0L1iS900 = 0;
  _M0L3idxS901 = _M0L6_2atmpS2769;
  while (1) {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2768 =
      _M0L4selfS903->$0;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS902;
    if (
      _M0L3idxS901 < 0
      || _M0L3idxS901 >= Moonbit_array_length(_M0L7entriesS2768)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS902
    = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2768[
        _M0L3idxS901
      ];
    if (_M0L7_2abindS902 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS904 =
        _M0L7_2abindS902;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L8_2aentryS905 =
        _M0L7_2aSomeS904;
      int32_t _M0L4hashS2762 = _M0L8_2aentryS905->$3;
      int32_t _if__result_4119;
      int32_t _M0L3pslS2763;
      int32_t _M0L6_2atmpS2764;
      int32_t _M0L6_2atmpS2766;
      int32_t _M0L14capacity__maskS2767;
      int32_t _M0L6_2atmpS2765;
      if (_M0L4hashS2762 == _M0L4hashS898) {
        moonbit_string_t _M0L3keyS2761 = _M0L8_2aentryS905->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4119
        = _M0L3keyS2761 == _M0L3keyS899
          || Moonbit_array_length(_M0L3keyS2761)
             == Moonbit_array_length(_M0L3keyS899)
             && 0
                == memcmp(_M0L3keyS2761, _M0L3keyS899, Moonbit_array_length(_M0L3keyS2761) * 2);
      } else {
        _if__result_4119 = 0;
      }
      if (_if__result_4119) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS905);
      }
      _M0L3pslS2763 = _M0L8_2aentryS905->$2;
      moonbit_decref(_M0L8_2aentryS905);
      if (_M0L1iS900 > _M0L3pslS2763) {
        return 0;
      }
      _M0L6_2atmpS2764 = _M0L1iS900 + 1;
      _M0L6_2atmpS2766 = _M0L3idxS901 + 1;
      _M0L14capacity__maskS2767 = _M0L4selfS903->$3;
      _M0L6_2atmpS2765 = _M0L6_2atmpS2766 & _M0L14capacity__maskS2767;
      _M0L1iS900 = _M0L6_2atmpS2764;
      _M0L3idxS901 = _M0L6_2atmpS2765;
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
  int32_t _M0L14capacity__maskS2780;
  int32_t _M0L6_2atmpS2779;
  int32_t _M0L1iS909;
  int32_t _M0L3idxS910;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS907 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS908);
  _M0L14capacity__maskS2780 = _M0L4selfS912->$3;
  _M0L6_2atmpS2779 = _M0L4hashS907 & _M0L14capacity__maskS2780;
  _M0L1iS909 = 0;
  _M0L3idxS910 = _M0L6_2atmpS2779;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2778 = _M0L4selfS912->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS911;
    if (
      _M0L3idxS910 < 0
      || _M0L3idxS910 >= Moonbit_array_length(_M0L7entriesS2778)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS911
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2778[_M0L3idxS910];
    if (_M0L7_2abindS911 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS913 = _M0L7_2abindS911;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS914 = _M0L7_2aSomeS913;
      int32_t _M0L4hashS2772 = _M0L8_2aentryS914->$3;
      int32_t _if__result_4121;
      int32_t _M0L3pslS2773;
      int32_t _M0L6_2atmpS2774;
      int32_t _M0L6_2atmpS2776;
      int32_t _M0L14capacity__maskS2777;
      int32_t _M0L6_2atmpS2775;
      if (_M0L4hashS2772 == _M0L4hashS907) {
        moonbit_string_t _M0L3keyS2771 = _M0L8_2aentryS914->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4121
        = _M0L3keyS2771 == _M0L3keyS908
          || Moonbit_array_length(_M0L3keyS2771)
             == Moonbit_array_length(_M0L3keyS908)
             && 0
                == memcmp(_M0L3keyS2771, _M0L3keyS908, Moonbit_array_length(_M0L3keyS2771) * 2);
      } else {
        _if__result_4121 = 0;
      }
      if (_if__result_4121) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS914);
      }
      _M0L3pslS2773 = _M0L8_2aentryS914->$2;
      moonbit_decref(_M0L8_2aentryS914);
      if (_M0L1iS909 > _M0L3pslS2773) {
        return 0;
      }
      _M0L6_2atmpS2774 = _M0L1iS909 + 1;
      _M0L6_2atmpS2776 = _M0L3idxS910 + 1;
      _M0L14capacity__maskS2777 = _M0L4selfS912->$3;
      _M0L6_2atmpS2775 = _M0L6_2atmpS2776 & _M0L14capacity__maskS2777;
      _M0L1iS909 = _M0L6_2atmpS2774;
      _M0L3idxS910 = _M0L6_2atmpS2775;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS849,
  moonbit_string_t _M0L3keyS845
) {
  int32_t _M0L4hashS844;
  int32_t _M0L14capacity__maskS2700;
  int32_t _M0L6_2atmpS2699;
  int32_t _M0L1iS846;
  int32_t _M0L3idxS847;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS844 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS845);
  _M0L14capacity__maskS2700 = _M0L4selfS849->$3;
  _M0L6_2atmpS2699 = _M0L4hashS844 & _M0L14capacity__maskS2700;
  _M0L1iS846 = 0;
  _M0L3idxS847 = _M0L6_2atmpS2699;
  while (1) {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2698 =
      _M0L4selfS849->$0;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS848;
    if (
      _M0L3idxS847 < 0
      || _M0L3idxS847 >= Moonbit_array_length(_M0L7entriesS2698)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS848
    = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2698[
        _M0L3idxS847
      ];
    if (_M0L7_2abindS848 == 0) {
      void* _M0L6_2atmpS2687 = 0;
      return _M0L6_2atmpS2687;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS850 =
        _M0L7_2abindS848;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L8_2aentryS851 =
        _M0L7_2aSomeS850;
      int32_t _M0L4hashS2689 = _M0L8_2aentryS851->$3;
      int32_t _if__result_4123;
      int32_t _M0L3pslS2692;
      int32_t _M0L6_2atmpS2694;
      int32_t _M0L6_2atmpS2696;
      int32_t _M0L14capacity__maskS2697;
      int32_t _M0L6_2atmpS2695;
      if (_M0L4hashS2689 == _M0L4hashS844) {
        moonbit_string_t _M0L3keyS2688 = _M0L8_2aentryS851->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4123
        = _M0L3keyS2688 == _M0L3keyS845
          || Moonbit_array_length(_M0L3keyS2688)
             == Moonbit_array_length(_M0L3keyS845)
             && 0
                == memcmp(_M0L3keyS2688, _M0L3keyS845, Moonbit_array_length(_M0L3keyS2688) * 2);
      } else {
        _if__result_4123 = 0;
      }
      if (_if__result_4123) {
        void* _M0L5valueS2691 = _M0L8_2aentryS851->$5;
        void* _M0L6_2atmpS2690;
        moonbit_incref(_M0L5valueS2691);
        _M0L6_2atmpS2690 = _M0L5valueS2691;
        return _M0L6_2atmpS2690;
      } else {
        moonbit_incref(_M0L8_2aentryS851);
      }
      _M0L3pslS2692 = _M0L8_2aentryS851->$2;
      moonbit_decref(_M0L8_2aentryS851);
      if (_M0L1iS846 > _M0L3pslS2692) {
        void* _M0L6_2atmpS2693 = 0;
        return _M0L6_2atmpS2693;
      }
      _M0L6_2atmpS2694 = _M0L1iS846 + 1;
      _M0L6_2atmpS2696 = _M0L3idxS847 + 1;
      _M0L14capacity__maskS2697 = _M0L4selfS849->$3;
      _M0L6_2atmpS2695 = _M0L6_2atmpS2696 & _M0L14capacity__maskS2697;
      _M0L1iS846 = _M0L6_2atmpS2694;
      _M0L3idxS847 = _M0L6_2atmpS2695;
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
  int32_t _M0L14capacity__maskS2714;
  int32_t _M0L6_2atmpS2713;
  int32_t _M0L1iS855;
  int32_t _M0L3idxS856;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS853 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS854);
  _M0L14capacity__maskS2714 = _M0L4selfS858->$3;
  _M0L6_2atmpS2713 = _M0L4hashS853 & _M0L14capacity__maskS2714;
  _M0L1iS855 = 0;
  _M0L3idxS856 = _M0L6_2atmpS2713;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2712 = _M0L4selfS858->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS857;
    if (
      _M0L3idxS856 < 0
      || _M0L3idxS856 >= Moonbit_array_length(_M0L7entriesS2712)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS857
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2712[_M0L3idxS856];
    if (_M0L7_2abindS857 == 0) {
      moonbit_string_t _M0L6_2atmpS2701 = 0;
      return _M0L6_2atmpS2701;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS859 = _M0L7_2abindS857;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS860 = _M0L7_2aSomeS859;
      int32_t _M0L4hashS2703 = _M0L8_2aentryS860->$3;
      int32_t _if__result_4125;
      int32_t _M0L3pslS2706;
      int32_t _M0L6_2atmpS2708;
      int32_t _M0L6_2atmpS2710;
      int32_t _M0L14capacity__maskS2711;
      int32_t _M0L6_2atmpS2709;
      if (_M0L4hashS2703 == _M0L4hashS853) {
        moonbit_string_t _M0L3keyS2702 = _M0L8_2aentryS860->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4125
        = _M0L3keyS2702 == _M0L3keyS854
          || Moonbit_array_length(_M0L3keyS2702)
             == Moonbit_array_length(_M0L3keyS854)
             && 0
                == memcmp(_M0L3keyS2702, _M0L3keyS854, Moonbit_array_length(_M0L3keyS2702) * 2);
      } else {
        _if__result_4125 = 0;
      }
      if (_if__result_4125) {
        moonbit_string_t _M0L5valueS2705 = _M0L8_2aentryS860->$5;
        moonbit_string_t _M0L6_2atmpS2704;
        moonbit_incref(_M0L5valueS2705);
        _M0L6_2atmpS2704 = _M0L5valueS2705;
        return _M0L6_2atmpS2704;
      } else {
        moonbit_incref(_M0L8_2aentryS860);
      }
      _M0L3pslS2706 = _M0L8_2aentryS860->$2;
      moonbit_decref(_M0L8_2aentryS860);
      if (_M0L1iS855 > _M0L3pslS2706) {
        moonbit_string_t _M0L6_2atmpS2707 = 0;
        return _M0L6_2atmpS2707;
      }
      _M0L6_2atmpS2708 = _M0L1iS855 + 1;
      _M0L6_2atmpS2710 = _M0L3idxS856 + 1;
      _M0L14capacity__maskS2711 = _M0L4selfS858->$3;
      _M0L6_2atmpS2709 = _M0L6_2atmpS2710 & _M0L14capacity__maskS2711;
      _M0L1iS855 = _M0L6_2atmpS2708;
      _M0L3idxS856 = _M0L6_2atmpS2709;
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
  int32_t _M0L14capacity__maskS2728;
  int32_t _M0L6_2atmpS2727;
  int32_t _M0L1iS864;
  int32_t _M0L3idxS865;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS862 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS863);
  _M0L14capacity__maskS2728 = _M0L4selfS867->$3;
  _M0L6_2atmpS2727 = _M0L4hashS862 & _M0L14capacity__maskS2728;
  _M0L1iS864 = 0;
  _M0L3idxS865 = _M0L6_2atmpS2727;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2726 = _M0L4selfS867->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS866;
    if (
      _M0L3idxS865 < 0
      || _M0L3idxS865 >= Moonbit_array_length(_M0L7entriesS2726)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS866
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2726[_M0L3idxS865];
    if (_M0L7_2abindS866 == 0) {
      void* _M0L4NoneS2715 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS2715;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS868 = _M0L7_2abindS866;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS869 = _M0L7_2aSomeS868;
      int32_t _M0L4hashS2717 = _M0L8_2aentryS869->$3;
      int32_t _if__result_4127;
      int32_t _M0L3pslS2720;
      int32_t _M0L6_2atmpS2722;
      int32_t _M0L6_2atmpS2724;
      int32_t _M0L14capacity__maskS2725;
      int32_t _M0L6_2atmpS2723;
      if (_M0L4hashS2717 == _M0L4hashS862) {
        moonbit_string_t _M0L3keyS2716 = _M0L8_2aentryS869->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4127
        = _M0L3keyS2716 == _M0L3keyS863
          || Moonbit_array_length(_M0L3keyS2716)
             == Moonbit_array_length(_M0L3keyS863)
             && 0
                == memcmp(_M0L3keyS2716, _M0L3keyS863, Moonbit_array_length(_M0L3keyS2716) * 2);
      } else {
        _if__result_4127 = 0;
      }
      if (_if__result_4127) {
        float _M0L5valueS2719 = _M0L8_2aentryS869->$5;
        void* _M0L4SomeS2718 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS2718)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS2718)->$0
        = _M0L5valueS2719;
        return _M0L4SomeS2718;
      } else {
        moonbit_incref(_M0L8_2aentryS869);
      }
      _M0L3pslS2720 = _M0L8_2aentryS869->$2;
      moonbit_decref(_M0L8_2aentryS869);
      if (_M0L1iS864 > _M0L3pslS2720) {
        void* _M0L4NoneS2721 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS2721;
      }
      _M0L6_2atmpS2722 = _M0L1iS864 + 1;
      _M0L6_2atmpS2724 = _M0L3idxS865 + 1;
      _M0L14capacity__maskS2725 = _M0L4selfS867->$3;
      _M0L6_2atmpS2723 = _M0L6_2atmpS2724 & _M0L14capacity__maskS2725;
      _M0L1iS864 = _M0L6_2atmpS2722;
      _M0L3idxS865 = _M0L6_2atmpS2723;
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
  int32_t _M0L14capacity__maskS2740;
  int32_t _M0L6_2atmpS2739;
  int32_t _M0L1iS873;
  int32_t _M0L3idxS874;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS871 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS872);
  _M0L14capacity__maskS2740 = _M0L4selfS876->$3;
  _M0L6_2atmpS2739 = _M0L4hashS871 & _M0L14capacity__maskS2740;
  _M0L1iS873 = 0;
  _M0L3idxS874 = _M0L6_2atmpS2739;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2738 = _M0L4selfS876->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS875;
    if (
      _M0L3idxS874 < 0
      || _M0L3idxS874 >= Moonbit_array_length(_M0L7entriesS2738)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS875
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2738[_M0L3idxS874];
    if (_M0L7_2abindS875 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS877 = _M0L7_2abindS875;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS878 = _M0L7_2aSomeS877;
      int32_t _M0L4hashS2730 = _M0L8_2aentryS878->$3;
      int32_t _if__result_4129;
      int32_t _M0L3pslS2733;
      int32_t _M0L6_2atmpS2734;
      int32_t _M0L6_2atmpS2736;
      int32_t _M0L14capacity__maskS2737;
      int32_t _M0L6_2atmpS2735;
      if (_M0L4hashS2730 == _M0L4hashS871) {
        moonbit_string_t _M0L3keyS2729 = _M0L8_2aentryS878->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4129
        = _M0L3keyS2729 == _M0L3keyS872
          || Moonbit_array_length(_M0L3keyS2729)
             == Moonbit_array_length(_M0L3keyS872)
             && 0
                == memcmp(_M0L3keyS2729, _M0L3keyS872, Moonbit_array_length(_M0L3keyS2729) * 2);
      } else {
        _if__result_4129 = 0;
      }
      if (_if__result_4129) {
        int32_t _M0L5valueS2732 = _M0L8_2aentryS878->$5;
        int64_t _M0L6_2atmpS2731 = (int64_t)_M0L5valueS2732;
        return _M0L6_2atmpS2731;
      } else {
        moonbit_incref(_M0L8_2aentryS878);
      }
      _M0L3pslS2733 = _M0L8_2aentryS878->$2;
      moonbit_decref(_M0L8_2aentryS878);
      if (_M0L1iS873 > _M0L3pslS2733) {
        return 4294967296ll;
      }
      _M0L6_2atmpS2734 = _M0L1iS873 + 1;
      _M0L6_2atmpS2736 = _M0L3idxS874 + 1;
      _M0L14capacity__maskS2737 = _M0L4selfS876->$3;
      _M0L6_2atmpS2735 = _M0L6_2atmpS2736 & _M0L14capacity__maskS2737;
      _M0L1iS873 = _M0L6_2atmpS2734;
      _M0L3idxS874 = _M0L6_2atmpS2735;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPB3Map3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE _M0L3arrS790,
  int64_t _M0L8capacityS792
) {
  int32_t _M0L3endS2641;
  int32_t _M0L5startS2642;
  int32_t _M0L6lengthS789;
  int32_t _M0L8capacityS791;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L1mS795;
  int32_t _M0L3endS2638;
  int32_t _M0L5startS2639;
  int32_t _M0L7_2abindS796;
  int32_t _M0L2__S797;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2641 = _M0L3arrS790.$2;
  _M0L5startS2642 = _M0L3arrS790.$1;
  _M0L6lengthS789 = _M0L3endS2641 - _M0L5startS2642;
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
    int32_t _M0L6_2atmpS2640;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2640 = _M0FPB21capacity__for__length(_M0L6lengthS789);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS791
    = _M0MPC13int3Int3max(_M0L11_2acapacityS794, _M0L6_2atmpS2640);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS795
  = _M0FPB8new__mapGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L8capacityS791);
  _M0L3endS2638 = _M0L3arrS790.$2;
  _M0L5startS2639 = _M0L3arrS790.$1;
  _M0L7_2abindS796 = _M0L3endS2638 - _M0L5startS2639;
  _M0L2__S797 = 0;
  while (1) {
    if (_M0L2__S797 < _M0L7_2abindS796) {
      struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L3bufS2635 =
        _M0L3arrS790.$0;
      int32_t _M0L5startS2637 = _M0L3arrS790.$1;
      int32_t _M0L6_2atmpS2636 = _M0L5startS2637 + _M0L2__S797;
      struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L1eS798 =
        (struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L3bufS2635[
          _M0L6_2atmpS2636
        ];
      moonbit_string_t _M0L6_2atmpS2632 = _M0L1eS798->$0;
      void* _M0L6_2atmpS2633 = _M0L1eS798->$1;
      int32_t _M0L6_2atmpS2634;
      moonbit_incref(_M0L6_2atmpS2633);
      moonbit_incref(_M0L6_2atmpS2632);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L1mS795, _M0L6_2atmpS2632, _M0L6_2atmpS2633);
      moonbit_decref(_M0L6_2atmpS2632);
      moonbit_decref(_M0L6_2atmpS2633);
      _M0L6_2atmpS2634 = _M0L2__S797 + 1;
      _M0L2__S797 = _M0L6_2atmpS2634;
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
  int32_t _M0L3endS2652;
  int32_t _M0L5startS2653;
  int32_t _M0L6lengthS800;
  int32_t _M0L8capacityS802;
  struct _M0TPB3MapGsiE* _M0L1mS806;
  int32_t _M0L3endS2649;
  int32_t _M0L5startS2650;
  int32_t _M0L7_2abindS807;
  int32_t _M0L2__S808;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2652 = _M0L3arrS801.$2;
  _M0L5startS2653 = _M0L3arrS801.$1;
  _M0L6lengthS800 = _M0L3endS2652 - _M0L5startS2653;
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
    int32_t _M0L6_2atmpS2651;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2651 = _M0FPB21capacity__for__length(_M0L6lengthS800);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS802
    = _M0MPC13int3Int3max(_M0L11_2acapacityS805, _M0L6_2atmpS2651);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS806 = _M0FPB8new__mapGsiE(_M0L8capacityS802);
  _M0L3endS2649 = _M0L3arrS801.$2;
  _M0L5startS2650 = _M0L3arrS801.$1;
  _M0L7_2abindS807 = _M0L3endS2649 - _M0L5startS2650;
  _M0L2__S808 = 0;
  while (1) {
    if (_M0L2__S808 < _M0L7_2abindS807) {
      struct _M0TUsiE** _M0L3bufS2646 = _M0L3arrS801.$0;
      int32_t _M0L5startS2648 = _M0L3arrS801.$1;
      int32_t _M0L6_2atmpS2647 = _M0L5startS2648 + _M0L2__S808;
      struct _M0TUsiE* _M0L1eS809 =
        (struct _M0TUsiE*)_M0L3bufS2646[_M0L6_2atmpS2647];
      moonbit_string_t _M0L6_2atmpS2643 = _M0L1eS809->$0;
      int32_t _M0L6_2atmpS2644 = _M0L1eS809->$1;
      int32_t _M0L6_2atmpS2645;
      moonbit_incref(_M0L6_2atmpS2643);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS806, _M0L6_2atmpS2643, _M0L6_2atmpS2644);
      moonbit_decref(_M0L6_2atmpS2643);
      _M0L6_2atmpS2645 = _M0L2__S808 + 1;
      _M0L2__S808 = _M0L6_2atmpS2645;
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
  int32_t _M0L3endS2663;
  int32_t _M0L5startS2664;
  int32_t _M0L6lengthS811;
  int32_t _M0L8capacityS813;
  struct _M0TPB3MapGssE* _M0L1mS817;
  int32_t _M0L3endS2660;
  int32_t _M0L5startS2661;
  int32_t _M0L7_2abindS818;
  int32_t _M0L2__S819;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2663 = _M0L3arrS812.$2;
  _M0L5startS2664 = _M0L3arrS812.$1;
  _M0L6lengthS811 = _M0L3endS2663 - _M0L5startS2664;
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
    int32_t _M0L6_2atmpS2662;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2662 = _M0FPB21capacity__for__length(_M0L6lengthS811);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS813
    = _M0MPC13int3Int3max(_M0L11_2acapacityS816, _M0L6_2atmpS2662);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS817 = _M0FPB8new__mapGssE(_M0L8capacityS813);
  _M0L3endS2660 = _M0L3arrS812.$2;
  _M0L5startS2661 = _M0L3arrS812.$1;
  _M0L7_2abindS818 = _M0L3endS2660 - _M0L5startS2661;
  _M0L2__S819 = 0;
  while (1) {
    if (_M0L2__S819 < _M0L7_2abindS818) {
      struct _M0TUssE** _M0L3bufS2657 = _M0L3arrS812.$0;
      int32_t _M0L5startS2659 = _M0L3arrS812.$1;
      int32_t _M0L6_2atmpS2658 = _M0L5startS2659 + _M0L2__S819;
      struct _M0TUssE* _M0L1eS820 =
        (struct _M0TUssE*)_M0L3bufS2657[_M0L6_2atmpS2658];
      moonbit_string_t _M0L6_2atmpS2654 = _M0L1eS820->$0;
      moonbit_string_t _M0L6_2atmpS2655 = _M0L1eS820->$1;
      int32_t _M0L6_2atmpS2656;
      moonbit_incref(_M0L6_2atmpS2655);
      moonbit_incref(_M0L6_2atmpS2654);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS817, _M0L6_2atmpS2654, _M0L6_2atmpS2655);
      moonbit_decref(_M0L6_2atmpS2654);
      moonbit_decref(_M0L6_2atmpS2655);
      _M0L6_2atmpS2656 = _M0L2__S819 + 1;
      _M0L2__S819 = _M0L6_2atmpS2656;
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
  int32_t _M0L3endS2674;
  int32_t _M0L5startS2675;
  int32_t _M0L6lengthS822;
  int32_t _M0L8capacityS824;
  struct _M0TPB3MapGsbE* _M0L1mS828;
  int32_t _M0L3endS2671;
  int32_t _M0L5startS2672;
  int32_t _M0L7_2abindS829;
  int32_t _M0L2__S830;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2674 = _M0L3arrS823.$2;
  _M0L5startS2675 = _M0L3arrS823.$1;
  _M0L6lengthS822 = _M0L3endS2674 - _M0L5startS2675;
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
    int32_t _M0L6_2atmpS2673;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2673 = _M0FPB21capacity__for__length(_M0L6lengthS822);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS824
    = _M0MPC13int3Int3max(_M0L11_2acapacityS827, _M0L6_2atmpS2673);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS828 = _M0FPB8new__mapGsbE(_M0L8capacityS824);
  _M0L3endS2671 = _M0L3arrS823.$2;
  _M0L5startS2672 = _M0L3arrS823.$1;
  _M0L7_2abindS829 = _M0L3endS2671 - _M0L5startS2672;
  _M0L2__S830 = 0;
  while (1) {
    if (_M0L2__S830 < _M0L7_2abindS829) {
      struct _M0TUsbE** _M0L3bufS2668 = _M0L3arrS823.$0;
      int32_t _M0L5startS2670 = _M0L3arrS823.$1;
      int32_t _M0L6_2atmpS2669 = _M0L5startS2670 + _M0L2__S830;
      struct _M0TUsbE* _M0L1eS831 =
        (struct _M0TUsbE*)_M0L3bufS2668[_M0L6_2atmpS2669];
      moonbit_string_t _M0L6_2atmpS2665 = _M0L1eS831->$0;
      int32_t _M0L6_2atmpS2666 = _M0L1eS831->$1;
      int32_t _M0L6_2atmpS2667;
      moonbit_incref(_M0L6_2atmpS2665);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS828, _M0L6_2atmpS2665, _M0L6_2atmpS2666);
      moonbit_decref(_M0L6_2atmpS2665);
      _M0L6_2atmpS2667 = _M0L2__S830 + 1;
      _M0L2__S830 = _M0L6_2atmpS2667;
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
  int32_t _M0L3endS2685;
  int32_t _M0L5startS2686;
  int32_t _M0L6lengthS833;
  int32_t _M0L8capacityS835;
  struct _M0TPB3MapGsfE* _M0L1mS839;
  int32_t _M0L3endS2682;
  int32_t _M0L5startS2683;
  int32_t _M0L7_2abindS840;
  int32_t _M0L2__S841;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2685 = _M0L3arrS834.$2;
  _M0L5startS2686 = _M0L3arrS834.$1;
  _M0L6lengthS833 = _M0L3endS2685 - _M0L5startS2686;
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
    int32_t _M0L6_2atmpS2684;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2684 = _M0FPB21capacity__for__length(_M0L6lengthS833);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS835
    = _M0MPC13int3Int3max(_M0L11_2acapacityS838, _M0L6_2atmpS2684);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS839 = _M0FPB8new__mapGsfE(_M0L8capacityS835);
  _M0L3endS2682 = _M0L3arrS834.$2;
  _M0L5startS2683 = _M0L3arrS834.$1;
  _M0L7_2abindS840 = _M0L3endS2682 - _M0L5startS2683;
  _M0L2__S841 = 0;
  while (1) {
    if (_M0L2__S841 < _M0L7_2abindS840) {
      struct _M0TUsfE** _M0L3bufS2679 = _M0L3arrS834.$0;
      int32_t _M0L5startS2681 = _M0L3arrS834.$1;
      int32_t _M0L6_2atmpS2680 = _M0L5startS2681 + _M0L2__S841;
      struct _M0TUsfE* _M0L1eS842 =
        (struct _M0TUsfE*)_M0L3bufS2679[_M0L6_2atmpS2680];
      moonbit_string_t _M0L6_2atmpS2676 = _M0L1eS842->$0;
      float _M0L6_2atmpS2677 = _M0L1eS842->$1;
      int32_t _M0L6_2atmpS2678;
      moonbit_incref(_M0L6_2atmpS2676);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS839, _M0L6_2atmpS2676, _M0L6_2atmpS2677);
      moonbit_decref(_M0L6_2atmpS2676);
      _M0L6_2atmpS2678 = _M0L2__S841 + 1;
      _M0L2__S841 = _M0L6_2atmpS2678;
      continue;
    }
    break;
  }
  return _M0L1mS839;
}

int32_t _M0MPB3Map3setGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS774,
  moonbit_string_t _M0L3keyS775,
  void* _M0L5valueS776
) {
  int32_t _M0L6_2atmpS2627;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2627 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS775);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS774, _M0L3keyS775, _M0L5valueS776, _M0L6_2atmpS2627);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS777,
  moonbit_string_t _M0L3keyS778,
  moonbit_string_t _M0L5valueS779
) {
  int32_t _M0L6_2atmpS2628;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2628 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS778);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS777, _M0L3keyS778, _M0L5valueS779, _M0L6_2atmpS2628);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS780,
  moonbit_string_t _M0L3keyS781,
  int32_t _M0L5valueS782
) {
  int32_t _M0L6_2atmpS2629;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2629 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS781);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS780, _M0L3keyS781, _M0L5valueS782, _M0L6_2atmpS2629);
  return 0;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS783,
  moonbit_string_t _M0L3keyS784,
  float _M0L5valueS785
) {
  int32_t _M0L6_2atmpS2630;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2630 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS784);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS783, _M0L3keyS784, _M0L5valueS785, _M0L6_2atmpS2630);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS786,
  moonbit_string_t _M0L3keyS787,
  int32_t _M0L5valueS788
) {
  int32_t _M0L6_2atmpS2631;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2631 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS787);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS786, _M0L3keyS787, _M0L5valueS788, _M0L6_2atmpS2631);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS697,
  moonbit_string_t _M0L3keyS703,
  void* _M0L5valueS704,
  int32_t _M0L4hashS699
) {
  int32_t _M0L14capacity__maskS2554;
  int32_t _M0L6_2atmpS2553;
  int32_t _M0L3pslS694;
  int32_t _M0L3idxS695;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2554 = _M0L4selfS697->$3;
  _M0L6_2atmpS2553 = _M0L4hashS699 & _M0L14capacity__maskS2554;
  _M0L3pslS694 = 0;
  _M0L3idxS695 = _M0L6_2atmpS2553;
  while (1) {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2552 =
      _M0L4selfS697->$0;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS696;
    if (
      _M0L3idxS695 < 0
      || _M0L3idxS695 >= Moonbit_array_length(_M0L7entriesS2552)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS696
    = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2552[
        _M0L3idxS695
      ];
    if (_M0L7_2abindS696 == 0) {
      int32_t _M0L4sizeS2537 = _M0L4selfS697->$1;
      int32_t _M0L8grow__atS2538 = _M0L4selfS697->$4;
      int32_t _M0L7_2abindS700;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS701;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L5entryS702;
      if (_M0L4sizeS2537 >= _M0L8grow__atS2538) {
        int32_t _M0L14capacity__maskS2540;
        int32_t _M0L6_2atmpS2539;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS697);
        _M0L14capacity__maskS2540 = _M0L4selfS697->$3;
        _M0L6_2atmpS2539 = _M0L4hashS699 & _M0L14capacity__maskS2540;
        _M0L3pslS694 = 0;
        _M0L3idxS695 = _M0L6_2atmpS2539;
        continue;
      }
      _M0L7_2abindS700 = _M0L4selfS697->$6;
      _M0L7_2abindS701 = 0;
      moonbit_incref(_M0L3keyS703);
      moonbit_incref(_M0L5valueS704);
      _M0L5entryS702
      = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE));
      Moonbit_object_header(_M0L5entryS702)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
      _M0L5entryS702->$0 = _M0L7_2abindS700;
      _M0L5entryS702->$1 = _M0L7_2abindS701;
      _M0L5entryS702->$2 = _M0L3pslS694;
      _M0L5entryS702->$3 = _M0L4hashS699;
      _M0L5entryS702->$4 = _M0L3keyS703;
      _M0L5entryS702->$5 = _M0L5valueS704;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L5entryS702);
      moonbit_decref(_M0L5entryS702);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS705 =
        _M0L7_2abindS696;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS706 =
        _M0L7_2aSomeS705;
      int32_t _M0L4hashS2542 = _M0L14_2acurr__entryS706->$3;
      int32_t _if__result_4136;
      int32_t _M0L3pslS2543;
      int32_t _M0L6_2atmpS2548;
      int32_t _M0L6_2atmpS2550;
      int32_t _M0L14capacity__maskS2551;
      int32_t _M0L6_2atmpS2549;
      if (_M0L4hashS2542 == _M0L4hashS699) {
        moonbit_string_t _M0L3keyS2541 = _M0L14_2acurr__entryS706->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4136
        = _M0L3keyS2541 == _M0L3keyS703
          || Moonbit_array_length(_M0L3keyS2541)
             == Moonbit_array_length(_M0L3keyS703)
             && 0
                == memcmp(_M0L3keyS2541, _M0L3keyS703, Moonbit_array_length(_M0L3keyS2541) * 2);
      } else {
        _if__result_4136 = 0;
      }
      if (_if__result_4136) {
        void* _M0L6_2aoldS3718 = _M0L14_2acurr__entryS706->$5;
        moonbit_incref(_M0L5valueS704);
        moonbit_decref(_M0L6_2aoldS3718);
        _M0L14_2acurr__entryS706->$5 = _M0L5valueS704;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS706);
      }
      _M0L3pslS2543 = _M0L14_2acurr__entryS706->$2;
      if (_M0L3pslS694 > _M0L3pslS2543) {
        int32_t _M0L4sizeS2544 = _M0L4selfS697->$1;
        int32_t _M0L8grow__atS2545 = _M0L4selfS697->$4;
        int32_t _M0L7_2abindS707;
        struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS708;
        struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L5entryS709;
        if (_M0L4sizeS2544 >= _M0L8grow__atS2545) {
          int32_t _M0L14capacity__maskS2547;
          int32_t _M0L6_2atmpS2546;
          moonbit_decref(_M0L14_2acurr__entryS706);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS697);
          _M0L14capacity__maskS2547 = _M0L4selfS697->$3;
          _M0L6_2atmpS2546 = _M0L4hashS699 & _M0L14capacity__maskS2547;
          _M0L3pslS694 = 0;
          _M0L3idxS695 = _M0L6_2atmpS2546;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L14_2acurr__entryS706);
        moonbit_decref(_M0L14_2acurr__entryS706);
        _M0L7_2abindS707 = _M0L4selfS697->$6;
        _M0L7_2abindS708 = 0;
        moonbit_incref(_M0L3keyS703);
        moonbit_incref(_M0L5valueS704);
        _M0L5entryS709
        = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE));
        Moonbit_object_header(_M0L5entryS709)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
        _M0L5entryS709->$0 = _M0L7_2abindS707;
        _M0L5entryS709->$1 = _M0L7_2abindS708;
        _M0L5entryS709->$2 = _M0L3pslS694;
        _M0L5entryS709->$3 = _M0L4hashS699;
        _M0L5entryS709->$4 = _M0L3keyS703;
        _M0L5entryS709->$5 = _M0L5valueS704;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L5entryS709);
        moonbit_decref(_M0L5entryS709);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS706);
      }
      _M0L6_2atmpS2548 = _M0L3pslS694 + 1;
      _M0L6_2atmpS2550 = _M0L3idxS695 + 1;
      _M0L14capacity__maskS2551 = _M0L4selfS697->$3;
      _M0L6_2atmpS2549 = _M0L6_2atmpS2550 & _M0L14capacity__maskS2551;
      _M0L3pslS694 = _M0L6_2atmpS2548;
      _M0L3idxS695 = _M0L6_2atmpS2549;
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
  int32_t _M0L14capacity__maskS2572;
  int32_t _M0L6_2atmpS2571;
  int32_t _M0L3pslS710;
  int32_t _M0L3idxS711;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2572 = _M0L4selfS713->$3;
  _M0L6_2atmpS2571 = _M0L4hashS715 & _M0L14capacity__maskS2572;
  _M0L3pslS710 = 0;
  _M0L3idxS711 = _M0L6_2atmpS2571;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2570 = _M0L4selfS713->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS712;
    if (
      _M0L3idxS711 < 0
      || _M0L3idxS711 >= Moonbit_array_length(_M0L7entriesS2570)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS712
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2570[_M0L3idxS711];
    if (_M0L7_2abindS712 == 0) {
      int32_t _M0L4sizeS2555 = _M0L4selfS713->$1;
      int32_t _M0L8grow__atS2556 = _M0L4selfS713->$4;
      int32_t _M0L7_2abindS716;
      struct _M0TPB5EntryGssE* _M0L7_2abindS717;
      struct _M0TPB5EntryGssE* _M0L5entryS718;
      if (_M0L4sizeS2555 >= _M0L8grow__atS2556) {
        int32_t _M0L14capacity__maskS2558;
        int32_t _M0L6_2atmpS2557;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS713);
        _M0L14capacity__maskS2558 = _M0L4selfS713->$3;
        _M0L6_2atmpS2557 = _M0L4hashS715 & _M0L14capacity__maskS2558;
        _M0L3pslS710 = 0;
        _M0L3idxS711 = _M0L6_2atmpS2557;
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
      int32_t _M0L4hashS2560 = _M0L14_2acurr__entryS722->$3;
      int32_t _if__result_4138;
      int32_t _M0L3pslS2561;
      int32_t _M0L6_2atmpS2566;
      int32_t _M0L6_2atmpS2568;
      int32_t _M0L14capacity__maskS2569;
      int32_t _M0L6_2atmpS2567;
      if (_M0L4hashS2560 == _M0L4hashS715) {
        moonbit_string_t _M0L3keyS2559 = _M0L14_2acurr__entryS722->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4138
        = _M0L3keyS2559 == _M0L3keyS719
          || Moonbit_array_length(_M0L3keyS2559)
             == Moonbit_array_length(_M0L3keyS719)
             && 0
                == memcmp(_M0L3keyS2559, _M0L3keyS719, Moonbit_array_length(_M0L3keyS2559) * 2);
      } else {
        _if__result_4138 = 0;
      }
      if (_if__result_4138) {
        moonbit_string_t _M0L6_2aoldS3722 = _M0L14_2acurr__entryS722->$5;
        moonbit_incref(_M0L5valueS720);
        moonbit_decref(_M0L6_2aoldS3722);
        _M0L14_2acurr__entryS722->$5 = _M0L5valueS720;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS722);
      }
      _M0L3pslS2561 = _M0L14_2acurr__entryS722->$2;
      if (_M0L3pslS710 > _M0L3pslS2561) {
        int32_t _M0L4sizeS2562 = _M0L4selfS713->$1;
        int32_t _M0L8grow__atS2563 = _M0L4selfS713->$4;
        int32_t _M0L7_2abindS723;
        struct _M0TPB5EntryGssE* _M0L7_2abindS724;
        struct _M0TPB5EntryGssE* _M0L5entryS725;
        if (_M0L4sizeS2562 >= _M0L8grow__atS2563) {
          int32_t _M0L14capacity__maskS2565;
          int32_t _M0L6_2atmpS2564;
          moonbit_decref(_M0L14_2acurr__entryS722);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS713);
          _M0L14capacity__maskS2565 = _M0L4selfS713->$3;
          _M0L6_2atmpS2564 = _M0L4hashS715 & _M0L14capacity__maskS2565;
          _M0L3pslS710 = 0;
          _M0L3idxS711 = _M0L6_2atmpS2564;
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
      _M0L6_2atmpS2566 = _M0L3pslS710 + 1;
      _M0L6_2atmpS2568 = _M0L3idxS711 + 1;
      _M0L14capacity__maskS2569 = _M0L4selfS713->$3;
      _M0L6_2atmpS2567 = _M0L6_2atmpS2568 & _M0L14capacity__maskS2569;
      _M0L3pslS710 = _M0L6_2atmpS2566;
      _M0L3idxS711 = _M0L6_2atmpS2567;
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
  int32_t _M0L14capacity__maskS2590;
  int32_t _M0L6_2atmpS2589;
  int32_t _M0L3pslS726;
  int32_t _M0L3idxS727;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2590 = _M0L4selfS729->$3;
  _M0L6_2atmpS2589 = _M0L4hashS731 & _M0L14capacity__maskS2590;
  _M0L3pslS726 = 0;
  _M0L3idxS727 = _M0L6_2atmpS2589;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2588 = _M0L4selfS729->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS728;
    if (
      _M0L3idxS727 < 0
      || _M0L3idxS727 >= Moonbit_array_length(_M0L7entriesS2588)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS728
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2588[_M0L3idxS727];
    if (_M0L7_2abindS728 == 0) {
      int32_t _M0L4sizeS2573 = _M0L4selfS729->$1;
      int32_t _M0L8grow__atS2574 = _M0L4selfS729->$4;
      int32_t _M0L7_2abindS732;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS733;
      struct _M0TPB5EntryGsbE* _M0L5entryS734;
      if (_M0L4sizeS2573 >= _M0L8grow__atS2574) {
        int32_t _M0L14capacity__maskS2576;
        int32_t _M0L6_2atmpS2575;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS729);
        _M0L14capacity__maskS2576 = _M0L4selfS729->$3;
        _M0L6_2atmpS2575 = _M0L4hashS731 & _M0L14capacity__maskS2576;
        _M0L3pslS726 = 0;
        _M0L3idxS727 = _M0L6_2atmpS2575;
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
      int32_t _M0L4hashS2578 = _M0L14_2acurr__entryS738->$3;
      int32_t _if__result_4140;
      int32_t _M0L3pslS2579;
      int32_t _M0L6_2atmpS2584;
      int32_t _M0L6_2atmpS2586;
      int32_t _M0L14capacity__maskS2587;
      int32_t _M0L6_2atmpS2585;
      if (_M0L4hashS2578 == _M0L4hashS731) {
        moonbit_string_t _M0L3keyS2577 = _M0L14_2acurr__entryS738->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4140
        = _M0L3keyS2577 == _M0L3keyS735
          || Moonbit_array_length(_M0L3keyS2577)
             == Moonbit_array_length(_M0L3keyS735)
             && 0
                == memcmp(_M0L3keyS2577, _M0L3keyS735, Moonbit_array_length(_M0L3keyS2577) * 2);
      } else {
        _if__result_4140 = 0;
      }
      if (_if__result_4140) {
        _M0L14_2acurr__entryS738->$5 = _M0L5valueS736;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS738);
      }
      _M0L3pslS2579 = _M0L14_2acurr__entryS738->$2;
      if (_M0L3pslS726 > _M0L3pslS2579) {
        int32_t _M0L4sizeS2580 = _M0L4selfS729->$1;
        int32_t _M0L8grow__atS2581 = _M0L4selfS729->$4;
        int32_t _M0L7_2abindS739;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS740;
        struct _M0TPB5EntryGsbE* _M0L5entryS741;
        if (_M0L4sizeS2580 >= _M0L8grow__atS2581) {
          int32_t _M0L14capacity__maskS2583;
          int32_t _M0L6_2atmpS2582;
          moonbit_decref(_M0L14_2acurr__entryS738);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS729);
          _M0L14capacity__maskS2583 = _M0L4selfS729->$3;
          _M0L6_2atmpS2582 = _M0L4hashS731 & _M0L14capacity__maskS2583;
          _M0L3pslS726 = 0;
          _M0L3idxS727 = _M0L6_2atmpS2582;
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
      _M0L6_2atmpS2584 = _M0L3pslS726 + 1;
      _M0L6_2atmpS2586 = _M0L3idxS727 + 1;
      _M0L14capacity__maskS2587 = _M0L4selfS729->$3;
      _M0L6_2atmpS2585 = _M0L6_2atmpS2586 & _M0L14capacity__maskS2587;
      _M0L3pslS726 = _M0L6_2atmpS2584;
      _M0L3idxS727 = _M0L6_2atmpS2585;
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
  int32_t _M0L14capacity__maskS2608;
  int32_t _M0L6_2atmpS2607;
  int32_t _M0L3pslS742;
  int32_t _M0L3idxS743;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2608 = _M0L4selfS745->$3;
  _M0L6_2atmpS2607 = _M0L4hashS747 & _M0L14capacity__maskS2608;
  _M0L3pslS742 = 0;
  _M0L3idxS743 = _M0L6_2atmpS2607;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2606 = _M0L4selfS745->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS744;
    if (
      _M0L3idxS743 < 0
      || _M0L3idxS743 >= Moonbit_array_length(_M0L7entriesS2606)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS744
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2606[_M0L3idxS743];
    if (_M0L7_2abindS744 == 0) {
      int32_t _M0L4sizeS2591 = _M0L4selfS745->$1;
      int32_t _M0L8grow__atS2592 = _M0L4selfS745->$4;
      int32_t _M0L7_2abindS748;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS749;
      struct _M0TPB5EntryGsfE* _M0L5entryS750;
      if (_M0L4sizeS2591 >= _M0L8grow__atS2592) {
        int32_t _M0L14capacity__maskS2594;
        int32_t _M0L6_2atmpS2593;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS745);
        _M0L14capacity__maskS2594 = _M0L4selfS745->$3;
        _M0L6_2atmpS2593 = _M0L4hashS747 & _M0L14capacity__maskS2594;
        _M0L3pslS742 = 0;
        _M0L3idxS743 = _M0L6_2atmpS2593;
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
      int32_t _M0L4hashS2596 = _M0L14_2acurr__entryS754->$3;
      int32_t _if__result_4142;
      int32_t _M0L3pslS2597;
      int32_t _M0L6_2atmpS2602;
      int32_t _M0L6_2atmpS2604;
      int32_t _M0L14capacity__maskS2605;
      int32_t _M0L6_2atmpS2603;
      if (_M0L4hashS2596 == _M0L4hashS747) {
        moonbit_string_t _M0L3keyS2595 = _M0L14_2acurr__entryS754->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4142
        = _M0L3keyS2595 == _M0L3keyS751
          || Moonbit_array_length(_M0L3keyS2595)
             == Moonbit_array_length(_M0L3keyS751)
             && 0
                == memcmp(_M0L3keyS2595, _M0L3keyS751, Moonbit_array_length(_M0L3keyS2595) * 2);
      } else {
        _if__result_4142 = 0;
      }
      if (_if__result_4142) {
        _M0L14_2acurr__entryS754->$5 = _M0L5valueS752;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS754);
      }
      _M0L3pslS2597 = _M0L14_2acurr__entryS754->$2;
      if (_M0L3pslS742 > _M0L3pslS2597) {
        int32_t _M0L4sizeS2598 = _M0L4selfS745->$1;
        int32_t _M0L8grow__atS2599 = _M0L4selfS745->$4;
        int32_t _M0L7_2abindS755;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS756;
        struct _M0TPB5EntryGsfE* _M0L5entryS757;
        if (_M0L4sizeS2598 >= _M0L8grow__atS2599) {
          int32_t _M0L14capacity__maskS2601;
          int32_t _M0L6_2atmpS2600;
          moonbit_decref(_M0L14_2acurr__entryS754);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS745);
          _M0L14capacity__maskS2601 = _M0L4selfS745->$3;
          _M0L6_2atmpS2600 = _M0L4hashS747 & _M0L14capacity__maskS2601;
          _M0L3pslS742 = 0;
          _M0L3idxS743 = _M0L6_2atmpS2600;
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
      _M0L6_2atmpS2602 = _M0L3pslS742 + 1;
      _M0L6_2atmpS2604 = _M0L3idxS743 + 1;
      _M0L14capacity__maskS2605 = _M0L4selfS745->$3;
      _M0L6_2atmpS2603 = _M0L6_2atmpS2604 & _M0L14capacity__maskS2605;
      _M0L3pslS742 = _M0L6_2atmpS2602;
      _M0L3idxS743 = _M0L6_2atmpS2603;
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
  int32_t _M0L14capacity__maskS2626;
  int32_t _M0L6_2atmpS2625;
  int32_t _M0L3pslS758;
  int32_t _M0L3idxS759;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2626 = _M0L4selfS761->$3;
  _M0L6_2atmpS2625 = _M0L4hashS763 & _M0L14capacity__maskS2626;
  _M0L3pslS758 = 0;
  _M0L3idxS759 = _M0L6_2atmpS2625;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2624 = _M0L4selfS761->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS760;
    if (
      _M0L3idxS759 < 0
      || _M0L3idxS759 >= Moonbit_array_length(_M0L7entriesS2624)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS760
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2624[_M0L3idxS759];
    if (_M0L7_2abindS760 == 0) {
      int32_t _M0L4sizeS2609 = _M0L4selfS761->$1;
      int32_t _M0L8grow__atS2610 = _M0L4selfS761->$4;
      int32_t _M0L7_2abindS764;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS765;
      struct _M0TPB5EntryGsiE* _M0L5entryS766;
      if (_M0L4sizeS2609 >= _M0L8grow__atS2610) {
        int32_t _M0L14capacity__maskS2612;
        int32_t _M0L6_2atmpS2611;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS761);
        _M0L14capacity__maskS2612 = _M0L4selfS761->$3;
        _M0L6_2atmpS2611 = _M0L4hashS763 & _M0L14capacity__maskS2612;
        _M0L3pslS758 = 0;
        _M0L3idxS759 = _M0L6_2atmpS2611;
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
      int32_t _M0L4hashS2614 = _M0L14_2acurr__entryS770->$3;
      int32_t _if__result_4144;
      int32_t _M0L3pslS2615;
      int32_t _M0L6_2atmpS2620;
      int32_t _M0L6_2atmpS2622;
      int32_t _M0L14capacity__maskS2623;
      int32_t _M0L6_2atmpS2621;
      if (_M0L4hashS2614 == _M0L4hashS763) {
        moonbit_string_t _M0L3keyS2613 = _M0L14_2acurr__entryS770->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4144
        = _M0L3keyS2613 == _M0L3keyS767
          || Moonbit_array_length(_M0L3keyS2613)
             == Moonbit_array_length(_M0L3keyS767)
             && 0
                == memcmp(_M0L3keyS2613, _M0L3keyS767, Moonbit_array_length(_M0L3keyS2613) * 2);
      } else {
        _if__result_4144 = 0;
      }
      if (_if__result_4144) {
        _M0L14_2acurr__entryS770->$5 = _M0L5valueS768;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS770);
      }
      _M0L3pslS2615 = _M0L14_2acurr__entryS770->$2;
      if (_M0L3pslS758 > _M0L3pslS2615) {
        int32_t _M0L4sizeS2616 = _M0L4selfS761->$1;
        int32_t _M0L8grow__atS2617 = _M0L4selfS761->$4;
        int32_t _M0L7_2abindS771;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS772;
        struct _M0TPB5EntryGsiE* _M0L5entryS773;
        if (_M0L4sizeS2616 >= _M0L8grow__atS2617) {
          int32_t _M0L14capacity__maskS2619;
          int32_t _M0L6_2atmpS2618;
          moonbit_decref(_M0L14_2acurr__entryS770);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS761);
          _M0L14capacity__maskS2619 = _M0L4selfS761->$3;
          _M0L6_2atmpS2618 = _M0L4hashS763 & _M0L14capacity__maskS2619;
          _M0L3pslS758 = 0;
          _M0L3idxS759 = _M0L6_2atmpS2618;
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
      _M0L6_2atmpS2620 = _M0L3pslS758 + 1;
      _M0L6_2atmpS2622 = _M0L3idxS759 + 1;
      _M0L14capacity__maskS2623 = _M0L4selfS761->$3;
      _M0L6_2atmpS2621 = _M0L6_2atmpS2622 & _M0L14capacity__maskS2623;
      _M0L3pslS758 = _M0L6_2atmpS2620;
      _M0L3idxS759 = _M0L6_2atmpS2621;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS655
) {
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L9old__headS654;
  int32_t _M0L8capacityS2504;
  int32_t _M0L13new__capacityS656;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2498;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L6_2atmpS2497;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L6_2aoldS3738;
  int32_t _M0L6_2atmpS2499;
  int32_t _M0L8capacityS2501;
  int32_t _M0L6_2atmpS2500;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2502;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3737;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L1xS657;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS654 = _M0L4selfS655->$5;
  _M0L8capacityS2504 = _M0L4selfS655->$2;
  _M0L13new__capacityS656 = _M0L8capacityS2504 << 1;
  _M0L6_2atmpS2498 = 0;
  _M0L6_2atmpS2497
  = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS656, _M0L6_2atmpS2498);
  _M0L6_2aoldS3738 = _M0L4selfS655->$0;
  if (_M0L9old__headS654) {
    moonbit_incref(_M0L9old__headS654);
  }
  moonbit_decref(_M0L6_2aoldS3738);
  _M0L4selfS655->$0 = _M0L6_2atmpS2497;
  _M0L4selfS655->$2 = _M0L13new__capacityS656;
  _M0L6_2atmpS2499 = _M0L13new__capacityS656 - 1;
  _M0L4selfS655->$3 = _M0L6_2atmpS2499;
  _M0L8capacityS2501 = _M0L4selfS655->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2500 = _M0FPB21calc__grow__threshold(_M0L8capacityS2501);
  _M0L4selfS655->$4 = _M0L6_2atmpS2500;
  _M0L4selfS655->$1 = 0;
  _M0L6_2atmpS2502 = 0;
  _M0L6_2aoldS3737 = _M0L4selfS655->$5;
  if (_M0L6_2aoldS3737) {
    moonbit_decref(_M0L6_2aoldS3737);
  }
  _M0L4selfS655->$5 = _M0L6_2atmpS2502;
  _M0L4selfS655->$6 = -1;
  _M0L1xS657 = _M0L9old__headS654;
  while (1) {
    if (_M0L1xS657 == 0) {
      if (_M0L1xS657) {
        moonbit_decref(_M0L1xS657);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS659 =
        _M0L1xS657;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4_2aeS660 =
        _M0L7_2aSomeS659;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L15next__in__chainS661 =
        _M0L4_2aeS660->$1;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2503 =
        0;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3735 =
        _M0L4_2aeS660->$1;
      if (_M0L15next__in__chainS661) {
        moonbit_incref(_M0L15next__in__chainS661);
      }
      if (_M0L6_2aoldS3735) {
        moonbit_decref(_M0L6_2aoldS3735);
      }
      _M0L4_2aeS660->$1 = _M0L6_2atmpS2503;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS655, _M0L4_2aeS660);
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
  int32_t _M0L8capacityS2512;
  int32_t _M0L13new__capacityS664;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2506;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS2505;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS3743;
  int32_t _M0L6_2atmpS2507;
  int32_t _M0L8capacityS2509;
  int32_t _M0L6_2atmpS2508;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2510;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3742;
  struct _M0TPB5EntryGssE* _M0L1xS665;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS662 = _M0L4selfS663->$5;
  _M0L8capacityS2512 = _M0L4selfS663->$2;
  _M0L13new__capacityS664 = _M0L8capacityS2512 << 1;
  _M0L6_2atmpS2506 = 0;
  _M0L6_2atmpS2505
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS664, _M0L6_2atmpS2506);
  _M0L6_2aoldS3743 = _M0L4selfS663->$0;
  if (_M0L9old__headS662) {
    moonbit_incref(_M0L9old__headS662);
  }
  moonbit_decref(_M0L6_2aoldS3743);
  _M0L4selfS663->$0 = _M0L6_2atmpS2505;
  _M0L4selfS663->$2 = _M0L13new__capacityS664;
  _M0L6_2atmpS2507 = _M0L13new__capacityS664 - 1;
  _M0L4selfS663->$3 = _M0L6_2atmpS2507;
  _M0L8capacityS2509 = _M0L4selfS663->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2508 = _M0FPB21calc__grow__threshold(_M0L8capacityS2509);
  _M0L4selfS663->$4 = _M0L6_2atmpS2508;
  _M0L4selfS663->$1 = 0;
  _M0L6_2atmpS2510 = 0;
  _M0L6_2aoldS3742 = _M0L4selfS663->$5;
  if (_M0L6_2aoldS3742) {
    moonbit_decref(_M0L6_2aoldS3742);
  }
  _M0L4selfS663->$5 = _M0L6_2atmpS2510;
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
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2511 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3740 = _M0L4_2aeS668->$1;
      if (_M0L15next__in__chainS669) {
        moonbit_incref(_M0L15next__in__chainS669);
      }
      if (_M0L6_2aoldS3740) {
        moonbit_decref(_M0L6_2aoldS3740);
      }
      _M0L4_2aeS668->$1 = _M0L6_2atmpS2511;
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
  int32_t _M0L8capacityS2520;
  int32_t _M0L13new__capacityS672;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2514;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS2513;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS3748;
  int32_t _M0L6_2atmpS2515;
  int32_t _M0L8capacityS2517;
  int32_t _M0L6_2atmpS2516;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2518;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3747;
  struct _M0TPB5EntryGsbE* _M0L1xS673;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS670 = _M0L4selfS671->$5;
  _M0L8capacityS2520 = _M0L4selfS671->$2;
  _M0L13new__capacityS672 = _M0L8capacityS2520 << 1;
  _M0L6_2atmpS2514 = 0;
  _M0L6_2atmpS2513
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS672, _M0L6_2atmpS2514);
  _M0L6_2aoldS3748 = _M0L4selfS671->$0;
  if (_M0L9old__headS670) {
    moonbit_incref(_M0L9old__headS670);
  }
  moonbit_decref(_M0L6_2aoldS3748);
  _M0L4selfS671->$0 = _M0L6_2atmpS2513;
  _M0L4selfS671->$2 = _M0L13new__capacityS672;
  _M0L6_2atmpS2515 = _M0L13new__capacityS672 - 1;
  _M0L4selfS671->$3 = _M0L6_2atmpS2515;
  _M0L8capacityS2517 = _M0L4selfS671->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2516 = _M0FPB21calc__grow__threshold(_M0L8capacityS2517);
  _M0L4selfS671->$4 = _M0L6_2atmpS2516;
  _M0L4selfS671->$1 = 0;
  _M0L6_2atmpS2518 = 0;
  _M0L6_2aoldS3747 = _M0L4selfS671->$5;
  if (_M0L6_2aoldS3747) {
    moonbit_decref(_M0L6_2aoldS3747);
  }
  _M0L4selfS671->$5 = _M0L6_2atmpS2518;
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
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2519 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3745 = _M0L4_2aeS676->$1;
      if (_M0L15next__in__chainS677) {
        moonbit_incref(_M0L15next__in__chainS677);
      }
      if (_M0L6_2aoldS3745) {
        moonbit_decref(_M0L6_2aoldS3745);
      }
      _M0L4_2aeS676->$1 = _M0L6_2atmpS2519;
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
  int32_t _M0L8capacityS2528;
  int32_t _M0L13new__capacityS680;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2522;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS2521;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS3753;
  int32_t _M0L6_2atmpS2523;
  int32_t _M0L8capacityS2525;
  int32_t _M0L6_2atmpS2524;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2526;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3752;
  struct _M0TPB5EntryGsfE* _M0L1xS681;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS678 = _M0L4selfS679->$5;
  _M0L8capacityS2528 = _M0L4selfS679->$2;
  _M0L13new__capacityS680 = _M0L8capacityS2528 << 1;
  _M0L6_2atmpS2522 = 0;
  _M0L6_2atmpS2521
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS680, _M0L6_2atmpS2522);
  _M0L6_2aoldS3753 = _M0L4selfS679->$0;
  if (_M0L9old__headS678) {
    moonbit_incref(_M0L9old__headS678);
  }
  moonbit_decref(_M0L6_2aoldS3753);
  _M0L4selfS679->$0 = _M0L6_2atmpS2521;
  _M0L4selfS679->$2 = _M0L13new__capacityS680;
  _M0L6_2atmpS2523 = _M0L13new__capacityS680 - 1;
  _M0L4selfS679->$3 = _M0L6_2atmpS2523;
  _M0L8capacityS2525 = _M0L4selfS679->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2524 = _M0FPB21calc__grow__threshold(_M0L8capacityS2525);
  _M0L4selfS679->$4 = _M0L6_2atmpS2524;
  _M0L4selfS679->$1 = 0;
  _M0L6_2atmpS2526 = 0;
  _M0L6_2aoldS3752 = _M0L4selfS679->$5;
  if (_M0L6_2aoldS3752) {
    moonbit_decref(_M0L6_2aoldS3752);
  }
  _M0L4selfS679->$5 = _M0L6_2atmpS2526;
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
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2527 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3750 = _M0L4_2aeS684->$1;
      if (_M0L15next__in__chainS685) {
        moonbit_incref(_M0L15next__in__chainS685);
      }
      if (_M0L6_2aoldS3750) {
        moonbit_decref(_M0L6_2aoldS3750);
      }
      _M0L4_2aeS684->$1 = _M0L6_2atmpS2527;
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
  int32_t _M0L8capacityS2536;
  int32_t _M0L13new__capacityS688;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2530;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS2529;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS3758;
  int32_t _M0L6_2atmpS2531;
  int32_t _M0L8capacityS2533;
  int32_t _M0L6_2atmpS2532;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2534;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3757;
  struct _M0TPB5EntryGsiE* _M0L1xS689;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS686 = _M0L4selfS687->$5;
  _M0L8capacityS2536 = _M0L4selfS687->$2;
  _M0L13new__capacityS688 = _M0L8capacityS2536 << 1;
  _M0L6_2atmpS2530 = 0;
  _M0L6_2atmpS2529
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS688, _M0L6_2atmpS2530);
  _M0L6_2aoldS3758 = _M0L4selfS687->$0;
  if (_M0L9old__headS686) {
    moonbit_incref(_M0L9old__headS686);
  }
  moonbit_decref(_M0L6_2aoldS3758);
  _M0L4selfS687->$0 = _M0L6_2atmpS2529;
  _M0L4selfS687->$2 = _M0L13new__capacityS688;
  _M0L6_2atmpS2531 = _M0L13new__capacityS688 - 1;
  _M0L4selfS687->$3 = _M0L6_2atmpS2531;
  _M0L8capacityS2533 = _M0L4selfS687->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2532 = _M0FPB21calc__grow__threshold(_M0L8capacityS2533);
  _M0L4selfS687->$4 = _M0L6_2atmpS2532;
  _M0L4selfS687->$1 = 0;
  _M0L6_2atmpS2534 = 0;
  _M0L6_2aoldS3757 = _M0L4selfS687->$5;
  if (_M0L6_2aoldS3757) {
    moonbit_decref(_M0L6_2aoldS3757);
  }
  _M0L4selfS687->$5 = _M0L6_2atmpS2534;
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
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2535 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3755 = _M0L4_2aeS692->$1;
      if (_M0L15next__in__chainS693) {
        moonbit_incref(_M0L15next__in__chainS693);
      }
      if (_M0L6_2aoldS3755) {
        moonbit_decref(_M0L6_2aoldS3755);
      }
      _M0L4_2aeS692->$1 = _M0L6_2atmpS2535;
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

int32_t _M0MPB3Map20rehash__place__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS614,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L5outerS610
) {
  int32_t _M0L4hashS609;
  int32_t _M0L14capacity__maskS2456;
  int32_t _M0L6_2atmpS2455;
  int32_t _M0L3pslS611;
  int32_t _M0L3idxS612;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS609 = _M0L5outerS610->$3;
  _M0L14capacity__maskS2456 = _M0L4selfS614->$3;
  _M0L6_2atmpS2455 = _M0L4hashS609 & _M0L14capacity__maskS2456;
  _M0L3pslS611 = 0;
  _M0L3idxS612 = _M0L6_2atmpS2455;
  while (1) {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2454 =
      _M0L4selfS614->$0;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS613;
    if (
      _M0L3idxS612 < 0
      || _M0L3idxS612 >= Moonbit_array_length(_M0L7entriesS2454)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS613
    = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2454[
        _M0L3idxS612
      ];
    if (_M0L7_2abindS613 == 0) {
      int32_t _M0L4tailS2447;
      _M0L5outerS610->$2 = _M0L3pslS611;
      _M0L4tailS2447 = _M0L4selfS614->$6;
      _M0L5outerS610->$0 = _M0L4tailS2447;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS615 =
        _M0L7_2abindS613;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2acurrS616 =
        _M0L7_2aSomeS615;
      int32_t _M0L3pslS2448 = _M0L7_2acurrS616->$2;
      if (_M0L3pslS611 > _M0L3pslS2448) {
        int32_t _M0L4tailS2449;
        moonbit_incref(_M0L7_2acurrS616);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L7_2acurrS616);
        moonbit_decref(_M0L7_2acurrS616);
        _M0L5outerS610->$2 = _M0L3pslS611;
        _M0L4tailS2449 = _M0L4selfS614->$6;
        _M0L5outerS610->$0 = _M0L4tailS2449;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2450 = _M0L3pslS611 + 1;
        int32_t _M0L6_2atmpS2452 = _M0L3idxS612 + 1;
        int32_t _M0L14capacity__maskS2453 = _M0L4selfS614->$3;
        int32_t _M0L6_2atmpS2451 =
          _M0L6_2atmpS2452 & _M0L14capacity__maskS2453;
        _M0L3pslS611 = _M0L6_2atmpS2450;
        _M0L3idxS612 = _M0L6_2atmpS2451;
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
  int32_t _M0L14capacity__maskS2466;
  int32_t _M0L6_2atmpS2465;
  int32_t _M0L3pslS620;
  int32_t _M0L3idxS621;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS618 = _M0L5outerS619->$3;
  _M0L14capacity__maskS2466 = _M0L4selfS623->$3;
  _M0L6_2atmpS2465 = _M0L4hashS618 & _M0L14capacity__maskS2466;
  _M0L3pslS620 = 0;
  _M0L3idxS621 = _M0L6_2atmpS2465;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2464 = _M0L4selfS623->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS622;
    if (
      _M0L3idxS621 < 0
      || _M0L3idxS621 >= Moonbit_array_length(_M0L7entriesS2464)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS622
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2464[_M0L3idxS621];
    if (_M0L7_2abindS622 == 0) {
      int32_t _M0L4tailS2457;
      _M0L5outerS619->$2 = _M0L3pslS620;
      _M0L4tailS2457 = _M0L4selfS623->$6;
      _M0L5outerS619->$0 = _M0L4tailS2457;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS624 = _M0L7_2abindS622;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS625 = _M0L7_2aSomeS624;
      int32_t _M0L3pslS2458 = _M0L7_2acurrS625->$2;
      if (_M0L3pslS620 > _M0L3pslS2458) {
        int32_t _M0L4tailS2459;
        moonbit_incref(_M0L7_2acurrS625);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS623, _M0L3idxS621, _M0L7_2acurrS625);
        moonbit_decref(_M0L7_2acurrS625);
        _M0L5outerS619->$2 = _M0L3pslS620;
        _M0L4tailS2459 = _M0L4selfS623->$6;
        _M0L5outerS619->$0 = _M0L4tailS2459;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2460 = _M0L3pslS620 + 1;
        int32_t _M0L6_2atmpS2462 = _M0L3idxS621 + 1;
        int32_t _M0L14capacity__maskS2463 = _M0L4selfS623->$3;
        int32_t _M0L6_2atmpS2461 =
          _M0L6_2atmpS2462 & _M0L14capacity__maskS2463;
        _M0L3pslS620 = _M0L6_2atmpS2460;
        _M0L3idxS621 = _M0L6_2atmpS2461;
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
  int32_t _M0L14capacity__maskS2476;
  int32_t _M0L6_2atmpS2475;
  int32_t _M0L3pslS629;
  int32_t _M0L3idxS630;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS627 = _M0L5outerS628->$3;
  _M0L14capacity__maskS2476 = _M0L4selfS632->$3;
  _M0L6_2atmpS2475 = _M0L4hashS627 & _M0L14capacity__maskS2476;
  _M0L3pslS629 = 0;
  _M0L3idxS630 = _M0L6_2atmpS2475;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2474 = _M0L4selfS632->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS631;
    if (
      _M0L3idxS630 < 0
      || _M0L3idxS630 >= Moonbit_array_length(_M0L7entriesS2474)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS631
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2474[_M0L3idxS630];
    if (_M0L7_2abindS631 == 0) {
      int32_t _M0L4tailS2467;
      _M0L5outerS628->$2 = _M0L3pslS629;
      _M0L4tailS2467 = _M0L4selfS632->$6;
      _M0L5outerS628->$0 = _M0L4tailS2467;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS632, _M0L3idxS630, _M0L5outerS628);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS633 = _M0L7_2abindS631;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS634 = _M0L7_2aSomeS633;
      int32_t _M0L3pslS2468 = _M0L7_2acurrS634->$2;
      if (_M0L3pslS629 > _M0L3pslS2468) {
        int32_t _M0L4tailS2469;
        moonbit_incref(_M0L7_2acurrS634);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS632, _M0L3idxS630, _M0L7_2acurrS634);
        moonbit_decref(_M0L7_2acurrS634);
        _M0L5outerS628->$2 = _M0L3pslS629;
        _M0L4tailS2469 = _M0L4selfS632->$6;
        _M0L5outerS628->$0 = _M0L4tailS2469;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS632, _M0L3idxS630, _M0L5outerS628);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2470 = _M0L3pslS629 + 1;
        int32_t _M0L6_2atmpS2472 = _M0L3idxS630 + 1;
        int32_t _M0L14capacity__maskS2473 = _M0L4selfS632->$3;
        int32_t _M0L6_2atmpS2471 =
          _M0L6_2atmpS2472 & _M0L14capacity__maskS2473;
        _M0L3pslS629 = _M0L6_2atmpS2470;
        _M0L3idxS630 = _M0L6_2atmpS2471;
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
  int32_t _M0L14capacity__maskS2486;
  int32_t _M0L6_2atmpS2485;
  int32_t _M0L3pslS638;
  int32_t _M0L3idxS639;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS636 = _M0L5outerS637->$3;
  _M0L14capacity__maskS2486 = _M0L4selfS641->$3;
  _M0L6_2atmpS2485 = _M0L4hashS636 & _M0L14capacity__maskS2486;
  _M0L3pslS638 = 0;
  _M0L3idxS639 = _M0L6_2atmpS2485;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2484 = _M0L4selfS641->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS640;
    if (
      _M0L3idxS639 < 0
      || _M0L3idxS639 >= Moonbit_array_length(_M0L7entriesS2484)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS640
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2484[_M0L3idxS639];
    if (_M0L7_2abindS640 == 0) {
      int32_t _M0L4tailS2477;
      _M0L5outerS637->$2 = _M0L3pslS638;
      _M0L4tailS2477 = _M0L4selfS641->$6;
      _M0L5outerS637->$0 = _M0L4tailS2477;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5outerS637);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS642 = _M0L7_2abindS640;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS643 = _M0L7_2aSomeS642;
      int32_t _M0L3pslS2478 = _M0L7_2acurrS643->$2;
      if (_M0L3pslS638 > _M0L3pslS2478) {
        int32_t _M0L4tailS2479;
        moonbit_incref(_M0L7_2acurrS643);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS641, _M0L3idxS639, _M0L7_2acurrS643);
        moonbit_decref(_M0L7_2acurrS643);
        _M0L5outerS637->$2 = _M0L3pslS638;
        _M0L4tailS2479 = _M0L4selfS641->$6;
        _M0L5outerS637->$0 = _M0L4tailS2479;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5outerS637);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2480 = _M0L3pslS638 + 1;
        int32_t _M0L6_2atmpS2482 = _M0L3idxS639 + 1;
        int32_t _M0L14capacity__maskS2483 = _M0L4selfS641->$3;
        int32_t _M0L6_2atmpS2481 =
          _M0L6_2atmpS2482 & _M0L14capacity__maskS2483;
        _M0L3pslS638 = _M0L6_2atmpS2480;
        _M0L3idxS639 = _M0L6_2atmpS2481;
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
  int32_t _M0L14capacity__maskS2496;
  int32_t _M0L6_2atmpS2495;
  int32_t _M0L3pslS647;
  int32_t _M0L3idxS648;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS645 = _M0L5outerS646->$3;
  _M0L14capacity__maskS2496 = _M0L4selfS650->$3;
  _M0L6_2atmpS2495 = _M0L4hashS645 & _M0L14capacity__maskS2496;
  _M0L3pslS647 = 0;
  _M0L3idxS648 = _M0L6_2atmpS2495;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2494 = _M0L4selfS650->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS649;
    if (
      _M0L3idxS648 < 0
      || _M0L3idxS648 >= Moonbit_array_length(_M0L7entriesS2494)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS649
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2494[_M0L3idxS648];
    if (_M0L7_2abindS649 == 0) {
      int32_t _M0L4tailS2487;
      _M0L5outerS646->$2 = _M0L3pslS647;
      _M0L4tailS2487 = _M0L4selfS650->$6;
      _M0L5outerS646->$0 = _M0L4tailS2487;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS650, _M0L3idxS648, _M0L5outerS646);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS651 = _M0L7_2abindS649;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS652 = _M0L7_2aSomeS651;
      int32_t _M0L3pslS2488 = _M0L7_2acurrS652->$2;
      if (_M0L3pslS647 > _M0L3pslS2488) {
        int32_t _M0L4tailS2489;
        moonbit_incref(_M0L7_2acurrS652);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS650, _M0L3idxS648, _M0L7_2acurrS652);
        moonbit_decref(_M0L7_2acurrS652);
        _M0L5outerS646->$2 = _M0L3pslS647;
        _M0L4tailS2489 = _M0L4selfS650->$6;
        _M0L5outerS646->$0 = _M0L4tailS2489;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS650, _M0L3idxS648, _M0L5outerS646);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2490 = _M0L3pslS647 + 1;
        int32_t _M0L6_2atmpS2492 = _M0L3idxS648 + 1;
        int32_t _M0L14capacity__maskS2493 = _M0L4selfS650->$3;
        int32_t _M0L6_2atmpS2491 =
          _M0L6_2atmpS2492 & _M0L14capacity__maskS2493;
        _M0L3pslS647 = _M0L6_2atmpS2490;
        _M0L3idxS648 = _M0L6_2atmpS2491;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS563,
  int32_t _M0L3idxS568,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L5entryS567
) {
  int32_t _M0L3pslS2382;
  int32_t _M0L6_2atmpS2378;
  int32_t _M0L6_2atmpS2380;
  int32_t _M0L14capacity__maskS2381;
  int32_t _M0L6_2atmpS2379;
  int32_t _M0L3pslS559;
  int32_t _M0L3idxS560;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L5entryS561;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2382 = _M0L5entryS567->$2;
  _M0L6_2atmpS2378 = _M0L3pslS2382 + 1;
  _M0L6_2atmpS2380 = _M0L3idxS568 + 1;
  _M0L14capacity__maskS2381 = _M0L4selfS563->$3;
  _M0L6_2atmpS2379 = _M0L6_2atmpS2380 & _M0L14capacity__maskS2381;
  moonbit_incref(_M0L5entryS567);
  _M0L3pslS559 = _M0L6_2atmpS2378;
  _M0L3idxS560 = _M0L6_2atmpS2379;
  _M0L5entryS561 = _M0L5entryS567;
  while (1) {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2377 =
      _M0L4selfS563->$0;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS562;
    if (
      _M0L3idxS560 < 0
      || _M0L3idxS560 >= Moonbit_array_length(_M0L7entriesS2377)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS562
    = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2377[
        _M0L3idxS560
      ];
    if (_M0L7_2abindS562 == 0) {
      _M0L5entryS561->$2 = _M0L3pslS559;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS563, _M0L5entryS561, _M0L3idxS560);
      moonbit_decref(_M0L5entryS561);
      break;
    } else {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS565 =
        _M0L7_2abindS562;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L14_2acurr__entryS566 =
        _M0L7_2aSomeS565;
      int32_t _M0L3pslS2367 = _M0L14_2acurr__entryS566->$2;
      if (_M0L3pslS559 > _M0L3pslS2367) {
        int32_t _M0L3pslS2372;
        int32_t _M0L6_2atmpS2368;
        int32_t _M0L6_2atmpS2370;
        int32_t _M0L14capacity__maskS2371;
        int32_t _M0L6_2atmpS2369;
        _M0L5entryS561->$2 = _M0L3pslS559;
        moonbit_incref(_M0L14_2acurr__entryS566);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(_M0L4selfS563, _M0L5entryS561, _M0L3idxS560);
        moonbit_decref(_M0L5entryS561);
        _M0L3pslS2372 = _M0L14_2acurr__entryS566->$2;
        _M0L6_2atmpS2368 = _M0L3pslS2372 + 1;
        _M0L6_2atmpS2370 = _M0L3idxS560 + 1;
        _M0L14capacity__maskS2371 = _M0L4selfS563->$3;
        _M0L6_2atmpS2369 = _M0L6_2atmpS2370 & _M0L14capacity__maskS2371;
        _M0L3pslS559 = _M0L6_2atmpS2368;
        _M0L3idxS560 = _M0L6_2atmpS2369;
        _M0L5entryS561 = _M0L14_2acurr__entryS566;
        continue;
      } else {
        int32_t _M0L6_2atmpS2373 = _M0L3pslS559 + 1;
        int32_t _M0L6_2atmpS2375 = _M0L3idxS560 + 1;
        int32_t _M0L14capacity__maskS2376 = _M0L4selfS563->$3;
        int32_t _M0L6_2atmpS2374 =
          _M0L6_2atmpS2375 & _M0L14capacity__maskS2376;
        struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _tmp_4156 =
          _M0L5entryS561;
        _M0L3pslS559 = _M0L6_2atmpS2373;
        _M0L3idxS560 = _M0L6_2atmpS2374;
        _M0L5entryS561 = _tmp_4156;
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
  int32_t _M0L3pslS2398;
  int32_t _M0L6_2atmpS2394;
  int32_t _M0L6_2atmpS2396;
  int32_t _M0L14capacity__maskS2397;
  int32_t _M0L6_2atmpS2395;
  int32_t _M0L3pslS569;
  int32_t _M0L3idxS570;
  struct _M0TPB5EntryGssE* _M0L5entryS571;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2398 = _M0L5entryS577->$2;
  _M0L6_2atmpS2394 = _M0L3pslS2398 + 1;
  _M0L6_2atmpS2396 = _M0L3idxS578 + 1;
  _M0L14capacity__maskS2397 = _M0L4selfS573->$3;
  _M0L6_2atmpS2395 = _M0L6_2atmpS2396 & _M0L14capacity__maskS2397;
  moonbit_incref(_M0L5entryS577);
  _M0L3pslS569 = _M0L6_2atmpS2394;
  _M0L3idxS570 = _M0L6_2atmpS2395;
  _M0L5entryS571 = _M0L5entryS577;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2393 = _M0L4selfS573->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS572;
    if (
      _M0L3idxS570 < 0
      || _M0L3idxS570 >= Moonbit_array_length(_M0L7entriesS2393)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS572
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2393[_M0L3idxS570];
    if (_M0L7_2abindS572 == 0) {
      _M0L5entryS571->$2 = _M0L3pslS569;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS573, _M0L5entryS571, _M0L3idxS570);
      moonbit_decref(_M0L5entryS571);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS575 = _M0L7_2abindS572;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS576 = _M0L7_2aSomeS575;
      int32_t _M0L3pslS2383 = _M0L14_2acurr__entryS576->$2;
      if (_M0L3pslS569 > _M0L3pslS2383) {
        int32_t _M0L3pslS2388;
        int32_t _M0L6_2atmpS2384;
        int32_t _M0L6_2atmpS2386;
        int32_t _M0L14capacity__maskS2387;
        int32_t _M0L6_2atmpS2385;
        _M0L5entryS571->$2 = _M0L3pslS569;
        moonbit_incref(_M0L14_2acurr__entryS576);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS573, _M0L5entryS571, _M0L3idxS570);
        moonbit_decref(_M0L5entryS571);
        _M0L3pslS2388 = _M0L14_2acurr__entryS576->$2;
        _M0L6_2atmpS2384 = _M0L3pslS2388 + 1;
        _M0L6_2atmpS2386 = _M0L3idxS570 + 1;
        _M0L14capacity__maskS2387 = _M0L4selfS573->$3;
        _M0L6_2atmpS2385 = _M0L6_2atmpS2386 & _M0L14capacity__maskS2387;
        _M0L3pslS569 = _M0L6_2atmpS2384;
        _M0L3idxS570 = _M0L6_2atmpS2385;
        _M0L5entryS571 = _M0L14_2acurr__entryS576;
        continue;
      } else {
        int32_t _M0L6_2atmpS2389 = _M0L3pslS569 + 1;
        int32_t _M0L6_2atmpS2391 = _M0L3idxS570 + 1;
        int32_t _M0L14capacity__maskS2392 = _M0L4selfS573->$3;
        int32_t _M0L6_2atmpS2390 =
          _M0L6_2atmpS2391 & _M0L14capacity__maskS2392;
        struct _M0TPB5EntryGssE* _tmp_4158 = _M0L5entryS571;
        _M0L3pslS569 = _M0L6_2atmpS2389;
        _M0L3idxS570 = _M0L6_2atmpS2390;
        _M0L5entryS571 = _tmp_4158;
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
  int32_t _M0L3pslS2414;
  int32_t _M0L6_2atmpS2410;
  int32_t _M0L6_2atmpS2412;
  int32_t _M0L14capacity__maskS2413;
  int32_t _M0L6_2atmpS2411;
  int32_t _M0L3pslS579;
  int32_t _M0L3idxS580;
  struct _M0TPB5EntryGsbE* _M0L5entryS581;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2414 = _M0L5entryS587->$2;
  _M0L6_2atmpS2410 = _M0L3pslS2414 + 1;
  _M0L6_2atmpS2412 = _M0L3idxS588 + 1;
  _M0L14capacity__maskS2413 = _M0L4selfS583->$3;
  _M0L6_2atmpS2411 = _M0L6_2atmpS2412 & _M0L14capacity__maskS2413;
  moonbit_incref(_M0L5entryS587);
  _M0L3pslS579 = _M0L6_2atmpS2410;
  _M0L3idxS580 = _M0L6_2atmpS2411;
  _M0L5entryS581 = _M0L5entryS587;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2409 = _M0L4selfS583->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS582;
    if (
      _M0L3idxS580 < 0
      || _M0L3idxS580 >= Moonbit_array_length(_M0L7entriesS2409)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS582
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2409[_M0L3idxS580];
    if (_M0L7_2abindS582 == 0) {
      _M0L5entryS581->$2 = _M0L3pslS579;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
      moonbit_decref(_M0L5entryS581);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS585 = _M0L7_2abindS582;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS586 = _M0L7_2aSomeS585;
      int32_t _M0L3pslS2399 = _M0L14_2acurr__entryS586->$2;
      if (_M0L3pslS579 > _M0L3pslS2399) {
        int32_t _M0L3pslS2404;
        int32_t _M0L6_2atmpS2400;
        int32_t _M0L6_2atmpS2402;
        int32_t _M0L14capacity__maskS2403;
        int32_t _M0L6_2atmpS2401;
        _M0L5entryS581->$2 = _M0L3pslS579;
        moonbit_incref(_M0L14_2acurr__entryS586);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
        moonbit_decref(_M0L5entryS581);
        _M0L3pslS2404 = _M0L14_2acurr__entryS586->$2;
        _M0L6_2atmpS2400 = _M0L3pslS2404 + 1;
        _M0L6_2atmpS2402 = _M0L3idxS580 + 1;
        _M0L14capacity__maskS2403 = _M0L4selfS583->$3;
        _M0L6_2atmpS2401 = _M0L6_2atmpS2402 & _M0L14capacity__maskS2403;
        _M0L3pslS579 = _M0L6_2atmpS2400;
        _M0L3idxS580 = _M0L6_2atmpS2401;
        _M0L5entryS581 = _M0L14_2acurr__entryS586;
        continue;
      } else {
        int32_t _M0L6_2atmpS2405 = _M0L3pslS579 + 1;
        int32_t _M0L6_2atmpS2407 = _M0L3idxS580 + 1;
        int32_t _M0L14capacity__maskS2408 = _M0L4selfS583->$3;
        int32_t _M0L6_2atmpS2406 =
          _M0L6_2atmpS2407 & _M0L14capacity__maskS2408;
        struct _M0TPB5EntryGsbE* _tmp_4160 = _M0L5entryS581;
        _M0L3pslS579 = _M0L6_2atmpS2405;
        _M0L3idxS580 = _M0L6_2atmpS2406;
        _M0L5entryS581 = _tmp_4160;
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
  int32_t _M0L3pslS2430;
  int32_t _M0L6_2atmpS2426;
  int32_t _M0L6_2atmpS2428;
  int32_t _M0L14capacity__maskS2429;
  int32_t _M0L6_2atmpS2427;
  int32_t _M0L3pslS589;
  int32_t _M0L3idxS590;
  struct _M0TPB5EntryGsfE* _M0L5entryS591;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2430 = _M0L5entryS597->$2;
  _M0L6_2atmpS2426 = _M0L3pslS2430 + 1;
  _M0L6_2atmpS2428 = _M0L3idxS598 + 1;
  _M0L14capacity__maskS2429 = _M0L4selfS593->$3;
  _M0L6_2atmpS2427 = _M0L6_2atmpS2428 & _M0L14capacity__maskS2429;
  moonbit_incref(_M0L5entryS597);
  _M0L3pslS589 = _M0L6_2atmpS2426;
  _M0L3idxS590 = _M0L6_2atmpS2427;
  _M0L5entryS591 = _M0L5entryS597;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2425 = _M0L4selfS593->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS592;
    if (
      _M0L3idxS590 < 0
      || _M0L3idxS590 >= Moonbit_array_length(_M0L7entriesS2425)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS592
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2425[_M0L3idxS590];
    if (_M0L7_2abindS592 == 0) {
      _M0L5entryS591->$2 = _M0L3pslS589;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
      moonbit_decref(_M0L5entryS591);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS595 = _M0L7_2abindS592;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS596 = _M0L7_2aSomeS595;
      int32_t _M0L3pslS2415 = _M0L14_2acurr__entryS596->$2;
      if (_M0L3pslS589 > _M0L3pslS2415) {
        int32_t _M0L3pslS2420;
        int32_t _M0L6_2atmpS2416;
        int32_t _M0L6_2atmpS2418;
        int32_t _M0L14capacity__maskS2419;
        int32_t _M0L6_2atmpS2417;
        _M0L5entryS591->$2 = _M0L3pslS589;
        moonbit_incref(_M0L14_2acurr__entryS596);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
        moonbit_decref(_M0L5entryS591);
        _M0L3pslS2420 = _M0L14_2acurr__entryS596->$2;
        _M0L6_2atmpS2416 = _M0L3pslS2420 + 1;
        _M0L6_2atmpS2418 = _M0L3idxS590 + 1;
        _M0L14capacity__maskS2419 = _M0L4selfS593->$3;
        _M0L6_2atmpS2417 = _M0L6_2atmpS2418 & _M0L14capacity__maskS2419;
        _M0L3pslS589 = _M0L6_2atmpS2416;
        _M0L3idxS590 = _M0L6_2atmpS2417;
        _M0L5entryS591 = _M0L14_2acurr__entryS596;
        continue;
      } else {
        int32_t _M0L6_2atmpS2421 = _M0L3pslS589 + 1;
        int32_t _M0L6_2atmpS2423 = _M0L3idxS590 + 1;
        int32_t _M0L14capacity__maskS2424 = _M0L4selfS593->$3;
        int32_t _M0L6_2atmpS2422 =
          _M0L6_2atmpS2423 & _M0L14capacity__maskS2424;
        struct _M0TPB5EntryGsfE* _tmp_4162 = _M0L5entryS591;
        _M0L3pslS589 = _M0L6_2atmpS2421;
        _M0L3idxS590 = _M0L6_2atmpS2422;
        _M0L5entryS591 = _tmp_4162;
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
  int32_t _M0L3pslS2446;
  int32_t _M0L6_2atmpS2442;
  int32_t _M0L6_2atmpS2444;
  int32_t _M0L14capacity__maskS2445;
  int32_t _M0L6_2atmpS2443;
  int32_t _M0L3pslS599;
  int32_t _M0L3idxS600;
  struct _M0TPB5EntryGsiE* _M0L5entryS601;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2446 = _M0L5entryS607->$2;
  _M0L6_2atmpS2442 = _M0L3pslS2446 + 1;
  _M0L6_2atmpS2444 = _M0L3idxS608 + 1;
  _M0L14capacity__maskS2445 = _M0L4selfS603->$3;
  _M0L6_2atmpS2443 = _M0L6_2atmpS2444 & _M0L14capacity__maskS2445;
  moonbit_incref(_M0L5entryS607);
  _M0L3pslS599 = _M0L6_2atmpS2442;
  _M0L3idxS600 = _M0L6_2atmpS2443;
  _M0L5entryS601 = _M0L5entryS607;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2441 = _M0L4selfS603->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS602;
    if (
      _M0L3idxS600 < 0
      || _M0L3idxS600 >= Moonbit_array_length(_M0L7entriesS2441)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS602
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2441[_M0L3idxS600];
    if (_M0L7_2abindS602 == 0) {
      _M0L5entryS601->$2 = _M0L3pslS599;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
      moonbit_decref(_M0L5entryS601);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS605 = _M0L7_2abindS602;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS606 = _M0L7_2aSomeS605;
      int32_t _M0L3pslS2431 = _M0L14_2acurr__entryS606->$2;
      if (_M0L3pslS599 > _M0L3pslS2431) {
        int32_t _M0L3pslS2436;
        int32_t _M0L6_2atmpS2432;
        int32_t _M0L6_2atmpS2434;
        int32_t _M0L14capacity__maskS2435;
        int32_t _M0L6_2atmpS2433;
        _M0L5entryS601->$2 = _M0L3pslS599;
        moonbit_incref(_M0L14_2acurr__entryS606);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
        moonbit_decref(_M0L5entryS601);
        _M0L3pslS2436 = _M0L14_2acurr__entryS606->$2;
        _M0L6_2atmpS2432 = _M0L3pslS2436 + 1;
        _M0L6_2atmpS2434 = _M0L3idxS600 + 1;
        _M0L14capacity__maskS2435 = _M0L4selfS603->$3;
        _M0L6_2atmpS2433 = _M0L6_2atmpS2434 & _M0L14capacity__maskS2435;
        _M0L3pslS599 = _M0L6_2atmpS2432;
        _M0L3idxS600 = _M0L6_2atmpS2433;
        _M0L5entryS601 = _M0L14_2acurr__entryS606;
        continue;
      } else {
        int32_t _M0L6_2atmpS2437 = _M0L3pslS599 + 1;
        int32_t _M0L6_2atmpS2439 = _M0L3idxS600 + 1;
        int32_t _M0L14capacity__maskS2440 = _M0L4selfS603->$3;
        int32_t _M0L6_2atmpS2438 =
          _M0L6_2atmpS2439 & _M0L14capacity__maskS2440;
        struct _M0TPB5EntryGsiE* _tmp_4164 = _M0L5entryS601;
        _M0L3pslS599 = _M0L6_2atmpS2437;
        _M0L3idxS600 = _M0L6_2atmpS2438;
        _M0L5entryS601 = _tmp_4164;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS529,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L5entryS531,
  int32_t _M0L8new__idxS530
) {
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2357;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2358;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3781;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS532;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2357 = _M0L4selfS529->$0;
  _M0L6_2atmpS2358 = _M0L5entryS531;
  if (
    _M0L8new__idxS530 < 0
    || _M0L8new__idxS530 >= Moonbit_array_length(_M0L7entriesS2357)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3781
  = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2357[
      _M0L8new__idxS530
    ];
  if (_M0L6_2atmpS2358) {
    moonbit_incref(_M0L6_2atmpS2358);
  }
  if (_M0L6_2aoldS3781) {
    moonbit_decref(_M0L6_2aoldS3781);
  }
  _M0L7entriesS2357[_M0L8new__idxS530] = _M0L6_2atmpS2358;
  _M0L7_2abindS532 = _M0L5entryS531->$1;
  if (_M0L7_2abindS532 == 0) {
    _M0L4selfS529->$6 = _M0L8new__idxS530;
  } else {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS533 =
      _M0L7_2abindS532;
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2anextS534 =
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
  struct _M0TPB5EntryGsiE** _M0L7entriesS2359;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2360;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3784;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS538;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2359 = _M0L4selfS535->$0;
  _M0L6_2atmpS2360 = _M0L5entryS537;
  if (
    _M0L8new__idxS536 < 0
    || _M0L8new__idxS536 >= Moonbit_array_length(_M0L7entriesS2359)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3784
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2359[_M0L8new__idxS536];
  if (_M0L6_2atmpS2360) {
    moonbit_incref(_M0L6_2atmpS2360);
  }
  if (_M0L6_2aoldS3784) {
    moonbit_decref(_M0L6_2aoldS3784);
  }
  _M0L7entriesS2359[_M0L8new__idxS536] = _M0L6_2atmpS2360;
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
  struct _M0TPB5EntryGssE** _M0L7entriesS2361;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2362;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3787;
  struct _M0TPB5EntryGssE* _M0L7_2abindS544;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2361 = _M0L4selfS541->$0;
  _M0L6_2atmpS2362 = _M0L5entryS543;
  if (
    _M0L8new__idxS542 < 0
    || _M0L8new__idxS542 >= Moonbit_array_length(_M0L7entriesS2361)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3787
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2361[_M0L8new__idxS542];
  if (_M0L6_2atmpS2362) {
    moonbit_incref(_M0L6_2atmpS2362);
  }
  if (_M0L6_2aoldS3787) {
    moonbit_decref(_M0L6_2aoldS3787);
  }
  _M0L7entriesS2361[_M0L8new__idxS542] = _M0L6_2atmpS2362;
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
  struct _M0TPB5EntryGsbE** _M0L7entriesS2363;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2364;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3790;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS550;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2363 = _M0L4selfS547->$0;
  _M0L6_2atmpS2364 = _M0L5entryS549;
  if (
    _M0L8new__idxS548 < 0
    || _M0L8new__idxS548 >= Moonbit_array_length(_M0L7entriesS2363)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3790
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2363[_M0L8new__idxS548];
  if (_M0L6_2atmpS2364) {
    moonbit_incref(_M0L6_2atmpS2364);
  }
  if (_M0L6_2aoldS3790) {
    moonbit_decref(_M0L6_2aoldS3790);
  }
  _M0L7entriesS2363[_M0L8new__idxS548] = _M0L6_2atmpS2364;
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
  struct _M0TPB5EntryGsfE** _M0L7entriesS2365;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2366;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3793;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS556;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2365 = _M0L4selfS553->$0;
  _M0L6_2atmpS2366 = _M0L5entryS555;
  if (
    _M0L8new__idxS554 < 0
    || _M0L8new__idxS554 >= Moonbit_array_length(_M0L7entriesS2365)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3793
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2365[_M0L8new__idxS554];
  if (_M0L6_2atmpS2366) {
    moonbit_incref(_M0L6_2atmpS2366);
  }
  if (_M0L6_2aoldS3793) {
    moonbit_decref(_M0L6_2aoldS3793);
  }
  _M0L7entriesS2365[_M0L8new__idxS554] = _M0L6_2atmpS2366;
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

int32_t _M0MPB3Map20add__entry__to__tailGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS510,
  int32_t _M0L3idxS512,
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L5entryS511
) {
  int32_t _M0L7_2abindS509;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2317;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2318;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3795;
  int32_t _M0L4sizeS2320;
  int32_t _M0L6_2atmpS2319;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS509 = _M0L4selfS510->$6;
  switch (_M0L7_2abindS509) {
    case -1: {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2312 =
        _M0L5entryS511;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3797 =
        _M0L4selfS510->$5;
      if (_M0L6_2atmpS2312) {
        moonbit_incref(_M0L6_2atmpS2312);
      }
      if (_M0L6_2aoldS3797) {
        moonbit_decref(_M0L6_2aoldS3797);
      }
      _M0L4selfS510->$5 = _M0L6_2atmpS2312;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7entriesS2316 =
        _M0L4selfS510->$0;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2315;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2313;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2314;
      struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2aoldS3798;
      if (
        _M0L7_2abindS509 < 0
        || _M0L7_2abindS509 >= Moonbit_array_length(_M0L7entriesS2316)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2315
      = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2316[
          _M0L7_2abindS509
        ];
      if (_M0L6_2atmpS2315) {
        moonbit_incref(_M0L6_2atmpS2315);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2313
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE(_M0L6_2atmpS2315);
      if (_M0L6_2atmpS2315) {
        moonbit_decref(_M0L6_2atmpS2315);
      }
      _M0L6_2atmpS2314 = _M0L5entryS511;
      _M0L6_2aoldS3798 = _M0L6_2atmpS2313->$1;
      if (_M0L6_2atmpS2314) {
        moonbit_incref(_M0L6_2atmpS2314);
      }
      if (_M0L6_2aoldS3798) {
        moonbit_decref(_M0L6_2aoldS3798);
      }
      _M0L6_2atmpS2313->$1 = _M0L6_2atmpS2314;
      moonbit_decref(_M0L6_2atmpS2313);
      break;
    }
  }
  _M0L4selfS510->$6 = _M0L3idxS512;
  _M0L7entriesS2317 = _M0L4selfS510->$0;
  _M0L6_2atmpS2318 = _M0L5entryS511;
  if (
    _M0L3idxS512 < 0
    || _M0L3idxS512 >= Moonbit_array_length(_M0L7entriesS2317)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3795
  = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)_M0L7entriesS2317[
      _M0L3idxS512
    ];
  if (_M0L6_2atmpS2318) {
    moonbit_incref(_M0L6_2atmpS2318);
  }
  if (_M0L6_2aoldS3795) {
    moonbit_decref(_M0L6_2aoldS3795);
  }
  _M0L7entriesS2317[_M0L3idxS512] = _M0L6_2atmpS2318;
  _M0L4sizeS2320 = _M0L4selfS510->$1;
  _M0L6_2atmpS2319 = _M0L4sizeS2320 + 1;
  _M0L4selfS510->$1 = _M0L6_2atmpS2319;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS514,
  int32_t _M0L3idxS516,
  struct _M0TPB5EntryGssE* _M0L5entryS515
) {
  int32_t _M0L7_2abindS513;
  struct _M0TPB5EntryGssE** _M0L7entriesS2326;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2327;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3801;
  int32_t _M0L4sizeS2329;
  int32_t _M0L6_2atmpS2328;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS513 = _M0L4selfS514->$6;
  switch (_M0L7_2abindS513) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2321 = _M0L5entryS515;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3803 = _M0L4selfS514->$5;
      if (_M0L6_2atmpS2321) {
        moonbit_incref(_M0L6_2atmpS2321);
      }
      if (_M0L6_2aoldS3803) {
        moonbit_decref(_M0L6_2aoldS3803);
      }
      _M0L4selfS514->$5 = _M0L6_2atmpS2321;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS2325 = _M0L4selfS514->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2324;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2322;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2323;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3804;
      if (
        _M0L7_2abindS513 < 0
        || _M0L7_2abindS513 >= Moonbit_array_length(_M0L7entriesS2325)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2324
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS2325[_M0L7_2abindS513];
      if (_M0L6_2atmpS2324) {
        moonbit_incref(_M0L6_2atmpS2324);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2322
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS2324);
      if (_M0L6_2atmpS2324) {
        moonbit_decref(_M0L6_2atmpS2324);
      }
      _M0L6_2atmpS2323 = _M0L5entryS515;
      _M0L6_2aoldS3804 = _M0L6_2atmpS2322->$1;
      if (_M0L6_2atmpS2323) {
        moonbit_incref(_M0L6_2atmpS2323);
      }
      if (_M0L6_2aoldS3804) {
        moonbit_decref(_M0L6_2aoldS3804);
      }
      _M0L6_2atmpS2322->$1 = _M0L6_2atmpS2323;
      moonbit_decref(_M0L6_2atmpS2322);
      break;
    }
  }
  _M0L4selfS514->$6 = _M0L3idxS516;
  _M0L7entriesS2326 = _M0L4selfS514->$0;
  _M0L6_2atmpS2327 = _M0L5entryS515;
  if (
    _M0L3idxS516 < 0
    || _M0L3idxS516 >= Moonbit_array_length(_M0L7entriesS2326)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3801
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2326[_M0L3idxS516];
  if (_M0L6_2atmpS2327) {
    moonbit_incref(_M0L6_2atmpS2327);
  }
  if (_M0L6_2aoldS3801) {
    moonbit_decref(_M0L6_2aoldS3801);
  }
  _M0L7entriesS2326[_M0L3idxS516] = _M0L6_2atmpS2327;
  _M0L4sizeS2329 = _M0L4selfS514->$1;
  _M0L6_2atmpS2328 = _M0L4sizeS2329 + 1;
  _M0L4selfS514->$1 = _M0L6_2atmpS2328;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS518,
  int32_t _M0L3idxS520,
  struct _M0TPB5EntryGsbE* _M0L5entryS519
) {
  int32_t _M0L7_2abindS517;
  struct _M0TPB5EntryGsbE** _M0L7entriesS2335;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2336;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3807;
  int32_t _M0L4sizeS2338;
  int32_t _M0L6_2atmpS2337;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS517 = _M0L4selfS518->$6;
  switch (_M0L7_2abindS517) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2330 = _M0L5entryS519;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3809 = _M0L4selfS518->$5;
      if (_M0L6_2atmpS2330) {
        moonbit_incref(_M0L6_2atmpS2330);
      }
      if (_M0L6_2aoldS3809) {
        moonbit_decref(_M0L6_2aoldS3809);
      }
      _M0L4selfS518->$5 = _M0L6_2atmpS2330;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2334 = _M0L4selfS518->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2333;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2331;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2332;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3810;
      if (
        _M0L7_2abindS517 < 0
        || _M0L7_2abindS517 >= Moonbit_array_length(_M0L7entriesS2334)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2333
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2334[_M0L7_2abindS517];
      if (_M0L6_2atmpS2333) {
        moonbit_incref(_M0L6_2atmpS2333);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2331
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2333);
      if (_M0L6_2atmpS2333) {
        moonbit_decref(_M0L6_2atmpS2333);
      }
      _M0L6_2atmpS2332 = _M0L5entryS519;
      _M0L6_2aoldS3810 = _M0L6_2atmpS2331->$1;
      if (_M0L6_2atmpS2332) {
        moonbit_incref(_M0L6_2atmpS2332);
      }
      if (_M0L6_2aoldS3810) {
        moonbit_decref(_M0L6_2aoldS3810);
      }
      _M0L6_2atmpS2331->$1 = _M0L6_2atmpS2332;
      moonbit_decref(_M0L6_2atmpS2331);
      break;
    }
  }
  _M0L4selfS518->$6 = _M0L3idxS520;
  _M0L7entriesS2335 = _M0L4selfS518->$0;
  _M0L6_2atmpS2336 = _M0L5entryS519;
  if (
    _M0L3idxS520 < 0
    || _M0L3idxS520 >= Moonbit_array_length(_M0L7entriesS2335)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3807
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2335[_M0L3idxS520];
  if (_M0L6_2atmpS2336) {
    moonbit_incref(_M0L6_2atmpS2336);
  }
  if (_M0L6_2aoldS3807) {
    moonbit_decref(_M0L6_2aoldS3807);
  }
  _M0L7entriesS2335[_M0L3idxS520] = _M0L6_2atmpS2336;
  _M0L4sizeS2338 = _M0L4selfS518->$1;
  _M0L6_2atmpS2337 = _M0L4sizeS2338 + 1;
  _M0L4selfS518->$1 = _M0L6_2atmpS2337;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS522,
  int32_t _M0L3idxS524,
  struct _M0TPB5EntryGsfE* _M0L5entryS523
) {
  int32_t _M0L7_2abindS521;
  struct _M0TPB5EntryGsfE** _M0L7entriesS2344;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2345;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3813;
  int32_t _M0L4sizeS2347;
  int32_t _M0L6_2atmpS2346;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS521 = _M0L4selfS522->$6;
  switch (_M0L7_2abindS521) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2339 = _M0L5entryS523;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3815 = _M0L4selfS522->$5;
      if (_M0L6_2atmpS2339) {
        moonbit_incref(_M0L6_2atmpS2339);
      }
      if (_M0L6_2aoldS3815) {
        moonbit_decref(_M0L6_2aoldS3815);
      }
      _M0L4selfS522->$5 = _M0L6_2atmpS2339;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2343 = _M0L4selfS522->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2342;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2340;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2341;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3816;
      if (
        _M0L7_2abindS521 < 0
        || _M0L7_2abindS521 >= Moonbit_array_length(_M0L7entriesS2343)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2342
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2343[_M0L7_2abindS521];
      if (_M0L6_2atmpS2342) {
        moonbit_incref(_M0L6_2atmpS2342);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2340
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2342);
      if (_M0L6_2atmpS2342) {
        moonbit_decref(_M0L6_2atmpS2342);
      }
      _M0L6_2atmpS2341 = _M0L5entryS523;
      _M0L6_2aoldS3816 = _M0L6_2atmpS2340->$1;
      if (_M0L6_2atmpS2341) {
        moonbit_incref(_M0L6_2atmpS2341);
      }
      if (_M0L6_2aoldS3816) {
        moonbit_decref(_M0L6_2aoldS3816);
      }
      _M0L6_2atmpS2340->$1 = _M0L6_2atmpS2341;
      moonbit_decref(_M0L6_2atmpS2340);
      break;
    }
  }
  _M0L4selfS522->$6 = _M0L3idxS524;
  _M0L7entriesS2344 = _M0L4selfS522->$0;
  _M0L6_2atmpS2345 = _M0L5entryS523;
  if (
    _M0L3idxS524 < 0
    || _M0L3idxS524 >= Moonbit_array_length(_M0L7entriesS2344)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3813
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2344[_M0L3idxS524];
  if (_M0L6_2atmpS2345) {
    moonbit_incref(_M0L6_2atmpS2345);
  }
  if (_M0L6_2aoldS3813) {
    moonbit_decref(_M0L6_2aoldS3813);
  }
  _M0L7entriesS2344[_M0L3idxS524] = _M0L6_2atmpS2345;
  _M0L4sizeS2347 = _M0L4selfS522->$1;
  _M0L6_2atmpS2346 = _M0L4sizeS2347 + 1;
  _M0L4selfS522->$1 = _M0L6_2atmpS2346;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS526,
  int32_t _M0L3idxS528,
  struct _M0TPB5EntryGsiE* _M0L5entryS527
) {
  int32_t _M0L7_2abindS525;
  struct _M0TPB5EntryGsiE** _M0L7entriesS2353;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2354;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3819;
  int32_t _M0L4sizeS2356;
  int32_t _M0L6_2atmpS2355;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS525 = _M0L4selfS526->$6;
  switch (_M0L7_2abindS525) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2348 = _M0L5entryS527;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3821 = _M0L4selfS526->$5;
      if (_M0L6_2atmpS2348) {
        moonbit_incref(_M0L6_2atmpS2348);
      }
      if (_M0L6_2aoldS3821) {
        moonbit_decref(_M0L6_2aoldS3821);
      }
      _M0L4selfS526->$5 = _M0L6_2atmpS2348;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2352 = _M0L4selfS526->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2351;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2349;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2350;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3822;
      if (
        _M0L7_2abindS525 < 0
        || _M0L7_2abindS525 >= Moonbit_array_length(_M0L7entriesS2352)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2351
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2352[_M0L7_2abindS525];
      if (_M0L6_2atmpS2351) {
        moonbit_incref(_M0L6_2atmpS2351);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2349
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2351);
      if (_M0L6_2atmpS2351) {
        moonbit_decref(_M0L6_2atmpS2351);
      }
      _M0L6_2atmpS2350 = _M0L5entryS527;
      _M0L6_2aoldS3822 = _M0L6_2atmpS2349->$1;
      if (_M0L6_2atmpS2350) {
        moonbit_incref(_M0L6_2atmpS2350);
      }
      if (_M0L6_2aoldS3822) {
        moonbit_decref(_M0L6_2aoldS3822);
      }
      _M0L6_2atmpS2349->$1 = _M0L6_2atmpS2350;
      moonbit_decref(_M0L6_2atmpS2349);
      break;
    }
  }
  _M0L4selfS526->$6 = _M0L3idxS528;
  _M0L7entriesS2353 = _M0L4selfS526->$0;
  _M0L6_2atmpS2354 = _M0L5entryS527;
  if (
    _M0L3idxS528 < 0
    || _M0L3idxS528 >= Moonbit_array_length(_M0L7entriesS2353)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3819
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2353[_M0L3idxS528];
  if (_M0L6_2atmpS2354) {
    moonbit_incref(_M0L6_2atmpS2354);
  }
  if (_M0L6_2aoldS3819) {
    moonbit_decref(_M0L6_2aoldS3819);
  }
  _M0L7entriesS2353[_M0L3idxS528] = _M0L6_2atmpS2354;
  _M0L4sizeS2356 = _M0L4selfS526->$1;
  _M0L6_2atmpS2355 = _M0L4sizeS2356 + 1;
  _M0L4selfS526->$1 = _M0L6_2atmpS2355;
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
  int32_t _M0L6_2atmpS2310;
  int32_t _M0L6_2atmpS2309;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS505 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS506);
  _M0L6_2atmpS2310 = _M0Lm8capacityS505;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2309 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2310);
  if (_M0L6lengthS506 > _M0L6_2atmpS2309) {
    int32_t _M0L6_2atmpS2311 = _M0Lm8capacityS505;
    _M0Lm8capacityS505 = _M0L6_2atmpS2311 * 2;
  }
  return _M0Lm8capacityS505;
}

struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0FPB8new__mapGsRP39moonbitdb9moonbitdb3lib10RedisValueE(
  int32_t _M0L8capacityS476
) {
  int32_t _M0L8capacityS475;
  int32_t _M0L7_2abindS477;
  int32_t _M0L7_2abindS478;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6_2atmpS2304;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE** _M0L7_2abindS479;
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2abindS480;
  struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _block_4165;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS475
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS476);
  _M0L7_2abindS477 = _M0L8capacityS475 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS478 = _M0FPB21calc__grow__threshold(_M0L8capacityS475);
  _M0L6_2atmpS2304 = 0;
  _M0L7_2abindS479
  = (struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS475, _M0L6_2atmpS2304);
  _M0L7_2abindS480 = 0;
  _block_4165
  = (struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP39moonbitdb9moonbitdb3lib10RedisValueE));
  Moonbit_object_header(_block_4165)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
  _block_4165->$0 = _M0L7_2abindS479;
  _block_4165->$1 = 0;
  _block_4165->$2 = _M0L8capacityS475;
  _block_4165->$3 = _M0L7_2abindS477;
  _block_4165->$4 = _M0L7_2abindS478;
  _block_4165->$5 = _M0L7_2abindS480;
  _block_4165->$6 = -1;
  return _block_4165;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS482) {
  int32_t _M0L8capacityS481;
  int32_t _M0L7_2abindS483;
  int32_t _M0L7_2abindS484;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2305;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS485;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS486;
  struct _M0TPB3MapGsiE* _block_4166;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS481
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS482);
  _M0L7_2abindS483 = _M0L8capacityS481 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS484 = _M0FPB21calc__grow__threshold(_M0L8capacityS481);
  _M0L6_2atmpS2305 = 0;
  _M0L7_2abindS485
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS481, _M0L6_2atmpS2305);
  _M0L7_2abindS486 = 0;
  _block_4166
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_4166)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 100, 0);
  _block_4166->$0 = _M0L7_2abindS485;
  _block_4166->$1 = 0;
  _block_4166->$2 = _M0L8capacityS481;
  _block_4166->$3 = _M0L7_2abindS483;
  _block_4166->$4 = _M0L7_2abindS484;
  _block_4166->$5 = _M0L7_2abindS486;
  _block_4166->$6 = -1;
  return _block_4166;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS488) {
  int32_t _M0L8capacityS487;
  int32_t _M0L7_2abindS489;
  int32_t _M0L7_2abindS490;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2306;
  struct _M0TPB5EntryGssE** _M0L7_2abindS491;
  struct _M0TPB5EntryGssE* _M0L7_2abindS492;
  struct _M0TPB3MapGssE* _block_4167;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS487
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS488);
  _M0L7_2abindS489 = _M0L8capacityS487 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS490 = _M0FPB21calc__grow__threshold(_M0L8capacityS487);
  _M0L6_2atmpS2306 = 0;
  _M0L7_2abindS491
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS487, _M0L6_2atmpS2306);
  _M0L7_2abindS492 = 0;
  _block_4167
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_4167)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 104, 0);
  _block_4167->$0 = _M0L7_2abindS491;
  _block_4167->$1 = 0;
  _block_4167->$2 = _M0L8capacityS487;
  _block_4167->$3 = _M0L7_2abindS489;
  _block_4167->$4 = _M0L7_2abindS490;
  _block_4167->$5 = _M0L7_2abindS492;
  _block_4167->$6 = -1;
  return _block_4167;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS494) {
  int32_t _M0L8capacityS493;
  int32_t _M0L7_2abindS495;
  int32_t _M0L7_2abindS496;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2307;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS497;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS498;
  struct _M0TPB3MapGsbE* _block_4168;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS493
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS494);
  _M0L7_2abindS495 = _M0L8capacityS493 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS496 = _M0FPB21calc__grow__threshold(_M0L8capacityS493);
  _M0L6_2atmpS2307 = 0;
  _M0L7_2abindS497
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS493, _M0L6_2atmpS2307);
  _M0L7_2abindS498 = 0;
  _block_4168
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_4168)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 108, 0);
  _block_4168->$0 = _M0L7_2abindS497;
  _block_4168->$1 = 0;
  _block_4168->$2 = _M0L8capacityS493;
  _block_4168->$3 = _M0L7_2abindS495;
  _block_4168->$4 = _M0L7_2abindS496;
  _block_4168->$5 = _M0L7_2abindS498;
  _block_4168->$6 = -1;
  return _block_4168;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS500) {
  int32_t _M0L8capacityS499;
  int32_t _M0L7_2abindS501;
  int32_t _M0L7_2abindS502;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2308;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS503;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS504;
  struct _M0TPB3MapGsfE* _block_4169;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS499
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS500);
  _M0L7_2abindS501 = _M0L8capacityS499 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS502 = _M0FPB21calc__grow__threshold(_M0L8capacityS499);
  _M0L6_2atmpS2308 = 0;
  _M0L7_2abindS503
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS499, _M0L6_2atmpS2308);
  _M0L7_2abindS504 = 0;
  _block_4169
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_4169)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 112, 0);
  _block_4169->$0 = _M0L7_2abindS503;
  _block_4169->$1 = 0;
  _block_4169->$2 = _M0L8capacityS499;
  _block_4169->$3 = _M0L7_2abindS501;
  _block_4169->$4 = _M0L7_2abindS502;
  _block_4169->$5 = _M0L7_2abindS504;
  _block_4169->$6 = -1;
  return _block_4169;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS474) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS474 >= 0) {
    int32_t _M0L6_2atmpS2303;
    int32_t _M0L6_2atmpS2302;
    int32_t _M0L6_2atmpS2301;
    int32_t _M0L6_2atmpS2300;
    if (_M0L4selfS474 <= 1) {
      return 1;
    }
    if (_M0L4selfS474 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2303 = _M0L4selfS474 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2302 = moonbit_clz32(_M0L6_2atmpS2303);
    _M0L6_2atmpS2301 = _M0L6_2atmpS2302 - 1;
    _M0L6_2atmpS2300 = 2147483647 >> (_M0L6_2atmpS2301 & 31);
    return _M0L6_2atmpS2300 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS473) {
  int32_t _M0L6_2atmpS2299;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2299 = _M0L8capacityS473 * 13;
  return _M0L6_2atmpS2299 / 16;
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

struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueEE(
  struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L4selfS463
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS463 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2aSomeS464 =
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
  int32_t _M0L3endS2274;
  int32_t _M0L5startS2275;
  int32_t _M0L6_2atmpS2273;
  #line 1497 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2274 = _M0L4selfS435.$2;
  _M0L5startS2275 = _M0L4selfS435.$1;
  _M0L6_2atmpS2273 = _M0L3endS2274 - _M0L5startS2275;
  if (_M0L6_2atmpS2273 == 0) {
    return (moonbit_string_t)moonbit_string_literal_14.data;
  } else {
    moonbit_string_t* _M0L3bufS2297 = _M0L4selfS435.$0;
    int32_t _M0L5startS2298 = _M0L4selfS435.$1;
    moonbit_string_t _M0L5_2ahdS436 =
      (moonbit_string_t)_M0L3bufS2297[_M0L5startS2298];
    moonbit_string_t* _M0L9_2ax__bufS437 = _M0L4selfS435.$0;
    int32_t _M0L5startS2296 = _M0L4selfS435.$1;
    int32_t _M0L11_2ax__startS438 = 1 + _M0L5startS2296;
    int32_t _M0L9_2ax__endS439 = _M0L4selfS435.$2;
    struct _M0TPC16string10StringView _M0L2hdS440;
    int32_t _M0L7_2abindS441;
    int32_t _M0L3endS2294;
    int32_t _M0L5startS2295;
    int32_t _M0L6_2atmpS2293;
    int32_t _M0L10size__hintS442;
    int32_t _M0L2__S443;
    int32_t _M0L10size__hintS444;
    int32_t _M0L10size__hintS449;
    struct _M0TPB13StringBuilder* _M0L3bufS450;
    int32_t _M0L3endS2277;
    int32_t _M0L5startS2278;
    int32_t _M0L6_2atmpS2276;
    moonbit_string_t _result_4173;
    moonbit_incref(_M0L9_2ax__bufS437);
    moonbit_incref(_M0L5_2ahdS436);
    #line 1504 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS440
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS436);
    moonbit_decref(_M0L5_2ahdS436);
    _M0L7_2abindS441 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
    _M0L3endS2294 = _M0L2hdS440.$2;
    _M0L5startS2295 = _M0L2hdS440.$1;
    _M0L6_2atmpS2293 = _M0L3endS2294 - _M0L5startS2295;
    _M0L2__S443 = 0;
    _M0L10size__hintS444 = _M0L6_2atmpS2293;
    while (1) {
      if (_M0L2__S443 < _M0L7_2abindS441) {
        int32_t _M0L6_2atmpS2292 = _M0L11_2ax__startS438 + _M0L2__S443;
        moonbit_string_t _M0L1sS445 =
          (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2292];
        int32_t _M0L6_2atmpS2283 = _M0L2__S443 + 1;
        struct _M0TPC16string10StringView _M0L7_2abindS447;
        int32_t _M0L3endS2290;
        int32_t _M0L5startS2291;
        int32_t _M0L6_2atmpS2289;
        int32_t _M0L6_2atmpS2285;
        int32_t _M0L3endS2287;
        int32_t _M0L5startS2288;
        int32_t _M0L6_2atmpS2286;
        int32_t _M0L6_2atmpS2284;
        moonbit_incref(_M0L1sS445);
        #line 1506 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
        _M0L7_2abindS447
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS445);
        moonbit_decref(_M0L1sS445);
        _M0L3endS2290 = _M0L7_2abindS447.$2;
        _M0L5startS2291 = _M0L7_2abindS447.$1;
        moonbit_decref(_M0L7_2abindS447.$0);
        _M0L6_2atmpS2289 = _M0L3endS2290 - _M0L5startS2291;
        _M0L6_2atmpS2285 = _M0L10size__hintS444 + _M0L6_2atmpS2289;
        _M0L3endS2287 = _M0L9separatorS448.$2;
        _M0L5startS2288 = _M0L9separatorS448.$1;
        _M0L6_2atmpS2286 = _M0L3endS2287 - _M0L5startS2288;
        _M0L6_2atmpS2284 = _M0L6_2atmpS2285 + _M0L6_2atmpS2286;
        _M0L2__S443 = _M0L6_2atmpS2283;
        _M0L10size__hintS444 = _M0L6_2atmpS2284;
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
    _M0L3endS2277 = _M0L9separatorS448.$2;
    _M0L5startS2278 = _M0L9separatorS448.$1;
    _M0L6_2atmpS2276 = _M0L3endS2277 - _M0L5startS2278;
    if (_M0L6_2atmpS2276 == 0) {
      int32_t _M0L7_2abindS451 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
      int32_t _M0L2__S452 = 0;
      while (1) {
        if (_M0L2__S452 < _M0L7_2abindS451) {
          int32_t _M0L6_2atmpS2280 = _M0L11_2ax__startS438 + _M0L2__S452;
          moonbit_string_t _M0L1sS453 =
            (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2280];
          struct _M0TPC16string10StringView _M0L1sS454;
          int32_t _M0L6_2atmpS2279;
          moonbit_incref(_M0L1sS453);
          #line 1517 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS454
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS453);
          moonbit_decref(_M0L1sS453);
          #line 1518 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS450, _M0L1sS454);
          moonbit_decref(_M0L1sS454.$0);
          _M0L6_2atmpS2279 = _M0L2__S452 + 1;
          _M0L2__S452 = _M0L6_2atmpS2279;
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
          int32_t _M0L6_2atmpS2282 = _M0L11_2ax__startS438 + _M0L2__S457;
          moonbit_string_t _M0L1sS458 =
            (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2282];
          struct _M0TPC16string10StringView _M0L1sS459;
          int32_t _M0L6_2atmpS2281;
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
          _M0L6_2atmpS2281 = _M0L2__S457 + 1;
          _M0L2__S457 = _M0L6_2atmpS2281;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS437);
        }
        break;
      }
    }
    #line 1528 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _result_4173 = _M0MPB13StringBuilder10to__string(_M0L3bufS450);
    moonbit_decref(_M0L3bufS450);
    return _result_4173;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS431,
  int32_t _M0L5indexS432
) {
  uint64_t* _M0L6_2atmpS2271;
  uint64_t _result_4174;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS431);
  _M0L6_2atmpS2271 = _M0L4selfS431;
  if (
    _M0L5indexS432 < 0
    || _M0L5indexS432 >= Moonbit_array_length(_M0L6_2atmpS2271)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4174 = (uint64_t)_M0L6_2atmpS2271[_M0L5indexS432];
  moonbit_decref(_M0L6_2atmpS2271);
  return _result_4174;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS433,
  int32_t _M0L5indexS434
) {
  uint32_t* _M0L6_2atmpS2272;
  uint32_t _result_4175;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS433);
  _M0L6_2atmpS2272 = _M0L4selfS433;
  if (
    _M0L5indexS434 < 0
    || _M0L5indexS434 >= Moonbit_array_length(_M0L6_2atmpS2272)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4175 = (uint32_t)_M0L6_2atmpS2272[_M0L5indexS434];
  moonbit_decref(_M0L6_2atmpS2272);
  return _result_4175;
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
    return (moonbit_string_t)moonbit_string_literal_25.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_26.data;
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS427) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS427;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS426
) {
  int32_t _M0L6_2atmpS2270;
  #line 24 "/home/developer/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2270 = Moonbit_array_length(_M0L4selfS426);
  moonbit_incref(_M0L4selfS426);
  return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS426,
                                               .$1 = 0,
                                               .$2 = _M0L6_2atmpS2270};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS417,
  moonbit_string_t _M0L5valueS419
) {
  int32_t _M0L3lenS2255;
  moonbit_string_t* _M0L6_2atmpS2257;
  int32_t _M0L6_2atmpS2256;
  int32_t _M0L6lengthS418;
  moonbit_string_t* _M0L3bufS2258;
  moonbit_string_t _M0L6_2aoldS3831;
  int32_t _M0L6_2atmpS2259;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2255 = _M0L4selfS417->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2257 = _M0MPC15array5Array6bufferGsE(_M0L4selfS417);
  _M0L6_2atmpS2256 = Moonbit_array_length(_M0L6_2atmpS2257);
  moonbit_decref(_M0L6_2atmpS2257);
  if (_M0L3lenS2255 == _M0L6_2atmpS2256) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS417);
  }
  _M0L6lengthS418 = _M0L4selfS417->$1;
  _M0L3bufS2258 = _M0L4selfS417->$0;
  _M0L6_2aoldS3831 = (moonbit_string_t)_M0L3bufS2258[_M0L6lengthS418];
  moonbit_incref(_M0L5valueS419);
  moonbit_decref(_M0L6_2aoldS3831);
  _M0L3bufS2258[_M0L6lengthS418] = _M0L5valueS419;
  _M0L6_2atmpS2259 = _M0L6lengthS418 + 1;
  _M0L4selfS417->$1 = _M0L6_2atmpS2259;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS420,
  struct _M0TUsfE* _M0L5valueS422
) {
  int32_t _M0L3lenS2260;
  struct _M0TUsfE** _M0L6_2atmpS2262;
  int32_t _M0L6_2atmpS2261;
  int32_t _M0L6lengthS421;
  struct _M0TUsfE** _M0L3bufS2263;
  struct _M0TUsfE* _M0L6_2aoldS3833;
  int32_t _M0L6_2atmpS2264;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2260 = _M0L4selfS420->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2262 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS420);
  _M0L6_2atmpS2261 = Moonbit_array_length(_M0L6_2atmpS2262);
  moonbit_decref(_M0L6_2atmpS2262);
  if (_M0L3lenS2260 == _M0L6_2atmpS2261) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS420);
  }
  _M0L6lengthS421 = _M0L4selfS420->$1;
  _M0L3bufS2263 = _M0L4selfS420->$0;
  _M0L6_2aoldS3833 = (struct _M0TUsfE*)_M0L3bufS2263[_M0L6lengthS421];
  moonbit_incref(_M0L5valueS422);
  if (_M0L6_2aoldS3833) {
    moonbit_decref(_M0L6_2aoldS3833);
  }
  _M0L3bufS2263[_M0L6lengthS421] = _M0L5valueS422;
  _M0L6_2atmpS2264 = _M0L6lengthS421 + 1;
  _M0L4selfS420->$1 = _M0L6_2atmpS2264;
  return 0;
}

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS423,
  moonbit_string_t _M0L5valueS425
) {
  int32_t _M0L3lenS2265;
  moonbit_string_t* _M0L6_2atmpS2267;
  int32_t _M0L6_2atmpS2266;
  int32_t _M0L6lengthS424;
  moonbit_string_t* _M0L3bufS2268;
  moonbit_string_t _M0L6_2aoldS3835;
  int32_t _M0L6_2atmpS2269;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2265 = _M0L4selfS423->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2267 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS423);
  _M0L6_2atmpS2266 = Moonbit_array_length(_M0L6_2atmpS2267);
  moonbit_decref(_M0L6_2atmpS2267);
  if (_M0L3lenS2265 == _M0L6_2atmpS2266) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGOsE(_M0L4selfS423);
  }
  _M0L6lengthS424 = _M0L4selfS423->$1;
  _M0L3bufS2268 = _M0L4selfS423->$0;
  _M0L6_2aoldS3835 = (moonbit_string_t)_M0L3bufS2268[_M0L6lengthS424];
  if (_M0L5valueS425) {
    moonbit_incref(_M0L5valueS425);
  }
  if (_M0L6_2aoldS3835) {
    moonbit_decref(_M0L6_2aoldS3835);
  }
  _M0L3bufS2268[_M0L6lengthS424] = _M0L5valueS425;
  _M0L6_2atmpS2269 = _M0L6lengthS424 + 1;
  _M0L4selfS423->$1 = _M0L6_2atmpS2269;
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

int32_t _M0MPC15array5Array7reallocGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS412
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
  _M0MPC15array5Array14resize__bufferGUsfEE(_M0L4selfS412, _M0L8new__capS413);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS415
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
  _M0MPC15array5Array14resize__bufferGOsE(_M0L4selfS415, _M0L8new__capS416);
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
  moonbit_string_t* _M0L6_2aoldS3837;
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
  _M0L6_2aoldS3837 = _M0L4selfS391->$0;
  moonbit_decref(_M0L6_2aoldS3837);
  _M0L4selfS391->$0 = _M0L8new__bufS395;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS397,
  int32_t _M0L13new__capacityS400
) {
  struct _M0TUsfE** _M0L8old__bufS396;
  int32_t _M0L8old__capS398;
  int32_t _M0L9copy__lenS399;
  struct _M0TUsfE** _M0L8new__bufS401;
  struct _M0TUsfE** _M0L6_2aoldS3839;
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
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(_M0L8old__bufS396, _M0L13new__capacityS400, _M0L9copy__lenS399, 0, 0);
  moonbit_decref(_M0L8old__bufS396);
  _M0L6_2aoldS3839 = _M0L4selfS397->$0;
  moonbit_decref(_M0L6_2aoldS3839);
  _M0L4selfS397->$0 = _M0L8new__bufS401;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS403,
  int32_t _M0L13new__capacityS406
) {
  moonbit_string_t* _M0L8old__bufS402;
  int32_t _M0L8old__capS404;
  int32_t _M0L9copy__lenS405;
  moonbit_string_t* _M0L8new__bufS407;
  moonbit_string_t* _M0L6_2aoldS3841;
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
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(_M0L8old__bufS402, _M0L13new__capacityS406, _M0L9copy__lenS405, 0, 0);
  moonbit_decref(_M0L8old__bufS402);
  _M0L6_2aoldS3841 = _M0L4selfS403->$0;
  moonbit_decref(_M0L6_2aoldS3841);
  _M0L4selfS403->$0 = _M0L8new__bufS407;
  return 0;
}

int32_t _M0MPC15array5Array6lengthGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS387
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS387->$1;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS388) {
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
  int32_t _M0L3endS2253;
  int32_t _M0L5startS2254;
  int32_t _M0L8str__lenS383;
  int32_t _M0L3lenS2246;
  int32_t _M0L6_2atmpS2245;
  uint16_t* _M0L4dataS2247;
  int32_t _M0L3lenS2248;
  moonbit_string_t _M0L6_2atmpS2249;
  int32_t _M0L6_2atmpS2250;
  int32_t _M0L3lenS2252;
  int32_t _M0L6_2atmpS2251;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2253 = _M0L3strS384.$2;
  _M0L5startS2254 = _M0L3strS384.$1;
  _M0L8str__lenS383 = _M0L3endS2253 - _M0L5startS2254;
  _M0L3lenS2246 = _M0L4selfS385->$1;
  _M0L6_2atmpS2245 = _M0L3lenS2246 + _M0L8str__lenS383;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS385, _M0L6_2atmpS2245);
  _M0L4dataS2247 = _M0L4selfS385->$0;
  _M0L3lenS2248 = _M0L4selfS385->$1;
  moonbit_incref(_M0L4dataS2247);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2249 = _M0MPC16string10StringView4data(_M0L3strS384);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2250 = _M0MPC16string10StringView13start__offset(_M0L3strS384);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2247, _M0L3lenS2248, _M0L6_2atmpS2249, _M0L6_2atmpS2250, _M0L8str__lenS383);
  moonbit_decref(_M0L4dataS2247);
  moonbit_decref(_M0L6_2atmpS2249);
  _M0L3lenS2252 = _M0L4selfS385->$1;
  _M0L6_2atmpS2251 = _M0L3lenS2252 + _M0L8str__lenS383;
  _M0L4selfS385->$1 = _M0L6_2atmpS2251;
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS380
) {
  moonbit_string_t* _M0L8_2afieldS3844;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3844 = _M0L4selfS380->$0;
  moonbit_incref(_M0L8_2afieldS3844);
  return _M0L8_2afieldS3844;
}

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS381
) {
  moonbit_string_t* _M0L8_2afieldS3845;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3845 = _M0L4selfS381->$0;
  moonbit_incref(_M0L8_2afieldS3845);
  return _M0L8_2afieldS3845;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS382
) {
  struct _M0TUsfE** _M0L8_2afieldS3846;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3846 = _M0L4selfS382->$0;
  moonbit_incref(_M0L8_2afieldS3846);
  return _M0L8_2afieldS3846;
}

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(
  struct _M0TWEOUssE* _M0L1fS364,
  int64_t _M0L10size__hintS361
) {
  int64_t _M0L10size__hintS360;
  struct _M0TPB4IterGUssEE* _block_4176;
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
  _block_4176
  = (struct _M0TPB4IterGUssEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUssEE));
  Moonbit_object_header(_block_4176)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 116, 0);
  _block_4176->$0 = _M0L1fS364;
  _block_4176->$1 = _M0L10size__hintS360;
  return _block_4176;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS369,
  int64_t _M0L10size__hintS366
) {
  int64_t _M0L10size__hintS365;
  struct _M0TPB4IterGUsbEE* _block_4177;
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
  _block_4177
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_4177)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 119, 0);
  _block_4177->$0 = _M0L1fS369;
  _block_4177->$1 = _M0L10size__hintS365;
  return _block_4177;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS374,
  int64_t _M0L10size__hintS371
) {
  int64_t _M0L10size__hintS370;
  struct _M0TPB4IterGUsfEE* _block_4178;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS371 == 4294967296ll) {
    _M0L10size__hintS370 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS372 = _M0L10size__hintS371;
    int32_t _M0L4_2anS373 = (int32_t)_M0L7_2aSomeS372;
    if (_M0L4_2anS373 > 0) {
      _M0L10size__hintS370 = (int64_t)_M0L4_2anS373;
    } else {
      _M0L10size__hintS370 = _M0MPB4Iter3newN6constrS9988GUsfEE;
    }
  }
  moonbit_incref(_M0L1fS374);
  _block_4178
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_4178)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 122, 0);
  _block_4178->$0 = _M0L1fS374;
  _block_4178->$1 = _M0L10size__hintS370;
  return _block_4178;
}

struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0MPB4Iter3newGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE(
  struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L1fS379,
  int64_t _M0L10size__hintS376
) {
  int64_t _M0L10size__hintS375;
  struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _block_4179;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS376 == 4294967296ll) {
    _M0L10size__hintS375 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS377 = _M0L10size__hintS376;
    int32_t _M0L4_2anS378 = (int32_t)_M0L7_2aSomeS377;
    if (_M0L4_2anS378 > 0) {
      _M0L10size__hintS375 = (int64_t)_M0L4_2anS378;
    } else {
      _M0L10size__hintS375
      = _M0MPB4Iter3newN6constrS9988GUsRP39moonbitdb9moonbitdb3lib10RedisValueEE;
    }
  }
  moonbit_incref(_M0L1fS379);
  _block_4179
  = (struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE));
  Moonbit_object_header(_block_4179)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 125, 0);
  _block_4179->$0 = _M0L1fS379;
  _block_4179->$1 = _M0L10size__hintS375;
  return _block_4179;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS352,
  int32_t _M0L5radixS351
) {
  int32_t _if__result_4180;
  uint16_t* _M0L6bufferS353;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS351 < 2) {
    _if__result_4180 = 1;
  } else {
    _if__result_4180 = _M0L5radixS351 > 36;
  }
  if (_if__result_4180) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_27.data);
  }
  if (_M0L4selfS352 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_3.data;
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
  int32_t _M0L6_2atmpS2244;
  uint64_t _M0L3numS327;
  int32_t _M0L6offsetS328;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2244 = _M0L10total__lenS350 - _M0L12digit__startS338;
  _M0L3numS327 = _M0L3numS349;
  _M0L6offsetS328 = _M0L6_2atmpS2244;
  while (1) {
    if (_M0L3numS327 >= 10000ull) {
      uint64_t _M0L1tS329 = _M0L3numS327 / 10000ull;
      uint64_t _M0L6_2atmpS2221 = _M0L3numS327 % 10000ull;
      int32_t _M0L1rS330 = (int32_t)_M0L6_2atmpS2221;
      int32_t _M0L2d1S331 = _M0L1rS330 / 100;
      int32_t _M0L2d2S332 = _M0L1rS330 % 100;
      int32_t _M0L6_2atmpS2220 = _M0L2d1S331 / 10;
      int32_t _M0L6_2atmpS2219 = 48 + _M0L6_2atmpS2220;
      int32_t _M0L6d1__hiS333 = (uint16_t)_M0L6_2atmpS2219;
      int32_t _M0L6_2atmpS2218 = _M0L2d1S331 % 10;
      int32_t _M0L6_2atmpS2217 = 48 + _M0L6_2atmpS2218;
      int32_t _M0L6d1__loS334 = (uint16_t)_M0L6_2atmpS2217;
      int32_t _M0L6_2atmpS2216 = _M0L2d2S332 / 10;
      int32_t _M0L6_2atmpS2215 = 48 + _M0L6_2atmpS2216;
      int32_t _M0L6d2__hiS335 = (uint16_t)_M0L6_2atmpS2215;
      int32_t _M0L6_2atmpS2214 = _M0L2d2S332 % 10;
      int32_t _M0L6_2atmpS2213 = 48 + _M0L6_2atmpS2214;
      int32_t _M0L6d2__loS336 = (uint16_t)_M0L6_2atmpS2213;
      int32_t _M0L6_2atmpS2205 = _M0L12digit__startS338 + _M0L6offsetS328;
      int32_t _M0L6_2atmpS2204 = _M0L6_2atmpS2205 - 4;
      int32_t _M0L6_2atmpS2207;
      int32_t _M0L6_2atmpS2206;
      int32_t _M0L6_2atmpS2209;
      int32_t _M0L6_2atmpS2208;
      int32_t _M0L6_2atmpS2211;
      int32_t _M0L6_2atmpS2210;
      int32_t _M0L6_2atmpS2212;
      _M0L6bufferS337[_M0L6_2atmpS2204] = _M0L6d1__hiS333;
      _M0L6_2atmpS2207 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2206 = _M0L6_2atmpS2207 - 3;
      _M0L6bufferS337[_M0L6_2atmpS2206] = _M0L6d1__loS334;
      _M0L6_2atmpS2209 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2208 = _M0L6_2atmpS2209 - 2;
      _M0L6bufferS337[_M0L6_2atmpS2208] = _M0L6d2__hiS335;
      _M0L6_2atmpS2211 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2210 = _M0L6_2atmpS2211 - 1;
      _M0L6bufferS337[_M0L6_2atmpS2210] = _M0L6d2__loS336;
      _M0L6_2atmpS2212 = _M0L6offsetS328 - 4;
      _M0L3numS327 = _M0L1tS329;
      _M0L6offsetS328 = _M0L6_2atmpS2212;
      continue;
    } else {
      int32_t _M0L6_2atmpS2243 = (int32_t)_M0L3numS327;
      int32_t _M0L9remainingS340 = _M0L6_2atmpS2243;
      int32_t _M0L6offsetS341 = _M0L6offsetS328;
      while (1) {
        if (_M0L9remainingS340 >= 100) {
          int32_t _M0L1tS342 = _M0L9remainingS340 / 100;
          int32_t _M0L1dS343 = _M0L9remainingS340 % 100;
          int32_t _M0L6_2atmpS2230 = _M0L1dS343 / 10;
          int32_t _M0L6_2atmpS2229 = 48 + _M0L6_2atmpS2230;
          int32_t _M0L5d__hiS344 = (uint16_t)_M0L6_2atmpS2229;
          int32_t _M0L6_2atmpS2228 = _M0L1dS343 % 10;
          int32_t _M0L6_2atmpS2227 = 48 + _M0L6_2atmpS2228;
          int32_t _M0L5d__loS345 = (uint16_t)_M0L6_2atmpS2227;
          int32_t _M0L6_2atmpS2223 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2222 = _M0L6_2atmpS2223 - 2;
          int32_t _M0L6_2atmpS2225;
          int32_t _M0L6_2atmpS2224;
          int32_t _M0L6_2atmpS2226;
          _M0L6bufferS337[_M0L6_2atmpS2222] = _M0L5d__hiS344;
          _M0L6_2atmpS2225 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2224 = _M0L6_2atmpS2225 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2224] = _M0L5d__loS345;
          _M0L6_2atmpS2226 = _M0L6offsetS341 - 2;
          _M0L9remainingS340 = _M0L1tS342;
          _M0L6offsetS341 = _M0L6_2atmpS2226;
          continue;
        } else if (_M0L9remainingS340 >= 10) {
          int32_t _M0L6_2atmpS2238 = _M0L9remainingS340 / 10;
          int32_t _M0L6_2atmpS2237 = 48 + _M0L6_2atmpS2238;
          int32_t _M0L5d__hiS347 = (uint16_t)_M0L6_2atmpS2237;
          int32_t _M0L6_2atmpS2236 = _M0L9remainingS340 % 10;
          int32_t _M0L6_2atmpS2235 = 48 + _M0L6_2atmpS2236;
          int32_t _M0L5d__loS348 = (uint16_t)_M0L6_2atmpS2235;
          int32_t _M0L6_2atmpS2232 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2231 = _M0L6_2atmpS2232 - 2;
          int32_t _M0L6_2atmpS2234;
          int32_t _M0L6_2atmpS2233;
          _M0L6bufferS337[_M0L6_2atmpS2231] = _M0L5d__hiS347;
          _M0L6_2atmpS2234 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2233 = _M0L6_2atmpS2234 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2233] = _M0L5d__loS348;
        } else {
          int32_t _M0L6_2atmpS2242 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2239 = _M0L6_2atmpS2242 - 1;
          int32_t _M0L6_2atmpS2241 = 48 + _M0L9remainingS340;
          int32_t _M0L6_2atmpS2240 = (uint16_t)_M0L6_2atmpS2241;
          _M0L6bufferS337[_M0L6_2atmpS2239] = _M0L6_2atmpS2240;
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
  int32_t _M0L6_2atmpS2189;
  int32_t _M0L6_2atmpS2188;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS310 = _M0MPC13int3Int10to__uint64(_M0L5radixS311);
  _M0L6_2atmpS2189 = _M0L5radixS311 - 1;
  _M0L6_2atmpS2188 = _M0L5radixS311 & _M0L6_2atmpS2189;
  if (_M0L6_2atmpS2188 == 0) {
    int32_t _M0L5shiftS312;
    uint64_t _M0L4maskS313;
    int32_t _M0L6_2atmpS2196;
    int32_t _M0L6offsetS314;
    uint64_t _M0L1nS315;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS312 = moonbit_ctz32(_M0L5radixS311);
    _M0L4maskS313 = _M0L4baseS310 - 1ull;
    _M0L6_2atmpS2196 = _M0L10total__lenS320 - _M0L12digit__startS318;
    _M0L6offsetS314 = _M0L6_2atmpS2196;
    _M0L1nS315 = _M0L3numS321;
    while (1) {
      if (_M0L1nS315 > 0ull) {
        uint64_t _M0L6_2atmpS2195 = _M0L1nS315 & _M0L4maskS313;
        int32_t _M0L5digitS316 = (int32_t)_M0L6_2atmpS2195;
        int32_t _M0L6_2atmpS2192 = _M0L12digit__startS318 + _M0L6offsetS314;
        int32_t _M0L6_2atmpS2190 = _M0L6_2atmpS2192 - 1;
        int32_t _M0L6_2atmpS2191 =
          ((moonbit_string_t)moonbit_string_literal_28.data)[_M0L5digitS316];
        int32_t _M0L6_2atmpS2193;
        uint64_t _M0L6_2atmpS2194;
        _M0L6bufferS317[_M0L6_2atmpS2190] = _M0L6_2atmpS2191;
        _M0L6_2atmpS2193 = _M0L6offsetS314 - 1;
        _M0L6_2atmpS2194 = _M0L1nS315 >> (_M0L5shiftS312 & 63);
        _M0L6offsetS314 = _M0L6_2atmpS2193;
        _M0L1nS315 = _M0L6_2atmpS2194;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2203 = _M0L10total__lenS320 - _M0L12digit__startS318;
    int32_t _M0L6offsetS322 = _M0L6_2atmpS2203;
    uint64_t _M0L1nS323 = _M0L3numS321;
    while (1) {
      if (_M0L1nS323 > 0ull) {
        uint64_t _M0L1qS324 = _M0L1nS323 / _M0L4baseS310;
        uint64_t _M0L6_2atmpS2202 = _M0L1qS324 * _M0L4baseS310;
        uint64_t _M0L6_2atmpS2201 = _M0L1nS323 - _M0L6_2atmpS2202;
        int32_t _M0L5digitS325 = (int32_t)_M0L6_2atmpS2201;
        int32_t _M0L6_2atmpS2199 = _M0L12digit__startS318 + _M0L6offsetS322;
        int32_t _M0L6_2atmpS2197 = _M0L6_2atmpS2199 - 1;
        int32_t _M0L6_2atmpS2198 =
          ((moonbit_string_t)moonbit_string_literal_28.data)[_M0L5digitS325];
        int32_t _M0L6_2atmpS2200;
        _M0L6bufferS317[_M0L6_2atmpS2197] = _M0L6_2atmpS2198;
        _M0L6_2atmpS2200 = _M0L6offsetS322 - 1;
        _M0L6offsetS322 = _M0L6_2atmpS2200;
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
  int32_t _M0L6_2atmpS2187;
  int32_t _M0L6offsetS299;
  uint64_t _M0L1nS300;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2187 = _M0L10total__lenS308 - _M0L12digit__startS305;
  _M0L6offsetS299 = _M0L6_2atmpS2187;
  _M0L1nS300 = _M0L3numS309;
  while (1) {
    if (_M0L6offsetS299 >= 2) {
      uint64_t _M0L6_2atmpS2184 = _M0L1nS300 & 255ull;
      int32_t _M0L9byte__valS301 = (int32_t)_M0L6_2atmpS2184;
      int32_t _M0L2hiS302 = _M0L9byte__valS301 / 16;
      int32_t _M0L2loS303 = _M0L9byte__valS301 % 16;
      int32_t _M0L6_2atmpS2178 = _M0L12digit__startS305 + _M0L6offsetS299;
      int32_t _M0L6_2atmpS2176 = _M0L6_2atmpS2178 - 2;
      int32_t _M0L6_2atmpS2177 =
        ((moonbit_string_t)moonbit_string_literal_28.data)[_M0L2hiS302];
      int32_t _M0L6_2atmpS2181;
      int32_t _M0L6_2atmpS2179;
      int32_t _M0L6_2atmpS2180;
      int32_t _M0L6_2atmpS2182;
      uint64_t _M0L6_2atmpS2183;
      _M0L6bufferS304[_M0L6_2atmpS2176] = _M0L6_2atmpS2177;
      _M0L6_2atmpS2181 = _M0L12digit__startS305 + _M0L6offsetS299;
      _M0L6_2atmpS2179 = _M0L6_2atmpS2181 - 1;
      _M0L6_2atmpS2180
      = ((moonbit_string_t)moonbit_string_literal_28.data)[
        _M0L2loS303
      ];
      _M0L6bufferS304[_M0L6_2atmpS2179] = _M0L6_2atmpS2180;
      _M0L6_2atmpS2182 = _M0L6offsetS299 - 2;
      _M0L6_2atmpS2183 = _M0L1nS300 >> 8;
      _M0L6offsetS299 = _M0L6_2atmpS2182;
      _M0L1nS300 = _M0L6_2atmpS2183;
      continue;
    } else if (_M0L6offsetS299 == 1) {
      uint64_t _M0L6_2atmpS2186 = _M0L1nS300 & 15ull;
      int32_t _M0L6nibbleS307 = (int32_t)_M0L6_2atmpS2186;
      int32_t _M0L6_2atmpS2185 =
        ((moonbit_string_t)moonbit_string_literal_28.data)[_M0L6nibbleS307];
      _M0L6bufferS304[_M0L12digit__startS305] = _M0L6_2atmpS2185;
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
      uint64_t _M0L6_2atmpS2174 = _M0L3numS296 / _M0L4baseS294;
      int32_t _M0L6_2atmpS2175 = _M0L5countS297 + 1;
      _M0L3numS296 = _M0L6_2atmpS2174;
      _M0L5countS297 = _M0L6_2atmpS2175;
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
    int32_t _M0L6_2atmpS2173;
    int32_t _M0L6_2atmpS2172;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS292 = moonbit_clz64(_M0L5valueS291);
    _M0L6_2atmpS2173 = 63 - _M0L14leading__zerosS292;
    _M0L6_2atmpS2172 = _M0L6_2atmpS2173 / 4;
    return _M0L6_2atmpS2172 + 1;
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
  int32_t _if__result_4187;
  int32_t _M0L12is__negativeS275;
  uint32_t _M0L3numS276;
  uint16_t* _M0L6bufferS277;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS273 < 2) {
    _if__result_4187 = 1;
  } else {
    _if__result_4187 = _M0L5radixS273 > 36;
  }
  if (_if__result_4187) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_27.data);
  }
  if (_M0L4selfS274 == 0) {
    return (moonbit_string_t)moonbit_string_literal_3.data;
  }
  _M0L12is__negativeS275 = _M0L4selfS274 < 0;
  if (_M0L12is__negativeS275) {
    int32_t _M0L6_2atmpS2171 = -_M0L4selfS274;
    _M0L3numS276 = *(uint32_t*)&_M0L6_2atmpS2171;
  } else {
    _M0L3numS276 = *(uint32_t*)&_M0L4selfS274;
  }
  switch (_M0L5radixS273) {
    case 10: {
      int32_t _M0L10digit__lenS278;
      int32_t _M0L6_2atmpS2168;
      int32_t _M0L10total__lenS279;
      uint16_t* _M0L6bufferS280;
      int32_t _M0L12digit__startS281;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS278 = _M0FPB12dec__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2168 = 1;
      } else {
        _M0L6_2atmpS2168 = 0;
      }
      _M0L10total__lenS279 = _M0L10digit__lenS278 + _M0L6_2atmpS2168;
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
      int32_t _M0L6_2atmpS2169;
      int32_t _M0L10total__lenS283;
      uint16_t* _M0L6bufferS284;
      int32_t _M0L12digit__startS285;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS282 = _M0FPB12hex__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2169 = 1;
      } else {
        _M0L6_2atmpS2169 = 0;
      }
      _M0L10total__lenS283 = _M0L10digit__lenS282 + _M0L6_2atmpS2169;
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
      int32_t _M0L6_2atmpS2170;
      int32_t _M0L10total__lenS287;
      uint16_t* _M0L6bufferS288;
      int32_t _M0L12digit__startS289;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS286
      = _M0FPB14radix__count32(_M0L3numS276, _M0L5radixS273);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2170 = 1;
      } else {
        _M0L6_2atmpS2170 = 0;
      }
      _M0L10total__lenS287 = _M0L10digit__lenS286 + _M0L6_2atmpS2170;
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
      uint32_t _M0L6_2atmpS2166 = _M0L3numS270 / _M0L4baseS268;
      int32_t _M0L6_2atmpS2167 = _M0L5countS271 + 1;
      _M0L3numS270 = _M0L6_2atmpS2166;
      _M0L5countS271 = _M0L6_2atmpS2167;
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
    int32_t _M0L6_2atmpS2165;
    int32_t _M0L6_2atmpS2164;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS266 = moonbit_clz32(_M0L5valueS265);
    _M0L6_2atmpS2165 = 31 - _M0L14leading__zerosS266;
    _M0L6_2atmpS2164 = _M0L6_2atmpS2165 / 4;
    return _M0L6_2atmpS2164 + 1;
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
  int32_t _M0L6_2atmpS2163;
  uint32_t _M0L3numS240;
  int32_t _M0L6offsetS241;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2163 = _M0L10total__lenS263 - _M0L12digit__startS251;
  _M0L3numS240 = _M0L3numS262;
  _M0L6offsetS241 = _M0L6_2atmpS2163;
  while (1) {
    if (_M0L3numS240 >= 10000u) {
      uint32_t _M0L1tS242 = _M0L3numS240 / 10000u;
      uint32_t _M0L6_2atmpS2140 = _M0L3numS240 % 10000u;
      int32_t _M0L1rS243 = *(int32_t*)&_M0L6_2atmpS2140;
      int32_t _M0L2d1S244 = _M0L1rS243 / 100;
      int32_t _M0L2d2S245 = _M0L1rS243 % 100;
      int32_t _M0L6_2atmpS2139 = _M0L2d1S244 / 10;
      int32_t _M0L6_2atmpS2138 = 48 + _M0L6_2atmpS2139;
      int32_t _M0L6d1__hiS246 = (uint16_t)_M0L6_2atmpS2138;
      int32_t _M0L6_2atmpS2137 = _M0L2d1S244 % 10;
      int32_t _M0L6_2atmpS2136 = 48 + _M0L6_2atmpS2137;
      int32_t _M0L6d1__loS247 = (uint16_t)_M0L6_2atmpS2136;
      int32_t _M0L6_2atmpS2135 = _M0L2d2S245 / 10;
      int32_t _M0L6_2atmpS2134 = 48 + _M0L6_2atmpS2135;
      int32_t _M0L6d2__hiS248 = (uint16_t)_M0L6_2atmpS2134;
      int32_t _M0L6_2atmpS2133 = _M0L2d2S245 % 10;
      int32_t _M0L6_2atmpS2132 = 48 + _M0L6_2atmpS2133;
      int32_t _M0L6d2__loS249 = (uint16_t)_M0L6_2atmpS2132;
      int32_t _M0L6_2atmpS2124 = _M0L12digit__startS251 + _M0L6offsetS241;
      int32_t _M0L6_2atmpS2123 = _M0L6_2atmpS2124 - 4;
      int32_t _M0L6_2atmpS2126;
      int32_t _M0L6_2atmpS2125;
      int32_t _M0L6_2atmpS2128;
      int32_t _M0L6_2atmpS2127;
      int32_t _M0L6_2atmpS2130;
      int32_t _M0L6_2atmpS2129;
      int32_t _M0L6_2atmpS2131;
      _M0L6bufferS250[_M0L6_2atmpS2123] = _M0L6d1__hiS246;
      _M0L6_2atmpS2126 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2125 = _M0L6_2atmpS2126 - 3;
      _M0L6bufferS250[_M0L6_2atmpS2125] = _M0L6d1__loS247;
      _M0L6_2atmpS2128 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2127 = _M0L6_2atmpS2128 - 2;
      _M0L6bufferS250[_M0L6_2atmpS2127] = _M0L6d2__hiS248;
      _M0L6_2atmpS2130 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2129 = _M0L6_2atmpS2130 - 1;
      _M0L6bufferS250[_M0L6_2atmpS2129] = _M0L6d2__loS249;
      _M0L6_2atmpS2131 = _M0L6offsetS241 - 4;
      _M0L3numS240 = _M0L1tS242;
      _M0L6offsetS241 = _M0L6_2atmpS2131;
      continue;
    } else {
      int32_t _M0L6_2atmpS2162 = *(int32_t*)&_M0L3numS240;
      int32_t _M0L9remainingS253 = _M0L6_2atmpS2162;
      int32_t _M0L6offsetS254 = _M0L6offsetS241;
      while (1) {
        if (_M0L9remainingS253 >= 100) {
          int32_t _M0L1tS255 = _M0L9remainingS253 / 100;
          int32_t _M0L1dS256 = _M0L9remainingS253 % 100;
          int32_t _M0L6_2atmpS2149 = _M0L1dS256 / 10;
          int32_t _M0L6_2atmpS2148 = 48 + _M0L6_2atmpS2149;
          int32_t _M0L5d__hiS257 = (uint16_t)_M0L6_2atmpS2148;
          int32_t _M0L6_2atmpS2147 = _M0L1dS256 % 10;
          int32_t _M0L6_2atmpS2146 = 48 + _M0L6_2atmpS2147;
          int32_t _M0L5d__loS258 = (uint16_t)_M0L6_2atmpS2146;
          int32_t _M0L6_2atmpS2142 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2141 = _M0L6_2atmpS2142 - 2;
          int32_t _M0L6_2atmpS2144;
          int32_t _M0L6_2atmpS2143;
          int32_t _M0L6_2atmpS2145;
          _M0L6bufferS250[_M0L6_2atmpS2141] = _M0L5d__hiS257;
          _M0L6_2atmpS2144 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2143 = _M0L6_2atmpS2144 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2143] = _M0L5d__loS258;
          _M0L6_2atmpS2145 = _M0L6offsetS254 - 2;
          _M0L9remainingS253 = _M0L1tS255;
          _M0L6offsetS254 = _M0L6_2atmpS2145;
          continue;
        } else if (_M0L9remainingS253 >= 10) {
          int32_t _M0L6_2atmpS2157 = _M0L9remainingS253 / 10;
          int32_t _M0L6_2atmpS2156 = 48 + _M0L6_2atmpS2157;
          int32_t _M0L5d__hiS260 = (uint16_t)_M0L6_2atmpS2156;
          int32_t _M0L6_2atmpS2155 = _M0L9remainingS253 % 10;
          int32_t _M0L6_2atmpS2154 = 48 + _M0L6_2atmpS2155;
          int32_t _M0L5d__loS261 = (uint16_t)_M0L6_2atmpS2154;
          int32_t _M0L6_2atmpS2151 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2150 = _M0L6_2atmpS2151 - 2;
          int32_t _M0L6_2atmpS2153;
          int32_t _M0L6_2atmpS2152;
          _M0L6bufferS250[_M0L6_2atmpS2150] = _M0L5d__hiS260;
          _M0L6_2atmpS2153 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2152 = _M0L6_2atmpS2153 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2152] = _M0L5d__loS261;
        } else {
          int32_t _M0L6_2atmpS2161 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2158 = _M0L6_2atmpS2161 - 1;
          int32_t _M0L6_2atmpS2160 = 48 + _M0L9remainingS253;
          int32_t _M0L6_2atmpS2159 = (uint16_t)_M0L6_2atmpS2160;
          _M0L6bufferS250[_M0L6_2atmpS2158] = _M0L6_2atmpS2159;
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
  int32_t _M0L6_2atmpS2108;
  int32_t _M0L6_2atmpS2107;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS223 = *(uint32_t*)&_M0L5radixS224;
  _M0L6_2atmpS2108 = _M0L5radixS224 - 1;
  _M0L6_2atmpS2107 = _M0L5radixS224 & _M0L6_2atmpS2108;
  if (_M0L6_2atmpS2107 == 0) {
    int32_t _M0L5shiftS225;
    uint32_t _M0L4maskS226;
    int32_t _M0L6_2atmpS2115;
    int32_t _M0L6offsetS227;
    uint32_t _M0L1nS228;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS225 = moonbit_ctz32(_M0L5radixS224);
    _M0L4maskS226 = _M0L4baseS223 - 1u;
    _M0L6_2atmpS2115 = _M0L10total__lenS233 - _M0L12digit__startS231;
    _M0L6offsetS227 = _M0L6_2atmpS2115;
    _M0L1nS228 = _M0L3numS234;
    while (1) {
      if (_M0L1nS228 > 0u) {
        uint32_t _M0L6_2atmpS2114 = _M0L1nS228 & _M0L4maskS226;
        int32_t _M0L5digitS229 = *(int32_t*)&_M0L6_2atmpS2114;
        int32_t _M0L6_2atmpS2111 = _M0L12digit__startS231 + _M0L6offsetS227;
        int32_t _M0L6_2atmpS2109 = _M0L6_2atmpS2111 - 1;
        int32_t _M0L6_2atmpS2110 =
          ((moonbit_string_t)moonbit_string_literal_28.data)[_M0L5digitS229];
        int32_t _M0L6_2atmpS2112;
        uint32_t _M0L6_2atmpS2113;
        _M0L6bufferS230[_M0L6_2atmpS2109] = _M0L6_2atmpS2110;
        _M0L6_2atmpS2112 = _M0L6offsetS227 - 1;
        _M0L6_2atmpS2113 = _M0L1nS228 >> (_M0L5shiftS225 & 31);
        _M0L6offsetS227 = _M0L6_2atmpS2112;
        _M0L1nS228 = _M0L6_2atmpS2113;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2122 = _M0L10total__lenS233 - _M0L12digit__startS231;
    int32_t _M0L6offsetS235 = _M0L6_2atmpS2122;
    uint32_t _M0L1nS236 = _M0L3numS234;
    while (1) {
      if (_M0L1nS236 > 0u) {
        uint32_t _M0L1qS237 = _M0L1nS236 / _M0L4baseS223;
        uint32_t _M0L6_2atmpS2121 = _M0L1qS237 * _M0L4baseS223;
        uint32_t _M0L6_2atmpS2120 = _M0L1nS236 - _M0L6_2atmpS2121;
        int32_t _M0L5digitS238 = *(int32_t*)&_M0L6_2atmpS2120;
        int32_t _M0L6_2atmpS2118 = _M0L12digit__startS231 + _M0L6offsetS235;
        int32_t _M0L6_2atmpS2116 = _M0L6_2atmpS2118 - 1;
        int32_t _M0L6_2atmpS2117 =
          ((moonbit_string_t)moonbit_string_literal_28.data)[_M0L5digitS238];
        int32_t _M0L6_2atmpS2119;
        _M0L6bufferS230[_M0L6_2atmpS2116] = _M0L6_2atmpS2117;
        _M0L6_2atmpS2119 = _M0L6offsetS235 - 1;
        _M0L6offsetS235 = _M0L6_2atmpS2119;
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
  int32_t _M0L6_2atmpS2106;
  int32_t _M0L6offsetS212;
  uint32_t _M0L1nS213;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2106 = _M0L10total__lenS221 - _M0L12digit__startS218;
  _M0L6offsetS212 = _M0L6_2atmpS2106;
  _M0L1nS213 = _M0L3numS222;
  while (1) {
    if (_M0L6offsetS212 >= 2) {
      uint32_t _M0L6_2atmpS2103 = _M0L1nS213 & 255u;
      int32_t _M0L9byte__valS214 = *(int32_t*)&_M0L6_2atmpS2103;
      int32_t _M0L2hiS215 = _M0L9byte__valS214 / 16;
      int32_t _M0L2loS216 = _M0L9byte__valS214 % 16;
      int32_t _M0L6_2atmpS2097 = _M0L12digit__startS218 + _M0L6offsetS212;
      int32_t _M0L6_2atmpS2095 = _M0L6_2atmpS2097 - 2;
      int32_t _M0L6_2atmpS2096 =
        ((moonbit_string_t)moonbit_string_literal_28.data)[_M0L2hiS215];
      int32_t _M0L6_2atmpS2100;
      int32_t _M0L6_2atmpS2098;
      int32_t _M0L6_2atmpS2099;
      int32_t _M0L6_2atmpS2101;
      uint32_t _M0L6_2atmpS2102;
      _M0L6bufferS217[_M0L6_2atmpS2095] = _M0L6_2atmpS2096;
      _M0L6_2atmpS2100 = _M0L12digit__startS218 + _M0L6offsetS212;
      _M0L6_2atmpS2098 = _M0L6_2atmpS2100 - 1;
      _M0L6_2atmpS2099
      = ((moonbit_string_t)moonbit_string_literal_28.data)[
        _M0L2loS216
      ];
      _M0L6bufferS217[_M0L6_2atmpS2098] = _M0L6_2atmpS2099;
      _M0L6_2atmpS2101 = _M0L6offsetS212 - 2;
      _M0L6_2atmpS2102 = _M0L1nS213 >> 8;
      _M0L6offsetS212 = _M0L6_2atmpS2101;
      _M0L1nS213 = _M0L6_2atmpS2102;
      continue;
    } else if (_M0L6offsetS212 == 1) {
      uint32_t _M0L6_2atmpS2105 = _M0L1nS213 & 15u;
      int32_t _M0L6nibbleS220 = *(int32_t*)&_M0L6_2atmpS2105;
      int32_t _M0L6_2atmpS2104 =
        ((moonbit_string_t)moonbit_string_literal_28.data)[_M0L6nibbleS220];
      _M0L6bufferS217[_M0L12digit__startS218] = _M0L6_2atmpS2104;
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
    int64_t _M0L6_2atmpS2087;
    if (_M0L4_2anS193 > 0) {
      int32_t _M0L6_2atmpS2088 = _M0L4_2anS193 - 1;
      _M0L6_2atmpS2087 = (int64_t)_M0L6_2atmpS2088;
    } else {
      _M0L6_2atmpS2087 = _M0MPB4Iter4nextN6constrS9980GUssEE;
    }
    _M0L4selfS189->$1 = _M0L6_2atmpS2087;
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
    int64_t _M0L6_2atmpS2089;
    if (_M0L4_2anS199 > 0) {
      int32_t _M0L6_2atmpS2090 = _M0L4_2anS199 - 1;
      _M0L6_2atmpS2089 = (int64_t)_M0L6_2atmpS2090;
    } else {
      _M0L6_2atmpS2089 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS195->$1 = _M0L6_2atmpS2089;
  }
  return _M0L6resultS196;
}

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS201
) {
  struct _M0TWEOUsfE* _M0L7_2afuncS200;
  struct _M0TUsfE* _M0L6resultS202;
  int64_t _M0L7_2abindS203;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS200 = _M0L4selfS201->$0;
  moonbit_incref(_M0L7_2afuncS200);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS202 = _M0L7_2afuncS200->code(_M0L7_2afuncS200);
  moonbit_decref(_M0L7_2afuncS200);
  _M0L7_2abindS203 = _M0L4selfS201->$1;
  if (_M0L6resultS202 == 0) {
    _M0L4selfS201->$1 = _M0MPB4Iter4nextN6constrS9981GUsfEE;
  } else if (_M0L7_2abindS203 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS204 = _M0L7_2abindS203;
    int32_t _M0L4_2anS205 = (int32_t)_M0L7_2aSomeS204;
    int64_t _M0L6_2atmpS2091;
    if (_M0L4_2anS205 > 0) {
      int32_t _M0L6_2atmpS2092 = _M0L4_2anS205 - 1;
      _M0L6_2atmpS2091 = (int64_t)_M0L6_2atmpS2092;
    } else {
      _M0L6_2atmpS2091 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS201->$1 = _M0L6_2atmpS2091;
  }
  return _M0L6resultS202;
}

struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0MPB4Iter4nextGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE(
  struct _M0TPB4IterGUsRP39moonbitdb9moonbitdb3lib10RedisValueEE* _M0L4selfS207
) {
  struct _M0TWEOUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L7_2afuncS206;
  struct _M0TUsRP39moonbitdb9moonbitdb3lib10RedisValueE* _M0L6resultS208;
  int64_t _M0L7_2abindS209;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS206 = _M0L4selfS207->$0;
  moonbit_incref(_M0L7_2afuncS206);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS208 = _M0L7_2afuncS206->code(_M0L7_2afuncS206);
  moonbit_decref(_M0L7_2afuncS206);
  _M0L7_2abindS209 = _M0L4selfS207->$1;
  if (_M0L6resultS208 == 0) {
    _M0L4selfS207->$1
    = _M0MPB4Iter4nextN6constrS9981GUsRP39moonbitdb9moonbitdb3lib10RedisValueEE;
  } else if (_M0L7_2abindS209 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS210 = _M0L7_2abindS209;
    int32_t _M0L4_2anS211 = (int32_t)_M0L7_2aSomeS210;
    int64_t _M0L6_2atmpS2093;
    if (_M0L4_2anS211 > 0) {
      int32_t _M0L6_2atmpS2094 = _M0L4_2anS211 - 1;
      _M0L6_2atmpS2093 = (int64_t)_M0L6_2atmpS2094;
    } else {
      _M0L6_2atmpS2093
      = _M0MPB4Iter4nextN6constrS9980GUsRP39moonbitdb9moonbitdb3lib10RedisValueEE;
    }
    _M0L4selfS207->$1 = _M0L6_2atmpS2093;
  }
  return _M0L6resultS208;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS179,
  struct _M0TPB6Logger _M0L6loggerS178
) {
  moonbit_string_t _M0L6_2atmpS2082;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2082 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS179);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS178.$0->$method_0(_M0L6loggerS178.$1, _M0L6_2atmpS2082);
  moonbit_decref(_M0L6_2atmpS2082);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS181,
  struct _M0TPB6Logger _M0L6loggerS180
) {
  moonbit_string_t _M0L6_2atmpS2083;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2083 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS181);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS180.$0->$method_0(_M0L6loggerS180.$1, _M0L6_2atmpS2083);
  moonbit_decref(_M0L6_2atmpS2083);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGbE(
  int32_t _M0L4selfS183,
  struct _M0TPB6Logger _M0L6loggerS182
) {
  moonbit_string_t _M0L6_2atmpS2084;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2084 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS183);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS182.$0->$method_0(_M0L6loggerS182.$1, _M0L6_2atmpS2084);
  moonbit_decref(_M0L6_2atmpS2084);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS185,
  struct _M0TPB6Logger _M0L6loggerS184
) {
  moonbit_string_t _M0L6_2atmpS2085;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2085 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS185);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS184.$0->$method_0(_M0L6loggerS184.$1, _M0L6_2atmpS2085);
  moonbit_decref(_M0L6_2atmpS2085);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS187,
  struct _M0TPB6Logger _M0L6loggerS186
) {
  moonbit_string_t _M0L6_2atmpS2086;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2086 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS187);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS186.$0->$method_0(_M0L6loggerS186.$1, _M0L6_2atmpS2086);
  moonbit_decref(_M0L6_2atmpS2086);
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
  moonbit_string_t _M0L8_2afieldS3851;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS3851 = _M0L4selfS176.$0;
  moonbit_incref(_M0L8_2afieldS3851);
  return _M0L8_2afieldS3851;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS172,
  moonbit_string_t _M0L5valueS173,
  int32_t _M0L5startS174,
  int32_t _M0L3lenS175
) {
  int32_t _M0L6_2atmpS2081;
  int64_t _M0L6_2atmpS2080;
  struct _M0TPC16string10StringView _M0L6_2atmpS2079;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2081 = _M0L5startS174 + _M0L3lenS175;
  _M0L6_2atmpS2080 = (int64_t)_M0L6_2atmpS2081;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2079
  = _M0MPC16string6String11sub_2einner(_M0L5valueS173, _M0L5startS174, _M0L6_2atmpS2080);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS172, _M0L6_2atmpS2079);
  moonbit_decref(_M0L6_2atmpS2079.$0);
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
  int32_t _if__result_4194;
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
      _if__result_4194 = _M0L3endS166 <= _M0L3lenS164;
    } else {
      _if__result_4194 = 0;
    }
  } else {
    _if__result_4194 = 0;
  }
  if (_if__result_4194) {
    if (_M0L5startS170 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2076 = _M0L4selfS165[_M0L5startS170];
      int32_t _M0L6_2atmpS2075;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2075
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2076);
      if (!_M0L6_2atmpS2075) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS166 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2078 = _M0L4selfS165[_M0L3endS166];
      int32_t _M0L6_2atmpS2077;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2077
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2078);
      if (!_M0L6_2atmpS2077) {
        
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
  struct _M0TPB6Logger _M0L6_2atmpS2074;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS163);
  _M0L6_2atmpS2074
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS163
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS162.$0->$method_0(_M0L4showS162.$1, _M0L6_2atmpS2074);
  if (_M0L6_2atmpS2074.$1) {
    moonbit_decref(_M0L6_2atmpS2074.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS161,
  struct _M0TPB4Show _M0L4showS160
) {
  struct _M0TPB6Logger _M0L6_2atmpS2073;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS161);
  _M0L6_2atmpS2073
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS161
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS160.$0->$method_0(_M0L4showS160.$1, _M0L6_2atmpS2073);
  if (_M0L6_2atmpS2073.$1) {
    moonbit_decref(_M0L6_2atmpS2073.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS159) {
  uint32_t _M0L6_2atmpS2072;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2072 = _M0FPB14avalanche__acc(_M0L3accS159);
  return *(int32_t*)&_M0L6_2atmpS2072;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS158) {
  uint32_t _M0Lm3accS157;
  uint32_t _M0L6_2atmpS2061;
  uint32_t _M0L6_2atmpS2063;
  uint32_t _M0L6_2atmpS2062;
  uint32_t _M0L6_2atmpS2064;
  uint32_t _M0L6_2atmpS2065;
  uint32_t _M0L6_2atmpS2067;
  uint32_t _M0L6_2atmpS2066;
  uint32_t _M0L6_2atmpS2068;
  uint32_t _M0L6_2atmpS2069;
  uint32_t _M0L6_2atmpS2071;
  uint32_t _M0L6_2atmpS2070;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS157 = _M0L3accS158;
  _M0L6_2atmpS2061 = _M0Lm3accS157;
  _M0L6_2atmpS2063 = _M0Lm3accS157;
  _M0L6_2atmpS2062 = _M0L6_2atmpS2063 >> 15;
  _M0Lm3accS157 = _M0L6_2atmpS2061 ^ _M0L6_2atmpS2062;
  _M0L6_2atmpS2064 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2064 * 2246822519u;
  _M0L6_2atmpS2065 = _M0Lm3accS157;
  _M0L6_2atmpS2067 = _M0Lm3accS157;
  _M0L6_2atmpS2066 = _M0L6_2atmpS2067 >> 13;
  _M0Lm3accS157 = _M0L6_2atmpS2065 ^ _M0L6_2atmpS2066;
  _M0L6_2atmpS2068 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2068 * 3266489917u;
  _M0L6_2atmpS2069 = _M0Lm3accS157;
  _M0L6_2atmpS2071 = _M0Lm3accS157;
  _M0L6_2atmpS2070 = _M0L6_2atmpS2071 >> 16;
  _M0Lm3accS157 = _M0L6_2atmpS2069 ^ _M0L6_2atmpS2070;
  return _M0Lm3accS157;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS156) {
  int64_t _M0L6_2atmpS2060;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2060 = (int64_t)_M0L4selfS156;
  return *(uint64_t*)&_M0L6_2atmpS2060;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS155,
  moonbit_string_t _M0L3strS154
) {
  int32_t _M0L8str__lenS153;
  int32_t _M0L3lenS2055;
  int32_t _M0L6_2atmpS2054;
  uint16_t* _M0L4dataS2056;
  int32_t _M0L3lenS2057;
  int32_t _M0L3lenS2059;
  int32_t _M0L6_2atmpS2058;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS153 = Moonbit_array_length(_M0L3strS154);
  _M0L3lenS2055 = _M0L4selfS155->$1;
  _M0L6_2atmpS2054 = _M0L3lenS2055 + _M0L8str__lenS153;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS155, _M0L6_2atmpS2054);
  _M0L4dataS2056 = _M0L4selfS155->$0;
  _M0L3lenS2057 = _M0L4selfS155->$1;
  moonbit_incref(_M0L4dataS2056);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2056, _M0L3lenS2057, _M0L3strS154, 0, _M0L8str__lenS153);
  moonbit_decref(_M0L4dataS2056);
  _M0L3lenS2059 = _M0L4selfS155->$1;
  _M0L6_2atmpS2058 = _M0L3lenS2059 + _M0L8str__lenS153;
  _M0L4selfS155->$1 = _M0L6_2atmpS2058;
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
      int32_t _M0L6_2atmpS2051 = _M0L3strS150[_M0L1iS147];
      int32_t _M0L6_2atmpS2052;
      int32_t _M0L6_2atmpS2053;
      if (
        _M0L1jS148 < 0 || _M0L1jS148 >= Moonbit_array_length(_M0L4selfS149)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS149[_M0L1jS148] = _M0L6_2atmpS2051;
      _M0L6_2atmpS2052 = _M0L1iS147 + 1;
      _M0L6_2atmpS2053 = _M0L1jS148 + 1;
      _M0L1iS147 = _M0L6_2atmpS2052;
      _M0L1jS148 = _M0L6_2atmpS2053;
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
    int32_t _M0L3lenS2030 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2029 = _M0L3lenS2030 + 1;
    uint16_t* _M0L4dataS2031;
    int32_t _M0L3lenS2032;
    int32_t _M0L6_2atmpS2033;
    int32_t _M0L3lenS2035;
    int32_t _M0L6_2atmpS2034;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2029);
    _M0L4dataS2031 = _M0L4selfS141->$0;
    _M0L3lenS2032 = _M0L4selfS141->$1;
    moonbit_incref(_M0L4dataS2031);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2033 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS139);
    if (
      _M0L3lenS2032 < 0
      || _M0L3lenS2032 >= Moonbit_array_length(_M0L4dataS2031)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2031[_M0L3lenS2032] = _M0L6_2atmpS2033;
    moonbit_decref(_M0L4dataS2031);
    _M0L3lenS2035 = _M0L4selfS141->$1;
    _M0L6_2atmpS2034 = _M0L3lenS2035 + 1;
    _M0L4selfS141->$1 = _M0L6_2atmpS2034;
  } else if (_M0L4codeS139 <= 1114111u) {
    int32_t _M0L3lenS2037 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2036 = _M0L3lenS2037 + 2;
    uint32_t _M0L4codeS142;
    uint16_t* _M0L4dataS2038;
    int32_t _M0L3lenS2039;
    uint32_t _M0L6_2atmpS2042;
    uint32_t _M0L6_2atmpS2041;
    int32_t _M0L6_2atmpS2040;
    uint16_t* _M0L4dataS2043;
    int32_t _M0L3lenS2048;
    int32_t _M0L6_2atmpS2044;
    uint32_t _M0L6_2atmpS2047;
    uint32_t _M0L6_2atmpS2046;
    int32_t _M0L6_2atmpS2045;
    int32_t _M0L3lenS2050;
    int32_t _M0L6_2atmpS2049;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2036);
    _M0L4codeS142 = _M0L4codeS139 - 65536u;
    _M0L4dataS2038 = _M0L4selfS141->$0;
    _M0L3lenS2039 = _M0L4selfS141->$1;
    _M0L6_2atmpS2042 = _M0L4codeS142 >> 10;
    _M0L6_2atmpS2041 = 55296u + _M0L6_2atmpS2042;
    moonbit_incref(_M0L4dataS2038);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2040 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2041);
    if (
      _M0L3lenS2039 < 0
      || _M0L3lenS2039 >= Moonbit_array_length(_M0L4dataS2038)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2038[_M0L3lenS2039] = _M0L6_2atmpS2040;
    moonbit_decref(_M0L4dataS2038);
    _M0L4dataS2043 = _M0L4selfS141->$0;
    _M0L3lenS2048 = _M0L4selfS141->$1;
    _M0L6_2atmpS2044 = _M0L3lenS2048 + 1;
    _M0L6_2atmpS2047 = _M0L4codeS142 & 1023u;
    _M0L6_2atmpS2046 = 56320u + _M0L6_2atmpS2047;
    moonbit_incref(_M0L4dataS2043);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2045 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2046);
    if (
      _M0L6_2atmpS2044 < 0
      || _M0L6_2atmpS2044 >= Moonbit_array_length(_M0L4dataS2043)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2043[_M0L6_2atmpS2044] = _M0L6_2atmpS2045;
    moonbit_decref(_M0L4dataS2043);
    _M0L3lenS2050 = _M0L4selfS141->$1;
    _M0L6_2atmpS2049 = _M0L3lenS2050 + 2;
    _M0L4selfS141->$1 = _M0L6_2atmpS2049;
  } else {
    #line 108 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_29.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS133,
  int32_t _M0L8requiredS134
) {
  uint16_t* _M0L4dataS2028;
  int32_t _M0L12current__lenS132;
  int32_t _M0L13enough__spaceS135;
  int32_t _M0L13enough__spaceS136;
  uint16_t* _M0L4dataS2024;
  int32_t _M0L6_2atmpS2025;
  int32_t _M0L3lenS2026;
  uint16_t* _M0L9new__dataS138;
  uint16_t* _M0L6_2aoldS3856;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS2028 = _M0L4selfS133->$0;
  _M0L12current__lenS132 = Moonbit_array_length(_M0L4dataS2028);
  if (_M0L8requiredS134 <= _M0L12current__lenS132) {
    return 0;
  }
  _M0L13enough__spaceS136 = _M0L12current__lenS132;
  while (1) {
    if (_M0L13enough__spaceS136 < _M0L8requiredS134) {
      int32_t _M0L6_2atmpS2027 = _M0L13enough__spaceS136 * 2;
      _M0L13enough__spaceS136 = _M0L6_2atmpS2027;
      continue;
    } else {
      _M0L13enough__spaceS135 = _M0L13enough__spaceS136;
    }
    break;
  }
  _M0L4dataS2024 = _M0L4selfS133->$0;
  moonbit_incref(_M0L4dataS2024);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2025 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS2026 = _M0L4selfS133->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS138
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2024, _M0L13enough__spaceS135, _M0L6_2atmpS2025, _M0L3lenS2026, 0, 0);
  moonbit_decref(_M0L4dataS2024);
  _M0L6_2aoldS3856 = _M0L4selfS133->$0;
  moonbit_decref(_M0L6_2aoldS3856);
  _M0L4selfS133->$0 = _M0L9new__dataS138;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS131) {
  int32_t _M0L6_2atmpS2023;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2023 = *(int32_t*)&_M0L4selfS131;
  return (uint16_t)_M0L6_2atmpS2023;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS130) {
  int32_t _M0L6_2atmpS2022;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2022 = _M0L4selfS130;
  return *(uint32_t*)&_M0L6_2atmpS2022;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS128
) {
  int32_t _M0L3lenS2014;
  uint16_t* _M0L4dataS2016;
  int32_t _M0L6_2atmpS2015;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS2014 = _M0L4selfS128->$1;
  _M0L4dataS2016 = _M0L4selfS128->$0;
  _M0L6_2atmpS2015 = Moonbit_array_length(_M0L4dataS2016);
  if (_M0L3lenS2014 == _M0L6_2atmpS2015) {
    uint16_t* _M0L4dataS2017 = _M0L4selfS128->$0;
    moonbit_incref(_M0L4dataS2017);
    return _M0L4dataS2017;
  } else {
    uint16_t* _M0L4dataS2018 = _M0L4selfS128->$0;
    int32_t _M0L3lenS2019 = _M0L4selfS128->$1;
    int32_t _M0L6_2atmpS2020;
    int32_t _M0L3lenS2021;
    uint16_t* _M0L4dataS129;
    moonbit_incref(_M0L4dataS2018);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2020 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS2021 = _M0L4selfS128->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS129
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2018, _M0L3lenS2019, _M0L6_2atmpS2020, _M0L3lenS2021, 0, 0);
    moonbit_decref(_M0L4dataS2018);
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
  int32_t _if__result_4197;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS121 >= 0) {
    if (_M0L3lenS122 >= 0) {
      if (_M0L11src__offsetS123 >= 0) {
        if (_M0L11dst__offsetS124 >= 0) {
          int32_t _M0L6_2atmpS2010 = _M0L11src__offsetS123 + _M0L3lenS122;
          int32_t _M0L6_2atmpS2011 = Moonbit_array_length(_M0L3srcS125);
          if (_M0L6_2atmpS2010 <= _M0L6_2atmpS2011) {
            int32_t _M0L6_2atmpS2009 = _M0L11dst__offsetS124 + _M0L3lenS122;
            _if__result_4197 = _M0L6_2atmpS2009 <= _M0L13allocate__lenS121;
          } else {
            _if__result_4197 = 0;
          }
        } else {
          _if__result_4197 = 0;
        }
      } else {
        _if__result_4197 = 0;
      }
    } else {
      _if__result_4197 = 0;
    }
  } else {
    _if__result_4197 = 0;
  }
  if (_if__result_4197) {
    moonbit_incref(_M0L3srcS125);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS125, _M0L13allocate__lenS121, _M0L4initS126, _M0L11src__offsetS123, _M0L11dst__offsetS124, _M0L3lenS122);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS127;
    int32_t _M0L6_2atmpS2013;
    moonbit_string_t _M0L6_2atmpS2012;
    uint16_t* _result_4198;
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L18_2astring__builderS127
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_30.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L13allocate__lenS121);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_31.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11src__offsetS123);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_32.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11dst__offsetS124);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_33.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L3lenS122);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_34.data);
    _M0L6_2atmpS2013 = Moonbit_array_length(_M0L3srcS125);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L6_2atmpS2013);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS2012
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS127);
    moonbit_decref(_M0L18_2astring__builderS127);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_4198 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS2012);
    moonbit_decref(_M0L6_2atmpS2012);
    return _result_4198;
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
  struct _M0TPB13StringBuilder* _block_4199;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS112 < 1) {
    _M0L7initialS111 = 1;
  } else {
    int32_t _M0L6_2atmpS2008 = _M0L10size__hintS112 + 1;
    _M0L7initialS111 = _M0L6_2atmpS2008 / 2;
  }
  _M0L4dataS113 = (uint16_t*)moonbit_make_string(_M0L7initialS111, 0);
  _block_4199
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4199)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 128, 0);
  _block_4199->$0 = _M0L4dataS113;
  _block_4199->$1 = 0;
  return _block_4199;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS97,
  int32_t _M0L13allocate__lenS93,
  int32_t _M0L3lenS94,
  int32_t _M0L11src__offsetS95,
  int32_t _M0L11dst__offsetS96
) {
  int32_t _if__result_4200;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS93 >= 0) {
    if (_M0L3lenS94 >= 0) {
      if (_M0L11src__offsetS95 >= 0) {
        if (_M0L11dst__offsetS96 >= 0) {
          int32_t _M0L6_2atmpS1994 = _M0L11src__offsetS95 + _M0L3lenS94;
          int32_t _M0L6_2atmpS1995;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS1995
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
          if (_M0L6_2atmpS1994 <= _M0L6_2atmpS1995) {
            int32_t _M0L6_2atmpS1993 = _M0L11dst__offsetS96 + _M0L3lenS94;
            _if__result_4200 = _M0L6_2atmpS1993 <= _M0L13allocate__lenS93;
          } else {
            _if__result_4200 = 0;
          }
        } else {
          _if__result_4200 = 0;
        }
      } else {
        _if__result_4200 = 0;
      }
    } else {
      _if__result_4200 = 0;
    }
  } else {
    _if__result_4200 = 0;
  }
  if (_if__result_4200) {
    moonbit_incref(_M0L3srcS97);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS93, (moonbit_string_t)moonbit_string_literal_14.data, _M0L3srcS97, _M0L11src__offsetS95, _M0L11dst__offsetS96, _M0L3lenS94);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS98;
    int32_t _M0L6_2atmpS1997;
    moonbit_string_t _M0L6_2atmpS1996;
    moonbit_string_t* _result_4201;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS98
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_30.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L13allocate__lenS93);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_31.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11src__offsetS95);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_32.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11dst__offsetS96);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_33.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L3lenS94);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_34.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1997 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L6_2atmpS1997);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS1996
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS98);
    moonbit_decref(_M0L18_2astring__builderS98);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4201
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS1996);
    moonbit_decref(_M0L6_2atmpS1996);
    return _result_4201;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS103,
  int32_t _M0L13allocate__lenS99,
  int32_t _M0L3lenS100,
  int32_t _M0L11src__offsetS101,
  int32_t _M0L11dst__offsetS102
) {
  int32_t _if__result_4202;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS99 >= 0) {
    if (_M0L3lenS100 >= 0) {
      if (_M0L11src__offsetS101 >= 0) {
        if (_M0L11dst__offsetS102 >= 0) {
          int32_t _M0L6_2atmpS1999 = _M0L11src__offsetS101 + _M0L3lenS100;
          int32_t _M0L6_2atmpS2000;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2000
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS103);
          if (_M0L6_2atmpS1999 <= _M0L6_2atmpS2000) {
            int32_t _M0L6_2atmpS1998 = _M0L11dst__offsetS102 + _M0L3lenS100;
            _if__result_4202 = _M0L6_2atmpS1998 <= _M0L13allocate__lenS99;
          } else {
            _if__result_4202 = 0;
          }
        } else {
          _if__result_4202 = 0;
        }
      } else {
        _if__result_4202 = 0;
      }
    } else {
      _if__result_4202 = 0;
    }
  } else {
    _if__result_4202 = 0;
  }
  if (_if__result_4202) {
    moonbit_incref(_M0L3srcS103);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS99, 0, _M0L3srcS103, _M0L11src__offsetS101, _M0L11dst__offsetS102, _M0L3lenS100);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS104;
    int32_t _M0L6_2atmpS2002;
    moonbit_string_t _M0L6_2atmpS2001;
    struct _M0TUsfE** _result_4203;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS104
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_30.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L13allocate__lenS99);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_31.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11src__offsetS101);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_32.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11dst__offsetS102);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_33.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L3lenS100);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_34.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2002 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS103);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L6_2atmpS2002);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2001
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS104);
    moonbit_decref(_M0L18_2astring__builderS104);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4203
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS2001);
    moonbit_decref(_M0L6_2atmpS2001);
    return _result_4203;
  }
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t* _M0L3srcS109,
  int32_t _M0L13allocate__lenS105,
  int32_t _M0L3lenS106,
  int32_t _M0L11src__offsetS107,
  int32_t _M0L11dst__offsetS108
) {
  int32_t _if__result_4204;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS105 >= 0) {
    if (_M0L3lenS106 >= 0) {
      if (_M0L11src__offsetS107 >= 0) {
        if (_M0L11dst__offsetS108 >= 0) {
          int32_t _M0L6_2atmpS2004 = _M0L11src__offsetS107 + _M0L3lenS106;
          int32_t _M0L6_2atmpS2005;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2005
          = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS109);
          if (_M0L6_2atmpS2004 <= _M0L6_2atmpS2005) {
            int32_t _M0L6_2atmpS2003 = _M0L11dst__offsetS108 + _M0L3lenS106;
            _if__result_4204 = _M0L6_2atmpS2003 <= _M0L13allocate__lenS105;
          } else {
            _if__result_4204 = 0;
          }
        } else {
          _if__result_4204 = 0;
        }
      } else {
        _if__result_4204 = 0;
      }
    } else {
      _if__result_4204 = 0;
    }
  } else {
    _if__result_4204 = 0;
  }
  if (_if__result_4204) {
    moonbit_incref(_M0L3srcS109);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS105, 0, _M0L3srcS109, _M0L11src__offsetS107, _M0L11dst__offsetS108, _M0L3lenS106);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS110;
    int32_t _M0L6_2atmpS2007;
    moonbit_string_t _M0L6_2atmpS2006;
    moonbit_string_t* _result_4205;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS110
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_30.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L13allocate__lenS105);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_31.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11src__offsetS107);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_32.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11dst__offsetS108);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_33.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L3lenS106);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_34.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2007 = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS109);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L6_2atmpS2007);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2006
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS110);
    moonbit_decref(_M0L18_2astring__builderS110);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4205
    = _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(_M0L6_2atmpS2006);
    moonbit_decref(_M0L6_2atmpS2006);
    return _result_4205;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS84,
  moonbit_string_t _M0L3objS83
) {
  struct _M0TPB6Logger _M0L6_2atmpS1988;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS84);
  _M0L6_2atmpS1988
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS84
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS83, _M0L6_2atmpS1988);
  if (_M0L6_2atmpS1988.$1) {
    moonbit_decref(_M0L6_2atmpS1988.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS86,
  int32_t _M0L3objS85
) {
  struct _M0TPB6Logger _M0L6_2atmpS1989;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS86);
  _M0L6_2atmpS1989
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS86
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS85, _M0L6_2atmpS1989);
  if (_M0L6_2atmpS1989.$1) {
    moonbit_decref(_M0L6_2atmpS1989.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder* _M0L4selfS88,
  int32_t _M0L3objS87
) {
  struct _M0TPB6Logger _M0L6_2atmpS1990;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS88);
  _M0L6_2atmpS1990
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS88
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGbE(_M0L3objS87, _M0L6_2atmpS1990);
  if (_M0L6_2atmpS1990.$1) {
    moonbit_decref(_M0L6_2atmpS1990.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS90,
  float _M0L3objS89
) {
  struct _M0TPB6Logger _M0L6_2atmpS1991;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS90);
  _M0L6_2atmpS1991
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS90
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS89, _M0L6_2atmpS1991);
  if (_M0L6_2atmpS1991.$1) {
    moonbit_decref(_M0L6_2atmpS1991.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS92,
  uint64_t _M0L3objS91
) {
  struct _M0TPB6Logger _M0L6_2atmpS1992;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS92);
  _M0L6_2atmpS1992
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS92
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS91, _M0L6_2atmpS1992);
  if (_M0L6_2atmpS1992.$1) {
    moonbit_decref(_M0L6_2atmpS1992.$1);
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
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS66, (moonbit_string_t)moonbit_string_literal_14.data);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L3dstS65, _M0L11dst__offsetS67, _M0L3srcS68, _M0L11src__offsetS69, _M0L9blit__lenS70);
  moonbit_decref(_M0L3srcS68);
  return _M0L3dstS65;
}

struct _M0TUsfE** _M0MPB18UninitializedArray23unsafe__make__and__blitGUsfEE(
  struct _M0TUsfE** _M0L3srcS74,
  int32_t _M0L13allocate__lenS72,
  int32_t _M0L11src__offsetS75,
  int32_t _M0L11dst__offsetS73,
  int32_t _M0L9blit__lenS76
) {
  struct _M0TUsfE** _M0L3dstS71;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS71
  = (struct _M0TUsfE**)moonbit_make_ref_array(_M0L13allocate__lenS72, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsfEE(_M0L3dstS71, _M0L11dst__offsetS73, _M0L3srcS74, _M0L11src__offsetS75, _M0L9blit__lenS76);
  moonbit_decref(_M0L3srcS74);
  return _M0L3dstS71;
}

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGOsE(
  moonbit_string_t* _M0L3srcS80,
  int32_t _M0L13allocate__lenS78,
  int32_t _M0L11src__offsetS81,
  int32_t _M0L11dst__offsetS79,
  int32_t _M0L9blit__lenS82
) {
  moonbit_string_t* _M0L3dstS77;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS77
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS78, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGOsE(_M0L3dstS77, _M0L11dst__offsetS79, _M0L3srcS80, _M0L11src__offsetS81, _M0L9blit__lenS82);
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsfEE(
  struct _M0TUsfE** _M0L3dstS55,
  int32_t _M0L11dst__offsetS56,
  struct _M0TUsfE** _M0L3srcS57,
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGOsE(
  moonbit_string_t* _M0L3dstS60,
  int32_t _M0L11dst__offsetS61,
  moonbit_string_t* _M0L3srcS62,
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
  int32_t _if__result_4206;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS14 == _M0L3srcS15) {
    _if__result_4206 = _M0L11dst__offsetS16 < _M0L11src__offsetS17;
  } else {
    _if__result_4206 = 0;
  }
  if (_if__result_4206) {
    int32_t _M0L1iS18 = 0;
    while (1) {
      if (_M0L1iS18 < _M0L3lenS19) {
        int32_t _M0L6_2atmpS1952 = _M0L11dst__offsetS16 + _M0L1iS18;
        int32_t _M0L6_2atmpS1954 = _M0L11src__offsetS17 + _M0L1iS18;
        int32_t _M0L6_2atmpS1953;
        int32_t _M0L6_2atmpS1955;
        if (
          _M0L6_2atmpS1954 < 0
          || _M0L6_2atmpS1954 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1953 = (int32_t)_M0L3srcS15[_M0L6_2atmpS1954];
        if (
          _M0L6_2atmpS1952 < 0
          || _M0L6_2atmpS1952 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS1952] = _M0L6_2atmpS1953;
        _M0L6_2atmpS1955 = _M0L1iS18 + 1;
        _M0L1iS18 = _M0L6_2atmpS1955;
        continue;
      } else {
        moonbit_decref(_M0L3srcS15);
        moonbit_decref(_M0L3dstS14);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1960 = _M0L3lenS19 - 1;
    int32_t _M0L1iS21 = _M0L6_2atmpS1960;
    while (1) {
      if (_M0L1iS21 >= 0) {
        int32_t _M0L6_2atmpS1956 = _M0L11dst__offsetS16 + _M0L1iS21;
        int32_t _M0L6_2atmpS1958 = _M0L11src__offsetS17 + _M0L1iS21;
        int32_t _M0L6_2atmpS1957;
        int32_t _M0L6_2atmpS1959;
        if (
          _M0L6_2atmpS1958 < 0
          || _M0L6_2atmpS1958 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1957 = (int32_t)_M0L3srcS15[_M0L6_2atmpS1958];
        if (
          _M0L6_2atmpS1956 < 0
          || _M0L6_2atmpS1956 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS1956] = _M0L6_2atmpS1957;
        _M0L6_2atmpS1959 = _M0L1iS21 - 1;
        _M0L1iS21 = _M0L6_2atmpS1959;
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
  int32_t _if__result_4209;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_4209 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_4209 = 0;
  }
  if (_if__result_4209) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS1961 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS1963 = _M0L11src__offsetS26 + _M0L1iS27;
        moonbit_string_t _M0L6_2atmpS1962;
        moonbit_string_t _M0L6_2aoldS3862;
        int32_t _M0L6_2atmpS1964;
        if (
          _M0L6_2atmpS1963 < 0
          || _M0L6_2atmpS1963 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1962 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS1963];
        if (
          _M0L6_2atmpS1961 < 0
          || _M0L6_2atmpS1961 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3862 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS1961];
        moonbit_incref(_M0L6_2atmpS1962);
        moonbit_decref(_M0L6_2aoldS3862);
        _M0L3dstS23[_M0L6_2atmpS1961] = _M0L6_2atmpS1962;
        _M0L6_2atmpS1964 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS1964;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1969 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS1969;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS1965 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS1967 = _M0L11src__offsetS26 + _M0L1iS30;
        moonbit_string_t _M0L6_2atmpS1966;
        moonbit_string_t _M0L6_2aoldS3864;
        int32_t _M0L6_2atmpS1968;
        if (
          _M0L6_2atmpS1967 < 0
          || _M0L6_2atmpS1967 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1966 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS1967];
        if (
          _M0L6_2atmpS1965 < 0
          || _M0L6_2atmpS1965 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3864 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS1965];
        moonbit_incref(_M0L6_2atmpS1966);
        moonbit_decref(_M0L6_2aoldS3864);
        _M0L3dstS23[_M0L6_2atmpS1965] = _M0L6_2atmpS1966;
        _M0L6_2atmpS1968 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS1968;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsfEEE(
  struct _M0TUsfE** _M0L3dstS32,
  int32_t _M0L11dst__offsetS34,
  struct _M0TUsfE** _M0L3srcS33,
  int32_t _M0L11src__offsetS35,
  int32_t _M0L3lenS37
) {
  int32_t _if__result_4212;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_4212 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_4212 = 0;
  }
  if (_if__result_4212) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS1970 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS1972 = _M0L11src__offsetS35 + _M0L1iS36;
        struct _M0TUsfE* _M0L6_2atmpS1971;
        struct _M0TUsfE* _M0L6_2aoldS3866;
        int32_t _M0L6_2atmpS1973;
        if (
          _M0L6_2atmpS1972 < 0
          || _M0L6_2atmpS1972 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1971 = (struct _M0TUsfE*)_M0L3srcS33[_M0L6_2atmpS1972];
        if (
          _M0L6_2atmpS1970 < 0
          || _M0L6_2atmpS1970 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3866 = (struct _M0TUsfE*)_M0L3dstS32[_M0L6_2atmpS1970];
        if (_M0L6_2atmpS1971) {
          moonbit_incref(_M0L6_2atmpS1971);
        }
        if (_M0L6_2aoldS3866) {
          moonbit_decref(_M0L6_2aoldS3866);
        }
        _M0L3dstS32[_M0L6_2atmpS1970] = _M0L6_2atmpS1971;
        _M0L6_2atmpS1973 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS1973;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1978 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS1978;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS1974 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS1976 = _M0L11src__offsetS35 + _M0L1iS39;
        struct _M0TUsfE* _M0L6_2atmpS1975;
        struct _M0TUsfE* _M0L6_2aoldS3868;
        int32_t _M0L6_2atmpS1977;
        if (
          _M0L6_2atmpS1976 < 0
          || _M0L6_2atmpS1976 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1975 = (struct _M0TUsfE*)_M0L3srcS33[_M0L6_2atmpS1976];
        if (
          _M0L6_2atmpS1974 < 0
          || _M0L6_2atmpS1974 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3868 = (struct _M0TUsfE*)_M0L3dstS32[_M0L6_2atmpS1974];
        if (_M0L6_2atmpS1975) {
          moonbit_incref(_M0L6_2atmpS1975);
        }
        if (_M0L6_2aoldS3868) {
          moonbit_decref(_M0L6_2aoldS3868);
        }
        _M0L3dstS32[_M0L6_2atmpS1974] = _M0L6_2atmpS1975;
        _M0L6_2atmpS1977 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS1977;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGOsEE(
  moonbit_string_t* _M0L3dstS41,
  int32_t _M0L11dst__offsetS43,
  moonbit_string_t* _M0L3srcS42,
  int32_t _M0L11src__offsetS44,
  int32_t _M0L3lenS46
) {
  int32_t _if__result_4215;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_4215 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_4215 = 0;
  }
  if (_if__result_4215) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS1979 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS1981 = _M0L11src__offsetS44 + _M0L1iS45;
        moonbit_string_t _M0L6_2atmpS1980;
        moonbit_string_t _M0L6_2aoldS3870;
        int32_t _M0L6_2atmpS1982;
        if (
          _M0L6_2atmpS1981 < 0
          || _M0L6_2atmpS1981 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1980 = (moonbit_string_t)_M0L3srcS42[_M0L6_2atmpS1981];
        if (
          _M0L6_2atmpS1979 < 0
          || _M0L6_2atmpS1979 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3870 = (moonbit_string_t)_M0L3dstS41[_M0L6_2atmpS1979];
        if (_M0L6_2atmpS1980) {
          moonbit_incref(_M0L6_2atmpS1980);
        }
        if (_M0L6_2aoldS3870) {
          moonbit_decref(_M0L6_2aoldS3870);
        }
        _M0L3dstS41[_M0L6_2atmpS1979] = _M0L6_2atmpS1980;
        _M0L6_2atmpS1982 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS1982;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1987 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS1987;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS1983 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS1985 = _M0L11src__offsetS44 + _M0L1iS48;
        moonbit_string_t _M0L6_2atmpS1984;
        moonbit_string_t _M0L6_2aoldS3872;
        int32_t _M0L6_2atmpS1986;
        if (
          _M0L6_2atmpS1985 < 0
          || _M0L6_2atmpS1985 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1984 = (moonbit_string_t)_M0L3srcS42[_M0L6_2atmpS1985];
        if (
          _M0L6_2atmpS1983 < 0
          || _M0L6_2atmpS1983 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3872 = (moonbit_string_t)_M0L3dstS41[_M0L6_2atmpS1983];
        if (_M0L6_2atmpS1984) {
          moonbit_incref(_M0L6_2atmpS1984);
        }
        if (_M0L6_2aoldS3872) {
          moonbit_decref(_M0L6_2aoldS3872);
        }
        _M0L3dstS41[_M0L6_2atmpS1983] = _M0L6_2atmpS1984;
        _M0L6_2atmpS1986 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS1986;
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

int32_t _M0MPB18UninitializedArray6lengthGUsfEE(
  struct _M0TUsfE** _M0L4selfS12
) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS12);
}

int32_t _M0MPB18UninitializedArray6lengthGOsE(moonbit_string_t* _M0L4selfS13) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS13);
}

uint32_t _M0FPB13consume4__acc(uint32_t _M0L3accS9, uint32_t _M0L5inputS10) {
  uint32_t _M0L6_2atmpS1951;
  uint32_t _M0L6_2atmpS1950;
  uint32_t _M0L6_2atmpS1949;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1951 = _M0L5inputS10 * 3266489917u;
  _M0L6_2atmpS1950 = _M0L3accS9 + _M0L6_2atmpS1951;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1949 = _M0FPB4rotl(_M0L6_2atmpS1950, 17);
  return _M0L6_2atmpS1949 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS7, int32_t _M0L1rS8) {
  uint32_t _M0L6_2atmpS1946;
  int32_t _M0L6_2atmpS1948;
  uint32_t _M0L6_2atmpS1947;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1946 = _M0L1xS7 << (_M0L1rS8 & 31);
  _M0L6_2atmpS1948 = 32 - _M0L1rS8;
  _M0L6_2atmpS1947 = _M0L1xS7 >> (_M0L6_2atmpS1948 & 31);
  return _M0L6_2atmpS1946 | _M0L6_2atmpS1947;
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

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(
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
  void* _M0L11_2aobj__ptrS1832,
  struct _M0TPB4Show _M0L8_2aparamS1831
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1830 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1832;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS1830, _M0L8_2aparamS1831);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1829,
  struct _M0TPB4Show _M0L8_2aparamS1828
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1827 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1829;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS1827, _M0L8_2aparamS1828);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1826,
  int32_t _M0L8_2aparamS1825
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1824 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1826;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1824, _M0L8_2aparamS1825);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1823,
  struct _M0TPC16string10StringView _M0L8_2aparamS1822
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1821 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1823;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1821, _M0L8_2aparamS1822);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1820,
  moonbit_string_t _M0L8_2aparamS1817,
  int32_t _M0L8_2aparamS1818,
  int32_t _M0L8_2aparamS1819
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1816 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1820;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1816, _M0L8_2aparamS1817, _M0L8_2aparamS1818, _M0L8_2aparamS1819);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1815,
  moonbit_string_t _M0L8_2aparamS1814
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1813 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1815;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1813, _M0L8_2aparamS1814);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP39moonbitdb9moonbitdb3lib8Database* _M0L2dbS1757;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1758;
  moonbit_string_t _M0L6_2atmpS1835;
  moonbit_string_t _M0L6_2atmpS1834;
  moonbit_string_t _M0L6_2atmpS1833;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1759;
  moonbit_string_t _M0L6_2atmpS1838;
  moonbit_string_t _M0L6_2atmpS1837;
  moonbit_string_t _M0L6_2atmpS1836;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1760;
  int32_t _M0L6_2atmpS1840;
  moonbit_string_t _M0L6_2atmpS1839;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1761;
  int32_t _M0L6_2atmpS1842;
  moonbit_string_t _M0L6_2atmpS1841;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1762;
  moonbit_string_t _M0L6_2atmpS1845;
  moonbit_string_t _M0L6_2atmpS1844;
  moonbit_string_t _M0L6_2atmpS1843;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1763;
  int64_t _M0L6_2atmpS1848;
  moonbit_string_t _M0L6_2atmpS1847;
  moonbit_string_t _M0L6_2atmpS1846;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1764;
  int64_t _M0L6_2atmpS1851;
  moonbit_string_t _M0L6_2atmpS1850;
  moonbit_string_t _M0L6_2atmpS1849;
  int32_t _M0L6_2atmpS1852;
  int32_t _M0L6_2atmpS1853;
  int32_t _M0L6_2atmpS1854;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1765;
  moonbit_string_t _M0L6_2atmpS1857;
  moonbit_string_t _M0L6_2atmpS1856;
  moonbit_string_t _M0L6_2atmpS1855;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1766;
  int32_t _M0L6_2atmpS1859;
  moonbit_string_t _M0L6_2atmpS1858;
  struct _M0TPB3MapGssE* _M0L3allS1767;
  moonbit_string_t* _M0L6_2atmpS1945;
  struct _M0TPB5ArrayGsE* _M0L6fieldsS1768;
  struct _M0TPB4IterGUssEE* _M0L5_2aitS1769;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1777;
  moonbit_string_t _M0L7_2abindS1778;
  int32_t _M0L6_2atmpS1863;
  struct _M0TPC16string10StringView _M0L6_2atmpS1862;
  moonbit_string_t _M0L6_2atmpS1861;
  moonbit_string_t _M0L6_2atmpS1860;
  int32_t _M0L6_2atmpS1864;
  int32_t _M0L6_2atmpS1865;
  int32_t _M0L6_2atmpS1866;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1779;
  int32_t _M0L6_2atmpS1868;
  moonbit_string_t _M0L6_2atmpS1867;
  struct _M0TPB5ArrayGsE* _M0L5tasksS1780;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1781;
  moonbit_string_t _M0L7_2abindS1782;
  int32_t _M0L6_2atmpS1872;
  struct _M0TPC16string10StringView _M0L6_2atmpS1871;
  moonbit_string_t _M0L6_2atmpS1870;
  moonbit_string_t _M0L6_2atmpS1869;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1783;
  moonbit_string_t _M0L6_2atmpS1875;
  moonbit_string_t _M0L6_2atmpS1874;
  moonbit_string_t _M0L6_2atmpS1873;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1784;
  moonbit_string_t _M0L6_2atmpS1878;
  moonbit_string_t _M0L6_2atmpS1877;
  moonbit_string_t _M0L6_2atmpS1876;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1785;
  moonbit_string_t _M0L6_2atmpS1881;
  moonbit_string_t _M0L6_2atmpS1880;
  moonbit_string_t _M0L6_2atmpS1879;
  int32_t _M0L6_2atmpS1882;
  int32_t _M0L6_2atmpS1883;
  int32_t _M0L6_2atmpS1884;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1786;
  int32_t _M0L6_2atmpS1886;
  moonbit_string_t _M0L6_2atmpS1885;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1787;
  int32_t _M0L6_2atmpS1888;
  moonbit_string_t _M0L6_2atmpS1887;
  struct _M0TPB5ArrayGsE* _M0L4tagsS1788;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1789;
  moonbit_string_t _M0L7_2abindS1790;
  int32_t _M0L6_2atmpS1892;
  struct _M0TPC16string10StringView _M0L6_2atmpS1891;
  moonbit_string_t _M0L6_2atmpS1890;
  moonbit_string_t _M0L6_2atmpS1889;
  int32_t _M0L6_2atmpS1893;
  int32_t _M0L6_2atmpS1894;
  int32_t _M0L6_2atmpS1895;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1791;
  int32_t _M0L6_2atmpS1897;
  moonbit_string_t _M0L6_2atmpS1896;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1792;
  void* _M0L6_2atmpS1900;
  moonbit_string_t _M0L6_2atmpS1899;
  moonbit_string_t _M0L6_2atmpS1898;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1793;
  int64_t _M0L6_2atmpS1903;
  moonbit_string_t _M0L6_2atmpS1902;
  moonbit_string_t _M0L6_2atmpS1901;
  struct _M0TPB5ArrayGsE* _M0L3topS1794;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1795;
  moonbit_string_t _M0L7_2abindS1796;
  int32_t _M0L6_2atmpS1907;
  struct _M0TPC16string10StringView _M0L6_2atmpS1906;
  moonbit_string_t _M0L6_2atmpS1905;
  moonbit_string_t _M0L6_2atmpS1904;
  int32_t _M0L6_2atmpS1908;
  int32_t _M0L6_2atmpS1909;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1797;
  int32_t _M0L6_2atmpS1911;
  moonbit_string_t _M0L6_2atmpS1910;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1798;
  int32_t _M0L6_2atmpS1913;
  moonbit_string_t _M0L6_2atmpS1912;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1799;
  int32_t _M0L6_2atmpS1915;
  moonbit_string_t _M0L6_2atmpS1914;
  moonbit_string_t* _M0L6_2atmpS1920;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1917;
  moonbit_string_t* _M0L6_2atmpS1919;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1918;
  int32_t _M0L6_2atmpS1916;
  moonbit_string_t* _M0L6_2atmpS1944;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1943;
  struct _M0TPB5ArrayGOsE* _M0L4valsS1800;
  int32_t _M0L1iS1801;
  moonbit_string_t* _M0L6_2atmpS1942;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1941;
  int32_t _M0L7deletedS1804;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1805;
  moonbit_string_t _M0L6_2atmpS1926;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1806;
  int32_t _M0L6_2atmpS1928;
  moonbit_string_t _M0L6_2atmpS1927;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1807;
  moonbit_string_t _M0L6_2atmpS1930;
  moonbit_string_t _M0L6_2atmpS1929;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1808;
  moonbit_string_t _M0L6_2atmpS1932;
  moonbit_string_t _M0L6_2atmpS1931;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1809;
  moonbit_string_t _M0L6_2atmpS1934;
  moonbit_string_t _M0L6_2atmpS1933;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1810;
  moonbit_string_t _M0L6_2atmpS1936;
  moonbit_string_t _M0L6_2atmpS1935;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1811;
  moonbit_string_t _M0L6_2atmpS1938;
  moonbit_string_t _M0L6_2atmpS1937;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1812;
  moonbit_string_t _M0L6_2atmpS1940;
  moonbit_string_t _M0L6_2atmpS1939;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 23 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L2dbS1757 = _M0MP39moonbitdb9moonbitdb3lib8Database3new();
  #line 25 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_35.data);
  #line 26 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MP39moonbitdb9moonbitdb3lib8Database3set(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_36.data, (moonbit_string_t)moonbit_string_literal_37.data);
  #line 27 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MP39moonbitdb9moonbitdb3lib8Database3set(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_38.data, (moonbit_string_t)moonbit_string_literal_39.data);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1758
  = _M0MPB13StringBuilder21StringBuilder_2einner(10);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1758, (moonbit_string_t)moonbit_string_literal_40.data);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1835
  = _M0MP39moonbitdb9moonbitdb3lib8Database3get(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_36.data);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1834
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(_M0L6_2atmpS1835);
  if (_M0L6_2atmpS1835) {
    moonbit_decref(_M0L6_2atmpS1835);
  }
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1758, _M0L6_2atmpS1834);
  moonbit_decref(_M0L6_2atmpS1834);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1833
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1758);
  moonbit_decref(_M0L18_2astring__builderS1758);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1833);
  moonbit_decref(_M0L6_2atmpS1833);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1759
  = _M0MPB13StringBuilder21StringBuilder_2einner(9);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1759, (moonbit_string_t)moonbit_string_literal_41.data);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1838
  = _M0MP39moonbitdb9moonbitdb3lib8Database3get(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_38.data);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1837
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(_M0L6_2atmpS1838);
  if (_M0L6_2atmpS1838) {
    moonbit_decref(_M0L6_2atmpS1838);
  }
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1759, _M0L6_2atmpS1837);
  moonbit_decref(_M0L6_2atmpS1837);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1836
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1759);
  moonbit_decref(_M0L18_2astring__builderS1759);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1836);
  moonbit_decref(_M0L6_2atmpS1836);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1760
  = _M0MPB13StringBuilder21StringBuilder_2einner(13);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1760, (moonbit_string_t)moonbit_string_literal_42.data);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1840
  = _M0MP39moonbitdb9moonbitdb3lib8Database6strlen(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_36.data);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1760, _M0L6_2atmpS1840);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1839
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1760);
  moonbit_decref(_M0L18_2astring__builderS1760);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1839);
  moonbit_decref(_M0L6_2atmpS1839);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1761
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1761, (moonbit_string_t)moonbit_string_literal_43.data);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1842
  = _M0MP39moonbitdb9moonbitdb3lib8Database6append(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_36.data, (moonbit_string_t)moonbit_string_literal_44.data);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1761, _M0L6_2atmpS1842);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1841
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1761);
  moonbit_decref(_M0L18_2astring__builderS1761);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1841);
  moonbit_decref(_M0L6_2atmpS1841);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1762
  = _M0MPB13StringBuilder21StringBuilder_2einner(23);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1762, (moonbit_string_t)moonbit_string_literal_45.data);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1845
  = _M0MP39moonbitdb9moonbitdb3lib8Database3get(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_36.data);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1844
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(_M0L6_2atmpS1845);
  if (_M0L6_2atmpS1845) {
    moonbit_decref(_M0L6_2atmpS1845);
  }
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1762, _M0L6_2atmpS1844);
  moonbit_decref(_M0L6_2atmpS1844);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1843
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1762);
  moonbit_decref(_M0L18_2astring__builderS1762);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1843);
  moonbit_decref(_M0L6_2atmpS1843);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1763
  = _M0MPB13StringBuilder21StringBuilder_2einner(10);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1763, (moonbit_string_t)moonbit_string_literal_46.data);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1848
  = _M0MP39moonbitdb9moonbitdb3lib8Database4incr(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_38.data);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1847
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage14show__opt__int(_M0L6_2atmpS1848);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1763, _M0L6_2atmpS1847);
  moonbit_decref(_M0L6_2atmpS1847);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1846
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1763);
  moonbit_decref(_M0L18_2astring__builderS1763);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1846);
  moonbit_decref(_M0L6_2atmpS1846);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1764
  = _M0MPB13StringBuilder21StringBuilder_2einner(10);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1764, (moonbit_string_t)moonbit_string_literal_47.data);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1851
  = _M0MP39moonbitdb9moonbitdb3lib8Database4decr(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_38.data);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1850
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage14show__opt__int(_M0L6_2atmpS1851);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1764, _M0L6_2atmpS1850);
  moonbit_decref(_M0L6_2atmpS1850);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1849
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1764);
  moonbit_decref(_M0L18_2astring__builderS1764);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1849);
  moonbit_decref(_M0L6_2atmpS1849);
  #line 36 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_48.data);
  #line 37 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1852
  = _M0MP39moonbitdb9moonbitdb3lib8Database4hset(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_49.data, (moonbit_string_t)moonbit_string_literal_50.data, (moonbit_string_t)moonbit_string_literal_51.data);
  #line 38 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1853
  = _M0MP39moonbitdb9moonbitdb3lib8Database4hset(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_49.data, (moonbit_string_t)moonbit_string_literal_52.data, (moonbit_string_t)moonbit_string_literal_53.data);
  #line 39 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1854
  = _M0MP39moonbitdb9moonbitdb3lib8Database4hset(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_49.data, (moonbit_string_t)moonbit_string_literal_38.data, (moonbit_string_t)moonbit_string_literal_39.data);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1765
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1765, (moonbit_string_t)moonbit_string_literal_54.data);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1857
  = _M0MP39moonbitdb9moonbitdb3lib8Database4hget(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_49.data, (moonbit_string_t)moonbit_string_literal_50.data);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1856
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(_M0L6_2atmpS1857);
  if (_M0L6_2atmpS1857) {
    moonbit_decref(_M0L6_2atmpS1857);
  }
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1765, _M0L6_2atmpS1856);
  moonbit_decref(_M0L6_2atmpS1856);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1855
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1765);
  moonbit_decref(_M0L18_2astring__builderS1765);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1855);
  moonbit_decref(_M0L6_2atmpS1855);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1766
  = _M0MPB13StringBuilder21StringBuilder_2einner(13);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1766, (moonbit_string_t)moonbit_string_literal_55.data);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1859
  = _M0MP39moonbitdb9moonbitdb3lib8Database4hlen(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_49.data);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1766, _M0L6_2atmpS1859);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1858
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1766);
  moonbit_decref(_M0L18_2astring__builderS1766);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1858);
  moonbit_decref(_M0L6_2atmpS1858);
  #line 42 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L3allS1767
  = _M0MP39moonbitdb9moonbitdb3lib8Database7hgetall(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_49.data);
  _M0L6_2atmpS1945 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6fieldsS1768
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6fieldsS1768)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6fieldsS1768->$0 = _M0L6_2atmpS1945;
  _M0L6fieldsS1768->$1 = 0;
  #line 43 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L5_2aitS1769 = _M0MPB3Map5iter2GssE(_M0L3allS1767);
  moonbit_decref(_M0L3allS1767);
  while (1) {
    moonbit_string_t _M0L1fS1771;
    struct _M0TUssE* _M0L7_2abindS1773;
    #line 44 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
    _M0L7_2abindS1773 = _M0MPB5Iter24nextGssE(_M0L5_2aitS1769);
    if (_M0L7_2abindS1773 == 0) {
      if (_M0L7_2abindS1773) {
        moonbit_decref(_M0L7_2abindS1773);
      }
      moonbit_decref(_M0L5_2aitS1769);
    } else {
      struct _M0TUssE* _M0L7_2aSomeS1774 = _M0L7_2abindS1773;
      struct _M0TUssE* _M0L4_2axS1775 = _M0L7_2aSomeS1774;
      moonbit_string_t _M0L8_2afieldS3874 = _M0L4_2axS1775->$0;
      int32_t _M0L6_2acntS3940 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1775));
      moonbit_string_t _M0L4_2afS1776;
      if (_M0L6_2acntS3940 > 1) {
        int32_t _M0L11_2anew__cntS3942 = _M0L6_2acntS3940 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1775), _M0L11_2anew__cntS3942);
        moonbit_incref(_M0L8_2afieldS3874);
      } else if (_M0L6_2acntS3940 == 1) {
        moonbit_string_t _M0L8_2afieldS3941 = _M0L4_2axS1775->$1;
        moonbit_decref(_M0L8_2afieldS3941);
        #line 44 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
        moonbit_free(_M0L4_2axS1775);
      }
      _M0L4_2afS1776 = _M0L8_2afieldS3874;
      _M0L1fS1771 = _M0L4_2afS1776;
      goto join_1770;
    }
    goto joinlet_4219;
    join_1770:;
    #line 45 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
    _M0MPC15array5Array4pushGsE(_M0L6fieldsS1768, _M0L1fS1771);
    moonbit_decref(_M0L1fS1771);
    continue;
    joinlet_4219:;
    break;
  }
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1777
  = _M0MPB13StringBuilder21StringBuilder_2einner(18);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1777, (moonbit_string_t)moonbit_string_literal_56.data);
  _M0L7_2abindS1778 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS1863 = Moonbit_array_length(_M0L7_2abindS1778);
  _M0L6_2atmpS1862
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1778, .$1 = 0, .$2 = _M0L6_2atmpS1863
  };
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1861
  = _M0MPC15array5Array4joinGsE(_M0L6fieldsS1768, _M0L6_2atmpS1862);
  moonbit_decref(_M0L6fieldsS1768);
  moonbit_decref(_M0L6_2atmpS1862.$0);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1777, _M0L6_2atmpS1861);
  moonbit_decref(_M0L6_2atmpS1861);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1777, (moonbit_string_t)moonbit_string_literal_58.data);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1860
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1777);
  moonbit_decref(_M0L18_2astring__builderS1777);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1860);
  moonbit_decref(_M0L6_2atmpS1860);
  #line 49 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_59.data);
  #line 50 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1864
  = _M0MP39moonbitdb9moonbitdb3lib8Database5rpush(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data, (moonbit_string_t)moonbit_string_literal_61.data);
  #line 51 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1865
  = _M0MP39moonbitdb9moonbitdb3lib8Database5rpush(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data, (moonbit_string_t)moonbit_string_literal_62.data);
  #line 52 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1866
  = _M0MP39moonbitdb9moonbitdb3lib8Database5rpush(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data, (moonbit_string_t)moonbit_string_literal_63.data);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1779
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1779, (moonbit_string_t)moonbit_string_literal_64.data);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1868
  = _M0MP39moonbitdb9moonbitdb3lib8Database4llen(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1779, _M0L6_2atmpS1868);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1867
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1779);
  moonbit_decref(_M0L18_2astring__builderS1779);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1867);
  moonbit_decref(_M0L6_2atmpS1867);
  #line 54 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L5tasksS1780
  = _M0MP39moonbitdb9moonbitdb3lib8Database6lrange(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data, 0, -1);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1781
  = _M0MPB13StringBuilder21StringBuilder_2einner(21);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1781, (moonbit_string_t)moonbit_string_literal_65.data);
  _M0L7_2abindS1782 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS1872 = Moonbit_array_length(_M0L7_2abindS1782);
  _M0L6_2atmpS1871
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1782, .$1 = 0, .$2 = _M0L6_2atmpS1872
  };
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1870
  = _M0MPC15array5Array4joinGsE(_M0L5tasksS1780, _M0L6_2atmpS1871);
  moonbit_decref(_M0L5tasksS1780);
  moonbit_decref(_M0L6_2atmpS1871.$0);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1781, _M0L6_2atmpS1870);
  moonbit_decref(_M0L6_2atmpS1870);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1781, (moonbit_string_t)moonbit_string_literal_58.data);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1869
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1781);
  moonbit_decref(_M0L18_2astring__builderS1781);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1869);
  moonbit_decref(_M0L6_2atmpS1869);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1783
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1783, (moonbit_string_t)moonbit_string_literal_66.data);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1875
  = _M0MP39moonbitdb9moonbitdb3lib8Database4lpop(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1874
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(_M0L6_2atmpS1875);
  if (_M0L6_2atmpS1875) {
    moonbit_decref(_M0L6_2atmpS1875);
  }
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1783, _M0L6_2atmpS1874);
  moonbit_decref(_M0L6_2atmpS1874);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1873
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1783);
  moonbit_decref(_M0L18_2astring__builderS1783);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1873);
  moonbit_decref(_M0L6_2atmpS1873);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1784
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1784, (moonbit_string_t)moonbit_string_literal_67.data);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1878
  = _M0MP39moonbitdb9moonbitdb3lib8Database4rpop(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1877
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(_M0L6_2atmpS1878);
  if (_M0L6_2atmpS1878) {
    moonbit_decref(_M0L6_2atmpS1878);
  }
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1784, _M0L6_2atmpS1877);
  moonbit_decref(_M0L6_2atmpS1877);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1876
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1784);
  moonbit_decref(_M0L18_2astring__builderS1784);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1876);
  moonbit_decref(_M0L6_2atmpS1876);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1785
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1785, (moonbit_string_t)moonbit_string_literal_68.data);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1881
  = _M0MP39moonbitdb9moonbitdb3lib8Database6lindex(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data, 0);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1880
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(_M0L6_2atmpS1881);
  if (_M0L6_2atmpS1881) {
    moonbit_decref(_M0L6_2atmpS1881);
  }
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1785, _M0L6_2atmpS1880);
  moonbit_decref(_M0L6_2atmpS1880);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1879
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1785);
  moonbit_decref(_M0L18_2astring__builderS1785);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1879);
  moonbit_decref(_M0L6_2atmpS1879);
  #line 60 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_69.data);
  #line 61 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1882
  = _M0MP39moonbitdb9moonbitdb3lib8Database4sadd(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_70.data, (moonbit_string_t)moonbit_string_literal_71.data);
  #line 62 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1883
  = _M0MP39moonbitdb9moonbitdb3lib8Database4sadd(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_70.data, (moonbit_string_t)moonbit_string_literal_72.data);
  #line 63 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1884
  = _M0MP39moonbitdb9moonbitdb3lib8Database4sadd(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_70.data, (moonbit_string_t)moonbit_string_literal_73.data);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1786
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1786, (moonbit_string_t)moonbit_string_literal_74.data);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1886
  = _M0MP39moonbitdb9moonbitdb3lib8Database5scard(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_70.data);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1786, _M0L6_2atmpS1886);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1885
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1786);
  moonbit_decref(_M0L18_2astring__builderS1786);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1885);
  moonbit_decref(_M0L6_2atmpS1885);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1787
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1787, (moonbit_string_t)moonbit_string_literal_75.data);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1888
  = _M0MP39moonbitdb9moonbitdb3lib8Database9sismember(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_70.data, (moonbit_string_t)moonbit_string_literal_73.data);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1787, _M0L6_2atmpS1888);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1887
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1787);
  moonbit_decref(_M0L18_2astring__builderS1787);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1887);
  moonbit_decref(_M0L6_2atmpS1887);
  #line 66 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L4tagsS1788
  = _M0MP39moonbitdb9moonbitdb3lib8Database8smembers(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_70.data);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1789
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1789, (moonbit_string_t)moonbit_string_literal_76.data);
  _M0L7_2abindS1790 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS1892 = Moonbit_array_length(_M0L7_2abindS1790);
  _M0L6_2atmpS1891
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1790, .$1 = 0, .$2 = _M0L6_2atmpS1892
  };
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1890
  = _M0MPC15array5Array4joinGsE(_M0L4tagsS1788, _M0L6_2atmpS1891);
  moonbit_decref(_M0L4tagsS1788);
  moonbit_decref(_M0L6_2atmpS1891.$0);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1789, _M0L6_2atmpS1890);
  moonbit_decref(_M0L6_2atmpS1890);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1789, (moonbit_string_t)moonbit_string_literal_58.data);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1889
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1789);
  moonbit_decref(_M0L18_2astring__builderS1789);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1889);
  moonbit_decref(_M0L6_2atmpS1889);
  #line 69 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_77.data);
  #line 70 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1893
  = _M0MP39moonbitdb9moonbitdb3lib8Database4zadd(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_78.data, 0x1.9p+6f, (moonbit_string_t)moonbit_string_literal_79.data);
  #line 71 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1894
  = _M0MP39moonbitdb9moonbitdb3lib8Database4zadd(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_78.data, 0x1.9p+7f, (moonbit_string_t)moonbit_string_literal_80.data);
  #line 72 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1895
  = _M0MP39moonbitdb9moonbitdb3lib8Database4zadd(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_78.data, 0x1.2cp+7f, (moonbit_string_t)moonbit_string_literal_81.data);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1791
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1791, (moonbit_string_t)moonbit_string_literal_82.data);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1897
  = _M0MP39moonbitdb9moonbitdb3lib8Database5zcard(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_78.data);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1791, _M0L6_2atmpS1897);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1896
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1791);
  moonbit_decref(_M0L18_2astring__builderS1791);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1896);
  moonbit_decref(_M0L6_2atmpS1896);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1792
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1792, (moonbit_string_t)moonbit_string_literal_83.data);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1900
  = _M0MP39moonbitdb9moonbitdb3lib8Database6zscore(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_78.data, (moonbit_string_t)moonbit_string_literal_80.data);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1899
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage16show__opt__float(_M0L6_2atmpS1900);
  moonbit_decref(_M0L6_2atmpS1900);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1792, _M0L6_2atmpS1899);
  moonbit_decref(_M0L6_2atmpS1899);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1898
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1792);
  moonbit_decref(_M0L18_2astring__builderS1792);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1898);
  moonbit_decref(_M0L6_2atmpS1898);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1793
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1793, (moonbit_string_t)moonbit_string_literal_84.data);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1903
  = _M0MP39moonbitdb9moonbitdb3lib8Database5zrank(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_78.data, (moonbit_string_t)moonbit_string_literal_81.data);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1902
  = _M0FP49moonbitdb9moonbitdb8examples12basic__usage14show__opt__int(_M0L6_2atmpS1903);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1793, _M0L6_2atmpS1902);
  moonbit_decref(_M0L6_2atmpS1902);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1901
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1793);
  moonbit_decref(_M0L18_2astring__builderS1793);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1901);
  moonbit_decref(_M0L6_2atmpS1901);
  #line 76 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L3topS1794
  = _M0MP39moonbitdb9moonbitdb3lib8Database6zrange(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_78.data, 0, 2);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1795
  = _M0MPB13StringBuilder21StringBuilder_2einner(14);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1795, (moonbit_string_t)moonbit_string_literal_85.data);
  _M0L7_2abindS1796 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS1907 = Moonbit_array_length(_M0L7_2abindS1796);
  _M0L6_2atmpS1906
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1796, .$1 = 0, .$2 = _M0L6_2atmpS1907
  };
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1905
  = _M0MPC15array5Array4joinGsE(_M0L3topS1794, _M0L6_2atmpS1906);
  moonbit_decref(_M0L3topS1794);
  moonbit_decref(_M0L6_2atmpS1906.$0);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1795, _M0L6_2atmpS1905);
  moonbit_decref(_M0L6_2atmpS1905);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1795, (moonbit_string_t)moonbit_string_literal_58.data);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1904
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1795);
  moonbit_decref(_M0L18_2astring__builderS1795);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1904);
  moonbit_decref(_M0L6_2atmpS1904);
  #line 79 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_86.data);
  #line 80 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1908
  = _M0MP39moonbitdb9moonbitdb3lib8Database3set(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_87.data, (moonbit_string_t)moonbit_string_literal_88.data);
  #line 81 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1909
  = _M0MP39moonbitdb9moonbitdb3lib8Database6expire(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_87.data, 30);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1797
  = _M0MPB13StringBuilder21StringBuilder_2einner(20);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1797, (moonbit_string_t)moonbit_string_literal_89.data);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1911
  = _M0MP39moonbitdb9moonbitdb3lib8Database3ttl(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_87.data);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1797, _M0L6_2atmpS1911);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1797, (moonbit_string_t)moonbit_string_literal_90.data);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1910
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1797);
  moonbit_decref(_M0L18_2astring__builderS1797);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1910);
  moonbit_decref(_M0L6_2atmpS1910);
  #line 83 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MP39moonbitdb9moonbitdb3lib8Database13advance__time(_M0L2dbS1757, 20000);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1798
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1798, (moonbit_string_t)moonbit_string_literal_91.data);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1913
  = _M0MP39moonbitdb9moonbitdb3lib8Database3ttl(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_87.data);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1798, _M0L6_2atmpS1913);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1798, (moonbit_string_t)moonbit_string_literal_90.data);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1912
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1798);
  moonbit_decref(_M0L18_2astring__builderS1798);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1912);
  moonbit_decref(_M0L6_2atmpS1912);
  #line 85 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MP39moonbitdb9moonbitdb3lib8Database13advance__time(_M0L2dbS1757, 15000);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1799
  = _M0MPB13StringBuilder21StringBuilder_2einner(17);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1799, (moonbit_string_t)moonbit_string_literal_92.data);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1915
  = _M0MP39moonbitdb9moonbitdb3lib8Database6exists(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_87.data);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1799, _M0L6_2atmpS1915);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1914
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1799);
  moonbit_decref(_M0L18_2astring__builderS1799);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1914);
  moonbit_decref(_M0L6_2atmpS1914);
  #line 88 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_93.data);
  _M0L6_2atmpS1920 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS1920[0] = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L6_2atmpS1920[1] = (moonbit_string_t)moonbit_string_literal_95.data;
  _M0L6_2atmpS1920[2] = (moonbit_string_t)moonbit_string_literal_96.data;
  _M0L6_2atmpS1917
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1917)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6_2atmpS1917->$0 = _M0L6_2atmpS1920;
  _M0L6_2atmpS1917->$1 = 3;
  _M0L6_2atmpS1919 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS1919[0] = (moonbit_string_t)moonbit_string_literal_97.data;
  _M0L6_2atmpS1919[1] = (moonbit_string_t)moonbit_string_literal_98.data;
  _M0L6_2atmpS1919[2] = (moonbit_string_t)moonbit_string_literal_99.data;
  _M0L6_2atmpS1918
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1918)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6_2atmpS1918->$0 = _M0L6_2atmpS1919;
  _M0L6_2atmpS1918->$1 = 3;
  #line 89 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1916
  = _M0MP39moonbitdb9moonbitdb3lib8Database4mset(_M0L2dbS1757, _M0L6_2atmpS1917, _M0L6_2atmpS1918);
  moonbit_decref(_M0L6_2atmpS1917);
  moonbit_decref(_M0L6_2atmpS1918);
  _M0L6_2atmpS1944 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1944[0] = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L6_2atmpS1944[1] = (moonbit_string_t)moonbit_string_literal_95.data;
  _M0L6_2atmpS1944[2] = (moonbit_string_t)moonbit_string_literal_96.data;
  _M0L6_2atmpS1944[3] = (moonbit_string_t)moonbit_string_literal_100.data;
  _M0L6_2atmpS1943
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1943)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6_2atmpS1943->$0 = _M0L6_2atmpS1944;
  _M0L6_2atmpS1943->$1 = 4;
  #line 90 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L4valsS1800
  = _M0MP39moonbitdb9moonbitdb3lib8Database4mget(_M0L2dbS1757, _M0L6_2atmpS1943);
  moonbit_decref(_M0L6_2atmpS1943);
  #line 91 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_101.data);
  _M0L1iS1801 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1921;
    #line 92 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
    _M0L6_2atmpS1921 = _M0MPC15array5Array6lengthGOsE(_M0L4valsS1800);
    if (_M0L1iS1801 < _M0L6_2atmpS1921) {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1802;
      moonbit_string_t _M0L6_2atmpS1924;
      moonbit_string_t _M0L6_2atmpS1923;
      moonbit_string_t _M0L6_2atmpS1922;
      int32_t _M0L6_2atmpS1925;
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0L18_2astring__builderS1802
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1802, (moonbit_string_t)moonbit_string_literal_102.data);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1802, _M0L1iS1801);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1802, (moonbit_string_t)moonbit_string_literal_103.data);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0L6_2atmpS1924
      = _M0MPC15array5Array2atGOsE(_M0L4valsS1800, _M0L1iS1801);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0L6_2atmpS1923
      = _M0FP49moonbitdb9moonbitdb8examples12basic__usage9show__opt(_M0L6_2atmpS1924);
      if (_M0L6_2atmpS1924) {
        moonbit_decref(_M0L6_2atmpS1924);
      }
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1802, _M0L6_2atmpS1923);
      moonbit_decref(_M0L6_2atmpS1923);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0L6_2atmpS1922
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1802);
      moonbit_decref(_M0L18_2astring__builderS1802);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS1922);
      moonbit_decref(_M0L6_2atmpS1922);
      _M0L6_2atmpS1925 = _M0L1iS1801 + 1;
      _M0L1iS1801 = _M0L6_2atmpS1925;
      continue;
    } else {
      moonbit_decref(_M0L4valsS1800);
    }
    break;
  }
  _M0L6_2atmpS1942 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS1942[0] = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L6_2atmpS1942[1] = (moonbit_string_t)moonbit_string_literal_95.data;
  _M0L6_2atmpS1942[2] = (moonbit_string_t)moonbit_string_literal_96.data;
  _M0L6_2atmpS1941
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1941)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6_2atmpS1941->$0 = _M0L6_2atmpS1942;
  _M0L6_2atmpS1941->$1 = 3;
  #line 95 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L7deletedS1804
  = _M0MP39moonbitdb9moonbitdb3lib8Database4mdel(_M0L2dbS1757, _M0L6_2atmpS1941);
  moonbit_decref(_M0L6_2atmpS1941);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1805
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1805, (moonbit_string_t)moonbit_string_literal_104.data);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1805, _M0L7deletedS1804);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1926
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1805);
  moonbit_decref(_M0L18_2astring__builderS1805);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1926);
  moonbit_decref(_M0L6_2atmpS1926);
  #line 98 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_105.data);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1806
  = _M0MPB13StringBuilder21StringBuilder_2einner(8);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1806, (moonbit_string_t)moonbit_string_literal_106.data);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1928
  = _M0MP39moonbitdb9moonbitdb3lib8Database6dbsize(_M0L2dbS1757);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1806, _M0L6_2atmpS1928);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1927
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1806);
  moonbit_decref(_M0L18_2astring__builderS1806);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1927);
  moonbit_decref(_M0L6_2atmpS1927);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1807
  = _M0MPB13StringBuilder21StringBuilder_2einner(6);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1807, (moonbit_string_t)moonbit_string_literal_107.data);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1930 = _M0MP39moonbitdb9moonbitdb3lib8Database4ping();
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1807, _M0L6_2atmpS1930);
  moonbit_decref(_M0L6_2atmpS1930);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1929
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1807);
  moonbit_decref(_M0L18_2astring__builderS1807);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1929);
  moonbit_decref(_M0L6_2atmpS1929);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1808
  = _M0MPB13StringBuilder21StringBuilder_2einner(6);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1808, (moonbit_string_t)moonbit_string_literal_108.data);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1932
  = _M0MP39moonbitdb9moonbitdb3lib8Database4echo(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_109.data);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1808, _M0L6_2atmpS1932);
  moonbit_decref(_M0L6_2atmpS1932);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1931
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1808);
  moonbit_decref(_M0L18_2astring__builderS1808);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1931);
  moonbit_decref(_M0L6_2atmpS1931);
  #line 103 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_110.data);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1809
  = _M0MPB13StringBuilder21StringBuilder_2einner(13);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1809, (moonbit_string_t)moonbit_string_literal_111.data);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1934
  = _M0MP39moonbitdb9moonbitdb3lib8Database8type__of(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_49.data);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1809, _M0L6_2atmpS1934);
  moonbit_decref(_M0L6_2atmpS1934);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1933
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1809);
  moonbit_decref(_M0L18_2astring__builderS1809);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1933);
  moonbit_decref(_M0L6_2atmpS1933);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1810
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1810, (moonbit_string_t)moonbit_string_literal_112.data);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1936
  = _M0MP39moonbitdb9moonbitdb3lib8Database8type__of(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_60.data);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1810, _M0L6_2atmpS1936);
  moonbit_decref(_M0L6_2atmpS1936);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1935
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1810);
  moonbit_decref(_M0L18_2astring__builderS1810);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1935);
  moonbit_decref(_M0L6_2atmpS1935);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1811
  = _M0MPB13StringBuilder21StringBuilder_2einner(18);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1811, (moonbit_string_t)moonbit_string_literal_113.data);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1938
  = _M0MP39moonbitdb9moonbitdb3lib8Database8type__of(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_78.data);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1811, _M0L6_2atmpS1938);
  moonbit_decref(_M0L6_2atmpS1938);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1937
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1811);
  moonbit_decref(_M0L18_2astring__builderS1811);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1937);
  moonbit_decref(_M0L6_2atmpS1937);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1812
  = _M0MPB13StringBuilder21StringBuilder_2einner(18);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1812, (moonbit_string_t)moonbit_string_literal_114.data);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1940
  = _M0MP39moonbitdb9moonbitdb3lib8Database8type__of(_M0L2dbS1757, (moonbit_string_t)moonbit_string_literal_115.data);
  moonbit_decref(_M0L2dbS1757);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1812, _M0L6_2atmpS1940);
  moonbit_decref(_M0L6_2atmpS1940);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1939
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1812);
  moonbit_decref(_M0L18_2astring__builderS1812);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1939);
  moonbit_decref(_M0L6_2atmpS1939);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  return 0;
}